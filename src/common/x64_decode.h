/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

/* x86-64 instructions read where they lie: each one's length and parts (prefixes, REX, VEX,
 * EVEX or XOP, the opcode and its map, ModRM, SIB, the memory operand, displacement and
 * immediates) into a small record, nothing copied and nothing allocated. In place of Zydis
 * for what the core reads of x86-64 code: the game's own (the CPU patches) and the SRT
 * walkers it writes itself (common/x64_emit.c).
 *
 * Long mode only. An instruction the decoder does not know, or one longer than 15 bytes or
 * than the bytes there are, is refused (length 0), as Zydis refuses it.
 *
 * Host independent (it reads bytes, never runs them), C89, no global state. */

#ifndef SHADPS4_X64_DECODE_H
#define SHADPS4_X64_DECODE_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Opcode maps */
enum {
    X64_MAP_ONE = 0, /* one byte */
    X64_MAP_0F = 1,
    X64_MAP_0F38 = 2,
    X64_MAP_0F3A = 3,
    X64_MAP_3DNOW = 4, /* 0F 0F: the opcode is the byte after ModRM */
    X64_MAP_EVEX5 = 5, /* EVEX maps 5 and 6 */
    X64_MAP_EVEX6 = 6,
    X64_MAP_XOP8 = 8, /* XOP maps 8 to 10 */
    X64_MAP_XOP9 = 9,
    X64_MAP_XOPA = 10
};

/* How the instruction is encoded */
enum { X64_ENC_LEGACY = 0, X64_ENC_VEX = 1, X64_ENC_EVEX = 2, X64_ENC_XOP = 3 };

typedef struct x64_insn {
    uint8_t len;
    uint8_t enc;
    uint8_t map;
    uint8_t op;   /* the opcode byte, in its map */
    uint8_t p66;  /* operand size prefix */
    uint8_t p67;  /* address size prefix */
    uint8_t rep;  /* the last of F2 and F3, or 0 */
    uint8_t lock; /* F0 */
    uint8_t seg;  /* the last segment prefix (26, 2E, 36, 3E, 64, 65), or 0 */
    uint8_t rex;  /* the REX in effect (40 to 4F), or 0 */
    uint8_t w;    /* REX.W, VEX.W, EVEX.W or XOP.W */
    uint8_t vl;   /* VEX.L / EVEX.L'L: 0 128, 1 256, 2 512 bits */
    uint8_t vvvv; /* the extra register (inverted out of the prefix): 0 to 31 */
    uint8_t pp;   /* VEX, EVEX or XOP implied prefix: 0 none, 1 66, 2 F3, 3 F2 */
    uint8_t has_modrm;
    uint8_t modrm;
    uint8_t mod; /* ModRM.mod */
    uint8_t reg; /* ModRM.reg with REX.R (and EVEX.R'): 0 to 31 */
    uint8_t rm;  /* ModRM.rm with REX.B: a register when mod is 3 */
    uint8_t has_sib;
    uint8_t sib;
    /* The memory operand (when mod is not 3) */
    int8_t base;       /* register, -1 for none */
    int8_t index;      /* register (a vector register in a VSIB), -1 for none */
    uint8_t scale;     /* 1, 2, 4 or 8 */
    uint8_t rip;       /* rip-relative: the displacement is from the instruction's end */
    uint8_t disp_size; /* 0, 1 or 4 bytes (8 for a moffs) */
    uint8_t disp_off;  /* where the displacement starts in the instruction */
    uint8_t imm_size;  /* bytes of the first immediate (or relative offset) */
    uint8_t imm_off;
    uint8_t imm2_size; /* a second immediate (ENTER, EXTRQ, INSERTQ) */
    uint8_t rel;       /* the immediate is a branch offset from the instruction's end */
    int64_t disp;
    uint64_t imm; /* sign extended to 64 bits when the instruction does so */
    uint64_t imm2;
} x64_insn_t;

/* The instruction at @p, of the @avail bytes there: its length, 0 when there is none */
unsigned x64_decode(const uint8_t* p, size_t avail, x64_insn_t* in);

/* The absolute address a rip-relative operand or relative branch of @in, at @address, reaches */
uint64_t x64_target(const x64_insn_t* in, uint64_t address);

#ifdef __cplusplus
}
#endif

#endif
