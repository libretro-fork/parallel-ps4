// SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "common/assert.h"
#include "common/div_ceil.h"
#include "shader_recompiler/backend/spirv/spirv_emit_context.h"
#include "shader_recompiler/frontend/fetch_shader.h"
#include "shader_recompiler/ir/attribute.h"
#include "shader_recompiler/ir/microinstruction.h"
#include "shader_recompiler/runtime_info.h"

#include <fmt/format.h>
#include "common/small_vector.h"

#include <numbers>
#include <string_view>

namespace Shader::Backend::SPIRV {
namespace {

static constexpr u32 NumVertices(AmdGpu::PrimitiveType type) {
    switch (type) {
    case AmdGpu::PrimitiveType::PointList:
        return 1u;
    case AmdGpu::PrimitiveType::LineList:
    case AmdGpu::PrimitiveType::LineStrip:
        return 2u;
    case AmdGpu::PrimitiveType::TriangleList:
    case AmdGpu::PrimitiveType::TriangleStrip:
    case AmdGpu::PrimitiveType::RectList:
        return 3u;
    case AmdGpu::PrimitiveType::AdjTriangleList:
        return 6u;
    case AmdGpu::PrimitiveType::AdjLineList:
        return 4u;
    default:
        UNREACHABLE();
    }
}

} // Anonymous namespace

EmitContext::EmitContext(spv_module_t& module, const Profile& profile_,
                         const RuntimeInfo& runtime_info_, Info& info_, Bindings& binding_)
    : m{module}, info{info_}, runtime_info{runtime_info_}, profile{profile_},
      hw_stage{info.hw_stage}, sw_stage{info.sw_stage}, binding{binding_} {
    ASSERT_MSG(spv_module_reset(&m, 4096), "SPIR-V module: out of memory");
    if (info.uses_dma) {
        spv_SetMemoryModel(&m, u32(SpvAddressingModelPhysicalStorageBuffer64),
                           u32(SpvMemoryModelGLSL450));
    } else {
        spv_SetMemoryModel(&m, u32(SpvAddressingModelLogical), u32(SpvMemoryModelGLSL450));
    }
    if (m.names) {
        spv_String(&m, fmt::format("{:#x}", info.pgm_hash).c_str());
    }

    spv_AddCapability(&m, u32(SpvCapabilityShader));
    DefineArithmeticTypes();
    DefineInterfaces();
    DefineSharedMemory();
    DefineBuffers();
    DefineImagesAndSamplers();
    DefineFunctions();
}

Id EmitContext::Def(const IR::Value& value) {
    if (!value.IsImmediate()) {
        return value.Inst()->Definition<Id>();
    }
    switch (value.Type()) {
    case IR::Type::Void:
        return Id{};
    case IR::Type::U1:
        return value.U1() ? true_value : false_value;
    case IR::Type::U32:
        return ConstU32(value.U32());
    case IR::Type::U64:
        return spv_Constant64(&m, U64, u64(value.U64()));
    case IR::Type::F32:
        return ConstF32(value.F32());
    case IR::Type::F64:
        return spv_ConstantF64(&m, F64[1], f64(value.F64()));
    case IR::Type::StringLiteral:
        return spv_String(&m, std::string(value.StringLiteral()).c_str());
    default:
        UNREACHABLE_MSG("Immediate type {}", value.Type());
    }
}

void EmitContext::DefineArithmeticTypes() {
    void_id = spv_Name(&m, spv_TypeVoid(&m), "void_id");
    U1[1] = spv_Name(&m, spv_TypeBool(&m), "bool_id");
    U8 = spv_Name(&m, spv_TypeUInt(&m, 8), "u8_id");
    S8 = spv_Name(&m, spv_TypeSInt(&m, 8), "i8_id");
    U16 = spv_Name(&m, spv_TypeUInt(&m, 16), "u16_id");
    S16 = spv_Name(&m, spv_TypeSInt(&m, 16), "i16_id");
    if (info.uses_fp16) {
        F16[1] = spv_Name(&m, spv_TypeFloat(&m, 16), "f16_id");
    }
    if (info.uses_fp64) {
        F64[1] = spv_Name(&m, spv_TypeFloat(&m, 64), "f64_id");
    }
    F32[1] = spv_Name(&m, spv_TypeFloat(&m, 32), "f32_id");
    S32[1] = spv_Name(&m, spv_TypeSInt(&m, 32), "i32_id");
    U32[1] = spv_Name(&m, spv_TypeUInt(&m, 32), "u32_id");
    U64 = spv_Name(&m, spv_TypeUInt(&m, 64), "u64_id");

    for (u32 i = 2; i <= 4; i++) {
        if (info.uses_fp16) {
            F16[i] = NameFmt(&m, spv_TypeVector(&m, F16[1], u32(i)), "f16vec{}_id", i);
        }
        if (info.uses_fp64) {
            F64[i] = NameFmt(&m, spv_TypeVector(&m, F64[1], u32(i)), "f64vec{}_id", i);
        }
        F32[i] = NameFmt(&m, spv_TypeVector(&m, F32[1], u32(i)), "f32vec{}_id", i);
        S32[i] = NameFmt(&m, spv_TypeVector(&m, S32[1], u32(i)), "i32vec{}_id", i);
        U32[i] = NameFmt(&m, spv_TypeVector(&m, U32[1], u32(i)), "u32vec{}_id", i);
        U1[i] = NameFmt(&m, spv_TypeVector(&m, U1[1], u32(i)), "bvec{}_id", i);
    }

    true_value = spv_ConstantTrue(&m, U1[1]);
    false_value = spv_ConstantFalse(&m, U1[1]);
    u8_one_value = spv_Constant32(&m, U8, u32(1U));
    u8_zero_value = spv_Constant32(&m, U8, u32(0U));
    u16_zero_value = spv_Constant32(&m, U16, u32(0U));
    u32_one_value = ConstU32(1U);
    u32_zero_value = ConstU32(0U);
    f32_one_value = ConstF32(1.f);
    f32_zero_value = ConstF32(0.f);
    u64_one_value = spv_Constant64(&m, U64, u64(1ULL));
    u64_zero_value = spv_Constant64(&m, U64, u64(0ULL));

    pi_x2 = ConstF32(2.0f * float{std::numbers::pi});

    input_f32 = spv_Name(&m, spv_TypePointer(&m, u32(SpvStorageClassInput), F32[1]), "input_f32");
    input_u32 = spv_Name(&m, spv_TypePointer(&m, u32(SpvStorageClassInput), U32[1]), "input_u32");
    input_s32 = spv_Name(&m, spv_TypePointer(&m, u32(SpvStorageClassInput), S32[1]), "input_s32");

    output_f32 =
        spv_Name(&m, spv_TypePointer(&m, u32(SpvStorageClassOutput), F32[1]), "output_f32");
    output_u32 =
        spv_Name(&m, spv_TypePointer(&m, u32(SpvStorageClassOutput), U32[1]), "output_u32");
    output_s32 =
        spv_Name(&m, spv_TypePointer(&m, u32(SpvStorageClassOutput), S32[1]), "output_s32");

    full_result_i32x2 = spv_Name(
        &m, spv_TypeStruct(&m, std::array<u32, 2>{S32[1], S32[1]}.data(), 2), "full_result_i32x2");
    full_result_u32x2 = spv_Name(
        &m, spv_TypeStruct(&m, std::array<u32, 2>{U32[1], U32[1]}.data(), 2), "full_result_u32x2");
    frexp_result_f32 = spv_Name(
        &m, spv_TypeStruct(&m, std::array<u32, 2>{F32[1], S32[1]}.data(), 2), "frexp_result_f32");
    if (info.uses_fp64) {
        frexp_result_f64 =
            spv_Name(&m, spv_TypeStruct(&m, std::array<u32, 2>{F64[1], S32[1]}.data(), 2),
                     "frexp_result_f64");
    }
    if (info.uses_dma) {
        physical_pointer_type_u32 =
            spv_TypePointer(&m, u32(SpvStorageClassPhysicalStorageBuffer), U32[1]);
    }
}

void EmitContext::DefineInterfaces() {
    DefinePushDataBlock();
    DefineInputs();
    DefineOutputs();
}

const VectorIds& GetAttributeType(EmitContext& ctx, AmdGpu::NumberFormat fmt) {
    switch (GetNumberClass(fmt)) {
    case AmdGpu::NumberClass::Float:
        return ctx.F32;
    case AmdGpu::NumberClass::Sint:
        return ctx.S32;
    case AmdGpu::NumberClass::Uint:
        return ctx.U32;
    default:
        break;
    }
    UNREACHABLE_MSG("Invalid attribute type {}", fmt);
}

EmitContext::SpirvAttribute EmitContext::GetAttributeInfo(AmdGpu::NumberFormat fmt, Id id,
                                                          u32 num_components, bool output,
                                                          bool loaded, bool array) {
    switch (GetNumberClass(fmt)) {
    case AmdGpu::NumberClass::Float:
        return {id, output ? output_f32 : input_f32, F32[1], num_components, false, loaded, array};
    case AmdGpu::NumberClass::Uint:
        return {id, output ? output_u32 : input_u32, U32[1], num_components, true, loaded, array};
    case AmdGpu::NumberClass::Sint:
        return {id, output ? output_s32 : input_s32, S32[1], num_components, true, loaded, array};
    default:
        break;
    }
    UNREACHABLE_MSG("Invalid attribute type {}", fmt);
}

Id EmitContext::GetBufferSize(const u32 sharp_idx) {
    // Can this be done with memory access? Like we do now with ReadConst
    const auto& srt_flatbuf = buffers[flatbuf_index];
    ASSERT(srt_flatbuf.buffer_type == BufferType::Flatbuf);
    const auto [id, pointer_type] = srt_flatbuf.Alias(PointerType::U32);

    const auto rsrc1{spv_OpLoad(
        &m, U32[1],
        spv_OpAccessChain2(&m, pointer_type, id, u32_zero_value, ConstU32(sharp_idx + 1)))};
    const auto rsrc2{spv_OpLoad(
        &m, U32[1],
        spv_OpAccessChain2(&m, pointer_type, id, u32_zero_value, ConstU32(sharp_idx + 2)))};

    const auto stride{spv_OpBitFieldUExtract(&m, U32[1], rsrc1, ConstU32(16u), ConstU32(14u))};
    const auto num_records{rsrc2};

    const auto stride_zero{spv_OpIEqual(&m, U1[1], stride, u32_zero_value)};
    const auto stride_size{spv_OpIMul(&m, U32[1], num_records, stride)};
    return spv_OpSelect(&m, U32[1], stride_zero, num_records, stride_size);
}

void EmitContext::DefineBufferProperties() {
    if (!profile.needs_buffer_offsets) {
        return;
    }
    for (u32 i = 0; i < buffers.size(); i++) {
        auto& buffer = buffers[i];
        const auto& desc = info.buffers[i];
        const u32 binding = buffer.binding;
        if (buffer.buffer_type != BufferType::Guest) {
            continue;
        }

        const u32 half = PushData::BufOffsetIndex + (binding >> 4);
        const u32 comp = (binding & 0xf) >> 2;
        const u32 offset = (binding & 0x3) << 3;
        const Id ptr{
            spv_OpAccessChain2(&m, spv_TypePointer(&m, u32(SpvStorageClassPushConstant), U32[1]),
                               push_data_block, ConstU32(half), ConstU32(comp))};
        const Id value{spv_OpLoad(&m, U32[1], ptr)};

        const Id buf_offset{
            spv_OpBitFieldUExtract(&m, U32[1], value, ConstU32(offset), ConstU32(8U))};
        NameFmt(&m, buf_offset, "buf{}_off", binding);
        buffer.Offset(PointerSize::B8) = buf_offset;

        if (True(desc.used_types & IR::Type::U16)) {
            const Id buf_word_offset{spv_OpShiftRightLogical(&m, U32[1], buf_offset, ConstU32(1U))};
            NameFmt(&m, buf_word_offset, "buf{}_word_off", binding);
            buffer.Offset(PointerSize::B16) = buf_word_offset;
        }
        if (True(desc.used_types & IR::Type::U32)) {
            const Id buf_dword_offset{
                spv_OpShiftRightLogical(&m, U32[1], buf_offset, ConstU32(2U))};
            NameFmt(&m, buf_dword_offset, "buf{}_dword_off", binding);
            buffer.Offset(PointerSize::B32) = buf_dword_offset;
        }
        if (True(desc.used_types & IR::Type::U64)) {
            const Id buf_qword_offset{
                spv_OpShiftRightLogical(&m, U32[1], buf_offset, ConstU32(3U))};
            NameFmt(&m, buf_qword_offset, "buf{}_qword_off", binding);
            buffer.Offset(PointerSize::B64) = buf_qword_offset;
        }
    }
}

void EmitContext::DefineAmdPerVertexAttribs() {
    if (!profile.supports_amd_shader_explicit_vertex_parameter) {
        return;
    }
    for (s32 i = 0; i < runtime_info.hw.fs.num_inputs; i++) {
        const auto& input = runtime_info.hw.fs.inputs[i];
        if (input.IsDefault() || info.fs_interpolation[i].primary != Qualifier::PerVertex) {
            continue;
        }
        auto& param = input_params[i];
        const Id pointer = param.id;
        param.id_array[0] =
            spv_OpInterpolateAtVertexAMD(&m, F32[param.num_components], pointer, ConstU32(0U));
        param.id_array[1] =
            spv_OpInterpolateAtVertexAMD(&m, F32[param.num_components], pointer, ConstU32(1U));
        param.id_array[2] =
            spv_OpInterpolateAtVertexAMD(&m, F32[param.num_components], pointer, ConstU32(2U));
        param.is_loaded = true;
    }
}

void EmitContext::DefineWorkgroupIndex() {
    const Id workgroup_id_val{spv_OpLoad(&m, U32[3], workgroup_id)};
    const Id workgroup_x{spv_OpCompositeExtract1(&m, U32[1], workgroup_id_val, 0)};
    const Id workgroup_y{spv_OpCompositeExtract1(&m, U32[1], workgroup_id_val, 1)};
    const Id workgroup_z{spv_OpCompositeExtract1(&m, U32[1], workgroup_id_val, 2)};
    const Id num_workgroups{spv_OpLoad(&m, U32[3], num_workgroups_id)};
    const Id num_workgroups_x{spv_OpCompositeExtract1(&m, U32[1], num_workgroups, 0)};
    const Id num_workgroups_y{spv_OpCompositeExtract1(&m, U32[1], num_workgroups, 1)};
    workgroup_index_id = spv_OpIAdd(
        &m, U32[1],
        spv_OpIAdd(&m, U32[1], workgroup_x, spv_OpIMul(&m, U32[1], workgroup_y, num_workgroups_x)),
        spv_OpIMul(&m, U32[1], workgroup_z,
                   spv_OpIMul(&m, U32[1], num_workgroups_x, num_workgroups_y)));
    spv_Name(&m, workgroup_index_id, "workgroup_index");
}

void EmitContext::DefineInputs() {
    if (info.uses_lane_id) {
        subgroup_local_invocation_id =
            DefineVariable(U32[1], SpvBuiltInSubgroupLocalInvocationId, SpvStorageClassInput);
        // Flat is for fragment inputs only (VUID-StandaloneSpirv-Flat-06202)
        if (hw_stage == HwStage::Fragment) {
            spv_Decorate0(&m, subgroup_local_invocation_id, u32(SpvDecorationFlat));
        }
    }
    if (info.loads.GetAny(IR::Attribute::SubgroupLtMask)) {
        subgroup_lt_mask = DefineVariable(U32[4], SpvBuiltInSubgroupLtMask, SpvStorageClassInput);
        if (hw_stage == HwStage::Fragment) {
            spv_Decorate0(&m, subgroup_lt_mask, u32(SpvDecorationFlat));
        }
    }
    switch (sw_stage) {
    case SwStage::Vertex: {
        vertex_index = DefineVariable(U32[1], SpvBuiltInVertexIndex, SpvStorageClassInput);
        instance_id = DefineVariable(U32[1], SpvBuiltInInstanceIndex, SpvStorageClassInput);
        if (info.loads.Get(IR::Attribute::BaseVertex)) {
            base_vertex = DefineVariable(U32[1], SpvBuiltInBaseVertex, SpvStorageClassInput);
        }
        if (info.loads.Get(IR::Attribute::BaseInstance)) {
            base_instance = DefineVariable(U32[1], SpvBuiltInBaseInstance, SpvStorageClassInput);
        }

        Shader::Gcn::FetchShaderData fetch_shader;
        if (!Gcn::ParseFetchShader(info, fetch_shader)) {
            break;
        }
        ASSERT(fetch_shader.attributes.size() <= IR::NumParams);
        for (u32 semantic = 0; semantic < fetch_shader.attributes.size(); ++semantic) {
            const auto& attrib = fetch_shader.attributes[semantic];
            const auto sharp = attrib.GetSharp(info);
            const Id type{GetAttributeType(*this, sharp.GetNumberFmt())[4]};
            Id id{DefineInput(type, semantic)};
            if (attrib.GetStepRate() != Gcn::VertexAttribute::InstanceIdType::None) {
                NameFmt(&m, id, "vs_instance_attr{}", semantic);
            } else {
                NameFmt(&m, id, "vs_in_attr{}", semantic);
            }
            input_params[semantic] = GetAttributeInfo(sharp.GetNumberFmt(), id, 4, false);
        }
        break;
    }
    case SwStage::Fragment: {
        if (info.loads.GetAny(IR::Attribute::FragCoord) ||
            (info.loads.GetAny(IR::Attribute::BaryCoordPullModel) &&
             !profile.supports_amd_shader_explicit_vertex_parameter)) {
            frag_coord = DefineVariable(F32[4], SpvBuiltInFragCoord, SpvStorageClassInput);
        }
        if (info.loads.Get(IR::Attribute::IsFrontFace)) {
            front_facing = DefineVariable(U1[1], SpvBuiltInFrontFacing, SpvStorageClassInput);
        }
        if (info.loads.GetAny(IR::Attribute::RenderTargetIndex)) {
            output_layer = DefineVariable(U32[1], SpvBuiltInLayer, SpvStorageClassInput);
            spv_Decorate0(&m, output_layer, u32(SpvDecorationFlat));
        }
        if (info.loads.Get(IR::Attribute::SampleIndex)) {
            sample_index = DefineVariable(U32[1], SpvBuiltInSampleId, SpvStorageClassInput);
            spv_Decorate0(&m, sample_index, u32(SpvDecorationFlat));
        }
        if (info.loads.Get(IR::Attribute::IsHelperInvocation)) {
            helper_invocation =
                DefineVariable(U1[1], SpvBuiltInHelperInvocation, SpvStorageClassInput);
        }
        if (info.loads.Get(IR::Attribute::SampleMask)) {
            sample_mask_in = DefineVariable(spv_TypeArray(&m, U32[1], u32_one_value),
                                            SpvBuiltInSampleMask, SpvStorageClassInput);
        }
        if (info.loads.GetAny(IR::Attribute::BaryCoordSmooth)) {
            if (profile.supports_amd_shader_explicit_vertex_parameter) {
                bary_coord_smooth =
                    DefineVariable(F32[2], SpvBuiltInBaryCoordSmoothAMD, SpvStorageClassInput);
            } else if (profile.supports_fragment_shader_barycentric && !(bary_coord != 0)) {
                bary_coord = DefineVariable(F32[3], SpvBuiltInBaryCoordKHR, SpvStorageClassInput);
            }
        }
        if (info.loads.GetAny(IR::Attribute::BaryCoordPullModel)) {
            if (profile.supports_amd_shader_explicit_vertex_parameter) {
                bary_coord_pull_model =
                    DefineVariable(F32[3], SpvBuiltInBaryCoordPullModelAMD, SpvStorageClassInput);
            } else if (profile.supports_fragment_shader_barycentric && !(bary_coord != 0)) {
                bary_coord = DefineVariable(F32[3], SpvBuiltInBaryCoordKHR, SpvStorageClassInput);
            }
        }
        if (info.loads.GetAny(IR::Attribute::BaryCoordSmoothCentroid)) {
            if (profile.supports_amd_shader_explicit_vertex_parameter) {
                bary_coord_smooth_centroid = DefineVariable(
                    F32[2], SpvBuiltInBaryCoordSmoothCentroidAMD, SpvStorageClassInput);
            } else if (profile.supports_fragment_shader_barycentric && !(bary_coord != 0)) {
                bary_coord = DefineVariable(F32[3], SpvBuiltInBaryCoordKHR, SpvStorageClassInput);
            }
        }
        if (info.loads.GetAny(IR::Attribute::BaryCoordSmoothSample)) {
            if (profile.supports_amd_shader_explicit_vertex_parameter) {
                bary_coord_smooth_sample = DefineVariable(
                    F32[2], SpvBuiltInBaryCoordSmoothSampleAMD, SpvStorageClassInput);
            } else if (profile.supports_fragment_shader_barycentric) {
                if (!(bary_coord != 0)) {
                    bary_coord =
                        DefineVariable(F32[3], SpvBuiltInBaryCoordKHR, SpvStorageClassInput);
                }
                // we would need sample_index to interpolate the bary_coord later
                if (!(sample_index != 0)) {
                    sample_index = DefineVariable(U32[1], SpvBuiltInSampleId, SpvStorageClassInput);
                    spv_Decorate0(&m, sample_index, u32(SpvDecorationFlat));
                }
            }
        }
        if (info.loads.GetAny(IR::Attribute::BaryCoordNoPersp)) {
            if (profile.supports_amd_shader_explicit_vertex_parameter) {
                bary_coord_nopersp =
                    DefineVariable(F32[2], SpvBuiltInBaryCoordNoPerspAMD, SpvStorageClassInput);
            } else if (profile.supports_fragment_shader_barycentric) {
                bary_coord_nopersp =
                    DefineVariable(F32[3], SpvBuiltInBaryCoordNoPerspKHR, SpvStorageClassInput);
            }
        }
        if (info.loads.GetAny(IR::Attribute::BaryCoordNoPerspSample)) {
            if (profile.supports_amd_shader_explicit_vertex_parameter) {
                bary_coord_nopersp_sample = DefineVariable(
                    F32[2], SpvBuiltInBaryCoordNoPerspSampleAMD, SpvStorageClassInput);
            } else if (profile.supports_fragment_shader_barycentric) {
                bary_coord_nopersp_sample =
                    DefineVariable(F32[3], SpvBuiltInBaryCoordNoPerspKHR, SpvStorageClassInput);
                // spv_Decorate0(&m, bary_coord_nopersp_sample, u32(SpvDecorationSample));
            }
        }

        const bool has_clip_distance_inputs = runtime_info.hw.fs.clip_distance_emulation;
        // Clip distances attribute vector is the last in inputs array
        const auto num_inputs = runtime_info.hw.fs.num_inputs - (has_clip_distance_inputs ? 1 : 0);

        for (s32 i = 0; i < num_inputs; i++) {
            const auto& input = runtime_info.hw.fs.inputs[i];
            if (input.IsDefault()) {
                continue;
            }
            const IR::Attribute param = IR::Attribute::Param0 + i;
            const u32 num_components = info.loads.NumComponents(param);
            const auto [primary, auxiliary] = info.fs_interpolation[i];
            const Id type = F32[num_components];
            const Id attr_id = [&] {
                const auto bind_location = input.param_index + (has_clip_distance_inputs ? 1 : 0);
                if (primary == Qualifier::PerVertex &&
                    profile.supports_fragment_shader_barycentric) {
                    return NameFmt(
                        &m, DefineInput(spv_TypeArray(&m, type, ConstU32(3U)), bind_location),
                        "fs_in_attr{}_p", i);
                }
                return NameFmt(&m, DefineInput(type, bind_location), "fs_in_attr{}", i);
            }();
            if (primary == Qualifier::PerVertex) {
                spv_Decorate0(&m, attr_id,
                              u32(profile.supports_amd_shader_explicit_vertex_parameter
                                      ? SpvDecorationExplicitInterpAMD
                                      : SpvDecorationPerVertexKHR));
            } else if (primary != Qualifier::Smooth) {
                spv_Decorate0(&m, attr_id,
                              u32(primary == Qualifier::Flat ? SpvDecorationFlat
                                                             : SpvDecorationNoPerspective));
            }
            if (auxiliary != Qualifier::None) {
                spv_Decorate0(&m, attr_id,
                              u32(auxiliary == Qualifier::Centroid ? SpvDecorationCentroid
                                                                   : SpvDecorationSample));
            }
            input_params[i] = GetAttributeInfo(AmdGpu::NumberFormat::Float, attr_id, num_components,
                                               false, false, primary == Qualifier::PerVertex);
        }

        if (has_clip_distance_inputs) {
            const auto type = F32[MaxEmulatedClipDistances];
            const auto attr_id = NameFmt(&m, DefineInput(type, 0), "cldist_attr{}", 0);
            input_params[num_inputs] = GetAttributeInfo(AmdGpu::NumberFormat::Float, attr_id,
                                                        MaxEmulatedClipDistances, false);
        }
        break;
    }
    case SwStage::Compute:
        if (info.loads.GetAny(IR::Attribute::WorkgroupIndex) ||
            info.loads.GetAny(IR::Attribute::WorkgroupId)) {
            workgroup_id = DefineVariable(U32[3], SpvBuiltInWorkgroupId, SpvStorageClassInput);
        }
        if (info.loads.GetAny(IR::Attribute::WorkgroupIndex)) {
            num_workgroups_id =
                DefineVariable(U32[3], SpvBuiltInNumWorkgroups, SpvStorageClassInput);
        }
        if (info.loads.GetAny(IR::Attribute::LocalInvocationId)) {
            local_invocation_id =
                DefineVariable(U32[3], SpvBuiltInLocalInvocationId, SpvStorageClassInput);
        }
        if (info.loads.Get(IR::Attribute::LocalInvocationIndex)) {
            local_invocation_index =
                DefineVariable(U32[1], SpvBuiltInLocalInvocationIndex, SpvStorageClassInput);
        }
        break;
    case SwStage::Geometry: {
        primitive_id = DefineVariable(U32[1], SpvBuiltInPrimitiveId, SpvStorageClassInput);
        const auto gl_per_vertex = spv_Name(
            &m,
            spv_TypeStruct(
                &m,
                std::array<u32, 3>{F32[4], F32[1], spv_TypeArray(&m, F32[1], ConstU32(1u))}.data(),
                3),
            "gl_PerVertex");
        spv_MemberName(&m, gl_per_vertex, 0, "gl_Position");
        spv_MemberName(&m, gl_per_vertex, 1, "gl_PointSize");
        spv_MemberName(&m, gl_per_vertex, 2, "gl_ClipDistance");
        spv_MemberDecorate1(&m, gl_per_vertex, 0, u32(SpvDecorationBuiltIn),
                            u32(static_cast<u32>(SpvBuiltInPosition)));
        spv_MemberDecorate1(&m, gl_per_vertex, 1, u32(SpvDecorationBuiltIn),
                            u32(static_cast<u32>(SpvBuiltInPointSize)));
        spv_MemberDecorate1(&m, gl_per_vertex, 2, u32(SpvDecorationBuiltIn),
                            u32(static_cast<u32>(SpvBuiltInClipDistance)));
        spv_Decorate0(&m, gl_per_vertex, u32(SpvDecorationBlock));
        const auto num_verts_in = NumVertices(runtime_info.hw.gs.in_primitive);
        const auto vertices_in = spv_TypeArray(&m, gl_per_vertex, ConstU32(num_verts_in));
        gl_in = spv_Name(&m, DefineVar(vertices_in, SpvStorageClassInput), "gl_in");
        interfaces.push_back(gl_in);

        const auto num_params = runtime_info.hw.gs.in_vertex_data_size / 4 - 1u;
        for (int param_id = 0; param_id < num_params; ++param_id) {
            const Id type{spv_TypeArray(&m, F32[4], ConstU32(num_verts_in))};
            const Id id{DefineInput(type, param_id)};
            NameFmt(&m, id, "gs_in_attr{}", param_id);
            input_params[param_id] =
                GetAttributeInfo(AmdGpu::NumberFormat::Float, id, 4, false, false, true);
        }
        break;
    }
    case SwStage::TessellationControl: {
        invocation_id = DefineVariable(U32[1], SpvBuiltInInvocationId, SpvStorageClassInput);
        patch_vertices = DefineVariable(U32[1], SpvBuiltInPatchVertices, SpvStorageClassInput);
        primitive_id = DefineVariable(U32[1], SpvBuiltInPrimitiveId, SpvStorageClassInput);

        const u32 num_attrs = Common::AlignUp(runtime_info.sw.tcs.ls_stride, 16) >> 4;
        if (num_attrs > 0) {
            const Id per_vertex_type{spv_TypeArray(&m, F32[4], ConstU32(num_attrs))};
            // The input vertex count isn't statically known, so make length 32 (what
            // glslang does)
            const Id patch_array_type{spv_TypeArray(&m, per_vertex_type, ConstU32(32u))};
            input_attr_array = DefineInput(patch_array_type, 0);
            spv_Name(&m, input_attr_array, "in_attrs");
        }
        break;
    }
    case SwStage::TessellationEval: {
        tess_coord = DefineInput(F32[3], std::nullopt, SpvBuiltInTessCoord);
        primitive_id = DefineVariable(U32[1], SpvBuiltInPrimitiveId, SpvStorageClassInput);

        const u32 num_attrs = Common::AlignUp(runtime_info.sw.tes.hs_output_cp_stride, 16) >> 4;
        if (num_attrs > 0) {
            const Id per_vertex_type{spv_TypeArray(&m, F32[4], ConstU32(num_attrs))};
            // The input vertex count isn't statically known, so make length 32 (what
            // glslang does)
            const Id patch_array_type{spv_TypeArray(&m, per_vertex_type, ConstU32(32u))};
            input_attr_array = DefineInput(patch_array_type, 0);
            spv_Name(&m, input_attr_array, "in_attrs");
        }

        const u32 patch_base_location = num_attrs;
        for (size_t index = 0; index < 30; ++index) {
            if (!(info.uses_patches & (1U << index))) {
                continue;
            }
            const Id id{DefineInput(F32[4], patch_base_location + index)};
            spv_Decorate0(&m, id, u32(SpvDecorationPatch));
            NameFmt(&m, id, "patch_in{}", index);
            patches[index] = id;
        }
        break;
    }
    default:
        break;
    }
}

void EmitContext::DefineVertexBlock() {
    const std::array<Id, 8> zero{f32_zero_value, f32_zero_value, f32_zero_value, f32_zero_value,
                                 f32_zero_value, f32_zero_value, f32_zero_value, f32_zero_value};
    output_position = DefineVariable(F32[4], SpvBuiltInPosition, SpvStorageClassOutput);
    const bool needs_clip_distance_emulation = sw_stage == SwStage::Vertex &&
                                               hw_stage == HwStage::Vertex &&
                                               profile.needs_clip_distance_emulation;
    const auto has_clip_distance_outputs = info.stores.GetAny(IR::Attribute::ClipDistance);
    if (has_clip_distance_outputs && !needs_clip_distance_emulation) {
        const Id type{spv_TypeArray(&m, F32[1], ConstU32(8U))};
        const Id initializer{spv_ConstantComposite(&m, type, zero.data(), u32(zero.size()))};
        clip_distances =
            DefineVariable(type, SpvBuiltInClipDistance, SpvStorageClassOutput, initializer);
    }
    if (info.stores.GetAny(IR::Attribute::CullDistance)) {
        const Id type{spv_TypeArray(&m, F32[1], ConstU32(8U))};
        const Id initializer{spv_ConstantComposite(&m, type, zero.data(), u32(zero.size()))};
        cull_distances =
            DefineVariable(type, SpvBuiltInCullDistance, SpvStorageClassOutput, initializer);
    }
    if (info.stores.GetAny(IR::Attribute::PointSize)) {
        output_point_size = DefineVariable(F32[1], SpvBuiltInPointSize, SpvStorageClassOutput);
    }
    if (info.stores.GetAny(IR::Attribute::RenderTargetIndex)) {
        output_layer = DefineVariable(U32[1], SpvBuiltInLayer, SpvStorageClassOutput);
    }
    if (info.stores.GetAny(IR::Attribute::ViewportIndex)) {
        output_viewport_index =
            DefineVariable(U32[1], SpvBuiltInViewportIndex, SpvStorageClassOutput);
    }
}

void EmitContext::DefineOutputs() {
    switch (sw_stage) {
    case SwStage::Vertex: {
        DefineVertexBlock();
        if (hw_stage == Shader::HwStage::Local) {
            const u32 num_attrs = Common::AlignUp(runtime_info.hw.ls.ls_stride, 16) >> 4;
            if (num_attrs > 0) {
                const Id type{spv_TypeArray(&m, F32[4], ConstU32(num_attrs))};
                output_attr_array = DefineOutput(type, 0);
                spv_Name(&m, output_attr_array, "out_attrs");
            }
        } else {
            const bool needs_clip_distance_emulation =
                hw_stage == HwStage::Vertex && profile.needs_clip_distance_emulation &&
                info.stores.GetAny(IR::Attribute::ClipDistance);
            u32 num_attrs = 0u;
            for (u32 i = 0; i < IR::NumParams; i++) {
                const IR::Attribute param{IR::Attribute::Param0 + i};
                if (!info.stores.GetAny(param)) {
                    continue;
                }
                const u32 num_components = info.stores.NumComponents(param);
                const Id id{
                    DefineOutput(F32[num_components], i + (needs_clip_distance_emulation ? 1 : 0))};
                NameFmt(&m, id, "out_attr{}", i);
                output_params[i] =
                    GetAttributeInfo(AmdGpu::NumberFormat::Float, id, num_components, true);
                ++num_attrs;
            }

            if (needs_clip_distance_emulation) {
                clip_distances = Id{DefineOutput(F32[MaxEmulatedClipDistances], 0)};
                output_params[num_attrs] = GetAttributeInfo(
                    AmdGpu::NumberFormat::Float, clip_distances, MaxEmulatedClipDistances, true);
                NameFmt(&m, clip_distances, "cldist_attr{}", 0);
            }
        }
        break;
    }
    case SwStage::TessellationControl: {
        if (info.stores_tess_level_outer) {
            const Id type{spv_TypeArray(&m, F32[1], ConstU32(4U))};
            output_tess_level_outer = DefineOutput(type, std::nullopt, SpvBuiltInTessLevelOuter);
            spv_Decorate0(&m, output_tess_level_outer, u32(SpvDecorationPatch));
        }
        if (info.stores_tess_level_inner) {
            const Id type{spv_TypeArray(&m, F32[1], ConstU32(2U))};
            output_tess_level_inner = DefineOutput(type, std::nullopt, SpvBuiltInTessLevelInner);
            spv_Decorate0(&m, output_tess_level_inner, u32(SpvDecorationPatch));
        }

        const u32 num_attrs = Common::AlignUp(runtime_info.sw.tcs.hs_output_cp_stride, 16) >> 4;
        if (num_attrs > 0) {
            const Id per_vertex_type{spv_TypeArray(&m, F32[4], ConstU32(num_attrs))};
            // The input vertex count isn't statically known, so make length 32 (what
            // glslang does)
            const Id patch_array_type{spv_TypeArray(
                &m, per_vertex_type, ConstU32(runtime_info.sw.tcs.NumOutputControlPoints()))};
            output_attr_array = DefineOutput(patch_array_type, 0);
            spv_Name(&m, output_attr_array, "out_attrs");
        }

        const u32 patch_base_location = num_attrs;
        for (size_t index = 0; index < 30; ++index) {
            if (!(info.uses_patches & (1U << index))) {
                continue;
            }
            const Id id{DefineOutput(F32[4], patch_base_location + index)};
            spv_Decorate0(&m, id, u32(SpvDecorationPatch));
            NameFmt(&m, id, "patch_out{}", index);
            patches[index] = id;
        }
        break;
    }
    case SwStage::TessellationEval: {
        DefineVertexBlock();
        for (u32 i = 0; i < IR::NumParams; i++) {
            const IR::Attribute param{IR::Attribute::Param0 + i};
            if (!info.stores.GetAny(param)) {
                continue;
            }
            const u32 num_components = info.stores.NumComponents(param);
            const Id id{DefineOutput(F32[num_components], i)};
            NameFmt(&m, id, "out_attr{}", i);
            output_params[i] =
                GetAttributeInfo(AmdGpu::NumberFormat::Float, id, num_components, true);
        }
        break;
    }
    case SwStage::Fragment: {
        if (info.stores.Get(IR::Attribute::Depth)) {
            frag_depth = DefineVariable(F32[1], SpvBuiltInFragDepth, SpvStorageClassOutput);
        }
        if (info.stores.Get(IR::Attribute::SampleMask)) {
            sample_mask = DefineVariable(spv_TypeArray(&m, U32[1], u32_one_value),
                                         SpvBuiltInSampleMask, SpvStorageClassOutput);
        }
        if (info.stores.Get(IR::Attribute::StencilRef) && profile.supports_shader_stencil_export) {
            stencil_ref =
                DefineVariable(S32[1], SpvBuiltInFragStencilRefEXT, SpvStorageClassOutput);
        }
        u32 num_render_targets = 0;
        for (u32 i = 0; i < IR::NumRenderTargets; i++) {
            const IR::Attribute mrt{IR::Attribute::RenderTarget0 + i};
            if (!info.stores.GetAny(mrt)) {
                continue;
            }
            const u32 num_components = info.stores.NumComponents(mrt);
            const AmdGpu::NumberFormat num_format{runtime_info.hw.fs.color_buffers[i].num_format};
            const Id type{GetAttributeType(*this, num_format)[num_components]};
            Id id;
            if (runtime_info.hw.fs.dual_source_blending) {
                id = DefineOutput(type, 0);
                spv_Decorate1(&m, id, u32(SpvDecorationIndex), u32(i));
            } else {
                id = DefineOutput(type, i);
            }
            NameFmt(&m, id, "frag_color{}", i);
            frag_outputs[i] = GetAttributeInfo(num_format, id, num_components, true);
            ++num_render_targets;
        }
        // Dual source blending allows at most 2 render targets, one for each source.
        // Fewer targets are allowed but the missing blending source values will be
        // undefined.
        ASSERT_MSG(!runtime_info.hw.fs.dual_source_blending || num_render_targets <= 2,
                   "Dual source blending enabled, there must be at most two MRT exports");
        break;
    }
    case SwStage::Geometry: {
        DefineVertexBlock();
        for (u32 attr_id = 0; attr_id < info.gs_copy_data.num_attrs; attr_id++) {
            const Id id{DefineOutput(F32[4], attr_id)};
            NameFmt(&m, id, "out_attr{}", attr_id);
            output_params[attr_id] = GetAttributeInfo(AmdGpu::NumberFormat::Float, id, 4, true);
        }
        break;
    }
    case SwStage::Compute:
        break;
    default:
        UNREACHABLE();
    }
}

void EmitContext::DefinePushDataBlock() {
    // Create push constants block for instance steps rates
    const Id struct_type{spv_Name(
        &m,
        spv_TypeStruct(
            &m, std::array<u32, 7>{F32[1], F32[1], F32[1], F32[1], U32[4], U32[4], U32[2]}.data(),
            7),
        "AuxData")};
    spv_Decorate0(&m, struct_type, u32(SpvDecorationBlock));
    spv_MemberName(&m, struct_type, u32(PushData::XOffsetIndex), "xoffset");
    spv_MemberName(&m, struct_type, u32(PushData::YOffsetIndex), "yoffset");
    spv_MemberName(&m, struct_type, u32(PushData::XScaleIndex), "xscale");
    spv_MemberName(&m, struct_type, u32(PushData::YScaleIndex), "yscale");
    spv_MemberName(&m, struct_type, u32(PushData::BufOffsetIndex + 0), "buf_offsets0");
    spv_MemberName(&m, struct_type, u32(PushData::BufOffsetIndex + 1), "buf_offsets1");
    spv_MemberName(&m, struct_type, u32(PushData::BufOffsetIndex + 2), "buf_offsets2");
    spv_MemberDecorate1(&m, struct_type, u32(PushData::XOffsetIndex), u32(SpvDecorationOffset), 0U);
    spv_MemberDecorate1(&m, struct_type, u32(PushData::YOffsetIndex), u32(SpvDecorationOffset), 4U);
    spv_MemberDecorate1(&m, struct_type, u32(PushData::XScaleIndex), u32(SpvDecorationOffset), 8U);
    spv_MemberDecorate1(&m, struct_type, u32(PushData::YScaleIndex), u32(SpvDecorationOffset), 12U);
    spv_MemberDecorate1(&m, struct_type, u32(PushData::BufOffsetIndex + 0),
                        u32(SpvDecorationOffset), 16U);
    spv_MemberDecorate1(&m, struct_type, u32(PushData::BufOffsetIndex + 1),
                        u32(SpvDecorationOffset), 32U);
    spv_MemberDecorate1(&m, struct_type, u32(PushData::BufOffsetIndex + 2),
                        u32(SpvDecorationOffset), 48U);
    push_data_block = DefineVar(struct_type, SpvStorageClassPushConstant);
    spv_Name(&m, push_data_block, "push_data");
    interfaces.push_back(push_data_block);
}

EmitContext::BufferSpv EmitContext::DefineBuffer(bool is_written, bool is_coherent, u32 elem_shift,
                                                 BufferType buffer_type, Id data_type) {
    // Define array type.
    const Id record_array_type{spv_TypeRuntimeArray(&m, data_type)};
    // Define block struct type. Don't perform decorations twice on the same Id.
    const Id struct_type{spv_TypeStruct1(&m, record_array_type)};
    if (std::ranges::find(buf_type_ids, record_array_type) == buf_type_ids.end()) {
        spv_Decorate1(&m, record_array_type, u32(SpvDecorationArrayStride), u32(1 << elem_shift));
        spv_Decorate0(&m, struct_type, u32(SpvDecorationBlock));
        spv_MemberName(&m, struct_type, 0, "data");
        spv_MemberDecorate1(&m, struct_type, 0, u32(SpvDecorationOffset), 0U);
        buf_type_ids.push_back(record_array_type);
    }
    // Define buffer binding interface.
    constexpr auto storage_class = SpvStorageClassStorageBuffer;
    const Id struct_pointer_type{spv_TypePointer(&m, u32(storage_class), struct_type)};
    const Id pointer_type = spv_TypePointer(&m, u32(storage_class), data_type);
    const Id id{spv_AddGlobalVariable(&m, struct_pointer_type, u32(storage_class), 0)};
    spv_Decorate1(&m, id, u32(SpvDecorationBinding), u32(binding.unified));
    spv_Decorate1(&m, id, u32(SpvDecorationDescriptorSet), 0U);
    if (!is_written) {
        spv_Decorate0(&m, id, u32(SpvDecorationNonWritable));
    } else if (is_coherent) {
        // The guest V# MTYPE marks this buffer as shared between invocations;
        // Coherent forbids caching its accesses in registers across barriers.
        spv_Decorate0(&m, id, u32(SpvDecorationCoherent));
    }
    switch (buffer_type) {
    case BufferType::GdsBuffer:
        spv_Name(&m, id, "gds_buffer");
        break;
    case BufferType::Flatbuf:
        spv_Name(&m, id, "srt_flatbuf");
        break;
    case BufferType::ClipPlanes:
        spv_Name(&m, id, "clip_planes");
        break;
    case BufferType::BdaPagetable:
        spv_Name(&m, id, "bda_pagetable");
        break;
    case BufferType::FaultBuffer:
        spv_Name(&m, id, "fault_buffer");
        break;
    case BufferType::SharedMemory:
        spv_Name(&m, id, "ssbo_shmem");
        break;
    default:
        NameFmt(&m, id, "ssbo_{}", binding.buffer);
        break;
    }
    interfaces.push_back(id);
    return {id, pointer_type};
};

void EmitContext::DefineBuffers() {
    for (const auto& desc : info.buffers) {
        const auto buf_sharp = desc.GetSharp(info);
        // MTYPE class 3 marks guest buffers shared between invocations; shared
        // memory lowered to a storage buffer needs the same guarantee.
        const bool is_coherent =
            desc.buffer_type == BufferType::SharedMemory || buf_sharp.mtype == 3;

        // Set indexes for special buffers.
        if (desc.buffer_type == BufferType::Flatbuf) {
            flatbuf_index = buffers.size();
        } else if (desc.buffer_type == BufferType::BdaPagetable) {
            bda_pagetable_index = buffers.size();
        } else if (desc.buffer_type == BufferType::FaultBuffer) {
            fault_buffer_index = buffers.size();
        }

        // Define aliases depending on the shader usage.
        auto& spv_buffer = buffers.emplace_back(binding.buffer++, desc.buffer_type);
        if (True(desc.used_types & IR::Type::U64)) {
            spv_buffer.Alias(PointerType::U64) =
                DefineBuffer(desc.is_written, is_coherent, 3, desc.buffer_type, U64);
        }
        if (True(desc.used_types & IR::Type::U32)) {
            spv_buffer.Alias(PointerType::U32) =
                DefineBuffer(desc.is_written, is_coherent, 2, desc.buffer_type, U32[1]);
        }
        if (True(desc.used_types & IR::Type::F32)) {
            spv_buffer.Alias(PointerType::F32) =
                DefineBuffer(desc.is_written, is_coherent, 2, desc.buffer_type, F32[1]);
        }
        if (True(desc.used_types & IR::Type::U16)) {
            spv_buffer.Alias(PointerType::U16) =
                DefineBuffer(desc.is_written, is_coherent, 1, desc.buffer_type, U16);
        }
        if (True(desc.used_types & IR::Type::U8)) {
            spv_buffer.Alias(PointerType::U8) =
                DefineBuffer(desc.is_written, is_coherent, 0, desc.buffer_type, U8);
        }
        ++binding.unified;
    }
}

u32 GetFormat(const AmdGpu::Image& image) {
    if (image.GetDataFmt() == AmdGpu::DataFormat::Format32 &&
        image.GetNumberFmt() == AmdGpu::NumberFormat::Uint) {
        return SpvImageFormatR32ui;
    }
    if (image.GetDataFmt() == AmdGpu::DataFormat::Format32 &&
        image.GetNumberFmt() == AmdGpu::NumberFormat::Sint) {
        return SpvImageFormatR32i;
    }
    if (image.GetDataFmt() == AmdGpu::DataFormat::Format32 &&
        image.GetNumberFmt() == AmdGpu::NumberFormat::Float) {
        return SpvImageFormatR32f;
    }
    if (image.GetDataFmt() == AmdGpu::DataFormat::Format32_32 &&
        image.GetNumberFmt() == AmdGpu::NumberFormat::Float) {
        return SpvImageFormatRg32f;
    }
    if (image.GetDataFmt() == AmdGpu::DataFormat::Format32_32 &&
        image.GetNumberFmt() == AmdGpu::NumberFormat::Uint) {
        return SpvImageFormatRg32ui;
    }
    if (image.GetDataFmt() == AmdGpu::DataFormat::Format32_32_32_32 &&
        image.GetNumberFmt() == AmdGpu::NumberFormat::Uint) {
        return SpvImageFormatRgba32ui;
    }
    if (image.GetDataFmt() == AmdGpu::DataFormat::Format16 &&
        image.GetNumberFmt() == AmdGpu::NumberFormat::Float) {
        return SpvImageFormatR16f;
    }
    if (image.GetDataFmt() == AmdGpu::DataFormat::Format16 &&
        image.GetNumberFmt() == AmdGpu::NumberFormat::Uint) {
        return SpvImageFormatR16ui;
    }
    if (image.GetDataFmt() == AmdGpu::DataFormat::Format16_16 &&
        image.GetNumberFmt() == AmdGpu::NumberFormat::Float) {
        return SpvImageFormatRg16f;
    }
    if (image.GetDataFmt() == AmdGpu::DataFormat::Format16_16 &&
        image.GetNumberFmt() == AmdGpu::NumberFormat::Snorm) {
        return SpvImageFormatRg16Snorm;
    }
    if (image.GetDataFmt() == AmdGpu::DataFormat::Format8_8 &&
        image.GetNumberFmt() == AmdGpu::NumberFormat::Unorm) {
        return SpvImageFormatRg8;
    }
    if (image.GetDataFmt() == AmdGpu::DataFormat::Format16_16_16_16 &&
        image.GetNumberFmt() == AmdGpu::NumberFormat::Float) {
        return SpvImageFormatRgba16f;
    }
    if (image.GetDataFmt() == AmdGpu::DataFormat::Format16_16_16_16 &&
        image.GetNumberFmt() == AmdGpu::NumberFormat::Unorm) {
        return SpvImageFormatRgba16;
    }
    if (image.GetDataFmt() == AmdGpu::DataFormat::Format8 &&
        image.GetNumberFmt() == AmdGpu::NumberFormat::Unorm) {
        return SpvImageFormatR8;
    }
    if (image.GetDataFmt() == AmdGpu::DataFormat::Format8_8_8_8 &&
        image.GetNumberFmt() == AmdGpu::NumberFormat::Unorm) {
        return SpvImageFormatRgba8;
    }
    if (image.GetDataFmt() == AmdGpu::DataFormat::Format8_8_8_8 &&
        image.GetNumberFmt() == AmdGpu::NumberFormat::Uint) {
        return SpvImageFormatRgba8ui;
    }
    if (image.GetDataFmt() == AmdGpu::DataFormat::Format10_11_11 &&
        image.GetNumberFmt() == AmdGpu::NumberFormat::Float) {
        return SpvImageFormatR11fG11fB10f;
    }
    if (image.GetDataFmt() == AmdGpu::DataFormat::Format32_32_32_32 &&
        image.GetNumberFmt() == AmdGpu::NumberFormat::Float) {
        return SpvImageFormatRgba32f;
    }
    UNREACHABLE_MSG("Unknown storage format data_format={}, num_format={}", image.GetDataFmt(),
                    image.GetNumberFmt());
}

Id ImageType(EmitContext& ctx, const ImageResource& desc, Id sampled_type) {
    const auto image = desc.GetSharp(ctx.info);
    const auto format = desc.is_atomic ? GetFormat(image) : SpvImageFormatUnknown;
    const auto type = image.GetViewType(desc.is_array);
    const u32 sampled = desc.is_written ? 2 : 1;
    switch (type) {
    case AmdGpu::ImageType::Color1D:
    case AmdGpu::ImageType::Color2D:
        return spv_TypeImage(&ctx.m, sampled_type, u32(SpvDim2D), u32(false), u32(false),
                             u32(false), u32(sampled), u32(format), ~0u);
    case AmdGpu::ImageType::Color1DArray:
    case AmdGpu::ImageType::Color2DArray:
    case AmdGpu::ImageType::Cube:
        return spv_TypeImage(&ctx.m, sampled_type, u32(SpvDim2D), u32(false), u32(true), u32(false),
                             u32(sampled), u32(format), ~0u);
    case AmdGpu::ImageType::Color2DMsaa:
        return spv_TypeImage(&ctx.m, sampled_type, u32(SpvDim2D), u32(false), u32(false), u32(true),
                             u32(sampled), u32(format), ~0u);
    case AmdGpu::ImageType::Color3D:
        return spv_TypeImage(&ctx.m, sampled_type, u32(SpvDim3D), u32(false), u32(false),
                             u32(false), u32(sampled), u32(format), ~0u);
    default:
        break;
    }
    UNREACHABLE_MSG("Invalid texture type {}", type);
}

void EmitContext::DefineImagesAndSamplers() {
    for (const auto& image_desc : info.images) {
        const auto sharp = image_desc.GetSharp(info);
        const auto nfmt = sharp.GetNumberFmt();
        const bool is_integer = AmdGpu::IsInteger(nfmt);
        const bool is_storage = image_desc.is_written;
        const MipStorageFallbackMode mip_fallback_mode = image_desc.mip_fallback_mode;
        const VectorIds& data_types = GetAttributeType(*this, nfmt);
        const Id sampled_type = data_types[1];
        const Id image_type{ImageType(*this, image_desc, sampled_type)};

        const u32 num_bindings = image_desc.NumBindings(info);
        Id pointee_type = image_type;
        if (mip_fallback_mode == MipStorageFallbackMode::DynamicIndex) {
            pointee_type = spv_TypeArray(&m, pointee_type, ConstU32(num_bindings));
        }

        const Id pointer_type{
            spv_TypePointer(&m, u32(SpvStorageClassUniformConstant), pointee_type)};
        const Id id{
            spv_AddGlobalVariable(&m, pointer_type, u32(SpvStorageClassUniformConstant), 0)};
        spv_Decorate1(&m, id, u32(SpvDecorationBinding), u32(binding.unified));
        binding.unified += num_bindings;
        spv_Decorate1(&m, id, u32(SpvDecorationDescriptorSet), 0U);
        NameFmt(&m, id, "{}_{}{}", hw_stage, "img", images.size());
        images.push_back({
            .data_types = &data_types,
            .id = id,
            .sampled_type = is_storage ? sampled_type : spv_TypeSampledImage(&m, image_type),
            .image_type = image_type,
            .view_type = sharp.GetViewType(image_desc.is_array),
            .is_integer = is_integer,
            .is_storage = is_storage,
            .mip_fallback_mode = mip_fallback_mode,
        });
        interfaces.push_back(id);
    }
    if (std::ranges::any_of(info.images,
                            [](const ImageResource& image) { return image.is_atomic; })) {
        image_u32 = spv_TypePointer(&m, u32(SpvStorageClassImage), U32[1]);
        image_f32 = spv_TypePointer(&m, u32(SpvStorageClassImage), F32[1]);
    }
    if (info.samplers.empty()) {
        return;
    }
    sampler_type = spv_TypeSampler(&m);
    sampler_pointer_type = spv_TypePointer(&m, u32(SpvStorageClassUniformConstant), sampler_type);
    for (const auto& samp_desc : info.samplers) {
        const Id id{spv_AddGlobalVariable(&m, sampler_pointer_type,
                                          u32(SpvStorageClassUniformConstant), 0)};
        spv_Decorate1(&m, id, u32(SpvDecorationBinding), u32(binding.unified++));
        spv_Decorate1(&m, id, u32(SpvDecorationDescriptorSet), 0U);
        NameFmt(&m, id, "{}_{}{}", hw_stage, "samp", samplers.size());
        samplers.push_back(id);
        interfaces.push_back(id);
    }
}

void EmitContext::DefineSharedMemory() {
    const auto num_types = std::popcount(static_cast<u32>(info.shared_types));
    if (num_types == 0) {
        return;
    }
    ASSERT(info.hw_stage == HwStage::Compute);
    const u32 shared_memory_size =
        runtime_info.hw.cs.shared_memory_size + info.shared_memory_scratch_size;

    const auto make_type = [&](IR::Type type, Id element_type, u32 element_size,
                               std::string_view name) {
        if (False(info.shared_types & type)) {
            // Skip unused shared memory types.
            return std::make_tuple(Id{}, Id{}, Id{});
        }

        const u32 num_elements{Common::DivCeil(shared_memory_size, element_size)};
        const Id array_type{spv_TypeArray(&m, element_type, ConstU32(num_elements))};

        const auto mem_type = [&] {
            if (num_types > 1) {
                const Id struct_type{spv_TypeStruct1(&m, array_type)};
                spv_Decorate0(&m, struct_type, u32(SpvDecorationBlock));
                spv_MemberDecorate1(&m, struct_type, 0u, u32(SpvDecorationOffset), 0u);
                return struct_type;
            } else {
                return array_type;
            }
        }();

        const Id pointer = spv_TypePointer(&m, u32(SpvStorageClassWorkgroup), mem_type);
        const Id element_pointer = spv_TypePointer(&m, u32(SpvStorageClassWorkgroup), element_type);
        const Id variable = spv_AddGlobalVariable(&m, pointer, u32(SpvStorageClassWorkgroup), 0);
        spv_Name(&m, variable, std::string(name).c_str());
        interfaces.push_back(variable);

        if (num_types > 1) {
            spv_Decorate1(&m, array_type, u32(SpvDecorationArrayStride), u32(element_size));
            spv_Decorate0(&m, variable, u32(SpvDecorationAliased));
        }

        return std::make_tuple(variable, element_pointer, pointer);
    };
    std::tie(shared_memory_u16, shared_u16, shared_memory_u16_type) =
        make_type(IR::Type::U16, U16, 2u, "shared_mem_u16");
    std::tie(shared_memory_u32, shared_u32, shared_memory_u32_type) =
        make_type(IR::Type::U32, U32[1], 4u, "shared_mem_u32");
    std::tie(shared_memory_u64, shared_u64, shared_memory_u64_type) =
        make_type(IR::Type::U64, U64, 8u, "shared_mem_u64");
}

Id EmitContext::DefineFloat32ToUfloatM5(u32 mantissa_bits, const std::string_view name) {
    // https://gitlab.freedesktop.org/mesa/mesa/-/blob/main/src/util/format_r11g11b10f.h
    const auto func_type{spv_TypeFunction(&m, U32[1], std::array<u32, 1>{F32[1]}.data(), 1)};
    const auto func{spv_OpFunction(&m, U32[1], u32(SpvFunctionControlMaskNone), func_type)};
    const auto value{spv_OpFunctionParameter(&m, F32[1])};
    spv_Name(&m, func, std::string(name).c_str());
    AddLabel();

    const auto raw_value{spv_OpBitcast(&m, U32[1], value)};
    const auto exponent{spv_OpBitcast(
        &m, S32[1], spv_OpBitFieldSExtract(&m, U32[1], raw_value, ConstU32(23U), ConstU32(8U)))};
    const auto sign{spv_OpBitFieldUExtract(&m, U32[1], raw_value, ConstU32(31U), ConstU32(1U))};

    const auto is_zero{spv_OpLogicalOr(&m, U1[1], spv_OpIEqual(&m, U1[1], raw_value, ConstU32(0U)),
                                       spv_OpIEqual(&m, U1[1], sign, ConstU32(1U)))};
    const auto is_nan{spv_OpIsNan(&m, U1[1], value)};
    const auto is_inf{spv_OpIsInf(&m, U1[1], value)};
    const auto is_denorm{spv_OpSLessThanEqual(&m, U1[1], exponent, ConstS32(-15))};

    const auto denorm_mantissa{spv_OpConvertFToU(
        &m, U32[1],
        spv_OpRoundEven(&m, F32[1],
                        spv_OpFMul(&m, F32[1], value,
                                   ConstF32(static_cast<float>(1 << (mantissa_bits + 14))))))};
    const auto denorm_overflow{spv_OpINotEqual(
        &m, U1[1], spv_OpShiftRightLogical(&m, U32[1], denorm_mantissa, ConstU32(mantissa_bits)),
        ConstU32(0U))};
    const auto denorm{
        spv_OpSelect(&m, U32[1], denorm_overflow, ConstU32(1U << mantissa_bits), denorm_mantissa)};

    const auto norm_mantissa{spv_OpConvertFToU(
        &m, U32[1],
        spv_OpRoundEven(
            &m, F32[1],
            spv_OpLdexp(
                &m, F32[1], value,
                spv_OpISub(&m, S32[1], ConstS32(static_cast<int>(mantissa_bits)), exponent))))};
    const auto norm_overflow{
        spv_OpUGreaterThanEqual(&m, U1[1], norm_mantissa, ConstU32(2U << mantissa_bits))};
    const auto norm_final_mantissa{spv_OpBitwiseAnd(
        &m, U32[1],
        spv_OpSelect(&m, U32[1], norm_overflow,
                     spv_OpShiftRightLogical(&m, U32[1], norm_mantissa, ConstU32(1U)),
                     norm_mantissa),
        ConstU32((1U << mantissa_bits) - 1))};
    const auto norm_final_exponent{spv_OpBitcast(
        &m, U32[1],
        spv_OpIAdd(&m, S32[1],
                   spv_OpSelect(&m, S32[1], norm_overflow,
                                spv_OpIAdd(&m, S32[1], exponent, ConstS32(1)), exponent),
                   ConstS32(15)))};
    const auto norm{spv_OpBitFieldInsert(&m, U32[1], norm_final_mantissa, norm_final_exponent,
                                         ConstU32(mantissa_bits), ConstU32(5U))};

    const auto result{spv_OpSelect(
        &m, U32[1], is_zero, ConstU32(0U),
        spv_OpSelect(&m, U32[1], is_nan, ConstU32(31u << mantissa_bits | 1U),
                     spv_OpSelect(&m, U32[1], is_inf, ConstU32(31U << mantissa_bits),
                                  spv_OpSelect(&m, U32[1], is_denorm, denorm, norm))))};

    spv_OpReturnValue(&m, result);
    spv_OpFunctionEnd(&m);
    return func;
}

Id EmitContext::DefineUfloatM5ToFloat32(u32 mantissa_bits, const std::string_view name) {
    // https://gitlab.freedesktop.org/mesa/mesa/-/blob/main/src/util/format_r11g11b10f.h
    const auto func_type{spv_TypeFunction(&m, F32[1], std::array<u32, 1>{U32[1]}.data(), 1)};
    const auto func{spv_OpFunction(&m, F32[1], u32(SpvFunctionControlMaskNone), func_type)};
    const auto value{spv_OpFunctionParameter(&m, U32[1])};
    spv_Name(&m, func, std::string(name).c_str());
    AddLabel();

    const Id exponent{
        spv_OpBitFieldUExtract(&m, U32[1], value, ConstU32(mantissa_bits), ConstU32(5U))};
    const Id mantissa{
        spv_OpBitFieldUExtract(&m, U32[1], value, ConstU32(0U), ConstU32(mantissa_bits))};
    const Id mantissa_f{spv_OpConvertUToF(&m, F32[1], mantissa)};
    const Id a{spv_OpSelect(
        &m, F32[1], spv_OpINotEqual(&m, U1[1], mantissa, u32_zero_value),
        spv_OpFMul(&m, F32[1], ConstF32(1.f / (1 << (14 + mantissa_bits))), mantissa_f),
        f32_zero_value)};
    const Id b{
        spv_OpBitcast(&m, F32[1], spv_OpBitwiseOr(&m, U32[1], mantissa, ConstU32(0x7f800000U)))};
    const Id exponent_c{spv_OpISub(&m, U32[1], exponent, ConstU32(15U))};
    const Id scale_a{spv_OpFDiv(
        &m, F32[1], ConstF32(1.f),
        spv_OpConvertUToF(&m, F32[1],
                          spv_OpShiftLeftLogical(&m, U32[1], u32_one_value,
                                                 spv_OpSNegate(&m, U32[1], exponent_c))))};
    const Id scale_b{spv_OpConvertUToF(
        &m, F32[1], spv_OpShiftLeftLogical(&m, U32[1], u32_one_value, exponent_c))};
    const Id scale{spv_OpSelect(&m, F32[1], spv_OpSLessThan(&m, U1[1], exponent_c, u32_zero_value),
                                scale_a, scale_b)};
    const Id c{spv_OpFMul(
        &m, F32[1], scale,
        spv_OpFAdd(&m, F32[1], ConstF32(1.f),
                   spv_OpFDiv(&m, F32[1], mantissa_f, ConstF32(f32(1 << mantissa_bits)))))};
    const Id result{spv_OpSelect(
        &m, F32[1], spv_OpIEqual(&m, U1[1], exponent, u32_zero_value), a,
        spv_OpSelect(&m, F32[1], spv_OpIEqual(&m, U1[1], exponent, ConstU32(31U)), b, c))};
    spv_OpReturnValue(&m, result);
    spv_OpFunctionEnd(&m);
    return func;
}

// The guest's address space as the buffer cache has it (BufferCache::ADDRESS_SPACE_BITS): the
// page table and the fault buffer have a page for every address below this and none above
static constexpr u64 GUEST_ADDRESS_SPACE_BITS = 40;

Id EmitContext::DefineGetBdaPointer() {
    const auto caching_pagebits{
        spv_Constant64(&m, U64, u64(static_cast<u64>(profile.sparse_page_shift)))};
    const auto caching_pagemask{
        spv_Constant64(&m, U64, u64(static_cast<u64>((1u << profile.sparse_page_shift) - 1)))};

    const auto func_type{spv_TypeFunction(&m, U64, std::array<u32, 1>{U64}.data(), 1)};
    const auto func{spv_OpFunction(&m, U64, u32(SpvFunctionControlMaskNone), func_type)};
    const auto address{spv_OpFunctionParameter(&m, U64)};
    spv_Name(&m, func, "get_bda_pointer");
    const auto entry_label{AddLabel()};

    const auto lookup_label{spv_OpLabel(&m)};
    const auto fault_label{spv_OpLabel(&m)};
    const auto available_label{spv_OpLabel(&m)};
    const auto merge_label{spv_OpLabel(&m)};
    const auto end_label{spv_OpLabel(&m)};

    // An address outside the guest's address space (what a shader takes for a pointer need not
    // be one) has no page in the table and no bit in the fault buffer: looking it up read past
    // the table, and marking the fault wrote past the fault buffer, which lost the GPU device
    // ("write to an invalid address"; Sekiro after its intro movie). It reads as nothing.
    const auto in_space{
        spv_OpIEqual(&m, U1[1],
                     spv_OpShiftRightLogical(
                         &m, U64, address, spv_Constant64(&m, U64, u64(GUEST_ADDRESS_SPACE_BITS))),
                     u64_zero_value)};
    spv_OpSelectionMerge(&m, end_label, u32(SpvSelectionControlMaskNone));
    spv_OpBranchConditional(&m, in_space, lookup_label, end_label);
    AddLabel(lookup_label);

    // Get page BDA
    const auto page{spv_OpShiftRightLogical(&m, U64, address, caching_pagebits)};
    const auto page32{spv_OpUConvert(&m, U32[1], page)};
    const auto& bda_buffer{buffers[bda_pagetable_index]};
    const auto [bda_buffer_id, bda_pointer_type] = bda_buffer.Alias(PointerType::U64);
    const auto bda_ptr{
        spv_OpAccessChain2(&m, bda_pointer_type, bda_buffer_id, u32_zero_value, page32)};
    const auto bda{spv_OpLoad(&m, U64, bda_ptr)};

    // Check if page is GPU cached
    const auto is_fault{spv_OpIEqual(&m, U1[1], bda, u64_zero_value)};
    spv_OpSelectionMerge(&m, merge_label, u32(SpvSelectionControlMaskNone));
    spv_OpBranchConditional(&m, is_fault, fault_label, available_label);

    // First time acces, mark as fault
    AddLabel(fault_label);
    const auto& fault_buffer{buffers[fault_buffer_index]};
    const auto [fault_buffer_id, fault_pointer_type] = fault_buffer.Alias(PointerType::U32);
    const auto page_div32{spv_OpShiftRightLogical(&m, U32[1], page32, ConstU32(5U))};
    const auto page_mod32{spv_OpBitwiseAnd(&m, U32[1], page32, ConstU32(31U))};
    const auto page_mask{spv_OpShiftLeftLogical(&m, U32[1], u32_one_value, page_mod32)};
    const auto fault_ptr{
        spv_OpAccessChain2(&m, fault_pointer_type, fault_buffer_id, u32_zero_value, page_div32)};
    const auto fault_value{spv_OpLoad(&m, U32[1], fault_ptr)};
    const auto fault_value_masked{spv_OpBitwiseOr(&m, U32[1], fault_value, page_mask)};
    spv_OpStore(&m, fault_ptr, fault_value_masked);

    // Return null pointer
    const auto fallback_result{u64_zero_value};
    spv_OpBranch(&m, merge_label);

    // Value is available, compute address
    AddLabel(available_label);
    const auto offset_in_bda{spv_OpBitwiseAnd(&m, U64, address, caching_pagemask)};
    const auto addr{spv_OpIAdd(&m, U64, bda, offset_in_bda)};
    spv_OpBranch(&m, merge_label);

    // Merge
    AddLabel(merge_label);
    const auto looked_up{spv_OpPhi(
        &m, U64, std::array<u32, 4>{addr, available_label, fallback_result, fault_label}.data(),
        4)};
    spv_OpBranch(&m, end_label);

    AddLabel(end_label);
    const auto result{spv_OpPhi(
        &m, U64, std::array<u32, 4>{looked_up, merge_label, u64_zero_value, entry_label}.data(),
        4)};
    spv_OpReturnValue(&m, result);
    spv_OpFunctionEnd(&m);
    return func;
}

Id EmitContext::DefineReadConst(bool dynamic) {
    const auto func_type{
        !dynamic
            ? spv_TypeFunction(&m, U32[1], std::array<u32, 3>{U32[2], U32[1], U32[1]}.data(), 3)
            : spv_TypeFunction(&m, U32[1], std::array<u32, 2>{U32[2], U32[1]}.data(), 2)};
    const auto func{spv_OpFunction(&m, U32[1], u32(SpvFunctionControlMaskNone), func_type)};
    const auto base{spv_OpFunctionParameter(&m, U32[2])};
    const auto offset{spv_OpFunctionParameter(&m, U32[1])};
    const auto flatbuf_offset{!dynamic ? spv_OpFunctionParameter(&m, U32[1]) : Id{}};
    spv_Name(&m, func, std::string(dynamic ? "read_const_dynamic" : "read_const").c_str());
    AddLabel();

    const auto base_lo{spv_OpUConvert(&m, U64, spv_OpCompositeExtract1(&m, U32[1], base, 0))};
    const auto base_hi{spv_OpUConvert(&m, U64, spv_OpCompositeExtract1(&m, U32[1], base, 1))};
    const auto base_shift{spv_OpShiftLeftLogical(&m, U64, base_hi, ConstU32(32U))};
    const auto base_addr{spv_OpBitwiseOr(&m, U64, base_lo, base_shift)};
    const auto offset_bytes{spv_OpShiftLeftLogical(&m, U32[1], offset, ConstU32(2U))};
    const auto addr{spv_OpIAdd(&m, U64, base_addr, spv_OpUConvert(&m, U64, offset_bytes))};

    const auto result = EmitDwordMemoryRead(addr, [&]() {
        if (dynamic) {
            return u32_zero_value;
        } else {
            return EmitFlatbufferLoad(flatbuf_offset);
        }
    });

    spv_OpReturnValue(&m, result);
    spv_OpFunctionEnd(&m);
    return func;
}

void EmitContext::DefineFunctions() {
    if (info.uses_pack_10_11_11) {
        f32_to_uf11 = DefineFloat32ToUfloatM5(6, "f32_to_uf11");
        f32_to_uf10 = DefineFloat32ToUfloatM5(5, "f32_to_uf10");
    }
    if (info.uses_unpack_10_11_11) {
        uf11_to_f32 = DefineUfloatM5ToFloat32(6, "uf11_to_f32");
        uf10_to_f32 = DefineUfloatM5ToFloat32(5, "uf10_to_f32");
    }
    if (info.uses_dma) {
        get_bda_pointer = DefineGetBdaPointer();
    }

    if (True(info.readconst_types & Info::ReadConstType::Immediate)) {
        LOG_DEBUG(Render_Recompiler, "Shader {:#x} uses immediate ReadConst", info.pgm_hash);
        read_const = DefineReadConst(false);
    }
    if (True(info.readconst_types & Info::ReadConstType::Dynamic)) {
        LOG_DEBUG(Render_Recompiler, "Shader {:#x} uses dynamic ReadConst", info.pgm_hash);
        read_const_dynamic = DefineReadConst(true);
    }
}

} // namespace Shader::Backend::SPIRV
