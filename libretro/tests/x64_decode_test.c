/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

/* The x86-64 decoder (src/common/x64_decode.c), against what Zydis 4 decoded of the same bytes,
 * which the decoder replaces. Zydis made the answers below once, when the decoder was written:
 * for the cases listed (the forms that told the two apart while the decoder was being written,
 * and the usual ones) the length (0: refused), the displacement and immediate sizes and the
 * Intel syntax text Zydis's formatter wrote; and for 1,048,576 instruction-shaped byte strings
 * made here, some cut short, a digest of everything decoded (length, opcode, ModRM,
 * displacement and immediates with their offsets and values) and one of the rest (mnemonic,
 * category, widths, every operand as Zydis gives it, and the text). The two decoders also
 * agreed at every offset of a game's 7.3 MB of code, in all of it (text too), and over 200
 * million such strings. AVX-512 and Knights Corner (62) are left out: the decoder refuses
 * them. */

#include <stdio.h>
#include <string.h>

#include "common/x64_decode.h"

#define CHUNKS 16
#define CHUNK_CASES 65536
#define FNV_BASIS 0xcbf29ce484222325ull

struct known {
    const char* bytes;
    unsigned n, len, disp_size, imm_size;
    const char* text; /* at address 0x140001000 */
};

#define KNOWN_ADDRESS 0x140001000ull

static unsigned long long rnd(unsigned long long* s) {
    *s ^= *s << 13;
    *s ^= *s >> 7;
    *s ^= *s << 17;
    return *s;
}

static unsigned long long case_seed(unsigned chunk) {
    return 0x9e3779b97f4a7c15ull * (chunk + 1);
}

/* An instruction-shaped byte string: a few legacy prefixes, then one-byte, 0F, 0F 38, 0F 3A,
 * VEX (C5, C4), XOP or 3DNow! opcodes, then anything; one in eight with fewer bytes there */
static void case_make(unsigned long long* s, unsigned char b[24], unsigned* avail) {
    static const unsigned char pf[18] = {0x66, 0x67, 0xF2, 0xF3, 0xF0, 0x2E, 0x3E, 0x26, 0x36,
                                         0x64, 0x65, 0x40, 0x41, 0x44, 0x48, 0x4C, 0x4F, 0x45};
    unsigned k = 0, j, np, kind;
    for (j = 0; j < 24; j++)
        b[j] = (unsigned char)rnd(s);
    np = (unsigned)(rnd(s) % 4);
    kind = (unsigned)(rnd(s) % 8);
    for (j = 0; j < np && (rnd(s) & 1); j++)
        b[k++] = pf[rnd(s) % sizeof(pf)];
    switch (kind) {
    case 2:
        b[k++] = 0x0F;
        break;
    case 3:
        b[k++] = 0x0F;
        b[k++] = (rnd(s) & 1) ? 0x38 : 0x3A;
        break;
    case 4:
        b[k++] = 0xC5;
        break;
    case 5:
        b[k++] = 0xC4;
        b[k] = (unsigned char)((b[k] & 0xE0) | (rnd(s) % 4));
        k++;
        break;
    case 6:
        b[k++] = 0x8F;
        b[k] = (unsigned char)((b[k] & 0xE0) | (8 + rnd(s) % 3));
        k++;
        break;
    case 7:
        b[k++] = 0x0F;
        b[k++] = 0x0F;
        break;
    default:
        break;
    }
    if (b[k] == 0x62 && kind < 2)
        b[k] = 0x90;
    *avail = rnd(s) % 8 == 0 ? (unsigned)(rnd(s) % 16) : 24;
}

/* Where case @i of a chunk is taken to lie, for its branch targets and rip-relative operands */
static unsigned long long case_address(unsigned i) {
    return 0x7FF600000000ull + (unsigned long long)i * 0x1234567ull;
}

static unsigned long long fold(unsigned long long h, unsigned long long v, unsigned bytes) {
    unsigned i;
    for (i = 0; i < bytes; i++) {
        h ^= (v >> (8 * i)) & 0xFF;
        h *= 0x100000001b3ull;
    }
    return h;
}

static unsigned long long case_digest(unsigned long long h, const x64_insn_t* in) {
    h = fold(h, in->len, 1);
    if (!in->len)
        return h;
    h = fold(h, in->op, 1);
    h = fold(h, in->has_modrm, 1);
    if (in->has_modrm)
        h = fold(h, in->modrm, 1);
    h = fold(h, in->disp_size, 1);
    h = fold(h, in->disp_off, 1);
    h = fold(h, (unsigned long long)in->disp, 8);
    h = fold(h, in->imm_size, 1);
    h = fold(h, in->imm_off, 1);
    h = fold(h, in->imm, 8);
    h = fold(h, in->rel, 1);
    h = fold(h, in->imm2_size, 1);
    h = fold(h, in->imm2, 8);
    return h;
}

static unsigned long long fold_text(unsigned long long h, const char* s) {
    for (; *s; s++)
        h = fold(h, (unsigned char)*s, 1);
    return fold(h, 0, 1);
}

/* Mnemonic, category, widths, operands; @text, as written at the case's address */
static unsigned long long case_digest_ops(unsigned long long h, const x64_insn_t* in,
                                          const x64_operand_t* ops, unsigned n, const char* text) {
    unsigned i;
    if (!in->len)
        return fold(h, 0, 1);
    h = fold_text(h, x64_mnemonic_name(in->mnemonic));
    h = fold(h, in->category, 1);
    h = fold(h, in->operand_width, 1);
    h = fold(h, in->address_width, 1);
    h = fold(h, n, 1);
    h = fold(h, in->operand_count_visible, 1);
    for (i = 0; i < n; i++) {
        const x64_operand_t* o = &ops[i];
        h = fold(h, o->type, 1);
        h = fold(h, o->visibility, 1);
        h = fold(h, o->actions, 1);
        h = fold(h, o->encoding, 1);
        h = fold(h, o->size, 2);
        switch (o->type) {
        case X64_OPERAND_REGISTER:
            h = fold(h, o->reg, 2);
            break;
        case X64_OPERAND_MEMORY:
            h = fold(h, o->mem.type, 1);
            h = fold(h, o->mem.segment, 2);
            h = fold(h, o->mem.base, 2);
            h = fold(h, o->mem.index, 2);
            h = fold(h, o->mem.scale, 1);
            h = fold(h, o->mem.disp_size, 1);
            h = fold(h, o->mem.disp_size ? (unsigned long long)o->mem.disp : 0, 8);
            break;
        case X64_OPERAND_IMMEDIATE:
            h = fold(h, o->imm.value, 8);
            h = fold(h, o->imm.is_signed, 1);
            h = fold(h, o->imm.is_relative, 1);
            break;
        case X64_OPERAND_POINTER:
            h = fold(h, o->ptr.segment, 2);
            h = fold(h, o->ptr.offset, 4);
            break;
        default:
            break;
        }
    }
    return fold_text(h, text);
}

#ifndef X64_DECODE_TEST_NO_MAIN

#include "x64_decode_test_zydis.h"

int main(void) {
    unsigned i, c, failed = 0;
    x64_insn_t in;
    x64_operand_t ops[X64_MAX_OPERANDS];
    char text[256];
    for (i = 0; i < sizeof(known) / sizeof(known[0]); i++) {
        const struct known* k = &known[i];
        const unsigned len = x64_decode((const uint8_t*)k->bytes, k->n, &in);
        text[0] = 0;
        if (len) {
            x64_operands(&in, ops);
            x64_format(&in, ops, KNOWN_ADDRESS, text, sizeof(text));
        }
        if (len != k->len || (len && (in.disp_size != k->disp_size || in.imm_size != k->imm_size)) ||
            strcmp(text, k->text) != 0) {
            unsigned j;
            printf("FAIL:");
            for (j = 0; j < k->n; j++)
                printf(" %02x", (unsigned char)k->bytes[j]);
            printf(": length %u disp %u imm %u \"%s\", Zydis %u %u %u \"%s\"\n", len, in.disp_size,
                   in.imm_size, text, k->len, k->disp_size, k->imm_size, k->text);
            failed++;
        }
    }
    for (c = 0; c < CHUNKS; c++) {
        unsigned long long s = case_seed(c), h = FNV_BASIS, h2 = FNV_BASIS;
        for (i = 0; i < CHUNK_CASES; i++) {
            unsigned char b[24];
            unsigned avail, n = 0;
            case_make(&s, b, &avail);
            text[0] = 0;
            if (x64_decode(b, avail, &in)) {
                n = x64_operands(&in, ops);
                x64_format(&in, ops, case_address(i), text, sizeof(text));
            }
            h = case_digest(h, &in);
            h2 = case_digest_ops(h2, &in, ops, n, text);
        }
        if (h != chunk_digest[c]) {
            printf("FAIL: generated cases, chunk %u: digest %016llx, Zydis %016llx\n", c, h,
                   chunk_digest[c]);
            failed++;
        }
        if (h2 != chunk_digest_ops[c]) {
            printf("FAIL: generated cases, chunk %u: operands and text %016llx, Zydis %016llx\n",
                   c, h2, chunk_digest_ops[c]);
            failed++;
        }
    }
    if (failed) {
        printf("x64_decode_test: %u failed\n", failed);
        return 1;
    }
    printf("x64_decode_test: %u known forms and %u generated cases as Zydis decodes and writes "
           "them\n",
           (unsigned)(sizeof(known) / sizeof(known[0])), CHUNKS * CHUNK_CASES);
    return 0;
}

#endif
