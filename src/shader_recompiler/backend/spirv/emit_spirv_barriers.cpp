// SPDX-FileCopyrightText: Copyright 2021 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "shader_recompiler/backend/spirv/emit_spirv_instructions.h"
#include "shader_recompiler/backend/spirv/spirv_emit_context.h"

namespace Shader::Backend::SPIRV {
namespace {
void MemoryBarrier(EmitContext& ctx, u32 scope) {
    const int semantics{SpvMemorySemanticsAcquireReleaseMask | SpvMemorySemanticsUniformMemoryMask |
                        SpvMemorySemanticsWorkgroupMemoryMask |
                        SpvMemorySemanticsAtomicCounterMemoryMask |
                        SpvMemorySemanticsImageMemoryMask};
    spv_OpMemoryBarrier(&ctx.m, ctx.ConstU32(static_cast<u32>(scope)),
                        ctx.ConstU32(static_cast<u32>(semantics)));
}
} // Anonymous namespace

void EmitBarrier(EmitContext& ctx) {
    const SpvScope_ execution{SpvScopeWorkgroup};
    u32 memory;
    u32 memory_semantics;
    if (ctx.sw_stage == Shader::SwStage::TessellationControl) {
        memory = SpvScopeInvocation;
        memory_semantics = SpvMemorySemanticsMaskNone;
    } else {
        memory = SpvScopeWorkgroup;
        memory_semantics =
            SpvMemorySemanticsAcquireReleaseMask | SpvMemorySemanticsWorkgroupMemoryMask;
    }
    spv_OpControlBarrier(&ctx.m, ctx.ConstU32(static_cast<u32>(execution)),
                         ctx.ConstU32(static_cast<u32>(memory)),
                         ctx.ConstU32(static_cast<u32>(memory_semantics)));
}

void EmitSubgroupBarrier(EmitContext& ctx) {
    const u32 semantics{static_cast<u32>(SpvMemorySemanticsAcquireReleaseMask |
                                         SpvMemorySemanticsWorkgroupMemoryMask)};
    spv_OpMemoryBarrier(&ctx.m, ctx.ConstU32(static_cast<u32>(SpvScopeSubgroup)),
                        ctx.ConstU32(semantics));
}

void EmitWorkgroupMemoryBarrier(EmitContext& ctx) {
    MemoryBarrier(ctx, SpvScopeWorkgroup);
}

void EmitDeviceMemoryBarrier(EmitContext& ctx) {
    MemoryBarrier(ctx, SpvScopeDevice);
}

} // namespace Shader::Backend::SPIRV
