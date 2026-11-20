// SPDX-FileCopyrightText: Copyright 2024-2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "common/assert.h"
#include "core/emulator_settings.h"
#include "shader_recompiler/backend/spirv/emit_spirv_instructions.h"
#include "shader_recompiler/backend/spirv/spirv_emit_context.h"
#include "shader_recompiler/ir/attribute.h"
#include "shader_recompiler/ir/microinstruction.h"
#include "shader_recompiler/ir/patch.h"
#include "shader_recompiler/runtime_info.h"

namespace Shader::Backend::SPIRV {

using PointerType = EmitContext::PointerType;
using PointerSize = EmitContext::PointerSize;

static std::pair<Id, bool> OutputAttrComponentType(EmitContext& ctx, IR::Attribute attr) {
    if (IR::IsParam(attr)) {
        const u32 index{u32(attr) - u32(IR::Attribute::Param0)};
        const auto& info{ctx.output_params.at(index)};
        return {info.component_type, info.is_integer};
    }
    if (IR::IsMrt(attr)) {
        const u32 index{u32(attr) - u32(IR::Attribute::RenderTarget0)};
        const auto& info{ctx.frag_outputs.at(index)};
        return {info.component_type, info.is_integer};
    }
    switch (attr) {
    case IR::Attribute::Position0:
    case IR::Attribute::ClipDistance:
    case IR::Attribute::CullDistance:
    case IR::Attribute::Depth:
    case IR::Attribute::PointSize:
        return {ctx.F32[1], false};
    case IR::Attribute::RenderTargetIndex:
    case IR::Attribute::ViewportIndex:
    case IR::Attribute::SampleMask:
        return {ctx.U32[1], true};
    case IR::Attribute::StencilRef:
        return {ctx.S32[1], true};
    default:
        UNREACHABLE_MSG("Write attribute %s", Shader::IR::NameOf(attr).c_str());
    }
}

Id EmitGetUserData(EmitContext& ctx, IR::ScalarReg reg) {
    const Id ud_reg{ctx.EmitFlatbufferLoad(ctx.ConstU32(static_cast<u32>(reg)))};
    NameFmt(&ctx.m, ud_reg, "ud_%u", u32(reg));
    return ud_reg;
}

Id EmitReadConst(EmitContext& ctx, IR::Inst* inst, Id addr, Id offset) {
    const u32 flatbuf_off_dw = inst->Flags<u32>();
    if (!emu_settings_get_direct_memory_access_enabled()) {
        return ctx.EmitFlatbufferLoad(ctx.ConstU32(flatbuf_off_dw));
    }
    if (flatbuf_off_dw == 0) {
        return spv_OpFunctionCall(&ctx.m, ctx.U32[1], ctx.read_const_dynamic,
                                  std::array<u32, 2>{addr, offset}.data(), 2);
    } else {
        return ctx.EmitFlatbufferLoad(ctx.ConstU32(flatbuf_off_dw));
    }
}

Id EmitReadConstBuffer(EmitContext& ctx, Imm32 handle_imm, Id index) {
    u32 handle = handle_imm;
    const auto& buffer = ctx.buffers[handle];
    if (const Id offset = buffer.Offset(PointerSize::B32); (offset != 0)) {
        index = spv_OpIAdd(&ctx.m, ctx.U32[1], index, offset);
    }
    const auto [id, pointer_type] = buffer.Alias(PointerType::U32);
    const Id ptr{spv_OpAccessChain2(&ctx.m, pointer_type, id, ctx.u32_zero_value, index)};
    const Id result{spv_OpLoad(&ctx.m, ctx.U32[1], ptr)};
    return result;
}

Id EmitGetAttribute(EmitContext& ctx, IR::Attribute attr, Imm32 comp_imm, Imm32 index_imm) {
    u32 comp = comp_imm;
    u32 index = index_imm;
    if (IR::IsParam(attr)) {
        const u32 param_index{u32(attr) - u32(IR::Attribute::Param0)};
        const auto& param{ctx.input_params.at(param_index)};
        const Id value = [&] {
            if (param.is_array) {
                ASSERT(param.num_components > 1);
                if (param.is_loaded) {
                    return spv_OpCompositeExtract1(&ctx.m, param.component_type,
                                                   param.id_array[index], u32(comp));
                } else {
                    return spv_OpLoad(&ctx.m, param.component_type,
                                      spv_OpAccessChain2(&ctx.m, param.pointer_type, param.id,
                                                         ctx.ConstU32(index), ctx.ConstU32(comp)));
                }
            } else {
                ASSERT(!param.is_loaded);
                if (param.num_components > 1) {
                    return spv_OpLoad(&ctx.m, param.component_type,
                                      spv_OpAccessChain1(&ctx.m, param.pointer_type, param.id,
                                                         ctx.ConstU32(comp)));
                } else {
                    return spv_OpLoad(&ctx.m, param.component_type, param.id);
                }
            }
        }();
        return param.is_integer ? spv_OpBitcast(&ctx.m, ctx.F32[1], value) : value;
    }
    if (IR::IsBarycentricCoord(attr) && attr != IR::Attribute::BaryCoordPullModel &&
        ctx.profile.supports_fragment_shader_barycentric) {
        ++comp;
    }
    switch (attr) {
    case IR::Attribute::Position0:
        ASSERT(ctx.sw_stage == SwStage::Geometry);
        return spv_OpLoad(&ctx.m, ctx.F32[1],
                          spv_OpAccessChain3(&ctx.m, ctx.input_f32, ctx.gl_in, ctx.ConstU32(index),
                                             ctx.ConstU32(0U), ctx.ConstU32(comp)));
    case IR::Attribute::FragCoord:
        return spv_OpLoad(
            &ctx.m, ctx.F32[1],
            spv_OpAccessChain1(&ctx.m, ctx.input_f32, ctx.frag_coord, ctx.ConstU32(comp)));
    case IR::Attribute::TessellationEvaluationPointU:
        return spv_OpLoad(
            &ctx.m, ctx.F32[1],
            spv_OpAccessChain1(&ctx.m, ctx.input_f32, ctx.tess_coord, ctx.u32_zero_value));
    case IR::Attribute::TessellationEvaluationPointV:
        return spv_OpLoad(
            &ctx.m, ctx.F32[1],
            spv_OpAccessChain1(&ctx.m, ctx.input_f32, ctx.tess_coord, ctx.ConstU32(1U)));
    case IR::Attribute::BaryCoordSmooth:
        if (ctx.profile.supports_amd_shader_explicit_vertex_parameter) {
            return spv_OpLoad(&ctx.m, ctx.F32[1],
                              spv_OpAccessChain1(&ctx.m, ctx.input_f32, ctx.bary_coord_smooth,
                                                 ctx.ConstU32(comp)));
        } else {
            return spv_OpCompositeExtract1(
                &ctx.m, ctx.F32[1], spv_OpLoad(&ctx.m, ctx.F32[3], ctx.bary_coord), u32(comp));
        }
    case IR::Attribute::BaryCoordSmoothCentroid:
        if (ctx.profile.supports_amd_shader_explicit_vertex_parameter) {
            return spv_OpLoad(&ctx.m, ctx.F32[1],
                              spv_OpAccessChain1(&ctx.m, ctx.input_f32,
                                                 ctx.bary_coord_smooth_centroid,
                                                 ctx.ConstU32(comp)));
        } else {
            return spv_OpCompositeExtract1(
                &ctx.m, ctx.F32[1], spv_OpInterpolateAtCentroid(&ctx.m, ctx.F32[3], ctx.bary_coord),
                u32(comp));
        }
    case IR::Attribute::BaryCoordSmoothSample:
        if (ctx.profile.supports_amd_shader_explicit_vertex_parameter) {
            return spv_OpLoad(&ctx.m, ctx.F32[1],
                              spv_OpAccessChain1(&ctx.m, ctx.input_f32,
                                                 ctx.bary_coord_smooth_sample, ctx.ConstU32(comp)));
        } else {
            return spv_OpCompositeExtract1(
                &ctx.m, ctx.F32[1],
                spv_OpInterpolateAtSample(&ctx.m, ctx.F32[3], ctx.bary_coord,
                                          spv_OpLoad(&ctx.m, ctx.U32[1], ctx.sample_index)),
                u32(comp));
        }
    case IR::Attribute::BaryCoordNoPersp:
        return spv_OpLoad(
            &ctx.m, ctx.F32[1],
            spv_OpAccessChain1(&ctx.m, ctx.input_f32, ctx.bary_coord_nopersp, ctx.ConstU32(comp)));
    case IR::Attribute::BaryCoordNoPerspSample:
        return spv_OpLoad(&ctx.m, ctx.F32[1],
                          spv_OpAccessChain1(&ctx.m, ctx.input_f32, ctx.bary_coord_nopersp_sample,
                                             ctx.ConstU32(comp)));
    case IR::Attribute::BaryCoordPullModel: {
        ASSERT_MSG(comp < 3, "Invalid BaryCoordPullModel component %u", comp);

        if (ctx.profile.supports_amd_shader_explicit_vertex_parameter) {
            return spv_OpLoad(&ctx.m, ctx.F32[1],
                              spv_OpAccessChain1(&ctx.m, ctx.input_f32, ctx.bary_coord_pull_model,
                                                 ctx.ConstU32(comp)));
        }

        // FragCoord.w is reciprocal clip W at the fragment center. Perspective barycentrics
        // are (I/W)/(1/W) and (J/W)/(1/W), so multiplying by FragCoord.w recovers the
        // pull-model values expected by the guest.
        const Id inv_w =
            spv_OpLoad(&ctx.m, ctx.F32[1],
                       spv_OpAccessChain1(&ctx.m, ctx.input_f32, ctx.frag_coord, ctx.ConstU32(3U)));
        if (comp == 2) {
            return inv_w;
        }
        const Id ij = spv_OpCompositeExtract1(
            &ctx.m, ctx.F32[1], spv_OpLoad(&ctx.m, ctx.F32[3], ctx.bary_coord), u32(comp + 1));
        return spv_OpFMul(&ctx.m, ctx.F32[1], ij, inv_w);
    }
    default:
        UNREACHABLE_MSG("Read attribute %s", Shader::IR::NameOf(attr).c_str());
    }
}

Id EmitGetAttributeU1(EmitContext& ctx, IR::Attribute attr, Imm32 comp_imm) {
    u32 comp = comp_imm;
    ASSERT(comp == 0);
    switch (attr) {
    case IR::Attribute::IsFrontFace:
        return spv_OpLoad(&ctx.m, ctx.U1[1], ctx.front_facing);
    case IR::Attribute::IsHelperInvocation:
        return spv_OpLoad(&ctx.m, ctx.U1[1], ctx.helper_invocation);
    default:
        UNREACHABLE_MSG("Unsupported U1 attribute %s", Shader::IR::NameOf(attr).c_str());
    }
}

Id EmitGetAttributeU32(EmitContext& ctx, IR::Attribute attr, Imm32 comp_imm) {
    u32 comp = comp_imm;
    switch (attr) {
    case IR::Attribute::VertexId:
        return spv_OpLoad(&ctx.m, ctx.U32[1], ctx.vertex_index);
    case IR::Attribute::InstanceId:
        return spv_OpLoad(&ctx.m, ctx.U32[1], ctx.instance_id);
    case IR::Attribute::BaseVertex:
        return spv_OpLoad(&ctx.m, ctx.U32[1], ctx.base_vertex);
    case IR::Attribute::BaseInstance:
        return spv_OpLoad(&ctx.m, ctx.U32[1], ctx.base_instance);
    case IR::Attribute::WorkgroupIndex:
        return ctx.workgroup_index_id;
    case IR::Attribute::WorkgroupId:
        return spv_OpCompositeExtract1(&ctx.m, ctx.U32[1],
                                       spv_OpLoad(&ctx.m, ctx.U32[3], ctx.workgroup_id), u32(comp));
    case IR::Attribute::LocalInvocationId:
        return spv_OpCompositeExtract1(
            &ctx.m, ctx.U32[1], spv_OpLoad(&ctx.m, ctx.U32[3], ctx.local_invocation_id), u32(comp));
    case IR::Attribute::LocalInvocationIndex:
        return spv_OpLoad(&ctx.m, ctx.U32[1], ctx.local_invocation_index);
    case IR::Attribute::SampleIndex:
        return spv_OpLoad(&ctx.m, ctx.U32[1], ctx.sample_index);
    case IR::Attribute::SampleMask:
        return spv_OpLoad(
            &ctx.m, ctx.U32[1],
            spv_OpAccessChain1(&ctx.m, ctx.input_u32, ctx.sample_mask_in, ctx.u32_zero_value));
    case IR::Attribute::RenderTargetIndex:
        return spv_OpLoad(&ctx.m, ctx.U32[1], ctx.output_layer);
    case IR::Attribute::PrimitiveId:
        return spv_OpLoad(&ctx.m, ctx.U32[1], ctx.primitive_id);
    case IR::Attribute::InvocationId:
        ASSERT(ctx.info.sw_stage == SwStage::Geometry ||
               ctx.info.sw_stage == SwStage::TessellationControl);
        return spv_OpLoad(&ctx.m, ctx.U32[1], ctx.invocation_id);
    case IR::Attribute::SubgroupLtMask:
        return spv_OpLoad(
            &ctx.m, ctx.U32[1],
            spv_OpAccessChain1(&ctx.m, ctx.input_u32, ctx.subgroup_lt_mask, ctx.ConstU32(comp)));
    case IR::Attribute::PatchVertices:
        ASSERT(ctx.info.sw_stage == SwStage::TessellationControl);
        return spv_OpLoad(&ctx.m, ctx.U32[1], ctx.patch_vertices);
    case IR::Attribute::PackedHullInvocationInfo: {
        ASSERT(ctx.info.sw_stage == SwStage::TessellationControl);
        // [0:8]: patch id within VGT
        // [8:12]: output control point id
        // But 0:8 should be treated as 0 for attribute addressing purposes
        if (ctx.runtime_info.sw.tcs.IsPassthrough()) {
            // Gcn shader would run with 1 thread, but we need to run a thread for
            // each output control point.
            // If Gcn shader uses this value, we should make sure all threads in the
            // Vulkan shader use 0
            return ctx.u32_zero_value;
        } else {
            const Id invocation_id = spv_OpLoad(&ctx.m, ctx.U32[1], ctx.invocation_id);
            return spv_OpShiftLeftLogical(&ctx.m, ctx.U32[1], invocation_id, ctx.ConstU32(8u));
        }
    }
    default:
        UNREACHABLE_MSG("Read U32 attribute %s", Shader::IR::NameOf(attr).c_str());
    }
}

void EmitSetAttribute(EmitContext& ctx, IR::Attribute attr, Id value, Imm32 element_imm) {
    u32 element = element_imm;
    const auto op_store = [&](Id pointer) {
        const auto [component_type, is_integer] = OutputAttrComponentType(ctx, attr);
        if (is_integer) {
            spv_OpStore(&ctx.m, pointer, spv_OpBitcast(&ctx.m, component_type, value));
        } else {
            spv_OpStore(&ctx.m, pointer, value);
        }
    };
    if (IR::IsParam(attr)) {
        const u32 attr_index{u32(attr) - u32(IR::Attribute::Param0)};
        if (ctx.hw_stage == HwStage::Local) {
            const auto component_ptr =
                spv_TypePointer(&ctx.m, u32(SpvStorageClassOutput), ctx.F32[1]);
            return op_store(spv_OpAccessChain2(&ctx.m, component_ptr, ctx.output_attr_array,
                                               ctx.ConstU32(attr_index), ctx.ConstU32(element)));
        } else {
            const auto& info{ctx.output_params.at(attr_index)};
            ASSERT(info.num_components > 0);
            if (info.num_components == 1) {
                return op_store(info.id);
            } else {
                return op_store(
                    spv_OpAccessChain1(&ctx.m, info.pointer_type, info.id, ctx.ConstU32(element)));
            }
        }
    }
    if (IR::IsMrt(attr)) {
        const u32 index{u32(attr) - u32(IR::Attribute::RenderTarget0)};
        const auto& info{ctx.frag_outputs.at(index)};
        if (element < 3 && ctx.runtime_info.hw.fs.color_buffers[index].blend_self_scale) {
            // Emulates GCN's factor-scaled min/max blend: min/max(src*src, dst*dst).
            value = spv_OpFMul(&ctx.m, ctx.F32[1], value, value);
        }
        if (info.num_components == 1) {
            return op_store(info.id);
        } else {
            return op_store(
                spv_OpAccessChain1(&ctx.m, info.pointer_type, info.id, ctx.ConstU32(element)));
        }
    }
    switch (attr) {
    case IR::Attribute::Position0:
        return op_store(
            spv_OpAccessChain1(&ctx.m, ctx.output_f32, ctx.output_position, ctx.ConstU32(element)));
    case IR::Attribute::ClipDistance:
        return op_store(
            spv_OpAccessChain1(&ctx.m, ctx.output_f32, ctx.clip_distances, ctx.ConstU32(element)));
    case IR::Attribute::CullDistance:
        return op_store(
            spv_OpAccessChain1(&ctx.m, ctx.output_f32, ctx.cull_distances, ctx.ConstU32(element)));
    case IR::Attribute::PointSize:
        return op_store(ctx.output_point_size);
    case IR::Attribute::RenderTargetIndex:
        return op_store(ctx.output_layer);
    case IR::Attribute::ViewportIndex:
        return op_store(ctx.output_viewport_index);
    case IR::Attribute::Depth:
        return op_store(ctx.frag_depth);
    case IR::Attribute::SampleMask:
        return op_store(
            spv_OpAccessChain1(&ctx.m, ctx.output_u32, ctx.sample_mask, ctx.u32_zero_value));
    case IR::Attribute::StencilRef:
        if (ctx.profile.supports_shader_stencil_export) {
            return op_store(ctx.stencil_ref);
        }
        return;
    default:
        UNREACHABLE_MSG("Write attribute %s", Shader::IR::NameOf(attr).c_str());
    }
}

Id EmitGetTessGenericAttribute(EmitContext& ctx, Id vertex_index, Id attr_index, Id comp_index) {
    const auto attr_comp_ptr = spv_TypePointer(&ctx.m, u32(SpvStorageClassInput), ctx.F32[1]);
    return spv_OpLoad(&ctx.m, ctx.F32[1],
                      spv_OpAccessChain3(&ctx.m, attr_comp_ptr, ctx.input_attr_array, vertex_index,
                                         attr_index, comp_index));
}

Id EmitReadTcsGenericOuputAttribute(EmitContext& ctx, Id vertex_index, Id attr_index,
                                    Id comp_index) {
    const auto attr_comp_ptr = spv_TypePointer(&ctx.m, u32(SpvStorageClassOutput), ctx.F32[1]);
    return spv_OpLoad(&ctx.m, ctx.F32[1],
                      spv_OpAccessChain3(&ctx.m, attr_comp_ptr, ctx.output_attr_array, vertex_index,
                                         attr_index, comp_index));
}

void EmitSetTcsGenericAttribute(EmitContext& ctx, Id value, Id attr_index, Id comp_index) {
    // Implied vertex index is invocation_id
    const auto component_ptr = spv_TypePointer(&ctx.m, u32(SpvStorageClassOutput), ctx.F32[1]);
    Id pointer = spv_OpAccessChain3(&ctx.m, component_ptr, ctx.output_attr_array,
                                    spv_OpLoad(&ctx.m, ctx.U32[1], ctx.invocation_id), attr_index,
                                    comp_index);
    spv_OpStore(&ctx.m, pointer, value);
}

Id EmitGetPatch(EmitContext& ctx, IR::Patch patch) {
    const u32 index{IR::GenericPatchIndex(patch)};
    const Id element{ctx.ConstU32(IR::GenericPatchElement(patch))};
    const Id type{ctx.sw_stage == SwStage::TessellationControl ? ctx.output_f32 : ctx.input_f32};
    const Id pointer{spv_OpAccessChain1(&ctx.m, type, ctx.patches.at(index), element)};
    return spv_OpLoad(&ctx.m, ctx.F32[1], pointer);
}

void EmitSetPatch(EmitContext& ctx, IR::Patch patch, Id value) {
    const Id pointer{[&] {
        if (IR::IsGeneric(patch)) {
            const u32 index{IR::GenericPatchIndex(patch)};
            const Id element{ctx.ConstU32(IR::GenericPatchElement(patch))};
            return spv_OpAccessChain1(&ctx.m, ctx.output_f32, ctx.patches.at(index), element);
        }
        switch (patch) {
        case IR::Patch::TessellationLodLeft:
        case IR::Patch::TessellationLodRight:
        case IR::Patch::TessellationLodTop:
        case IR::Patch::TessellationLodBottom: {
            const u32 index{static_cast<u32>(patch) - u32(IR::Patch::TessellationLodLeft)};
            const Id index_id{ctx.ConstU32(index)};
            return spv_OpAccessChain1(&ctx.m, ctx.output_f32, ctx.output_tess_level_outer,
                                      index_id);
        }
        case IR::Patch::TessellationLodInteriorU:
            return spv_OpAccessChain1(&ctx.m, ctx.output_f32, ctx.output_tess_level_inner,
                                      ctx.u32_zero_value);
        case IR::Patch::TessellationLodInteriorV:
            return spv_OpAccessChain1(&ctx.m, ctx.output_f32, ctx.output_tess_level_inner,
                                      ctx.ConstU32(1u));
        default:
            UNREACHABLE_MSG("Patch %u", u32(patch));
        }
    }()};
    spv_OpStore(&ctx.m, pointer, value);
}

template <u32 N, PointerType alias>
static Id EmitLoadBufferB32xN(EmitContext& ctx, IR::Inst* inst, u32 handle, Id address) {
    const auto& spv_buffer = ctx.buffers[handle];
    if (const Id offset = spv_buffer.Offset(PointerSize::B32); (offset != 0)) {
        address = spv_OpIAdd(&ctx.m, ctx.U32[1], address, offset);
    }
    const auto& data_types = alias == PointerType::U32 ? ctx.U32 : ctx.F32;
    const auto [id, pointer_type] = spv_buffer.Alias(alias);

    Common::StaticVector<Id, N> ids;
    for (u32 i = 0; i < N; i++) {
        const Id index_i =
            i == 0 ? address : spv_OpIAdd(&ctx.m, ctx.U32[1], address, ctx.ConstU32(i));
        const Id ptr_i = spv_OpAccessChain2(&ctx.m, pointer_type, id, ctx.u32_zero_value, index_i);
        const Id result_i = spv_OpLoad(&ctx.m, data_types[1], ptr_i);
        ids.push_back(result_i);
    }

    const Id result =
        N == 1 ? ids[0]
               : spv_OpCompositeConstruct(&ctx.m, data_types[N], ids.data(), u32(ids.size()));
    return result;
}

Id EmitLoadBufferU8(EmitContext& ctx, IR::Inst* inst, Imm32 handle_imm, Id address) {
    u32 handle = handle_imm;
    const auto& spv_buffer = ctx.buffers[handle];
    if (const Id offset = spv_buffer.Offset(PointerSize::B8); (offset != 0)) {
        address = spv_OpIAdd(&ctx.m, ctx.U32[1], address, offset);
    }
    const auto [id, pointer_type] = spv_buffer.Alias(PointerType::U8);
    const Id ptr{spv_OpAccessChain2(&ctx.m, pointer_type, id, ctx.u32_zero_value, address)};
    const Id result{spv_OpLoad(&ctx.m, ctx.U8, ptr)};
    return result;
}

Id EmitLoadBufferU16(EmitContext& ctx, IR::Inst* inst, Imm32 handle_imm, Id address) {
    u32 handle = handle_imm;
    const auto& spv_buffer = ctx.buffers[handle];
    if (const Id offset = spv_buffer.Offset(PointerSize::B16); (offset != 0)) {
        address = spv_OpIAdd(&ctx.m, ctx.U32[1], address, offset);
    }
    const auto [id, pointer_type] = spv_buffer.Alias(PointerType::U16);
    const Id ptr{spv_OpAccessChain2(&ctx.m, pointer_type, id, ctx.u32_zero_value, address)};
    const Id result{spv_OpLoad(&ctx.m, ctx.U16, ptr)};
    return result;
}

Id EmitLoadBufferU32(EmitContext& ctx, IR::Inst* inst, Imm32 handle_imm, Id address) {
    u32 handle = handle_imm;
    return EmitLoadBufferB32xN<1, PointerType::U32>(ctx, inst, handle, address);
}

Id EmitLoadBufferU32x2(EmitContext& ctx, IR::Inst* inst, Imm32 handle_imm, Id address) {
    u32 handle = handle_imm;
    return EmitLoadBufferB32xN<2, PointerType::U32>(ctx, inst, handle, address);
}

Id EmitLoadBufferU32x3(EmitContext& ctx, IR::Inst* inst, Imm32 handle_imm, Id address) {
    u32 handle = handle_imm;
    return EmitLoadBufferB32xN<3, PointerType::U32>(ctx, inst, handle, address);
}

Id EmitLoadBufferU32x4(EmitContext& ctx, IR::Inst* inst, Imm32 handle_imm, Id address) {
    u32 handle = handle_imm;
    return EmitLoadBufferB32xN<4, PointerType::U32>(ctx, inst, handle, address);
}

Id EmitLoadBufferU64(EmitContext& ctx, IR::Inst* inst, Imm32 handle_imm, Id address) {
    u32 handle = handle_imm;
    const auto& spv_buffer = ctx.buffers[handle];
    if (const Id offset = spv_buffer.Offset(PointerSize::B64); (offset != 0)) {
        address = spv_OpIAdd(&ctx.m, ctx.U32[1], address, offset);
    }
    const auto [id, pointer_type] = spv_buffer.Alias(PointerType::U64);
    const Id ptr{spv_OpAccessChain2(&ctx.m, pointer_type, id, ctx.u64_zero_value, address)};
    const Id result{spv_OpLoad(&ctx.m, ctx.U64, ptr)};
    return result;
}

Id EmitLoadBufferF32(EmitContext& ctx, IR::Inst* inst, Imm32 handle_imm, Id address) {
    u32 handle = handle_imm;
    return EmitLoadBufferB32xN<1, PointerType::F32>(ctx, inst, handle, address);
}

Id EmitLoadBufferF32x2(EmitContext& ctx, IR::Inst* inst, Imm32 handle_imm, Id address) {
    u32 handle = handle_imm;
    return EmitLoadBufferB32xN<2, PointerType::F32>(ctx, inst, handle, address);
}

Id EmitLoadBufferF32x3(EmitContext& ctx, IR::Inst* inst, Imm32 handle_imm, Id address) {
    u32 handle = handle_imm;
    return EmitLoadBufferB32xN<3, PointerType::F32>(ctx, inst, handle, address);
}

Id EmitLoadBufferF32x4(EmitContext& ctx, IR::Inst* inst, Imm32 handle_imm, Id address) {
    u32 handle = handle_imm;
    return EmitLoadBufferB32xN<4, PointerType::F32>(ctx, inst, handle, address);
}

Id EmitLoadBufferFormatF32(EmitContext& ctx, IR::Inst* inst, Imm32 handle_imm, Id address) {
    u32 handle = handle_imm;
    UNREACHABLE_MSG("SPIR-V instruction");
}

template <u32 N, PointerType alias>
static void EmitStoreBufferB32xN(EmitContext& ctx, IR::Inst* inst, u32 handle, Id address,
                                 Id value) {
    const auto& spv_buffer = ctx.buffers[handle];
    if (const Id offset = spv_buffer.Offset(PointerSize::B32); (offset != 0)) {
        address = spv_OpIAdd(&ctx.m, ctx.U32[1], address, offset);
    }
    const auto& data_types = alias == PointerType::U32 ? ctx.U32 : ctx.F32;
    const auto [id, pointer_type] = spv_buffer.Alias(alias);

    for (u32 i = 0; i < N; i++) {
        const Id index_i =
            i == 0 ? address : spv_OpIAdd(&ctx.m, ctx.U32[1], address, ctx.ConstU32(i));
        const Id ptr_i = spv_OpAccessChain2(&ctx.m, pointer_type, id, ctx.u32_zero_value, index_i);
        const Id value_i =
            N == 1 ? value : spv_OpCompositeExtract1(&ctx.m, data_types[1], value, u32(i));
        spv_OpStore(&ctx.m, ptr_i, value_i);
    }
}

void EmitStoreBufferU8(EmitContext& ctx, IR::Inst*, Imm32 handle_imm, Id address, Id value) {
    u32 handle = handle_imm;
    const auto& spv_buffer = ctx.buffers[handle];
    if (const Id offset = spv_buffer.Offset(PointerSize::B8); (offset != 0)) {
        address = spv_OpIAdd(&ctx.m, ctx.U32[1], address, offset);
    }
    const auto [id, pointer_type] = spv_buffer.Alias(PointerType::U8);
    const Id ptr{spv_OpAccessChain2(&ctx.m, pointer_type, id, ctx.u32_zero_value, address)};
    spv_OpStore(&ctx.m, ptr, value);
}

void EmitStoreBufferU16(EmitContext& ctx, IR::Inst*, Imm32 handle_imm, Id address, Id value) {
    u32 handle = handle_imm;
    const auto& spv_buffer = ctx.buffers[handle];
    if (const Id offset = spv_buffer.Offset(PointerSize::B16); (offset != 0)) {
        address = spv_OpIAdd(&ctx.m, ctx.U32[1], address, offset);
    }
    const auto [id, pointer_type] = spv_buffer.Alias(PointerType::U16);
    const Id ptr{spv_OpAccessChain2(&ctx.m, pointer_type, id, ctx.u32_zero_value, address)};
    spv_OpStore(&ctx.m, ptr, value);
}

void EmitStoreBufferU32(EmitContext& ctx, IR::Inst* inst, Imm32 handle_imm, Id address, Id value) {
    u32 handle = handle_imm;
    EmitStoreBufferB32xN<1, PointerType::U32>(ctx, inst, handle, address, value);
}

void EmitStoreBufferU32x2(EmitContext& ctx, IR::Inst* inst, Imm32 handle_imm, Id address,
                          Id value) {
    u32 handle = handle_imm;
    EmitStoreBufferB32xN<2, PointerType::U32>(ctx, inst, handle, address, value);
}

void EmitStoreBufferU32x3(EmitContext& ctx, IR::Inst* inst, Imm32 handle_imm, Id address,
                          Id value) {
    u32 handle = handle_imm;
    EmitStoreBufferB32xN<3, PointerType::U32>(ctx, inst, handle, address, value);
}

void EmitStoreBufferU32x4(EmitContext& ctx, IR::Inst* inst, Imm32 handle_imm, Id address,
                          Id value) {
    u32 handle = handle_imm;
    EmitStoreBufferB32xN<4, PointerType::U32>(ctx, inst, handle, address, value);
}

void EmitStoreBufferU64(EmitContext& ctx, IR::Inst*, Imm32 handle_imm, Id address, Id value) {
    u32 handle = handle_imm;
    const auto& spv_buffer = ctx.buffers[handle];
    if (const Id offset = spv_buffer.Offset(PointerSize::B64); (offset != 0)) {
        address = spv_OpIAdd(&ctx.m, ctx.U32[1], address, offset);
    }
    const auto [id, pointer_type] = spv_buffer.Alias(PointerType::U64);
    const Id ptr{spv_OpAccessChain2(&ctx.m, pointer_type, id, ctx.u64_zero_value, address)};
    spv_OpStore(&ctx.m, ptr, value);
}

void EmitStoreBufferF32(EmitContext& ctx, IR::Inst* inst, Imm32 handle_imm, Id address, Id value) {
    u32 handle = handle_imm;
    EmitStoreBufferB32xN<1, PointerType::F32>(ctx, inst, handle, address, value);
}

void EmitStoreBufferF32x2(EmitContext& ctx, IR::Inst* inst, Imm32 handle_imm, Id address,
                          Id value) {
    u32 handle = handle_imm;
    EmitStoreBufferB32xN<2, PointerType::F32>(ctx, inst, handle, address, value);
}

void EmitStoreBufferF32x3(EmitContext& ctx, IR::Inst* inst, Imm32 handle_imm, Id address,
                          Id value) {
    u32 handle = handle_imm;
    EmitStoreBufferB32xN<3, PointerType::F32>(ctx, inst, handle, address, value);
}

void EmitStoreBufferF32x4(EmitContext& ctx, IR::Inst* inst, Imm32 handle_imm, Id address,
                          Id value) {
    u32 handle = handle_imm;
    EmitStoreBufferB32xN<4, PointerType::F32>(ctx, inst, handle, address, value);
}

void EmitStoreBufferFormatF32(EmitContext& ctx, IR::Inst* inst, Imm32 handle_imm, Id address,
                              Id value) {
    u32 handle = handle_imm;
    UNREACHABLE_MSG("SPIR-V instruction");
}

void EmitGetScalarRegister(EmitContext&) {
    UNREACHABLE_MSG("Unreachable instruction");
}

void EmitSetScalarRegister(EmitContext&) {
    UNREACHABLE_MSG("Unreachable instruction");
}

void EmitGetVectorRegister(EmitContext& ctx) {
    UNREACHABLE_MSG("Unreachable instruction");
}

void EmitSetVectorRegister(EmitContext& ctx) {
    UNREACHABLE_MSG("Unreachable instruction");
}

void EmitSetVirtualRegister(EmitContext& ctx) {
    UNREACHABLE_MSG("Unreachable instruction");
}

void EmitGetVirtualRegister(EmitContext& ctx) {
    UNREACHABLE_MSG("Unreachable instruction");
}

void EmitSetGotoVariable(EmitContext&) {
    UNREACHABLE_MSG("Unreachable instruction");
}

void EmitGetGotoVariable(EmitContext&) {
    UNREACHABLE_MSG("Unreachable instruction");
}

} // namespace Shader::Backend::SPIRV
