// SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "shader_recompiler/backend/spirv/emit_spirv_instructions.h"
#include "shader_recompiler/backend/spirv/spirv_emit_context.h"

namespace Shader::Backend::SPIRV {

/// A bit field: @offset and @size in bits
struct R {
    u32 offset;
    u32 size;
};

/// The @n fields @fields of @value, each extracted into a word of its own, into @out
static void ExtractBitFields(EmitContext& ctx, const Id value, bool is_signed, const R* fields,
                             u32 n, Id* out) {
    Id (*const op_func)(spv_module_t*, Id, Id, Id, Id) =
        is_signed ? &spv_OpBitFieldSExtract : &spv_OpBitFieldUExtract;
    for (u32 i = 0; i < n; ++i) {
        out[i] = op_func(&ctx.m, ctx.U32[1], value, ctx.ConstU32(fields[i].offset),
                         ctx.ConstU32(fields[i].size));
    }
}

/// The @n words @values put into the fields @fields of one word, the first as it is
static Id InsertBitFields(EmitContext& ctx, const Id* values, const R* fields, u32 n) {
    Id result = values[0];
    for (u32 i = 1; i < n; ++i) {
        result = spv_OpBitFieldInsert(&ctx.m, ctx.U32[1], result, values[i],
                                      ctx.ConstU32(fields[i].offset), ctx.ConstU32(fields[i].size));
    }
    return result;
}

/// The @n components of the vector @value, of @type, into @out
static void ExtractComposite(EmitContext& ctx, const VectorIds type, const Id value, u32 n,
                             Id* out) {
    for (u32 i = 0; i < n; i++) {
        out[i] = spv_OpCompositeExtract1(&ctx.m, type[1], value, u32(i));
    }
}

/// Four words to hand on as an array
struct Parts4 {
    Id v[4];
};

static constexpr R Fields2x16[2] = {{0, 16}, {16, 16}};
static constexpr R Fields4x8[4] = {{0, 8}, {8, 8}, {16, 8}, {24, 8}};
static constexpr R Fields10_11_11[3] = {{0, 11}, {11, 11}, {22, 10}};
static constexpr R Fields2_10_10_10[4] = {{0, 10}, {10, 10}, {20, 10}, {30, 2}};

Id EmitBitCastU16F16(EmitContext& ctx, Id value) {
    return spv_OpBitcast(&ctx.m, ctx.U16, value);
}

Id EmitBitCastU32F32(EmitContext& ctx, Id value) {
    return spv_OpBitcast(&ctx.m, ctx.U32[1], value);
}

Id EmitBitCastF16U16(EmitContext& ctx, Id value) {
    return spv_OpBitcast(&ctx.m, ctx.F16[1], value);
}

Id EmitBitCastF32U32(EmitContext& ctx, Id value) {
    return spv_OpBitcast(&ctx.m, ctx.F32[1], value);
}

Id EmitPackUint2x32(EmitContext& ctx, Id value) {
    return spv_OpBitcast(&ctx.m, ctx.U64, value);
}

Id EmitUnpackUint2x32(EmitContext& ctx, Id value) {
    return spv_OpBitcast(&ctx.m, ctx.U32[2], value);
}

Id EmitPackDouble2x32(EmitContext& ctx, Id value) {
    return spv_OpBitcast(&ctx.m, ctx.F64[1], value);
}

Id EmitUnpackDouble2x32(EmitContext& ctx, Id value) {
    return spv_OpBitcast(&ctx.m, ctx.U32[2], value);
}

Id EmitPackUnorm2x16(EmitContext& ctx, Id value) {
    return spv_OpPackUnorm2x16(&ctx.m, ctx.U32[1], value);
}

Id EmitUnpackUnorm2x16(EmitContext& ctx, Id value) {
    return spv_OpUnpackUnorm2x16(&ctx.m, ctx.F32[2], value);
}

Id EmitPackSnorm2x16(EmitContext& ctx, Id value) {
    return spv_OpPackSnorm2x16(&ctx.m, ctx.U32[1], value);
}

Id EmitUnpackSnorm2x16(EmitContext& ctx, Id value) {
    return spv_OpUnpackSnorm2x16(&ctx.m, ctx.F32[2], value);
}

Id EmitPackUint2x16(EmitContext& ctx, Id value) {
    const uint32_t unpacked{spv_OpBitcast(&ctx.m, ctx.U32[2], value)};
    Id parts[2];
    ExtractComposite(ctx, ctx.U32, unpacked, 2, parts);
    const Id x = parts[0];
    const Id y = parts[1];
    const Id values[2] = {x, y};
    return InsertBitFields(ctx, values, Fields2x16, 2);
}

Id EmitUnpackUint2x16(EmitContext& ctx, Id value) {
    Id parts[2];
    ExtractBitFields(ctx, value, false, Fields2x16, 2, parts);
    const Id x = parts[0];
    const Id y = parts[1];
    const uint32_t unpacked{spv_OpCompositeConstruct2(&ctx.m, ctx.U32[2], x, y)};
    return spv_OpBitcast(&ctx.m, ctx.F32[2], unpacked);
}

Id EmitPackSint2x16(EmitContext& ctx, Id value) {
    return EmitPackUint2x16(ctx, value);
}

Id EmitUnpackSint2x16(EmitContext& ctx, Id value) {
    Id parts[2];
    ExtractBitFields(ctx, value, true, Fields2x16, 2, parts);
    const Id x = parts[0];
    const Id y = parts[1];
    const uint32_t unpacked{spv_OpCompositeConstruct2(&ctx.m, ctx.U32[2], x, y)};
    return spv_OpBitcast(&ctx.m, ctx.F32[2], unpacked);
}

Id EmitPackHalf2x16(EmitContext& ctx, Id value) {
    return spv_OpPackHalf2x16(&ctx.m, ctx.U32[1], value);
}

Id EmitUnpackHalf2x16(EmitContext& ctx, Id value) {
    return spv_OpUnpackHalf2x16(&ctx.m, ctx.F32[2], value);
}

Id EmitPackUnorm4x8(EmitContext& ctx, Id value) {
    return spv_OpPackUnorm4x8(&ctx.m, ctx.U32[1], value);
}

Id EmitUnpackUnorm4x8(EmitContext& ctx, Id value) {
    return spv_OpUnpackUnorm4x8(&ctx.m, ctx.F32[4], value);
}

Id EmitPackSnorm4x8(EmitContext& ctx, Id value) {
    return spv_OpPackSnorm4x8(&ctx.m, ctx.U32[1], value);
}

Id EmitUnpackSnorm4x8(EmitContext& ctx, Id value) {
    return spv_OpUnpackSnorm4x8(&ctx.m, ctx.F32[4], value);
}

Id EmitPackUint4x8(EmitContext& ctx, Id value) {
    const uint32_t unpacked{spv_OpBitcast(&ctx.m, ctx.U32[4], value)};
    Id parts[4];
    ExtractComposite(ctx, ctx.U32, unpacked, 4, parts);
    const Id x = parts[0];
    const Id y = parts[1];
    const Id z = parts[2];
    const Id w = parts[3];
    const Id values[4] = {x, y, z, w};
    return InsertBitFields(ctx, values, Fields4x8, 4);
}

Id EmitUnpackUint4x8(EmitContext& ctx, Id value) {
    Id parts[4];
    ExtractBitFields(ctx, value, false, Fields4x8, 4, parts);
    const Id x = parts[0];
    const Id y = parts[1];
    const Id z = parts[2];
    const Id w = parts[3];
    const uint32_t unpacked{spv_OpCompositeConstruct4(&ctx.m, ctx.U32[4], x, y, z, w)};
    return spv_OpBitcast(&ctx.m, ctx.F32[4], unpacked);
}

Id EmitPackSint4x8(EmitContext& ctx, Id value) {
    return EmitPackUint4x8(ctx, value);
}

Id EmitUnpackSint4x8(EmitContext& ctx, Id value) {
    Id parts[4];
    ExtractBitFields(ctx, value, true, Fields4x8, 4, parts);
    const Id x = parts[0];
    const Id y = parts[1];
    const Id z = parts[2];
    const Id w = parts[3];
    const uint32_t unpacked{spv_OpCompositeConstruct4(&ctx.m, ctx.U32[4], x, y, z, w)};
    return spv_OpBitcast(&ctx.m, ctx.F32[4], unpacked);
}

Id EmitPackUfloat10_11_11(EmitContext& ctx, Id value) {
    Id parts[3];
    ExtractComposite(ctx, ctx.F32, value, 3, parts);
    const Id x = parts[0];
    const Id y = parts[1];
    const Id z = parts[2];
    const uint32_t cvt_x{spv_OpFunctionCall(&ctx.m, ctx.U32[1], ctx.f32_to_uf11, &x, 1)};
    const uint32_t cvt_y{spv_OpFunctionCall(&ctx.m, ctx.U32[1], ctx.f32_to_uf11, &y, 1)};
    const uint32_t cvt_z{spv_OpFunctionCall(&ctx.m, ctx.U32[1], ctx.f32_to_uf10, &z, 1)};
    const Id values[3] = {cvt_x, cvt_y, cvt_z};
    return InsertBitFields(ctx, values, Fields10_11_11, 3);
}

Id EmitUnpackUfloat10_11_11(EmitContext& ctx, Id value) {
    Id parts[3];
    ExtractBitFields(ctx, value, false, Fields10_11_11, 3, parts);
    const Id x = parts[0];
    const Id y = parts[1];
    const Id z = parts[2];
    const uint32_t cvt_x{spv_OpFunctionCall(&ctx.m, ctx.F32[1], ctx.uf11_to_f32, &x, 1)};
    const uint32_t cvt_y{spv_OpFunctionCall(&ctx.m, ctx.F32[1], ctx.uf11_to_f32, &y, 1)};
    const uint32_t cvt_z{spv_OpFunctionCall(&ctx.m, ctx.F32[1], ctx.uf10_to_f32, &z, 1)};
    return spv_OpCompositeConstruct3(&ctx.m, ctx.F32[3], cvt_x, cvt_y, cvt_z);
}

Id EmitPackUnorm2_10_10_10(EmitContext& ctx, Id value) {
    const uint32_t unorm_min{spv_ConstantComposite(
        &ctx.m, ctx.F32[4],
        Parts4{{ctx.ConstF32(0.f), ctx.ConstF32(0.f), ctx.ConstF32(0.f), ctx.ConstF32(0.f)}}.v, 4)};
    const uint32_t unorm_max{spv_ConstantComposite(
        &ctx.m, ctx.F32[4],
        Parts4{{ctx.ConstF32(1.f), ctx.ConstF32(1.f), ctx.ConstF32(1.f), ctx.ConstF32(1.f)}}.v, 4)};
    const uint32_t clamped{spv_OpFClamp(&ctx.m, ctx.F32[4], value, unorm_min, unorm_max)};
    const uint32_t unorm_mul{
        spv_ConstantComposite(&ctx.m, ctx.F32[4],
                              Parts4{{ctx.ConstF32(1023.f), ctx.ConstF32(1023.f),
                                      ctx.ConstF32(1023.f), ctx.ConstF32(3.f)}}
                                  .v,
                              4)};
    const uint32_t as_float{spv_OpFMul(&ctx.m, ctx.F32[4], clamped, unorm_mul)};
    const uint32_t as_uint{
        spv_OpConvertFToU(&ctx.m, ctx.U32[4], spv_OpRoundEven(&ctx.m, ctx.F32[4], as_float))};
    return EmitPackUint2_10_10_10(ctx, spv_OpBitcast(&ctx.m, ctx.F32[4], as_uint));
}

Id EmitUnpackUnorm2_10_10_10(EmitContext& ctx, Id value) {
    const uint32_t unpacked{
        spv_OpBitcast(&ctx.m, ctx.U32[4], EmitUnpackUint2_10_10_10(ctx, value))};
    const uint32_t as_float{spv_OpConvertUToF(&ctx.m, ctx.F32[4], unpacked)};
    const uint32_t unorm_div{
        spv_ConstantComposite(&ctx.m, ctx.F32[4],
                              Parts4{{ctx.ConstF32(1023.f), ctx.ConstF32(1023.f),
                                      ctx.ConstF32(1023.f), ctx.ConstF32(3.f)}}
                                  .v,
                              4)};
    return spv_OpFDiv(&ctx.m, ctx.F32[4], as_float, unorm_div);
}

Id EmitPackSnorm2_10_10_10(EmitContext& ctx, Id value) {
    const uint32_t snorm_min{spv_ConstantComposite(
        &ctx.m, ctx.F32[4],
        Parts4{{ctx.ConstF32(-1.f), ctx.ConstF32(-1.f), ctx.ConstF32(-1.f), ctx.ConstF32(-1.f)}}.v,
        4)};
    const uint32_t snorm_max{spv_ConstantComposite(
        &ctx.m, ctx.F32[4],
        Parts4{{ctx.ConstF32(1.f), ctx.ConstF32(1.f), ctx.ConstF32(1.f), ctx.ConstF32(1.f)}}.v, 4)};
    const uint32_t clamped{spv_OpFClamp(&ctx.m, ctx.F32[4], value, snorm_min, snorm_max)};
    const uint32_t snorm_mul{spv_ConstantComposite(
        &ctx.m, ctx.F32[4],
        Parts4{{ctx.ConstF32(511.f), ctx.ConstF32(511.f), ctx.ConstF32(511.f), ctx.ConstF32(1.f)}}
            .v,
        4)};
    const uint32_t as_float{spv_OpFMul(&ctx.m, ctx.F32[4], clamped, snorm_mul)};
    const uint32_t as_sint{
        spv_OpConvertFToS(&ctx.m, ctx.U32[4], spv_OpRoundEven(&ctx.m, ctx.F32[4], as_float))};
    return EmitPackSint2_10_10_10(ctx, spv_OpBitcast(&ctx.m, ctx.F32[4], as_sint));
}

Id EmitUnpackSnorm2_10_10_10(EmitContext& ctx, Id value) {
    const uint32_t unpacked{
        spv_OpBitcast(&ctx.m, ctx.U32[4], EmitUnpackSint2_10_10_10(ctx, value))};
    const uint32_t as_float{spv_OpConvertSToF(&ctx.m, ctx.F32[4], unpacked)};
    const uint32_t snorm_div{spv_ConstantComposite(
        &ctx.m, ctx.F32[4],
        Parts4{{ctx.ConstF32(511.f), ctx.ConstF32(511.f), ctx.ConstF32(511.f), ctx.ConstF32(1.f)}}
            .v,
        4)};
    return spv_OpFDiv(&ctx.m, ctx.F32[4], as_float, snorm_div);
}

Id EmitPackUint2_10_10_10(EmitContext& ctx, Id value) {
    const uint32_t unpacked{spv_OpBitcast(&ctx.m, ctx.U32[4], value)};
    Id parts[4];
    ExtractComposite(ctx, ctx.U32, unpacked, 4, parts);
    const Id x = parts[0];
    const Id y = parts[1];
    const Id z = parts[2];
    const Id w = parts[3];
    const Id values[4] = {x, y, z, w};
    return InsertBitFields(ctx, values, Fields2_10_10_10, 4);
}

Id EmitUnpackUint2_10_10_10(EmitContext& ctx, Id value) {
    Id parts[4];
    ExtractBitFields(ctx, value, false, Fields2_10_10_10, 4, parts);
    const Id x = parts[0];
    const Id y = parts[1];
    const Id z = parts[2];
    const Id w = parts[3];
    const uint32_t unpacked{spv_OpCompositeConstruct4(&ctx.m, ctx.U32[4], x, y, z, w)};
    return spv_OpBitcast(&ctx.m, ctx.F32[4], unpacked);
}

Id EmitPackSint2_10_10_10(EmitContext& ctx, Id value) {
    return EmitPackUint2_10_10_10(ctx, value);
}

Id EmitUnpackSint2_10_10_10(EmitContext& ctx, Id value) {
    Id parts[4];
    ExtractBitFields(ctx, value, true, Fields2_10_10_10, 4, parts);
    const Id x = parts[0];
    const Id y = parts[1];
    const Id z = parts[2];
    const Id w = parts[3];
    const uint32_t unpacked{spv_OpCompositeConstruct4(&ctx.m, ctx.U32[4], x, y, z, w)};
    return spv_OpBitcast(&ctx.m, ctx.F32[4], unpacked);
}

} // namespace Shader::Backend::SPIRV
