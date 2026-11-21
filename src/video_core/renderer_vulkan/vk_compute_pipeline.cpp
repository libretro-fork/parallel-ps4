// SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "common/small_vector.h"

#include "shader_recompiler/info.h"
#include "video_core/renderer_vulkan/vk_compute_pipeline.h"
#include "video_core/renderer_vulkan/vk_instance.h"
#include "video_core/renderer_vulkan/vk_scheduler.h"

namespace Vulkan {

namespace {

using LayoutBindings = Common::SmallVector<VkDescriptorSetLayoutBinding, 32>;

/// Adds the binding of @count descriptors of @type at *@binding, and moves *@binding past it
void AddBinding(LayoutBindings& bindings, u32& binding, VkDescriptorType type, u32 count) {
    VkDescriptorSetLayoutBinding layout_binding{};
    layout_binding.binding = binding;
    layout_binding.descriptorType = type;
    layout_binding.descriptorCount = count;
    layout_binding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    bindings.push_back(layout_binding);
    binding += count;
}

} // Anonymous namespace

ComputePipeline::ComputePipeline(const Instance& instance, Scheduler& scheduler,
                                 DescriptorHeap& desc_heap, const Shader::Profile& profile,
                                 VkPipelineCache pipeline_cache, ComputePipelineKey compute_key_,
                                 const Shader::Info& info_, VkShaderModule module,
                                 SerializationSupport& sdata, bool preloading /*=false*/)
    : Pipeline{instance, scheduler, desc_heap, profile, pipeline_cache, true},
      compute_key{compute_key_} {
    stages[int(Shader::SwStage::Compute)] = &info_;
    const Shader::Info* const info = &info_;
    char debug_str[DebugStringSize];
    GetDebugString(debug_str, sizeof(debug_str));

    u32 binding{};
    LayoutBindings bindings;
    // During deserialization the sharps are out of reach, and only the "is storage" flag (not
    // even effective yet) would come from them, so every buffer is a storage buffer
    for (size_t i = 0; i < info->buffers.size(); ++i) {
        AddBinding(bindings, binding, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1);
    }
    for (const Shader::ImageResource& image : info->images) {
        AddBinding(bindings, binding,
                   image.is_written ? VK_DESCRIPTOR_TYPE_STORAGE_IMAGE
                                    : VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
                   image.NumBindings(*info));
    }
    for (size_t i = 0; i < info->samplers.size(); ++i) {
        AddBinding(bindings, binding, VK_DESCRIPTOR_TYPE_SAMPLER, 1);
    }

    uses_push_descriptors = binding < instance.MaxPushDescriptors();
    CreateDescSetLayout(bindings.data(), static_cast<u32>(bindings.size()), "compute");
    CreatePipelineLayout(VK_SHADER_STAGE_COMPUTE_BIT, "compute");
    const VkDevice vk_device{device};
    SetObjectName(vk_device, pipeline_layout, "Compute PipelineLayout %s", debug_str);

    VkPipelineShaderStageRequiredSubgroupSizeCreateInfo subgroup_size_ci{};
    subgroup_size_ci.sType =
        VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_REQUIRED_SUBGROUP_SIZE_CREATE_INFO;
    subgroup_size_ci.requiredSubgroupSize = 64;
    VkComputePipelineCreateInfo compute_pipeline_ci{};
    compute_pipeline_ci.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    compute_pipeline_ci.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    compute_pipeline_ci.stage.pNext =
        instance.IsSubgroupSize64Supported() ? &subgroup_size_ci : nullptr;
    compute_pipeline_ci.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    compute_pipeline_ci.stage.module = module;
    compute_pipeline_ci.stage.pName = "main";
    compute_pipeline_ci.layout = pipeline_layout;
    const VkResult pipeline_result = vkCreateComputePipelines(
        device, pipeline_cache, 1, &compute_pipeline_ci, nullptr, &pipeline);
    ASSERT_MSG(pipeline_result == VK_SUCCESS, "Failed to create compute pipeline: %s",
               ResultName(pipeline_result));
    SetObjectName(vk_device, pipeline, "Compute Pipeline %s", debug_str);
}

ComputePipeline::~ComputePipeline() = default;

} // namespace Vulkan
