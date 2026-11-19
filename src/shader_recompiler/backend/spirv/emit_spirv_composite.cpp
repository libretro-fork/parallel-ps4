// SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "shader_recompiler/backend/spirv/emit_spirv_instructions.h"
#include "shader_recompiler/backend/spirv/spirv_emit_context.h"
#include "shader_recompiler/ir/microinstruction.h"

namespace Shader::Backend::SPIRV {

template <typename... Args>
Id EmitCompositeConstruct(EmitContext& ctx, IR::Inst* inst, Id type, Args&&... args) {
    const std::array<Id, sizeof...(Args)> parts{args...};
    return inst->AreAllArgsImmediates()
               ? spv_ConstantComposite(&ctx.m, type, parts.data(), u32(parts.size()))
               : spv_OpCompositeConstruct(&ctx.m, type, parts.data(), u32(parts.size()));
}

Id EmitCompositeConstructU32x2(EmitContext& ctx, IR::Inst* inst, Id e1, Id e2) {
    return EmitCompositeConstruct(ctx, inst, ctx.U32[2], e1, e2);
}

Id EmitCompositeConstructU32x3(EmitContext& ctx, IR::Inst* inst, Id e1, Id e2, Id e3) {
    return EmitCompositeConstruct(ctx, inst, ctx.U32[3], e1, e2, e3);
}

Id EmitCompositeConstructU32x4(EmitContext& ctx, IR::Inst* inst, Id e1, Id e2, Id e3, Id e4) {
    return EmitCompositeConstruct(ctx, inst, ctx.U32[4], e1, e2, e3, e4);
}

Id EmitCompositeConstructU32x2x2(EmitContext& ctx, IR::Inst* inst, Id e1, Id e2) {
    return EmitCompositeConstruct(ctx, inst, ctx.U32[4], e1, e2);
}

Id EmitCompositeExtractU32x2(EmitContext& ctx, Id composite, Imm32 index_imm) {
    u32 index = index_imm;
    return spv_OpCompositeExtract1(&ctx.m, ctx.U32[1], composite, u32(index));
}

Id EmitCompositeExtractU32x3(EmitContext& ctx, Id composite, Imm32 index_imm) {
    u32 index = index_imm;
    return spv_OpCompositeExtract1(&ctx.m, ctx.U32[1], composite, u32(index));
}

Id EmitCompositeExtractU32x4(EmitContext& ctx, Id composite, Imm32 index_imm) {
    u32 index = index_imm;
    return spv_OpCompositeExtract1(&ctx.m, ctx.U32[1], composite, u32(index));
}

Id EmitCompositeInsertU32x2(EmitContext& ctx, Id composite, Id object, Imm32 index_imm) {
    u32 index = index_imm;
    return spv_OpCompositeInsert1(&ctx.m, ctx.U32[2], object, composite, u32(index));
}

Id EmitCompositeInsertU32x3(EmitContext& ctx, Id composite, Id object, Imm32 index_imm) {
    u32 index = index_imm;
    return spv_OpCompositeInsert1(&ctx.m, ctx.U32[3], object, composite, u32(index));
}

Id EmitCompositeInsertU32x4(EmitContext& ctx, Id composite, Id object, Imm32 index_imm) {
    u32 index = index_imm;
    return spv_OpCompositeInsert1(&ctx.m, ctx.U32[4], object, composite, u32(index));
}

Id EmitCompositeShuffleU32x2(EmitContext& ctx, Id composite1, Id composite2, Imm32 comp0_imm,
                             Imm32 comp1_imm) {
    u32 comp0 = comp0_imm;
    u32 comp1 = comp1_imm;
    return spv_OpVectorShuffle(&ctx.m, ctx.U32[2], composite1, composite2,
                               std::array<u32, 2>{u32(comp0), u32(comp1)}.data(), 2);
}

Id EmitCompositeShuffleU32x3(EmitContext& ctx, Id composite1, Id composite2, Imm32 comp0_imm,
                             Imm32 comp1_imm, Imm32 comp2_imm) {
    u32 comp0 = comp0_imm;
    u32 comp1 = comp1_imm;
    u32 comp2 = comp2_imm;
    return spv_OpVectorShuffle(&ctx.m, ctx.U32[3], composite1, composite2,
                               std::array<u32, 3>{u32(comp0), u32(comp1), u32(comp2)}.data(), 3);
}

Id EmitCompositeShuffleU32x4(EmitContext& ctx, Id composite1, Id composite2, Imm32 comp0_imm,
                             Imm32 comp1_imm, Imm32 comp2_imm, Imm32 comp3_imm) {
    u32 comp0 = comp0_imm;
    u32 comp1 = comp1_imm;
    u32 comp2 = comp2_imm;
    u32 comp3 = comp3_imm;
    return spv_OpVectorShuffle(
        &ctx.m, ctx.U32[4], composite1, composite2,
        std::array<u32, 4>{u32(comp0), u32(comp1), u32(comp2), u32(comp3)}.data(), 4);
}

Id EmitCompositeConstructF32x2(EmitContext& ctx, IR::Inst* inst, Id e1, Id e2) {
    return EmitCompositeConstruct(ctx, inst, ctx.F32[2], e1, e2);
}

Id EmitCompositeConstructF32x3(EmitContext& ctx, IR::Inst* inst, Id e1, Id e2, Id e3) {
    return EmitCompositeConstruct(ctx, inst, ctx.F32[3], e1, e2, e3);
}

Id EmitCompositeConstructF32x4(EmitContext& ctx, IR::Inst* inst, Id e1, Id e2, Id e3, Id e4) {
    return EmitCompositeConstruct(ctx, inst, ctx.F32[4], e1, e2, e3, e4);
}

Id EmitCompositeConstructF32x2x2(EmitContext& ctx, IR::Inst* inst, Id e1, Id e2) {
    return EmitCompositeConstruct(ctx, inst, ctx.F32[4], e1, e2);
}

Id EmitCompositeExtractF32x2(EmitContext& ctx, Id composite, Imm32 index_imm) {
    u32 index = index_imm;
    return spv_OpCompositeExtract1(&ctx.m, ctx.F32[1], composite, u32(index));
}

Id EmitCompositeExtractF32x3(EmitContext& ctx, Id composite, Imm32 index_imm) {
    u32 index = index_imm;
    return spv_OpCompositeExtract1(&ctx.m, ctx.F32[1], composite, u32(index));
}

Id EmitCompositeExtractF32x4(EmitContext& ctx, Id composite, Imm32 index_imm) {
    u32 index = index_imm;
    return spv_OpCompositeExtract1(&ctx.m, ctx.F32[1], composite, u32(index));
}

Id EmitCompositeInsertF32x2(EmitContext& ctx, Id composite, Id object, Imm32 index_imm) {
    u32 index = index_imm;
    return spv_OpCompositeInsert1(&ctx.m, ctx.F32[2], object, composite, u32(index));
}

Id EmitCompositeInsertF32x3(EmitContext& ctx, Id composite, Id object, Imm32 index_imm) {
    u32 index = index_imm;
    return spv_OpCompositeInsert1(&ctx.m, ctx.F32[3], object, composite, u32(index));
}

Id EmitCompositeInsertF32x4(EmitContext& ctx, Id composite, Id object, Imm32 index_imm) {
    u32 index = index_imm;
    return spv_OpCompositeInsert1(&ctx.m, ctx.F32[4], object, composite, u32(index));
}

Id EmitCompositeShuffleF32x2(EmitContext& ctx, Id composite1, Id composite2, Imm32 comp0_imm,
                             Imm32 comp1_imm) {
    u32 comp0 = comp0_imm;
    u32 comp1 = comp1_imm;
    return spv_OpVectorShuffle(&ctx.m, ctx.F32[2], composite1, composite2,
                               std::array<u32, 2>{u32(comp0), u32(comp1)}.data(), 2);
}

Id EmitCompositeShuffleF32x3(EmitContext& ctx, Id composite1, Id composite2, Imm32 comp0_imm,
                             Imm32 comp1_imm, Imm32 comp2_imm) {
    u32 comp0 = comp0_imm;
    u32 comp1 = comp1_imm;
    u32 comp2 = comp2_imm;
    return spv_OpVectorShuffle(&ctx.m, ctx.F32[3], composite1, composite2,
                               std::array<u32, 3>{u32(comp0), u32(comp1), u32(comp2)}.data(), 3);
}

Id EmitCompositeShuffleF32x4(EmitContext& ctx, Id composite1, Id composite2, Imm32 comp0_imm,
                             Imm32 comp1_imm, Imm32 comp2_imm, Imm32 comp3_imm) {
    u32 comp0 = comp0_imm;
    u32 comp1 = comp1_imm;
    u32 comp2 = comp2_imm;
    u32 comp3 = comp3_imm;
    return spv_OpVectorShuffle(
        &ctx.m, ctx.F32[4], composite1, composite2,
        std::array<u32, 4>{u32(comp0), u32(comp1), u32(comp2), u32(comp3)}.data(), 4);
}

} // namespace Shader::Backend::SPIRV
