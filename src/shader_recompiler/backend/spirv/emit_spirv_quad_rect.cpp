// SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "shader_recompiler/backend/spirv/emit_spirv_quad_rect.h"
#include "shader_recompiler/backend/spirv/spirv_emit_context.h"
#include "shader_recompiler/runtime_info.h"

namespace Shader::Backend::SPIRV {

constexpr u32 SPIRV_VERSION_1_5 = 0x00010500;

struct QuadRectListEmitter {
    spv_module_t m; ///< first: the rest is made into it

    ~QuadRectListEmitter() {
        spv_module_free(&m);
    }

    explicit QuadRectListEmitter(const HwFragmentRuntimeInfo& fs_info_) : fs_info{fs_info_} {
        spv_module_init(&m, spv_module_heap_grow, nullptr);
        ASSERT_MSG(spv_module_reset(&m, 1024), "SPIR-V module: out of memory");
        void_id = spv_TypeVoid(&m);
        bool_id = spv_TypeBool(&m);
        float_id = spv_TypeFloat(&m, 32);
        uint_id = spv_TypeUInt(&m, 32U);
        int_id = spv_TypeInt(&m, 32U, u32(true));
        bvec2_id = spv_TypeVector(&m, bool_id, 2);
        vec2_id = spv_TypeVector(&m, float_id, 2);
        vec3_id = spv_TypeVector(&m, float_id, 3);
        vec4_id = spv_TypeVector(&m, float_id, 4);

        float_one = spv_ConstantF32(&m, float_id, f32(1.0f));
        float_min_one = spv_ConstantF32(&m, float_id, f32(-1.0f));
        int_zero = spv_Constant32(&m, int_id, u32(0));

        const Id float_arr{spv_TypeArray(&m, float_id, spv_Constant32(&m, uint_id, u32(1U)))};
        gl_per_vertex_type = spv_TypeStruct(
            &m, std::array<u32, 4>{vec4_id, float_id, float_arr, float_arr}.data(), 4);
        spv_Decorate0(&m, gl_per_vertex_type, u32(SpvDecorationBlock));
        spv_MemberDecorate1(&m, gl_per_vertex_type, 0U, u32(SpvDecorationBuiltIn),
                            u32(static_cast<u32>(SpvBuiltInPosition)));
        spv_MemberDecorate1(&m, gl_per_vertex_type, 1U, u32(SpvDecorationBuiltIn),
                            u32(static_cast<u32>(SpvBuiltInPointSize)));
        spv_MemberDecorate1(&m, gl_per_vertex_type, 2U, u32(SpvDecorationBuiltIn),
                            u32(static_cast<u32>(SpvBuiltInClipDistance)));
        spv_MemberDecorate1(&m, gl_per_vertex_type, 3U, u32(SpvDecorationBuiltIn),
                            u32(static_cast<u32>(SpvBuiltInCullDistance)));
    }

    /// Emits tessellation control shader for interpolating the 4th vertex of rectange primitive
    void EmitRectListTCS() {
        DefineEntry(SpvExecutionModelTessellationControl);

        // Set passthrough tessellation factors
        const Id output_float_id{spv_TypePointer(&m, u32(SpvStorageClassOutput), float_id)};
        for (int i = 0; i < 4; i++) {
            const Id ptr{spv_OpAccessChain1(&m, output_float_id, gl_tess_level_outer, Int(i))};
            spv_OpStore(&m, ptr, float_one);
        }
        for (int i = 0; i < 2; i++) {
            const Id ptr{spv_OpAccessChain1(&m, output_float_id, gl_tess_level_inner, Int(i))};
            spv_OpStore(&m, ptr, float_one);
        }

        const Id input_vec4{spv_TypePointer(&m, u32(SpvStorageClassInput), vec4_id)};
        const Id output_vec4{spv_TypePointer(&m, u32(SpvStorageClassOutput), vec4_id)};

        // Emit interpolation block of the 4th vertex in rect.
        // Load positions
        std::array<Id, 3> pos;
        for (int i = 0; i < 3; i++) {
            pos[i] = spv_OpLoad(&m, vec4_id,
                                spv_OpAccessChain2(&m, input_vec4, gl_in, Int(i), int_zero));
        }

        std::array<Id, 3> point_coord_equal;
        for (int i = 0; i < 3; i++) {
            // point_coord_equal[i] = equal(gl_in[i].gl_Position.xy, gl_in[(i + 1) %
            // 3].gl_Position.xy);
            const Id pos_l_xy{spv_OpVectorShuffle(&m, vec2_id, pos[i], pos[i],
                                                  std::array<u32, 2>{0, 1}.data(), 2)};
            const Id pos_r_xy{spv_OpVectorShuffle(&m, vec2_id, pos[(i + 1) % 3], pos[(i + 1) % 3],
                                                  std::array<u32, 2>{0, 1}.data(), 2)};
            point_coord_equal[i] = spv_OpFOrdEqual(&m, bvec2_id, pos_l_xy, pos_r_xy);
        }

        std::array<Id, 3> bary_coord;
        std::array<Id, 3> is_edge_vertex;
        for (int i = 0; i < 3; i++) {
            // bool xy_equal = point_coord_equal[i].x && point_coord_equal[(i + 2) % 3].y;
            const Id xy_equal{spv_OpLogicalAnd(
                &m, bool_id, spv_OpCompositeExtract1(&m, bool_id, point_coord_equal[i], 0),
                spv_OpCompositeExtract1(&m, bool_id, point_coord_equal[(i + 2) % 3], 1))};
            // bool yx_equal = point_coord_equal[i].y && point_coord_equal[(i + 2) % 3].x;
            const Id yx_equal{spv_OpLogicalAnd(
                &m, bool_id, spv_OpCompositeExtract1(&m, bool_id, point_coord_equal[i], 1),
                spv_OpCompositeExtract1(&m, bool_id, point_coord_equal[(i + 2) % 3], 0))};
            // bary_coord[i] = (xy_equal || yx_equal) ? -1.f : 1.f;
            is_edge_vertex[i] = spv_OpLogicalOr(&m, bool_id, xy_equal, yx_equal);
            bary_coord[i] = spv_OpSelect(&m, float_id, is_edge_vertex[i], float_min_one, float_one);
        }

        const auto interpolate = [&](Id v0, Id v1, Id v2) {
            // return v0 * bary_coord.x + v1 * bary_coord.y + v2 * bary_coord.z;
            const Id p0{spv_OpVectorTimesScalar(&m, vec4_id, v0, bary_coord[0])};
            const Id p1{spv_OpVectorTimesScalar(&m, vec4_id, v1, bary_coord[1])};
            const Id p2{spv_OpVectorTimesScalar(&m, vec4_id, v2, bary_coord[2])};
            return spv_OpFAdd(&m, vec4_id, p0, spv_OpFAdd(&m, vec4_id, p1, p2));
        };

        // int vertex_index_id = is_edge_vertex[1] ? 1 : (is_edge_vertex[2] ? 2 : 0);
        Id vertex_index{spv_OpSelect(&m, int_id, is_edge_vertex[2], Int(2), Int(0))};
        vertex_index = spv_OpSelect(&m, int_id, is_edge_vertex[1], Int(1), vertex_index);

        // int index = (vertex_index_id + gl_InvocationID) % 3;
        const Id invocation_id{spv_OpLoad(&m, int_id, gl_invocation_id)};
        const Id invocation_3{spv_OpIEqual(&m, bool_id, invocation_id, Int(3))};
        const Id index{
            spv_OpSMod(&m, int_id, spv_OpIAdd(&m, int_id, vertex_index, invocation_id), Int(3))};

        // gl_out[gl_InvocationID].gl_Position = gl_InvocationID == 3 ? pos3 :
        // gl_in[index].gl_Position;
        const Id pos3{interpolate(pos[0], pos[1], pos[2])};
        const Id in_ptr{spv_OpAccessChain2(&m, input_vec4, gl_in, index, Int(0))};
        const Id position{
            spv_OpSelect(&m, vec4_id, invocation_3, pos3, spv_OpLoad(&m, vec4_id, in_ptr))};
        spv_OpStore(&m, spv_OpAccessChain2(&m, output_vec4, gl_out, invocation_id, Int(0)),
                    position);

        // Set attributes
        for (int i = 0; i < inputs.size(); i++) {
            // vec4 in_paramN3 = interpolate(bary_coord, in_paramN[0], in_paramN[1], in_paramN[2]);
            const Id v0{
                spv_OpLoad(&m, vec4_id, spv_OpAccessChain1(&m, input_vec4, inputs[i], Int(0)))};
            const Id v1{
                spv_OpLoad(&m, vec4_id, spv_OpAccessChain1(&m, input_vec4, inputs[i], Int(1)))};
            const Id v2{
                spv_OpLoad(&m, vec4_id, spv_OpAccessChain1(&m, input_vec4, inputs[i], Int(2)))};
            const Id in_param3{interpolate(v0, v1, v2)};
            // out_paramN[gl_InvocationID] = gl_InvocationID == 3 ? in_paramN3 : in_paramN[index];
            const Id in_param{
                spv_OpLoad(&m, vec4_id, spv_OpAccessChain1(&m, input_vec4, inputs[i], index))};
            const Id out_param{spv_OpSelect(&m, vec4_id, invocation_3, in_param3, in_param)};
            spv_OpStore(&m, spv_OpAccessChain1(&m, output_vec4, outputs[i], invocation_id),
                        out_param);
        }

        spv_OpReturn(&m);
        spv_OpFunctionEnd(&m);
    }

    /// Emits a passthrough quad tessellation control shader that outputs 4 control points.
    void EmitQuadListTCS() {
        DefineEntry(SpvExecutionModelTessellationControl);
        const Id array_type{spv_TypeArray(&m, int_id, Int(4))};
        const Id values{spv_ConstantComposite(
            &m, array_type, std::array<u32, 4>{Int(1), Int(2), Int(0), Int(3)}.data(), 4)};
        const Id indices{
            spv_AddLocalVariable(&m, spv_TypePointer(&m, u32(SpvStorageClassFunction), array_type),
                                 u32(SpvStorageClassFunction), values)};

        // Set passthrough tessellation factors
        const Id output_float{spv_TypePointer(&m, u32(SpvStorageClassOutput), float_id)};
        for (int i = 0; i < 4; i++) {
            const Id ptr{spv_OpAccessChain1(&m, output_float, gl_tess_level_outer, Int(i))};
            spv_OpStore(&m, ptr, float_one);
        }
        for (int i = 0; i < 2; i++) {
            const Id ptr{spv_OpAccessChain1(&m, output_float, gl_tess_level_inner, Int(i))};
            spv_OpStore(&m, ptr, float_one);
        }

        const Id input_vec4{spv_TypePointer(&m, u32(SpvStorageClassInput), vec4_id)};
        const Id output_vec4{spv_TypePointer(&m, u32(SpvStorageClassOutput), vec4_id)};
        const Id func_int{spv_TypePointer(&m, u32(SpvStorageClassFunction), int_id)};
        const Id invocation_id{spv_OpLoad(&m, int_id, gl_invocation_id)};
        const Id index{
            spv_OpLoad(&m, int_id, spv_OpAccessChain1(&m, func_int, indices, invocation_id))};

        // gl_out[gl_InvocationID].gl_Position = gl_in[gl_InvocationID].gl_Position;
        const Id in_position{
            spv_OpLoad(&m, vec4_id, spv_OpAccessChain2(&m, input_vec4, gl_in, index, Int(0)))};
        spv_OpStore(&m, spv_OpAccessChain2(&m, output_vec4, gl_out, invocation_id, Int(0)),
                    in_position);

        for (int i = 0; i < inputs.size(); i++) {
            // out_paramN[gl_InvocationID] = in_paramN[gl_InvocationID];
            const Id in_param{
                spv_OpLoad(&m, vec4_id, spv_OpAccessChain1(&m, input_vec4, inputs[i], index))};
            spv_OpStore(&m, spv_OpAccessChain1(&m, output_vec4, outputs[i], invocation_id),
                        in_param);
        }

        spv_OpReturn(&m);
        spv_OpFunctionEnd(&m);
    }

    /// Emits a passthrough quad tessellation evaluation shader that outputs 4 control points.
    void EmitPassthroughTES() {
        DefineEntry(SpvExecutionModelTessellationEvaluation);

        // const int index = int(gl_TessCoord.y) * 2 + int(gl_TessCoord.x);
        const Id input_float{spv_TypePointer(&m, u32(SpvStorageClassInput), float_id)};
        const Id tess_coord_x{
            spv_OpLoad(&m, float_id, spv_OpAccessChain1(&m, input_float, gl_tess_coord, Int(0)))};
        const Id tess_coord_y{
            spv_OpLoad(&m, float_id, spv_OpAccessChain1(&m, input_float, gl_tess_coord, Int(1)))};
        const Id index{spv_OpIAdd(
            &m, int_id, spv_OpIMul(&m, int_id, spv_OpConvertFToS(&m, int_id, tess_coord_y), Int(2)),
            spv_OpConvertFToS(&m, int_id, tess_coord_x))};

        // gl_Position = gl_in[index].gl_Position;
        const Id input_vec4{spv_TypePointer(&m, u32(SpvStorageClassInput), vec4_id)};
        const Id output_vec4{spv_TypePointer(&m, u32(SpvStorageClassOutput), vec4_id)};
        const Id position{
            spv_OpLoad(&m, vec4_id, spv_OpAccessChain2(&m, input_vec4, gl_in, index, Int(0)))};
        spv_OpStore(&m, spv_OpAccessChain1(&m, output_vec4, gl_per_vertex, Int(0)), position);

        // out_paramN = in_paramN[index];
        for (int i = 0; i < inputs.size(); i++) {
            const Id param{
                spv_OpLoad(&m, vec4_id, spv_OpAccessChain1(&m, input_vec4, inputs[i], index))};
            spv_OpStore(&m, outputs[i], param);
        }

        spv_OpReturn(&m);
        spv_OpFunctionEnd(&m);
    }

private:
    Id Int(s32 value) {
        return spv_Constant32(&m, int_id, u32(value));
    }

    Id AddInput(Id type) {
        const Id input{spv_AddGlobalVariable(&m,
                                             spv_TypePointer(&m, u32(SpvStorageClassInput), type),
                                             u32(SpvStorageClassInput), 0)};
        interfaces.push_back(input);
        return input;
    }

    Id AddOutput(Id type) {
        const Id output{spv_AddGlobalVariable(&m,
                                              spv_TypePointer(&m, u32(SpvStorageClassOutput), type),
                                              u32(SpvStorageClassOutput), 0)};
        interfaces.push_back(output);
        return output;
    }

    void DefineEntry(u32 model) {
        spv_AddCapability(&m, u32(SpvCapabilityShader));
        spv_AddCapability(&m, u32(SpvCapabilityTessellation));
        const Id void_function{spv_TypeFunction0(&m, void_id)};
        main = spv_OpFunction(&m, void_id, u32(SpvFunctionControlMaskNone), void_function);
        if (model == SpvExecutionModelTessellationControl) {
            spv_AddExecutionMode1(&m, main, u32(SpvExecutionModeOutputVertices), 4U);
        } else {
            spv_AddExecutionMode0(&m, main, u32(SpvExecutionModeQuads));
            spv_AddExecutionMode0(&m, main, u32(SpvExecutionModeSpacingEqual));
            spv_AddExecutionMode0(&m, main, u32(SpvExecutionModeVertexOrderCw));
        }
        DefineInputs(model);
        DefineOutputs(model);
        spv_AddEntryPoint(&m, u32(model), main, "main", interfaces.data(), u32(interfaces.size()));
        spv_AddLabel(&m, spv_OpLabel(&m));
    }

    void DefineOutputs(u32 model) {
        if (model == SpvExecutionModelTessellationControl) {
            const Id gl_per_vertex_array{
                spv_TypeArray(&m, gl_per_vertex_type, spv_Constant32(&m, uint_id, u32(4U)))};
            gl_out = AddOutput(gl_per_vertex_array);

            const Id arr2_id{spv_TypeArray(&m, float_id, spv_Constant32(&m, uint_id, u32(2U)))};
            gl_tess_level_inner = AddOutput(arr2_id);
            spv_Decorate1(&m, gl_tess_level_inner, u32(SpvDecorationBuiltIn),
                          u32(SpvBuiltInTessLevelInner));
            spv_Decorate0(&m, gl_tess_level_inner, u32(SpvDecorationPatch));

            const Id arr4_id{spv_TypeArray(&m, float_id, spv_Constant32(&m, uint_id, u32(4U)))};
            gl_tess_level_outer = AddOutput(arr4_id);
            spv_Decorate1(&m, gl_tess_level_outer, u32(SpvDecorationBuiltIn),
                          u32(SpvBuiltInTessLevelOuter));
            spv_Decorate0(&m, gl_tess_level_outer, u32(SpvDecorationPatch));
        } else {
            gl_per_vertex = AddOutput(gl_per_vertex_type);
        }
        outputs.reserve(fs_info.num_inputs);
        for (int i = 0; i < fs_info.num_inputs; i++) {
            const auto& input = fs_info.inputs[i];
            if (input.IsDefault()) {
                continue;
            }
            outputs.emplace_back(AddOutput(model == SpvExecutionModelTessellationControl
                                               ? spv_TypeArray(&m, vec4_id, Int(4))
                                               : vec4_id));
            spv_Decorate1(&m, outputs.back(), u32(SpvDecorationLocation), u32(input.param_index));
        }
    }

    void DefineInputs(u32 model) {
        if (model == SpvExecutionModelTessellationEvaluation) {
            gl_tess_coord = AddInput(vec3_id);
            spv_Decorate1(&m, gl_tess_coord, u32(SpvDecorationBuiltIn), u32(SpvBuiltInTessCoord));
        } else {
            gl_invocation_id = AddInput(int_id);
            spv_Decorate1(&m, gl_invocation_id, u32(SpvDecorationBuiltIn),
                          u32(SpvBuiltInInvocationId));
        }
        const Id gl_per_vertex_array{
            spv_TypeArray(&m, gl_per_vertex_type, spv_Constant32(&m, uint_id, u32(32U)))};
        gl_in = AddInput(gl_per_vertex_array);
        const Id float_arr{spv_TypeArray(&m, vec4_id, Int(32))};
        inputs.reserve(fs_info.num_inputs);
        for (int i = 0; i < fs_info.num_inputs; i++) {
            const auto& input = fs_info.inputs[i];
            if (input.IsDefault()) {
                continue;
            }
            inputs.emplace_back(AddInput(float_arr));
            spv_Decorate1(&m, inputs.back(), u32(SpvDecorationLocation), u32(input.param_index));
        }
    }

private:
    HwFragmentRuntimeInfo fs_info;
    Id main;
    Id void_id;
    Id bool_id;
    Id float_id;
    Id uint_id;
    Id int_id;
    Id bvec2_id;
    Id vec2_id;
    Id vec3_id;
    Id vec4_id;
    Id float_one;
    Id float_min_one;
    Id int_zero;
    Id gl_per_vertex_type;
    Id gl_in;
    union {
        Id gl_out;
        Id gl_per_vertex;
    };
    Id gl_tess_level_inner;
    Id gl_tess_level_outer;
    union {
        Id gl_tess_coord;
        Id gl_invocation_id;
    };
    std::vector<Id> inputs;
    std::vector<Id> outputs;
    std::vector<Id> interfaces;
};

std::vector<u32> EmitAuxilaryTessShader(AuxShaderType type, const HwFragmentRuntimeInfo& fs_info) {
    QuadRectListEmitter ctx{fs_info};
    switch (type) {
    case AuxShaderType::RectListTCS:
        ctx.EmitRectListTCS();
        break;
    case AuxShaderType::QuadListTCS:
        ctx.EmitQuadListTCS();
        break;
    case AuxShaderType::PassthroughTES:
        ctx.EmitPassthroughTES();
        break;
    }
    return FinishModule(&ctx.m, SPIRV_VERSION_1_5);
}

} // namespace Shader::Backend::SPIRV
