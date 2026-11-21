// SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "shader_recompiler/backend/spirv/emit_spirv_instructions.h"
#include "shader_recompiler/backend/spirv/spirv_emit_context.h"

namespace Shader::Backend::SPIRV {

Id DecorateFp(EmitContext& ctx, IR::Inst* inst, Id op) {
    // Pre-rasterization stages keep it, so position math stays the same across pipelines.
    const bool can_contract = ctx.profile.fast_shader_math && (ctx.sw_stage == SwStage::Fragment ||
                                                               ctx.sw_stage == SwStage::Compute);
    if (!can_contract) {
        spv_Decorate0(&ctx.m, op, u32(SpvDecorationNoContraction));
    }
    return op;
}

Id EmitFPAbs32(EmitContext& ctx, Id value) {
    return spv_OpFAbs(&ctx.m, ctx.F32[1], value);
}

Id EmitFPAbs64(EmitContext& ctx, Id value) {
    return spv_OpFAbs(&ctx.m, ctx.F64[1], value);
}

Id EmitFPAdd32(EmitContext& ctx, IR::Inst* inst, Id a, Id b) {
    return DecorateFp(ctx, inst, spv_OpFAdd(&ctx.m, ctx.F32[1], a, b));
}

Id EmitFPAdd64(EmitContext& ctx, IR::Inst* inst, Id a, Id b) {
    return DecorateFp(ctx, inst, spv_OpFAdd(&ctx.m, ctx.F64[1], a, b));
}

Id EmitFPSub32(EmitContext& ctx, IR::Inst* inst, Id a, Id b) {
    return DecorateFp(ctx, inst, spv_OpFSub(&ctx.m, ctx.F32[1], a, b));
}

Id EmitFPFma32(EmitContext& ctx, IR::Inst* inst, Id a, Id b, Id c) {
    return DecorateFp(ctx, inst, spv_OpFma(&ctx.m, ctx.F32[1], a, b, c));
}

Id EmitFPFma64(EmitContext& ctx, IR::Inst* inst, Id a, Id b, Id c) {
    return DecorateFp(ctx, inst, spv_OpFma(&ctx.m, ctx.F64[1], a, b, c));
}

Id EmitFPMax32(EmitContext& ctx, Id a, Id b) {
    return spv_OpFMax(&ctx.m, ctx.F32[1], a, b);
}

Id EmitFPMax64(EmitContext& ctx, Id a, Id b) {
    return spv_OpFMax(&ctx.m, ctx.F64[1], a, b);
}

Id EmitFPMin32(EmitContext& ctx, Id a, Id b) {
    return spv_OpFMin(&ctx.m, ctx.F32[1], a, b);
}

Id EmitFPMin64(EmitContext& ctx, Id a, Id b) {
    return spv_OpFMin(&ctx.m, ctx.F64[1], a, b);
}

Id EmitFPMinTri32(EmitContext& ctx, Id a, Id b, Id c) {
    return spv_OpNMin(&ctx.m, ctx.F32[1], a, spv_OpNMin(&ctx.m, ctx.F32[1], b, c));
}

Id EmitFPMaxTri32(EmitContext& ctx, Id a, Id b, Id c) {
    return spv_OpNMax(&ctx.m, ctx.F32[1], a, spv_OpNMax(&ctx.m, ctx.F32[1], b, c));
}

Id EmitFPMedTri32(EmitContext& ctx, Id a, Id b, Id c) {
    const Id is_nan{
        spv_OpLogicalOr(&ctx.m, ctx.U1[1], spv_OpIsNan(&ctx.m, ctx.U1[1], a),
                        spv_OpLogicalOr(&ctx.m, ctx.U1[1], spv_OpIsNan(&ctx.m, ctx.U1[1], b),
                                        spv_OpIsNan(&ctx.m, ctx.U1[1], c)))};
    const Id mmx{spv_OpFMin(&ctx.m, ctx.F32[1], spv_OpFMax(&ctx.m, ctx.F32[1], a, b), c)};
    const Id med3{spv_OpFMax(&ctx.m, ctx.F32[1], spv_OpFMin(&ctx.m, ctx.F32[1], a, b), mmx)};
    const Id min3{spv_OpNMin(&ctx.m, ctx.F32[1], a, spv_OpNMin(&ctx.m, ctx.F32[1], b, c))};
    return spv_OpSelect(&ctx.m, ctx.F32[1], is_nan, min3, med3);
}

Id EmitFPMul32(EmitContext& ctx, IR::Inst* inst, Id a, Id b) {
    return DecorateFp(ctx, inst, spv_OpFMul(&ctx.m, ctx.F32[1], a, b));
}

Id EmitFPMul64(EmitContext& ctx, IR::Inst* inst, Id a, Id b) {
    return DecorateFp(ctx, inst, spv_OpFMul(&ctx.m, ctx.F64[1], a, b));
}

Id EmitFPDiv32(EmitContext& ctx, IR::Inst* inst, Id a, Id b) {
    return DecorateFp(ctx, inst, spv_OpFDiv(&ctx.m, ctx.F32[1], a, b));
}

Id EmitFPDiv64(EmitContext& ctx, IR::Inst* inst, Id a, Id b) {
    return DecorateFp(ctx, inst, spv_OpFDiv(&ctx.m, ctx.F64[1], a, b));
}

Id EmitFPNeg32(EmitContext& ctx, Id value) {
    return spv_OpFNegate(&ctx.m, ctx.F32[1], value);
}

Id EmitFPNeg64(EmitContext& ctx, Id value) {
    return spv_OpFNegate(&ctx.m, ctx.F64[1], value);
}

Id EmitFPSin(EmitContext& ctx, Id value) {
    return spv_OpSin(&ctx.m, ctx.F32[1], spv_OpFMul(&ctx.m, ctx.F32[1], ctx.pi_x2, value));
}

Id EmitFPCos(EmitContext& ctx, Id value) {
    return spv_OpCos(&ctx.m, ctx.F32[1], spv_OpFMul(&ctx.m, ctx.F32[1], ctx.pi_x2, value));
}

Id EmitFPExp2(EmitContext& ctx, Id value) {
    return spv_OpExp2(&ctx.m, ctx.F32[1], value);
}

Id EmitFPPow(EmitContext& ctx, Id x, Id y) {
    return spv_OpPow(&ctx.m, ctx.F32[1], x, y);
}

Id EmitFPLdexp(EmitContext& ctx, Id value, Id exp) {
    return spv_OpLdexp(&ctx.m, ctx.F32[1], value, exp);
}

Id EmitFPLog2(EmitContext& ctx, Id value) {
    return spv_OpLog2(&ctx.m, ctx.F32[1], value);
}

Id EmitFPRecip32(EmitContext& ctx, Id value) {
    return spv_OpFDiv(&ctx.m, ctx.F32[1], ctx.ConstF32(1.0f), value);
}

Id EmitFPRecip64(EmitContext& ctx, Id value) {
    return spv_OpFDiv(&ctx.m, ctx.F64[1], spv_ConstantF64(&ctx.m, ctx.F64[1], f64(f64{1.0})),
                      value);
}

Id EmitFPRecipSqrt32(EmitContext& ctx, Id value) {
    return spv_OpInverseSqrt(&ctx.m, ctx.F32[1], value);
}

Id EmitFPRecipSqrt64(EmitContext& ctx, Id value) {
    return spv_OpInverseSqrt(&ctx.m, ctx.F64[1], value);
}

Id EmitFPSqrt(EmitContext& ctx, Id value) {
    return spv_OpSqrt(&ctx.m, ctx.F32[1], value);
}

Id EmitFPSaturate32(EmitContext& ctx, Id value) {
    return spv_OpNClamp(&ctx.m, ctx.F32[1], value, ctx.f32_zero_value, ctx.f32_one_value);
}

Id EmitFPSaturate64(EmitContext& ctx, Id value) {
    const Id zero{spv_ConstantF64(&ctx.m, ctx.F64[1], f64(f64{0.0}))};
    const Id one{spv_ConstantF64(&ctx.m, ctx.F64[1], f64(f64{1.0}))};
    return spv_OpNClamp(&ctx.m, ctx.F64[1], value, zero, one);
}

Id EmitFPClamp32(EmitContext& ctx, Id value, Id min_value, Id max_value) {
    return spv_OpFClamp(&ctx.m, ctx.F32[1], value, min_value, max_value);
}

Id EmitFPClamp64(EmitContext& ctx, Id value, Id min_value, Id max_value) {
    return spv_OpFClamp(&ctx.m, ctx.F64[1], value, min_value, max_value);
}

Id EmitFPRoundEven32(EmitContext& ctx, Id value) {
    return spv_OpRoundEven(&ctx.m, ctx.F32[1], value);
}

Id EmitFPRoundEven64(EmitContext& ctx, Id value) {
    return spv_OpRoundEven(&ctx.m, ctx.F64[1], value);
}

Id EmitFPFloor32(EmitContext& ctx, Id value) {
    return spv_OpFloor(&ctx.m, ctx.F32[1], value);
}

Id EmitFPFloor64(EmitContext& ctx, Id value) {
    return spv_OpFloor(&ctx.m, ctx.F64[1], value);
}

Id EmitFPCeil32(EmitContext& ctx, Id value) {
    return spv_OpCeil(&ctx.m, ctx.F32[1], value);
}

Id EmitFPCeil64(EmitContext& ctx, Id value) {
    return spv_OpCeil(&ctx.m, ctx.F64[1], value);
}

Id EmitFPTrunc32(EmitContext& ctx, Id value) {
    return spv_OpTrunc(&ctx.m, ctx.F32[1], value);
}

Id EmitFPTrunc64(EmitContext& ctx, Id value) {
    return spv_OpTrunc(&ctx.m, ctx.F64[1], value);
}

Id EmitFPFract32(EmitContext& ctx, Id value) {
    return spv_OpFract(&ctx.m, ctx.F32[1], value);
}

Id EmitFPFract64(EmitContext& ctx, Id value) {
    return spv_OpFract(&ctx.m, ctx.F64[1], value);
}

Id EmitFPFrexpSig32(EmitContext& ctx, Id value) {
    const uint32_t frexp = spv_OpFrexpStruct(&ctx.m, ctx.frexp_result_f32, value);
    return spv_OpCompositeExtract1(&ctx.m, ctx.F32[1], frexp, 0);
}

Id EmitFPFrexpSig64(EmitContext& ctx, Id value) {
    const uint32_t frexp = spv_OpFrexpStruct(&ctx.m, ctx.frexp_result_f64, value);
    return spv_OpCompositeExtract1(&ctx.m, ctx.F64[1], frexp, 0);
}

Id EmitFPFrexpExp32(EmitContext& ctx, Id value) {
    const uint32_t frexp = spv_OpFrexpStruct(&ctx.m, ctx.frexp_result_f32, value);
    return spv_OpBitcast(&ctx.m, ctx.U32[1], spv_OpCompositeExtract1(&ctx.m, ctx.S32[1], frexp, 1));
}

Id EmitFPFrexpExp64(EmitContext& ctx, Id value) {
    const uint32_t frexp = spv_OpFrexpStruct(&ctx.m, ctx.frexp_result_f64, value);
    return spv_OpBitcast(&ctx.m, ctx.U32[1], spv_OpCompositeExtract1(&ctx.m, ctx.S32[1], frexp, 1));
}

Id EmitFPOrdEqual16(EmitContext& ctx, Id lhs, Id rhs) {
    return spv_OpFOrdEqual(&ctx.m, ctx.U1[1], lhs, rhs);
}

Id EmitFPOrdEqual32(EmitContext& ctx, Id lhs, Id rhs) {
    return spv_OpFOrdEqual(&ctx.m, ctx.U1[1], lhs, rhs);
}

Id EmitFPOrdEqual64(EmitContext& ctx, Id lhs, Id rhs) {
    return spv_OpFOrdEqual(&ctx.m, ctx.U1[1], lhs, rhs);
}

Id EmitFPUnordEqual16(EmitContext& ctx, Id lhs, Id rhs) {
    return spv_OpFUnordEqual(&ctx.m, ctx.U1[1], lhs, rhs);
}

Id EmitFPUnordEqual32(EmitContext& ctx, Id lhs, Id rhs) {
    return spv_OpFUnordEqual(&ctx.m, ctx.U1[1], lhs, rhs);
}

Id EmitFPUnordEqual64(EmitContext& ctx, Id lhs, Id rhs) {
    return spv_OpFUnordEqual(&ctx.m, ctx.U1[1], lhs, rhs);
}

Id EmitFPOrdNotEqual16(EmitContext& ctx, Id lhs, Id rhs) {
    return spv_OpFOrdNotEqual(&ctx.m, ctx.U1[1], lhs, rhs);
}

Id EmitFPOrdNotEqual32(EmitContext& ctx, Id lhs, Id rhs) {
    return spv_OpFOrdNotEqual(&ctx.m, ctx.U1[1], lhs, rhs);
}

Id EmitFPOrdNotEqual64(EmitContext& ctx, Id lhs, Id rhs) {
    return spv_OpFOrdNotEqual(&ctx.m, ctx.U1[1], lhs, rhs);
}

Id EmitFPUnordNotEqual16(EmitContext& ctx, Id lhs, Id rhs) {
    return spv_OpFUnordNotEqual(&ctx.m, ctx.U1[1], lhs, rhs);
}

Id EmitFPUnordNotEqual32(EmitContext& ctx, Id lhs, Id rhs) {
    return spv_OpFUnordNotEqual(&ctx.m, ctx.U1[1], lhs, rhs);
}

Id EmitFPUnordNotEqual64(EmitContext& ctx, Id lhs, Id rhs) {
    return spv_OpFUnordNotEqual(&ctx.m, ctx.U1[1], lhs, rhs);
}

Id EmitFPOrdLessThan16(EmitContext& ctx, Id lhs, Id rhs) {
    return spv_OpFOrdLessThan(&ctx.m, ctx.U1[1], lhs, rhs);
}

Id EmitFPOrdLessThan32(EmitContext& ctx, Id lhs, Id rhs) {
    return spv_OpFOrdLessThan(&ctx.m, ctx.U1[1], lhs, rhs);
}

Id EmitFPOrdLessThan64(EmitContext& ctx, Id lhs, Id rhs) {
    return spv_OpFOrdLessThan(&ctx.m, ctx.U1[1], lhs, rhs);
}

Id EmitFPUnordLessThan16(EmitContext& ctx, Id lhs, Id rhs) {
    return spv_OpFUnordLessThan(&ctx.m, ctx.U1[1], lhs, rhs);
}

Id EmitFPUnordLessThan32(EmitContext& ctx, Id lhs, Id rhs) {
    return spv_OpFUnordLessThan(&ctx.m, ctx.U1[1], lhs, rhs);
}

Id EmitFPUnordLessThan64(EmitContext& ctx, Id lhs, Id rhs) {
    return spv_OpFUnordLessThan(&ctx.m, ctx.U1[1], lhs, rhs);
}

Id EmitFPOrdGreaterThan16(EmitContext& ctx, Id lhs, Id rhs) {
    return spv_OpFOrdGreaterThan(&ctx.m, ctx.U1[1], lhs, rhs);
}

Id EmitFPOrdGreaterThan32(EmitContext& ctx, Id lhs, Id rhs) {
    return spv_OpFOrdGreaterThan(&ctx.m, ctx.U1[1], lhs, rhs);
}

Id EmitFPOrdGreaterThan64(EmitContext& ctx, Id lhs, Id rhs) {
    return spv_OpFOrdGreaterThan(&ctx.m, ctx.U1[1], lhs, rhs);
}

Id EmitFPUnordGreaterThan16(EmitContext& ctx, Id lhs, Id rhs) {
    return spv_OpFUnordGreaterThan(&ctx.m, ctx.U1[1], lhs, rhs);
}

Id EmitFPUnordGreaterThan32(EmitContext& ctx, Id lhs, Id rhs) {
    return spv_OpFUnordGreaterThan(&ctx.m, ctx.U1[1], lhs, rhs);
}

Id EmitFPUnordGreaterThan64(EmitContext& ctx, Id lhs, Id rhs) {
    return spv_OpFUnordGreaterThan(&ctx.m, ctx.U1[1], lhs, rhs);
}

Id EmitFPOrdLessThanEqual16(EmitContext& ctx, Id lhs, Id rhs) {
    return spv_OpFOrdLessThanEqual(&ctx.m, ctx.U1[1], lhs, rhs);
}

Id EmitFPOrdLessThanEqual32(EmitContext& ctx, Id lhs, Id rhs) {
    return spv_OpFOrdLessThanEqual(&ctx.m, ctx.U1[1], lhs, rhs);
}

Id EmitFPOrdLessThanEqual64(EmitContext& ctx, Id lhs, Id rhs) {
    return spv_OpFOrdLessThanEqual(&ctx.m, ctx.U1[1], lhs, rhs);
}

Id EmitFPUnordLessThanEqual16(EmitContext& ctx, Id lhs, Id rhs) {
    return spv_OpFUnordLessThanEqual(&ctx.m, ctx.U1[1], lhs, rhs);
}

Id EmitFPUnordLessThanEqual32(EmitContext& ctx, Id lhs, Id rhs) {
    return spv_OpFUnordLessThanEqual(&ctx.m, ctx.U1[1], lhs, rhs);
}

Id EmitFPUnordLessThanEqual64(EmitContext& ctx, Id lhs, Id rhs) {
    return spv_OpFUnordLessThanEqual(&ctx.m, ctx.U1[1], lhs, rhs);
}

Id EmitFPOrdGreaterThanEqual16(EmitContext& ctx, Id lhs, Id rhs) {
    return spv_OpFOrdGreaterThanEqual(&ctx.m, ctx.U1[1], lhs, rhs);
}

Id EmitFPOrdGreaterThanEqual32(EmitContext& ctx, Id lhs, Id rhs) {
    return spv_OpFOrdGreaterThanEqual(&ctx.m, ctx.U1[1], lhs, rhs);
}

Id EmitFPOrdGreaterThanEqual64(EmitContext& ctx, Id lhs, Id rhs) {
    return spv_OpFOrdGreaterThanEqual(&ctx.m, ctx.U1[1], lhs, rhs);
}

Id EmitFPUnordGreaterThanEqual16(EmitContext& ctx, Id lhs, Id rhs) {
    return spv_OpFUnordGreaterThanEqual(&ctx.m, ctx.U1[1], lhs, rhs);
}

Id EmitFPUnordGreaterThanEqual32(EmitContext& ctx, Id lhs, Id rhs) {
    return spv_OpFUnordGreaterThanEqual(&ctx.m, ctx.U1[1], lhs, rhs);
}

Id EmitFPUnordGreaterThanEqual64(EmitContext& ctx, Id lhs, Id rhs) {
    return spv_OpFUnordGreaterThanEqual(&ctx.m, ctx.U1[1], lhs, rhs);
}

Id EmitFPIsNan16(EmitContext& ctx, Id value) {
    return spv_OpIsNan(&ctx.m, ctx.U1[1], value);
}

Id EmitFPIsNan32(EmitContext& ctx, Id value) {
    return spv_OpIsNan(&ctx.m, ctx.U1[1], value);
}

Id EmitFPIsNan64(EmitContext& ctx, Id value) {
    return spv_OpIsNan(&ctx.m, ctx.U1[1], value);
}

Id EmitFPIsInf32(EmitContext& ctx, Id value) {
    return spv_OpIsInf(&ctx.m, ctx.U1[1], value);
}

Id EmitFPIsInf64(EmitContext& ctx, Id value) {
    return spv_OpIsInf(&ctx.m, ctx.U1[1], value);
}

void EmitFPCmpClass32(EmitContext&) {
    UNREACHABLE();
}

} // namespace Shader::Backend::SPIRV
