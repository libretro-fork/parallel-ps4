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

static constexpr auto AllGraphicsStageBits =
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

    auto GetStages() const {
        static_assert(static_cast<u32>(Shader::SwStage::Compute) == Shader::MaxStageTypes - 1);
        if (is_compute) {
            return std::span{stages.cend() - 1, stages.cend()};
        } else {
            return std::span{stages.cbegin(), stages.cend() - 1};
        }
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
        const auto* info = stages[u32(stage)];
        return info ? info->pgm_hash : 0;
    }

    bool IsCompute() const {
        return is_compute;
    }

    /// The guest's shaders the pipeline binds from: a pipeline made on the compiler thread was
    /// made from copies of them, and is given the cache's own once it is filed
    void SetStages(std::span<const Shader::Info* const, Shader::MaxStageTypes> infos) {
        std::ranges::copy(infos, stages.begin());
    }

    using DescriptorWrites = std::vector<vk::WriteDescriptorSet>;
    void BindResources(DescriptorWrites& set_writes, const Shader::PushData& push_data) const;

protected:
    [[nodiscard]] std::string GetDebugString() const;

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
    std::array<const Shader::Info*, Shader::MaxStageTypes> stages{};
    bool uses_push_descriptors{};
    bool is_compute;
};

} // namespace Vulkan
