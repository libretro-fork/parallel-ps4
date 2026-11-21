// SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "common/small_vector.h"

#include "common/assert.h"
#include "shader_recompiler/backend/spirv/emit_spirv_discard_frag.h"
#include "shader_recompiler/backend/spirv/emit_spirv_quad_rect.h"
#include "video_core/renderer_vulkan/liverpool_to_vk.h"
#include "video_core/renderer_vulkan/vk_graphics_pipeline.h"
#include "video_core/renderer_vulkan/vk_instance.h"
#include "video_core/renderer_vulkan/vk_scheduler.h"
#include "video_core/renderer_vulkan/vk_shader_util.h"

namespace Vulkan {

using Shader::Backend::SPIRV::AuxShaderType;

static constexpr VkShaderStageFlagBits LogicalStageToStageBit[] = {
    VK_SHADER_STAGE_FRAGMENT_BIT,
    VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT,
    VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT,
    VK_SHADER_STAGE_VERTEX_BIT,
    VK_SHADER_STAGE_GEOMETRY_BIT,
    VK_SHADER_STAGE_COMPUTE_BIT,
};

/// A stage of @module for @stage_bit, as the pipeline is made with it
static VkPipelineShaderStageCreateInfo ShaderStage(VkShaderStageFlagBits stage_bit,
                                                   VkShaderModule module) {
    return VkPipelineShaderStageCreateInfo{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .stage = stage_bit,
        .module = module,
        .pName = "main",
    };
}

/// A module of the SPIR-V the shader recompiler made for a stage it adds
static VkShaderModule AuxModule(const std::vector<u32>& code, const Instance& instance) {
    return static_cast<VkShaderModule>(CompileSPV(code.data(), code.size(), instance.GetDevice()));
}

/// Whether a blend factor reads the source alpha
static bool ReadsSrcAlpha(VkBlendFactor factor) {
    return factor == VK_BLEND_FACTOR_SRC_ALPHA || factor == VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
}

/// Whether a min or max blend has a factor other than one, which the GPU has and Vulkan does not
static bool IsScaledMinMax(VkBlendOp op, VkBlendFactor src, VkBlendFactor dst) {
    return (op == VK_BLEND_OP_MIN || op == VK_BLEND_OP_MAX) &&
           (src != VK_BLEND_FACTOR_ONE || dst != VK_BLEND_FACTOR_ONE);
}

/// LiverpoolToVK's min/max normalisation of one blend, on the C API's enums
static void NormalizeMinMaxBlend(VkBlendOp& op, VkBlendFactor& src, VkBlendFactor& dst,
                                 AmdGpu::NumberFormat num_format) {
    vk::BlendOp vk_op = static_cast<vk::BlendOp>(op);
    vk::BlendFactor vk_src = static_cast<vk::BlendFactor>(src);
    vk::BlendFactor vk_dst = static_cast<vk::BlendFactor>(dst);
    LiverpoolToVK::NormalizeMinMaxBlend(vk_op, vk_src, vk_dst, num_format);
    op = static_cast<VkBlendOp>(vk_op);
    src = static_cast<VkBlendFactor>(vk_src);
    dst = static_cast<VkBlendFactor>(vk_dst);
}

GraphicsPipeline::GraphicsPipeline(const Instance& instance, Scheduler& scheduler,
                                   DescriptorHeap& desc_heap, const Shader::Profile& profile,
                                   const GraphicsPipelineKey& key_, VkPipelineCache pipeline_cache,
                                   const Shader::Info* const* infos,
                                   const Shader::RuntimeInfo* runtime_infos,
                                   const Shader::Gcn::FetchShaderData* fetch_shader_,
                                   const VkShaderModule* modules, SerializationSupport& sdata,
                                   bool preloading)
    : Pipeline{instance, scheduler, desc_heap, profile, pipeline_cache}, key{key_} {
    if (fetch_shader_) {
        fetch_shader = *fetch_shader_;
    }

    const vk::Device vk_device{device};
    SetStages(infos);
    BuildDescSetLayout(preloading);
    char debug_str[DebugStringSize];
    GetDebugString(debug_str, sizeof(debug_str));

    CreatePipelineLayout(AllGraphicsStageBits, "graphics");
    SetObjectName(vk_device, vk::PipelineLayout{pipeline_layout}, "Graphics PipelineLayout %s",
                  debug_str);

    const Shader::HwFragmentRuntimeInfo& fs_info =
        runtime_infos[u32(Shader::SwStage::Fragment)].hw.fs;
    if (!preloading) {
        VertexInputs<AmdGpu::Buffer> guest_buffers;
        if (!instance.IsVertexInputDynamicState()) {
            const Shader::SwVertexRuntimeInfo& vs_info =
                runtime_infos[u32(Shader::SwStage::Vertex)].sw.vs;
            GetVertexInputs(sdata.vertex_attributes, sdata.vertex_bindings, sdata.divisors,
                            guest_buffers, vs_info.step_rate_0, vs_info.step_rate_1);
        }
    }

    const VkPipelineVertexInputDivisorStateCreateInfo divisor_state = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_DIVISOR_STATE_CREATE_INFO,
        .vertexBindingDivisorCount = static_cast<u32>(sdata.divisors.size()),
        .pVertexBindingDivisors = sdata.divisors.data(),
    };

    const VkPipelineVertexInputStateCreateInfo vertex_input_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .pNext = sdata.divisors.empty() ? nullptr : &divisor_state,
        .vertexBindingDescriptionCount = static_cast<u32>(sdata.vertex_bindings.size()),
        .pVertexBindingDescriptions = sdata.vertex_bindings.data(),
        .vertexAttributeDescriptionCount = static_cast<u32>(sdata.vertex_attributes.size()),
        .pVertexAttributeDescriptions = sdata.vertex_attributes.data(),
    };

    const VkPipelineInputAssemblyStateCreateInfo input_assembly = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .topology = static_cast<VkPrimitiveTopology>(LiverpoolToVK::PrimitiveType(key.prim_type)),
    };

    const bool is_rect_list = key.prim_type == AmdGpu::PrimitiveType::RectList;
    const bool is_quad_list = key.prim_type == AmdGpu::PrimitiveType::QuadList;
    const VkPipelineTessellationStateCreateInfo tessellation_state = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO,
        .patchControlPoints = is_rect_list ? 3U : (is_quad_list ? 4U : key.patch_control_points),
    };

    // The rasterization state, with the provoking vertex and depth clip where the device has
    // them
    VkPipelineRasterizationDepthClipStateCreateInfoEXT depth_clip = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_DEPTH_CLIP_STATE_CREATE_INFO_EXT,
        .depthClipEnable = key.depth_clip_enable,
    };
    VkPipelineRasterizationProvokingVertexStateCreateInfoEXT provoking_vertex = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_PROVOKING_VERTEX_STATE_CREATE_INFO_EXT,
        .provokingVertexMode = key.provoking_vtx_last == AmdGpu::ProvokingVtxLast::First
                                   ? VK_PROVOKING_VERTEX_MODE_FIRST_VERTEX_EXT
                                   : VK_PROVOKING_VERTEX_MODE_LAST_VERTEX_EXT,
    };
    VkPipelineRasterizationStateCreateInfo rasterization = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .depthClampEnable = key.depth_clamp_enable &&
                            (!key.depth_clip_enable || instance.IsDepthClipEnableSupported()),
        .rasterizerDiscardEnable = VK_FALSE,
        .polygonMode = static_cast<VkPolygonMode>(LiverpoolToVK::PolygonMode(key.polygon_mode)),
        .lineWidth = 1.0f,
    };
    const void** raster_next = const_cast<const void**>(&rasterization.pNext);
    if (instance.IsProvokingVertexSupported()) {
        *raster_next = &provoking_vertex;
        raster_next = const_cast<const void**>(&provoking_vertex.pNext);
    }
    if (instance.IsDepthClipEnableSupported()) {
        *raster_next = &depth_clip;
    }

    if (!preloading) {
        sdata.multisampling = VkPipelineMultisampleStateCreateInfo{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
            .rasterizationSamples = static_cast<VkSampleCountFlagBits>(
                LiverpoolToVK::NumSamples(key.num_samples, instance.GetColorSampleCounts() &
                                                               instance.GetDepthSampleCounts())),
            .sampleShadingEnable =
                fs_info.addr_flags.persp_sample_ena || fs_info.addr_flags.linear_sample_ena,
        };
    }

    const VkPipelineViewportDepthClipControlCreateInfoEXT clip_control = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_DEPTH_CLIP_CONTROL_CREATE_INFO_EXT,
        .negativeOneToOne = key.clip_space == AmdGpu::ClipSpace::MinusWToW,
    };

    const VkPipelineViewportStateCreateInfo viewport_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .pNext = instance.IsDepthClipControlSupported() ? &clip_control : nullptr,
    };

    Common::StaticVector<VkDynamicState, 32> dynamic_states = {
        VK_DYNAMIC_STATE_VIEWPORT_WITH_COUNT,  VK_DYNAMIC_STATE_SCISSOR_WITH_COUNT,
        VK_DYNAMIC_STATE_BLEND_CONSTANTS,      VK_DYNAMIC_STATE_DEPTH_TEST_ENABLE,
        VK_DYNAMIC_STATE_DEPTH_WRITE_ENABLE,   VK_DYNAMIC_STATE_DEPTH_COMPARE_OP,
        VK_DYNAMIC_STATE_DEPTH_BIAS_ENABLE,    VK_DYNAMIC_STATE_DEPTH_BIAS,
        VK_DYNAMIC_STATE_STENCIL_TEST_ENABLE,  VK_DYNAMIC_STATE_STENCIL_REFERENCE,
        VK_DYNAMIC_STATE_STENCIL_COMPARE_MASK, VK_DYNAMIC_STATE_STENCIL_WRITE_MASK,
        VK_DYNAMIC_STATE_STENCIL_OP,           VK_DYNAMIC_STATE_CULL_MODE,
        VK_DYNAMIC_STATE_FRONT_FACE,           VK_DYNAMIC_STATE_RASTERIZER_DISCARD_ENABLE,
        VK_DYNAMIC_STATE_LINE_WIDTH,           VK_DYNAMIC_STATE_PRIMITIVE_RESTART_ENABLE,
    };

    if (instance.IsDepthBoundsSupported()) {
        dynamic_states.push_back(VK_DYNAMIC_STATE_DEPTH_BOUNDS_TEST_ENABLE);
        dynamic_states.push_back(VK_DYNAMIC_STATE_DEPTH_BOUNDS);
    }
    if (instance.IsDynamicColorWriteMaskSupported()) {
        dynamic_states.push_back(VK_DYNAMIC_STATE_COLOR_WRITE_MASK_EXT);
    }
    // The positions themselves are dynamic state, set before a draw
    const VkPipelineSampleLocationsStateCreateInfoEXT sample_locations = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_SAMPLE_LOCATIONS_STATE_CREATE_INFO_EXT,
        .sampleLocationsEnable = VK_TRUE,
    };
    VkPipelineMultisampleStateCreateInfo multisampling = sdata.multisampling;
    // What the cache held is the state itself, never what it pointed at
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.pNext = nullptr;
    multisampling.pSampleMask = nullptr;
    if (key.sample_locations) {
        multisampling.pNext = &sample_locations;
        dynamic_states.push_back(VK_DYNAMIC_STATE_SAMPLE_LOCATIONS_EXT);
    }
    if (instance.IsVertexInputDynamicState()) {
        dynamic_states.push_back(VK_DYNAMIC_STATE_VERTEX_INPUT_EXT);
    } else if (!sdata.vertex_bindings.empty()) {
        dynamic_states.push_back(VK_DYNAMIC_STATE_VERTEX_INPUT_BINDING_STRIDE);
    }

    const VkPipelineDynamicStateCreateInfo dynamic_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        .dynamicStateCount = static_cast<u32>(dynamic_states.size()),
        .pDynamicStates = dynamic_states.data(),
    };

    Common::StaticVector<VkPipelineShaderStageCreateInfo, MaxShaderStages> shader_stages;
    u32 stage = u32(Shader::SwStage::Vertex);
    if (infos[stage]) {
        shader_stages.push_back(ShaderStage(VK_SHADER_STAGE_VERTEX_BIT, modules[stage]));
    }
    stage = u32(Shader::SwStage::Geometry);
    if (infos[stage]) {
        shader_stages.push_back(ShaderStage(VK_SHADER_STAGE_GEOMETRY_BIT, modules[stage]));
    }
    stage = u32(Shader::SwStage::TessellationControl);
    if (infos[stage]) {
        shader_stages.push_back(
            ShaderStage(VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT, modules[stage]));
    } else if (is_rect_list || is_quad_list) {
        const AuxShaderType type =
            is_quad_list ? AuxShaderType::QuadListTCS : AuxShaderType::RectListTCS;
        if (!preloading) {
            sdata.tcs = Shader::Backend::SPIRV::EmitAuxilaryTessShader(type, fs_info);
        }
        shader_stages.push_back(
            ShaderStage(VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT, AuxModule(sdata.tcs, instance)));
    }
    stage = u32(Shader::SwStage::TessellationEval);
    if (infos[stage]) {
        shader_stages.push_back(
            ShaderStage(VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT, modules[stage]));
    } else if (is_rect_list || is_quad_list) {
        if (!preloading) {
            sdata.tes = Shader::Backend::SPIRV::EmitAuxilaryTessShader(
                AuxShaderType::PassthroughTES, fs_info);
        }
        shader_stages.push_back(ShaderStage(VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT,
                                            AuxModule(sdata.tes, instance)));
    }
    stage = u32(Shader::SwStage::Fragment);
    if (infos[stage]) {
        shader_stages.push_back(ShaderStage(VK_SHADER_STAGE_FRAGMENT_BIT, modules[stage]));
    } else if (fs_info.clip_distance_emulation) {
        if (!preloading) {
            const Shader::HwVertexRuntimeInfo& vs =
                runtime_infos[static_cast<u32>(Shader::SwStage::Vertex)].hw.vs;
            sdata.fragment = Shader::Backend::SPIRV::EmitDiscardFragmentShader(vs.outputs);
        }
        shader_stages.push_back(
            ShaderStage(VK_SHADER_STAGE_FRAGMENT_BIT, AuxModule(sdata.fragment, instance)));
    }

    const VkPipelineShaderStageRequiredSubgroupSizeCreateInfo subgroup_size_ci = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_REQUIRED_SUBGROUP_SIZE_CREATE_INFO,
        .requiredSubgroupSize = 64,
    };
    for (VkPipelineShaderStageCreateInfo& stage_ci : shader_stages) {
        stage_ci.pNext = instance.IsSubgroupSize64Supported() ? &subgroup_size_ci : nullptr;
    }

    const VkFormat depth_format = static_cast<VkFormat>(
        instance.GetSupportedFormat(LiverpoolToVK::DepthFormat(key.z_format, key.stencil_format),
                                    vk::FormatFeatureFlagBits2::eDepthStencilAttachment));
    VkFormat color_formats[Shader::IR::NumRenderTargets];
    for (u32 i = 0; i < key.num_color_attachments; ++i) {
        const Shader::PsColorBuffer& col_buf = key.color_buffers[i];
        const vk::Format format =
            LiverpoolToVK::SurfaceFormat(col_buf.data_format, col_buf.num_format);
        const vk::Format color_format =
            instance.GetSupportedFormat(format, vk::FormatFeatureFlagBits2::eColorAttachment);
        if (!instance.IsFormatSupported(color_format,
                                        vk::FormatFeatureFlagBits2::eColorAttachment)) {
            LOG_WARNING(Render_Vulkan,
                        "color buffer format %d does not support COLOR_ATTACHMENT_BIT",
                        static_cast<int>(color_format));
        }
        color_formats[i] = static_cast<VkFormat>(color_format);
    }

    VkSampleCountFlagBits color_samples[AmdGpu::NUM_COLOR_BUFFERS];
    for (u32 i = 0; i < AmdGpu::NUM_COLOR_BUFFERS; ++i) {
        const u8 num_samples = key.color_samples[i];
        color_samples[i] = num_samples
                               ? static_cast<VkSampleCountFlagBits>(LiverpoolToVK::NumSamples(
                                     num_samples, instance.GetColorSampleCounts()))
                               : VK_SAMPLE_COUNT_1_BIT;
    }
    const VkAttachmentSampleCountInfoAMD mixed_samples = {
        .sType = VK_STRUCTURE_TYPE_ATTACHMENT_SAMPLE_COUNT_INFO_AMD,
        .colorAttachmentCount = key.num_color_attachments,
        .pColorAttachmentSamples = color_samples,
        .depthStencilAttachmentSamples = static_cast<VkSampleCountFlagBits>(
            LiverpoolToVK::NumSamples(key.depth_samples, instance.GetDepthSampleCounts())),
    };

    const VkPipelineRenderingCreateInfo pipeline_rendering_ci = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
        .pNext = instance.IsMixedDepthSamplesSupported() ? &mixed_samples : nullptr,
        .colorAttachmentCount = key.num_color_attachments,
        .pColorAttachmentFormats = color_formats,
        .depthAttachmentFormat = key.z_format != AmdGpu::DepthBuffer::ZFormat::Invalid
                                     ? depth_format
                                     : VK_FORMAT_UNDEFINED,
        .stencilAttachmentFormat = key.stencil_format != AmdGpu::DepthBuffer::StencilFormat::Invalid
                                       ? depth_format
                                       : VK_FORMAT_UNDEFINED,
    };

    VkPipelineColorBlendAttachmentState attachments[AmdGpu::NUM_COLOR_BUFFERS];
    for (u32 i = 0; i < key.num_color_attachments; i++) {
        const AmdGpu::BlendControl& control = key.blend_controls[i];

        const VkBlendFactor src_color =
            static_cast<VkBlendFactor>(LiverpoolToVK::BlendFactor(control.color_src_factor));
        const VkBlendFactor dst_color =
            static_cast<VkBlendFactor>(LiverpoolToVK::BlendFactor(control.color_dst_factor));
        const VkBlendOp color_blend =
            static_cast<VkBlendOp>(LiverpoolToVK::BlendOp(control.color_func));

        const VkBlendFactor src_alpha =
            control.separate_alpha_blend
                ? static_cast<VkBlendFactor>(LiverpoolToVK::BlendFactor(control.alpha_src_factor))
                : src_color;
        const VkBlendFactor dst_alpha =
            control.separate_alpha_blend
                ? static_cast<VkBlendFactor>(LiverpoolToVK::BlendFactor(control.alpha_dst_factor))
                : dst_color;
        const VkBlendOp alpha_blend =
            control.separate_alpha_blend
                ? static_cast<VkBlendOp>(LiverpoolToVK::BlendOp(control.alpha_func))
                : color_blend;

        VkPipelineColorBlendAttachmentState& attachment = attachments[i];
        attachment = VkPipelineColorBlendAttachmentState{
            .blendEnable = control.enable,
            .srcColorBlendFactor = src_color,
            .dstColorBlendFactor = dst_color,
            .colorBlendOp = color_blend,
            .srcAlphaBlendFactor = src_alpha,
            .dstAlphaBlendFactor = dst_alpha,
            .alphaBlendOp = alpha_blend,
            .colorWriteMask =
                instance.IsDynamicColorWriteMaskSupported()
                    ? VkColorComponentFlags{VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                            VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT}
                    : key.write_masks[i],
        };

        // The shader squares its color output for this attachment (see PsColorBuffer), so the
        // factors must not scale the operands again.
        if (key.color_buffers[i].blend_self_scale) {
            LOG_WARNING(
                Render_Vulkan,
                "Emulating scaled min/max blend with squared shader output on attachment %u", i);
            attachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
            attachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
        }

        // On GCN GPU there is an additional mask which allows to control color components exported
        // from a pixel shader. A situation possible, when the game may mask out the alpha channel,
        // while it is still need to be used in blending ops. For such cases, HW will default alpha
        // to 1 and perform the blending, while shader normally outputs 0 in the last component.
        // Unfortunatelly, Vulkan doesn't provide any control on blend inputs, so below we detecting
        // such cases and override alpha value in order to emulate HW behaviour.
        const bool has_alpha_masked_out =
            (key.cb_shader_mask.GetMask(i) & AmdGpu::ColorBufferMask::ComponentA) == 0;
        if (has_alpha_masked_out && ReadsSrcAlpha(src_color)) {
            attachment.srcColorBlendFactor = src_color == VK_BLEND_FACTOR_SRC_ALPHA
                                                 ? VK_BLEND_FACTOR_ONE
                                                 : VK_BLEND_FACTOR_ZERO; // 1-A
        }
        if (has_alpha_masked_out && ReadsSrcAlpha(dst_color)) {
            attachment.dstColorBlendFactor = dst_color == VK_BLEND_FACTOR_SRC_ALPHA
                                                 ? VK_BLEND_FACTOR_ONE
                                                 : VK_BLEND_FACTOR_ZERO; // 1-A
        }

        NormalizeMinMaxBlend(attachment.colorBlendOp, attachment.srcColorBlendFactor,
                             attachment.dstColorBlendFactor, key.color_buffers[i].num_format);
        NormalizeMinMaxBlend(attachment.alphaBlendOp, attachment.srcAlphaBlendFactor,
                             attachment.dstAlphaBlendFactor, key.color_buffers[i].num_format);

        if (control.enable &&
            (IsScaledMinMax(attachment.colorBlendOp, attachment.srcColorBlendFactor,
                            attachment.dstColorBlendFactor) ||
             IsScaledMinMax(attachment.alphaBlendOp, attachment.srcAlphaBlendFactor,
                            attachment.dstAlphaBlendFactor))) {
            LOG_WARNING(
                Render_Vulkan,
                "Unimplemented use of min/max blend op with blend factor not equal to one.");
        }
    }

    const VkPipelineColorBlendStateCreateInfo color_blending = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .logicOpEnable =
            instance.IsLogicOpSupported() && key.logic_op != AmdGpu::ColorControl::LogicOp::Copy,
        .logicOp = static_cast<VkLogicOp>(LiverpoolToVK::LogicOp(key.logic_op)),
        .attachmentCount = key.num_color_attachments,
        .pAttachments = attachments,
        .blendConstants = {1.0f, 1.0f, 1.0f, 1.0f},
    };

    // Required by spec unless VK_EXT_extended_dynamic_state3 is supported.
    // In practice, we use dynamic state for all of it.
    const VkPipelineDepthStencilStateCreateInfo depth_stencil_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
    };

    const VkGraphicsPipelineCreateInfo pipeline_info = {
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .pNext = &pipeline_rendering_ci,
        .stageCount = static_cast<u32>(shader_stages.size()),
        .pStages = shader_stages.data(),
        .pVertexInputState = !instance.IsVertexInputDynamicState() ? &vertex_input_info : nullptr,
        .pInputAssemblyState = &input_assembly,
        .pTessellationState = &tessellation_state,
        .pViewportState = &viewport_info,
        .pRasterizationState = &rasterization,
        .pMultisampleState = &multisampling,
        .pDepthStencilState =
            !instance.IsExtendedDynamicState3Supported() ? &depth_stencil_info : nullptr,
        .pColorBlendState = &color_blending,
        .pDynamicState = &dynamic_info,
        .layout = pipeline_layout,
    };

    const VkResult pipeline_result =
        vkCreateGraphicsPipelines(device, pipeline_cache, 1, &pipeline_info, nullptr, &pipeline);
    ASSERT_MSG(pipeline_result == VK_SUCCESS, "Failed to create graphics pipeline: %s",
               ResultName(pipeline_result));
    SetObjectName(vk_device, vk::Pipeline{pipeline}, "Graphics Pipeline %s", debug_str);
}

GraphicsPipeline::~GraphicsPipeline() = default;

template <typename Attribute, typename Binding>
void GraphicsPipeline::GetVertexInputs(
    VertexInputs<Attribute>& attributes, VertexInputs<Binding>& bindings,
    VertexInputs<VkVertexInputBindingDivisorDescriptionEXT>& divisors,
    VertexInputs<AmdGpu::Buffer>& guest_buffers, u32 step_rate_0, u32 step_rate_1) const {
    using InstanceIdType = Shader::Gcn::VertexAttribute::InstanceIdType;
    // The dynamic state's structs carry a structure type, the pipeline's do not
    constexpr bool is_dynamic = Common::IsSame<Binding, VkVertexInputBindingDescription2EXT>;
    if (fetch_shader.Empty() || fetch_shader.attributes.empty()) {
        return;
    }
    const Shader::Info& vs_info = GetStage(Shader::SwStage::Vertex);
    for (u32 semantic = 0; semantic < fetch_shader.attributes.size(); ++semantic) {
        const Shader::Gcn::VertexAttribute& attrib = fetch_shader.attributes[semantic];
        const InstanceIdType step_rate = attrib.GetStepRate();
        const AmdGpu::Buffer buffer = attrib.GetSharp(vs_info);
        Attribute attribute{};
        attribute.location = semantic;
        attribute.binding = semantic;
        attribute.format = static_cast<VkFormat>(
            LiverpoolToVK::SurfaceFormat(buffer.GetDataFmt(), buffer.GetNumberFmt()));
        attribute.offset = 0;
        Binding binding{};
        binding.binding = semantic;
        binding.stride = buffer.GetStride();
        binding.inputRate = step_rate == InstanceIdType::None ? VK_VERTEX_INPUT_RATE_VERTEX
                                                              : VK_VERTEX_INPUT_RATE_INSTANCE;
        const u32 divisor = step_rate == InstanceIdType::OverStepRate0
                                ? step_rate_0
                                : (step_rate == InstanceIdType::OverStepRate1 ? step_rate_1 : 1);
        if constexpr (is_dynamic) {
            attribute.sType = VK_STRUCTURE_TYPE_VERTEX_INPUT_ATTRIBUTE_DESCRIPTION_2_EXT;
            binding.sType = VK_STRUCTURE_TYPE_VERTEX_INPUT_BINDING_DESCRIPTION_2_EXT;
            binding.divisor = divisor;
        } else if (step_rate != InstanceIdType::None) {
            divisors.push_back(VkVertexInputBindingDivisorDescriptionEXT{
                .binding = semantic,
                .divisor = divisor,
            });
        }
        attributes.push_back(attribute);
        bindings.push_back(binding);
        guest_buffers.emplace_back(buffer);
    }
}

// Declare templated GetVertexInputs for necessary types.
template void GraphicsPipeline::GetVertexInputs(
    VertexInputs<VkVertexInputAttributeDescription>& attributes,
    VertexInputs<VkVertexInputBindingDescription>& bindings,
    VertexInputs<VkVertexInputBindingDivisorDescriptionEXT>& divisors,
    VertexInputs<AmdGpu::Buffer>& guest_buffers, u32 step_rate_0, u32 step_rate_1) const;
template void GraphicsPipeline::GetVertexInputs(
    VertexInputs<VkVertexInputAttributeDescription2EXT>& attributes,
    VertexInputs<VkVertexInputBindingDescription2EXT>& bindings,
    VertexInputs<VkVertexInputBindingDivisorDescriptionEXT>& divisors,
    VertexInputs<AmdGpu::Buffer>& guest_buffers, u32 step_rate_0, u32 step_rate_1) const;

void GraphicsPipeline::BuildDescSetLayout(bool preloading) {
    Common::SmallVector<VkDescriptorSetLayoutBinding, 32> bindings;
    u32 binding{};

    // While preloading the sharps are out of reach, and every buffer is a storage buffer either
    // way (see the comment in the compute pipeline's creation)
    (void)preloading;
    for (const Shader::Info* stage : stages) {
        if (!stage) {
            continue;
        }
        const VkShaderStageFlags stage_bit = LogicalStageToStageBit[u32(stage->sw_stage)];
        for (size_t i = 0; i < stage->buffers.size(); ++i) {
            bindings.push_back(VkDescriptorSetLayoutBinding{
                .binding = binding++,
                .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                .descriptorCount = 1,
                .stageFlags = stage_bit,
            });
        }
        for (const Shader::ImageResource& image : stage->images) {
            const u32 num_bindings = image.NumBindings(*stage);
            bindings.push_back(VkDescriptorSetLayoutBinding{
                .binding = binding,
                .descriptorType = image.is_written ? VK_DESCRIPTOR_TYPE_STORAGE_IMAGE
                                                   : VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
                .descriptorCount = num_bindings,
                .stageFlags = stage_bit,
            });
            binding += num_bindings;
        }
        for (size_t i = 0; i < stage->samplers.size(); ++i) {
            bindings.push_back(VkDescriptorSetLayoutBinding{
                .binding = binding++,
                .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER,
                .descriptorCount = 1,
                .stageFlags = stage_bit,
            });
        }
    }
    uses_push_descriptors = binding < instance.MaxPushDescriptors();
    CreateDescSetLayout(bindings.data(), static_cast<u32>(bindings.size()), "graphics");
}

} // namespace Vulkan
