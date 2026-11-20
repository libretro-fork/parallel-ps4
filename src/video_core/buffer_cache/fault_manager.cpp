// SPDX-FileCopyrightText: Copyright 2025 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <vector>

#include "common/div_ceil.h"
#include "video_core/buffer_cache/buffer_cache.h"
#include "video_core/buffer_cache/fault_manager.h"
#include "video_core/renderer_vulkan/vk_instance.h"
#include "video_core/renderer_vulkan/vk_platform.h"
#include "video_core/renderer_vulkan/vk_scheduler.h"
#include "video_core/renderer_vulkan/vk_shader_util.h"

#include "video_core/host_shaders/host_spirv.h"

namespace VideoCore {

static constexpr size_t MaxPageFaults = 1024;
static constexpr size_t PageFaultAreaSize = MaxPageFaults * sizeof(u64);

FaultManager::FaultManager(const Vulkan::Instance& instance, Vulkan::Scheduler& scheduler_,
                           BufferCache& buffer_cache_, u32 sparse_pagebits, u64 sparse_num_pages_)
    : device{static_cast<VkDevice>(instance.GetDevice())}, scheduler{scheduler_},
      buffer_cache{buffer_cache_}, sparse_pagesize{1ULL << sparse_pagebits},
      sparse_num_pages{sparse_num_pages_}, fault_buffer_size{sparse_num_pages_ / 8},
      fault_buffer{instance, 0, fault_buffer_size, MemoryType::DeviceLocal},
      download_buffer{instance, 0, MaxPendingFaults * PageFaultAreaSize, MemoryType::HostCached} {
    const vk::Device vk_device{device};
    Vulkan::SetObjectName(vk_device, fault_buffer.Handle(), "Fault Buffer");

    VkDescriptorSetLayoutBinding bindings[2]{};
    for (u32 i = 0; i < 2; ++i) {
        bindings[i].binding = i;
        bindings[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        bindings[i].descriptorCount = 1;
        bindings[i].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    }
    VkDescriptorSetLayoutCreateInfo desc_layout_ci{};
    desc_layout_ci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    desc_layout_ci.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR;
    desc_layout_ci.bindingCount = 2;
    desc_layout_ci.pBindings = bindings;
    Vulkan::Check(static_cast<vk::Result>(
        vkCreateDescriptorSetLayout(device, &desc_layout_ci, nullptr, &fault_process_desc_layout)));

    std::vector<u32> words(HOST_SPIRV_MAX_WORDS);
    const size_t num_words = host_spirv_fault_buffer_process_comp(words.data(), words.size(),
                                                                  sparse_pagebits, MaxPageFaults);
    const auto module = Vulkan::CompileSPV({words.data(), num_words}, vk_device);
    Vulkan::SetObjectName(vk_device, module, "Fault Buffer Parser");

    VkPipelineLayoutCreateInfo layout_info{};
    layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layout_info.setLayoutCount = 1U;
    layout_info.pSetLayouts = &fault_process_desc_layout;
    Vulkan::Check(static_cast<vk::Result>(
        vkCreatePipelineLayout(device, &layout_info, nullptr, &fault_process_pipeline_layout)));

    VkComputePipelineCreateInfo pipeline_info{};
    pipeline_info.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    pipeline_info.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    pipeline_info.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    pipeline_info.stage.module = static_cast<VkShaderModule>(module);
    pipeline_info.stage.pName = "main";
    pipeline_info.layout = fault_process_pipeline_layout;
    Vulkan::Check(static_cast<vk::Result>(vkCreateComputePipelines(
        device, VK_NULL_HANDLE, 1, &pipeline_info, nullptr, &fault_process_pipeline)));
    Vulkan::SetObjectName(vk_device, vk::Pipeline{fault_process_pipeline},
                          "Fault Buffer Parser Pipeline");

    vkDestroyShaderModule(device, static_cast<VkShaderModule>(module), nullptr);
}

FaultManager::~FaultManager() {
    vkDestroyPipeline(device, fault_process_pipeline, nullptr);
    vkDestroyPipelineLayout(device, fault_process_pipeline_layout, nullptr);
    vkDestroyDescriptorSetLayout(device, fault_process_desc_layout, nullptr);
}

void FaultManager::ProcessFaultBuffer() {
    if (u64 wait_tick = fault_areas[current_area]) {
        scheduler.Wait(wait_tick);
        scheduler.PopPendingOperations();
    }

    const u32 offset = current_area * PageFaultAreaSize;
    u8* mapped = download_buffer.mapped_data.data() + offset;
    std::memset(mapped, 0, PageFaultAreaSize);

    const VkBuffer fault_handle = static_cast<VkBuffer>(fault_buffer.Handle());
    VkBufferMemoryBarrier2 pre_barrier{};
    pre_barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2;
    pre_barrier.srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
    pre_barrier.srcAccessMask = VK_ACCESS_2_SHADER_WRITE_BIT;
    pre_barrier.dstStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
    pre_barrier.dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT;
    pre_barrier.buffer = fault_handle;
    pre_barrier.offset = 0;
    pre_barrier.size = fault_buffer_size;
    VkBufferMemoryBarrier2 post_barrier = pre_barrier;
    post_barrier.srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
    post_barrier.srcAccessMask = VK_ACCESS_2_SHADER_WRITE_BIT;
    post_barrier.dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
    post_barrier.dstAccessMask = VK_ACCESS_2_SHADER_WRITE_BIT;

    VkDescriptorBufferInfo fault_buffer_info{};
    fault_buffer_info.buffer = fault_handle;
    fault_buffer_info.offset = 0;
    fault_buffer_info.range = fault_buffer_size;
    VkDescriptorBufferInfo download_info{};
    download_info.buffer = static_cast<VkBuffer>(download_buffer.Handle());
    download_info.offset = offset;
    download_info.range = PageFaultAreaSize;
    VkWriteDescriptorSet writes[2]{};
    for (u32 i = 0; i < 2; ++i) {
        writes[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[i].dstSet = VK_NULL_HANDLE;
        writes[i].dstBinding = i;
        writes[i].dstArrayElement = 0;
        writes[i].descriptorCount = 1;
        writes[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    }
    writes[0].pBufferInfo = &fault_buffer_info;
    writes[1].pBufferInfo = &download_info;

    VkDependencyInfo pre_dependency{};
    pre_dependency.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
    pre_dependency.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
    pre_dependency.bufferMemoryBarrierCount = 1;
    pre_dependency.pBufferMemoryBarriers = &pre_barrier;
    VkDependencyInfo post_dependency = pre_dependency;
    post_dependency.pBufferMemoryBarriers = &post_barrier;

    scheduler.EndRendering();
    const auto cmdbuf = static_cast<VkCommandBuffer>(scheduler.CommandBuffer());
    vkCmdPipelineBarrier2(cmdbuf, &pre_dependency);
    vkCmdBindPipeline(cmdbuf, VK_PIPELINE_BIND_POINT_COMPUTE, fault_process_pipeline);
    vkCmdPushDescriptorSetKHR(cmdbuf, VK_PIPELINE_BIND_POINT_COMPUTE, fault_process_pipeline_layout,
                              0, 2, writes);
    // 1 bit per page, 32 pages per workgroup
    const u32 num_threads = sparse_num_pages / 32;
    const u32 num_workgroups = Common::DivCeil(num_threads, 64u);
    vkCmdDispatch(cmdbuf, num_workgroups, 1, 1);

    vkCmdPipelineBarrier2(cmdbuf, &post_dependency);

    scheduler.DeferOperation([this, mapped, area = current_area] {
        fault_ranges.Clear();
        const u64* fault_buf = std::bit_cast<const u64*>(mapped);
        const u32 fault_count = fault_buf[0];
        for (u32 i = 1; i <= fault_count; ++i) {
            fault_ranges.Add(fault_buf[i], sparse_pagesize);
            LOG_INFO(Render_Vulkan, "Accessed non-GPU cached memory at 0x%" PRIx64, fault_buf[i]);
        }
        fault_ranges.ForEach([&](VAddr start, VAddr end) {
            ASSERT_MSG((end - start) <= std::numeric_limits<u32>::max(),
                       "Buffer size is too large");
            (void)buffer_cache.ObtainBuffer(start, end - start);
        });
        fault_areas[area] = 0;
    });

    fault_areas[current_area++] = scheduler.CurrentTick();
    current_area %= MaxPendingFaults;
}

} // namespace VideoCore
