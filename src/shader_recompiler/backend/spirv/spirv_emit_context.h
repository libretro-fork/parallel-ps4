// SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "common/alignment.h"
#include "common/concepts.h"
#include "common/small_vector.h"
#include "common/string_util.h"
#include "common/u64_map.h"
#include "shader_recompiler/backend/spirv/spv_ops.h"

#include "shader_recompiler/backend/bindings.h"
#include "shader_recompiler/info.h"
#include "shader_recompiler/ir/value.h"
#include "shader_recompiler/profile.h"

namespace Shader::Backend::SPIRV {

/// The module's words (spv_module_finish), where they lie: the module's until its next reset
inline Common::Span<const u32> ModuleWords(spv_module_t* m, u32 version) {
    size_t words = 0;
    const u32* module = spv_module_finish(m, version, 0, &words);
    ASSERT_MSG(module, "SPIR-V module: out of memory");
    return {module, words};
}

/// The same, copied out, for a module that goes before its words are used
inline Common::HeapArray<u32> FinishModule(spv_module_t* m, u32 version) {
    const Common::Span<const u32> words = ModuleWords(m, version);
    return Common::HeapArray<u32>(words.data(), words.size());
}

/// OpName of @target, the name formatted only when the module writes names
u32 NameFmt(spv_module_t* m, u32 target, const char* format, ...) SHAD_PRINTF(3, 4);

/// An id of the SPIR-V module (spv_module.h): a plain word, 0 for none
using Id = u32;

/// Words handed to a call of the module that takes them with their count: a braced list that
/// lives until the call returns (Words<2>{{a, b}}.v)
template <u32 N>
struct Words {
    u32 v[N];
};

struct VectorIds {
    [[nodiscard]] Id& operator[](u32 index) {
        return ids[index - 1];
    }

    [[nodiscard]] const Id& operator[](u32 index) const {
        return ids[index - 1];
    }

    [[nodiscard]] Id& Get(u32 index) {
        return ids[index - 1];
    }

    [[nodiscard]] const Id& Get(u32 index) const {
        return ids[index - 1];
    }

    Id ids[4];
};

class EmitContext final {
public:
    spv_module_t& m; ///< The module written (spv_module.h), the caller's and reused: first

    explicit EmitContext(spv_module_t& module, const Profile& profile,
                         const RuntimeInfo& runtime_info, Info& info, Bindings& binding);
    ~EmitContext();
    EmitContext(const EmitContext&) = delete;
    EmitContext& operator=(const EmitContext&) = delete;

    Id Def(const IR::Value& value);

    void DefineBufferProperties();
    void DefineAmdPerVertexAttribs();
    void DefineWorkgroupIndex();

    /// No location or builtin decoration
    static constexpr u32 NoDecoration = ~0u;

    [[nodiscard]] Id DefineInput(Id type, u32 location = NoDecoration, u32 builtin = NoDecoration) {
        const Id input_id{DefineVariable(type, builtin, SpvStorageClassInput)};
        if (location != NoDecoration) {
            spv_Decorate1(&m, input_id, u32(SpvDecorationLocation), u32(location));
        }
        return input_id;
    }

    [[nodiscard]] Id DefineOutput(Id type, u32 location = NoDecoration,
                                  u32 builtin = NoDecoration) {
        const Id output_id{DefineVariable(type, builtin, SpvStorageClassOutput)};
        if (location != NoDecoration) {
            spv_Decorate1(&m, output_id, u32(SpvDecorationLocation), u32(location));
        }
        return output_id;
    }

    [[nodiscard]] Id DefineUniformConst(Id type, u32 set, u32 binding, bool readonly = false) {
        const Id uniform_id{DefineVar(type, SpvStorageClassUniformConstant)};
        spv_Decorate1(&m, uniform_id, u32(SpvDecorationDescriptorSet), u32(set));
        spv_Decorate1(&m, uniform_id, u32(SpvDecorationBinding), u32(binding));
        if (readonly) {
            spv_Decorate0(&m, uniform_id, u32(SpvDecorationNonWritable));
        }
        return uniform_id;
    }

    template <bool global = true>
    /// @initializer 0: none
    [[nodiscard]] Id DefineVar(Id type, u32 storage_class, Id initializer = 0) {
        const Id pointer_type_id{spv_TypePointer(&m, u32(storage_class), type)};
        return global ? spv_AddGlobalVariable(&m, pointer_type_id, u32(storage_class), initializer)
                      : spv_AddLocalVariable(&m, pointer_type_id, u32(storage_class), initializer);
    }

    [[nodiscard]] Id DefineVariable(Id type, u32 builtin, u32 storage_class, Id initializer = 0) {
        const Id id{DefineVar(type, storage_class, initializer)};
        if (builtin != NoDecoration) {
            spv_Decorate1(&m, id, u32(SpvDecorationBuiltIn), u32(builtin));
        }
        interfaces.push_back(id);
        return id;
    }

    [[nodiscard]] Id ConstU32(u32 value) {
        return spv_Constant32(&m, U32[1], u32(value));
    }

    template <typename... Args>
    [[nodiscard]] Id ConstU32(Args&&... values) {
        constexpr u32 size = static_cast<u32>(sizeof...(values));
        static_assert(size >= 2);
        const Id constituents[size] = {spv_Constant32(&m, U32[1], u32(values))...};
        const Id type = size <= 4 ? U32[size] : spv_TypeArray(&m, U32[1], ConstU32(size));
        return spv_ConstantComposite(&m, type, constituents, size);
    }

    [[nodiscard]] Id ConstS32(s32 value) {
        return spv_Constant32(&m, S32[1], u32(value));
    }

    template <typename... Args>
    [[nodiscard]] Id ConstS32(Args&&... values) {
        constexpr u32 size = static_cast<u32>(sizeof...(values));
        static_assert(size >= 2);
        const Id constituents[size] = {spv_Constant32(&m, S32[1], u32(values))...};
        const Id type = size <= 4 ? S32[size] : spv_TypeArray(&m, S32[1], ConstU32(size));
        return spv_ConstantComposite(&m, type, constituents, size);
    }

    [[nodiscard]] Id ConstF32(f32 value) {
        return spv_ConstantF32(&m, F32[1], f32(value));
    }

    template <typename... Args>
    [[nodiscard]] Id ConstF32(Args... values) {
        constexpr u32 size = static_cast<u32>(sizeof...(values));
        static_assert(size >= 2);
        const Id constituents[size] = {spv_ConstantF32(&m, F32[1], f32(values))...};
        const Id type = size <= 4 ? F32[size] : spv_TypeArray(&m, F32[1], ConstU32(size));
        return spv_ConstantComposite(&m, type, constituents, size);
    }

    inline Id AddLabel() {
        last_label = spv_AddLabel0(&m);
        return last_label;
    }

    inline Id AddLabel(Id label) {
        last_label = spv_AddLabel(&m, label);
        return last_label;
    }

    template <typename Fallback>
    Id EmitDwordMemoryRead(Id address, Fallback&& fallback) {
        const Id available_label = spv_OpLabel(&m);
        const Id fallback_label = spv_OpLabel(&m);
        const Id merge_label = spv_OpLabel(&m);

        const Id addr = spv_OpFunctionCall(&m, U64, get_bda_pointer, &address, 1);
        const Id is_available = spv_OpINotEqual(&m, U1[1], addr, u64_zero_value);
        spv_OpSelectionMerge(&m, merge_label, u32(SpvSelectionControlMaskNone));
        spv_OpBranchConditional(&m, is_available, available_label, fallback_label);

        // Available
        AddLabel(available_label);
        const Id addr_ptr = spv_OpConvertUToPtr(&m, physical_pointer_type_u32, addr);
        const Id result = spv_OpLoadAligned(&m, U32[1], addr_ptr, 4u);
        spv_OpBranch(&m, merge_label);

        // Fallback
        AddLabel(fallback_label);
        const Id fallback_result = fallback();
        spv_OpBranch(&m, merge_label);

        // Merge
        AddLabel(merge_label);
        const Id final_result = spv_OpPhi(
            &m, U32[1], Words<4>{{fallback_result, fallback_label, result, available_label}}.v, 4);
        return final_result;
    }

    Id EmitSharedMemoryAccess(const Id result_type, const Id shared_mem, const Id index) {
        if (Common::PopCount(static_cast<u32>(info.shared_types)) > 1) {
            return spv_OpAccessChain2(&m, result_type, shared_mem, u32_zero_value, index);
        }
        // Workgroup layout struct omitted.
        return spv_OpAccessChain1(&m, result_type, shared_mem, index);
    }

    Id EmitFlatbufferLoad(Id flatbuf_offset) {
        const BufferDefinition& flatbuf_buffer{buffers[flatbuf_index]};
        ASSERT(flatbuf_buffer.binding >= 0 && flatbuf_buffer.buffer_type == BufferType::Flatbuf);
        const BufferSpv& flatbuf_alias = flatbuf_buffer.aliases[u32(PointerType::U32)];
        const Id ptr{spv_OpAccessChain2(&m, flatbuf_alias.pointer_type, flatbuf_alias.id,
                                        u32_zero_value, flatbuf_offset)};
        return spv_OpLoad(&m, U32[1], ptr);
    }

    Info& info;
    const RuntimeInfo& runtime_info;
    const Profile& profile;
    HwStage hw_stage;
    SwStage sw_stage{};

    Id last_label{};

    Id void_id{};
    Id U8{};
    Id S8{};
    Id U16{};
    Id S16{};
    Id U64{};
    VectorIds F16{};
    VectorIds F32{};
    VectorIds F64{};
    VectorIds S32{};
    VectorIds U32{};
    VectorIds U1{};

    Id full_result_i32x2;
    Id full_result_u32x2;
    Id frexp_result_f32;
    Id frexp_result_f64;

    Id pi_x2;

    Id true_value{};
    Id false_value{};
    Id u8_one_value{};
    Id u8_zero_value{};
    Id u16_zero_value{};
    Id u32_one_value{};
    Id u32_zero_value{};
    Id f32_one_value{};
    Id f32_zero_value{};
    Id u64_one_value{};
    Id u64_zero_value{};

    Id shared_u16{};
    Id shared_u32{};
    Id shared_u64{};

    Id input_u32{};
    Id input_f32{};
    Id input_s32{};
    Id output_u32{};
    Id output_f32{};
    Id output_s32{};

    Id gl_in{};

    Common::SmallVector<Id, 16> interfaces;

    Id output_position{};
    Id output_point_size{};
    Id output_layer{};
    Id output_viewport_index{};
    Id primitive_id{};
    Id vertex_index{};
    Id instance_id{};
    Id push_data_block{};
    Id base_vertex{};
    Id base_instance{};
    Id frag_coord{};
    Id front_facing{};
    Id frag_depth{};
    Id stencil_ref{};
    Id sample_mask{};
    Id sample_mask_in{};
    Id helper_invocation{};
    Id sample_index{};
    Id clip_distances{};
    Id cull_distances{};

    Id patch_vertices{};
    Id output_tess_level_outer{};
    Id output_tess_level_inner{};
    Id tess_coord;
    Id patches[30]{};

    Id workgroup_id{};
    Id num_workgroups_id{};
    Id workgroup_index_id{};
    Id local_invocation_id{};
    Id local_invocation_index{};
    Id invocation_id{};
    Id subgroup_local_invocation_id{};
    Id subgroup_lt_mask{};
    Id image_u32{};
    Id image_f32{};

    Id shared_memory_u16{};
    Id shared_memory_u32{};
    Id shared_memory_u64{};

    Id shared_memory_u16_type{};
    Id shared_memory_u32_type{};
    Id shared_memory_u64_type{};

    Id bary_coord{};
    Id bary_coord_smooth{};
    Id bary_coord_smooth_centroid{};
    Id bary_coord_smooth_sample{};
    Id bary_coord_pull_model{};
    Id bary_coord_nopersp{};
    Id bary_coord_nopersp_sample{};

    struct TextureDefinition {
        const VectorIds* data_types;
        Id id;
        Id sampled_type;
        Id image_type;
        AmdGpu::ImageType view_type;
        bool is_integer = false;
        bool is_storage = false;
        MipStorageFallbackMode mip_fallback_mode{};
    };

    enum class PointerType : u32 {
        U8,
        U16,
        U32,
        F32,
        U64,
        F64,
        NumAlias,
    };

    enum class PointerSize : u32 {
        B8,
        B16,
        B32,
        B64,
        NumClass,
    };

    struct BufferSpv {
        Id id;
        Id pointer_type;
    };

    struct BufferDefinition {
        u32 binding;
        BufferType buffer_type;
        Id offsets[u32(PointerSize::NumClass)];
        BufferSpv aliases[u32(PointerType::NumAlias)];

        BufferSpv& Alias(PointerType alias) {
            return aliases[u32(alias)];
        }
        const BufferSpv& Alias(PointerType alias) const {
            return aliases[u32(alias)];
        }

        Id& Offset(PointerSize size) {
            return offsets[u32(size)];
        }
        const Id& Offset(PointerSize size) const {
            return offsets[u32(size)];
        }
    };

    Bindings& binding;
    Common::SmallVector<Id, 16> buf_type_ids;
    Common::SmallVector<BufferDefinition, 16> buffers;
    Common::SmallVector<TextureDefinition, 8> images;
    Common::SmallVector<Id, 4> samplers;
    /// The label a block ends in, by the label it starts with: Id values
    u64_map_t first_to_last_label_map{};

    size_t flatbuf_index{};
    size_t bda_pagetable_index{};
    size_t fault_buffer_index{};
    Id physical_pointer_type_u32;

    Id sampler_type{};
    Id sampler_pointer_type{};

    struct SpirvAttribute {
        union {
            Id id;
            Id id_array[3];
        };
        Id pointer_type;
        Id component_type;
        u32 num_components;
        bool is_integer{};
        bool is_loaded{};
        bool is_array{};
    };
    Id input_attr_array;
    Id output_attr_array;
    SpirvAttribute input_params[IR::NumParams]{};
    SpirvAttribute output_params[IR::NumParams]{};
    SpirvAttribute frag_outputs[IR::NumRenderTargets]{};

    Id uf11_to_f32{};
    Id f32_to_uf11{};
    Id uf10_to_f32{};
    Id f32_to_uf10{};

    Id get_bda_pointer{};

    Id read_const{};
    Id read_const_dynamic{};

private:
    void DefineArithmeticTypes();
    void DefineInterfaces();
    void DefineInputs();
    void DefineVertexBlock();
    void DefineOutputs();
    void DefinePushDataBlock();
    void DefineBuffers();
    void DefineImagesAndSamplers();
    void DefineSharedMemory();
    void DefineFunctions();

    SpirvAttribute GetAttributeInfo(AmdGpu::NumberFormat fmt, Id id, u32 num_components,
                                    bool output, bool loaded = false, bool array = false);

    BufferSpv DefineBuffer(bool is_written, bool is_coherent, u32 elem_shift,
                           BufferType buffer_type, Id data_type);

    /// The shared memory variable of elements of @type, and its pointer types: none when the
    /// shader uses no shared memory of that type
    void DefineSharedMemoryType(IR::Type type, Id element_type, u32 element_size, const char* name,
                                u32 shared_memory_size, int num_types, Id* out_variable,
                                Id* out_element_pointer, Id* out_pointer);
    Id DefineFloat32ToUfloatM5(u32 mantissa_bits, const char* name);
    Id DefineUfloatM5ToFloat32(u32 mantissa_bits, const char* name);

    Id DefineGetBdaPointer();

    Id DefineReadConst(bool dynamic);

    Id GetBufferSize(u32 sharp_idx);
};

} // namespace Shader::Backend::SPIRV
