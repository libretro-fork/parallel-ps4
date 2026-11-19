// SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "shader_recompiler/backend/spirv/emit_spirv_instructions.h"
#include "shader_recompiler/backend/spirv/spirv_emit_context.h"

namespace Shader::Backend::SPIRV {
namespace {
void SetZeroFlag(EmitContext& ctx, IR::Inst* inst, Id result) {
    // IR::Inst* const zero{inst->GetAssociatedPseudoOperation(IR::Opcode::GetZeroFromOp)};
    // if (!zero) {
    //     return;
    // }
    // zero->SetDefinition(spv_OpIEqual(&ctx.m, ctx.U1[1], result, ctx.u32_zero_value));
    // zero->Invalidate();
}

void SetSignFlag(EmitContext& ctx, IR::Inst* inst, Id result) {
    // IR::Inst* const sign{inst->GetAssociatedPseudoOperation(IR::Opcode::GetSignFromOp)};
    // if (!sign) {
    //     return;
    // }
    // sign->SetDefinition(spv_OpSLessThan(&ctx.m, ctx.U1[1], result, ctx.u32_zero_value));
    // sign->Invalidate();
}
} // Anonymous namespace

Id EmitIAdd32(EmitContext& ctx, IR::Inst* inst, Id a, Id b) {
    return spv_OpIAdd(&ctx.m, ctx.U32[1], a, b);
    // Id result{};
    // if (IR::Inst* const carry{inst->GetAssociatedPseudoOperation(IR::Opcode::GetCarryFromOp)}) {
    //     const Id carry_type{spv_TypeStruct(&ctx.m, std::array<u32, 2>{ctx.U32[1],
    //     ctx.U32[1]}.data(), 2)}; const Id carry_result{spv_OpIAddCarry(&ctx.m, carry_type, a,
    //     b)}; result = spv_OpCompositeExtract1(&ctx.m, ctx.U32[1], carry_result, 0U);

    //    const Id carry_value{spv_OpCompositeExtract1(&ctx.m, ctx.U32[1], carry_result, 1U)};
    //    carry->SetDefinition(spv_OpINotEqual(&ctx.m, ctx.U1[1][1], carry_value,
    //    ctx.u32_zero_value)); carry->Invalidate();
    //} else {
    //    result = spv_OpIAdd(&ctx.m, ctx.U32[1], a, b);
    //}
    // SetZeroFlag(ctx, inst, result);
    // SetSignFlag(ctx, inst, result);
    // if (IR::Inst * overflow{inst->GetAssociatedPseudoOperation(IR::Opcode::GetOverflowFromOp)}) {
    //    // https://stackoverflow.com/questions/55468823/how-to-detect-integer-overflow-in-c
    //    constexpr u32 s32_max{static_cast<u32>(std::numeric_limits<s32>::max())};
    //    const Id is_positive{spv_OpSGreaterThanEqual(&ctx.m, ctx.U1[1], a, ctx.u32_zero_value)};
    //    const Id sub_a{spv_OpISub(&ctx.m, ctx.U32[1], ctx.Const(s32_max), a)};

    //    const Id positive_test{spv_OpSGreaterThan(&ctx.m, ctx.U1[1], b, sub_a)};
    //    const Id negative_test{spv_OpSLessThan(&ctx.m, ctx.U1[1], b, sub_a)};
    //    const Id carry_flag{spv_OpSelect(&ctx.m, ctx.U1[1], is_positive, positive_test,
    //    negative_test)}; overflow->SetDefinition(carry_flag); overflow->Invalidate();
    //}
    // return result;
}

Id EmitIAdd64(EmitContext& ctx, Id a, Id b) {
    return spv_OpIAdd(&ctx.m, ctx.U64, a, b);
}

Id EmitISub32(EmitContext& ctx, Id a, Id b) {
    return spv_OpISub(&ctx.m, ctx.U32[1], a, b);
}

Id EmitISub64(EmitContext& ctx, Id a, Id b) {
    return spv_OpISub(&ctx.m, ctx.U64, a, b);
}

Id EmitSMulHi(EmitContext& ctx, Id a, Id b) {
    const auto signed_a{spv_OpBitcast(&ctx.m, ctx.S32[1], a)};
    const auto signed_b{spv_OpBitcast(&ctx.m, ctx.S32[1], b)};
    const auto mul_ext{spv_OpSMulExtended(&ctx.m, ctx.full_result_i32x2, signed_a, signed_b)};
    const auto signed_hi{spv_OpCompositeExtract1(&ctx.m, ctx.S32[1], mul_ext, 1)};
    return spv_OpBitcast(&ctx.m, ctx.U32[1], signed_hi);
}

Id EmitUMulHi(EmitContext& ctx, Id a, Id b) {
    const auto mul_ext{spv_OpUMulExtended(&ctx.m, ctx.full_result_u32x2, a, b)};
    return spv_OpCompositeExtract1(&ctx.m, ctx.U32[1], mul_ext, 1);
}

Id EmitIMul32(EmitContext& ctx, Id a, Id b) {
    return spv_OpIMul(&ctx.m, ctx.U32[1], a, b);
}

Id EmitIMul64(EmitContext& ctx, Id a, Id b) {
    return spv_OpIMul(&ctx.m, ctx.U64, a, b);
}

Id EmitSDiv32(EmitContext& ctx, Id a, Id b) {
    return spv_OpSDiv(&ctx.m, ctx.U32[1], a, b);
}

Id EmitUDiv32(EmitContext& ctx, Id a, Id b) {
    return spv_OpUDiv(&ctx.m, ctx.U32[1], a, b);
}

Id EmitSMod32(EmitContext& ctx, Id a, Id b) {
    return spv_OpSMod(&ctx.m, ctx.U32[1], a, b);
}

Id EmitUMod32(EmitContext& ctx, Id a, Id b) {
    return spv_OpUMod(&ctx.m, ctx.U32[1], a, b);
}

Id EmitINeg32(EmitContext& ctx, Id value) {
    return spv_OpSNegate(&ctx.m, ctx.U32[1], value);
}

Id EmitINeg64(EmitContext& ctx, Id value) {
    return spv_OpSNegate(&ctx.m, ctx.U64, value);
}

Id EmitIAbs32(EmitContext& ctx, Id value) {
    return spv_OpSAbs(&ctx.m, ctx.U32[1], value);
}

Id EmitShiftLeftLogical32(EmitContext& ctx, Id base, Id shift) {
    return spv_OpShiftLeftLogical(&ctx.m, ctx.U32[1], base, shift);
}

Id EmitShiftLeftLogical64(EmitContext& ctx, Id base, Id shift) {
    return spv_OpShiftLeftLogical(&ctx.m, ctx.U64, base, shift);
}

Id EmitShiftRightLogical32(EmitContext& ctx, Id base, Id shift) {
    return spv_OpShiftRightLogical(&ctx.m, ctx.U32[1], base, shift);
}

Id EmitShiftRightLogical64(EmitContext& ctx, Id base, Id shift) {
    return spv_OpShiftRightLogical(&ctx.m, ctx.U64, base, shift);
}

Id EmitShiftRightArithmetic32(EmitContext& ctx, Id base, Id shift) {
    return spv_OpShiftRightArithmetic(&ctx.m, ctx.U32[1], base, shift);
}

Id EmitShiftRightArithmetic64(EmitContext& ctx, Id base, Id shift) {
    return spv_OpShiftRightArithmetic(&ctx.m, ctx.U64, base, shift);
}

Id EmitBitwiseAnd32(EmitContext& ctx, IR::Inst* inst, Id a, Id b) {
    const Id result{spv_OpBitwiseAnd(&ctx.m, ctx.U32[1], a, b)};
    SetZeroFlag(ctx, inst, result);
    SetSignFlag(ctx, inst, result);
    return result;
}

Id EmitBitwiseAnd64(EmitContext& ctx, IR::Inst* inst, Id a, Id b) {
    const Id result{spv_OpBitwiseAnd(&ctx.m, ctx.U64, a, b)};
    SetZeroFlag(ctx, inst, result);
    SetSignFlag(ctx, inst, result);
    return result;
}

Id EmitBitwiseOr32(EmitContext& ctx, IR::Inst* inst, Id a, Id b) {
    const Id result{spv_OpBitwiseOr(&ctx.m, ctx.U32[1], a, b)};
    SetZeroFlag(ctx, inst, result);
    SetSignFlag(ctx, inst, result);
    return result;
}

Id EmitBitwiseOr64(EmitContext& ctx, IR::Inst* inst, Id a, Id b) {
    const Id result{spv_OpBitwiseOr(&ctx.m, ctx.U64, a, b)};
    SetZeroFlag(ctx, inst, result);
    SetSignFlag(ctx, inst, result);
    return result;
}

Id EmitBitwiseXor32(EmitContext& ctx, IR::Inst* inst, Id a, Id b) {
    const Id result{spv_OpBitwiseXor(&ctx.m, ctx.U32[1], a, b)};
    SetZeroFlag(ctx, inst, result);
    SetSignFlag(ctx, inst, result);
    return result;
}

Id EmitBitwiseXor64(EmitContext& ctx, IR::Inst* inst, Id a, Id b) {
    const Id result{spv_OpBitwiseXor(&ctx.m, ctx.U64, a, b)};
    SetZeroFlag(ctx, inst, result);
    SetSignFlag(ctx, inst, result);
    return result;
}

Id EmitBitFieldInsert32(EmitContext& ctx, Id base, Id insert, Id offset, Id count) {
    return spv_OpBitFieldInsert(&ctx.m, ctx.U32[1], base, insert, offset, count);
}

Id EmitBitFieldInsert64(EmitContext& ctx, Id base, Id insert, Id offset, Id count) {
    return spv_OpBitFieldInsert(&ctx.m, ctx.U64, base, insert, offset, count);
}

Id EmitBitFieldSExtract(EmitContext& ctx, IR::Inst* inst, Id base, Id offset, Id count) {
    const Id result{spv_OpBitFieldSExtract(&ctx.m, ctx.U32[1], base, offset, count)};
    SetZeroFlag(ctx, inst, result);
    SetSignFlag(ctx, inst, result);
    return result;
}

Id EmitBitFieldUExtract(EmitContext& ctx, IR::Inst* inst, Id base, Id offset, Id count) {
    const Id result{spv_OpBitFieldUExtract(&ctx.m, ctx.U32[1], base, offset, count)};
    SetZeroFlag(ctx, inst, result);
    SetSignFlag(ctx, inst, result);
    return result;
}

Id EmitBitReverse32(EmitContext& ctx, Id value) {
    return spv_OpBitReverse(&ctx.m, ctx.U32[1], value);
}

Id EmitBitCount32(EmitContext& ctx, Id value) {
    return spv_OpBitCount(&ctx.m, ctx.U32[1], value);
}

Id EmitBitCount64(EmitContext& ctx, Id value) {
    // Vulkan restricts some bitwise operations to 32-bit only, so decompose into
    // two 32-bit values and add the result.
    const Id unpacked{spv_OpBitcast(&ctx.m, ctx.U32[2], value)};
    const Id lo{spv_OpCompositeExtract1(&ctx.m, ctx.U32[1], unpacked, 0U)};
    const Id hi{spv_OpCompositeExtract1(&ctx.m, ctx.U32[1], unpacked, 1U)};
    const Id lo_count{spv_OpBitCount(&ctx.m, ctx.U32[1], lo)};
    const Id hi_count{spv_OpBitCount(&ctx.m, ctx.U32[1], hi)};
    return spv_OpIAdd(&ctx.m, ctx.U32[1], lo_count, hi_count);
}

Id EmitBitwiseNot32(EmitContext& ctx, Id value) {
    return spv_OpNot(&ctx.m, ctx.U32[1], value);
}

Id EmitBitwiseNot64(EmitContext& ctx, Id value) {
    return spv_OpNot(&ctx.m, ctx.U64, value);
}

Id EmitFindSMsb32(EmitContext& ctx, Id value) {
    return spv_OpFindSMsb(&ctx.m, ctx.U32[1], value);
}

Id EmitFindUMsb32(EmitContext& ctx, Id value) {
    return spv_OpFindUMsb(&ctx.m, ctx.U32[1], value);
}

Id EmitFindUMsb64(EmitContext& ctx, Id value) {
    // Vulkan restricts some bitwise operations to 32-bit only, so decompose into
    // two 32-bit values and select the correct result.
    const Id unpacked{spv_OpBitcast(&ctx.m, ctx.U32[2], value)};
    const Id hi{spv_OpCompositeExtract1(&ctx.m, ctx.U32[1], unpacked, 1U)};
    const Id lo{spv_OpCompositeExtract1(&ctx.m, ctx.U32[1], unpacked, 0U)};
    const Id hi_msb{spv_OpFindUMsb(&ctx.m, ctx.U32[1], hi)};
    const Id lo_msb{spv_OpFindUMsb(&ctx.m, ctx.U32[1], lo)};
    const Id found_hi{spv_OpINotEqual(&ctx.m, ctx.U1[1], hi_msb, ctx.ConstU32(u32(-1)))};
    const Id shifted_hi{spv_OpIAdd(&ctx.m, ctx.U32[1], hi_msb, ctx.ConstU32(32u))};
    // value == 0 case is checked in IREmitter
    return spv_OpSelect(&ctx.m, ctx.U32[1], found_hi, shifted_hi, lo_msb);
}

Id EmitFindILsb32(EmitContext& ctx, Id value) {
    return spv_OpFindILsb(&ctx.m, ctx.U32[1], value);
}

Id EmitFindILsb64(EmitContext& ctx, Id value) {
    // Vulkan restricts some bitwise operations to 32-bit only, so decompose into
    // two 32-bit values and select the correct result.
    const Id unpacked{spv_OpBitcast(&ctx.m, ctx.U32[2], value)};
    const Id lo{spv_OpCompositeExtract1(&ctx.m, ctx.U32[1], unpacked, 0U)};
    const Id hi{spv_OpCompositeExtract1(&ctx.m, ctx.U32[1], unpacked, 1U)};
    const Id lo_lsb{spv_OpFindILsb(&ctx.m, ctx.U32[1], lo)};
    const Id hi_lsb{spv_OpFindILsb(&ctx.m, ctx.U32[1], hi)};
    const Id found_lo{spv_OpINotEqual(&ctx.m, ctx.U1[1], lo_lsb, ctx.ConstU32(u32(-1)))};
    return spv_OpSelect(&ctx.m, ctx.U32[1], found_lo, lo_lsb, hi_lsb);
}

Id EmitSMin32(EmitContext& ctx, Id a, Id b) {
    return spv_OpSMin(&ctx.m, ctx.U32[1], a, b);
}

Id EmitUMin32(EmitContext& ctx, Id a, Id b) {
    return spv_OpUMin(&ctx.m, ctx.U32[1], a, b);
}

Id EmitSMax32(EmitContext& ctx, Id a, Id b) {
    return spv_OpSMax(&ctx.m, ctx.U32[1], a, b);
}

Id EmitUMax32(EmitContext& ctx, Id a, Id b) {
    return spv_OpUMax(&ctx.m, ctx.U32[1], a, b);
}

Id EmitSMinTri32(EmitContext& ctx, Id a, Id b, Id c) {
    if (ctx.profile.supports_trinary_minmax) {
        return spv_OpSMin3AMD(&ctx.m, ctx.U32[1], a, b, c);
    }
    return spv_OpSMin(&ctx.m, ctx.U32[1], a, spv_OpSMin(&ctx.m, ctx.U32[1], b, c));
}

Id EmitUMinTri32(EmitContext& ctx, Id a, Id b, Id c) {
    if (ctx.profile.supports_trinary_minmax) {
        return spv_OpUMin3AMD(&ctx.m, ctx.U32[1], a, b, c);
    }
    return spv_OpUMin(&ctx.m, ctx.U32[1], a, spv_OpUMin(&ctx.m, ctx.U32[1], b, c));
}

Id EmitSMaxTri32(EmitContext& ctx, Id a, Id b, Id c) {
    if (ctx.profile.supports_trinary_minmax) {
        return spv_OpSMax3AMD(&ctx.m, ctx.U32[1], a, b, c);
    }
    return spv_OpSMax(&ctx.m, ctx.U32[1], a, spv_OpSMax(&ctx.m, ctx.U32[1], b, c));
}

Id EmitUMaxTri32(EmitContext& ctx, Id a, Id b, Id c) {
    if (ctx.profile.supports_trinary_minmax) {
        return spv_OpUMax3AMD(&ctx.m, ctx.U32[1], a, b, c);
    }
    return spv_OpUMax(&ctx.m, ctx.U32[1], a, spv_OpUMax(&ctx.m, ctx.U32[1], b, c));
}

Id EmitSMedTri32(EmitContext& ctx, Id a, Id b, Id c) {
    if (ctx.profile.supports_trinary_minmax) {
        return spv_OpSMid3AMD(&ctx.m, ctx.U32[1], a, b, c);
    }
    const Id mmx{spv_OpSMin(&ctx.m, ctx.U32[1], spv_OpSMax(&ctx.m, ctx.U32[1], a, b), c)};
    return spv_OpSMax(&ctx.m, ctx.U32[1], spv_OpSMin(&ctx.m, ctx.U32[1], a, b), mmx);
}

Id EmitUMedTri32(EmitContext& ctx, Id a, Id b, Id c) {
    if (ctx.profile.supports_trinary_minmax) {
        return spv_OpUMid3AMD(&ctx.m, ctx.U32[1], a, b, c);
    }
    const Id mmx{spv_OpUMin(&ctx.m, ctx.U32[1], spv_OpUMax(&ctx.m, ctx.U32[1], a, b), c)};
    return spv_OpUMax(&ctx.m, ctx.U32[1], spv_OpUMin(&ctx.m, ctx.U32[1], a, b), mmx);
}

Id EmitSClamp32(EmitContext& ctx, IR::Inst* inst, Id value, Id min, Id max) {
    Id result{};
    if (ctx.profile.has_broken_spirv_clamp) {
        value = spv_OpBitcast(&ctx.m, ctx.S32[1], value);
        min = spv_OpBitcast(&ctx.m, ctx.S32[1], min);
        max = spv_OpBitcast(&ctx.m, ctx.S32[1], max);
        result = spv_OpSMax(&ctx.m, ctx.S32[1], spv_OpSMin(&ctx.m, ctx.S32[1], value, max), min);
        result = spv_OpBitcast(&ctx.m, ctx.U32[1], result);
    } else {
        result = spv_OpSClamp(&ctx.m, ctx.U32[1], value, min, max);
    }
    SetZeroFlag(ctx, inst, result);
    SetSignFlag(ctx, inst, result);
    return result;
}

Id EmitUClamp32(EmitContext& ctx, IR::Inst* inst, Id value, Id min, Id max) {
    Id result{};
    if (ctx.profile.has_broken_spirv_clamp) {
        result = spv_OpUMax(&ctx.m, ctx.U32[1], spv_OpUMin(&ctx.m, ctx.U32[1], value, max), min);
    } else {
        result = spv_OpUClamp(&ctx.m, ctx.U32[1], value, min, max);
    }
    SetZeroFlag(ctx, inst, result);
    SetSignFlag(ctx, inst, result);
    return result;
}

Id EmitSLessThan32(EmitContext& ctx, Id lhs, Id rhs) {
    return spv_OpSLessThan(&ctx.m, ctx.U1[1], lhs, rhs);
}

Id EmitSLessThan64(EmitContext& ctx, Id lhs, Id rhs) {
    return spv_OpSLessThan(&ctx.m, ctx.U1[1], lhs, rhs);
}

Id EmitULessThan32(EmitContext& ctx, Id lhs, Id rhs) {
    return spv_OpULessThan(&ctx.m, ctx.U1[1], lhs, rhs);
}

Id EmitULessThan64(EmitContext& ctx, Id lhs, Id rhs) {
    return spv_OpULessThan(&ctx.m, ctx.U1[1], lhs, rhs);
}

Id EmitIEqual32(EmitContext& ctx, Id lhs, Id rhs) {
    return spv_OpIEqual(&ctx.m, ctx.U1[1], lhs, rhs);
}

Id EmitIEqual64(EmitContext& ctx, Id lhs, Id rhs) {
    return spv_OpIEqual(&ctx.m, ctx.U1[1], lhs, rhs);
}

Id EmitSLessThanEqual32(EmitContext& ctx, Id lhs, Id rhs) {
    return spv_OpSLessThanEqual(&ctx.m, ctx.U1[1], lhs, rhs);
}

Id EmitSLessThanEqual64(EmitContext& ctx, Id lhs, Id rhs) {
    return spv_OpSLessThanEqual(&ctx.m, ctx.U1[1], lhs, rhs);
}

Id EmitULessThanEqual32(EmitContext& ctx, Id lhs, Id rhs) {
    return spv_OpULessThanEqual(&ctx.m, ctx.U1[1], lhs, rhs);
}

Id EmitULessThanEqual64(EmitContext& ctx, Id lhs, Id rhs) {
    return spv_OpULessThanEqual(&ctx.m, ctx.U1[1], lhs, rhs);
}

Id EmitSGreaterThan32(EmitContext& ctx, Id lhs, Id rhs) {
    return spv_OpSGreaterThan(&ctx.m, ctx.U1[1], lhs, rhs);
}

Id EmitSGreaterThan64(EmitContext& ctx, Id lhs, Id rhs) {
    return spv_OpSGreaterThan(&ctx.m, ctx.U1[1], lhs, rhs);
}

Id EmitUGreaterThan32(EmitContext& ctx, Id lhs, Id rhs) {
    return spv_OpUGreaterThan(&ctx.m, ctx.U1[1], lhs, rhs);
}

Id EmitUGreaterThan64(EmitContext& ctx, Id lhs, Id rhs) {
    return spv_OpUGreaterThan(&ctx.m, ctx.U1[1], lhs, rhs);
}

Id EmitINotEqual32(EmitContext& ctx, Id lhs, Id rhs) {
    return spv_OpINotEqual(&ctx.m, ctx.U1[1], lhs, rhs);
}

Id EmitINotEqual64(EmitContext& ctx, Id lhs, Id rhs) {
    return spv_OpINotEqual(&ctx.m, ctx.U1[1], lhs, rhs);
}

Id EmitSGreaterThanEqual32(EmitContext& ctx, Id lhs, Id rhs) {
    return spv_OpSGreaterThanEqual(&ctx.m, ctx.U1[1], lhs, rhs);
}

Id EmitSGreaterThanEqual64(EmitContext& ctx, Id lhs, Id rhs) {
    return spv_OpSGreaterThanEqual(&ctx.m, ctx.U1[1], lhs, rhs);
}

Id EmitUGreaterThanEqual32(EmitContext& ctx, Id lhs, Id rhs) {
    return spv_OpUGreaterThanEqual(&ctx.m, ctx.U1[1], lhs, rhs);
}

Id EmitUGreaterThanEqual64(EmitContext& ctx, Id lhs, Id rhs) {
    return spv_OpUGreaterThanEqual(&ctx.m, ctx.U1[1], lhs, rhs);
}

Id EmitMaskedBitCount32(EmitContext& ctx, Id value, Id addend, bool hi) {
    UNREACHABLE_MSG("Unreachable instruction");
}

} // namespace Shader::Backend::SPIRV
