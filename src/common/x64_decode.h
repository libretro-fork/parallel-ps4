/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

/* x86-64 instructions read where they lie: each one's length and parts (prefixes, REX, VEX,
 * EVEX or XOP, the opcode and its map, ModRM, SIB, the memory operand, displacement and
 * immediates) into a small record, nothing copied and nothing allocated; its mnemonic and
 * category; and, asked for, its operands with what it does to each (x64_operands) and the
 * Intel syntax text of it (x64_format). In place of Zydis for what the core reads of x86-64
 * code: the game's own (the CPU patches) and the SRT walkers it writes itself
 * (common/x64_emit.c). Mnemonics, registers, categories and operands are Zydis 4's, numbered
 * and named as there (common/x64_names.h).
 *
 * Long mode only. An instruction the decoder does not know, or one longer than 15 bytes or
 * than the bytes there are, is refused (length 0), as Zydis refuses it.
 *
 * Host independent (it reads bytes, never runs them), C89, no global state. */

#ifndef SHADPS4_X64_DECODE_H
#define SHADPS4_X64_DECODE_H

#include <stddef.h>
#include <stdint.h>

#include "common/x64_names.h"

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
    uint8_t fsgs; /* the last of 64 and 65 (the only ones long mode heeds), or 0 */
    uint8_t notrack; /* a 3E after the last 64 or 65: NOTRACK, for an indirect branch */
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
    /* What it is */
    uint16_t mnemonic;     /* X64_MN_ */
    uint8_t category;      /* X64_CAT_ */
    uint8_t operand_width; /* bits: 8, 16, 32 or 64 (0 for none) */
    uint8_t address_width; /* bits: 32 or 64 */
    uint8_t operand_count; /* with the hidden ones */
    uint8_t operand_count_visible;
    uint8_t has_rex; /* a legacy REX prefix (in effect) */
    uint16_t form;   /* the decoder's own */
} x64_insn_t;

/* Operands, as Zydis gives them */
enum {
    X64_OPERAND_UNUSED,
    X64_OPERAND_REGISTER,
    X64_OPERAND_MEMORY,
    X64_OPERAND_POINTER,
    X64_OPERAND_IMMEDIATE
};
enum { X64_VISIBILITY_EXPLICIT = 1, X64_VISIBILITY_IMPLICIT = 2, X64_VISIBILITY_HIDDEN = 3 };
enum {
    X64_ACTION_READ = 1,
    X64_ACTION_WRITE = 2,
    X64_ACTION_CONDREAD = 4,
    X64_ACTION_CONDWRITE = 8
};
#define X64_ACTION_MASK_READ (X64_ACTION_READ | X64_ACTION_CONDREAD)
#define X64_ACTION_MASK_WRITE (X64_ACTION_WRITE | X64_ACTION_CONDWRITE)
/* Memory operand kinds: an access, an address computed only (LEA), MPX's, a vector index */
enum { X64_MEMOP_MEM = 1, X64_MEMOP_AGEN = 2, X64_MEMOP_MIB = 3, X64_MEMOP_VSIB = 4 };
#define X64_MAX_OPERANDS 10

typedef struct x64_operand {
    uint8_t type;
    uint8_t visibility;
    uint8_t actions;
    uint8_t encoding; /* how the instruction names it: Zydis's ZydisOperandEncoding */
    uint16_t size;    /* bits */
    uint16_t reg;     /* X64_REG_ */
    struct {
        uint8_t type;
        uint8_t scale;     /* 0 without an index */
        uint8_t disp_size; /* bytes, 0 for none */
        uint16_t segment, base, index;
        int64_t disp;
    } mem;
    struct {
        uint8_t is_signed, is_relative;
        uint64_t value; /* sign extended when signed */
    } imm;
    struct {
        uint16_t segment;
        uint32_t offset;
    } ptr;
} x64_operand_t;

/* The instruction at @p, of the @avail bytes there: its length, 0 when there is none */
unsigned x64_decode(const uint8_t* p, size_t avail, x64_insn_t* in);

/* The absolute address a rip-relative operand or relative branch of @in, at @address, reaches */
uint64_t x64_target(const x64_insn_t* in, uint64_t address);

/* The operands of @in into @out: their count, the hidden ones after the visible ones */
unsigned x64_operands(const x64_insn_t* in, x64_operand_t out[X64_MAX_OPERANDS]);

/* Intel syntax, as Zydis's formatter writes it, of @in at @address (branch targets are absolute)
 * into @buf (@size bytes, always terminated): the length written */
size_t x64_format(const x64_insn_t* in, const x64_operand_t* ops, uint64_t address, char* buf,
                  size_t size);

/* The absolute address operand @o of @in at @address reaches, as Zydis works it out: a relative
 * branch's target, a rip-relative operand's address, an absolute address. 0 when it has none. */
int x64_operand_address(const x64_insn_t* in, const x64_operand_t* o, uint64_t address,
                        uint64_t* out);

const char* x64_mnemonic_name(unsigned mnemonic);
const char* x64_register_name(unsigned reg);
/* The 64-bit register a register is part of (rax for al, ax, eax); itself if none */
unsigned x64_register_largest(unsigned reg);
unsigned x64_register_width(unsigned reg);

#ifdef __cplusplus
}
#endif

#endif
