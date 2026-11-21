// SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "shader_recompiler/profile.h"
#include "shader_recompiler/runtime_info.h"
#include "video_core/renderer_vulkan/vk_common.h"

#include "common/small_vector.h"

namespace Shader {
struct Info;
struct PushData;
} // namespace Shader

namespace Vulkan {

static constexpr vk::ShaderStageFlags AllGraphicsStageBits =
    vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eTessellationControl |
    vk::ShaderStageFlagBits::eTessellationEvaluation | vk::ShaderStageFlagBits::eGeometry |
    vk::ShaderStageFlagBits::eFragment;

class Instance;
class Scheduler;
class DescriptorHeap;

class Pipeline {
public:
    Pipeline(const Instance& instance, Scheduler& scheduler, DescriptorHeap& desc_heap,
             const Shader::Profile& profile, vk::PipelineCache pipeline_cache,
             bool is_compute = false);
    virtual ~Pipeline();

    Pipeline(const Pipeline&) = delete;
    Pipeline& operator=(const Pipeline&) = delete;

    vk::Pipeline Handle() const noexcept {
        return vk::Pipeline{pipeline};
    }

    vk::PipelineLayout GetLayout() const noexcept {
        return vk::PipelineLayout{pipeline_layout};
    }

    /// The stages the pipeline binds from, at *@out (some null for a graphics pipeline): the
    /// last for a compute pipeline, the others for a graphics one. Returns how many.
    u32 GetStages(const Shader::Info* const** out) const {
        static_assert(static_cast<u32>(Shader::SwStage::Compute) == Shader::MaxStageTypes - 1);
        if (is_compute) {
            *out = stages + Shader::MaxStageTypes - 1;
            return 1;
        }
        *out = stages;
        return Shader::MaxStageTypes - 1;
    }

    const Shader::Info& GetStage(Shader::SwStage stage) const noexcept {
        return *stages[u32(stage)];
    }

    /// The guest's shader at @stage, null where the pipeline has none
    const Shader::Info* StageInfo(Shader::SwStage stage) const noexcept {
        return stages[u32(stage)];
    }

    /// The hash of the guest's shader at @stage, 0 where the pipeline has none
    u64 StageHash(Shader::SwStage stage) const noexcept {
        const Shader::Info* info = stages[u32(stage)];
        return info ? info->pgm_hash : 0;
    }

    bool IsCompute() const {
        return is_compute;
    }

    /// The guest's shaders the pipeline binds from: a pipeline made on the compiler thread was
    /// made from copies of them, and is given the cache's own once it is filed
    /// @infos holds Shader::MaxStageTypes stages
    void SetStages(const Shader::Info* const* infos) {
        for (u32 i = 0; i < Shader::MaxStageTypes; ++i) {
            stages[i] = infos[i];
        }
    }

    /// Binds the push data and @num_writes descriptor writes at @set_writes (their dstSet is
    /// filled in where a descriptor set is used)
    void BindResources(VkWriteDescriptorSet* writes, u32 num_writes,
                       const Shader::PushData& push_data) const;

protected:
    /// The names of the pipeline's shaders, comma separated, into @out
    void GetDebugString(char* out, size_t size) const;

    /// Room enough for GetDebugString
    static constexpr size_t DebugStringSize = 192;

    /// Creates desc_layout from @bindings, as push descriptors where uses_push_descriptors is set
    void CreateDescSetLayout(const VkDescriptorSetLayoutBinding* bindings, u32 num_bindings,
                             const char* kind);

    /// Creates pipeline_layout over desc_layout, with the push data visible to @stage_flags
    void CreatePipelineLayout(VkShaderStageFlags stage_flags, const char* kind);

    const Instance& instance;
    Scheduler& scheduler;
    DescriptorHeap& desc_heap;
    const Shader::Profile& profile;
    VkDevice device;
    VkPipeline pipeline{VK_NULL_HANDLE};
    VkPipelineLayout pipeline_layout{VK_NULL_HANDLE};
    VkDescriptorSetLayout desc_layout{VK_NULL_HANDLE};
    const Shader::Info* stages[Shader::MaxStageTypes]{};
    bool uses_push_descriptors{};
    bool is_compute;
};

} // namespace Vulkan
