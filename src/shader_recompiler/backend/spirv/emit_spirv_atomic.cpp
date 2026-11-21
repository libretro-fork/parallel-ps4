// SPDX-FileCopyrightText: Copyright 2021 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "shader_recompiler/backend/spirv/emit_spirv_instructions.h"
#include "shader_recompiler/backend/spirv/spirv_emit_context.h"

namespace Shader::Backend::SPIRV {

namespace {
using PointerType = EmitContext::PointerType;
using PointerSize = EmitContext::PointerSize;

/// The scope and memory semantics of the atomics
struct AtomicScope {
    Id scope;
    Id semantics;
};

AtomicScope AtomicArgs(EmitContext& ctx) {
    const Id scope{ctx.ConstU32(static_cast<u32>(SpvScopeDevice))};
    const Id semantics{ctx.u32_zero_value};
    return {scope, semantics};
}

Id SharedAtomicU32(EmitContext& ctx, Id offset, Id value,
                   Id (*atomic_func)(spv_module_t*, Id, Id, Id, Id, Id)) {
    const Id shift_id{ctx.ConstU32(2U)};
    const Id index{spv_OpShiftRightLogical(&ctx.m, ctx.U32[1], offset, shift_id)};
    const Id pointer{ctx.EmitSharedMemoryAccess(ctx.shared_u32, ctx.shared_memory_u32, index)};
    const AtomicScope atomic_args = AtomicArgs(ctx);
    const Id scope = atomic_args.scope;
    const Id semantics = atomic_args.semantics;
    return atomic_func(&ctx.m, ctx.U32[1], pointer, scope, semantics, value);
}

Id SharedAtomicU32IncDec(EmitContext& ctx, Id offset,
                         Id (*atomic_func)(spv_module_t*, Id, Id, Id, Id)) {
    const Id shift_id{ctx.ConstU32(2U)};
    const Id index{spv_OpShiftRightLogical(&ctx.m, ctx.U32[1], offset, shift_id)};
    const Id pointer{ctx.EmitSharedMemoryAccess(ctx.shared_u32, ctx.shared_memory_u32, index)};
    const AtomicScope atomic_args = AtomicArgs(ctx);
    const Id scope = atomic_args.scope;
    const Id semantics = atomic_args.semantics;
    return atomic_func(&ctx.m, ctx.U32[1], pointer, scope, semantics);
}

Id SharedAtomicU64(EmitContext& ctx, Id offset, Id value,
                   Id (*atomic_func)(spv_module_t*, Id, Id, Id, Id, Id)) {
    const Id shift_id{ctx.ConstU32(3U)};
    const Id index{spv_OpShiftRightLogical(&ctx.m, ctx.U32[1], offset, shift_id)};
    const Id pointer{ctx.EmitSharedMemoryAccess(ctx.shared_u64, ctx.shared_memory_u64, index)};
    const AtomicScope atomic_args = AtomicArgs(ctx);
    const Id scope = atomic_args.scope;
    const Id semantics = atomic_args.semantics;
    return atomic_func(&ctx.m, ctx.U64, pointer, scope, semantics, value);
}

Id SharedAtomicU64IncDec(EmitContext& ctx, Id offset,
                         Id (*atomic_func)(spv_module_t*, Id, Id, Id, Id)) {
    const Id shift_id{ctx.ConstU32(3U)};
    const Id index{spv_OpShiftRightLogical(&ctx.m, ctx.U32[1], offset, shift_id)};
    const Id pointer{ctx.EmitSharedMemoryAccess(ctx.shared_u64, ctx.shared_memory_u64, index)};
    const AtomicScope atomic_args = AtomicArgs(ctx);
    const Id scope = atomic_args.scope;
    const Id semantics = atomic_args.semantics;
    return atomic_func(&ctx.m, ctx.U64, pointer, scope, semantics);
}

Id SharedAtomicU32CmpSwap(EmitContext& ctx, Id offset, Id value, Id cmp_value) {
    const Id shift_id{ctx.ConstU32(2U)};
    const Id index{spv_OpShiftRightLogical(&ctx.m, ctx.U32[1], offset, shift_id)};
    const Id pointer{ctx.EmitSharedMemoryAccess(ctx.shared_u32, ctx.shared_memory_u32, index)};
    const AtomicScope atomic_args = AtomicArgs(ctx);
    const Id scope = atomic_args.scope;
    const Id semantics = atomic_args.semantics;
    return spv_OpAtomicCompareExchange(&ctx.m, ctx.U32[1], pointer, scope, semantics, semantics,
                                       value, cmp_value);
}

Id SharedAtomicU64CmpSwap(EmitContext& ctx, Id offset, Id value, Id cmp_value) {
    const Id shift_id{ctx.ConstU32(3U)};
    const Id index{spv_OpShiftRightLogical(&ctx.m, ctx.U32[1], offset, shift_id)};
    const Id pointer{ctx.EmitSharedMemoryAccess(ctx.shared_u64, ctx.shared_memory_u64, index)};
    const AtomicScope atomic_args = AtomicArgs(ctx);
    const Id scope = atomic_args.scope;
    const Id semantics = atomic_args.semantics;
    return spv_OpAtomicCompareExchange(&ctx.m, ctx.U64, pointer, scope, semantics, semantics, value,
                                       cmp_value);
}

template <bool is_float = false>
Id BufferAtomicU32(EmitContext& ctx, IR::Inst* inst, u32 handle, Id address, Id value,
                   Id (*atomic_func)(spv_module_t*, Id, Id, Id, Id, Id)) {
    Shader::Backend::SPIRV::EmitContext::BufferDefinition const& buffer = ctx.buffers[handle];
    const Id type = is_float ? ctx.F32[1] : ctx.U32[1];
    if (const Id offset = buffer.Offset(PointerSize::B32); (offset != 0)) {
        address = spv_OpIAdd(&ctx.m, ctx.U32[1], address, offset);
    }
    const EmitContext::BufferSpv& buffer_alias =
        buffer.Alias(is_float ? PointerType::F32 : PointerType::U32);
    const Id id = buffer_alias.id;
    const Id pointer_type = buffer_alias.pointer_type;
    const Id ptr = spv_OpAccessChain2(&ctx.m, pointer_type, id, ctx.u32_zero_value, address);
    const AtomicScope atomic_args = AtomicArgs(ctx);
    const Id scope = atomic_args.scope;
    const Id semantics = atomic_args.semantics;
    return atomic_func(&ctx.m, type, ptr, scope, semantics, value);
}

Id BufferAtomicU32IncDec(EmitContext& ctx, IR::Inst* inst, u32 handle, Id address,
                         Id (*atomic_func)(spv_module_t*, Id, Id, Id, Id)) {
    Shader::Backend::SPIRV::EmitContext::BufferDefinition const& buffer = ctx.buffers[handle];
    if (const Id offset = buffer.Offset(PointerSize::B32); (offset != 0)) {
        address = spv_OpIAdd(&ctx.m, ctx.U32[1], address, offset);
    }
    const EmitContext::BufferSpv& buffer_alias = buffer.Alias(PointerType::U32);
    const Id id = buffer_alias.id;
    const Id pointer_type = buffer_alias.pointer_type;
    const Id ptr = spv_OpAccessChain2(&ctx.m, pointer_type, id, ctx.u32_zero_value, address);
    const AtomicScope atomic_args = AtomicArgs(ctx);
    const Id scope = atomic_args.scope;
    const Id semantics = atomic_args.semantics;
    return atomic_func(&ctx.m, ctx.U32[1], ptr, scope, semantics);
}

Id BufferAtomicU32CmpSwap(EmitContext& ctx, IR::Inst* inst, u32 handle, Id address, Id value,
                          Id cmp_value,
                          Id (*atomic_func)(spv_module_t*, Id, Id, Id, Id, Id, Id, Id)) {
    Shader::Backend::SPIRV::EmitContext::BufferDefinition const& buffer = ctx.buffers[handle];
    if (const Id offset = buffer.Offset(PointerSize::B32); (offset != 0)) {
        address = spv_OpIAdd(&ctx.m, ctx.U32[1], address, offset);
    }
    const EmitContext::BufferSpv& buffer_alias = buffer.Alias(PointerType::U32);
    const Id id = buffer_alias.id;
    const Id pointer_type = buffer_alias.pointer_type;
    const Id ptr = spv_OpAccessChain2(&ctx.m, pointer_type, id, ctx.u32_zero_value, address);
    const AtomicScope atomic_args = AtomicArgs(ctx);
    const Id scope = atomic_args.scope;
    const Id semantics = atomic_args.semantics;
    return atomic_func(&ctx.m, ctx.U32[1], ptr, scope, semantics, semantics, value, cmp_value);
}

Id BufferAtomicU64(EmitContext& ctx, IR::Inst* inst, u32 handle, Id address, Id value,
                   Id (*atomic_func)(spv_module_t*, Id, Id, Id, Id, Id)) {
    Shader::Backend::SPIRV::EmitContext::BufferDefinition const& buffer = ctx.buffers[handle];
    if (const Id offset = buffer.Offset(PointerSize::B64); (offset != 0)) {
        address = spv_OpIAdd(&ctx.m, ctx.U32[1], address, offset);
    }
    const EmitContext::BufferSpv& buffer_alias = buffer.Alias(PointerType::U64);
    const Id id = buffer_alias.id;
    const Id pointer_type = buffer_alias.pointer_type;
    const Id ptr = spv_OpAccessChain2(&ctx.m, pointer_type, id, ctx.u32_zero_value, address);
    const AtomicScope atomic_args = AtomicArgs(ctx);
    const Id scope = atomic_args.scope;
    const Id semantics = atomic_args.semantics;
    return atomic_func(&ctx.m, ctx.U64, ptr, scope, semantics, value);
}

Id FixImageAtomicCoords(EmitContext& ctx, Id coords, AmdGpu::ImageType image_type) {
    switch (image_type) {
    case AmdGpu::ImageType::Color1D: {
        // Lowered to 2D with height 1
        const unsigned int x = coords;
        return spv_OpCompositeConstruct2(&ctx.m, ctx.U32[2], x, ctx.u32_zero_value);
    }
    case AmdGpu::ImageType::Color1DArray: {
        // Lowered to 2D array with height 1
        const uint32_t x = spv_OpCompositeExtract1(&ctx.m, ctx.U32[1], coords, 0U);
        const uint32_t slice = spv_OpCompositeExtract1(&ctx.m, ctx.U32[1], coords, 1U);
        return spv_OpCompositeConstruct3(&ctx.m, ctx.U32[3], x, ctx.u32_zero_value, slice);
    }
    default:
        return coords;
    }
}

Id ImageAtomicU32(EmitContext& ctx, IR::Inst* inst, u32 handle, Id coords, Id value,
                  Id (*atomic_func)(spv_module_t*, Id, Id, Id, Id, Id)) {
    Shader::Backend::SPIRV::EmitContext::TextureDefinition const& texture =
        ctx.images[handle & 0xFFFF];
    const Id fixed_coords{FixImageAtomicCoords(ctx, coords, texture.view_type)};
    const Id pointer{
        spv_OpImageTexelPointer(&ctx.m, ctx.image_u32, texture.id, fixed_coords, ctx.ConstU32(0U))};
    const AtomicScope atomic_args = AtomicArgs(ctx);
    const Id scope = atomic_args.scope;
    const Id semantics = atomic_args.semantics;
    return atomic_func(&ctx.m, ctx.U32[1], pointer, scope, semantics, value);
}

Id ImageAtomicF32(EmitContext& ctx, IR::Inst* inst, u32 handle, Id coords, Id value,
                  Id (*atomic_func)(spv_module_t*, Id, Id, Id, Id, Id)) {
    Shader::Backend::SPIRV::EmitContext::TextureDefinition const& texture =
        ctx.images[handle & 0xFFFF];
    const Id fixed_coords{FixImageAtomicCoords(ctx, coords, texture.view_type)};
    const Id pointer{
        spv_OpImageTexelPointer(&ctx.m, ctx.image_f32, texture.id, fixed_coords, ctx.ConstU32(0U))};
    const AtomicScope atomic_args = AtomicArgs(ctx);
    const Id scope = atomic_args.scope;
    const Id semantics = atomic_args.semantics;
    return atomic_func(&ctx.m, ctx.F32[1], pointer, scope, semantics, value);
}

Id ImageAtomicU32CmpSwap(EmitContext& ctx, IR::Inst* inst, u32 handle, Id coords, Id value,
                         Id cmp_value,
                         Id (*atomic_func)(spv_module_t*, Id, Id, Id, Id, Id, Id, Id)) {
    Shader::Backend::SPIRV::EmitContext::TextureDefinition const& texture =
        ctx.images[handle & 0xFFFF];
    const Id fixed_coords{FixImageAtomicCoords(ctx, coords, texture.view_type)};
    const Id pointer{
        spv_OpImageTexelPointer(&ctx.m, ctx.image_u32, texture.id, fixed_coords, ctx.ConstU32(0U))};
    const AtomicScope atomic_args = AtomicArgs(ctx);
    const Id scope = atomic_args.scope;
    const Id semantics = atomic_args.semantics;
    return atomic_func(&ctx.m, ctx.U32[1], pointer, scope, semantics, semantics, value, cmp_value);
}
// A float atomic min or max made of integer atomics, for devices without
// VK_EXT_shader_atomic_float2 (NVIDIA): float order is signed integer order for values that
// are not negative and reversed unsigned order for negative ones. The two atomics are in
// branches of their own, so only the one for the value's sign runs: under an OpSelect both
// ran, and a min and a max on one address left the value coming in, whichever came last
// (depth reductions: The Order's shadow cascade bounds; from TdlifePeng's shadPS4, by way of
// doax3-scarlet-pc). The phi takes the labels the branches end in: an atomic may split its
// block itself.
template <typename NegFn, typename NonNegFn>
Id AtomicF32MinMaxFallback(EmitContext& ctx, Id value, NegFn&& emit_neg, NonNegFn&& emit_non_neg) {
    const Id u32_value = spv_OpBitcast(&ctx.m, ctx.U32[1], value);
    const Id sign_bit_set = spv_OpINotEqual(
        &ctx.m, ctx.U1[1],
        spv_OpBitFieldUExtract(&ctx.m, ctx.U32[1], u32_value, ctx.ConstU32(31u), ctx.ConstU32(1u)),
        ctx.u32_zero_value);
    const Id neg_label = spv_OpLabel(&ctx.m);
    const Id non_neg_label = spv_OpLabel(&ctx.m);
    const Id merge_label = spv_OpLabel(&ctx.m);
    spv_OpSelectionMerge(&ctx.m, merge_label, u32(SpvSelectionControlMaskNone));
    spv_OpBranchConditional(&ctx.m, sign_bit_set, neg_label, non_neg_label);
    ctx.AddLabel(neg_label);
    const Id neg_result = EmitBitCastF32U32(ctx, emit_neg(u32_value));
    const Id neg_end = ctx.last_label;
    spv_OpBranch(&ctx.m, merge_label);
    ctx.AddLabel(non_neg_label);
    const Id non_neg_result = EmitBitCastF32U32(ctx, emit_non_neg(u32_value));
    const Id non_neg_end = ctx.last_label;
    spv_OpBranch(&ctx.m, merge_label);
    ctx.AddLabel(merge_label);
    return spv_OpPhi(&ctx.m, ctx.F32[1],
                     Words<4>{{neg_result, neg_end, non_neg_result, non_neg_end}}.v, 4);
}
} // Anonymous namespace

Id EmitSharedAtomicIAdd32(EmitContext& ctx, Id offset, Id value) {
    return SharedAtomicU32(ctx, offset, value, &spv_OpAtomicIAdd);
}

Id EmitSharedAtomicIAdd64(EmitContext& ctx, Id offset, Id value) {
    return SharedAtomicU64(ctx, offset, value, &spv_OpAtomicIAdd);
}

Id EmitSharedAtomicUMax32(EmitContext& ctx, Id offset, Id value) {
    return SharedAtomicU32(ctx, offset, value, &spv_OpAtomicUMax);
}

Id EmitSharedAtomicUMax64(EmitContext& ctx, Id offset, Id value) {
    return SharedAtomicU64(ctx, offset, value, &spv_OpAtomicUMax);
}

Id EmitSharedAtomicSMax32(EmitContext& ctx, Id offset, Id value) {
    return SharedAtomicU32(ctx, offset, value, &spv_OpAtomicSMax);
}

Id EmitSharedAtomicSMax64(EmitContext& ctx, Id offset, Id value) {
    return SharedAtomicU64(ctx, offset, value, &spv_OpAtomicSMax);
}

Id EmitSharedAtomicUMin32(EmitContext& ctx, Id offset, Id value) {
    return SharedAtomicU32(ctx, offset, value, &spv_OpAtomicUMin);
}

Id EmitSharedAtomicUMin64(EmitContext& ctx, Id offset, Id value) {
    return SharedAtomicU64(ctx, offset, value, &spv_OpAtomicUMin);
}

Id EmitSharedAtomicSMin32(EmitContext& ctx, Id offset, Id value) {
    return SharedAtomicU32(ctx, offset, value, &spv_OpAtomicSMin);
}

Id EmitSharedAtomicSMin64(EmitContext& ctx, Id offset, Id value) {
    return SharedAtomicU64(ctx, offset, value, &spv_OpAtomicSMin);
}

Id EmitSharedAtomicAnd32(EmitContext& ctx, Id offset, Id value) {
    return SharedAtomicU32(ctx, offset, value, &spv_OpAtomicAnd);
}

Id EmitSharedAtomicAnd64(EmitContext& ctx, Id offset, Id value) {
    return SharedAtomicU64(ctx, offset, value, &spv_OpAtomicAnd);
}

Id EmitSharedAtomicOr32(EmitContext& ctx, Id offset, Id value) {
    return SharedAtomicU32(ctx, offset, value, &spv_OpAtomicOr);
}

Id EmitSharedAtomicOr64(EmitContext& ctx, Id offset, Id value) {
    return SharedAtomicU64(ctx, offset, value, &spv_OpAtomicOr);
}

Id EmitSharedAtomicXor32(EmitContext& ctx, Id offset, Id value) {
    return SharedAtomicU32(ctx, offset, value, &spv_OpAtomicXor);
}

Id EmitSharedAtomicXor64(EmitContext& ctx, Id offset, Id value) {
    return SharedAtomicU64(ctx, offset, value, &spv_OpAtomicXor);
}

Id EmitSharedAtomicISub32(EmitContext& ctx, Id offset, Id value) {
    return SharedAtomicU32(ctx, offset, value, &spv_OpAtomicISub);
}

Id EmitSharedAtomicISub64(EmitContext& ctx, Id offset, Id value) {
    return SharedAtomicU64(ctx, offset, value, &spv_OpAtomicISub);
}

Id EmitSharedAtomicCmpSwap32(EmitContext& ctx, Id offset, Id value, Id cmp_value) {
    return SharedAtomicU32CmpSwap(ctx, offset, value, cmp_value);
}

Id EmitSharedAtomicCmpSwap64(EmitContext& ctx, Id offset, Id value, Id cmp_value) {
    return SharedAtomicU64CmpSwap(ctx, offset, value, cmp_value);
}

Id EmitSharedAtomicInc32(EmitContext& ctx, Id offset) {
    return SharedAtomicU32IncDec(ctx, offset, &spv_OpAtomicIIncrement);
}

Id EmitSharedAtomicInc64(EmitContext& ctx, Id offset) {
    return SharedAtomicU64IncDec(ctx, offset, &spv_OpAtomicIIncrement);
}

Id EmitSharedAtomicDec32(EmitContext& ctx, Id offset) {
    return SharedAtomicU32IncDec(ctx, offset, &spv_OpAtomicIDecrement);
}

Id EmitSharedAtomicDec64(EmitContext& ctx, Id offset) {
    return SharedAtomicU64IncDec(ctx, offset, &spv_OpAtomicIDecrement);
}

Id EmitBufferAtomicIAdd32(EmitContext& ctx, IR::Inst* inst, Imm32 handle_imm, Id address,
                          Id value) {
    u32 handle = handle_imm;
    return BufferAtomicU32(ctx, inst, handle, address, value, &spv_OpAtomicIAdd);
}

Id EmitBufferAtomicIAdd64(EmitContext& ctx, IR::Inst* inst, Imm32 handle_imm, Id address,
                          Id value) {
    u32 handle = handle_imm;
    return BufferAtomicU64(ctx, inst, handle, address, value, &spv_OpAtomicIAdd);
}

Id EmitBufferAtomicISub32(EmitContext& ctx, IR::Inst* inst, Imm32 handle_imm, Id address,
                          Id value) {
    u32 handle = handle_imm;
    return BufferAtomicU32(ctx, inst, handle, address, value, &spv_OpAtomicISub);
}

Id EmitBufferAtomicSMin32(EmitContext& ctx, IR::Inst* inst, Imm32 handle_imm, Id address,
                          Id value) {
    u32 handle = handle_imm;
    return BufferAtomicU32(ctx, inst, handle, address, value, &spv_OpAtomicSMin);
}

Id EmitBufferAtomicSMin64(EmitContext& ctx, IR::Inst* inst, Imm32 handle_imm, Id address,
                          Id value) {
    u32 handle = handle_imm;
    return BufferAtomicU64(ctx, inst, handle, address, value, &spv_OpAtomicSMin);
}

Id EmitBufferAtomicUMin32(EmitContext& ctx, IR::Inst* inst, Imm32 handle_imm, Id address,
                          Id value) {
    u32 handle = handle_imm;
    return BufferAtomicU32(ctx, inst, handle, address, value, &spv_OpAtomicUMin);
}

Id EmitBufferAtomicUMin64(EmitContext& ctx, IR::Inst* inst, Imm32 handle_imm, Id address,
                          Id value) {
    u32 handle = handle_imm;
    return BufferAtomicU64(ctx, inst, handle, address, value, &spv_OpAtomicUMin);
}

Id EmitBufferAtomicFMin32(EmitContext& ctx, IR::Inst* inst, Imm32 handle_imm, Id address,
                          Id value) {
    u32 handle = handle_imm;
    if (ctx.profile.supports_buffer_fp32_atomic_min_max) {
        return BufferAtomicU32<true>(ctx, inst, handle, address, value, &spv_OpAtomicFMin);
    }

    return AtomicF32MinMaxFallback(
        ctx, value,
        [&](Id v) { return EmitBufferAtomicUMax32(ctx, inst, Imm32(handle), address, v); },
        [&](Id v) { return EmitBufferAtomicSMin32(ctx, inst, Imm32(handle), address, v); });
}

Id EmitBufferAtomicSMax32(EmitContext& ctx, IR::Inst* inst, Imm32 handle_imm, Id address,
                          Id value) {
    u32 handle = handle_imm;
    return BufferAtomicU32(ctx, inst, handle, address, value, &spv_OpAtomicSMax);
}

Id EmitBufferAtomicSMax64(EmitContext& ctx, IR::Inst* inst, Imm32 handle_imm, Id address,
                          Id value) {
    u32 handle = handle_imm;
    return BufferAtomicU64(ctx, inst, handle, address, value, &spv_OpAtomicSMax);
}

Id EmitBufferAtomicUMax32(EmitContext& ctx, IR::Inst* inst, Imm32 handle_imm, Id address,
                          Id value) {
    u32 handle = handle_imm;
    return BufferAtomicU32(ctx, inst, handle, address, value, &spv_OpAtomicUMax);
}

Id EmitBufferAtomicUMax64(EmitContext& ctx, IR::Inst* inst, Imm32 handle_imm, Id address,
                          Id value) {
    u32 handle = handle_imm;
    return BufferAtomicU64(ctx, inst, handle, address, value, &spv_OpAtomicUMax);
}

Id EmitBufferAtomicFMax32(EmitContext& ctx, IR::Inst* inst, Imm32 handle_imm, Id address,
                          Id value) {
    u32 handle = handle_imm;
    if (ctx.profile.supports_buffer_fp32_atomic_min_max) {
        return BufferAtomicU32<true>(ctx, inst, handle, address, value, &spv_OpAtomicFMax);
    }

    return AtomicF32MinMaxFallback(
        ctx, value,
        [&](Id v) { return EmitBufferAtomicUMin32(ctx, inst, Imm32(handle), address, v); },
        [&](Id v) { return EmitBufferAtomicSMax32(ctx, inst, Imm32(handle), address, v); });
}

Id EmitBufferAtomicInc32(EmitContext& ctx, IR::Inst* inst, Imm32 handle_imm, Id address) {
    u32 handle = handle_imm;
    return BufferAtomicU32IncDec(ctx, inst, handle, address, &spv_OpAtomicIIncrement);
}

Id EmitBufferAtomicDec32(EmitContext& ctx, IR::Inst* inst, Imm32 handle_imm, Id address) {
    u32 handle = handle_imm;
    return BufferAtomicU32IncDec(ctx, inst, handle, address, &spv_OpAtomicIDecrement);
}

Id EmitBufferAtomicAnd32(EmitContext& ctx, IR::Inst* inst, Imm32 handle_imm, Id address, Id value) {
    u32 handle = handle_imm;
    return BufferAtomicU32(ctx, inst, handle, address, value, &spv_OpAtomicAnd);
}

Id EmitBufferAtomicOr32(EmitContext& ctx, IR::Inst* inst, Imm32 handle_imm, Id address, Id value) {
    u32 handle = handle_imm;
    return BufferAtomicU32(ctx, inst, handle, address, value, &spv_OpAtomicOr);
}

Id EmitBufferAtomicXor32(EmitContext& ctx, IR::Inst* inst, Imm32 handle_imm, Id address, Id value) {
    u32 handle = handle_imm;
    return BufferAtomicU32(ctx, inst, handle, address, value, &spv_OpAtomicXor);
}

Id EmitBufferAtomicSwap32(EmitContext& ctx, IR::Inst* inst, Imm32 handle_imm, Id address,
                          Id value) {
    u32 handle = handle_imm;
    return BufferAtomicU32(ctx, inst, handle, address, value, &spv_OpAtomicExchange);
}

Id EmitBufferAtomicCmpSwap32(EmitContext& ctx, IR::Inst* inst, Imm32 handle_imm, Id address,
                             Id value, Id cmp_value) {
    u32 handle = handle_imm;
    return BufferAtomicU32CmpSwap(ctx, inst, handle, address, value, cmp_value,
                                  &spv_OpAtomicCompareExchange);
}

Id EmitBufferAtomicFCmpSwap32(EmitContext& ctx, IR::Inst* inst, Imm32 handle_imm, Id address,
                              Id value, Id cmp_value) {
    u32 handle = handle_imm;
    const uint32_t u32_value = spv_OpBitcast(&ctx.m, ctx.U32[1], value);
    const uint32_t u32_cmp = spv_OpBitcast(&ctx.m, ctx.U32[1], cmp_value);
    const unsigned int result = BufferAtomicU32CmpSwap(ctx, inst, handle, address, u32_value,
                                                       u32_cmp, &spv_OpAtomicCompareExchange);
    return spv_OpBitcast(&ctx.m, ctx.F32[1], result);
}

Id EmitImageAtomicIAdd32(EmitContext& ctx, IR::Inst* inst, Imm32 handle_imm, Id coords, Id value) {
    u32 handle = handle_imm;
    return ImageAtomicU32(ctx, inst, handle, coords, value, &spv_OpAtomicIAdd);
}

Id EmitImageAtomicSMin32(EmitContext& ctx, IR::Inst* inst, Imm32 handle_imm, Id coords, Id value) {
    u32 handle = handle_imm;
    return ImageAtomicU32(ctx, inst, handle, coords, value, &spv_OpAtomicSMin);
}

Id EmitImageAtomicUMin32(EmitContext& ctx, IR::Inst* inst, Imm32 handle_imm, Id coords, Id value) {
    u32 handle = handle_imm;
    return ImageAtomicU32(ctx, inst, handle, coords, value, &spv_OpAtomicUMin);
}

Id EmitImageAtomicSMax32(EmitContext& ctx, IR::Inst* inst, Imm32 handle_imm, Id coords, Id value) {
    u32 handle = handle_imm;
    return ImageAtomicU32(ctx, inst, handle, coords, value, &spv_OpAtomicSMax);
}

Id EmitImageAtomicUMax32(EmitContext& ctx, IR::Inst* inst, Imm32 handle_imm, Id coords, Id value) {
    u32 handle = handle_imm;
    return ImageAtomicU32(ctx, inst, handle, coords, value, &spv_OpAtomicUMax);
}

Id EmitImageAtomicFMax32(EmitContext& ctx, IR::Inst* inst, Imm32 handle_imm, Id coords, Id value) {
    u32 handle = handle_imm;
    if (ctx.profile.supports_image_fp32_atomic_min_max) {
        return ImageAtomicF32(ctx, inst, handle, coords, value, &spv_OpAtomicFMax);
    }

    return AtomicF32MinMaxFallback(
        ctx, value,
        [&](Id v) { return EmitImageAtomicUMin32(ctx, inst, Imm32(handle), coords, v); },
        [&](Id v) { return EmitImageAtomicSMax32(ctx, inst, Imm32(handle), coords, v); });
}

Id EmitImageAtomicFMin32(EmitContext& ctx, IR::Inst* inst, Imm32 handle_imm, Id coords, Id value) {
    u32 handle = handle_imm;
    if (ctx.profile.supports_image_fp32_atomic_min_max) {
        return ImageAtomicF32(ctx, inst, handle, coords, value, &spv_OpAtomicFMin);
    }

    return AtomicF32MinMaxFallback(
        ctx, value,
        [&](Id v) { return EmitImageAtomicUMax32(ctx, inst, Imm32(handle), coords, v); },
        [&](Id v) { return EmitImageAtomicSMin32(ctx, inst, Imm32(handle), coords, v); });
}

Id EmitImageAtomicInc32(EmitContext&, IR::Inst*, Imm32, Id, Id) {
    UNREACHABLE_MSG("SPIR-V Instruction");
}

Id EmitImageAtomicDec32(EmitContext&, IR::Inst*, Imm32, Id, Id) {
    UNREACHABLE_MSG("SPIR-V Instruction");
}

Id EmitImageAtomicAnd32(EmitContext& ctx, IR::Inst* inst, Imm32 handle_imm, Id coords, Id value) {
    u32 handle = handle_imm;
    return ImageAtomicU32(ctx, inst, handle, coords, value, &spv_OpAtomicAnd);
}

Id EmitImageAtomicOr32(EmitContext& ctx, IR::Inst* inst, Imm32 handle_imm, Id coords, Id value) {
    u32 handle = handle_imm;
    return ImageAtomicU32(ctx, inst, handle, coords, value, &spv_OpAtomicOr);
}

Id EmitImageAtomicXor32(EmitContext& ctx, IR::Inst* inst, Imm32 handle_imm, Id coords, Id value) {
    u32 handle = handle_imm;
    return ImageAtomicU32(ctx, inst, handle, coords, value, &spv_OpAtomicXor);
}

Id EmitImageAtomicExchange32(EmitContext& ctx, IR::Inst* inst, Imm32 handle_imm, Id coords,
                             Id value) {
    u32 handle = handle_imm;
    return ImageAtomicU32(ctx, inst, handle, coords, value, &spv_OpAtomicExchange);
}

Id EmitImageAtomicCmpSwap32(EmitContext& ctx, IR::Inst* inst, Imm32 handle_imm, Id coords, Id value,
                            Id cmp_value) {
    u32 handle = handle_imm;
    return ImageAtomicU32CmpSwap(ctx, inst, handle, coords, value, cmp_value,
                                 &spv_OpAtomicCompareExchange);
}

Id EmitDataAppend(EmitContext& ctx, Id gds_dw_offset, Id exec) {
    UNREACHABLE_MSG("SPIR-V Instruction");
}

Id EmitDataConsume(EmitContext& ctx, Id gds_dw_offset, Id exec) {
    UNREACHABLE_MSG("SPIR-V Instruction");
}

} // namespace Shader::Backend::SPIRV
