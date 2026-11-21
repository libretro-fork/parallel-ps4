// SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "common/small_vector.h"
#include "shader_recompiler/backend/spirv/emit_spirv_instructions.h"
#include "shader_recompiler/backend/spirv/spirv_emit_context.h"
#include "shader_recompiler/ir/microinstruction.h"

namespace Shader::Backend::SPIRV {

template <bool is_float>
static Id FixImageCoords(EmitContext& ctx, Id coords, AmdGpu::ImageType image_type) {
    const Shader::Backend::SPIRV::VectorIds coord_type = is_float ? ctx.F32 : ctx.U32;
    const unsigned int zero = is_float ? ctx.f32_zero_value : ctx.u32_zero_value;

    switch (image_type) {
    case AmdGpu::ImageType::Color1D: {
        // Lowered to 2D with height 1
        const unsigned int x = coords;
        return spv_OpCompositeConstruct2(&ctx.m, coord_type[2], x, zero);
    }
    case AmdGpu::ImageType::Color1DArray: {
        // Lowered to 2D array with height 1
        const uint32_t x = spv_OpCompositeExtract1(&ctx.m, coord_type[1], coords, 0U);
        const uint32_t slice = spv_OpCompositeExtract1(&ctx.m, coord_type[1], coords, 1U);
        return spv_OpCompositeConstruct3(&ctx.m, coord_type[3], x, zero, slice);
    }
    case AmdGpu::ImageType::Cube: {
        // Lowered to 2D array
        if (is_float) {
            const uint32_t x = spv_OpCompositeExtract1(&ctx.m, coord_type[1], coords, 0U);
            const uint32_t y = spv_OpCompositeExtract1(&ctx.m, coord_type[1], coords, 1U);
            const uint32_t face = spv_OpCompositeExtract1(&ctx.m, coord_type[1], coords, 2U);

            // AMD cube math results in coordinates in the range [1.0, 2.0]. We need
            // to convert this to the range [0.0, 1.0] to get correct results.
            const unsigned int one = ctx.ConstF32(1.f);
            const uint32_t fixed_x = spv_OpFSub(&ctx.m, coord_type[1], x, one);
            const uint32_t fixed_y = spv_OpFSub(&ctx.m, coord_type[1], y, one);
            const uint32_t fixed_face =
                spv_OpFma(&ctx.m, coord_type[1],
                          spv_OpFloor(&ctx.m, coord_type[1],
                                      spv_OpFDiv(&ctx.m, coord_type[1], face, ctx.ConstF32(8.f))),
                          ctx.ConstF32(-2.f), face);
            return spv_OpCompositeConstruct3(&ctx.m, coord_type[3], fixed_x, fixed_y, fixed_face);
        }
        return coords;
    }
    default:
        return coords;
    }
}

template <bool is_float>
static Id PadImageOperand(EmitContext& ctx, Id operand, AmdGpu::ImageType image_type) {
    if (image_type == AmdGpu::ImageType::Color1D || image_type == AmdGpu::ImageType::Color1DArray) {
        const Shader::Backend::SPIRV::VectorIds coord_type = is_float ? ctx.F32 : ctx.U32;
        const unsigned int zero = is_float ? ctx.f32_zero_value : ctx.u32_zero_value;
        return spv_OpCompositeConstruct2(&ctx.m, coord_type[2], operand, zero);
    }
    return operand;
}

struct ImageOperands {
    void Add(u32 new_mask, Id value) {
        if (!(value != 0)) {
            return;
        }
        mask = static_cast<u32>(static_cast<u32>(mask) | static_cast<u32>(new_mask));
        operands.push_back(value);
    }
    void Add(u32 new_mask, Id value1, Id value2) {
        mask = static_cast<u32>(static_cast<u32>(mask) | static_cast<u32>(new_mask));
        operands.push_back(value1);
        operands.push_back(value2);
    }

    void AddOffset(EmitContext& ctx, AmdGpu::ImageType image_type, const IR::Value& offset,
                   bool can_use_runtime_offsets = false) {
        if (offset.IsEmpty()) {
            return;
        }
        u32 op_mask{};
        Id value{};
        if (offset.IsImmediate()) {
            op_mask = SpvImageOperandsConstOffsetMask;
            value = ctx.ConstS32(static_cast<s32>(offset.U32()));
        } else {
            IR::Inst* const inst{offset.Inst()};
            if (inst->AreAllArgsImmediates()) {
                switch (inst->GetOpcode()) {
                case IR::Opcode::CompositeConstructU32x2:
                    op_mask = SpvImageOperandsConstOffsetMask;
                    value = ctx.ConstS32(static_cast<s32>(inst->Arg(0).U32()),
                                         static_cast<s32>(inst->Arg(1).U32()));
                    break;
                case IR::Opcode::CompositeConstructU32x3:
                    op_mask = SpvImageOperandsConstOffsetMask;
                    value = ctx.ConstS32(static_cast<s32>(inst->Arg(0).U32()),
                                         static_cast<s32>(inst->Arg(1).U32()),
                                         static_cast<s32>(inst->Arg(2).U32()));
                    break;
                default:
                    break;
                }
            }
        }

        if (op_mask != SpvImageOperandsConstOffsetMask) {
            if (!can_use_runtime_offsets) {
                LOG_WARNING(Render_Vulkan,
                            "Runtime offset provided to unsupported image sample instruction");
                return;
            }
            op_mask = SpvImageOperandsOffsetMask;
            value = ctx.Def(offset);
        }

        value = PadImageOperand<false>(ctx, value, image_type);
        Add(op_mask, value);
    }

    void AddDerivatives(EmitContext& ctx, AmdGpu::ImageType image_type, Id derivatives_dx,
                        Id derivatives_dy) {
        if (!(derivatives_dx != 0) || !(derivatives_dy != 0)) {
            return;
        }
        derivatives_dx = PadImageOperand<true>(ctx, derivatives_dx, image_type);
        derivatives_dy = PadImageOperand<true>(ctx, derivatives_dy, image_type);
        Add(SpvImageOperandsGradMask, derivatives_dx, derivatives_dy);
    }

    u32 mask{};
    Common::StaticVector<Id, 4> operands;
};

Id EmitImageHandle(EmitContext& ctx, Id, Id) {
    UNREACHABLE_MSG("Unreachable instruction");
}

Id EmitImageSampleRaw(EmitContext& ctx, IR::Inst* inst, Imm32 handle_imm, Id address1, Id address2,
                      Id address3, Id address4) {
    u32 handle = handle_imm;
    UNREACHABLE_MSG("Unreachable instruction");
}

Id EmitImageSampleImplicitLod(EmitContext& ctx, IR::Inst* inst, Imm32 handle_imm, Id coords,
                              Id bias, const IR::Value& offset) {
    u32 handle = handle_imm;
    Shader::Backend::SPIRV::EmitContext::TextureDefinition const& texture =
        ctx.images[handle & 0xFFFF];
    const Id image = spv_OpLoad(&ctx.m, texture.image_type, texture.id);
    const Id result_type = texture.data_types->Get(4);
    const Id sampler = spv_OpLoad(&ctx.m, ctx.sampler_type, ctx.samplers[handle >> 16]);
    const Id sampled_image = spv_OpSampledImage(&ctx.m, texture.sampled_type, image, sampler);
    const Id fixed_coords = FixImageCoords<true>(ctx, coords, texture.view_type);
    ImageOperands operands;
    operands.Add(SpvImageOperandsBiasMask, bias);
    operands.AddOffset(ctx, texture.view_type, offset);
    const Id sample = spv_OpImageSampleImplicitLod(&ctx.m, result_type, sampled_image, fixed_coords,
                                                   u32(operands.mask), operands.operands.data(),
                                                   u32(operands.operands.size()));
    return texture.is_integer ? spv_OpBitcast(&ctx.m, ctx.F32[4], sample) : sample;
}

Id EmitImageSampleExplicitLod(EmitContext& ctx, IR::Inst* inst, Imm32 handle_imm, Id coords, Id lod,
                              const IR::Value& offset) {
    u32 handle = handle_imm;
    Shader::Backend::SPIRV::EmitContext::TextureDefinition const& texture =
        ctx.images[handle & 0xFFFF];
    const Id image = spv_OpLoad(&ctx.m, texture.image_type, texture.id);
    const Id result_type = texture.data_types->Get(4);
    const Id sampler = spv_OpLoad(&ctx.m, ctx.sampler_type, ctx.samplers[handle >> 16]);
    const Id sampled_image = spv_OpSampledImage(&ctx.m, texture.sampled_type, image, sampler);
    const Id fixed_coords = FixImageCoords<true>(ctx, coords, texture.view_type);
    ImageOperands operands;
    operands.Add(SpvImageOperandsLodMask, lod);
    operands.AddOffset(ctx, texture.view_type, offset);
    const Id sample = spv_OpImageSampleExplicitLod(&ctx.m, result_type, sampled_image, fixed_coords,
                                                   u32(operands.mask), operands.operands.data(),
                                                   u32(operands.operands.size()));
    return texture.is_integer ? spv_OpBitcast(&ctx.m, ctx.F32[4], sample) : sample;
}

Id EmitImageSampleDrefImplicitLod(EmitContext& ctx, IR::Inst* inst, Imm32 handle_imm, Id coords,
                                  Id dref, Id bias, const IR::Value& offset) {
    u32 handle = handle_imm;
    Shader::Backend::SPIRV::EmitContext::TextureDefinition const& texture =
        ctx.images[handle & 0xFFFF];
    const Id image = spv_OpLoad(&ctx.m, texture.image_type, texture.id);
    const Id result_type = texture.data_types->Get(1);
    const Id sampler = spv_OpLoad(&ctx.m, ctx.sampler_type, ctx.samplers[handle >> 16]);
    const Id sampled_image = spv_OpSampledImage(&ctx.m, texture.sampled_type, image, sampler);
    const Id fixed_coords = FixImageCoords<true>(ctx, coords, texture.view_type);
    ImageOperands operands;
    operands.Add(SpvImageOperandsBiasMask, bias);
    operands.AddOffset(ctx, texture.view_type, offset);
    const Id sample = spv_OpImageSampleDrefImplicitLod(
        &ctx.m, result_type, sampled_image, fixed_coords, dref, u32(operands.mask),
        operands.operands.data(), u32(operands.operands.size()));
    const Id sample_typed = texture.is_integer ? spv_OpBitcast(&ctx.m, ctx.F32[1], sample) : sample;
    return spv_OpCompositeConstruct4(&ctx.m, ctx.F32[4], sample_typed, ctx.f32_zero_value,
                                     ctx.f32_zero_value, ctx.f32_zero_value);
}

Id EmitImageSampleDrefExplicitLod(EmitContext& ctx, IR::Inst* inst, Imm32 handle_imm, Id coords,
                                  Id dref, Id lod, const IR::Value& offset) {
    u32 handle = handle_imm;
    Shader::Backend::SPIRV::EmitContext::TextureDefinition const& texture =
        ctx.images[handle & 0xFFFF];
    const Id image = spv_OpLoad(&ctx.m, texture.image_type, texture.id);
    const Id result_type = texture.data_types->Get(1);
    const Id sampler = spv_OpLoad(&ctx.m, ctx.sampler_type, ctx.samplers[handle >> 16]);
    const Id sampled_image = spv_OpSampledImage(&ctx.m, texture.sampled_type, image, sampler);
    const Id fixed_coords = FixImageCoords<true>(ctx, coords, texture.view_type);
    ImageOperands operands;
    operands.Add(SpvImageOperandsLodMask, lod);
    operands.AddOffset(ctx, texture.view_type, offset);
    const Id sample = spv_OpImageSampleDrefExplicitLod(
        &ctx.m, result_type, sampled_image, fixed_coords, dref, u32(operands.mask),
        operands.operands.data(), u32(operands.operands.size()));
    const Id sample_typed = texture.is_integer ? spv_OpBitcast(&ctx.m, ctx.F32[1], sample) : sample;
    return spv_OpCompositeConstruct4(&ctx.m, ctx.F32[4], sample_typed, ctx.f32_zero_value,
                                     ctx.f32_zero_value, ctx.f32_zero_value);
}

Id EmitImageGather(EmitContext& ctx, IR::Inst* inst, Imm32 handle_imm, Id coords,
                   const IR::Value& offset) {
    u32 handle = handle_imm;
    Shader::Backend::SPIRV::EmitContext::TextureDefinition const& texture =
        ctx.images[handle & 0xFFFF];
    const Id image = spv_OpLoad(&ctx.m, texture.image_type, texture.id);
    const Id result_type = texture.data_types->Get(4);
    const Id sampler = spv_OpLoad(&ctx.m, ctx.sampler_type, ctx.samplers[handle >> 16]);
    const Id sampled_image = spv_OpSampledImage(&ctx.m, texture.sampled_type, image, sampler);
    const Id fixed_coords = FixImageCoords<true>(ctx, coords, texture.view_type);
    const u32 comp = inst->Flags<IR::TextureInstInfo>().gather_comp.Value();
    ImageOperands operands;
    operands.AddOffset(ctx, texture.view_type, offset, true);
    const Id texels = spv_OpImageGather(&ctx.m, result_type, sampled_image, fixed_coords,
                                        ctx.ConstU32(comp), u32(operands.mask),
                                        operands.operands.data(), u32(operands.operands.size()));
    return texture.is_integer ? spv_OpBitcast(&ctx.m, ctx.F32[4], texels) : texels;
}

Id EmitImageGatherDref(EmitContext& ctx, IR::Inst* inst, Imm32 handle_imm, Id coords,
                       const IR::Value& offset, Id dref) {
    u32 handle = handle_imm;
    Shader::Backend::SPIRV::EmitContext::TextureDefinition const& texture =
        ctx.images[handle & 0xFFFF];
    const Id image = spv_OpLoad(&ctx.m, texture.image_type, texture.id);
    const Id result_type = texture.data_types->Get(4);
    const Id sampler = spv_OpLoad(&ctx.m, ctx.sampler_type, ctx.samplers[handle >> 16]);
    const Id sampled_image = spv_OpSampledImage(&ctx.m, texture.sampled_type, image, sampler);
    const Id fixed_coords = FixImageCoords<true>(ctx, coords, texture.view_type);
    ImageOperands operands;
    operands.AddOffset(ctx, texture.view_type, offset, true);
    const Id texels = spv_OpImageDrefGather(&ctx.m, result_type, sampled_image, fixed_coords, dref,
                                            u32(operands.mask), operands.operands.data(),
                                            u32(operands.operands.size()));
    return texture.is_integer ? spv_OpBitcast(&ctx.m, ctx.F32[4], texels) : texels;
}

/// The levels of @image, 0 for one without mips
static Id QueryLevels(EmitContext& ctx, Id image, bool has_mips) {
    return has_mips ? spv_OpImageQueryLevels(&ctx.m, ctx.U32[1], image) : ctx.u32_zero_value;
}

/// The size of @image as @type, at @lod when the image has levels to ask of
static Id QuerySize(EmitContext& ctx, Id type, Id image, Id lod, bool uses_lod) {
    return uses_lod ? spv_OpImageQuerySizeLod(&ctx.m, type, image, lod)
                    : spv_OpImageQuerySize(&ctx.m, type, image);
}

Id EmitImageQueryDimensions(EmitContext& ctx, IR::Inst* inst, Imm32 handle_imm, Id lod,
                            bool has_mips) {
    u32 handle = handle_imm;
    Shader::Backend::SPIRV::EmitContext::TextureDefinition const& texture =
        ctx.images[handle & 0xFFFF];
    const Id image = spv_OpLoad(&ctx.m, texture.image_type, texture.id);
    const Id zero = ctx.u32_zero_value;
    const bool uses_lod{texture.view_type != AmdGpu::ImageType::Color2DMsaa && !texture.is_storage};
    switch (texture.view_type) {
    case AmdGpu::ImageType::Color1D: {
        const uint32_t width = spv_OpCompositeExtract1(
            &ctx.m, ctx.U32[1], QuerySize(ctx, ctx.U32[2], image, lod, uses_lod), 0U);
        return spv_OpCompositeConstruct4(&ctx.m, ctx.U32[4], width, zero, zero,
                                         QueryLevels(ctx, image, has_mips));
    }
    case AmdGpu::ImageType::Color1DArray: {
        const uint32_t base = QuerySize(ctx, ctx.U32[3], image, lod, uses_lod);
        const uint32_t width = spv_OpCompositeExtract1(&ctx.m, ctx.U32[1], base, 0U);
        const uint32_t slices = spv_OpCompositeExtract1(&ctx.m, ctx.U32[1], base, 2U);
        return spv_OpCompositeConstruct4(&ctx.m, ctx.U32[4], width, slices, zero,
                                         QueryLevels(ctx, image, has_mips));
    }
    case AmdGpu::ImageType::Color2D:
    case AmdGpu::ImageType::Color2DMsaa:
        return spv_OpCompositeConstruct3(&ctx.m, ctx.U32[4],
                                         QuerySize(ctx, ctx.U32[2], image, lod, uses_lod), zero,
                                         QueryLevels(ctx, image, has_mips));
    case AmdGpu::ImageType::Color2DArray:
    case AmdGpu::ImageType::Cube:
    case AmdGpu::ImageType::Color3D:
        return spv_OpCompositeConstruct2(&ctx.m, ctx.U32[4],
                                         QuerySize(ctx, ctx.U32[3], image, lod, uses_lod),
                                         QueryLevels(ctx, image, has_mips));
    default:
        UNREACHABLE_MSG("SPIR-V Instruction");
    }
}

Id EmitImageQueryLod(EmitContext& ctx, IR::Inst* inst, Imm32 handle_imm, Id coords) {
    u32 handle = handle_imm;
    Shader::Backend::SPIRV::EmitContext::TextureDefinition const& texture =
        ctx.images[handle & 0xFFFF];
    const Id image = spv_OpLoad(&ctx.m, texture.image_type, texture.id);
    const Id sampler = spv_OpLoad(&ctx.m, ctx.sampler_type, ctx.samplers[handle >> 16]);
    const Id sampled_image = spv_OpSampledImage(&ctx.m, texture.sampled_type, image, sampler);
    const Id fixed_coords = FixImageCoords<true>(ctx, coords, texture.view_type);
    return spv_OpImageQueryLod(&ctx.m, ctx.F32[2], sampled_image, fixed_coords);
}

Id EmitImageGradient(EmitContext& ctx, IR::Inst* inst, Imm32 handle_imm, Id coords,
                     Id derivatives_dx, Id derivatives_dy, const IR::Value& offset,
                     const IR::Value& lod_clamp) {
    u32 handle = handle_imm;
    Shader::Backend::SPIRV::EmitContext::TextureDefinition const& texture =
        ctx.images[handle & 0xFFFF];
    const Id image = spv_OpLoad(&ctx.m, texture.image_type, texture.id);
    const Id result_type = texture.data_types->Get(4);
    const Id sampler = spv_OpLoad(&ctx.m, ctx.sampler_type, ctx.samplers[handle >> 16]);
    const Id sampled_image = spv_OpSampledImage(&ctx.m, texture.sampled_type, image, sampler);
    const Id fixed_coords = FixImageCoords<true>(ctx, coords, texture.view_type);
    ImageOperands operands;
    operands.AddDerivatives(ctx, texture.view_type, derivatives_dx, derivatives_dy);
    operands.AddOffset(ctx, texture.view_type, offset);
    const Id sample = spv_OpImageSampleExplicitLod(&ctx.m, result_type, sampled_image, fixed_coords,
                                                   u32(operands.mask), operands.operands.data(),
                                                   u32(operands.operands.size()));
    return texture.is_integer ? spv_OpBitcast(&ctx.m, ctx.F32[4], sample) : sample;
}

Id EmitImageRead(EmitContext& ctx, IR::Inst* inst, Imm32 handle_imm, Id coords, Id lod, Id ms) {
    u32 handle = handle_imm;
    Shader::Backend::SPIRV::EmitContext::TextureDefinition const& texture =
        ctx.images[handle & 0xFFFF];
    const Id color_type = texture.data_types->Get(4);
    const Id fixed_coords = FixImageCoords<false>(ctx, coords, texture.view_type);
    ImageOperands operands;
    Id texel;
    if (!texture.is_storage) {
        const Id image = spv_OpLoad(&ctx.m, texture.image_type, texture.id);
        if (texture.view_type == AmdGpu::ImageType::Color2DMsaa) {
            // GCN hardware wraps out-of-range MSAA sample indices
            if ((ms != 0)) {
                const Id sample_count = spv_OpImageQuerySamples(&ctx.m, ctx.U32[1], image);
                const Id wrapped_ms = spv_OpUMod(&ctx.m, ctx.U32[1], ms, sample_count);
                operands.Add(SpvImageOperandsSampleMask, wrapped_ms);
            }
        } else {
            if ((ms != 0)) {
                LOG_ERROR(Render_Recompiler, "image is not MS but ms operand is provided");
            }
            operands.Add(SpvImageOperandsLodMask, lod);
            operands.Add(SpvImageOperandsSampleMask, ms);
        }
        texel = spv_OpImageFetch(&ctx.m, color_type, image, fixed_coords, u32(operands.mask),
                                 operands.operands.data(), u32(operands.operands.size()));
    } else {
        Id image_ptr = texture.id;
        if (ctx.profile.supports_image_load_store_lod) {
            operands.Add(SpvImageOperandsLodMask, lod);
        } else if ((lod != 0)) {
#if 1
            // It's confusing what interactions will cause this code path so leave it as
            // unreachable until a case is found.
            // Normally IMAGE_LOAD_MIP should translate -> OpImageFetch
            UNREACHABLE_MSG("Unsupported ImageRead with Lod");
#else
            LOG_WARNING(Render, "Fallback for ImageRead with LOD");
            ASSERT(texture.mip_fallback_mode == MipStorageFallbackMode::DynamicIndex);
            const Id single_image_ptr_type =
                spv_TypePointer(&ctx.m, u32(SpvStorageClassUniformConstant), texture.image_type);
            image_ptr = spv_OpAccessChain(&ctx.m, single_image_ptr_type, image_ptr, &lod, 1u);
#endif
        }
        const Id image = spv_OpLoad(&ctx.m, texture.image_type, image_ptr);
        texel = spv_OpImageRead(&ctx.m, color_type, image, fixed_coords, u32(operands.mask),
                                operands.operands.data(), u32(operands.operands.size()));
    }
    return texture.is_integer ? spv_OpBitcast(&ctx.m, ctx.F32[4], texel) : texel;
}

void EmitImageWrite(EmitContext& ctx, IR::Inst* inst, Imm32 handle_imm, Id coords, Id lod, Id ms,
                    Id color) {
    u32 handle = handle_imm;
    Shader::Backend::SPIRV::EmitContext::TextureDefinition const& texture =
        ctx.images[handle & 0xFFFF];
    Id image_ptr = texture.id;
    const Id color_type = texture.data_types->Get(4);
    const Id fixed_coords = FixImageCoords<false>(ctx, coords, texture.view_type);
    ImageOperands operands;
    operands.Add(SpvImageOperandsSampleMask, ms);
    if (ctx.profile.supports_image_load_store_lod) {
        operands.Add(SpvImageOperandsLodMask, lod);
    } else if ((lod != 0)) {
        LOG_WARNING(Render, "Fallback for ImageWrite with LOD");
        ASSERT(texture.mip_fallback_mode == MipStorageFallbackMode::DynamicIndex);
        const Id single_image_ptr_type =
            spv_TypePointer(&ctx.m, u32(SpvStorageClassUniformConstant), texture.image_type);
        image_ptr = spv_OpAccessChain(&ctx.m, single_image_ptr_type, image_ptr, &lod, 1u);
    }
    const Id image = spv_OpLoad(&ctx.m, texture.image_type, image_ptr);
    const Id texel = texture.is_integer ? spv_OpBitcast(&ctx.m, color_type, color) : color;
    spv_OpImageWrite(&ctx.m, image, fixed_coords, texel, u32(operands.mask),
                     operands.operands.data(), u32(operands.operands.size()));
}

static Id SelectCubeResult(EmitContext& ctx, Id x, Id y, Id z, Id x_res, Id y_res, Id z_res) {
    const uint32_t abs_x = spv_OpFAbs(&ctx.m, ctx.F32[1], x);
    const uint32_t abs_y = spv_OpFAbs(&ctx.m, ctx.F32[1], y);
    const uint32_t abs_z = spv_OpFAbs(&ctx.m, ctx.F32[1], z);

    const uint32_t z_face_cond{spv_OpLogicalAnd(
        &ctx.m, ctx.U1[1], spv_OpFOrdGreaterThanEqual(&ctx.m, ctx.U1[1], abs_z, abs_x),
        spv_OpFOrdGreaterThanEqual(&ctx.m, ctx.U1[1], abs_z, abs_y))};
    const uint32_t y_face_cond{spv_OpFOrdGreaterThanEqual(&ctx.m, ctx.U1[1], abs_y, abs_x)};

    return spv_OpSelect(&ctx.m, ctx.F32[1], z_face_cond, z_res,
                        spv_OpSelect(&ctx.m, ctx.F32[1], y_face_cond, y_res, x_res));
}

Id EmitCubeFaceIndex(EmitContext& ctx, IR::Inst* inst, Id x, Id y, Id z) {
    if (ctx.profile.supports_native_cube_calc) {
        return spv_OpCubeFaceIndexAMD(&ctx.m, ctx.F32[1],
                                      spv_OpCompositeConstruct3(&ctx.m, ctx.F32[3], x, y, z));
    }

    const uint32_t x_neg_cond{spv_OpFOrdLessThan(&ctx.m, ctx.U1[1], x, ctx.f32_zero_value)};
    const uint32_t y_neg_cond{spv_OpFOrdLessThan(&ctx.m, ctx.U1[1], y, ctx.f32_zero_value)};
    const uint32_t z_neg_cond{spv_OpFOrdLessThan(&ctx.m, ctx.U1[1], z, ctx.f32_zero_value)};
    const uint32_t x_face{
        spv_OpSelect(&ctx.m, ctx.F32[1], x_neg_cond, ctx.ConstF32(1.f), ctx.ConstF32(0.f))};
    const uint32_t y_face{
        spv_OpSelect(&ctx.m, ctx.F32[1], y_neg_cond, ctx.ConstF32(3.f), ctx.ConstF32(2.f))};
    const uint32_t z_face{
        spv_OpSelect(&ctx.m, ctx.F32[1], z_neg_cond, ctx.ConstF32(5.f), ctx.ConstF32(4.f))};

    return SelectCubeResult(ctx, x, y, z, x_face, y_face, z_face);
}

Id EmitCubeFaceCoordS(EmitContext& ctx, IR::Inst* inst, Id x, Id y, Id z) {
    const uint32_t x_neg_cond{spv_OpFOrdLessThan(&ctx.m, ctx.U1[1], x, ctx.f32_zero_value)};
    const uint32_t z_neg_cond{spv_OpFOrdLessThan(&ctx.m, ctx.U1[1], z, ctx.f32_zero_value)};
    const uint32_t x_sc{
        spv_OpSelect(&ctx.m, ctx.F32[1], x_neg_cond, z, spv_OpFNegate(&ctx.m, ctx.F32[1], z))};
    const unsigned int y_sc{x};
    const uint32_t z_sc{
        spv_OpSelect(&ctx.m, ctx.F32[1], z_neg_cond, spv_OpFNegate(&ctx.m, ctx.F32[1], x), x)};

    return SelectCubeResult(ctx, x, y, z, x_sc, y_sc, z_sc);
}

Id EmitCubeFaceCoordT(EmitContext& ctx, IR::Inst* inst, Id x, Id y, Id z) {
    const uint32_t y_neg_cond{spv_OpFOrdLessThan(&ctx.m, ctx.U1[1], y, ctx.f32_zero_value)};
    const uint32_t x_z_tc{spv_OpFNegate(&ctx.m, ctx.F32[1], y)};
    const uint32_t y_tc{
        spv_OpSelect(&ctx.m, ctx.F32[1], y_neg_cond, spv_OpFNegate(&ctx.m, ctx.F32[1], z), z)};

    return SelectCubeResult(ctx, x, y, z, x_z_tc, y_tc, x_z_tc);
}

Id EmitCubeFaceMajorAxis(EmitContext& ctx, IR::Inst* inst, Id x, Id y, Id z) {
    const unsigned int two{ctx.ConstF32(2.f)};
    const uint32_t x_major_axis{spv_OpFMul(&ctx.m, ctx.F32[1], x, two)};
    const uint32_t y_major_axis{spv_OpFMul(&ctx.m, ctx.F32[1], y, two)};
    const uint32_t z_major_axis{spv_OpFMul(&ctx.m, ctx.F32[1], z, two)};

    return SelectCubeResult(ctx, x, y, z, x_major_axis, y_major_axis, z_major_axis);
}

} // namespace Shader::Backend::SPIRV
