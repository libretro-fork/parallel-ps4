/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

/* Makes src/common/x64_decode_tables.h: what each x86-64 opcode path decodes to, taken from
 * Zydis 4 (the decoder the tables replace). Each opcode of each map is decoded by Zydis under
 * every prefix form (none, 66, F3, F2 and 66 with them; REX.W / VEX.W; VEX.L) and every ModRM
 * byte, and the instruction definition Zydis reaches is kept: whether it has ModRM, its
 * immediates, whether it takes LOCK and what its register operands may name. A hook in a copy of
 * Zydis's decoder hands those over; nothing of Zydis goes in the core.
 *
 * To run it, against a Zydis 4 source tree (Z) and its built objects, but Decoder.c's:
 *   cp $Z/src/Decoder.c zdec.c
 *   sed -i 's|^\(\s*\)ZydisGetInstructionEncodingInfo(node, &info);|&\n\1x64g_hook(definition,
 *     info, instruction->encoding, state->context->eosz_index);|' zdec.c
 *   sed -i 's|^#include <Zydis/Internal/SharedData.h>|&\nstatic void x64g_hook(const
 *     ZydisInstructionDefinition*, const ZydisInstructionEncodingInfo*, int, int);|' zdec.c
 *   cc -O2 -DZYDIS_STATIC_BUILD -DZYCORE_STATIC_BUILD -I$Z/include -I$Z/src
 *     -I$Z/dependencies/zycore/include x64_decode_gen.c <Zydis and Zycore objects> -o gen
 *   ./gen   (writes x64_decode_tables.h here)
 * (the two seds are one line each) */

#include "zdec.c" /* Zydis's decoder with the hook */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int g_hit; static unsigned g_rec;
static void x64g_hook(const ZydisInstructionDefinition* def, const ZydisInstructionEncodingInfo* info, int enc, int eosz) {
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
    if (regf > 31 || rmf > 31 || nds > 31) { fprintf(stderr, "opdef\n"); exit(1); }
    r |= lock << 11 | regf << 12 | rmf << 17 | nds << 22 | gather << 27 | nsdm << 28 | nssm << 29;
    /* Register checks that can fail: kinds with fewer registers than their encodings name, a
     * segment register as destination, no rip-relative operand, distinct registers */
    {
        unsigned kr = regf & 15, km = rmf & 15, kn = nds & 15;
        int plain_r = kr <= 4, plain_m = km <= 4, plain_n = kn <= 4;
        if (!plain_r || !plain_m || !plain_n || (regf >> 4) || (rmf >> 3) || gather || nsdm || nssm)
            r |= 1u << 30;
    }
    g_hit = 1; g_rec = r;
}

static ZydisDecoder dec;
static unsigned probe(const uint8_t* b, unsigned n) {
    ZydisDecodedInstruction in; uint8_t buf[32];
    memset(buf, 0, sizeof buf); memcpy(buf, b, n);
    g_hit = 0; g_rec = 0;
    ZydisDecoderDecodeInstruction(&dec, NULL, buf, 15, &in);
    return g_hit ? g_rec : 0;
}
static int full_ok(const uint8_t* b, unsigned n, unsigned* len) {
    ZydisDecodedInstruction in; uint8_t buf[32];
    memset(buf, 0, sizeof buf); memcpy(buf, b, n);
    if (!ZYAN_SUCCESS(ZydisDecoderDecodeInstruction(&dec, NULL, buf, 15, &in))) return 0;
    *len = in.length; return 1;
}

/* em: 0..3 legacy maps, 4..7 VEX maps 0..3, 8..10 XOP maps 8..10 */
static unsigned build(int em, int op, int v, int m, uint8_t* b, int alt) {
    unsigned n = 0;
    static const uint8_t cls[6][2] = {{0,0},{0x66,0},{0xF3,0},{0x66,0xF3},{0xF2,0},{0x66,0xF2}};
    if (em < 4) {
        int c = v % 6, w = v / 6, k;
        if (alt == 1) b[n++] = 0x67;
        for (k = 0; k < 2; k++) if (cls[c][k]) b[n++] = cls[c][k];
        if (w || alt == 2) b[n++] = 0x40 | (w ? 8 : 0) | (alt == 2 ? 1 : 0);
        if (em >= 1) b[n++] = 0x0F;
        if (em == 2) b[n++] = 0x38;
        if (em == 3) b[n++] = 0x3A;
    } else if (em < 8) {
        int pp = v & 3, l = v >> 2 & 1, w = v >> 3;
        if (alt == 1) b[n++] = 0x67;
        b[n++] = 0xC4; b[n++] = (uint8_t)((alt == 2 ? 0xC0 : 0xE0) | (em - 4)); b[n++] = (uint8_t)(w << 7 | 0x78 | l << 2 | pp);
    } else {
        int pp = v & 3, l = v >> 2 & 1, w = v >> 3;
        if (alt == 1) b[n++] = 0x67;
        b[n++] = 0x8F; b[n++] = (uint8_t)((alt == 2 ? 0xC0 : 0xE0) | (em)); b[n++] = (uint8_t)(w << 7 | 0x78 | l << 2 | pp);
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

#define MAXR 8192
static unsigned recs[MAXR]; static int nrecs = 1;
static uint16_t subs[MAXR][8]; static int nsubs;
static uint16_t mtabs[MAXR][16]; static int nmtabs;
static uint16_t rows[MAXR][16]; static int nrows = 1;
static uint16_t op_row[11][256];
static int rec_id(unsigned r) { int i; for (i = 0; i < nrecs; i++) if (recs[i] == r) return i; if (nrecs == MAXR) exit(2); recs[nrecs] = r; return nrecs++; }
static int find16(uint16_t (*t)[16], int* n, const uint16_t* e) { int i; for (i = 0; i < *n; i++) if (!memcmp(t[i], e, 32)) return i; memcpy(t[*n], e, 32); return (*n)++; }
static int find8(const uint16_t* e) { int i; for (i = 0; i < nsubs; i++) if (!memcmp(subs[i], e, 16)) return i; memcpy(subs[nsubs], e, 16); return nsubs++; }

int main(void) {
    int em, op, v, m, warn = 0, depdiff = 0;
    ZydisDecoderInit(&dec, ZYDIS_MACHINE_MODE_LONG_64, ZYDIS_STACK_WIDTH_64);
    for (em = 0; em < 11; em++) for (op = 0; op < 256; op++) {
        uint16_t row[16]; int nv = em < 4 ? 12 : 16;
        memset(row, 0, sizeof row);
        if (skip(em, op)) { op_row[em][op] = 0; continue; }
        for (v = 0; v < nv; v++) {
            unsigned r[256]; uint8_t b[32]; int reg, rm, uniform = 1; uint16_t mt[16];
            for (m = 0; m < 256; m++) {
                unsigned n = build(em, op, v, m, b, 0), a1, a2;
                r[m] = probe(b, n);
                n = build(em, op, v, m, b, 1); a1 = probe(b, n);
                n = build(em, op, v, m, b, 2); a2 = probe(b, n);
                if ((a1 != r[m] || a2 != r[m]) && !(em == 0 && op == 0x8F && (m & 0x1F) >= 8)) {
                    if (depdiff++ < 20) printf("dep em%d op%02x v%d m%02x: %08x 67:%08x B:%08x\n", em, op, v, m, r[m], a1, a2);
                }
                if (r[m] != r[0]) uniform = 0;
            }
            if (em == 0 && op == 0x8F) { /* XOP takes these; give them the /0 form's record so the table stays regular */
                for (m = 0; m < 256; m++) if ((m & 0x1F) >= 8) r[m] = r[(m & 0xC0) | (m & 7)];
                uniform = 1; for (m = 0; m < 256; m++) if (r[m] != r[0]) uniform = 0; }
            if (uniform) { row[v] = (uint16_t)(0x8000 | rec_id(r[0])); continue; }
            for (reg = 0; reg < 8; reg++) {
                unsigned base = r[reg << 3]; int same = 1; uint16_t sub[8];
                uint16_t msub[8]; int msame = 1;
                (void)base;
                for (m = 0; m < 192; m++) if ((m >> 3 & 7) == reg && r[m] != r[reg << 3 | (m & 7)]) { if (warn++ < 20) printf("mod varies em%d op%02x v%d reg%d m%02x\n", em, op, v, reg, m); }
                for (rm = 0; rm < 8; rm++) { msub[rm] = (uint16_t)rec_id(r[reg << 3 | rm]); if (msub[rm] != msub[0]) msame = 0; }
                mt[reg] = msame ? msub[0] : (uint16_t)(0x8000 | find8(msub));
                for (rm = 0; rm < 8; rm++) { sub[rm] = (uint16_t)rec_id(r[0xC0 | reg << 3 | rm]); if (sub[rm] != sub[0]) same = 0; }
                mt[8 + reg] = same ? sub[0] : (uint16_t)(0x8000 | find8(sub));
            }
            row[v] = (uint16_t)find16(mtabs, &nmtabs, mt);
        }
        op_row[em][op] = (uint16_t)find16(rows, &nrows, row);
    }
    fprintf(stderr, "recs %d subs %d mtabs %d rows %d warn %d depdiff %d\n", nrecs, nsubs, nmtabs, nrows, warn, depdiff);
    /* 3DNow suffixes */
    {
        int c, s, k; unsigned len, ok[2][256];
        static const uint8_t cls[6][2] = {{0,0},{0x66,0},{0xF3,0},{0x66,0xF3},{0xF2,0},{0x66,0xF2}};
        for (c = 0; c < 6; c++) for (k = 0; k < 2; k++) for (s = 0; s < 256; s++) {
            uint8_t b[16]; unsigned n = 0, o;
            if (cls[c][0]) b[n++] = cls[c][0]; if (cls[c][1]) b[n++] = cls[c][1];
            b[n++] = 0x0F; b[n++] = 0x0F; b[n++] = k ? 0xC0 : 0x00; b[n++] = (uint8_t)s;
            o = full_ok(b, n, &len);
            if (c == 0) ok[k][s] = o; else if (o != ok[k][s]) printf("3dnow class %d differs at %02x\n", c, s);
        }
        FILE* f = fopen("x64_decode_tables.h", "w");
        fprintf(f, "/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project\n * SPDX-License-Identifier: GPL-2.0-or-later */\n\n");
        fprintf(f, "/* Generated by tools/x64_decode_gen.c: what each opcode path of x86-64 decodes to, as Zydis\n * 4 decodes it. Included by common/x64_decode.c only. */\n\n");
        fprintf(f, "static const uint8_t now3d_ok[2][32] = {\n");
        for (k = 0; k < 2; k++) { fprintf(f, "    {"); for (s = 0; s < 32; s++) { unsigned bits = 0, t; for (t = 0; t < 8; t++) bits |= ok[k][s * 8 + t] << t; fprintf(f, "%s0x%02x", s ? ", " : "", bits); } fprintf(f, "}%s\n", k ? "" : ","); }
        fprintf(f, "};\n\n");
        fclose(f);
    }
    {
        FILE* f = fopen("x64_decode_tables.h", "a"); int i, j;
        fprintf(f, "static const uint32_t dec_recs[%d] = {", nrecs);
        for (i = 0; i < nrecs; i++) fprintf(f, "%s0x%08x", i % 8 ? ", " : i ? ",\n    " : "\n    ", recs[i]);
        fprintf(f, "\n};\n\nstatic const uint16_t dec_subs[%d][8] = {\n", nsubs ? nsubs : 1);
        for (i = 0; i < nsubs; i++) { fprintf(f, "    {"); for (j = 0; j < 8; j++) fprintf(f, "%s%u", j ? ", " : "", subs[i][j]); fprintf(f, "},\n"); }
        if (!nsubs) fprintf(f, "    {0}\n");
        fprintf(f, "};\n\nstatic const uint16_t dec_mtabs[%d][16] = {\n", nmtabs);
        for (i = 0; i < nmtabs; i++) { fprintf(f, "    {"); for (j = 0; j < 16; j++) fprintf(f, "%s0x%x", j ? ", " : "", mtabs[i][j]); fprintf(f, "},\n"); }
        fprintf(f, "};\n\nstatic const uint16_t dec_rows[%d][16] = {\n", nrows);
        for (i = 0; i < nrows; i++) { fprintf(f, "    {"); for (j = 0; j < 16; j++) fprintf(f, "%s0x%x", j ? ", " : "", rows[i][j]); fprintf(f, "},\n"); }
        fprintf(f, "};\n\nstatic const uint16_t dec_op_row[11][256] = {\n");
        for (i = 0; i < 11; i++) { fprintf(f, "    {"); for (j = 0; j < 256; j++) fprintf(f, "%s%u", j ? (j % 16 ? ", " : ",\n     ") : "", op_row[i][j]); fprintf(f, "}%s\n", i < 10 ? "," : ""); }
        fprintf(f, "};\n");
        fclose(f);
    }
    return 0;
}
