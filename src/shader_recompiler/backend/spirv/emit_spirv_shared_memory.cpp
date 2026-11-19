// SPDX-FileCopyrightText: Copyright 2021 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "shader_recompiler/backend/spirv/emit_spirv_instructions.h"
#include "shader_recompiler/backend/spirv/spirv_emit_context.h"

namespace Shader::Backend::SPIRV {

Id EmitLoadSharedU16(EmitContext& ctx, Id offset) {
    const Id shift_id{ctx.ConstU32(1U)};
    const Id index{spv_OpShiftRightLogical(&ctx.m, ctx.U32[1], offset, shift_id)};
    const Id pointer = ctx.EmitSharedMemoryAccess(ctx.shared_u16, ctx.shared_memory_u16, index);
    return spv_OpLoad(&ctx.m, ctx.U16, pointer);
}

Id EmitLoadSharedU32(EmitContext& ctx, Id offset) {
    const Id shift_id{ctx.ConstU32(2U)};
    const Id index{spv_OpShiftRightLogical(&ctx.m, ctx.U32[1], offset, shift_id)};
    const Id pointer = ctx.EmitSharedMemoryAccess(ctx.shared_u32, ctx.shared_memory_u32, index);
    return spv_OpLoad(&ctx.m, ctx.U32[1], pointer);
}

Id EmitLoadSharedU64(EmitContext& ctx, Id offset) {
    const Id shift_id{ctx.ConstU32(3U)};
    const Id index{spv_OpShiftRightLogical(&ctx.m, ctx.U32[1], offset, shift_id)};
    const Id pointer = ctx.EmitSharedMemoryAccess(ctx.shared_u64, ctx.shared_memory_u64, index);
    return spv_OpLoad(&ctx.m, ctx.U64, pointer);
}

void EmitWriteSharedU16(EmitContext& ctx, Id offset, Id value) {
    const Id shift{ctx.ConstU32(1U)};
    const Id index{spv_OpShiftRightLogical(&ctx.m, ctx.U32[1], offset, shift)};
    const Id pointer = ctx.EmitSharedMemoryAccess(ctx.shared_u16, ctx.shared_memory_u16, index);
    spv_OpStore(&ctx.m, pointer, value);
}

void EmitWriteSharedU32(EmitContext& ctx, Id offset, Id value) {
    const Id shift{ctx.ConstU32(2U)};
    const Id index{spv_OpShiftRightLogical(&ctx.m, ctx.U32[1], offset, shift)};
    const Id pointer = ctx.EmitSharedMemoryAccess(ctx.shared_u32, ctx.shared_memory_u32, index);
    spv_OpStore(&ctx.m, pointer, value);
}

void EmitWriteSharedU64(EmitContext& ctx, Id offset, Id value) {
    const Id shift{ctx.ConstU32(3U)};
    const Id index{spv_OpShiftRightLogical(&ctx.m, ctx.U32[1], offset, shift)};
    const Id pointer = ctx.EmitSharedMemoryAccess(ctx.shared_u64, ctx.shared_memory_u64, index);
    spv_OpStore(&ctx.m, pointer, value);
}

} // namespace Shader::Backend::SPIRV
