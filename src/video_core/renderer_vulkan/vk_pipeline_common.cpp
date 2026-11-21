// SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <stdio.h>

#include "shader_recompiler/resource.h"
#include "video_core/renderer_vulkan/vk_instance.h"
#include "video_core/renderer_vulkan/vk_pipeline_cache.h"
#include "video_core/renderer_vulkan/vk_pipeline_common.h"
#include "video_core/renderer_vulkan/vk_scheduler.h"

namespace Vulkan {

Pipeline::Pipeline(const Instance& instance_, Scheduler& scheduler_, DescriptorHeap& desc_heap_,
                   const Shader::Profile& profile_, VkPipelineCache pipeline_cache,
                   bool is_compute_ /*= false*/)
    : instance{instance_}, scheduler{scheduler_}, desc_heap{desc_heap_}, profile{profile_},
      device{instance_.GetDevice()}, is_compute{is_compute_} {}

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
               ResultName(result));
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
               ResultName(result));
}

void Pipeline::BindResources(VkWriteDescriptorSet* writes, u32 num_writes,
                             const Shader::PushData& push_data) const {
    const VkCommandBuffer cmdbuf = scheduler.CommandBuffer();
    const VkPipelineBindPoint bind_point =
        IsCompute() ? VK_PIPELINE_BIND_POINT_COMPUTE : VK_PIPELINE_BIND_POINT_GRAPHICS;

    const VkShaderStageFlags stage_flags =
        IsCompute() ? VkShaderStageFlags{VK_SHADER_STAGE_COMPUTE_BIT} : AllGraphicsStageBits;
    vkCmdPushConstants(cmdbuf, pipeline_layout, stage_flags, 0u, sizeof(push_data), &push_data);

    // Bind descriptor set.
    if (num_writes == 0) {
        return;
    }

    if (uses_push_descriptors) {
        vkCmdPushDescriptorSetKHR(cmdbuf, bind_point, pipeline_layout, 0, num_writes, writes);
        return;
    }

    const VkDescriptorSet desc_set =
        static_cast<VkDescriptorSet>(desc_heap.Commit(VkDescriptorSetLayout{desc_layout}));
    for (u32 i = 0; i < num_writes; ++i) {
        writes[i].dstSet = desc_set;
    }
    vkUpdateDescriptorSets(device, num_writes, writes, 0, nullptr);
    vkCmdBindDescriptorSets(cmdbuf, bind_point, pipeline_layout, 0, 1, &desc_set, 0, nullptr);
}

void Pipeline::GetDebugString(char* out, size_t size) const {
    size_t length = 0;
    out[0] = '\0';
    for (u32 i = 0; i < Shader::MaxStageTypes; ++i) {
        const Shader::Info* const stage = stages[i];
        if (!stage || length >= size) {
            continue;
        }
        char name[PipelineCache::ShaderNameSize];
        PipelineCache::GetShaderName(name, sizeof(name), stage->hw_stage, stage->pgm_hash);
        const int written = snprintf(out + length, size - length, "%s%s", length ? "," : "", name);
        if (written > 0) {
            length += static_cast<size_t>(written);
        }
    }
}

} // namespace Vulkan
