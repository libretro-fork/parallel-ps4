// SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "common/small_vector.h"
#include "common/string_util.h"

#include "shader_recompiler/resource.h"
#include "video_core/renderer_vulkan/vk_instance.h"
#include "video_core/renderer_vulkan/vk_pipeline_cache.h"
#include "video_core/renderer_vulkan/vk_pipeline_common.h"
#include "video_core/renderer_vulkan/vk_scheduler.h"

namespace Vulkan {

Pipeline::Pipeline(const Instance& instance_, Scheduler& scheduler_, DescriptorHeap& desc_heap_,
                   const Shader::Profile& profile_, vk::PipelineCache pipeline_cache,
                   bool is_compute_ /*= false*/)
    : instance{instance_}, scheduler{scheduler_}, desc_heap{desc_heap_}, profile{profile_},
      device{static_cast<VkDevice>(instance_.GetDevice())}, is_compute{is_compute_} {}

Pipeline::~Pipeline() {
    vkDestroyPipeline(device, pipeline, nullptr);
    vkDestroyPipelineLayout(device, pipeline_layout, nullptr);
    vkDestroyDescriptorSetLayout(device, desc_layout, nullptr);
}

void Pipeline::CreateDescSetLayout(const VkDescriptorSetLayoutBinding* bindings, u32 num_bindings,
                                   const char* kind) {
    VkDescriptorSetLayoutCreateInfo desc_layout_ci{};
    desc_layout_ci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    desc_layout_ci.flags =
        uses_push_descriptors ? VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR : 0;
    desc_layout_ci.bindingCount = num_bindings;
    desc_layout_ci.pBindings = bindings;
    const VkResult result =
        vkCreateDescriptorSetLayout(device, &desc_layout_ci, nullptr, &desc_layout);
    ASSERT_MSG(result == VK_SUCCESS, "Failed to create %s descriptor set layout: %s", kind,
               vk::to_string(static_cast<vk::Result>(result)).c_str());
}

void Pipeline::CreatePipelineLayout(VkShaderStageFlags stage_flags, const char* kind) {
    VkPushConstantRange push_constants{};
    push_constants.stageFlags = stage_flags;
    push_constants.offset = 0;
    push_constants.size = sizeof(Shader::PushData);
    VkPipelineLayoutCreateInfo layout_info{};
    layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layout_info.setLayoutCount = 1U;
    layout_info.pSetLayouts = &desc_layout;
    layout_info.pushConstantRangeCount = 1U;
    layout_info.pPushConstantRanges = &push_constants;
    const VkResult result = vkCreatePipelineLayout(device, &layout_info, nullptr, &pipeline_layout);
    ASSERT_MSG(result == VK_SUCCESS, "Failed to create %s pipeline layout: %s", kind,
               vk::to_string(static_cast<vk::Result>(result)).c_str());
}

void Pipeline::BindResources(DescriptorWrites& set_writes,
                             const Shader::PushData& push_data) const {
    const auto cmdbuf = static_cast<VkCommandBuffer>(scheduler.CommandBuffer());
    const auto bind_point =
        IsCompute() ? VK_PIPELINE_BIND_POINT_COMPUTE : VK_PIPELINE_BIND_POINT_GRAPHICS;

    const auto stage_flags = IsCompute() ? VkShaderStageFlags{VK_SHADER_STAGE_COMPUTE_BIT}
                                         : static_cast<VkShaderStageFlags>(AllGraphicsStageBits);
    vkCmdPushConstants(cmdbuf, pipeline_layout, stage_flags, 0u, sizeof(push_data), &push_data);

    // Bind descriptor set.
    if (set_writes.empty()) {
        return;
    }

    // vulkan.hpp's structs are layout-compatible with the C ones
    static_assert(sizeof(vk::WriteDescriptorSet) == sizeof(VkWriteDescriptorSet));
    auto* writes = reinterpret_cast<VkWriteDescriptorSet*>(set_writes.data());
    const auto num_writes = static_cast<u32>(set_writes.size());
    if (uses_push_descriptors) {
        vkCmdPushDescriptorSetKHR(cmdbuf, bind_point, pipeline_layout, 0, num_writes, writes);
        return;
    }

    const auto desc_set =
        static_cast<VkDescriptorSet>(desc_heap.Commit(vk::DescriptorSetLayout{desc_layout}));
    for (u32 i = 0; i < num_writes; ++i) {
        writes[i].dstSet = desc_set;
    }
    vkUpdateDescriptorSets(device, num_writes, writes, 0, nullptr);
    vkCmdBindDescriptorSets(cmdbuf, bind_point, pipeline_layout, 0, 1, &desc_set, 0, nullptr);
}

std::string Pipeline::GetDebugString() const {
    std::string stage_desc;
    for (const auto& stage : stages) {
        if (stage) {
            const auto shader_name = PipelineCache::GetShaderName(stage->hw_stage, stage->pgm_hash);
            if (stage_desc.empty()) {
                stage_desc = shader_name;
            } else {
                stage_desc =
                    Common::StringFromFormat("%s,%s", (stage_desc).c_str(), (shader_name).c_str());
            }
        }
    }
    return stage_desc;
}

} // namespace Vulkan
