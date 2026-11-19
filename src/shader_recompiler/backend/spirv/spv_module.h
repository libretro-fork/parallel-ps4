/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

/* A SPIR-V module as the shader recompiler writes it, on libretro-common's
 * spirv_words.h: ids handed out densely, types and constants made once,
 * capabilities and extensions kept once each in the order first asked
 * for, and the module laid out once, at the end, with no copy of its
 * function code. In place of Sirit.
 *
 * Sections are word buffers. Types and constants go through
 * spv_module_decl, which finds one already made by its words where they
 * lie (an open-addressing table of offsets into the types section, its
 * keys hashed with xxh3): nothing is allocated to look one up.
 *
 * The function code is written forward from a gap left at the front of
 * its buffer; spv_module_finish writes the header and the other sections
 * into that gap, back from where the code starts, so that the module is
 * one run of words with the code where it was written. A gap too small
 * for them costs one move of the code; the next module of the same
 * spv_module leaves a gap as large as the largest seen.
 *
 * A spv_module is made once and reset for each shader: its buffers keep
 * their storage, so a shader like those before it allocates nothing.
 * C89, no lock and no global state: one per thread. */

#ifndef SHADPS4_SPV_MODULE_H
#define SHADPS4_SPV_MODULE_H

#include <stddef.h>
#include <stdint.h>

#include <boolean.h>
#include <gfx/shaders/spirv_words.h>

#ifdef __cplusplus
extern "C" {
#endif

enum spv_module_section {
    SPV_SEC_EXT_IMPORTS = 0, /* OpExtInstImport */
    SPV_SEC_MEMORY_MODEL,
    SPV_SEC_ENTRY_POINTS,
    SPV_SEC_EXEC_MODES,
    SPV_SEC_DEBUG_STRINGS, /* OpString, OpSource */
    SPV_SEC_DEBUG_NAMES,   /* OpName, OpMemberName */
    SPV_SEC_ANNOTATIONS,   /* OpDecorate, OpMemberDecorate */
    SPV_SEC_TYPES,         /* types, constants, global variables */
    SPV_SEC_CODE,          /* functions */
    SPV_SEC_COUNT
};

#define SPV_MODULE_MAX_CAPS 96
#define SPV_MODULE_MAX_EXTS 32
#define SPV_MODULE_MAX_IMPORTS 8

typedef struct spv_decl_slot {
    uint32_t off; /* word of its opcode in the types section, plus one; 0: free */
    uint32_t id;
} spv_decl_slot_t;

typedef struct spv_module {
    struct spirv_words mem;
    struct spirv_words_buf sec[SPV_SEC_COUNT];
    uint32_t bound;  /* the next id */
    uint32_t gap;    /* words left before the code */
    uint32_t failed; /* out of memory somewhere: the module is not to be used */
    uint32_t names;  /* debug names (OpName, OpMemberName) written: 0 leaves them out, the
                      * caller's to set, kept across resets */
    uint32_t caps[SPV_MODULE_MAX_CAPS];
    uint32_t num_caps;
    const char* exts[SPV_MODULE_MAX_EXTS]; /* the caller's strings, kept while it writes */
    uint32_t num_exts;
    const char* import_names[SPV_MODULE_MAX_IMPORTS]; /* extended sets imported, and ids */
    uint32_t import_ids[SPV_MODULE_MAX_IMPORTS];
    uint32_t num_imports;
    struct spirv_words_buf phis; /* code offsets of the OpPhis written before their values */
    spv_decl_slot_t* decls; /* types and constants made, by their words */
    uint32_t decls_mask;    /* slots minus one: a power of two less one */
    uint32_t num_decls;
} spv_module_t;

/* Memory from the C heap (realloc and free), for spv_module_init. */
void* spv_module_heap_grow(void* user, void* old, size_t old_bytes, size_t new_bytes);

/* A module with its memory from @grow (@user its first argument). */
void spv_module_init(spv_module_t* m, spirv_words_grow_t grow, void* user);
/* Its memory, back to @grow with a size of 0 for each block it had. */
void spv_module_free(spv_module_t* m);
/* Empty again for the next shader, with storage for about @code_words of code: what it has
 * is kept. */
bool spv_module_reset(spv_module_t* m, uint32_t code_words);

/* A new id. */
static SPIRV_WORDS_INLINE uint32_t spv_module_id(spv_module_t* m) {
    return m->bound++;
}

/* The capability @cap and the extension @ext (a string that lives until spv_module_finish),
 * each kept once, in the order first asked for. */
void spv_module_capability(spv_module_t* m, uint32_t cap);
void spv_module_extension(spv_module_t* m, const char* ext);

/* Instructions into @sec: @n operand words. */
static SPIRV_WORDS_INLINE void spv_module_op(spv_module_t* m, enum spv_module_section sec,
                                             uint32_t op, const uint32_t* words, uint32_t n) {
    if (!spirv_words_op(&m->mem, &m->sec[sec], op, words, n)) {
        m->failed = 1;
    }
}

/* @op with a result type @type and a new result id, then @n operands: its id. In the code. */
static SPIRV_WORDS_INLINE uint32_t spv_module_result(spv_module_t* m, uint32_t op, uint32_t type,
                                                     const uint32_t* ops, uint32_t n) {
    uint32_t* p = spirv_words_op_at(&m->mem, &m->sec[SPV_SEC_CODE], op, n + 2);
    const uint32_t id = m->bound++;
    if (!p) {
        m->failed = 1;
        return id;
    }
    p[0] = type;
    p[1] = id;
    if (n) {
        memcpy(p + 2, ops, n * sizeof(*ops));
    }
    return id;
}

/* The id of the extended instruction set @name (a string that lives as long as the module),
 * imported the first time it is asked for. */
uint32_t spv_module_import(spv_module_t* m, const char* name);

/* OpPhi of the @n parents @blocks, its values left for spv_module_patch_phis: its id. */
uint32_t spv_module_phi_deferred(spv_module_t* m, uint32_t type, const uint32_t* blocks,
                                 uint32_t n);

/* Each deferred OpPhi's (value, parent) pairs in the order they were written, from @fn: it
 * gets the argument's index within its phi and the parent written, and sets both. */
typedef void (*spv_phi_patch_fn)(void* ctx, uint32_t arg, uint32_t parent, uint32_t* value_out,
                                 uint32_t* parent_out);
void spv_module_patch_phis(spv_module_t* m, spv_phi_patch_fn fn, void* ctx);

/* @op with a literal string after the @npre words and before the @npost ones. */
void spv_module_op_str(spv_module_t* m, enum spv_module_section sec, uint32_t op,
                       const uint32_t* pre, uint32_t npre, const char* str, const uint32_t* post,
                       uint32_t npost);

/* A type or constant: @op with, when @type is not 0, the result type @type, then its id,
 * then the @n operands. The one made before with the same words when there is one: its id,
 * and nothing is written. */
uint32_t spv_module_decl(spv_module_t* m, uint32_t op, uint32_t type, const uint32_t* ops,
                         uint32_t n);

/* The same, made anew whatever was made before (a global variable, a type that must be
 * distinct): its id. */
uint32_t spv_module_decl_new(spv_module_t* m, uint32_t op, uint32_t type, const uint32_t* ops,
                             uint32_t n);

/* The module: header (SPIR-V @version), capabilities, extensions, then the sections, laid out
 * in the code's own buffer, which it borrows until the next reset; NULL when memory ran out.
 * @words is set to its length. */
const uint32_t* spv_module_finish(spv_module_t* m, uint32_t version, uint32_t generator,
                                  size_t* words);

#ifdef __cplusplus
}
#endif

#endif
