// SPDX-FileCopyrightText: Copyright 2024-2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "shader_recompiler/backend/spirv/spirv_emit_context.h"

#include "shader_recompiler/backend/spirv/emit_spirv_discard_frag.h"

namespace Shader::Backend::SPIRV {

constexpr u32 SPIRV_VERSION_1_5 = 0x00010500;

struct DiscardShaderEmitter {
    spv_module_t m; ///< first: the rest is made into it

    ~DiscardShaderEmitter() {
        spv_module_free(&m);
    }

    explicit DiscardShaderEmitter(const Shader::OutputMap* outputs) {
        spv_module_init(&m, spv_module_heap_grow, nullptr);
        ASSERT_MSG(spv_module_reset(&m, 1024), "SPIR-V module: out of memory");
        void_id = spv_TypeVoid(&m);
        bool_id = spv_TypeBool(&m);
        float_id = spv_TypeFloat(&m, 32);
        vec4_id = spv_TypeVector(&m, float_id, 4);

        float_zero = spv_ConstantF32(&m, float_id, f32(0.0f));

        for (u8 location = 0; location < 3; ++location) {
            for (u8 elem = 0; elem < 4; ++elem) {
                switch (outputs[location][elem]) {
                case Output::ClipDist0:
                case Output::ClipDist1:
                case Output::ClipDist2:
                case Output::ClipDist3:
                case Output::ClipDist4:
                case Output::ClipDist5:
                case Output::ClipDist6:
                case Output::ClipDist7: {
                    clip_locations[num_clip_locations++] = ClipLocation{location, elem};
                    break;
                }
                default:
                    break;
                }
            }
        }
    }

    void EmitDiscardShader() {
        // The locations with clip distances in them, in order
        u8 locations_v[3];
        u32 num_locations = 0;
        for (u8 loc = 0; loc < 3; ++loc) {
            for (u32 i = 0; i < num_clip_locations; ++i) {
                if (clip_locations[i].loc == loc) {
                    locations_v[num_locations++] = loc;
                    break;
                }
            }
        }

        ASSERT_MSG(num_locations <= 2, "more than 2 clip planes locations");

        spv_AddCapability(&m, u32(SpvCapabilityShader));
        spv_AddExtension(&m, "SPV_EXT_demote_to_helper_invocation");
        spv_AddCapability(&m, u32(SpvCapabilityDemoteToHelperInvocation));
        spv_SetMemoryModel(&m, u32(SpvAddressingModelLogical), u32(SpvMemoryModelGLSL450));
        const Id void_function{spv_TypeFunction0(&m, void_id)};
        main = spv_OpFunction(&m, void_id, u32(SpvFunctionControlMaskNone), void_function);

        uint32_t ptr_id = spv_TypePointer(&m, u32(SpvStorageClassInput), vec4_id);

        Id locations_id[2] = {0};
        Id loc_vec_id[3] = {0};
        int locations_id_len = 0;
        for (u32 i = 0; i < num_locations; ++i) {
            locations_id_len++;
            locations_id[i] = spv_AddGlobalVariable(&m, ptr_id, u32(SpvStorageClassInput), 0);
            spv_Decorate1(&m, locations_id[i], u32(SpvDecorationLocation), u32(locations_v[i]));
            NameFmt(&m, locations_id[i], "ccdist%d_in", i);
        }

        spv_AddEntryPoint(&m, u32(SpvExecutionModelFragment), main, "main", locations_id,
                          u32(locations_id_len));
        spv_AddExecutionMode0(&m, main, u32(SpvExecutionModeOriginUpperLeft));
        spv_AddLabel(&m, spv_OpLabel(&m));

        for (int i = 0; i < locations_id_len; ++i) {
            uint32_t loc_vec = spv_OpLoad(&m, vec4_id, locations_id[i]);
            loc_vec_id[locations_v[i]] = loc_vec;
            NameFmt(&m, loc_vec, "ccdist%d", i);
        }

        for (u32 c = 0; c < num_clip_locations; ++c) {
            const u8 loc = clip_locations[c].loc;
            const u8 elem = clip_locations[c].elem;
            uint32_t plane = spv_OpCompositeExtract1(&m, float_id, loc_vec_id[loc], u32(elem));
            NameFmt(&m, plane, "plane%d", loc * 4 + elem);

            uint32_t clipped = spv_OpFOrdLessThan(&m, bool_id, plane, float_zero);
            const Id kill_label{spv_OpLabel(&m)};
            const Id merge_label{spv_OpLabel(&m)};
            spv_OpSelectionMerge(&m, merge_label, u32(SpvSelectionControlMaskNone));
            spv_OpBranchConditional(&m, clipped, kill_label, merge_label);
            spv_AddLabel(&m, kill_label);
            spv_OpDemoteToHelperInvocationEXT(&m);
            spv_OpBranch(&m, merge_label);
            spv_AddLabel(&m, merge_label);
        }

        spv_OpReturn(&m);
        spv_OpFunctionEnd(&m);
    }

private:
    Id main;
    Id void_id;
    Id bool_id;
    Id float_id;
    Id vec4_id;
    Id float_zero;

    /// A clip distance: the output location and component it is written to
    struct ClipLocation {
        u8 loc;
        u8 elem;
    };
    ClipLocation clip_locations[3 * 4];
    u32 num_clip_locations = 0;
};

Common::HeapArray<u32> EmitDiscardFragmentShader(const Shader::OutputMap* outputs) {
    DiscardShaderEmitter ctx{outputs};
    ctx.EmitDiscardShader();
    return FinishModule(&ctx.m, SPIRV_VERSION_1_5);
}

} // namespace Shader::Backend::SPIRV
