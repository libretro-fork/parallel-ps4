/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

/* The shader recompiler's SPIR-V module (spv_module.c): a compute shader written through it,
 * twice in the same module object, the second time after a gap too small for what comes
 * before the code, so that the code is moved once. Checked each time: types and constants
 * made once (the same id back for the same words, a new one for other words, many of them
 * through the table's growth), capabilities and extensions once each in the order asked
 * for, a dense id bound, the module laid out as SPIR-V's logical layout wants, and, when
 * spirv-val is installed, valid for Vulkan 1.2. An extended set imported once however often asked
 * for, and an OpPhi written before its value is known, its value and parent then set through
 * spv_module_patch_phis. Debug names (spv_Name, spv_MemberName) written only when the module
 * says names are wanted. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <spirv/unified1/spirv.h>

#include "shader_recompiler/backend/spirv/spv_ops.h"

static int failures;
static long live_blocks;

static void* test_grow(void* user, void* old, size_t old_bytes, size_t new_bytes) {
    void* nv;
    (void)user;
    (void)old_bytes;
    if (new_bytes == 0) {
        free(old);
        live_blocks--;
        return NULL;
    }
    nv = realloc(old, new_bytes);
    if (nv && !old) {
        live_blocks++;
    }
    return nv;
}

static void expect(const char* what, int ok) {
    if (!ok) {
        printf("spv_module: %s\n", what);
        failures++;
    }
}

/* The opcodes of the module in order, for the layout check */
static int layout_rank(uint32_t op) {
    switch (op) {
    case SpvOpCapability:
        return 0;
    case SpvOpExtension:
        return 1;
    case SpvOpExtInstImport:
        return 2;
    case SpvOpMemoryModel:
        return 3;
    case SpvOpEntryPoint:
        return 4;
    case SpvOpExecutionMode:
        return 5;
    case SpvOpString:
    case SpvOpSource:
        return 6;
    case SpvOpName:
    case SpvOpMemberName:
        return 7;
    case SpvOpDecorate:
    case SpvOpMemberDecorate:
        return 8;
    case SpvOpFunction:
        return 10;
    default:
        return -1;
    }
}

/* The deferred OpPhi's one value: the constant 1, from the parent written */
struct phi_ctx {
    uint32_t value, parent, calls;
};

static void patch_phi(void* ctx, uint32_t arg, uint32_t parent, uint32_t* value_out,
                      uint32_t* parent_out) {
    struct phi_ctx* p = (struct phi_ctx*)ctx;
    expect("the phi's argument index", arg == 0);
    expect("the phi's parent as written", parent == p->parent);
    *value_out = p->value;
    *parent_out = parent;
    p->calls++;
}

static void write_shader(spv_module_t* m, const char* path) {
    uint32_t w[8];
    uint32_t t_void, t_fn, t_u32, t_u32_again, t_v3, t_rta, t_struct, t_ptr_sb, t_ptr_u32;
    uint32_t t_ptr_in, c0, c1, c1_again, c64, var_buf, var_gid, ext, fn, label, gid, idx, ptr, val;
    uint32_t i, first_of_many = 0, last_of_many = 0, join, phi;
    struct phi_ctx pc;
    const uint32_t* mod;
    size_t words, at;
    int rank = -1, ordered = 1;
    FILE* f;

    expect("reset", spv_module_reset(m, 64));
    spv_module_capability(m, SpvCapabilityShader);
    spv_module_capability(m, SpvCapabilityInt64);
    spv_module_capability(m, SpvCapabilityShader); /* once only */
    spv_module_extension(m, "SPV_KHR_storage_buffer_storage_class");
    spv_module_extension(m, "SPV_KHR_storage_buffer_storage_class");

    ext = spv_module_import(m, "GLSL.std.450");
    expect("an extended set imported once", spv_module_import(m, "GLSL.std.450") == ext);
    w[0] = SpvAddressingModelLogical;
    w[1] = SpvMemoryModelGLSL450;
    spv_module_op(m, SPV_SEC_MEMORY_MODEL, SpvOpMemoryModel, w, 2);

    t_void = spv_module_decl(m, SpvOpTypeVoid, 0, NULL, 0);
    t_fn = spv_module_decl(m, SpvOpTypeFunction, 0, &t_void, 1);
    w[0] = 32;
    w[1] = 0;
    t_u32 = spv_module_decl(m, SpvOpTypeInt, 0, w, 2);
    t_u32_again = spv_module_decl(m, SpvOpTypeInt, 0, w, 2);
    expect("a type made twice", t_u32 == t_u32_again);
    w[0] = t_u32;
    w[1] = 3;
    t_v3 = spv_module_decl(m, SpvOpTypeVector, 0, w, 2);
    c0 = spv_module_decl(m, SpvOpConstant, t_u32, (w[0] = 0, w), 1);
    c1 = spv_module_decl(m, SpvOpConstant, t_u32, (w[0] = 1, w), 1);
    c1_again = spv_module_decl(m, SpvOpConstant, t_u32, (w[0] = 1, w), 1);
    expect("a constant made twice", c1 == c1_again);
    expect("two constants one", c0 != c1);
    /* Many constants, through the table's growth: each once */
    for (i = 0; i < 2000; i++) {
        const uint32_t c = spv_module_decl(m, SpvOpConstant, t_u32, (w[0] = 100 + i, w), 1);
        if (i == 0) {
            first_of_many = c;
        }
        last_of_many = c;
    }
    expect("the first of many found again after the table grew",
           spv_module_decl(m, SpvOpConstant, t_u32, (w[0] = 100, w), 1) == first_of_many);
    expect("the last of many found again",
           spv_module_decl(m, SpvOpConstant, t_u32, (w[0] = 2099, w), 1) == last_of_many);
    c64 = spv_module_decl(m, SpvOpConstant, t_u32, (w[0] = 64, w), 1);
    (void)c64;

    t_rta = spv_module_decl(m, SpvOpTypeRuntimeArray, 0, &t_u32, 1);
    t_struct = spv_module_decl_new(m, SpvOpTypeStruct, 0, &t_rta, 1);
    w[0] = SpvStorageClassStorageBuffer;
    w[1] = t_struct;
    t_ptr_sb = spv_module_decl(m, SpvOpTypePointer, 0, w, 2);
    w[1] = t_u32;
    t_ptr_u32 = spv_module_decl(m, SpvOpTypePointer, 0, w, 2);
    w[0] = SpvStorageClassInput;
    w[1] = t_v3;
    t_ptr_in = spv_module_decl(m, SpvOpTypePointer, 0, w, 2);
    var_buf = spv_module_decl_new(m, SpvOpVariable, t_ptr_sb,
                                  (w[0] = SpvStorageClassStorageBuffer, w), 1);
    var_gid = spv_module_decl_new(m, SpvOpVariable, t_ptr_in, (w[0] = SpvStorageClassInput, w),
                                  1);

    w[0] = t_rta;
    w[1] = SpvDecorationArrayStride;
    w[2] = 4;
    spv_module_op(m, SPV_SEC_ANNOTATIONS, SpvOpDecorate, w, 3);
    w[0] = t_struct;
    w[1] = SpvDecorationBlock;
    spv_module_op(m, SPV_SEC_ANNOTATIONS, SpvOpDecorate, w, 2);
    w[0] = t_struct;
    w[1] = 0;
    w[2] = SpvDecorationOffset;
    w[3] = 0;
    spv_module_op(m, SPV_SEC_ANNOTATIONS, SpvOpMemberDecorate, w, 4);
    w[0] = var_buf;
    w[1] = SpvDecorationDescriptorSet;
    w[2] = 0;
    spv_module_op(m, SPV_SEC_ANNOTATIONS, SpvOpDecorate, w, 3);
    w[1] = SpvDecorationBinding;
    spv_module_op(m, SPV_SEC_ANNOTATIONS, SpvOpDecorate, w, 3);
    w[0] = var_gid;
    w[1] = SpvDecorationBuiltIn;
    w[2] = SpvBuiltInGlobalInvocationId;
    spv_module_op(m, SPV_SEC_ANNOTATIONS, SpvOpDecorate, w, 3);
    spv_module_op_str(m, SPV_SEC_DEBUG_NAMES, SpvOpName, &var_buf, 1, "buffer", NULL, 0);

    /* void main() { buffer[gl_GlobalInvocationID.x] = 1; } */
    fn = spv_module_result(m, SpvOpFunction, t_void,
                           (w[0] = SpvFunctionControlMaskNone, w[1] = t_fn, w), 2);
    label = spv_module_id(m);
    spv_module_op(m, SPV_SEC_CODE, SpvOpLabel, &label, 1);
    gid = spv_module_result(m, SpvOpLoad, t_v3, &var_gid, 1);
    idx = spv_module_result(m, SpvOpCompositeExtract, t_u32, (w[0] = gid, w[1] = 0, w), 2);
    ptr = spv_module_result(m, SpvOpAccessChain, t_ptr_u32,
                            (w[0] = var_buf, w[1] = c0, w[2] = idx, w), 3);
    /* The value stored through an OpPhi of the one parent, its value set afterwards */
    join = spv_module_id(m);
    spv_module_op(m, SPV_SEC_CODE, SpvOpBranch, &join, 1);
    spv_module_op(m, SPV_SEC_CODE, SpvOpLabel, &join, 1);
    phi = spv_module_phi_deferred(m, t_u32, &label, 1);
    pc.value = c1;
    pc.parent = label;
    pc.calls = 0;
    spv_module_patch_phis(m, patch_phi, &pc);
    expect("the phi patched once", pc.calls == 1);
    val = phi;
    w[0] = ptr;
    w[1] = val;
    spv_module_op(m, SPV_SEC_CODE, SpvOpStore, w, 2);
    spv_module_op(m, SPV_SEC_CODE, SpvOpReturn, NULL, 0);
    spv_module_op(m, SPV_SEC_CODE, SpvOpFunctionEnd, NULL, 0);

    {
        uint32_t pre[2], iface[2];
        pre[0] = SpvExecutionModelGLCompute;
        pre[1] = fn;
        iface[0] = var_buf;
        iface[1] = var_gid;
        spv_module_op_str(m, SPV_SEC_ENTRY_POINTS, SpvOpEntryPoint, pre, 2, "main", iface, 2);
    }
    w[0] = fn;
    w[1] = SpvExecutionModeLocalSize;
    w[2] = 64;
    w[3] = 1;
    w[4] = 1;
    spv_module_op(m, SPV_SEC_EXEC_MODES, SpvOpExecutionMode, w, 5);

    mod = spv_module_finish(m, 0x00010500u, 0, &words);
    expect("finished", mod != NULL);
    if (!mod) {
        return;
    }
    expect("magic", mod[0] == SpvMagicNumber);
    expect("a dense bound", mod[3] == m->bound);
    expect("two capabilities, in the order asked",
           mod[5] == ((2u << 16) | SpvOpCapability) && mod[6] == SpvCapabilityShader &&
               mod[7] == ((2u << 16) | SpvOpCapability) && mod[8] == SpvCapabilityInt64 &&
               (mod[9] & 0xffff) == SpvOpExtension);
    /* The logical layout, and every instruction's length within the module */
    for (at = SPIRV_WORDS_HEADER; at < words;) {
        const uint32_t op = mod[at] & 0xffff, len = mod[at] >> 16;
        const int r = layout_rank(op);
        if (len == 0 || at + len > words) {
            ordered = 0;
            break;
        }
        if (r >= 0) {
            if (r < rank) {
                ordered = 0;
            }
            rank = r;
        }
        at += len;
    }
    expect("the logical layout, every instruction whole", ordered && at == words);
    f = fopen(path, "wb");
    if (f) {
        fwrite(mod, sizeof(uint32_t), words, f);
        fclose(f);
    }
}

static void validate(const char* path) {
    char cmd[512];
    if (system("command -v spirv-val >/dev/null 2>&1") != 0) {
        printf("spv_module: spirv-val not installed, not validated\n");
        return;
    }
    snprintf(cmd, sizeof(cmd), "spirv-val --target-env vulkan1.2 %s", path);
    expect("valid SPIR-V (spirv-val)", system(cmd) == 0);
}

/* spv_Name and spv_MemberName: nothing written with names off, the name with them on */
static void check_names(spv_module_t* m) {
    uint32_t t;
    expect("reset", spv_module_reset(m, 64));
    t = spv_module_id(m);
    m->names = 0;
    expect("a name off gives its target back", spv_Name(m, t, "off") == t);
    (void)spv_MemberName(m, t, 0, "off");
    expect("no name with names off", m->sec[SPV_SEC_DEBUG_NAMES].n == 0);
    m->names = 1;
    (void)spv_Name(m, t, "on");
    expect("the name with names on", m->sec[SPV_SEC_DEBUG_NAMES].n == 3 &&
                                          m->sec[SPV_SEC_DEBUG_NAMES].v[0] ==
                                              ((3u << 16) | SpvOpName) &&
                                          memcmp(&m->sec[SPV_SEC_DEBUG_NAMES].v[2], "on", 3) == 0);
    (void)spv_MemberName(m, t, 1, "on");
    expect("the member name with names on", m->sec[SPV_SEC_DEBUG_NAMES].n == 7);
    m->names = 0;
}

int main(int argc, char** argv) {
    spv_module_t m;
    const char* dir = argc > 1 ? argv[1] : ".";
    char path[400];
    spv_module_init(&m, test_grow, NULL);
    check_names(&m);

    snprintf(path, sizeof(path), "%s/spv_module_test_a.spv", dir);
    write_shader(&m, path);
    validate(path);

    /* Again in the same object, its gap too small: the code is moved once */
    m.gap = 8;
    snprintf(path, sizeof(path), "%s/spv_module_test_b.spv", dir);
    write_shader(&m, path);
    validate(path);
    expect("the next gap made larger", m.gap > 8);

    spv_module_free(&m);
    expect("every block freed", live_blocks == 0);
    if (failures == 0) {
        printf("spv_module: all passed\n");
    }
    return failures != 0;
}
