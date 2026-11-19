/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <string.h>

#include "shader_recompiler/backend/spirv/spv_module.h"

/* SPIR-V's own numbers, as spirv.h has them */
#define OP_EXTENSION 10
#define OP_CAPABILITY 17

/* The table of types and constants starts with this many slots and doubles at half full */
#define DECLS_FIRST 512u
/* The gap left for everything before the code, at first */
#define GAP_FIRST 2048u

/* A word mixed into a hash (murmur3's finaliser after an xor) */
static uint32_t mix(uint32_t h, uint32_t w) {
    h ^= w;
    h ^= h >> 16;
    h *= 0x85ebca6bu;
    h ^= h >> 13;
    h *= 0xc2b2ae35u;
    h ^= h >> 16;
    return h;
}

static uint32_t decl_hash(uint32_t opword, uint32_t type, const uint32_t* ops, uint32_t n) {
    uint32_t h = mix(0x9e3779b9u, opword);
    uint32_t i;
    h = mix(h, type);
    for (i = 0; i < n; i++) {
        h = mix(h, ops[i]);
    }
    return h;
}

/* Whether the declaration written at @at is @opword with @type and @ops */
static int decl_same(const uint32_t* at, uint32_t opword, uint32_t type, const uint32_t* ops,
                     uint32_t n) {
    if (at[0] != opword) {
        return 0;
    }
    if (type) {
        /* opword, type, id, operands */
        return at[1] == type && (n == 0 || memcmp(at + 3, ops, n * sizeof(*ops)) == 0);
    }
    /* opword, id, operands */
    return n == 0 || memcmp(at + 2, ops, n * sizeof(*ops)) == 0;
}

static int decls_grow(spv_module_t* m) {
    const uint32_t old_slots = m->decls ? m->decls_mask + 1 : 0;
    const uint32_t slots = old_slots ? old_slots * 2 : DECLS_FIRST;
    spv_decl_slot_t* table;
    const spv_decl_slot_t* old = m->decls;
    const uint32_t* types = m->sec[SPV_SEC_TYPES].v;
    uint32_t i;
    table = (spv_decl_slot_t*)m->mem.grow(m->mem.user, NULL, 0, slots * sizeof(*table));
    if (!table) {
        return 0;
    }
    memset(table, 0, slots * sizeof(*table));
    /* Each one made so far into the larger table, by its words where they lie */
    for (i = 0; i < old_slots; i++) {
        if (old[i].off) {
            const uint32_t* at = types + old[i].off - 1;
            const uint32_t len = at[0] >> 16;
            /* Its type and operands, as it was hashed: the id between left out */
            const int typed = old[i].id != at[1];
            const uint32_t type = typed ? at[1] : 0;
            const uint32_t* ops = at + (typed ? 3 : 2);
            const uint32_t n = len - (typed ? 3 : 2);
            uint32_t s = decl_hash(at[0], type, ops, n) & (slots - 1);
            while (table[s].off) {
                s = (s + 1) & (slots - 1);
            }
            table[s] = old[i];
        }
    }
    if (old) {
        (void)m->mem.grow(m->mem.user, (void*)old, old_slots * sizeof(*old), 0);
    }
    m->decls = table;
    m->decls_mask = slots - 1;
    return 1;
}

void spv_module_init(spv_module_t* m, spirv_words_grow_t grow, void* user) {
    memset(m, 0, sizeof(*m));
    m->mem.grow = grow;
    m->mem.user = user;
    m->gap = GAP_FIRST;
}

void spv_module_free(spv_module_t* m) {
    unsigned s;
    for (s = 0; s < SPV_SEC_COUNT; s++) {
        if (m->sec[s].v) {
            (void)m->mem.grow(m->mem.user, m->sec[s].v, m->sec[s].cap * sizeof(uint32_t), 0);
        }
    }
    if (m->decls) {
        (void)m->mem.grow(m->mem.user, m->decls, (m->decls_mask + 1) * sizeof(*m->decls), 0);
    }
    memset(m->sec, 0, sizeof(m->sec));
    m->decls = NULL;
}

bool spv_module_reset(spv_module_t* m, uint32_t code_words) {
    unsigned s;
    struct spirv_words_buf* code = &m->sec[SPV_SEC_CODE];
    for (s = 0; s < SPV_SEC_COUNT; s++) {
        m->sec[s].n = 0;
    }
    m->bound = 1;
    m->failed = 0;
    m->num_caps = 0;
    m->num_exts = 0;
    m->num_decls = 0;
    if (m->decls) {
        memset(m->decls, 0, (m->decls_mask + 1) * sizeof(*m->decls));
    } else if (!decls_grow(m)) {
        return false;
    }
    /* The code after the gap, room for it made once */
    if (!spirv_words_reserve(&m->mem, code, m->gap + code_words)) {
        return false;
    }
    code->n = m->gap;
    return true;
}

void spv_module_capability(spv_module_t* m, uint32_t cap) {
    uint32_t i;
    for (i = 0; i < m->num_caps; i++) {
        if (m->caps[i] == cap) {
            return;
        }
    }
    if (m->num_caps == SPV_MODULE_MAX_CAPS) {
        m->failed = 1;
        return;
    }
    m->caps[m->num_caps++] = cap;
}

void spv_module_extension(spv_module_t* m, const char* ext) {
    uint32_t i;
    for (i = 0; i < m->num_exts; i++) {
        if (m->exts[i] == ext || strcmp(m->exts[i], ext) == 0) {
            return;
        }
    }
    if (m->num_exts == SPV_MODULE_MAX_EXTS) {
        m->failed = 1;
        return;
    }
    m->exts[m->num_exts++] = ext;
}

void spv_module_op_str(spv_module_t* m, enum spv_module_section sec, uint32_t op,
                       const uint32_t* pre, uint32_t npre, const char* str, const uint32_t* post,
                       uint32_t npost) {
    if (!spirv_words_op_str(&m->mem, &m->sec[sec], op, pre, npre, str, post, npost)) {
        m->failed = 1;
    }
}

/* Written into the types section: its id */
static uint32_t decl_write(spv_module_t* m, uint32_t op, uint32_t type, const uint32_t* ops,
                           uint32_t n) {
    const uint32_t lead = type ? 2u : 1u;
    uint32_t* p = spirv_words_op_at(&m->mem, &m->sec[SPV_SEC_TYPES], op, n + lead);
    const uint32_t id = m->bound++;
    if (!p) {
        m->failed = 1;
        return id;
    }
    if (type) {
        p[0] = type;
    }
    p[lead - 1] = id;
    if (n) {
        memcpy(p + lead, ops, n * sizeof(*ops));
    }
    return id;
}

uint32_t spv_module_decl(spv_module_t* m, uint32_t op, uint32_t type, const uint32_t* ops,
                         uint32_t n) {
    const uint32_t opword = spirv_words_opword(op, n + (type ? 3u : 2u));
    uint32_t s, id;
    if (!m->decls || m->failed) {
        m->failed = 1;
        return m->bound++;
    }
    s = decl_hash(opword, type, ops, n) & m->decls_mask;
    for (;;) {
        const spv_decl_slot_t* slot = &m->decls[s];
        if (!slot->off) {
            break;
        }
        if (decl_same(m->sec[SPV_SEC_TYPES].v + slot->off - 1, opword, type, ops, n)) {
            return slot->id;
        }
        s = (s + 1) & m->decls_mask;
    }
    {
        const uint32_t at = m->sec[SPV_SEC_TYPES].n;
        id = decl_write(m, op, type, ops, n);
        if (m->failed) {
            return id;
        }
        m->decls[s].off = at + 1;
        m->decls[s].id = id;
    }
    /* Half full: twice the slots */
    if (++m->num_decls * 2 > m->decls_mask + 1 && !decls_grow(m)) {
        m->failed = 1;
    }
    return id;
}

uint32_t spv_module_decl_new(spv_module_t* m, uint32_t op, uint32_t type, const uint32_t* ops,
                             uint32_t n) {
    return decl_write(m, op, type, ops, n);
}

const uint32_t* spv_module_finish(spv_module_t* m, uint32_t version, uint32_t generator,
                                  size_t* words) {
    struct spirv_words_buf* code = &m->sec[SPV_SEC_CODE];
    const uint32_t code_start = m->gap;
    const uint32_t code_words = code->n - code_start;
    uint32_t prefix = SPIRV_WORDS_HEADER + m->num_caps * 2;
    uint32_t start, i, s;
    uint32_t* out;
    for (i = 0; i < m->num_exts; i++) {
        prefix += 1 + spirv_words_str_len(strlen(m->exts[i]));
    }
    for (s = 0; s < SPV_SEC_CODE; s++) {
        prefix += m->sec[s].n;
    }
    if (m->failed) {
        return NULL;
    }
    if (prefix > code_start) {
        /* The gap was too small: the code moved up once, and the next module leaves more */
        const uint32_t more = prefix - code_start;
        if (!spirv_words_room(&m->mem, code, more)) {
            return NULL;
        }
        memmove(code->v + prefix, code->v + code_start, code_words * sizeof(uint32_t));
        code->n += more;
        m->gap = prefix + prefix / 4;
        start = 0;
    } else {
        start = code_start - prefix;
    }
    out = code->v + start;
    out[0] = SPIRV_WORDS_MAGIC;
    out[1] = version;
    out[2] = generator;
    out[3] = m->bound;
    out[4] = 0;
    out += SPIRV_WORDS_HEADER;
    for (i = 0; i < m->num_caps; i++) {
        *out++ = spirv_words_opword(OP_CAPABILITY, 2);
        *out++ = m->caps[i];
    }
    for (i = 0; i < m->num_exts; i++) {
        const size_t len = strlen(m->exts[i]);
        const uint32_t nw = spirv_words_str_len(len);
        *out++ = spirv_words_opword(OP_EXTENSION, nw + 1);
        spirv_words_str_store(out, m->exts[i], len);
        out += nw;
    }
    for (s = 0; s < SPV_SEC_CODE; s++) {
        if (m->sec[s].n) {
            memcpy(out, m->sec[s].v, m->sec[s].n * sizeof(uint32_t));
        }
        out += m->sec[s].n;
    }
    *words = prefix + code_words;
    return code->v + start;
}
