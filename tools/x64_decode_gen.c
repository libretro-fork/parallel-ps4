/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

/* Makes src/common/x64_decode_tables.h and src/common/x64_names.h: what each x86-64 opcode path
 * decodes to, taken from Zydis 4 (the decoder the tables replace). Each opcode of each map is
 * decoded by Zydis under every prefix form (none, 66, F3, F2 and 66 with them; REX.W / VEX.W;
 * VEX.L; with and without 67 and REX.B) and every ModRM byte, and the instruction definition
 * Zydis reaches is kept: whether it has ModRM, its immediates, whether it takes LOCK and the
 * other prefixes, what its register operands may name, its mnemonic and category, its operand
 * definitions, and its operand and address widths. Zydis's register table and names go along.
 * A hook in a copy of Zydis's decoder hands those over; nothing of Zydis goes in the core.
 *
 * To run it, against a Zydis 4 source tree (Z) and its built objects, but Decoder.c's:
 *   cp $Z/src/Decoder.c zdec3.c
 *   sed -i 's|^\(\s*\)ZydisGetInstructionEncodingInfo(node, &info);|&\n\1x64g_hook(definition,
 *     info, instruction, state->context);|' zdec3.c
 *   sed -i 's|^#include <Zydis/Internal/SharedData.h>|&\nstatic void x64g_hook(const
 *     ZydisInstructionDefinition*, const ZydisInstructionEncodingInfo*, const
 *     ZydisDecodedInstruction*, const ZydisDecoderContext*);|' zdec3.c
 *   cc -O2 -DZYDIS_STATIC_BUILD -DZYCORE_STATIC_BUILD -I$Z/include -I$Z/src
 *     -I$Z/dependencies/zycore/include -DZ_CATEGORIES='"'$Z'/src/Generated/EnumInstructionCategory.inc"'
 *     x64_decode_gen.c <Zydis and Zycore objects> -o gen
 *   ./gen   (writes both headers here)
 * (each sed is one line) */

#include "zdec3.c" /* Zydis's decoder with the hook */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include Z_CATEGORIES

typedef struct {
    const void* def;
    unsigned rec, mn, cat, eosz, opw, asw, nops, nvis, pfx;
    unsigned ops[10][3];
    unsigned imm[2][3];
} raw_t;

static int g_hit;
static raw_t g_raw;

static unsigned rec_bits(const ZydisInstructionDefinition* def, const ZydisInstructionEncodingInfo* info, int enc, int eosz) {
    unsigned r = 1, sz, c, regf, rmf, nds = 0, lock = 0, gather = 0, nsdm = 0, nssm = 0;
    if (info->flags & ZYDIS_INSTR_ENC_FLAG_HAS_MODRM) r |= 2;
    if (info->flags & ZYDIS_INSTR_ENC_FLAG_FORCE_REG_FORM) r |= 4;
    if (info->flags & ZYDIS_INSTR_ENC_FLAG_HAS_DISP) r |= 8;
    if (info->flags & ZYDIS_INSTR_ENC_FLAG_HAS_IMM0) {
        sz = info->imm[0].size[eosz]; c = sz == 0 ? 0 : sz == 8 ? 1 : sz == 16 ? 2 : sz == 32 ? 3 : sz == 64 ? 4 : 7;
        if (c == 7) { fprintf(stderr, "imm0 %u\n", sz); exit(1); }
        r |= c << 4; if (info->imm[0].is_signed) r |= 0x80; if (info->imm[0].is_relative) r |= 0x100;
    }
    if (info->flags & ZYDIS_INSTR_ENC_FLAG_HAS_IMM1) {
        sz = info->imm[1].size[eosz]; c = sz == 0 ? 0 : sz == 8 ? 1 : sz == 16 ? 2 : 7;
        if (c == 7) { fprintf(stderr, "imm1 %u\n", sz); exit(1); }
        r |= c << 9;
    }
    regf = def->op_reg; rmf = def->op_rm;
    switch (enc) {
    case ZYDIS_INSTRUCTION_ENCODING_LEGACY: lock = ((const ZydisInstructionDefinitionLEGACY*)def)->accepts_LOCK; break;
    case ZYDIS_INSTRUCTION_ENCODING_XOP: nds = ((const ZydisInstructionDefinitionXOP*)def)->op_ndsndd; break;
    case ZYDIS_INSTRUCTION_ENCODING_VEX: { const ZydisInstructionDefinitionVEX* v = (const ZydisInstructionDefinitionVEX*)def;
        nds = v->op_ndsndd; gather = v->is_gather; nssm = v->no_source_source_match; break; }
    default: break;
    }
    /* A segment prefix names the memory operands' segment */
    if (def->accepts_segment && !(enc == ZYDIS_INSTRUCTION_ENCODING_LEGACY && ((const ZydisInstructionDefinitionLEGACY*)def)->accepts_branch_hints))
        r |= 1u << 31;
    r |= lock << 11 | regf << 12 | rmf << 17 | nds << 22 | gather << 27 | nsdm << 28 | nssm << 29;
    {
        unsigned kr = regf & 15, km = rmf & 15, kn = nds & 15;
        if (kr > 4 || km > 4 || kn > 4 || (regf >> 4) || (rmf >> 3) || gather || nsdm || nssm) r |= 1u << 30;
    }
    return r;
}

/* The prefixes a definition takes, as Zydis's formatter shows them, and a far branch */
static unsigned pfx_bits(const ZydisInstructionDefinition* def, int enc) {
    unsigned p = def->branch_type == ZYDIS_BRANCH_TYPE_FAR ? 256 : 0;
    if (enc == ZYDIS_INSTRUCTION_ENCODING_LEGACY) {
        const ZydisInstructionDefinitionLEGACY* l = (const ZydisInstructionDefinitionLEGACY*)def;
        p |= l->accepts_REP | l->accepts_REPEREPZ << 1 | l->accepts_REPNEREPNZ << 2 | l->accepts_BOUND << 3 |
             l->accepts_XACQUIRE << 4 | l->accepts_XRELEASE << 5 | l->accepts_NOTRACK << 6 | l->accepts_hle_without_lock << 7;
    }
    return p;
}

static void fill_ops(raw_t* w, const ZydisInstructionDefinition* def, unsigned eosz) {
    const ZydisOperandDefinition* o = ZydisGetOperandDefinitions(def);
    unsigned i;
    w->nops = def->operand_count; w->nvis = def->operand_count_visible;
    for (i = 0; i < w->nops; i++, o++) {
        unsigned w0 = o->type | o->visibility << 6 | o->actions << 8, extra = 0;
        if (o->type == ZYDIS_SEMANTIC_OPTYPE_IMPLICIT_REG) {
            unsigned v = o->op.reg.type == ZYDIS_IMPLREG_TYPE_STATIC ? o->op.reg.reg.reg : o->op.reg.reg.id;
            if (v >= 1024) { fprintf(stderr, "reg %u\n", v); exit(1); }
            extra = o->op.reg.type << 10 | v;
        } else if (o->type == ZYDIS_SEMANTIC_OPTYPE_IMPLICIT_MEM) {
            extra = o->op.mem.seg << 4 | o->op.mem.base;
        } else if (o->type != ZYDIS_SEMANTIC_OPTYPE_IMPLICIT_IMM1) {
            w0 |= (unsigned)o->op.encoding << 12;
        }
        w0 |= (unsigned)o->ignore_seg_override << 18 | (unsigned)o->is_multisource4 << 19;
        (void)eosz;
        w->ops[i][0] = w0; w->ops[i][1] = o->size[0] | (unsigned)o->size[1] << 16; w->ops[i][2] = o->size[2] | extra << 16;
    }
}

static void x64g_hook(const ZydisInstructionDefinition* def, const ZydisInstructionEncodingInfo* info,
                      const ZydisDecodedInstruction* in, const ZydisDecoderContext* ctx) {
    memset(&g_raw, 0, sizeof g_raw);
    g_raw.def = def;
    g_raw.rec = rec_bits(def, info, in->encoding, ctx->eosz_index);
    {
        unsigned k, e;
        for (k = 0; k < 2; k++) for (e = 0; e < 3; e++) {
            unsigned sz = (info->flags & (k ? ZYDIS_INSTR_ENC_FLAG_HAS_IMM1 : ZYDIS_INSTR_ENC_FLAG_HAS_IMM0)) ? info->imm[k].size[e] : 0;
            g_raw.imm[k][e] = sz == 0 ? 0 : sz == 8 ? 1 : sz == 16 ? 2 : sz == 32 ? 3 : 4;
        }
    }
    g_raw.mn = def->mnemonic; g_raw.cat = def->category; g_raw.eosz = ctx->eosz_index;
    g_raw.pfx = pfx_bits(def, in->encoding);
    g_raw.opw = in->operand_width; g_raw.asw = in->address_width;
    if (ctx->easz_index != (in->address_width == 16 ? 0 : in->address_width == 32 ? 1 : 2)) { fprintf(stderr, "easz\n"); exit(1); }
    if (in->operand_width != 0 && ctx->eosz_index != (in->operand_width == 16 ? 0 : in->operand_width == 32 ? 1 : 2)) { /* allowed: reported */ }
    fill_ops(&g_raw, def, ctx->eosz_index);
    g_hit = 1;
}

static ZydisDecoder dec;
static int probe(const uint8_t* b, unsigned n, raw_t* w) {
    ZydisDecodedInstruction in; uint8_t buf[32];
    memset(buf, 0, sizeof buf); memcpy(buf, b, n);
    g_hit = 0;
    ZydisDecoderDecodeInstruction(&dec, NULL, buf, 15, &in);
    if (!g_hit) { memset(w, 0, sizeof *w); return 0; }
    *w = g_raw; return 1;
}

/* Definitions (what an opcode path reaches) and forms (a definition at its operand and address
 * widths), deduplicated */
typedef struct { unsigned rec, imm, mn, cat, nops, nvis, pfx, ops[10][3]; } def_t;
typedef struct { unsigned def, eosz, opw, asw0, asw1; } form_t;
#define MAXF 16384
static def_t defs[MAXF]; static int ndefs = 1; static int dhash[1 << 16];
static form_t forms[MAXF]; static int nforms = 1; static int fhash[1 << 16];
static unsigned hsh(const void* x, size_t n) { const unsigned char* p = (const unsigned char*)x; unsigned h = 2166136261u; size_t i; for (i = 0; i < n; i++) h = (h ^ p[i]) * 16777619u; return h; }
static int def_id(const def_t* d) {
    unsigned h = hsh(d, sizeof *d) & 0xFFFF;
    while (dhash[h]) { if (!memcmp(&defs[dhash[h]], d, sizeof *d)) return dhash[h]; h = (h + 1) & 0xFFFF; }
    if (ndefs == MAXF) exit(4);
    defs[ndefs] = *d; dhash[h] = ndefs; return ndefs++;
}
static int fid(const form_t* f) {
    unsigned h = hsh(f, sizeof *f) & 0xFFFF;
    while (fhash[h]) { if (!memcmp(&forms[fhash[h]], f, sizeof *f)) return fhash[h]; h = (h + 1) & 0xFFFF; }
    if (nforms == MAXF) exit(4);
    forms[nforms] = *f; fhash[h] = nforms; return nforms++;
}
static int same_def(const raw_t* a, const raw_t* b) { return a->def == b->def && a->eosz == b->eosz && a->opw == b->opw && a->rec == b->rec; }
static int to_form(const raw_t* w, const raw_t* w67) {
    def_t d; form_t f;
    if (!w->def) return 0;
    memset(&d, 0, sizeof d); memset(&f, 0, sizeof f);
    d.rec = w->rec & ~0x670u; /* the immediate sizes go by the operand width: imm below */
    d.imm = w->imm[0][0] | w->imm[0][1] << 3 | w->imm[0][2] << 6 | w->imm[1][0] << 9 | w->imm[1][1] << 11 | w->imm[1][2] << 13;
    d.mn = w->mn; d.cat = w->cat; d.pfx = w->pfx; d.nops = w->nops; d.nvis = w->nvis; memcpy(d.ops, w->ops, sizeof d.ops);
    f.def = (unsigned)def_id(&d); f.eosz = w->eosz; f.opw = w->opw; f.asw0 = w->asw;
    f.asw1 = (w67 && w67->def) ? w67->asw : w->asw;
    return fid(&f);
}

/* The two opcodes whose definition goes by more than the row's prefixes: 90 by REX.B (NOP or
 * PAUSE, else XCHG), E3 by 67 (JRCXZ, else JECXZ) */
static uint16_t alt90[12], altE3[12];

static unsigned build(int em, int op, int v, int m, uint8_t* b, int alt) {
    unsigned n = 0;
    static const uint8_t cls[6][2] = {{0,0},{0x66,0},{0xF3,0},{0x66,0xF3},{0xF2,0},{0x66,0xF2}};
    int a67 = alt & 1, aB = alt & 2;
    if (a67) b[n++] = 0x67;
    if (em < 4) {
        int c = v % 6, w = v / 6, k;
        for (k = 0; k < 2; k++) if (cls[c][k]) b[n++] = cls[c][k];
        if (w || aB) b[n++] = (uint8_t)(0x40 | (w ? 8 : 0) | (aB ? 1 : 0));
        if (em >= 1) b[n++] = 0x0F;
        if (em == 2) b[n++] = 0x38;
        if (em == 3) b[n++] = 0x3A;
    } else {
        int pp = v & 3, l = v >> 2 & 1, w = v >> 3;
        b[n++] = em < 8 ? 0xC4 : 0x8F;
        b[n++] = (uint8_t)((aB ? 0xC0 : 0xE0) | (em < 8 ? em - 4 : em));
        b[n++] = (uint8_t)(w << 7 | 0x78 | l << 2 | pp);
    }
    b[n++] = (uint8_t)op; b[n++] = (uint8_t)m;
    return n;
}

static int skip(int em, int op) {
    if (em == 0) {
        if ((op & 0xF0) == 0x40) return 1;
        switch (op) { case 0x26: case 0x2E: case 0x36: case 0x3E: case 0x64: case 0x65: case 0x66: case 0x67:
        case 0xF0: case 0xF2: case 0xF3: case 0x0F: case 0xC4: case 0xC5: case 0x62: return 1; }
    }
    if (em == 1 && (op == 0x0F || op == 0x38 || op == 0x3A)) return 1;
    return 0;
}

/* The form at a probe; the opcodes whose 67 or REX.B / VEX.B picks another definition are
 * kept apart */
static int probe_form(int em, int op, int v, int m) {
    uint8_t b[32]; raw_t w, w67, wB, wB67; unsigned n;
    n = build(em, op, v, m, b, 0); probe(b, n, &w);
    n = build(em, op, v, m, b, 1); probe(b, n, &w67);
    n = build(em, op, v, m, b, 2); probe(b, n, &wB);
    n = build(em, op, v, m, b, 3); probe(b, n, &wB67);
    if (!w.def) { if (w67.def || wB.def) { fprintf(stderr, "variant valid alone em%d op%02x\n", em, op); exit(5); } return 0; }
    if (!same_def(&w, &wB)) {
        if (!(em == 0 && op == 0x90)) { fprintf(stderr, "B dep em%d op%02x\n", em, op); exit(5); }
        alt90[v] = (uint16_t)to_form(&wB, &wB67);
    }
    if (!same_def(&w, &w67)) {
        if (!(em == 0 && op == 0xE3)) { fprintf(stderr, "67 dep em%d op%02x\n", em, op); exit(5); }
        altE3[v] = (uint16_t)to_form(&w67, &w67);
        return to_form(&w, &w);
    }
    return to_form(&w, &w67);
}

#define MAXR 16384
static uint16_t subs[MAXR][8]; static int nsubs;
static uint16_t mtabs[MAXR][16]; static int nmtabs;
static uint16_t rows[MAXR][16]; static int nrows = 1;
static uint16_t op_row[11][256];
static int find16(uint16_t (*t)[16], int* n, const uint16_t* e) { int i; for (i = 0; i < *n; i++) if (!memcmp(t[i], e, 32)) return i; memcpy(t[*n], e, 32); return (*n)++; }
static int find8(const uint16_t* e) { int i; for (i = 0; i < nsubs; i++) if (!memcmp(subs[i], e, 16)) return i; memcpy(subs[nsubs], e, 16); return nsubs++; }

/* Dense mnemonic numbers, for the mnemonics the tables reach */
static int mn_dense[ZYDIS_MNEMONIC_MAX_VALUE + 1]; static int mn_zydis[4096]; static int nmn = 1;
static int dense_mn(unsigned z) { if (!z) return 0; if (!mn_dense[z]) { mn_zydis[nmn] = z; mn_dense[z] = nmn++; } return mn_dense[z]; }

static void upper(char* d, const char* s) { while (*s) { *d++ = (*s >= 'a' && *s <= 'z') ? *s - 32 : *s; s++; } *d = 0; }

int main(void) {
    int em, op, v, m, i, j, k;
    static uint16_t now3d[3][2][256];
    FILE* f;
    ZydisDecoderInit(&dec, ZYDIS_MACHINE_MODE_LONG_64, ZYDIS_STACK_WIDTH_64);
    for (em = 0; em < 11; em++) for (op = 0; op < 256; op++) {
        uint16_t row[16]; int nv = em < 4 ? 12 : 16;
        memset(row, 0, sizeof row);
        if (skip(em, op)) { op_row[em][op] = 0; continue; }
        for (v = 0; v < nv; v++) {
            int r[256], reg, rm, uniform = 1; uint16_t mt[16];
            for (m = 0; m < 256; m++) { r[m] = probe_form(em, op, v, m); if (r[m] != r[0]) uniform = 0; }
            if (em == 0 && op == 0x8F) { for (m = 0; m < 256; m++) if ((m & 0x1F) >= 8) r[m] = r[(m & 0xC0) | (m & 7)];
                uniform = 1; for (m = 0; m < 256; m++) if (r[m] != r[0]) uniform = 0; }
            if (uniform) { row[v] = (uint16_t)(0x8000 | r[0]); continue; }
            for (reg = 0; reg < 8; reg++) {
                uint16_t sub[8], msub[8]; int same = 1, msame = 1;
                for (m = 0; m < 192; m++) if ((m >> 3 & 7) == reg && r[m] != r[reg << 3 | (m & 7)]) { fprintf(stderr, "mod varies em%d op%02x\n", em, op); exit(6); }
                for (rm = 0; rm < 8; rm++) { msub[rm] = (uint16_t)r[reg << 3 | rm]; if (msub[rm] != msub[0]) msame = 0; }
                mt[reg] = msame ? msub[0] : (uint16_t)(0x8000 | find8(msub));
                for (rm = 0; rm < 8; rm++) { sub[rm] = (uint16_t)r[0xC0 | reg << 3 | rm]; if (sub[rm] != sub[0]) same = 0; }
                mt[8 + reg] = same ? sub[0] : (uint16_t)(0x8000 | find8(sub));
            }
            row[v] = (uint16_t)find16(mtabs, &nmtabs, mt);
        }
        op_row[em][op] = (uint16_t)find16(rows, &nrows, row);
    }
    /* 3DNow!: the form of each opcode byte, from Zydis's own tree, at each operand width the
     * prefixes give (none, 66, REX.W) */
    for (j = 0; j < 3; j++) {
        static const uint8_t pre[3][2] = {{0, 0}, {1, 0x66}, {1, 0x48}};
        uint8_t b[8]; raw_t base, w67; unsigned n = 0;
        if (pre[j][0]) b[n++] = pre[j][1];
        b[n++] = 0x0F; b[n++] = 0x0F; b[n++] = 0xC0; b[n++] = 0x0C; probe(b, n, &base);
        n = 0; b[n++] = 0x67; if (pre[j][0]) b[n++] = pre[j][1];
        b[n++] = 0x0F; b[n++] = 0x0F; b[n++] = 0x00; b[n++] = 0x0C; probe(b, n, &w67);
        for (k = 0; k < 2; k++) for (i = 0; i < 256; i++) {
            const ZydisDecoderTreeNode* node = ZydisDecoderTreeGetRootNode();
            node = ZydisDecoderTreeGetChildNode(node, 0x0F);
            node = ZydisDecoderTreeGetChildNode(node, 0x0F);
            node = ZydisDecoderTreeGetChildNode(node, (ZyanU16)i);
            now3d[j][k][i] = 0;
            if (node->type != ZYDIS_NODETYPE_INVALID) {
                const ZydisInstructionDefinition* def; raw_t w = base;
                node = ZydisDecoderTreeGetChildNode(node, k ? 0 : 1);
                if (!(node->type & ZYDIS_NODETYPE_DEFINITION_MASK)) continue;
                ZydisGetInstructionDefinition(ZYDIS_INSTRUCTION_ENCODING_3DNOW, node->value, &def);
                w.def = def; w.mn = def->mnemonic; w.cat = def->category;
                w.pfx = pfx_bits(def, ZYDIS_INSTRUCTION_ENCODING_3DNOW);
                w.rec = (w.rec & 0x7FFFFFFFu) | (def->accepts_segment ? 0x80000000u : 0);
                fill_ops(&w, def, base.eosz);
                now3d[j][k][i] = (uint16_t)to_form(&w, &w67);
            }
        }
    }
    fprintf(stderr, "defs %d forms %d subs %d mtabs %d rows %d\n", ndefs, nforms, nsubs, nmtabs, nrows);
    for (i = 1; i < ndefs; i++) defs[i].mn = (unsigned)dense_mn(defs[i].mn);
    f = fopen("x64_decode_tables.h", "w");
    fprintf(f, "/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project\n * SPDX-License-Identifier: GPL-2.0-or-later */\n\n");
    fprintf(f, "/* Generated by tools/x64_decode_gen.c: what each opcode path of x86-64 decodes to, as Zydis\n * 4 decodes it. Included by common/x64_decode.c only. */\n\n/* clang-format off */\n\n");
    {
        static unsigned opdefs[65536][3]; int nopd = 0; static int start[MAXF];
        for (i = 1; i < ndefs; i++) {
            int found = -1, s2;
            for (s2 = 0; s2 + (int)defs[i].nops <= nopd && defs[i].nops; s2++) if (!memcmp(opdefs[s2], defs[i].ops, defs[i].nops * 12)) { found = s2; break; }
            if (found < 0) { found = nopd; memcpy(opdefs[nopd], defs[i].ops, defs[i].nops * 12); nopd += defs[i].nops; }
            start[i] = defs[i].nops ? found : 0;
        }
        fprintf(f, "static const struct dec_def dec_defs[%d] = {\n    {0, 0, 0, 0, 0, 0, 0, 0}", ndefs);
        for (i = 1; i < ndefs; i++) {
            const def_t* x = &defs[i];
            fprintf(f, ",\n    {0x%08x, 0x%04x, %u, %u, 0x%03x, %u, %u, %u}", x->rec, x->imm, x->mn, start[i], x->pfx, x->cat, x->nops, x->nvis);
        }
        fprintf(f, "\n};\n\n/* Operand definitions: the kind, visibility, actions and encoding; the size in bytes at\n * each operand width; the register or memory base an implicit one names */\n");
        fprintf(f, "static const uint32_t dec_opdefs[%d][3] = {\n", nopd ? nopd : 1);
        for (i = 0; i < nopd; i++) fprintf(f, "    {0x%08x, 0x%08x, 0x%08x}%s\n", opdefs[i][0], opdefs[i][1], opdefs[i][2], i + 1 < nopd ? "," : "");
        if (!nopd) fprintf(f, "    {0, 0, 0}\n");
        fprintf(f, "};\n\n");
        fprintf(f, "/* Forms: a definition, its operand width index (0 16, 1 32, 2 64), its operand width and\n * its address width without and with 67 */\nstatic const struct dec_form dec_forms[%d] = {\n    {0, 0, 0, 0, 0}", nforms);
        for (i = 1; i < nforms; i++) fprintf(f, ",\n    {%u, %u, %u, %u, %u}", forms[i].def, forms[i].eosz, forms[i].opw, forms[i].asw0, forms[i].asw1);
        fprintf(f, "\n};\n\nstatic const uint16_t dec_alt90[12] = {");
        for (i = 0; i < 12; i++) fprintf(f, "%s%u", i ? ", " : "", alt90[i]);
        fprintf(f, "};\nstatic const uint16_t dec_altE3[12] = {");
        for (i = 0; i < 12; i++) fprintf(f, "%s%u", i ? ", " : "", altE3[i]);
        fprintf(f, "};\n\n");
        fprintf(stderr, "opdefs %d\n", nopd);
    }
    fprintf(f, "/* 3DNow!: the form of each opcode byte, at operand width 32 (no prefix), 16 (66) and 64\n * (REX.W); in memory and register forms */\nstatic const uint16_t now3d_form[3][2][256] = {\n");
    for (j = 0; j < 3; j++) { fprintf(f, "    {"); for (k = 0; k < 2; k++) { fprintf(f, "%s{", k ? ",\n     " : ""); for (i = 0; i < 256; i++) fprintf(f, "%s%u", i ? (i % 16 ? ", " : ",\n      ") : "", now3d[j][k][i]); fprintf(f, "}"); } fprintf(f, "}%s\n", j < 2 ? "," : ""); }
    fprintf(f, "};\n\nstatic const uint16_t dec_subs[%d][8] = {\n", nsubs ? nsubs : 1);
    for (i = 0; i < nsubs; i++) { fprintf(f, "    {"); for (j = 0; j < 8; j++) fprintf(f, "%s%u", j ? ", " : "", subs[i][j]); fprintf(f, "}%s\n", i + 1 < nsubs ? "," : ""); }
    fprintf(f, "};\n\nstatic const uint16_t dec_mtabs[%d][16] = {\n", nmtabs);
    for (i = 0; i < nmtabs; i++) { fprintf(f, "    {"); for (j = 0; j < 16; j++) fprintf(f, "%s0x%x", j ? ", " : "", mtabs[i][j]); fprintf(f, "}%s\n", i + 1 < nmtabs ? "," : ""); }
    fprintf(f, "};\n\nstatic const uint16_t dec_rows[%d][16] = {\n", nrows);
    for (i = 0; i < nrows; i++) { fprintf(f, "    {"); for (j = 0; j < 16; j++) fprintf(f, "%s0x%x", j ? ", " : "", rows[i][j]); fprintf(f, "}%s\n", i + 1 < nrows ? "," : ""); }
    fprintf(f, "};\n\nstatic const uint16_t dec_op_row[11][256] = {\n");
    for (i = 0; i < 11; i++) { fprintf(f, "    {"); for (j = 0; j < 256; j++) fprintf(f, "%s%u", j ? (j % 16 ? ", " : ",\n     ") : "", op_row[i][j]); fprintf(f, "}%s\n", i < 10 ? "," : ""); }
    fprintf(f, "};\n\n");
    /* Registers: class, number in it, width, the 64-bit register holding it */
    fprintf(f, "static const struct dec_reg dec_regs[%d] = {\n", ZYDIS_REGISTER_MAX_VALUE + 1);
    for (i = 0; i <= ZYDIS_REGISTER_MAX_VALUE; i++)
        fprintf(f, "    {%d, %d, %d, %d}%s\n", ZydisRegisterGetClass((ZydisRegister)i), i ? ZydisRegisterGetId((ZydisRegister)i) : 0,
                ZydisRegisterGetWidth(ZYDIS_MACHINE_MODE_LONG_64, (ZydisRegister)i), ZydisRegisterGetLargestEnclosing(ZYDIS_MACHINE_MODE_LONG_64, (ZydisRegister)i), i < ZYDIS_REGISTER_MAX_VALUE ? "," : "");
    fprintf(f, "};\n\n/* The first register of each class */\nstatic const uint16_t dec_class_first[%d] = {", ZYDIS_REGCLASS_MAX_VALUE + 1);
    for (i = 0; i <= ZYDIS_REGCLASS_MAX_VALUE; i++) fprintf(f, "%s%d", i ? ", " : "", i ? ZydisRegisterEncode((ZydisRegisterClass)i, 0) : 0);
    fprintf(f, "};\nstatic const uint8_t dec_class_count[%d] = {", ZYDIS_REGCLASS_MAX_VALUE + 1);
    for (i = 0; i <= ZYDIS_REGCLASS_MAX_VALUE; i++) { int c = 0; while (c < 64 && ZydisRegisterEncode((ZydisRegisterClass)i, (ZyanU8)c) != ZYDIS_REGISTER_NONE) c++; fprintf(f, "%s%d", i ? ", " : "", c); }
    fprintf(f, "};\n\nstatic const char* const dec_reg_names[%d] = {\n", ZYDIS_REGISTER_MAX_VALUE + 1);
    for (i = 0; i <= ZYDIS_REGISTER_MAX_VALUE; i++) { const char* s = ZydisRegisterGetString((ZydisRegister)i); fprintf(f, "    \"%s\"%s\n", s ? s : "none", i < ZYDIS_REGISTER_MAX_VALUE ? "," : ""); }
    fprintf(f, "};\n\nstatic const char* const dec_mnemonic_names[%d] = {\n    \"invalid\"", nmn);
    for (i = 1; i < nmn; i++) fprintf(f, ",\n    \"%s\"", ZydisMnemonicGetString((ZydisMnemonic)mn_zydis[i]));
    fprintf(f, "\n};\n\n/* clang-format on */\n");
    fclose(f);
    /* x64_names.h: the public numbers */
    f = fopen("x64_names.h", "w");
    fprintf(f, "/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project\n * SPDX-License-Identifier: GPL-2.0-or-later */\n\n");
    fprintf(f, "/* Generated by tools/x64_decode_gen.c: the registers, mnemonics and categories of\n * common/x64_decode.h, named as Zydis 4 names them. */\n\n#ifndef SHADPS4_X64_NAMES_H\n#define SHADPS4_X64_NAMES_H\n\n");
    fprintf(f, "enum {\n");
    for (i = 0; i <= ZYDIS_REGISTER_MAX_VALUE; i++) { char u[64]; const char* s = ZydisRegisterGetString((ZydisRegister)i); upper(u, s ? s : "none"); fprintf(f, "    X64_REG_%s%s\n", u, i < ZYDIS_REGISTER_MAX_VALUE ? "," : ""); }
    fprintf(f, "};\n#define X64_REG_COUNT %d\n\nenum {\n    X64_MN_INVALID", ZYDIS_REGISTER_MAX_VALUE + 1);
    for (i = 1; i < nmn; i++) { char u[64]; upper(u, ZydisMnemonicGetString((ZydisMnemonic)mn_zydis[i])); fprintf(f, ",\n    X64_MN_%s", u); }
    fprintf(f, ",\n    X64_MN_COUNT\n};\n\nenum {\n");
    for (i = 0; i <= ZYDIS_CATEGORY_MAX_VALUE; i++) fprintf(f, "    X64_CAT_%s%s\n", STR_INSTRUCTIONCATEGORY[i], i < ZYDIS_CATEGORY_MAX_VALUE ? "," : "");
    fprintf(f, "};\n\n#endif\n");
    fclose(f);
    fprintf(stderr, "mnemonics %d\n", nmn);
    return 0;
}
