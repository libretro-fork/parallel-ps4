// SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "shader_recompiler/backend/spirv/emit_spirv_instructions.h"
#include "shader_recompiler/backend/spirv/spirv_emit_context.h"

namespace Shader::Backend::SPIRV {
namespace {
Id ExtractU16(EmitContext& ctx, Id value) {
    return spv_OpUConvert(&ctx.m, ctx.U16, value);
}

Id ExtractS16(EmitContext& ctx, Id value) {
    return spv_OpSConvert(&ctx.m, ctx.S16, value);
}

Id ExtractU8(EmitContext& ctx, Id value) {
    return spv_OpUConvert(&ctx.m, ctx.U8, value);
}

Id ExtractS8(EmitContext& ctx, Id value) {
    return spv_OpSConvert(&ctx.m, ctx.S8, value);
}
} // Anonymous namespace

Id EmitConvertS16F32(EmitContext& ctx, Id value) {
    return spv_OpSConvert(&ctx.m, ctx.U32[1], spv_OpConvertFToS(&ctx.m, ctx.U16, value));
}

Id EmitConvertS16F64(EmitContext& ctx, Id value) {
    return spv_OpSConvert(&ctx.m, ctx.U32[1], spv_OpConvertFToS(&ctx.m, ctx.U16, value));
}

Id EmitConvertS32F32(EmitContext& ctx, Id value) {
    return spv_OpConvertFToS(&ctx.m, ctx.U32[1], value);
}

Id EmitConvertS32F64(EmitContext& ctx, Id value) {
    return spv_OpConvertFToS(&ctx.m, ctx.U32[1], value);
}

Id EmitConvertS64F32(EmitContext& ctx, Id value) {
    return spv_OpConvertFToS(&ctx.m, ctx.U64, value);
}

Id EmitConvertS64F64(EmitContext& ctx, Id value) {
    return spv_OpConvertFToS(&ctx.m, ctx.U64, value);
}

Id EmitConvertU16F32(EmitContext& ctx, Id value) {
    return spv_OpUConvert(&ctx.m, ctx.U32[1], spv_OpConvertFToU(&ctx.m, ctx.U16, value));
}

Id EmitConvertU16F64(EmitContext& ctx, Id value) {
    return spv_OpUConvert(&ctx.m, ctx.U32[1], spv_OpConvertFToU(&ctx.m, ctx.U16, value));
}

Id EmitConvertU32F32(EmitContext& ctx, Id value) {
    return spv_OpConvertFToU(&ctx.m, ctx.U32[1], value);
}

Id EmitConvertU32F64(EmitContext& ctx, Id value) {
    return spv_OpConvertFToU(&ctx.m, ctx.U32[1], value);
}

Id EmitConvertU64F32(EmitContext& ctx, Id value) {
    return spv_OpConvertFToU(&ctx.m, ctx.U64, value);
}

Id EmitConvertU64F64(EmitContext& ctx, Id value) {
    return spv_OpConvertFToU(&ctx.m, ctx.U64, value);
}

Id EmitConvertU64U32(EmitContext& ctx, Id value) {
    return spv_OpUConvert(&ctx.m, ctx.U64, value);
}

Id EmitConvertU32U64(EmitContext& ctx, Id value) {
    return spv_OpUConvert(&ctx.m, ctx.U32[1], value);
}

Id EmitConvertF16F32(EmitContext& ctx, Id value) {
    return spv_OpFConvert(&ctx.m, ctx.F16[1], value);
}

Id EmitConvertF32F16(EmitContext& ctx, Id value) {
    return spv_OpFConvert(&ctx.m, ctx.F32[1], value);
}

Id EmitConvertF32F64(EmitContext& ctx, Id value) {
    return spv_OpFConvert(&ctx.m, ctx.F32[1], value);
}

Id EmitConvertF64F32(EmitContext& ctx, Id value) {
    return spv_OpFConvert(&ctx.m, ctx.F64[1], value);
}

Id EmitConvertF32S8(EmitContext& ctx, Id value) {
    return spv_OpConvertSToF(&ctx.m, ctx.F32[1], ExtractS8(ctx, value));
}

Id EmitConvertF32S16(EmitContext& ctx, Id value) {
    return spv_OpConvertSToF(&ctx.m, ctx.F32[1], ExtractS16(ctx, value));
}

Id EmitConvertF32S32(EmitContext& ctx, Id value) {
    return spv_OpConvertSToF(&ctx.m, ctx.F32[1], value);
}

Id EmitConvertF32S64(EmitContext& ctx, Id value) {
    return spv_OpConvertSToF(&ctx.m, ctx.F32[1], value);
}

Id EmitConvertF32U8(EmitContext& ctx, Id value) {
    return spv_OpConvertUToF(&ctx.m, ctx.F32[1], ExtractU8(ctx, value));
}

Id EmitConvertF32U16(EmitContext& ctx, Id value) {
    return spv_OpConvertUToF(&ctx.m, ctx.F32[1], ExtractU16(ctx, value));
}

Id EmitConvertF32U32(EmitContext& ctx, Id value) {
    return spv_OpConvertUToF(&ctx.m, ctx.F32[1], value);
}

Id EmitConvertF32U64(EmitContext& ctx, Id value) {
    return spv_OpConvertUToF(&ctx.m, ctx.F32[1], value);
}

Id EmitConvertF64S8(EmitContext& ctx, Id value) {
    return spv_OpConvertSToF(&ctx.m, ctx.F64[1], ExtractS8(ctx, value));
}

Id EmitConvertF64S16(EmitContext& ctx, Id value) {
    return spv_OpConvertSToF(&ctx.m, ctx.F64[1], ExtractS16(ctx, value));
}

Id EmitConvertF64S32(EmitContext& ctx, Id value) {
    return spv_OpConvertSToF(&ctx.m, ctx.F64[1], value);
}

Id EmitConvertF64S64(EmitContext& ctx, Id value) {
    return spv_OpConvertSToF(&ctx.m, ctx.F64[1], value);
}

Id EmitConvertF64U8(EmitContext& ctx, Id value) {
    return spv_OpConvertUToF(&ctx.m, ctx.F64[1], ExtractU8(ctx, value));
}

Id EmitConvertF64U16(EmitContext& ctx, Id value) {
    return spv_OpConvertUToF(&ctx.m, ctx.F64[1], ExtractU16(ctx, value));
}

Id EmitConvertF64U32(EmitContext& ctx, Id value) {
    return spv_OpConvertUToF(&ctx.m, ctx.F64[1], value);
}

Id EmitConvertF64U64(EmitContext& ctx, Id value) {
    return spv_OpConvertUToF(&ctx.m, ctx.F64[1], value);
}

Id EmitConvertU16U32(EmitContext& ctx, Id value) {
    return spv_OpUConvert(&ctx.m, ctx.U16, value);
}

Id EmitConvertU32U16(EmitContext& ctx, Id value) {
    return spv_OpUConvert(&ctx.m, ctx.U32[1], value);
}

Id EmitConvertU8U32(EmitContext& ctx, Id value) {
    return spv_OpUConvert(&ctx.m, ctx.U8, value);
}

Id EmitConvertU32U8(EmitContext& ctx, Id value) {
    return spv_OpUConvert(&ctx.m, ctx.U32[1], value);
}

Id EmitConvertS32S8(EmitContext& ctx, Id value) {
    return spv_OpSConvert(&ctx.m, ctx.U32[1], value);
}

Id EmitConvertS32S16(EmitContext& ctx, Id value) {
    return spv_OpSConvert(&ctx.m, ctx.U32[1], value);
}

} // namespace Shader::Backend::SPIRV
