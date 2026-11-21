// SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <encodings/xxh3.h>
#include "common/small_vector.h"

#include "shader_recompiler/frontend/fetch_shader.h"
#include "video_core/amdgpu/regs_color.h"
#include "video_core/amdgpu/regs_depth.h"
#include "video_core/amdgpu/regs_primitive.h"
#include "video_core/renderer_vulkan/vk_pipeline_common.h"

namespace VideoCore {
class BufferCache;
class TextureCache;
} // namespace VideoCore

namespace Vulkan {

static constexpr u32 MaxShaderStages = static_cast<u32>(Shader::SwStage::NumLogicalStages);
static constexpr u32 MaxVertexBufferCount = 32;

class Instance;
class Scheduler;
class DescriptorHeap;

template <typename T>
using VertexInputs = Common::StaticVector<T, MaxVertexBufferCount>;

struct GraphicsPipelineKey {
    size_t stage_hashes[MaxShaderStages];
    VkFormat vertex_buffer_formats[MaxVertexBufferCount];
    u32 patch_control_points;
    u32 num_color_attachments;
    Shader::PsColorBuffer color_buffers[AmdGpu::NUM_COLOR_BUFFERS];
    AmdGpu::BlendControl blend_controls[AmdGpu::NUM_COLOR_BUFFERS];
    VkColorComponentFlags write_masks[AmdGpu::NUM_COLOR_BUFFERS];
    AmdGpu::ColorBufferMask cb_shader_mask;
    AmdGpu::ColorControl::LogicOp logic_op;
    u8 num_samples;
    u8 depth_samples;
    u8 color_samples[AmdGpu::NUM_COLOR_BUFFERS];
    u32 mrt_mask;
    struct {
        AmdGpu::DepthBuffer::ZFormat z_format : 2;
        AmdGpu::DepthBuffer::StencilFormat stencil_format : 1;
        u32 depth_clamp_enable : 1;
        u32 sample_locations : 1; // the game's own sample positions, set before each draw
    };
    struct {
        AmdGpu::PrimitiveType prim_type : 5;
        AmdGpu::PolygonMode polygon_mode : 2;
        AmdGpu::ClipSpace clip_space : 1;
        AmdGpu::ProvokingVtxLast provoking_vtx_last : 1;
        u32 depth_clip_enable : 1;
    };

    GraphicsPipelineKey() {
        memset(this, 0, sizeof(*this));
    }

    bool operator==(const GraphicsPipelineKey& key) const noexcept {
        return memcmp(this, &key, sizeof(key)) == 0;
    }

    /// The key's bytes hashed, as the pipeline cache files it
    u64 Hash() const noexcept {
        return xxh3_64(this, sizeof(*this), 0);
    }

    void Serialize(Serialization::Archive& ar) const;
    bool Deserialize(Serialization::Archive& ar);
};

class GraphicsPipeline : public Pipeline {
public:
    struct SerializationSupport {
        VertexInputs<VkVertexInputAttributeDescription> vertex_attributes{};
        VertexInputs<VkVertexInputBindingDescription> vertex_bindings{};
        VertexInputs<VkVertexInputBindingDivisorDescriptionEXT> divisors{};
        VkPipelineMultisampleStateCreateInfo multisampling{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
        // The SPIR-V the shader recompiler emits for the stages it adds, as it hands them over
        Common::HeapArray<u32> tcs{};
        Common::HeapArray<u32> tes{};
        Common::HeapArray<u32> fragment{};

        void Serialize(Serialization::Archive& ar) const;
        bool Deserialize(Serialization::Archive& ar);
    };

    /// @stages, @runtime_infos and @modules hold MaxShaderStages each
    GraphicsPipeline(const Instance& instance, Scheduler& scheduler, DescriptorHeap& desc_heap,
                     const Shader::Profile& profile, const GraphicsPipelineKey& key,
                     VkPipelineCache pipeline_cache, const Shader::Info* const* stages,
                     const Shader::RuntimeInfo* runtime_infos,
                     const Shader::Gcn::FetchShaderData* fetch_shader,
                     const VkShaderModule* modules, SerializationSupport& sdata, bool preloading);
    ~GraphicsPipeline();

    const Shader::Gcn::FetchShaderData& GetFetchShader() const noexcept {
        return fetch_shader;
    }

    const GraphicsPipelineKey& GetGraphicsKey() const {
        return key;
    }

    /// Gets the attributes and bindings for vertex inputs.
    template <typename Attribute, typename Binding>
    void GetVertexInputs(VertexInputs<Attribute>& attributes, VertexInputs<Binding>& bindings,
                         VertexInputs<VkVertexInputBindingDivisorDescriptionEXT>& divisors,
                         VertexInputs<AmdGpu::Buffer>& guest_buffers, u32 step_rate_0,
                         u32 step_rate_1) const;

private:
    void BuildDescSetLayout(bool preloading);

private:
    GraphicsPipelineKey key;
    Shader::Gcn::FetchShaderData fetch_shader{};
};

} // namespace Vulkan
