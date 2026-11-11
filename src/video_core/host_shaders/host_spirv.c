/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <stdarg.h>
#include <stddef.h>
#include <string.h>

#include <spirv/unified1/GLSL.std.450.h>
#include <spirv/unified1/spirv.h>

#include "video_core/host_shaders/host_spirv.h"

/* ------------------------------------------------------------------------ */
/* A SPIR-V module, built a section at a time and joined at the end.        */
/* ------------------------------------------------------------------------ */

#define SPV_VERSION_1_3 0x00010300u /* the entry point lists inputs and outputs only */

typedef struct spv_section {
    uint32_t* w;
    size_t n;
    size_t cap;
} spv_section_t;

typedef struct spv_key {
    uint32_t words[8];
    unsigned len;
    uint32_t id;
} spv_key_t;

#define CAP_SMALL 64
#define CAP_ANNOT 256
#define CAP_TYPES 1024
#define CAP_CODE 4096
#define SPV_STORAGE_WORDS (CAP_SMALL * 6 + CAP_ANNOT + CAP_TYPES + CAP_CODE)

#define SPV_MAX_KEYS 256
#define SPV_MAX_INTERFACE 16

typedef struct spv_module {
    uint32_t bound;
    int overflow;
    spv_section_t caps, exts, imports, model, entry, modes, annot, types, code;
    spv_key_t keys[SPV_MAX_KEYS]; /* types and constants made once */
    unsigned num_keys;
    uint32_t interface[SPV_MAX_INTERFACE];
    unsigned num_interface;
    uint32_t glsl;
    uint32_t main_fn;
    /* the types every shader here uses */
    uint32_t t_void, t_bool, t_u32, t_s32, t_f32;
    uint32_t t_f32x2, t_f32x3, t_f32x4, t_s32x2, t_u32x3, t_bool3;
    uint32_t storage[SPV_STORAGE_WORDS]; /* the sections' words */
} spv_module_t;

static void spv_section_init(spv_section_t* s, uint32_t** next, size_t cap) {
    s->w = *next;
    s->n = 0;
    s->cap = cap;
    *next += cap;
}

static void spv_word(spv_module_t* m, spv_section_t* s, uint32_t w) {
    if (s->n == s->cap) {
        m->overflow = 1;
        return;
    }
    s->w[s->n++] = w;
}

static void spv_emit(spv_module_t* m, spv_section_t* s, SpvOp op, unsigned n,
                     const uint32_t* operands) {
    unsigned i;
    spv_word(m, s, ((uint32_t)(n + 1) << 16) | (uint32_t)op);
    for (i = 0; i < n; i++)
        spv_word(m, s, operands[i]);
}

/* A literal string's words, NUL-terminated and padded */
static unsigned spv_string(uint32_t* out, const char* str) {
    size_t len = strlen(str) + 1;
    unsigned words = (unsigned)((len + 3) / 4);
    memset(out, 0, words * 4);
    memcpy(out, str, len);
    return words;
}

/* memcpy for words, which also takes an empty, null source */
static void copy_words(uint32_t* dst, const uint32_t* src, size_t n) {
    size_t i;
    for (i = 0; i < n; i++)
        dst[i] = src[i];
}

static uint32_t spv_id(spv_module_t* m) {
    return m->bound++;
}

/* A type or constant: @op with @n operands after its result id (and, for a
 * constant, after its type), made once and reused */
static uint32_t spv_unique(spv_module_t* m, SpvOp op, int has_type, unsigned n,
                           const uint32_t* operands) {
    uint32_t words[10];
    unsigned i;
    spv_key_t* key;
    for (i = 0; i < m->num_keys; i++) {
        key = &m->keys[i];
        if (key->len == n + 1 && key->words[0] == (uint32_t)op &&
            (n == 0 || memcmp(key->words + 1, operands, n * 4) == 0))
            return key->id;
    }
    if (m->num_keys == SPV_MAX_KEYS || n > 7) {
        m->overflow = 1;
        return 0;
    }
    key = &m->keys[m->num_keys++];
    key->words[0] = (uint32_t)op;
    copy_words(key->words + 1, operands, n);
    key->len = n + 1;
    key->id = spv_id(m);
    /* a constant's type comes before its id */
    if (has_type) {
        words[0] = operands[0];
        words[1] = key->id;
        copy_words(words + 2, operands + 1, n - 1);
    } else {
        words[0] = key->id;
        copy_words(words + 1, operands, n);
    }
    spv_emit(m, &m->types, op, n + 1, words);
    return key->id;
}

/* A type that is decorated (a block, an array with a stride): never shared */
static uint32_t spv_type_own(spv_module_t* m, SpvOp op, unsigned n, const uint32_t* operands) {
    uint32_t words[10];
    uint32_t id = spv_id(m);
    words[0] = id;
    copy_words(words + 1, operands, n);
    spv_emit(m, &m->types, op, n + 1, words);
    return id;
}

static uint32_t t0(spv_module_t* m, SpvOp op) {
    return spv_unique(m, op, 0, 0, NULL);
}
static uint32_t t1(spv_module_t* m, SpvOp op, uint32_t a) {
    return spv_unique(m, op, 0, 1, &a);
}
static uint32_t t2(spv_module_t* m, SpvOp op, uint32_t a, uint32_t b) {
    uint32_t v[2];
    v[0] = a;
    v[1] = b;
    return spv_unique(m, op, 0, 2, v);
}
static uint32_t t_int(spv_module_t* m, unsigned width, unsigned is_signed) {
    return t2(m, SpvOpTypeInt, width, is_signed);
}
static uint32_t t_ptr(spv_module_t* m, SpvStorageClass storage, uint32_t type) {
    return t2(m, SpvOpTypePointer, (uint32_t)storage, type);
}
static uint32_t t_image(spv_module_t* m, uint32_t sampled_type, int ms) {
    uint32_t v[7];
    v[0] = sampled_type;
    v[1] = SpvDim2D;
    v[2] = 0; /* depth */
    v[3] = 0; /* arrayed */
    v[4] = (uint32_t)ms;
    v[5] = 1; /* sampled */
    v[6] = SpvImageFormatUnknown;
    return spv_unique(m, SpvOpTypeImage, 0, 7, v);
}

static uint32_t k_u32(spv_module_t* m, uint32_t value) {
    uint32_t v[2];
    v[0] = m->t_u32;
    v[1] = value;
    return spv_unique(m, SpvOpConstant, 1, 2, v);
}
static uint32_t k_s32(spv_module_t* m, int32_t value) {
    uint32_t v[2];
    v[0] = m->t_s32;
    v[1] = (uint32_t)value;
    return spv_unique(m, SpvOpConstant, 1, 2, v);
}
static uint32_t k_f32(spv_module_t* m, float value) {
    uint32_t v[2];
    v[0] = m->t_f32;
    memcpy(&v[1], &value, 4);
    return spv_unique(m, SpvOpConstant, 1, 2, v);
}

static void spv_decorate(spv_module_t* m, uint32_t target, SpvDecoration decoration, int has_value,
                         uint32_t value) {
    uint32_t v[3];
    v[0] = target;
    v[1] = (uint32_t)decoration;
    v[2] = value;
    spv_emit(m, &m->annot, SpvOpDecorate, has_value ? 3 : 2, v);
}
static void spv_member_offset(spv_module_t* m, uint32_t type, uint32_t member, uint32_t offset) {
    uint32_t v[4];
    v[0] = type;
    v[1] = member;
    v[2] = SpvDecorationOffset;
    v[3] = offset;
    spv_emit(m, &m->annot, SpvOpMemberDecorate, 4, v);
}

static void spv_capability(spv_module_t* m, SpvCapability capability) {
    uint32_t v = (uint32_t)capability;
    spv_emit(m, &m->caps, SpvOpCapability, 1, &v);
}
static void spv_extension(spv_module_t* m, const char* name) {
    uint32_t v[16];
    spv_emit(m, &m->exts, SpvOpExtension, spv_string(v, name), v);
}

static uint32_t spv_global(spv_module_t* m, SpvStorageClass storage, uint32_t type) {
    uint32_t v[3];
    v[0] = t_ptr(m, storage, type);
    v[1] = spv_id(m);
    v[2] = (uint32_t)storage;
    spv_emit(m, &m->types, SpvOpVariable, 3, v);
    return v[1];
}
static uint32_t spv_io(spv_module_t* m, SpvStorageClass storage, uint32_t type, uint32_t location) {
    uint32_t var = spv_global(m, storage, type);
    spv_decorate(m, var, SpvDecorationLocation, 1, location);
    if (m->num_interface < SPV_MAX_INTERFACE)
        m->interface[m->num_interface++] = var;
    return var;
}
static uint32_t spv_builtin(spv_module_t* m, SpvStorageClass storage, uint32_t type,
                            SpvBuiltIn builtin) {
    uint32_t var = spv_global(m, storage, type);
    spv_decorate(m, var, SpvDecorationBuiltIn, 1, (uint32_t)builtin);
    if (m->num_interface < SPV_MAX_INTERFACE)
        m->interface[m->num_interface++] = var;
    return var;
}
static uint32_t spv_binding(spv_module_t* m, SpvStorageClass storage, uint32_t type,
                            uint32_t binding) {
    uint32_t var = spv_global(m, storage, type);
    spv_decorate(m, var, SpvDecorationDescriptorSet, 1, 0);
    spv_decorate(m, var, SpvDecorationBinding, 1, binding);
    return var;
}

/* -- instructions in main() ---------------------------------------------- */

static uint32_t opn(spv_module_t* m, SpvOp op, uint32_t type, unsigned n, const uint32_t* args) {
    uint32_t v[12];
    v[0] = type;
    v[1] = spv_id(m);
    copy_words(v + 2, args, n);
    spv_emit(m, &m->code, op, n + 2, v);
    return v[1];
}
static uint32_t op1(spv_module_t* m, SpvOp op, uint32_t type, uint32_t a) {
    return opn(m, op, type, 1, &a);
}
static uint32_t op2(spv_module_t* m, SpvOp op, uint32_t type, uint32_t a, uint32_t b) {
    uint32_t v[2];
    v[0] = a;
    v[1] = b;
    return opn(m, op, type, 2, v);
}
static uint32_t op3(spv_module_t* m, SpvOp op, uint32_t type, uint32_t a, uint32_t b, uint32_t c) {
    uint32_t v[3];
    v[0] = a;
    v[1] = b;
    v[2] = c;
    return opn(m, op, type, 3, v);
}
static uint32_t op4(spv_module_t* m, SpvOp op, uint32_t type, uint32_t a, uint32_t b, uint32_t c,
                    uint32_t d) {
    uint32_t v[4];
    v[0] = a;
    v[1] = b;
    v[2] = c;
    v[3] = d;
    return opn(m, op, type, 4, v);
}
/* An instruction with no result */
static void opv(spv_module_t* m, SpvOp op, unsigned n, ...) {
    uint32_t v[8];
    unsigned i;
    va_list ap;
    va_start(ap, n);
    for (i = 0; i < n && i < 8; i++)
        v[i] = va_arg(ap, uint32_t);
    va_end(ap);
    spv_emit(m, &m->code, op, n, v);
}
static uint32_t ext(spv_module_t* m, uint32_t type, enum GLSLstd450 inst, unsigned n,
                    const uint32_t* args) {
    uint32_t v[6];
    v[0] = m->glsl;
    v[1] = (uint32_t)inst;
    copy_words(v + 2, args, n);
    return opn(m, SpvOpExtInst, type, n + 2, v);
}
static uint32_t label(spv_module_t* m, uint32_t id) {
    spv_emit(m, &m->code, SpvOpLabel, 1, &id);
    return id;
}
static uint32_t load(spv_module_t* m, uint32_t type, uint32_t pointer) {
    return op1(m, SpvOpLoad, type, pointer);
}
static void store(spv_module_t* m, uint32_t pointer, uint32_t value) {
    opv(m, SpvOpStore, 2, pointer, value);
}
static uint32_t local(spv_module_t* m, uint32_t type) {
    uint32_t v[3];
    v[0] = t_ptr(m, SpvStorageClassFunction, type);
    v[1] = spv_id(m);
    v[2] = SpvStorageClassFunction;
    spv_emit(m, &m->code, SpvOpVariable, 3, v);
    return v[1];
}

/* u32 arithmetic, as GLSL's uint */
static uint32_t u_add(spv_module_t* m, uint32_t a, uint32_t b) {
    return op2(m, SpvOpIAdd, m->t_u32, a, b);
}
static uint32_t u_sub(spv_module_t* m, uint32_t a, uint32_t b) {
    return op2(m, SpvOpISub, m->t_u32, a, b);
}
static uint32_t u_mul(spv_module_t* m, uint32_t a, uint32_t b) {
    return op2(m, SpvOpIMul, m->t_u32, a, b);
}
static uint32_t u_div(spv_module_t* m, uint32_t a, uint32_t b) {
    return op2(m, SpvOpUDiv, m->t_u32, a, b);
}
static uint32_t u_mod(spv_module_t* m, uint32_t a, uint32_t b) {
    return op2(m, SpvOpUMod, m->t_u32, a, b);
}
static uint32_t u_and(spv_module_t* m, uint32_t a, uint32_t b) {
    return op2(m, SpvOpBitwiseAnd, m->t_u32, a, b);
}
static uint32_t u_or(spv_module_t* m, uint32_t a, uint32_t b) {
    return op2(m, SpvOpBitwiseOr, m->t_u32, a, b);
}
static uint32_t u_xor(spv_module_t* m, uint32_t a, uint32_t b) {
    return op2(m, SpvOpBitwiseXor, m->t_u32, a, b);
}
static uint32_t u_shl(spv_module_t* m, uint32_t a, uint32_t b) {
    return op2(m, SpvOpShiftLeftLogical, m->t_u32, a, b);
}
static uint32_t u_shr(spv_module_t* m, uint32_t a, uint32_t b) {
    return op2(m, SpvOpShiftRightLogical, m->t_u32, a, b);
}
/* With a constant second operand, folding the identities */
static uint32_t u_addk(spv_module_t* m, uint32_t a, uint32_t k) {
    return k ? u_add(m, a, k_u32(m, k)) : a;
}
static uint32_t u_mulk(spv_module_t* m, uint32_t a, uint32_t k) {
    return k == 1 ? a : u_mul(m, a, k_u32(m, k));
}
static uint32_t u_divk(spv_module_t* m, uint32_t a, uint32_t k) {
    return k == 1 ? a : u_div(m, a, k_u32(m, k));
}
static uint32_t u_modk(spv_module_t* m, uint32_t a, uint32_t k) {
    return u_mod(m, a, k_u32(m, k));
}
static uint32_t u_andk(spv_module_t* m, uint32_t a, uint32_t k) {
    return u_and(m, a, k_u32(m, k));
}
static uint32_t u_shlk(spv_module_t* m, uint32_t a, uint32_t k) {
    return k ? u_shl(m, a, k_u32(m, k)) : a;
}
static uint32_t u_shrk(spv_module_t* m, uint32_t a, uint32_t k) {
    return k ? u_shr(m, a, k_u32(m, k)) : a;
}
/* bitfieldExtract(v, bit, 1) */
static uint32_t u_bit(spv_module_t* m, uint32_t v, uint32_t bit) {
    return op3(m, SpvOpBitFieldUExtract, m->t_u32, v, k_u32(m, bit), k_u32(m, 1));
}

/* -- module start and end ------------------------------------------------- */

/* The builder lives on the caller's stack (some 32 KiB): making a module
 * allocates nothing */
static void spv_begin(spv_module_t* m) {
    uint32_t* next;
    uint32_t v[16];
    memset(m, 0, offsetof(spv_module_t, storage));
    m->bound = 1;
    next = m->storage;
    spv_section_init(&m->caps, &next, CAP_SMALL);
    spv_section_init(&m->exts, &next, CAP_SMALL);
    spv_section_init(&m->imports, &next, CAP_SMALL);
    spv_section_init(&m->model, &next, CAP_SMALL);
    spv_section_init(&m->entry, &next, CAP_SMALL);
    spv_section_init(&m->modes, &next, CAP_SMALL);
    spv_section_init(&m->annot, &next, CAP_ANNOT);
    spv_section_init(&m->types, &next, CAP_TYPES);
    spv_section_init(&m->code, &next, CAP_CODE);

    spv_capability(m, SpvCapabilityShader);
    m->glsl = spv_id(m);
    v[0] = m->glsl;
    spv_emit(m, &m->imports, SpvOpExtInstImport, 1 + spv_string(v + 1, "GLSL.std.450"), v);
    v[0] = SpvAddressingModelLogical;
    v[1] = SpvMemoryModelGLSL450;
    spv_emit(m, &m->model, SpvOpMemoryModel, 2, v);

    m->t_void = t0(m, SpvOpTypeVoid);
    m->t_bool = t0(m, SpvOpTypeBool);
    m->t_u32 = t_int(m, 32, 0);
    m->t_s32 = t_int(m, 32, 1);
    m->t_f32 = t1(m, SpvOpTypeFloat, 32);
    m->t_f32x2 = t2(m, SpvOpTypeVector, m->t_f32, 2);
    m->t_f32x3 = t2(m, SpvOpTypeVector, m->t_f32, 3);
    m->t_f32x4 = t2(m, SpvOpTypeVector, m->t_f32, 4);
    m->t_s32x2 = t2(m, SpvOpTypeVector, m->t_s32, 2);
    m->t_u32x3 = t2(m, SpvOpTypeVector, m->t_u32, 3);
    m->t_bool3 = t2(m, SpvOpTypeVector, m->t_bool, 3);
}

/* Opens main(); the code that follows is its body */
static void spv_main(spv_module_t* m) {
    uint32_t v[4];
    v[0] = m->t_void;
    v[1] = m->main_fn = spv_id(m);
    v[2] = SpvFunctionControlMaskNone;
    v[3] = t1(m, SpvOpTypeFunction, m->t_void);
    spv_emit(m, &m->code, SpvOpFunction, 4, v);
    label(m, spv_id(m));
}

static void spv_mode(spv_module_t* m, SpvExecutionMode mode, unsigned n, const uint32_t* values) {
    uint32_t v[5];
    v[0] = m->main_fn;
    v[1] = (uint32_t)mode;
    copy_words(v + 2, values, n);
    spv_emit(m, &m->modes, SpvOpExecutionMode, n + 2, v);
}

/* Closes main() and writes the module into @out */
static size_t spv_end(spv_module_t* m, SpvExecutionModel model, uint32_t* out, size_t cap) {
    uint32_t v[32];
    unsigned n = 0;
    size_t total, at = 0;
    const spv_section_t* sections[9];
    unsigned i;

    opv(m, SpvOpReturn, 0);
    opv(m, SpvOpFunctionEnd, 0);
    v[n++] = (uint32_t)model;
    v[n++] = m->main_fn;
    n += spv_string(v + n, "main");
    copy_words(v + n, m->interface, m->num_interface);
    n += m->num_interface;
    spv_emit(m, &m->entry, SpvOpEntryPoint, n, v);

    sections[0] = &m->caps;
    sections[1] = &m->exts;
    sections[2] = &m->imports;
    sections[3] = &m->model;
    sections[4] = &m->entry;
    sections[5] = &m->modes;
    sections[6] = &m->annot;
    sections[7] = &m->types;
    sections[8] = &m->code;
    total = 5;
    for (i = 0; i < 9; i++)
        total += sections[i]->n;
    if (m->overflow || total > cap)
        return 0;
    out[at++] = SpvMagicNumber;
    out[at++] = SPV_VERSION_1_3;
    out[at++] = 0; /* generator */
    out[at++] = m->bound;
    out[at++] = 0;
    for (i = 0; i < 9; i++) {
        copy_words(out + at, sections[i]->w, sections[i]->n);
        at += sections[i]->n;
    }
    return at;
}

/* ------------------------------------------------------------------------ */
/* The shaders                                                               */
/* ------------------------------------------------------------------------ */

size_t host_spirv_fs_tri_vert(uint32_t* out, size_t cap) {
    uint32_t vertex_index, position, uv, i, px, py, args[4];
    spv_module_t module, *m = &module;
    spv_begin(m);
    vertex_index = spv_builtin(m, SpvStorageClassInput, m->t_s32, SpvBuiltInVertexIndex);
    position = spv_builtin(m, SpvStorageClassOutput, m->t_f32x4, SpvBuiltInPosition);
    uv = spv_io(m, SpvStorageClassOutput, m->t_f32x2, 0);
    spv_main(m);
    /* pos = vec2(float((i & 1) << 2), float((i & 2) << 1)) */
    i = op1(m, SpvOpBitcast, m->t_u32, load(m, m->t_s32, vertex_index));
    px = op1(m, SpvOpConvertUToF, m->t_f32, u_shlk(m, u_andk(m, i, 1), 2));
    py = op1(m, SpvOpConvertUToF, m->t_f32, u_shlk(m, u_andk(m, i, 2), 1));
    args[0] = op2(m, SpvOpFSub, m->t_f32, px, k_f32(m, 1.0f));
    args[1] = op2(m, SpvOpFSub, m->t_f32, py, k_f32(m, 1.0f));
    args[2] = k_f32(m, 0.0f);
    args[3] = k_f32(m, 1.0f);
    store(m, position, opn(m, SpvOpCompositeConstruct, m->t_f32x4, 4, args));
    args[0] = op2(m, SpvOpFMul, m->t_f32, px, k_f32(m, 0.5f));
    args[1] = op2(m, SpvOpFMul, m->t_f32, py, k_f32(m, 0.5f));
    store(m, uv, opn(m, SpvOpCompositeConstruct, m->t_f32x2, 2, args));
    return spv_end(m, SpvExecutionModelVertex, out, cap);
}

static uint32_t splat3(spv_module_t* m, uint32_t v) {
    return op3(m, SpvOpCompositeConstruct, m->t_f32x3, v, v, v);
}

size_t host_spirv_post_process_frag(uint32_t* out, size_t cap) {
    uint32_t uv, color, image_t, sampled_t, tex, settings_t, settings, members[3];
    uint32_t pc_f32, pc_u32, gamma, hdr, srgb_input, linear, rgb, alpha, args[4];
    uint32_t dg_pow, dg_lin, dg, in, exponent, g_pow, g_lin, g, sdr, shuffle[5];
    const float cutoff = 0.0031308f, a = 1.055f, b = 0.055f, d = 12.92f;
    spv_module_t module, *m = &module;
    spv_begin(m);
    uv = spv_io(m, SpvStorageClassInput, m->t_f32x2, 0);
    color = spv_io(m, SpvStorageClassOutput, m->t_f32x4, 0);
    image_t = t_image(m, m->t_f32, 0);
    sampled_t = t1(m, SpvOpTypeSampledImage, image_t);
    tex = spv_binding(m, SpvStorageClassUniformConstant, sampled_t, 0);

    /* push_constant { float gamma; bool hdr; bool srgb_input; }, the bools as uints */
    members[0] = m->t_f32;
    members[1] = m->t_u32;
    members[2] = m->t_u32;
    settings_t = spv_type_own(m, SpvOpTypeStruct, 3, members);
    spv_decorate(m, settings_t, SpvDecorationBlock, 0, 0);
    spv_member_offset(m, settings_t, 0, 0);
    spv_member_offset(m, settings_t, 1, 4);
    spv_member_offset(m, settings_t, 2, 8);
    settings = spv_global(m, SpvStorageClassPushConstant, settings_t);

    spv_main(m);
    pc_f32 = t_ptr(m, SpvStorageClassPushConstant, m->t_f32);
    pc_u32 = t_ptr(m, SpvStorageClassPushConstant, m->t_u32);
    gamma = load(m, m->t_f32, op2(m, SpvOpAccessChain, pc_f32, settings, k_s32(m, 0)));
    hdr = op2(m, SpvOpINotEqual, m->t_bool,
              load(m, m->t_u32, op2(m, SpvOpAccessChain, pc_u32, settings, k_s32(m, 1))),
              k_u32(m, 0));
    srgb_input = op2(m, SpvOpINotEqual, m->t_bool,
                     load(m, m->t_u32, op2(m, SpvOpAccessChain, pc_u32, settings, k_s32(m, 2))),
                     k_u32(m, 0));

    linear = op2(m, SpvOpImageSampleImplicitLod, m->t_f32x4, load(m, sampled_t, tex),
                 load(m, m->t_f32x2, uv));
    shuffle[0] = linear;
    shuffle[1] = linear;
    shuffle[2] = 0;
    shuffle[3] = 1;
    shuffle[4] = 2;
    rgb = opn(m, SpvOpVectorShuffle, m->t_f32x3, 5, shuffle);
    alpha = op2(m, SpvOpCompositeExtract, m->t_f32, linear, 3);

    /* degamma(rgb) = mix(pow(max(rgb + b, 0) / a, 2.4), rgb / d, rgb < d * cutoff) */
    args[0] = op2(m, SpvOpFAdd, m->t_f32x3, rgb, splat3(m, k_f32(m, b)));
    args[1] = splat3(m, k_f32(m, 0.0f));
    args[0] = op2(m, SpvOpFDiv, m->t_f32x3, ext(m, m->t_f32x3, GLSLstd450FMax, 2, args),
                  splat3(m, k_f32(m, a)));
    args[1] = splat3(m, k_f32(m, 2.4f));
    dg_pow = ext(m, m->t_f32x3, GLSLstd450Pow, 2, args);
    dg_lin = op2(m, SpvOpFDiv, m->t_f32x3, rgb, splat3(m, k_f32(m, d)));
    dg = op3(m, SpvOpSelect, m->t_f32x3,
             op2(m, SpvOpFOrdLessThan, m->t_bool3, rgb, splat3(m, k_f32(m, d * cutoff))), dg_lin,
             dg_pow);
    in = op3(m, SpvOpSelect, m->t_f32x3,
             op3(m, SpvOpCompositeConstruct, m->t_bool3, srgb_input, srgb_input, srgb_input), dg,
             rgb);

    /* gamma(rgb) = mix(a * pow(rgb, 1 / (2.4 + 1 - gamma)) - b, d * rgb / gamma,
     *                  rgb < cutoff) */
    exponent = op2(m, SpvOpFDiv, m->t_f32, k_f32(m, 1.0f),
                   op2(m, SpvOpFSub, m->t_f32, k_f32(m, 2.4f + 1.0f), gamma));
    args[0] = in;
    args[1] = splat3(m, exponent);
    g_pow = op2(m, SpvOpFSub, m->t_f32x3,
                op2(m, SpvOpVectorTimesScalar, m->t_f32x3,
                    ext(m, m->t_f32x3, GLSLstd450Pow, 2, args), k_f32(m, a)),
                splat3(m, k_f32(m, b)));
    g_lin = op2(m, SpvOpFDiv, m->t_f32x3,
                op2(m, SpvOpVectorTimesScalar, m->t_f32x3, in, k_f32(m, d)), splat3(m, gamma));
    g = op3(m, SpvOpSelect, m->t_f32x3,
            op2(m, SpvOpFOrdLessThan, m->t_bool3, in, splat3(m, k_f32(m, cutoff))), g_lin, g_pow);
    args[0] = op2(m, SpvOpCompositeExtract, m->t_f32, g, 0);
    args[1] = op2(m, SpvOpCompositeExtract, m->t_f32, g, 1);
    args[2] = op2(m, SpvOpCompositeExtract, m->t_f32, g, 2);
    args[3] = alpha;
    sdr = opn(m, SpvOpCompositeConstruct, m->t_f32x4, 4, args);
    args[0] = args[1] = args[2] = args[3] = hdr;
    store(m, color,
          op3(m, SpvOpSelect, m->t_f32x4,
              opn(m, SpvOpCompositeConstruct, t2(m, SpvOpTypeVector, m->t_bool, 4), 4, args),
              linear, sdr));
    spv_mode(m, SpvExecutionModeOriginUpperLeft, 0, NULL);
    return spv_end(m, SpvExecutionModelFragment, out, cap);
}

size_t host_spirv_color_to_ms_depth_frag(uint32_t* out, size_t cap) {
    uint32_t uv, frag_depth, sample_id, image_t, tex, image, size, coord, texel;
    spv_module_t module, *m = &module;
    spv_begin(m);
    spv_capability(m, SpvCapabilitySampleRateShading);
    spv_capability(m, SpvCapabilityImageQuery);
    uv = spv_io(m, SpvStorageClassInput, m->t_f32x2, 0);
    frag_depth = spv_builtin(m, SpvStorageClassOutput, m->t_f32, SpvBuiltInFragDepth);
    sample_id = spv_builtin(m, SpvStorageClassInput, m->t_s32, SpvBuiltInSampleId);
    spv_decorate(m, sample_id, SpvDecorationFlat, 0, 0);
    image_t = t_image(m, m->t_f32, 0);
    tex = spv_binding(m, SpvStorageClassUniformConstant, image_t, 0);
    spv_main(m);
    /* ivec2 coord = ivec2(uv * vec2(textureSize(color, 0))) */
    image = load(m, image_t, tex);
    size = op1(m, SpvOpConvertSToF, m->t_f32x2,
               op2(m, SpvOpImageQuerySizeLod, m->t_s32x2, image, k_s32(m, 0)));
    coord = op1(m, SpvOpConvertFToS, m->t_s32x2,
                op2(m, SpvOpFMul, m->t_f32x2, load(m, m->t_f32x2, uv), size));
    texel = op4(m, SpvOpImageFetch, m->t_f32x4, image, coord, SpvImageOperandsLodMask, k_s32(m, 0));
    store(m, frag_depth,
          op2(m, SpvOpVectorExtractDynamic, m->t_f32, texel, load(m, m->t_s32, sample_id)));
    spv_mode(m, SpvExecutionModeOriginUpperLeft, 0, NULL);
    spv_mode(m, SpvExecutionModeDepthReplacing, 0, NULL);
    return spv_end(m, SpvExecutionModelFragment, out, cap);
}

size_t host_spirv_ms_image_blit_frag(uint32_t* out, size_t cap, int msaa) {
    uint32_t out_color, frag_coord, sample_id = 0, image_t, tex, fc, coord, image, texel;
    uint32_t shuffle[4];
    spv_module_t module, *m = &module;
    spv_begin(m);
    if (msaa)
        spv_capability(m, SpvCapabilitySampleRateShading);
    spv_io(m, SpvStorageClassInput, m->t_f32x2, 0); /* uv: unused, fed by the vertex stage */
    out_color = spv_io(m, SpvStorageClassOutput, m->t_f32x4, 0);
    frag_coord = spv_builtin(m, SpvStorageClassInput, m->t_f32x4, SpvBuiltInFragCoord);
    if (msaa) {
        sample_id = spv_builtin(m, SpvStorageClassInput, m->t_s32, SpvBuiltInSampleId);
        spv_decorate(m, sample_id, SpvDecorationFlat, 0, 0);
    }
    image_t = t_image(m, m->t_f32, msaa);
    tex = spv_binding(m, SpvStorageClassUniformConstant, image_t, 0);
    spv_main(m);
    fc = load(m, m->t_f32x4, frag_coord);
    shuffle[0] = fc;
    shuffle[1] = fc;
    shuffle[2] = 0;
    shuffle[3] = 1;
    coord =
        op1(m, SpvOpConvertFToS, m->t_s32x2, opn(m, SpvOpVectorShuffle, m->t_f32x2, 4, shuffle));
    image = load(m, image_t, tex);
    if (msaa)
        texel = op4(m, SpvOpImageFetch, m->t_f32x4, image, coord, SpvImageOperandsSampleMask,
                    load(m, m->t_s32, sample_id));
    else
        texel =
            op4(m, SpvOpImageFetch, m->t_f32x4, image, coord, SpvImageOperandsLodMask, k_s32(m, 0));
    store(m, out_color, texel);
    spv_mode(m, SpvExecutionModeOriginUpperLeft, 0, NULL);
    return spv_end(m, SpvExecutionModelFragment, out, cap);
}

/* A storage buffer holding one runtime array of @elem, @stride bytes apart */
static uint32_t storage_array(spv_module_t* m, uint32_t elem, uint32_t stride, uint32_t binding) {
    uint32_t array_t = spv_type_own(m, SpvOpTypeRuntimeArray, 1, &elem);
    uint32_t block_t;
    spv_decorate(m, array_t, SpvDecorationArrayStride, 1, stride);
    block_t = spv_type_own(m, SpvOpTypeStruct, 1, &array_t);
    spv_decorate(m, block_t, SpvDecorationBlock, 0, 0);
    spv_member_offset(m, block_t, 0, 0);
    return spv_binding(m, SpvStorageClassStorageBuffer, block_t, binding);
}

static void compute_mode(spv_module_t* m) {
    uint32_t size[3];
    size[0] = 64;
    size[1] = 1;
    size[2] = 1;
    spv_mode(m, SpvExecutionModeLocalSize, 3, size);
}

static uint32_t global_id_x(spv_module_t* m, uint32_t gid) {
    return op2(m, SpvOpCompositeExtract, m->t_u32, load(m, m->t_u32x3, gid), 0);
}

size_t host_spirv_fault_buffer_process_comp(uint32_t* out, size_t cap, uint32_t caching_pagebits,
                                            uint32_t max_page_faults) {
    uint32_t t_u64, gid, fault_buffer, download64, download32, sb_u32, sb_u64, word_var;
    uint32_t id, fault_ptr, base_bit, header, cond, body, cont, merge, stop, go, idx[2];
    uint32_t counter, store_index, full, word, bit, page, address, k64[3];
    spv_module_t module, *m = &module;
    spv_begin(m);
    spv_capability(m, SpvCapabilityInt64);
    t_u64 = t_int(m, 64, 0);
    gid = spv_builtin(m, SpvStorageClassInput, m->t_u32x3, SpvBuiltInGlobalInvocationId);
    fault_buffer = storage_array(m, m->t_u32, 4, 0);
    download64 = storage_array(m, t_u64, 8, 1);
    download32 = storage_array(m, m->t_u32, 4, 1);
    sb_u32 = t_ptr(m, SpvStorageClassStorageBuffer, m->t_u32);
    sb_u64 = t_ptr(m, SpvStorageClassStorageBuffer, t_u64);

    spv_main(m);
    word_var = local(m, m->t_u32);
    id = global_id_x(m, gid);
    fault_ptr = op3(m, SpvOpAccessChain, sb_u32, fault_buffer, k_s32(m, 0), id);
    store(m, word_var, load(m, m->t_u32, fault_ptr));
    store(m, fault_ptr, k_u32(m, 0));
    base_bit = u_mulk(m, id, 32);

    /* while (word != 0) */
    header = spv_id(m);
    cond = spv_id(m);
    body = spv_id(m);
    cont = spv_id(m);
    merge = spv_id(m);
    stop = spv_id(m);
    go = spv_id(m);
    opv(m, SpvOpBranch, 1, header);
    label(m, header);
    opv(m, SpvOpLoopMerge, 3, merge, cont, (uint32_t)SpvLoopControlMaskNone);
    opv(m, SpvOpBranch, 1, cond);
    label(m, cond);
    opv(m, SpvOpBranchConditional, 3,
        op2(m, SpvOpINotEqual, m->t_bool, load(m, m->t_u32, word_var), k_u32(m, 0)), body, merge);
    label(m, body);
    counter = op3(m, SpvOpAccessChain, sb_u32, download32, k_s32(m, 0), k_s32(m, 0));
    store_index = u_addk(m,
                         op4(m, SpvOpAtomicIAdd, m->t_u32, counter, k_u32(m, SpvScopeDevice),
                             k_u32(m, SpvMemorySemanticsMaskNone), k_u32(m, 1)),
                         1);
    full = op2(m, SpvOpUGreaterThanEqual, m->t_bool, store_index, k_u32(m, max_page_faults));
    opv(m, SpvOpSelectionMerge, 2, go, (uint32_t)SpvSelectionControlMaskNone);
    opv(m, SpvOpBranchConditional, 3, full, stop, go);
    label(m, stop);
    opv(m, SpvOpReturn, 0);
    label(m, go);
    word = load(m, m->t_u32, word_var);
    idx[0] = word;
    bit = ext(m, m->t_u32, GLSLstd450FindILsb, 1, idx);
    store(m, word_var, u_and(m, word, u_sub(m, word, k_u32(m, 1))));
    page = u_add(m, base_bit, bit);
    k64[0] = t_u64;
    k64[1] = caching_pagebits;
    k64[2] = 0;
    address = op2(m, SpvOpShiftLeftLogical, t_u64, op1(m, SpvOpUConvert, t_u64, page),
                  spv_unique(m, SpvOpConstant, 1, 3, k64));
    store(m, op3(m, SpvOpAccessChain, sb_u64, download64, k_s32(m, 0), store_index), address);
    opv(m, SpvOpBranch, 1, cont);
    label(m, cont);
    opv(m, SpvOpBranch, 1, header);
    label(m, merge);
    compute_mode(m);
    return spv_end(m, SpvExecutionModelGLCompute, out, cap);
}

/* ------------------------------------------------------------------------ */
/* Tiling: GCN surface addressing after AMD's addrlib, as tiling.comp had    */
/* it. Whatever depends only on the image's tiling parameters is worked out  */
/* here; the module computes only what depends on the texel.                 */
/* ------------------------------------------------------------------------ */

#define MICRO_TILE_WIDTH 8
#define MICRO_TILE_HEIGHT 8
#define MICRO_TILE_PIXELS (MICRO_TILE_WIDTH * MICRO_TILE_HEIGHT)
#define NUM_PIPE_INTERLEAVE_BITS 8

enum { MODE_DISPLAY = 0, MODE_THIN = 1, MODE_DEPTH = 2, MODE_ROTATED = 3 };
enum {
    ARRAY_2D_TILED_THIN1 = 4,
    ARRAY_PRT_TILED_THIN1 = 5,
    ARRAY_PRT_2D_TILED_THIN1 = 6,
    ARRAY_2D_TILED_THICK = 7,
    ARRAY_2D_TILED_XTHICK = 8,
    ARRAY_PRT_TILED_THICK = 9,
    ARRAY_PRT_2D_TILED_THICK = 10,
    ARRAY_PRT_3D_TILED_THIN1 = 11,
    ARRAY_3D_TILED_THIN1 = 12,
    ARRAY_3D_TILED_THICK = 13,
    ARRAY_3D_TILED_XTHICK = 14,
    ARRAY_PRT_3D_TILED_THICK = 15
};
enum { PIPE_P2 = 0, PIPE_P8_32x32_8x16 = 10, PIPE_P8_32x32_16x16 = 12 };

typedef struct tiling {
    uint32_t bpp;
    uint32_t num_samples, micro_tile_mode, micro_tile_thickness, is_tiler;
    uint32_t array_mode, pipe_config, bank_width, bank_height, num_banks, num_bank_bits;
    uint32_t tile_split_bytes, macro_tile_aspect;
    uint32_t num_pipes, num_pipe_bits;
} tiling_t;

/* Division in the constant expressions; a zero divisor, which no valid
 * image gives, yields zero here instead of trapping */
static uint32_t cdiv(uint32_t a, uint32_t b) {
    return b ? a / b : 0;
}

static int is_3d(const tiling_t* t) {
    return t->array_mode == ARRAY_3D_TILED_THIN1 || t->array_mode == ARRAY_3D_TILED_THICK ||
           t->array_mode == ARRAY_3D_TILED_XTHICK;
}

/* Which bit of x, y or z each bit of the pixel number within a micro tile is:
 * a letter and a bit per entry, 0 for none */
static void pixel_bits(const tiling_t* t, uint32_t thickness, char axis[9], uint32_t bit[9]) {
    static const char d8[] = "xxxyyy", d16[] = "xxxyyy", d32[] = "xxyxyy", d64[] = "xyxxyy",
                      d128[] = "xyxyxy", thin[] = "xyxyxy", k8[] = "xyxyzz", k32[] = "xyxzyz",
                      k64[] = "xyzxyz";
    static const uint32_t bd8[6] = {0, 1, 2, 1, 0, 2}, bd16[6] = {0, 1, 2, 0, 1, 2},
                          bd32[6] = {0, 1, 0, 2, 1, 2}, bd64[6] = {0, 0, 1, 2, 1, 2},
                          bd128[6] = {0, 0, 1, 1, 2, 2}, bthin[6] = {0, 0, 1, 1, 2, 2},
                          bk8[6] = {0, 0, 1, 1, 0, 1}, bk32[6] = {0, 0, 1, 0, 1, 1},
                          bk64[6] = {0, 0, 0, 1, 1, 1};
    const char* a = NULL;
    const uint32_t* b = NULL;
    unsigned i;
    memset(axis, 0, 9);
    memset(bit, 0, 9 * sizeof(uint32_t));
    if (t->micro_tile_mode == MODE_DISPLAY) {
        switch (t->bpp) {
        case 8:
            a = d8;
            b = bd8;
            break;
        case 16:
            a = d16;
            b = bd16;
            break;
        case 32:
            a = d32;
            b = bd32;
            break;
        case 64:
            a = d64;
            b = bd64;
            break;
        case 128:
            a = d128;
            b = bd128;
            break;
        default: /* 96: none, as tiling.comp had it */
            break;
        }
    } else if (t->micro_tile_mode == MODE_THIN || t->micro_tile_mode == MODE_DEPTH) {
        a = thin;
        b = bthin;
    } else {
        switch (t->bpp) {
        case 8:
        case 16:
            a = k8;
            b = bk8;
            break;
        case 32:
            a = k32;
            b = bk32;
            break;
        case 64:
        case 128:
            a = k64;
            b = bk64;
            break;
        default:
            break;
        }
        axis[6] = 'x';
        bit[6] = 2;
        axis[7] = 'y';
        bit[7] = 2;
        if (thickness == 8) {
            axis[8] = 'z';
            bit[8] = 2;
        }
    }
    if (a) {
        for (i = 0; i < 6; i++) {
            axis[i] = a[i];
            bit[i] = b[i];
        }
    }
}

static uint32_t pixel_index(spv_module_t* m, const tiling_t* t, uint32_t x, uint32_t y, uint32_t z,
                            uint32_t thickness) {
    char axis[9];
    uint32_t bit[9], result = k_u32(m, 0), src;
    unsigned i;
    pixel_bits(t, thickness, axis, bit);
    for (i = 0; i < 9; i++) {
        if (!axis[i])
            continue;
        src = axis[i] == 'x' ? x : axis[i] == 'y' ? y : z;
        result = u_or(m, result, u_shlk(m, u_bit(m, src, bit[i]), i));
    }
    return result;
}

static uint32_t addr_micro_tiled(spv_module_t* m, const tiling_t* t, uint32_t x, uint32_t y,
                                 uint32_t slice, uint32_t pitch, uint32_t height,
                                 uint32_t thickness) {
    const uint32_t micro_tile_bytes = MICRO_TILE_PIXELS * thickness * t->bpp * t->num_samples / 8;
    uint32_t slice_bytes, micro_tiles_per_row, slice_offset, micro_tile_offset, index, pixel_offset;
    slice_bytes = u_divk(
        m, u_addk(m, u_mulk(m, u_mul(m, pitch, height), thickness * t->bpp * t->num_samples), 7),
        8);
    micro_tiles_per_row = u_divk(m, pitch, MICRO_TILE_WIDTH);
    slice_offset = u_mul(m, u_divk(m, slice, thickness), slice_bytes);
    micro_tile_offset =
        u_mulk(m,
               u_add(m, u_mul(m, u_divk(m, y, MICRO_TILE_HEIGHT), micro_tiles_per_row),
                     u_divk(m, x, MICRO_TILE_WIDTH)),
               micro_tile_bytes);
    index = pixel_index(m, t, x, y, slice, thickness);
    /* sample 0, so no sample offset */
    pixel_offset = t->micro_tile_mode == MODE_DEPTH ? u_mulk(m, index, t->bpp * t->num_samples)
                                                    : u_mulk(m, index, t->bpp);
    return u_add(m, u_add(m, slice_offset, micro_tile_offset), u_divk(m, pixel_offset, 8));
}

static uint32_t pipe_from_coord(spv_module_t* m, const tiling_t* t, uint32_t x, uint32_t y,
                                uint32_t slice) {
    uint32_t tx = u_divk(m, x, MICRO_TILE_WIDTH), ty = u_divk(m, y, MICRO_TILE_HEIGHT);
    uint32_t x3 = u_bit(m, tx, 0), x4 = u_bit(m, tx, 1), x5 = u_bit(m, tx, 2);
    uint32_t y3 = u_bit(m, ty, 0), y4 = u_bit(m, ty, 1), y5 = u_bit(m, ty, 2);
    uint32_t pipe = k_u32(m, 0), step;
    if (t->pipe_config == PIPE_P2) {
        pipe = u_xor(m, x3, y3);
    } else if (t->pipe_config == PIPE_P8_32x32_8x16) {
        pipe = u_or(m, u_or(m, u_xor(m, u_xor(m, x4, y3), x5), u_shlk(m, u_xor(m, x3, y4), 1)),
                    u_shlk(m, u_xor(m, x5, y5), 2));
    } else if (t->pipe_config == PIPE_P8_32x32_16x16) {
        pipe = u_or(m, u_or(m, u_xor(m, u_xor(m, x3, y3), x4), u_shlk(m, u_xor(m, x4, y4), 1)),
                    u_shlk(m, u_xor(m, x5, y5), 2));
    }
    if (is_3d(t)) {
        step = t->num_pipes / 2 - 1;
        if (step < 1)
            step = 1;
        pipe = u_xor(m, pipe,
                     u_andk(m, u_mulk(m, u_divk(m, slice, t->micro_tile_thickness), step),
                            t->num_pipes - 1));
    }
    return pipe;
}

static uint32_t bank_from_coord(spv_module_t* m, const tiling_t* t, uint32_t x, uint32_t y,
                                uint32_t slice, uint32_t tile_split_slice, uint32_t bank_swizzle) {
    uint32_t tx = u_divk(m, u_divk(m, x, MICRO_TILE_WIDTH), t->bank_width * t->num_pipes);
    uint32_t ty = u_divk(m, u_divk(m, y, MICRO_TILE_HEIGHT), t->bank_height);
    uint32_t x3 = u_bit(m, tx, 0), x4 = u_bit(m, tx, 1), x5 = u_bit(m, tx, 2), x6 = u_bit(m, tx, 3);
    uint32_t y3 = u_bit(m, ty, 0), y4 = u_bit(m, ty, 1), y5 = u_bit(m, ty, 2), y6 = u_bit(m, ty, 3);
    uint32_t bank = k_u32(m, 0), slice_rotation = k_u32(m, 0), step;
    switch (t->num_banks) {
    case 16:
        bank = u_or(m,
                    u_or(m, u_or(m, u_xor(m, x3, y6), u_shlk(m, u_xor(m, u_xor(m, x4, y5), y6), 1)),
                         u_shlk(m, u_xor(m, x5, y4), 2)),
                    u_shlk(m, u_xor(m, x6, y3), 3));
        break;
    case 8:
        bank = u_or(m, u_or(m, u_xor(m, x3, y5), u_shlk(m, u_xor(m, u_xor(m, x4, y4), y5), 1)),
                    u_shlk(m, u_xor(m, x5, y3), 2));
        break;
    case 4:
        bank = u_or(m, u_xor(m, x3, y4), u_shlk(m, u_xor(m, x4, y3), 1));
        break;
    case 2:
        bank = u_xor(m, x3, y3);
        break;
    default:
        break;
    }
    if (t->array_mode == ARRAY_2D_TILED_THIN1 || t->array_mode == ARRAY_2D_TILED_THICK ||
        t->array_mode == ARRAY_2D_TILED_XTHICK) {
        slice_rotation =
            u_mulk(m, u_divk(m, slice, t->micro_tile_thickness), (t->num_banks / 2) - 1);
    } else if (is_3d(t)) {
        step = (t->num_pipes / 2) - 1;
        if (step < 1)
            step = 1;
        slice_rotation =
            u_divk(m, u_mulk(m, u_divk(m, slice, t->micro_tile_thickness), step), t->num_pipes);
    }
    bank = u_xor(m, bank, u_add(m, bank_swizzle, slice_rotation));
    if (t->array_mode == ARRAY_2D_TILED_THIN1 || t->array_mode == ARRAY_3D_TILED_THIN1 ||
        t->array_mode == ARRAY_PRT_2D_TILED_THIN1 || t->array_mode == ARRAY_PRT_3D_TILED_THIN1) {
        bank = u_xor(m, bank, u_mulk(m, tile_split_slice, (t->num_banks / 2) + 1));
    }
    return u_andk(m, bank, t->num_banks - 1);
}

static uint32_t addr_macro_tiled(spv_module_t* m, const tiling_t* t, uint32_t x, uint32_t y,
                                 uint32_t slice, uint32_t pitch, uint32_t height,
                                 uint32_t bank_swizzle) {
    const uint32_t micro_tile_bytes_full =
        MICRO_TILE_PIXELS * t->micro_tile_thickness * t->bpp * t->num_samples / 8;
    uint32_t micro_tile_bytes = micro_tile_bytes_full, slices_per_tile = 1;
    uint32_t macro_tile_pitch, macro_tile_height, macro_tile_bytes;
    uint32_t index, pixel_offset, element_offset, tile_split_slice = k_u32(m, 0);
    uint32_t macro_tiles_per_row, macro_tile_offset, slice_bytes, slice_offset;
    uint32_t tile_row_index, tile_column_index, tile_offset, total_offset, pipe, bank;

    index = pixel_index(m, t, x, y, slice, t->micro_tile_thickness);
    pixel_offset = t->micro_tile_mode == MODE_DEPTH ? u_mulk(m, index, t->bpp * t->num_samples)
                                                    : u_mulk(m, index, t->bpp);
    element_offset = u_divk(m, pixel_offset, 8);

    if (micro_tile_bytes_full > t->tile_split_bytes && t->micro_tile_thickness == 1) {
        slices_per_tile = cdiv(micro_tile_bytes_full, t->tile_split_bytes);
        tile_split_slice = u_divk(m, element_offset, t->tile_split_bytes);
        element_offset = u_modk(m, element_offset, t->tile_split_bytes);
        micro_tile_bytes = t->tile_split_bytes;
    }

    macro_tile_pitch = (MICRO_TILE_WIDTH * t->bank_width * t->num_pipes) * t->macro_tile_aspect;
    macro_tile_height =
        cdiv(MICRO_TILE_HEIGHT * t->bank_height * t->num_banks, t->macro_tile_aspect);
    macro_tile_bytes = cdiv(micro_tile_bytes * (macro_tile_pitch / MICRO_TILE_WIDTH) *
                                (macro_tile_height / MICRO_TILE_HEIGHT),
                            t->num_pipes * t->num_banks);

    macro_tiles_per_row = u_divk(m, pitch, macro_tile_pitch);
    macro_tile_offset =
        u_mulk(m,
               u_add(m, u_mul(m, u_divk(m, y, macro_tile_height), macro_tiles_per_row),
                     u_divk(m, x, macro_tile_pitch)),
               macro_tile_bytes);
    slice_bytes = u_mulk(m, u_mul(m, macro_tiles_per_row, u_divk(m, height, macro_tile_height)),
                         macro_tile_bytes);
    slice_offset =
        u_mul(m, slice_bytes,
              u_add(m, tile_split_slice,
                    u_mulk(m, u_divk(m, slice, t->micro_tile_thickness), slices_per_tile)));

    tile_row_index = u_modk(m, u_divk(m, y, MICRO_TILE_HEIGHT), t->bank_height);
    tile_column_index =
        u_modk(m, u_divk(m, u_divk(m, x, MICRO_TILE_WIDTH), t->num_pipes), t->bank_width);
    tile_offset = u_mulk(m, u_add(m, u_mulk(m, tile_row_index, t->bank_width), tile_column_index),
                         micro_tile_bytes);

    total_offset =
        u_add(m, u_add(m, u_add(m, slice_offset, macro_tile_offset), element_offset), tile_offset);

    if (t->array_mode == ARRAY_PRT_TILED_THIN1 || t->array_mode == ARRAY_PRT_TILED_THICK ||
        t->array_mode == ARRAY_PRT_2D_TILED_THIN1 || t->array_mode == ARRAY_PRT_2D_TILED_THICK ||
        t->array_mode == ARRAY_PRT_3D_TILED_THIN1 || t->array_mode == ARRAY_PRT_3D_TILED_THICK) {
        x = u_modk(m, x, macro_tile_pitch);
        y = u_modk(m, y, macro_tile_height);
    }

    pipe = pipe_from_coord(m, t, x, y, slice);
    bank = bank_from_coord(m, t, x, y, slice, tile_split_slice, bank_swizzle);

    return u_or(m,
                u_or(m,
                     u_or(m, u_andk(m, total_offset, (1u << NUM_PIPE_INTERLEAVE_BITS) - 1),
                          u_shlk(m, pipe, NUM_PIPE_INTERLEAVE_BITS)),
                     u_shlk(m, bank, NUM_PIPE_INTERLEAVE_BITS + t->num_pipe_bits)),
                u_shlk(m, u_shrk(m, total_offset, NUM_PIPE_INTERLEAVE_BITS),
                       NUM_PIPE_INTERLEAVE_BITS + t->num_pipe_bits + t->num_bank_bits));
}

size_t host_spirv_tiling_comp(uint32_t* out, size_t cap, unsigned bits_per_pixel, int macro_tiled,
                              int word_access, const uint32_t* params, size_t num_params) {
    tiling_t t;
    uint32_t p[12];
    uint32_t bytes_per_pixel = bits_per_pixel / 8, block_t, block_stride, gid, tiled_data;
    uint32_t linear_data, mip_t, members[4], mips_t, info_t, info, uni_u32, sb_block;
    uint32_t texel_var, mip_var, mip_size_var, global_id, num_mips, header, cond, body, cont;
    uint32_t merge, more, next_mip, texel, mip, pitch_height, pitch, height, x, y, slice;
    uint32_t in_mips, done, in_data, done_data, inside;
    uint32_t tiled_offset, micro, macro, is_micro, i;
    spv_module_t module, *m = &module;

    memset(p, 0, sizeof(p));
    for (i = 0; i < 12 && i < num_params; i++)
        p[i] = params[i];
    memset(&t, 0, sizeof(t));
    t.bpp = bits_per_pixel;
    t.num_samples = p[0];
    t.micro_tile_mode = p[1];
    t.micro_tile_thickness = p[2];
    t.is_tiler = p[3];
    if (macro_tiled) {
        t.array_mode = p[4];
        t.pipe_config = p[5];
        t.bank_width = p[6];
        t.bank_height = p[7];
        t.num_banks = p[8];
        t.num_bank_bits = p[9];
        t.tile_split_bytes = p[10];
        t.macro_tile_aspect = p[11];
    }
    t.num_pipes = t.pipe_config == PIPE_P2 ? 2 : 8;
    t.num_pipe_bits = t.pipe_config == PIPE_P2 ? 1 : 3;

    spv_begin(m);
    /* The block each invocation moves */
    block_stride = bytes_per_pixel;
    if (word_access) {
        block_t = m->t_u32;
        block_stride = 4;
    } else {
        switch (bits_per_pixel) {
        case 8:
            spv_capability(m, SpvCapabilityStorageBuffer8BitAccess);
            spv_extension(m, "SPV_KHR_8bit_storage");
            block_t = t_int(m, 8, 0);
            break;
        case 16:
            spv_capability(m, SpvCapabilityStorageBuffer16BitAccess);
            block_t = t_int(m, 16, 0);
            break;
        case 32:
            block_t = m->t_u32;
            break;
        default:
            block_t = t2(m, SpvOpTypeVector, m->t_u32, bits_per_pixel / 32);
            break;
        }
    }

    gid = spv_builtin(m, SpvStorageClassInput, m->t_u32x3, SpvBuiltInGlobalInvocationId);
    tiled_data = storage_array(m, block_t, block_stride, 0);
    linear_data = storage_array(m, block_t, block_stride, 1);

    /* uniform TilingInfo { uint bank_swizzle, micro_tiled_mips, num_mips;
     *                      MipInfo { uint size, pitch_height, offset; } mips[16]; } */
    members[0] = m->t_u32;
    members[1] = m->t_u32;
    members[2] = m->t_u32;
    mip_t = spv_type_own(m, SpvOpTypeStruct, 3, members);
    for (i = 0; i < 3; i++)
        spv_member_offset(m, mip_t, i, i * 4);
    members[0] = mip_t;
    members[1] = k_u32(m, 16);
    mips_t = spv_type_own(m, SpvOpTypeArray, 2, members);
    spv_decorate(m, mips_t, SpvDecorationArrayStride, 1, 12);
    members[0] = m->t_u32;
    members[1] = m->t_u32;
    members[2] = m->t_u32;
    members[3] = mips_t;
    info_t = spv_type_own(m, SpvOpTypeStruct, 4, members);
    spv_decorate(m, info_t, SpvDecorationBlock, 0, 0);
    for (i = 0; i < 4; i++)
        spv_member_offset(m, info_t, i, i * 4);
    info = spv_binding(m, SpvStorageClassUniform, info_t, 2);
    uni_u32 = t_ptr(m, SpvStorageClassUniform, m->t_u32);
    sb_block = t_ptr(m, SpvStorageClassStorageBuffer, block_t);

    spv_main(m);
    texel_var = local(m, m->t_u32);
    mip_var = local(m, m->t_u32);
    mip_size_var = local(m, m->t_u32);

#define INFO_WORD(member)                                                                          \
    load(m, m->t_u32, op2(m, SpvOpAccessChain, uni_u32, info, k_s32(m, member)))
#define MIP_WORD(mip_id, member)                                                                   \
    load(m, m->t_u32,                                                                              \
         op4(m, SpvOpAccessChain, uni_u32, info, k_s32(m, 3), (mip_id), k_s32(m, member)))

    global_id = global_id_x(m, gid);
    num_mips = INFO_WORD(2);

    /* GetMipLevel: walk the mips until the texel falls in one */
    store(m, texel_var, global_id);
    store(m, mip_var, k_u32(m, 0));
    store(m, mip_size_var, u_divk(m, MIP_WORD(k_u32(m, 0), 0), bytes_per_pixel));
    header = spv_id(m);
    cond = spv_id(m);
    body = spv_id(m);
    cont = spv_id(m);
    merge = spv_id(m);
    opv(m, SpvOpBranch, 1, header);
    label(m, header);
    opv(m, SpvOpLoopMerge, 3, merge, cont, (uint32_t)SpvLoopControlMaskNone);
    opv(m, SpvOpBranch, 1, cond);
    label(m, cond);
    more = op2(m, SpvOpLogicalAnd, m->t_bool,
               op2(m, SpvOpUGreaterThanEqual, m->t_bool, load(m, m->t_u32, texel_var),
                   load(m, m->t_u32, mip_size_var)),
               op2(m, SpvOpULessThan, m->t_bool, load(m, m->t_u32, mip_var), num_mips));
    opv(m, SpvOpBranchConditional, 3, more, body, merge);
    label(m, body);
    store(m, texel_var, u_sub(m, load(m, m->t_u32, texel_var), load(m, m->t_u32, mip_size_var)));
    next_mip = u_addk(m, load(m, m->t_u32, mip_var), 1);
    store(m, mip_var, next_mip);
    store(m, mip_size_var, u_divk(m, MIP_WORD(next_mip, 0), bytes_per_pixel));
    opv(m, SpvOpBranch, 1, cont);
    label(m, cont);
    opv(m, SpvOpBranch, 1, header);
    label(m, merge);

    texel = load(m, m->t_u32, texel_var);
    mip = load(m, m->t_u32, mip_var);
    /* A texel past the last mip has no place in the image: nothing is read or
     * written for it. (Its mip's words would be whatever is after the mips, and
     * its place in the tiled data anywhere at all: a write there took the GPU
     * device down.) */
    in_mips = spv_id(m);
    done = spv_id(m);
    inside = op2(m, SpvOpULessThan, m->t_bool, mip, num_mips);
    opv(m, SpvOpSelectionMerge, 2, done, (uint32_t)SpvSelectionControlMaskNone);
    opv(m, SpvOpBranchConditional, 3, inside, in_mips, done);
    label(m, in_mips);
    pitch_height = MIP_WORD(mip, 1);
    /* No division by nothing for a mip with no pitch or height */
    pitch = u_andk(m, pitch_height, 0xffffu);
    pitch = op3(m, SpvOpSelect, m->t_u32, op2(m, SpvOpIEqual, m->t_bool, pitch, k_u32(m, 0)),
                k_u32(m, 1), pitch);
    height = u_shrk(m, pitch_height, 16);
    height = op3(m, SpvOpSelect, m->t_u32, op2(m, SpvOpIEqual, m->t_bool, height, k_u32(m, 0)),
                 k_u32(m, 1), height);
    x = u_mod(m, texel, pitch);
    y = u_mod(m, u_div(m, texel, pitch), height);
    slice = u_div(m, texel, u_mul(m, pitch, height));

    tiled_offset = MIP_WORD(mip, 2);
    if (macro_tiled) {
        micro = addr_micro_tiled(m, &t, x, y, slice, pitch, height,
                                 t.micro_tile_thickness < 4 ? t.micro_tile_thickness : 4);
        macro = addr_macro_tiled(m, &t, x, y, slice, pitch, height, INFO_WORD(0));
        is_micro = op2(m, SpvOpINotEqual, m->t_bool,
                       u_and(m, INFO_WORD(1), u_shl(m, k_u32(m, 1), mip)), k_u32(m, 0));
        tiled_offset =
            u_add(m, tiled_offset, op3(m, SpvOpSelect, m->t_u32, is_micro, micro, macro));
    } else {
        tiled_offset =
            u_add(m, tiled_offset,
                  addr_micro_tiled(m, &t, x, y, slice, pitch, height, t.micro_tile_thickness));
    }
#undef INFO_WORD
#undef MIP_WORD

    /* Nor for a place past the end of the tiled data that is bound */
    in_data = spv_id(m);
    done_data = spv_id(m);
    inside = word_access ? u_shrk(m, tiled_offset, 2) : u_divk(m, tiled_offset, bytes_per_pixel);
    inside = op2(m, SpvOpULessThan, m->t_bool, inside,
                 op2(m, SpvOpArrayLength, m->t_u32, tiled_data, 0));
    opv(m, SpvOpSelectionMerge, 2, done_data, (uint32_t)SpvSelectionControlMaskNone);
    opv(m, SpvOpBranchConditional, 3, inside, in_data, done_data);
    label(m, in_data);

    if (word_access) {
        /* The texel inside its 32-bit word, put into the other buffer's word
         * with atomics, as neighbouring texels share a word */
        const uint32_t mask = (1u << bits_per_pixel) - 1u;
        uint32_t linear_offset = u_mulk(m, global_id, bytes_per_pixel);
        uint32_t src_offset = t.is_tiler ? linear_offset : tiled_offset;
        uint32_t dst_offset = t.is_tiler ? tiled_offset : linear_offset;
        uint32_t src_buf = t.is_tiler ? linear_data : tiled_data;
        uint32_t dst_buf = t.is_tiler ? tiled_data : linear_data;
        uint32_t scope = k_u32(m, SpvScopeDevice);
        uint32_t semantics = k_u32(m, SpvMemorySemanticsMaskNone);
        uint32_t word, value, shift, dst;
        word = load(
            m, m->t_u32,
            op3(m, SpvOpAccessChain, sb_block, src_buf, k_s32(m, 0), u_shrk(m, src_offset, 2)));
        value = u_andk(m, u_shr(m, word, u_mulk(m, u_andk(m, src_offset, 3), 8)), mask);
        shift = u_mulk(m, u_andk(m, dst_offset, 3), 8);
        dst = op3(m, SpvOpAccessChain, sb_block, dst_buf, k_s32(m, 0), u_shrk(m, dst_offset, 2));
        op4(m, SpvOpAtomicAnd, m->t_u32, dst, scope, semantics,
            op1(m, SpvOpNot, m->t_u32, u_shl(m, k_u32(m, mask), shift)));
        op4(m, SpvOpAtomicOr, m->t_u32, dst, scope, semantics, u_shl(m, value, shift));
    } else {
        uint32_t tiled_ptr = op3(m, SpvOpAccessChain, sb_block, tiled_data, k_s32(m, 0),
                                 u_divk(m, tiled_offset, bytes_per_pixel));
        uint32_t linear_ptr =
            op3(m, SpvOpAccessChain, sb_block, linear_data, k_s32(m, 0), global_id);
        if (t.is_tiler)
            store(m, tiled_ptr, load(m, block_t, linear_ptr));
        else
            store(m, linear_ptr, load(m, block_t, tiled_ptr));
    }
    opv(m, SpvOpBranch, 1, done_data);
    label(m, done_data);
    opv(m, SpvOpBranch, 1, done);
    label(m, done);
    compute_mode(m);
    return spv_end(m, SpvExecutionModelGLCompute, out, cap);
}
