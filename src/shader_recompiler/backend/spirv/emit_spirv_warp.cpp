// SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "shader_recompiler/backend/spirv/emit_spirv_instructions.h"
#include "shader_recompiler/backend/spirv/spirv_emit_context.h"

namespace Shader::Backend::SPIRV {

Id SubgroupScope(EmitContext& ctx) {
    return ctx.ConstU32(static_cast<u32>(SpvScopeSubgroup));
}

Id EmitWarpId(EmitContext& ctx) {
    UNREACHABLE();
}

Id EmitLaneId(EmitContext& ctx) {
    return spv_OpLoad(&ctx.m, ctx.U32[1], ctx.subgroup_local_invocation_id);
}

Id EmitQuadBroadcast(EmitContext& ctx, Id value, Id index) {
    return spv_OpGroupNonUniformQuadBroadcast(&ctx.m, ctx.U32[1], SubgroupScope(ctx), value, index);
}

Id EmitReadFirstLane(EmitContext& ctx, Id value) {
    return spv_OpGroupNonUniformBroadcastFirst(&ctx.m, ctx.U32[1], SubgroupScope(ctx), value);
}

Id EmitShuffle(EmitContext& ctx, Id value, Id index) {
    return spv_OpGroupNonUniformShuffle(&ctx.m, ctx.U32[1], SubgroupScope(ctx), value, index);
}

Id EmitShuffleXor(EmitContext& ctx, Id value, Id mask) {
    return spv_OpGroupNonUniformShuffleXor(&ctx.m, ctx.U32[1], SubgroupScope(ctx), value, mask);
}

Id EmitReadLane(EmitContext& ctx, Id value, Id lane) {
    return spv_OpGroupNonUniformBroadcast(&ctx.m, ctx.U32[1], SubgroupScope(ctx), value, lane);
}

Id EmitWriteLane(EmitContext& ctx, Id value, Id write_value, Imm32 lane_imm) {
    u32 lane = lane_imm;
    return ctx.u32_zero_value;
}

Id EmitBallot(EmitContext& ctx, Id bit) {
    const Id ballot{spv_OpGroupNonUniformBallot(&ctx.m, ctx.U32[4], SubgroupScope(ctx), bit)};
    return spv_OpBitcast(&ctx.m, ctx.U64,
                         spv_OpVectorShuffle(&ctx.m, ctx.U32[2], ballot, ballot,
                                             std::array<u32, 2>{0, 1}.data(), 2));
}

Id EmitBallotFindLsb(EmitContext& ctx, Id mask) {
    const Id value{spv_OpCompositeConstruct3(&ctx.m, ctx.U32[4],
                                             spv_OpBitcast(&ctx.m, ctx.U32[2], mask),
                                             ctx.u32_zero_value, ctx.u32_zero_value)};
    return spv_OpGroupNonUniformBallotFindLSB(&ctx.m, ctx.U32[1], SubgroupScope(ctx), value);
}

Id EmitInverseBallot(EmitContext& ctx, Id mask) {
    const Id value{spv_OpCompositeConstruct3(&ctx.m, ctx.U32[4],
                                             spv_OpBitcast(&ctx.m, ctx.U32[2], mask),
                                             ctx.u32_zero_value, ctx.u32_zero_value)};
    return spv_OpGroupNonUniformInverseBallot(&ctx.m, ctx.U1[1], SubgroupScope(ctx), value);
}

Id EmitGroupAny(EmitContext& ctx, Id bit) {
    return spv_OpGroupNonUniformAny(&ctx.m, ctx.U1[1], SubgroupScope(ctx), bit);
}

} // namespace Shader::Backend::SPIRV
