// SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "common/enum_names.h"
#include "common/run_once.h"

#include "common/alignment.h"
#include "common/assert.h"
#include "common/func_traits.h"
#include "core/emulator_settings.h"
#include "shader_recompiler/backend/spirv/emit_spirv.h"
#include "shader_recompiler/backend/spirv/emit_spirv_instructions.h"
#include "shader_recompiler/backend/spirv/spirv_emit_context.h"
#include "shader_recompiler/frontend/translate/translate.h"
#include "shader_recompiler/ir/basic_block.h"
#include "shader_recompiler/ir/program.h"
#include "shader_recompiler/runtime_info.h"

namespace Shader::Backend::SPIRV {
namespace {

static constexpr u32 GetInputPrimitiveType(AmdGpu::PrimitiveType type) {
    switch (type) {
    case AmdGpu::PrimitiveType::PointList:
        return SpvExecutionModeInputPoints;
    case AmdGpu::PrimitiveType::LineList:
    case AmdGpu::PrimitiveType::LineStrip:
        return SpvExecutionModeInputLines;
    case AmdGpu::PrimitiveType::TriangleList:
    case AmdGpu::PrimitiveType::TriangleStrip:
    case AmdGpu::PrimitiveType::RectList:
        return SpvExecutionModeTriangles;
    case AmdGpu::PrimitiveType::AdjTriangleList:
        return SpvExecutionModeInputTrianglesAdjacency;
    case AmdGpu::PrimitiveType::AdjLineList:
        return SpvExecutionModeInputLinesAdjacency;
    default:
        UNREACHABLE_MSG("Unknown input primitive type %u", u32(type));
    }
}

static constexpr u32 GetOutputPrimitiveType(AmdGpu::GsOutputPrimitiveType type) {
    switch (type) {
    case AmdGpu::GsOutputPrimitiveType::PointList:
        return SpvExecutionModeOutputVertices;
    case AmdGpu::GsOutputPrimitiveType::LineStrip:
        return SpvExecutionModeOutputLineStrip;
    case AmdGpu::GsOutputPrimitiveType::TriangleStrip:
        return SpvExecutionModeOutputTriangleStrip;
    default:
        UNREACHABLE_MSG("Unknown output primitive type %u", u32(type));
    }
}

template <typename F, F func, typename... Args>
void SetDefinition(EmitContext& ctx, IR::Inst* inst, Args... args) {
    inst->SetDefinition<Id>(func(ctx, static_cast<Args&&>(args)...));
}

template <typename ArgType>
ArgType Arg(EmitContext& ctx, const IR::Value& arg) {
    if constexpr (Common::IsSame<ArgType, Id>) {
        return ctx.Def(arg);
    } else if constexpr (Common::IsSame<ArgType, const IR::Value&>) {
        return arg;
    } else if constexpr (Common::IsSame<ArgType, Imm32>) {
        return Imm32(arg.U32());
    } else if constexpr (Common::IsSame<ArgType, u32>) {
        return arg.U32();
    } else if constexpr (Common::IsSame<ArgType, u64>) {
        return arg.U64();
    } else if constexpr (Common::IsSame<ArgType, bool>) {
        return arg.U1();
    } else if constexpr (Common::IsSame<ArgType, IR::Attribute>) {
        return arg.Attribute();
    } else if constexpr (Common::IsSame<ArgType, IR::ScalarReg>) {
        return arg.ScalarReg();
    } else if constexpr (Common::IsSame<ArgType, IR::VectorReg>) {
        return arg.VectorReg();
    } else if constexpr (Common::IsSame<ArgType, const char*>) {
        return arg.StringLiteral();
    } else if constexpr (Common::IsSame<ArgType, IR::Patch>) {
        return arg.Patch();
    }
    UNREACHABLE();
}

template <typename F, F func, bool is_first_arg_inst, size_t... I>
void Invoke(EmitContext& ctx, IR::Inst* inst, Common::IndexSequence<I...>) {
    using Traits = Common::FuncTraits<F>;
    if constexpr (Common::IsSame<typename Traits::ReturnType, Id>) {
        if constexpr (is_first_arg_inst) {
            SetDefinition<F, func>(
                ctx, inst, inst,
                Arg<typename Traits::template ArgType<I + 2>>(ctx, inst->Arg(I))...);
        } else {
            SetDefinition<F, func>(
                ctx, inst, Arg<typename Traits::template ArgType<I + 1>>(ctx, inst->Arg(I))...);
        }
    } else {
        if constexpr (is_first_arg_inst) {
            func(ctx, inst, Arg<typename Traits::template ArgType<I + 2>>(ctx, inst->Arg(I))...);
        } else {
            func(ctx, Arg<typename Traits::template ArgType<I + 1>>(ctx, inst->Arg(I))...);
        }
    }
}

template <typename F, F func>
void Invoke(EmitContext& ctx, IR::Inst* inst) {
    using Traits = Common::FuncTraits<F>;
    static_assert(Traits::NUM_ARGS >= 1, "Insufficient arguments");
    if constexpr (Traits::NUM_ARGS == 1) {
        Invoke<F, func, false>(ctx, inst, Common::MakeIndexSequence<0>{});
    } else {
        using FirstArgType = typename Traits::template ArgType<1>;
        static constexpr bool is_first_arg_inst = Common::IsSame<FirstArgType, IR::Inst*>;
        using Indices = Common::MakeIndexSequence<Traits::NUM_ARGS - (is_first_arg_inst ? 2 : 1)>;
        Invoke<F, func, is_first_arg_inst>(ctx, inst, Indices{});
    }
}

void EmitInst(EmitContext& ctx, IR::Inst* inst) {
    switch (inst->GetOpcode()) {
#define OPCODE(name, result_type, ...)                                                             \
    case IR::Opcode::name:                                                                         \
        return Invoke<decltype(&Emit##name), &Emit##name>(ctx, inst);
#include "shader_recompiler/ir/opcodes.inc"
#undef OPCODE
    }
    UNREACHABLE_MSG("Invalid opcode %.*s",
                    static_cast<int>(Shader::IR::NameOf(inst->GetOpcode()).size()),
                    Shader::IR::NameOf(inst->GetOpcode()).data());
}

Id TypeId(const EmitContext& ctx, IR::Type type) {
    switch (type) {
    case IR::Type::U1:
        return ctx.U1[1];
    case IR::Type::U32:
        return ctx.U32[1];
    case IR::Type::U32x2:
        return ctx.U32[2];
    case IR::Type::U32x3:
        return ctx.U32[3];
    case IR::Type::U32x4:
        return ctx.U32[4];
    case IR::Type::F32:
        return ctx.F32[1];
    case IR::Type::U64:
        return ctx.U64;
    default:
        UNREACHABLE_MSG("Phi node type %s", Shader::IR::NameOf(type).c_str());
    }
}

void Traverse(EmitContext& ctx, const IR::Program& program) {
    IR::Block* current_block{};
    for (const IR::AbstractSyntaxNode& node : program.syntax_list) {
        switch (node.type) {
        case IR::AbstractSyntaxNode::Type::Block: {
            const Id label{node.data.block->Definition<Id>()};
            if (current_block) {
                spv_OpBranch(&ctx.m, label);
            }
            current_block = node.data.block;
            ctx.AddLabel(label);
            for (IR::Inst& inst : node.data.block->Instructions()) {
                EmitInst(ctx, &inst);
            }
            *static_cast<Id*>(u64_map_insert(&ctx.first_to_last_label_map, label, nullptr)) =
                ctx.last_label;
            break;
        }
        case IR::AbstractSyntaxNode::Type::If: {
            const Id if_label{node.data.if_node.body->Definition<Id>()};
            const Id endif_label{node.data.if_node.merge->Definition<Id>()};
            spv_OpSelectionMerge(&ctx.m, endif_label, u32(SpvSelectionControlMaskNone));
            spv_OpBranchConditional(&ctx.m, ctx.Def(node.data.if_node.cond), if_label, endif_label);
            break;
        }
        case IR::AbstractSyntaxNode::Type::Loop: {
            const Id body_label{node.data.loop.body->Definition<Id>()};
            const Id continue_label{node.data.loop.continue_block->Definition<Id>()};
            const Id endloop_label{node.data.loop.merge->Definition<Id>()};

            spv_OpLoopMerge(&ctx.m, endloop_label, continue_label, u32(SpvLoopControlMaskNone));
            spv_OpBranch(&ctx.m, body_label);
            break;
        }
        case IR::AbstractSyntaxNode::Type::Break: {
            const Id break_label{node.data.break_node.merge->Definition<Id>()};
            const Id skip_label{node.data.break_node.skip->Definition<Id>()};
            spv_OpBranchConditional(&ctx.m, ctx.Def(node.data.break_node.cond), break_label,
                                    skip_label);
            break;
        }
        case IR::AbstractSyntaxNode::Type::EndIf:
            if (current_block) {
                spv_OpBranch(&ctx.m, node.data.end_if.merge->Definition<Id>());
            }
            break;
        case IR::AbstractSyntaxNode::Type::Repeat: {
            Id cond{ctx.Def(node.data.repeat.cond)};
            const Id loop_header_label{node.data.repeat.loop_header->Definition<Id>()};
            const Id merge_label{node.data.repeat.merge->Definition<Id>()};
            spv_OpBranchConditional(&ctx.m, cond, loop_header_label, merge_label);
            break;
        }
        case IR::AbstractSyntaxNode::Type::Return:
            spv_OpReturn(&ctx.m);
            break;
        case IR::AbstractSyntaxNode::Type::Unreachable:
            spv_OpUnreachable(&ctx.m);
            break;
        }
        if (node.type != IR::AbstractSyntaxNode::Type::Block) {
            current_block = nullptr;
        }
    }
}

Id DefineMain(EmitContext& ctx, const IR::Program& program) {
    const Id void_function{spv_TypeFunction0(&ctx.m, ctx.void_id)};
    const Id main{
        spv_OpFunction(&ctx.m, ctx.void_id, u32(SpvFunctionControlMaskNone), void_function)};
    for (IR::Block* const block : program.blocks) {
        block->SetDefinition(spv_OpLabel(&ctx.m));
    }
    Traverse(ctx, program);
    spv_OpFunctionEnd(&ctx.m);
    return main;
}

u32 ExecutionMode(AmdGpu::TessellationType primitive) {
    switch (primitive) {
    case AmdGpu::TessellationType::Isoline:
        return SpvExecutionModeIsolines;
    case AmdGpu::TessellationType::Triangle:
        return SpvExecutionModeTriangles;
    case AmdGpu::TessellationType::Quad:
        return SpvExecutionModeQuads;
    }
    UNREACHABLE_MSG("Tessellation primitive %s",
                    enum_name(&enum_names_AmdGpu_TessellationType, static_cast<s64>(primitive)));
}

u32 ExecutionMode(AmdGpu::TessellationPartitioning spacing) {
    switch (spacing) {
    case AmdGpu::TessellationPartitioning::Integer:
        return SpvExecutionModeSpacingEqual;
    case AmdGpu::TessellationPartitioning::FracOdd:
        return SpvExecutionModeSpacingFractionalOdd;
    case AmdGpu::TessellationPartitioning::FracEven:
        return SpvExecutionModeSpacingFractionalEven;
    case AmdGpu::TessellationPartitioning::Pow2:
        // Pow2 rounds tessellation factors to the nearest power of 2, which has no
        // direct Vulkan equivalent. SpacingEqual (integer) is the closest match.
        LOG_WARNING(Render_Vulkan, "Tessellation partitioning Pow2 has no Vulkan equivalent, "
                                   "falling back to SpacingEqual");
        return SpvExecutionModeSpacingEqual;
    default:
        break;
    }
    UNREACHABLE_MSG(
        "Tessellation spacing %s",
        enum_name(&enum_names_AmdGpu_TessellationPartitioning, static_cast<s64>(spacing)));
}

void SetupCapabilities(const Info& info, const Profile& profile, const RuntimeInfo& runtime_info,
                       EmitContext& ctx) {
    spv_AddCapability(&ctx.m, u32(SpvCapabilityImage1D));
    spv_AddCapability(&ctx.m, u32(SpvCapabilitySampled1D));
    spv_AddCapability(&ctx.m, u32(SpvCapabilityImageQuery));
    spv_AddCapability(&ctx.m, u32(SpvCapabilityInt8));
    spv_AddCapability(&ctx.m, u32(SpvCapabilityInt16));
    spv_AddCapability(&ctx.m, u32(SpvCapabilityInt64));
    spv_AddCapability(&ctx.m, u32(SpvCapabilityStorageBuffer8BitAccess));
    spv_AddCapability(&ctx.m, u32(SpvCapabilityStorageBuffer16BitAccess));
    if (info.uses_fp16) {
        spv_AddCapability(&ctx.m, u32(SpvCapabilityFloat16));
    }
    if (info.uses_fp64) {
        spv_AddCapability(&ctx.m, u32(SpvCapabilityFloat64));
    }
    if (info.has_storage_images) {
        spv_AddCapability(&ctx.m, u32(SpvCapabilityStorageImageExtendedFormats));
        spv_AddCapability(&ctx.m, u32(SpvCapabilityStorageImageReadWithoutFormat));
        spv_AddCapability(&ctx.m, u32(SpvCapabilityStorageImageWriteWithoutFormat));
        if (profile.supports_image_load_store_lod) {
            spv_AddExtension(&ctx.m, "SPV_AMD_shader_image_load_store_lod");
            spv_AddCapability(&ctx.m, u32(SpvCapabilityImageReadWriteLodAMD));
        }
    }
    if (info.has_image_gather) {
        spv_AddCapability(&ctx.m, u32(SpvCapabilityImageGatherExtended));
    }
    if (info.has_image_query) {
        spv_AddCapability(&ctx.m, u32(SpvCapabilityImageQuery));
    }
    if ((info.uses_image_atomic_float_min_max && profile.supports_image_fp32_atomic_min_max) ||
        (info.uses_buffer_atomic_float_min_max && profile.supports_buffer_fp32_atomic_min_max)) {
        spv_AddExtension(&ctx.m, "SPV_EXT_shader_atomic_float_min_max");
        spv_AddCapability(&ctx.m, u32(SpvCapabilityAtomicFloat32MinMaxEXT));
    }
    if (info.uses_lane_id) {
        spv_AddCapability(&ctx.m, u32(SpvCapabilityGroupNonUniform));
    }
    if (info.uses_group_quad) {
        spv_AddCapability(&ctx.m, u32(SpvCapabilityGroupNonUniformQuad));
    }
    if (info.uses_group_shuffle) {
        spv_AddCapability(&ctx.m, u32(SpvCapabilityGroupNonUniformShuffle));
    }
    if (info.uses_group_ballot || info.loads.Get(IR::Attribute::SubgroupLtMask)) {
        spv_AddCapability(&ctx.m, u32(SpvCapabilityGroupNonUniformBallot));
    }
    if (info.uses_shader_clock) {
        if (ctx.profile.supports_shader_subgroup_clock) {
            spv_AddExtension(&ctx.m, "SPV_KHR_shader_clock");
            spv_AddCapability(&ctx.m, u32(SpvCapabilityShaderClockKHR));
        } else {
            LOG_WARNING(Render_Recompiler,
                        "Shader requires support for ShaderClockKHR capability"
                        " that your Vulkan instance does not advertise. Results may vary");
        }
    }
    const Shader::SwStage stage = info.sw_stage;
    if (stage == SwStage::Vertex) {
        spv_AddExtension(&ctx.m, "SPV_KHR_shader_draw_parameters");
        spv_AddCapability(&ctx.m, u32(SpvCapabilityDrawParameters));
    }
    if (stage == SwStage::Geometry) {
        spv_AddCapability(&ctx.m, u32(SpvCapabilityGeometry));
    }
    if (info.hw_stage == HwStage::Fragment) {
        if (profile.supports_amd_shader_explicit_vertex_parameter) {
            spv_AddExtension(&ctx.m, "SPV_AMD_shader_explicit_vertex_parameter");
        } else if (profile.supports_fragment_shader_barycentric) {
            spv_AddExtension(&ctx.m, "SPV_KHR_fragment_shader_barycentric");
            spv_AddCapability(&ctx.m, u32(SpvCapabilityFragmentBarycentricKHR));
            spv_AddCapability(&ctx.m, u32(SpvCapabilityInterpolationFunction));
        }
        if (info.loads.Get(IR::Attribute::SampleIndex) ||
            runtime_info.hw.fs.addr_flags.linear_sample_ena ||
            runtime_info.hw.fs.addr_flags.persp_sample_ena ||
            (!profile.supports_amd_shader_explicit_vertex_parameter &&
             profile.supports_fragment_shader_barycentric &&
             info.loads.Get(IR::Attribute::BaryCoordSmoothSample))) {
            spv_AddCapability(&ctx.m, u32(SpvCapabilitySampleRateShading));
        }
        if (info.loads.GetAny(IR::Attribute::RenderTargetIndex)) {
            spv_AddCapability(&ctx.m, u32(SpvCapabilityGeometry));
        }
        if (info.stores.Get(IR::Attribute::StencilRef) && profile.supports_shader_stencil_export) {
            spv_AddExtension(&ctx.m, "SPV_EXT_shader_stencil_export");
            spv_AddCapability(&ctx.m, u32(SpvCapabilityStencilExportEXT));
        }
    }
    if (stage == SwStage::TessellationControl || stage == SwStage::TessellationEval) {
        spv_AddCapability(&ctx.m, u32(SpvCapabilityTessellation));
    }
    if (stage == SwStage::Vertex || stage == SwStage::TessellationControl ||
        stage == SwStage::TessellationEval) {
        if (info.stores.GetAny(IR::Attribute::RenderTargetIndex)) {
            spv_AddCapability(&ctx.m, u32(SpvCapabilityShaderLayer));
        }
        if (info.stores.GetAny(IR::Attribute::ViewportIndex)) {
            spv_AddCapability(&ctx.m, u32(SpvCapabilityShaderViewportIndex));
        }
    } else if (stage == SwStage::Geometry && info.stores.GetAny(IR::Attribute::ViewportIndex)) {
        spv_AddCapability(&ctx.m, u32(SpvCapabilityMultiViewport));
    }
    if (info.uses_dma) {
        spv_AddCapability(&ctx.m, u32(SpvCapabilityPhysicalStorageBufferAddresses));
        spv_AddExtension(&ctx.m, "SPV_KHR_physical_storage_buffer");
    }
    const int shared_type_count =
        static_cast<int>(Common::PopCount(static_cast<u32>(info.shared_types)));
    if (shared_type_count > 1 && profile.supports_workgroup_explicit_memory_layout) {
        spv_AddExtension(&ctx.m, "SPV_KHR_workgroup_memory_explicit_layout");
        spv_AddCapability(&ctx.m, u32(SpvCapabilityWorkgroupMemoryExplicitLayoutKHR));
        spv_AddCapability(&ctx.m, u32(SpvCapabilityWorkgroupMemoryExplicitLayout16BitAccessKHR));
    }
    if (info.uses_buffer_int64_atomics || info.uses_shared_int64_atomics) {
        if (info.uses_buffer_int64_atomics) {
            ASSERT_MSG(ctx.profile.supports_buffer_int64_atomics,
                       "Shader requires support for atomic Int64 buffer operations that your "
                       "Vulkan instance does not advertise");
        }
        if (info.uses_shared_int64_atomics) {
            ASSERT_MSG(ctx.profile.supports_shared_int64_atomics,
                       "Shader requires support for atomic Int64 shared memory operations that "
                       "your Vulkan instance does not advertise");
        }
        spv_AddCapability(&ctx.m, u32(SpvCapabilityInt64Atomics));
    }
}

void DefineEntryPoint(const Info& info, const RuntimeInfo& runtime_info, EmitContext& ctx,
                      Id main) {
    u32 execution_model{};
    switch (info.sw_stage) {
    case SwStage::Compute: {
        const u32* const workgroup_size = &ctx.runtime_info.hw.cs.workgroup_size[0];
        execution_model = SpvExecutionModelGLCompute;
        spv_AddExecutionMode3(&ctx.m, main, u32(SpvExecutionModeLocalSize), u32(workgroup_size[0]),
                              u32(workgroup_size[1]), u32(workgroup_size[2]));
        break;
    }
    case SwStage::Vertex:
        execution_model = SpvExecutionModelVertex;
        break;
    case SwStage::TessellationControl:
        execution_model = SpvExecutionModelTessellationControl;
        spv_AddCapability(&ctx.m, u32(SpvCapabilityTessellation));
        spv_AddExecutionMode1(&ctx.m, main, u32(SpvExecutionModeOutputVertices),
                              u32(ctx.runtime_info.sw.tcs.NumOutputControlPoints()));
        break;
    case SwStage::TessellationEval: {
        execution_model = SpvExecutionModelTessellationEvaluation;
        spv_AddExecutionMode0(&ctx.m, main, u32(ExecutionMode(ctx.runtime_info.sw.tes.tess_type)));
        spv_AddExecutionMode0(&ctx.m, main,
                              u32(ExecutionMode(ctx.runtime_info.sw.tes.tess_partitioning)));
        spv_AddExecutionMode0(
            &ctx.m, main,
            u32(ctx.runtime_info.sw.tes.tess_topology == AmdGpu::TessellationTopology::TriangleCcw
                    ? SpvExecutionModeVertexOrderCcw
                    : SpvExecutionModeVertexOrderCw));
        break;
    }
    case SwStage::Fragment: {
        execution_model = SpvExecutionModelFragment;
        if (ctx.profile.lower_left_origin_mode) {
            spv_AddExecutionMode0(&ctx.m, main, u32(SpvExecutionModeOriginLowerLeft));
        } else {
            spv_AddExecutionMode0(&ctx.m, main, u32(SpvExecutionModeOriginUpperLeft));
        }
        if (info.has_discard) {
            spv_AddCapability(&ctx.m, u32(SpvCapabilityDemoteToHelperInvocation));
        }
        const bool stores_depth = info.stores.Get(IR::Attribute::Depth);
        if (stores_depth) {
            spv_AddExecutionMode0(&ctx.m, main, u32(SpvExecutionModeDepthReplacing));
        }
        if (runtime_info.hw.fs.depth_before_shader) {
            spv_AddExecutionMode0(&ctx.m, main, u32(SpvExecutionModeEarlyFragmentTests));
            ASSERT_MSG(!stores_depth, "DEPTH_BEFORE_SHADER enabled with depth exporting shader");
        }
        break;
    }
    case SwStage::Geometry:
        execution_model = SpvExecutionModelGeometry;
        spv_AddExecutionMode0(&ctx.m, main,
                              u32(GetInputPrimitiveType(ctx.runtime_info.hw.gs.in_primitive)));
        spv_AddExecutionMode0(&ctx.m, main,
                              u32(GetOutputPrimitiveType(ctx.runtime_info.hw.gs.out_primitive[0])));
        spv_AddExecutionMode1(&ctx.m, main, u32(SpvExecutionModeOutputVertices),
                              u32(ctx.runtime_info.hw.gs.output_vertices));
        spv_AddExecutionMode1(&ctx.m, main, u32(SpvExecutionModeInvocations),
                              u32(ctx.runtime_info.hw.gs.num_invocations));
        break;
    default:
        UNREACHABLE_MSG("Stage %u", u32(info.hw_stage));
    }
    spv_AddEntryPoint(&ctx.m, u32(execution_model), main, "main", ctx.interfaces.data(),
                      u32(ctx.interfaces.size()));
}

void SetupDenormFlushMode(EmitContext& ctx, const Profile& profile, const RuntimeInfo& runtime_info,
                          Id main_func) {
    const AmdGpu::FpDenormMode fp32_denorm_mode = runtime_info.props.fp_denorm_mode32;
    if (fp32_denorm_mode == AmdGpu::FpDenormMode::InOutFlush) {
        if (profile.support_fp32_denorm_flush) {
            spv_AddCapability(&ctx.m, u32(SpvCapabilityDenormFlushToZero));
            spv_AddExecutionMode1(&ctx.m, main_func, u32(SpvExecutionModeDenormFlushToZero), 32U);
        } else {
            static run_once_t logged = RUN_ONCE_INIT;
            run_once(
                &logged,
                [](void*) {
                    LOG_WARNING(Render_Vulkan,
                                "Float32 denorm flushing is not supported by the GPU");
                },
                nullptr);
        }
    } else {
        LOG_WARNING(Render_Vulkan, "Unknown FP denorm mode %u", u32(fp32_denorm_mode));
    }

    if (!ctx.info.uses_fp16 && !ctx.info.uses_fp64) {
        return;
    }

    const AmdGpu::FpDenormMode fp16_64_denorm_mode = runtime_info.props.fp_denorm_mode16_64;
    const bool same_denorm_mode = fp16_64_denorm_mode == fp32_denorm_mode;
    if (!same_denorm_mode && !profile.supports_denorm_behavior_independence) {
        static run_once_t logged = RUN_ONCE_INIT;
        run_once(
            &logged,
            [](void*) {
                LOG_WARNING(Render_Vulkan,
                            "Float32 and Float16/64 denorm modes are different but the "
                            "GPU does not support that");
            },
            nullptr);
        return;
    }

    if (fp16_64_denorm_mode == AmdGpu::FpDenormMode::InOutFlush) {
        if (profile.support_fp16_denorm_flush) {
            spv_AddCapability(&ctx.m, u32(SpvCapabilityDenormFlushToZero));
            spv_AddExecutionMode1(&ctx.m, main_func, u32(SpvExecutionModeDenormFlushToZero), 16U);
        } else {
            static run_once_t logged = RUN_ONCE_INIT;
            run_once(
                &logged,
                [](void*) {
                    LOG_WARNING(Render_Vulkan,
                                "Float16 denorm flushing is not supported by the GPU");
                },
                nullptr);
        }
        if (profile.support_fp64_denorm_flush) {
            spv_AddCapability(&ctx.m, u32(SpvCapabilityDenormFlushToZero));
            spv_AddExecutionMode1(&ctx.m, main_func, u32(SpvExecutionModeDenormFlushToZero), 64U);
        } else {
            static run_once_t logged = RUN_ONCE_INIT;
            run_once(
                &logged,
                [](void*) {
                    LOG_WARNING(Render_Vulkan,
                                "Float64 denorm flushing is not supported by the GPU");
                },
                nullptr);
        }
    } else if (fp16_64_denorm_mode == AmdGpu::FpDenormMode::InOutAllow) {
        if (profile.support_fp16_denorm_preserve) {
            spv_AddCapability(&ctx.m, u32(SpvCapabilityDenormPreserve));
            spv_AddExecutionMode1(&ctx.m, main_func, u32(SpvExecutionModeDenormPreserve), 16U);
        } else {
            static run_once_t logged = RUN_ONCE_INIT;
            run_once(
                &logged,
                [](void*) {
                    LOG_WARNING(Render_Vulkan,
                                "Float16 denorm preserving is not supported by the GPU");
                },
                nullptr);
        }
        if (profile.support_fp64_denorm_preserve) {
            spv_AddCapability(&ctx.m, u32(SpvCapabilityDenormPreserve));
            spv_AddExecutionMode1(&ctx.m, main_func, u32(SpvExecutionModeDenormPreserve), 64U);
        } else {
            static run_once_t logged = RUN_ONCE_INIT;
            run_once(
                &logged,
                [](void*) {
                    LOG_WARNING(Render_Vulkan,
                                "Float64 denorm preserving is not supported by the GPU");
                },
                nullptr);
        }
    } else {
        LOG_WARNING(Render_Vulkan, "Unknown Float16/64 denorm mode %u", u32(fp16_64_denorm_mode));
    }
}

void SetupRoundingMode(EmitContext& ctx, const Profile& profile, const RuntimeInfo& runtime_info,
                       Id main_func) {
    const AmdGpu::FpRoundMode fp_round_mode = runtime_info.props.fp_round_mode32;
    if (fp_round_mode == AmdGpu::FpRoundMode::ToZero) {
        if (profile.support_fp32_round_to_zero) {
            spv_AddCapability(&ctx.m, u32(SpvCapabilityRoundingModeRTZ));
            spv_AddExecutionMode1(&ctx.m, main_func, u32(SpvExecutionModeRoundingModeRTZ), 32U);
        } else {
            static run_once_t logged = RUN_ONCE_INIT;
            run_once(
                &logged,
                [](void*) {
                    LOG_WARNING(Render_Vulkan,
                                "Float32 rounding to zero is not supported by the GPU");
                },
                nullptr);
        }
    } else if (fp_round_mode != AmdGpu::FpRoundMode::NearestEven) {
        LOG_WARNING(Render_Vulkan, "Unimplemented FP rounding mode %s",
                    enum_name(&enum_names_AmdGpu_FpRoundMode, static_cast<s64>(fp_round_mode)));
    }

    if (ctx.info.uses_fp16 || ctx.info.uses_fp64) {
        const AmdGpu::FpRoundMode fp16_64_round_mode = runtime_info.props.fp_round_mode16_64;
        const bool same_round_mode = fp16_64_round_mode == fp_round_mode;
        if (!same_round_mode && !profile.supports_rounding_mode_independence) {
            static run_once_t logged = RUN_ONCE_INIT;
            run_once(
                &logged,
                [](void*) {
                    LOG_WARNING(Render_Vulkan,
                                "Float32 and Float16/64 rounding modes are different "
                                "but the GPU does not support that");
                },
                nullptr);
            return;
        }

        if (fp16_64_round_mode == AmdGpu::FpRoundMode::ToZero) {
            if (ctx.info.uses_fp16) {
                if (profile.support_fp16_round_to_zero) {
                    spv_AddCapability(&ctx.m, u32(SpvCapabilityRoundingModeRTZ));
                    spv_AddExecutionMode1(&ctx.m, main_func, u32(SpvExecutionModeRoundingModeRTZ),
                                          16U);
                } else {
                    static run_once_t logged = RUN_ONCE_INIT;
                    run_once(
                        &logged,
                        [](void*) {
                            LOG_WARNING(Render_Vulkan,
                                        "Float16 rounding to zero is not supported by the GPU");
                        },
                        nullptr);
                }
            }
            if (ctx.info.uses_fp64) {
                if (profile.support_fp64_round_to_zero) {
                    spv_AddCapability(&ctx.m, u32(SpvCapabilityRoundingModeRTZ));
                    spv_AddExecutionMode1(&ctx.m, main_func, u32(SpvExecutionModeRoundingModeRTZ),
                                          64U);
                } else {
                    static run_once_t logged = RUN_ONCE_INIT;
                    run_once(
                        &logged,
                        [](void*) {
                            LOG_WARNING(Render_Vulkan,
                                        "Float64 rounding to zero is not supported by the GPU");
                        },
                        nullptr);
                }
            }
        } else if (fp16_64_round_mode != AmdGpu::FpRoundMode::NearestEven) {
            LOG_WARNING(Render_Vulkan, "Unknown Float16/64 rounding mode %u",
                        u32(fp16_64_round_mode));
        }
    }
}

void SetupInfNanPreserveMode(EmitContext& ctx, const Profile& profile,
                             const RuntimeInfo& runtime_info, Id main_func) {
    if (profile.fast_shader_math) {
        return;
    }
    if (profile.support_fp16_signed_zero_inf_nan_preserve ||
        profile.support_fp32_signed_zero_inf_nan_preserve ||
        profile.support_fp64_signed_zero_inf_nan_preserve) {
        spv_AddCapability(&ctx.m, u32(SpvCapabilitySignedZeroInfNanPreserve));
    }

    if (profile.support_fp32_signed_zero_inf_nan_preserve) {
        spv_AddExecutionMode1(&ctx.m, main_func, u32(SpvExecutionModeSignedZeroInfNanPreserve),
                              32U);
    } else {
        static run_once_t logged = RUN_ONCE_INIT;
        run_once(
            &logged,
            [](void*) {
                LOG_WARNING(
                    Render_Vulkan,
                    "Float32 signed zero/inf/nan preserve mode is not supported by the GPU");
            },
            nullptr);
    }
    if (ctx.info.uses_fp16) {
        if (profile.support_fp16_signed_zero_inf_nan_preserve) {
            spv_AddExecutionMode1(&ctx.m, main_func, u32(SpvExecutionModeSignedZeroInfNanPreserve),
                                  16U);
        } else {
            static run_once_t logged = RUN_ONCE_INIT;
            run_once(
                &logged,
                [](void*) {
                    LOG_WARNING(
                        Render_Vulkan,
                        "Float16 signed zero/inf/nan preserve mode is not supported by the GPU");
                },
                nullptr);
        }
    }
    if (ctx.info.uses_fp64) {
        if (profile.support_fp64_signed_zero_inf_nan_preserve) {
            spv_AddExecutionMode1(&ctx.m, main_func, u32(SpvExecutionModeSignedZeroInfNanPreserve),
                                  64U);
        } else {
            static run_once_t logged = RUN_ONCE_INIT;
            run_once(
                &logged,
                [](void*) {
                    LOG_WARNING(
                        Render_Vulkan,
                        "Float64 signed zero/inf/nan preserve mode is not supported by the GPU");
                },
                nullptr);
        }
    }
}

void SetupFloatMode(EmitContext& ctx, const Profile& profile, const RuntimeInfo& runtime_info,
                    Id main_func) {
    spv_AddExtension(&ctx.m, "SPV_KHR_float_controls");
    SetupDenormFlushMode(ctx, profile, runtime_info, main_func);
    SetupRoundingMode(ctx, profile, runtime_info, main_func);
    SetupInfNanPreserveMode(ctx, profile, runtime_info, main_func);
}

// What the deferred OpPhis' values are taken from, in the order they were written
struct PhiPatch {
    const IR::Program& program;
    EmitContext& ctx;
    IR::Block::const_iterator inst;
    size_t block_index;
};

void PatchPhiArg(void* arg_ctx, u32 phi_arg, u32 first_parent, u32* value_out, u32* parent_out) {
    PhiPatch& p = *static_cast<PhiPatch*>(arg_ctx);
    if (phi_arg == 0) {
        ++p.inst;
        while (p.inst == p.program.blocks[p.block_index]->end() ||
               p.inst->GetOpcode() != IR::Opcode::Phi) {
            ++p.block_index;
            p.inst = p.program.blocks[p.block_index]->begin();
        }
    }
    ASSERT(p.inst != p.program.blocks[p.block_index]->end());
    *value_out = p.ctx.Def(p.inst->Arg(phi_arg));
    const Id* const last =
        static_cast<const Id*>(u64_map_find(&p.ctx.first_to_last_label_map, first_parent));
    *parent_out = last ? *last : 0;
}

void PatchPhiNodes(const IR::Program& program, EmitContext& ctx) {
    PhiPatch patch{program, ctx, program.blocks.front()->begin(), 0};
    spv_module_patch_phis(&ctx.m, PatchPhiArg, &patch);
}
} // Anonymous namespace

Common::Span<const u32> EmitSPIRV(spv_module_t& module, const Profile& profile,
                                  const RuntimeInfo& runtime_info, const IR::Program& program,
                                  Bindings& binding) {
    // Debug names only for a shader that is dumped, where they are read
    module.names = emu_settings_get_dump_shaders() ? 1 : 0;
    EmitContext ctx{module, profile, runtime_info, program.info, binding};
    const Id main{DefineMain(ctx, program)};
    DefineEntryPoint(program.info, runtime_info, ctx, main);
    SetupCapabilities(program.info, profile, runtime_info, ctx);
    SetupFloatMode(ctx, profile, runtime_info, main);
    PatchPhiNodes(program, ctx);
    return ModuleWords(&ctx.m, profile.supported_spirv);
}

Id EmitPhi(EmitContext& ctx, IR::Inst* inst) {
    const size_t num_args{inst->NumArgs()};
    Common::SmallVector<Id, 32> blocks;
    blocks.reserve(num_args);
    for (size_t index = 0; index < num_args; ++index) {
        blocks.push_back(inst->PhiBlock(index)->Definition<Id>());
    }
    // The type of a phi instruction is stored in its flags
    const Id result_type{TypeId(ctx, inst->Flags<IR::Type>())};
    return spv_DeferredOpPhi(&ctx.m, result_type, blocks.data(), u32(blocks.size()));
}

void EmitVoid(EmitContext&) {}

Id EmitConditionRef(EmitContext& ctx, const IR::Value& value) {
    const Id id{ctx.Def(value)};
    ASSERT_MSG((id != 0), "Forward identity declaration");
    return id;
}

Id EmitGetPcLo(EmitContext& ctx, const IR::Value& value) {
    const Id id{ctx.Def(value)};
    ASSERT_MSG((id != 0), "Forward identity declaration");
    return id;
}

void EmitGetScc(EmitContext& ctx) {
    UNREACHABLE_MSG("Unreachable instruction");
}

void EmitGetExec(EmitContext& ctx) {
    UNREACHABLE_MSG("Unreachable instruction");
}

void EmitGetVcc(EmitContext& ctx) {
    UNREACHABLE_MSG("Unreachable instruction");
}

void EmitGetVccLo(EmitContext& ctx) {
    UNREACHABLE_MSG("Unreachable instruction");
}

void EmitGetVccHi(EmitContext& ctx) {
    UNREACHABLE_MSG("Unreachable instruction");
}

void EmitGetM0(EmitContext& ctx) {
    UNREACHABLE_MSG("Unreachable instruction");
}

void EmitSetScc(EmitContext& ctx) {
    UNREACHABLE_MSG("Unreachable instruction");
}

void EmitSetExec(EmitContext& ctx) {
    UNREACHABLE_MSG("Unreachable instruction");
}

void EmitSetVcc(EmitContext& ctx) {
    UNREACHABLE_MSG("Unreachable instruction");
}

void EmitSetVccLo(EmitContext& ctx) {
    UNREACHABLE_MSG("Unreachable instruction");
}

void EmitSetVccHi(EmitContext& ctx) {
    UNREACHABLE_MSG("Unreachable instruction");
}

void EmitSetM0(EmitContext& ctx) {
    UNREACHABLE_MSG("Unreachable instruction");
}

} // namespace Shader::Backend::SPIRV
