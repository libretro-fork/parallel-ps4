// SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "shader_recompiler/backend/spirv/emit_spirv_instructions.h"
#include "shader_recompiler/backend/spirv/spirv_emit_context.h"
#include "shader_recompiler/ir/debug_print.h"
#include "shader_recompiler/ir/microinstruction.h"

namespace Shader::Backend::SPIRV {

void EmitPrologue(EmitContext& ctx) {
    if (ctx.hw_stage == HwStage::Fragment) {
        ctx.DefineAmdPerVertexAttribs();
    }
    if (ctx.info.loads.Get(IR::Attribute::WorkgroupIndex)) {
        ctx.DefineWorkgroupIndex();
    }
    ctx.DefineBufferProperties();
}

void ConvertDepthMode(EmitContext& ctx) {
    const Id type{ctx.F32[1]};
    const Id position{spv_OpLoad(&ctx.m, ctx.F32[4], ctx.output_position)};
    const Id z{spv_OpCompositeExtract1(&ctx.m, type, position, 2u)};
    const Id w{spv_OpCompositeExtract1(&ctx.m, type, position, 3u)};
    const Id screen_depth{spv_OpFMul(&ctx.m, type, spv_OpFAdd(&ctx.m, type, z, w),
                                     spv_ConstantF32(&ctx.m, type, f32(0.5f)))};
    const Id vector{spv_OpCompositeInsert1(&ctx.m, ctx.F32[4], screen_depth, position, 2u)};
    spv_OpStore(&ctx.m, ctx.output_position, vector);
}

void ConvertPositionToClipSpace(EmitContext& ctx) {
    ASSERT_MSG(!ctx.info.stores.GetAny(IR::Attribute::ViewportIndex),
               "Multi-viewport with shader clip space conversion not yet implemented.");

    const Id type{ctx.F32[1]};
    Id position{spv_OpLoad(&ctx.m, ctx.F32[4], ctx.output_position)};
    const Id x{spv_OpCompositeExtract1(&ctx.m, type, position, 0u)};
    const Id y{spv_OpCompositeExtract1(&ctx.m, type, position, 1u)};
    const Id z{spv_OpCompositeExtract1(&ctx.m, type, position, 2u)};
    const Id w{spv_OpCompositeExtract1(&ctx.m, type, position, 3u)};
    const Id xoffset_ptr{
        spv_OpAccessChain1(&ctx.m, spv_TypePointer(&ctx.m, u32(SpvStorageClassPushConstant), type),
                           ctx.push_data_block, ctx.ConstU32(PushData::XOffsetIndex))};
    const Id xoffset{spv_OpLoad(&ctx.m, type, xoffset_ptr)};
    const Id yoffset_ptr{
        spv_OpAccessChain1(&ctx.m, spv_TypePointer(&ctx.m, u32(SpvStorageClassPushConstant), type),
                           ctx.push_data_block, ctx.ConstU32(PushData::YOffsetIndex))};
    const Id yoffset{spv_OpLoad(&ctx.m, type, yoffset_ptr)};
    const Id xscale_ptr{
        spv_OpAccessChain1(&ctx.m, spv_TypePointer(&ctx.m, u32(SpvStorageClassPushConstant), type),
                           ctx.push_data_block, ctx.ConstU32(PushData::XScaleIndex))};
    const Id xscale{spv_OpLoad(&ctx.m, type, xscale_ptr)};
    const Id yscale_ptr{
        spv_OpAccessChain1(&ctx.m, spv_TypePointer(&ctx.m, u32(SpvStorageClassPushConstant), type),
                           ctx.push_data_block, ctx.ConstU32(PushData::YScaleIndex))};
    const Id yscale{spv_OpLoad(&ctx.m, type, yscale_ptr)};
    const Id vport_w = spv_ConstantF32(
        &ctx.m, type, f32(float(std::min<u32>(ctx.profile.max_viewport_width / 2, 8_KB))));
    const Id wnd_x = spv_OpFAdd(&ctx.m, type, spv_OpFMul(&ctx.m, type, x, xscale), xoffset);
    const Id ndc_x = spv_OpFSub(&ctx.m, type, spv_OpFDiv(&ctx.m, type, wnd_x, vport_w),
                                spv_ConstantF32(&ctx.m, type, f32(1.f)));
    const Id vport_h = spv_ConstantF32(
        &ctx.m, type, f32(float(std::min<u32>(ctx.profile.max_viewport_height / 2, 8_KB))));
    const Id wnd_y = spv_OpFAdd(&ctx.m, type, spv_OpFMul(&ctx.m, type, y, yscale), yoffset);
    const Id ndc_y = spv_OpFSub(&ctx.m, type, spv_OpFDiv(&ctx.m, type, wnd_y, vport_h),
                                spv_ConstantF32(&ctx.m, type, f32(1.f)));
    const Id vector{spv_OpCompositeConstruct(
        &ctx.m, ctx.F32[4], (std::array<Id, 4>({ndc_x, ndc_y, z, w})).data(),
        u32((std::array<Id, 4>({ndc_x, ndc_y, z, w})).size()))};
    spv_OpStore(&ctx.m, ctx.output_position, vector);
}

void EmitEpilogue(EmitContext& ctx) {
    if (ctx.hw_stage == HwStage::Vertex &&
        ctx.runtime_info.hw.vs.emulate_depth_negative_one_to_one) {
        ConvertDepthMode(ctx);
    }
    if (ctx.hw_stage == HwStage::Vertex && ctx.runtime_info.hw.vs.clip_disable) {
        ConvertPositionToClipSpace(ctx);
    }
}

void EmitDiscard(EmitContext& ctx) {
    spv_OpDemoteToHelperInvocationEXT(&ctx.m);
}

void EmitDiscardCond(EmitContext& ctx, Id condition) {
    const Id kill_label{spv_OpLabel(&ctx.m)};
    const Id merge_label{spv_OpLabel(&ctx.m)};
    spv_OpSelectionMerge(&ctx.m, merge_label, u32(SpvSelectionControlMaskNone));
    spv_OpBranchConditional(&ctx.m, condition, kill_label, merge_label);
    ctx.AddLabel(kill_label);
    spv_OpDemoteToHelperInvocationEXT(&ctx.m);
    spv_OpBranch(&ctx.m, merge_label);
    ctx.AddLabel(merge_label);
}

void EmitEmitVertex(EmitContext& ctx) {
    spv_OpEmitVertex(&ctx.m);
}

void EmitEmitPrimitive(EmitContext& ctx) {
    spv_OpEndPrimitive(&ctx.m);
}

void EmitEmitVertex(EmitContext& ctx, const IR::Value& stream) {
    UNREACHABLE_MSG("Geometry streams");
}

void EmitEndPrimitive(EmitContext& ctx, const IR::Value& stream) {
    UNREACHABLE_MSG("Geometry streams");
}

void EmitDebugPrint(EmitContext& ctx, IR::Inst* inst, Id fmt, Id arg0, Id arg1, Id arg2, Id arg3) {
    IR::DebugPrintFlags flags = inst->Flags<IR::DebugPrintFlags>();
    std::array<Id, IR::DEBUGPRINT_NUM_FORMAT_ARGS> fmt_args = {arg0, arg1, arg2, arg3};
    std::span<unsigned int> fmt_args_span =
        std::span<Id>(fmt_args.begin(), fmt_args.begin() + flags.num_args);
    spv_OpDebugPrintf(&ctx.m, fmt, fmt_args_span.data(), u32(fmt_args_span.size()));
}

Id EmitMemtime(EmitContext& ctx) {
    if (ctx.profile.supports_shader_subgroup_clock) {
        return spv_OpReadClockKHR(&ctx.m, ctx.U64,
                                  ctx.ConstU32(std::to_underlying(SpvScopeSubgroup)));
    } else {
        return spv_Constant64(&ctx.m, ctx.U64, u64(1U));
    }
}

} // namespace Shader::Backend::SPIRV
