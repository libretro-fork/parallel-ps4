// SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "shader_recompiler/backend/spirv/emit_spirv_instructions.h"
#include "shader_recompiler/backend/spirv/spirv_emit_context.h"

namespace Shader::Backend::SPIRV {

struct R {
    R(u32 a, u32 b) : offset(a), size(b) {}
    u32 offset;
    u32 size;
};
template <bool is_signed, typename... Args>
static std::array<Id, sizeof...(Args)> ExtractBitFields(EmitContext& ctx, const Id value,
                                                        const Args... args) {
    const auto op_func = is_signed ? &spv_OpBitFieldSExtract : &spv_OpBitFieldUExtract;
    std::array<Id, sizeof...(Args)> result{};
    u32 i = 0;
    (
        [&] {
            result[i++] = op_func(&ctx.m, ctx.U32[1], value, ctx.ConstU32(args.offset),
                                  ctx.ConstU32(args.size));
        }(),
        ...);
    return result;
}

template <typename... Args>
static Id InsertBitFields(EmitContext& ctx, const std::initializer_list<Id> values,
                          const Args... args) {
    Id result{};
    auto it = values.begin();
    (
        [&] {
            if (it == values.begin()) {
                result = *it;
            } else {
                result = spv_OpBitFieldInsert(&ctx.m, ctx.U32[1], result, *it,
                                              ctx.ConstU32(args.offset), ctx.ConstU32(args.size));
            }
            ++it;
        }(),
        ...);
    return result;
}

template <u32 num_components>
static std::array<Id, num_components> ExtractComposite(EmitContext& ctx, const VectorIds type,
                                                       const Id value) {
    std::array<Id, num_components> result{};
    for (u32 i = 0; i < num_components; i++) {
        result[i] = spv_OpCompositeExtract1(&ctx.m, type[1], value, u32(i));
    }
    return result;
}

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
    const auto unpacked{spv_OpBitcast(&ctx.m, ctx.U32[2], value)};
    const auto [x, y] = ExtractComposite<2>(ctx, ctx.U32, unpacked);
    return InsertBitFields(ctx, {x, y}, R(0, 16), R(16, 16));
}

Id EmitUnpackUint2x16(EmitContext& ctx, Id value) {
    const auto [x, y] = ExtractBitFields<false>(ctx, value, R(0, 16), R(16, 16));
    const auto unpacked{spv_OpCompositeConstruct2(&ctx.m, ctx.U32[2], x, y)};
    return spv_OpBitcast(&ctx.m, ctx.F32[2], unpacked);
}

Id EmitPackSint2x16(EmitContext& ctx, Id value) {
    return EmitPackUint2x16(ctx, value);
}

Id EmitUnpackSint2x16(EmitContext& ctx, Id value) {
    const auto [x, y] = ExtractBitFields<true>(ctx, value, R(0, 16), R(16, 16));
    const auto unpacked{spv_OpCompositeConstruct2(&ctx.m, ctx.U32[2], x, y)};
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
    const auto unpacked{spv_OpBitcast(&ctx.m, ctx.U32[4], value)};
    const auto [x, y, z, w] = ExtractComposite<4>(ctx, ctx.U32, unpacked);
    return InsertBitFields(ctx, {x, y, z, w}, R(0, 8), R(8, 8), R(16, 8), R(24, 8));
}

Id EmitUnpackUint4x8(EmitContext& ctx, Id value) {
    const auto [x, y, z, w] =
        ExtractBitFields<false>(ctx, value, R(0, 8), R(8, 8), R(16, 8), R(24, 8));
    const auto unpacked{spv_OpCompositeConstruct4(&ctx.m, ctx.U32[4], x, y, z, w)};
    return spv_OpBitcast(&ctx.m, ctx.F32[4], unpacked);
}

Id EmitPackSint4x8(EmitContext& ctx, Id value) {
    return EmitPackUint4x8(ctx, value);
}

Id EmitUnpackSint4x8(EmitContext& ctx, Id value) {
    const auto [x, y, z, w] =
        ExtractBitFields<true>(ctx, value, R(0, 8), R(8, 8), R(16, 8), R(24, 8));
    const auto unpacked{spv_OpCompositeConstruct4(&ctx.m, ctx.U32[4], x, y, z, w)};
    return spv_OpBitcast(&ctx.m, ctx.F32[4], unpacked);
}

Id EmitPackUfloat10_11_11(EmitContext& ctx, Id value) {
    const auto [x, y, z] = ExtractComposite<3>(ctx, ctx.F32, value);
    const auto cvt_x{
        spv_OpFunctionCall(&ctx.m, ctx.U32[1], ctx.f32_to_uf11, std::array<u32, 1>{x}.data(), 1)};
    const auto cvt_y{
        spv_OpFunctionCall(&ctx.m, ctx.U32[1], ctx.f32_to_uf11, std::array<u32, 1>{y}.data(), 1)};
    const auto cvt_z{
        spv_OpFunctionCall(&ctx.m, ctx.U32[1], ctx.f32_to_uf10, std::array<u32, 1>{z}.data(), 1)};
    return InsertBitFields(ctx, {cvt_x, cvt_y, cvt_z}, R(0, 11), R(11, 11), R(22, 10));
}

Id EmitUnpackUfloat10_11_11(EmitContext& ctx, Id value) {
    const auto [x, y, z] = ExtractBitFields<false>(ctx, value, R(0, 11), R(11, 11), R(22, 10));
    const auto cvt_x{
        spv_OpFunctionCall(&ctx.m, ctx.F32[1], ctx.uf11_to_f32, std::array<u32, 1>{x}.data(), 1)};
    const auto cvt_y{
        spv_OpFunctionCall(&ctx.m, ctx.F32[1], ctx.uf11_to_f32, std::array<u32, 1>{y}.data(), 1)};
    const auto cvt_z{
        spv_OpFunctionCall(&ctx.m, ctx.F32[1], ctx.uf10_to_f32, std::array<u32, 1>{z}.data(), 1)};
    return spv_OpCompositeConstruct3(&ctx.m, ctx.F32[3], cvt_x, cvt_y, cvt_z);
}

Id EmitPackUnorm2_10_10_10(EmitContext& ctx, Id value) {
    const auto unorm_min{
        spv_ConstantComposite(&ctx.m, ctx.F32[4],
                              std::array<u32, 4>{ctx.ConstF32(0.f), ctx.ConstF32(0.f),
                                                 ctx.ConstF32(0.f), ctx.ConstF32(0.f)}
                                  .data(),
                              4)};
    const auto unorm_max{
        spv_ConstantComposite(&ctx.m, ctx.F32[4],
                              std::array<u32, 4>{ctx.ConstF32(1.f), ctx.ConstF32(1.f),
                                                 ctx.ConstF32(1.f), ctx.ConstF32(1.f)}
                                  .data(),
                              4)};
    const auto clamped{spv_OpFClamp(&ctx.m, ctx.F32[4], value, unorm_min, unorm_max)};
    const auto unorm_mul{
        spv_ConstantComposite(&ctx.m, ctx.F32[4],
                              std::array<u32, 4>{ctx.ConstF32(1023.f), ctx.ConstF32(1023.f),
                                                 ctx.ConstF32(1023.f), ctx.ConstF32(3.f)}
                                  .data(),
                              4)};
    const auto as_float{spv_OpFMul(&ctx.m, ctx.F32[4], clamped, unorm_mul)};
    const auto as_uint{
        spv_OpConvertFToU(&ctx.m, ctx.U32[4], spv_OpRoundEven(&ctx.m, ctx.F32[4], as_float))};
    return EmitPackUint2_10_10_10(ctx, spv_OpBitcast(&ctx.m, ctx.F32[4], as_uint));
}

Id EmitUnpackUnorm2_10_10_10(EmitContext& ctx, Id value) {
    const auto unpacked{spv_OpBitcast(&ctx.m, ctx.U32[4], EmitUnpackUint2_10_10_10(ctx, value))};
    const auto as_float{spv_OpConvertUToF(&ctx.m, ctx.F32[4], unpacked)};
    const auto unorm_div{
        spv_ConstantComposite(&ctx.m, ctx.F32[4],
                              std::array<u32, 4>{ctx.ConstF32(1023.f), ctx.ConstF32(1023.f),
                                                 ctx.ConstF32(1023.f), ctx.ConstF32(3.f)}
                                  .data(),
                              4)};
    return spv_OpFDiv(&ctx.m, ctx.F32[4], as_float, unorm_div);
}

Id EmitPackSnorm2_10_10_10(EmitContext& ctx, Id value) {
    const auto snorm_min{
        spv_ConstantComposite(&ctx.m, ctx.F32[4],
                              std::array<u32, 4>{ctx.ConstF32(-1.f), ctx.ConstF32(-1.f),
                                                 ctx.ConstF32(-1.f), ctx.ConstF32(-1.f)}
                                  .data(),
                              4)};
    const auto snorm_max{
        spv_ConstantComposite(&ctx.m, ctx.F32[4],
                              std::array<u32, 4>{ctx.ConstF32(1.f), ctx.ConstF32(1.f),
                                                 ctx.ConstF32(1.f), ctx.ConstF32(1.f)}
                                  .data(),
                              4)};
    const auto clamped{spv_OpFClamp(&ctx.m, ctx.F32[4], value, snorm_min, snorm_max)};
    const auto snorm_mul{
        spv_ConstantComposite(&ctx.m, ctx.F32[4],
                              std::array<u32, 4>{ctx.ConstF32(511.f), ctx.ConstF32(511.f),
                                                 ctx.ConstF32(511.f), ctx.ConstF32(1.f)}
                                  .data(),
                              4)};
    const auto as_float{spv_OpFMul(&ctx.m, ctx.F32[4], clamped, snorm_mul)};
    const auto as_sint{
        spv_OpConvertFToS(&ctx.m, ctx.U32[4], spv_OpRoundEven(&ctx.m, ctx.F32[4], as_float))};
    return EmitPackSint2_10_10_10(ctx, spv_OpBitcast(&ctx.m, ctx.F32[4], as_sint));
}

Id EmitUnpackSnorm2_10_10_10(EmitContext& ctx, Id value) {
    const auto unpacked{spv_OpBitcast(&ctx.m, ctx.U32[4], EmitUnpackSint2_10_10_10(ctx, value))};
    const auto as_float{spv_OpConvertSToF(&ctx.m, ctx.F32[4], unpacked)};
    const auto snorm_div{
        spv_ConstantComposite(&ctx.m, ctx.F32[4],
                              std::array<u32, 4>{ctx.ConstF32(511.f), ctx.ConstF32(511.f),
                                                 ctx.ConstF32(511.f), ctx.ConstF32(1.f)}
                                  .data(),
                              4)};
    return spv_OpFDiv(&ctx.m, ctx.F32[4], as_float, snorm_div);
}

Id EmitPackUint2_10_10_10(EmitContext& ctx, Id value) {
    const auto unpacked{spv_OpBitcast(&ctx.m, ctx.U32[4], value)};
    const auto [x, y, z, w] = ExtractComposite<4>(ctx, ctx.U32, unpacked);
    return InsertBitFields(ctx, {x, y, z, w}, R(0, 10), R(10, 10), R(20, 10), R(30, 2));
}

Id EmitUnpackUint2_10_10_10(EmitContext& ctx, Id value) {
    const auto [x, y, z, w] =
        ExtractBitFields<false>(ctx, value, R(0, 10), R(10, 10), R(20, 10), R(30, 2));
    const auto unpacked{spv_OpCompositeConstruct4(&ctx.m, ctx.U32[4], x, y, z, w)};
    return spv_OpBitcast(&ctx.m, ctx.F32[4], unpacked);
}

Id EmitPackSint2_10_10_10(EmitContext& ctx, Id value) {
    return EmitPackUint2_10_10_10(ctx, value);
}

Id EmitUnpackSint2_10_10_10(EmitContext& ctx, Id value) {
    const auto [x, y, z, w] =
        ExtractBitFields<true>(ctx, value, R(0, 10), R(10, 10), R(20, 10), R(30, 2));
    const auto unpacked{spv_OpCompositeConstruct4(&ctx.m, ctx.U32[4], x, y, z, w)};
    return spv_OpBitcast(&ctx.m, ctx.F32[4], unpacked);
}

} // namespace Shader::Backend::SPIRV
