/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

/* x86-64 machine code, written straight into the caller's memory: the instructions the core
 * makes at run time and nothing more. That is the SRT walkers (x86-64 on every host: an
 * aarch64 host interprets them), the stubs for symbols a game imports and nothing exports, and
 * the patches and trampolines for guest instructions the host CPU lacks. In place of Xbyak, and
 * byte for byte what Xbyak wrote for each of them (libretro/tests/x64_emit_test.c).
 *
 * The code goes where it is to run: a rip-relative operand and a jump are given the absolute
 * address they reach, and the offset is worked out from where the instruction lands. Nothing
 * is allocated and nothing is thrown: an instruction that does not fit sets x64_code_t.error,
 * and nothing more is written. The caller checks once after a run of instructions.
 *
 * Host independent (it writes bytes, never runs them), C89, no global state. */

#ifndef SHADPS4_X64_EMIT_H
#define SHADPS4_X64_EMIT_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* General registers by their encoding; 8-, 32- and 64-bit forms share the numbers (an 8-bit
 * 4 to 7 is spl to dil, never ah to bh) */
enum {
    X64_RAX = 0,
    X64_RCX,
    X64_RDX,
    X64_RBX,
    X64_RSP,
    X64_RBP,
    X64_RSI,
    X64_RDI,
    X64_R8,
    X64_R9,
    X64_R10,
    X64_R11,
    X64_R12,
    X64_R13,
    X64_R14,
    X64_R15
};

/* Segment overrides of a memory operand */
enum { X64_SEG_NONE = 0, X64_SEG_FS = 0x64, X64_SEG_GS = 0x65 };

/* Condition codes (the low nibble of Jcc, SETcc, CMOVcc) */
enum { X64_CC_B = 2, X64_CC_Z = 4, X64_CC_A = 7 };

/* Group 1 arithmetic, as their /digit */
enum { X64_ADD = 0, X64_OR = 1, X64_AND = 4, X64_SUB = 5, X64_XOR = 6, X64_CMP = 7 };

/* Shifts, as their /digit */
enum { X64_SHL = 4, X64_SHR = 5 };

typedef struct x64_code {
    uint8_t* p;  /* where the code is written, and where it runs */
    size_t size; /* bytes written */
    size_t cap;  /* bytes room */
    int error;   /* an instruction did not fit: nothing written since */
} x64_code_t;

/* A memory operand. No base and no index: an absolute 32-bit address. rip: the absolute
 * address @target, reached rip-relative. */
typedef struct x64_mem {
    int8_t base;  /* a register, or -1 */
    int8_t index; /* a register (an xmm for a gather), or -1 */
    uint8_t scale;
    uint8_t seg;
    uint8_t rip;
    int32_t disp;
    uint64_t target;
} x64_mem_t;

/* [base + disp]; [base + index * scale + disp]; rip-relative to @target; [address] */
x64_mem_t x64_m(int base, int32_t disp);
x64_mem_t x64_mi(int base, int index, int scale, int32_t disp);
x64_mem_t x64_mrip(uint64_t target);
x64_mem_t x64_mabs(int32_t address);

void x64_init(x64_code_t* c, void* mem, size_t cap);
/* The address the next instruction lands at */
uint8_t* x64_curr(const x64_code_t* c);

void x64_db(x64_code_t* c, const void* bytes, size_t n);
void x64_dq(x64_code_t* c, uint64_t v);
/* @n bytes of no-ops, the longest forms first */
void x64_nop(x64_code_t* c, size_t n);

/* @size: 8, 32 or 64 */
void x64_mov_ri(x64_code_t* c, int size, int r, uint64_t imm);
void x64_mov_rr(x64_code_t* c, int size, int dst, int src);
void x64_mov_rm(x64_code_t* c, int size, int r, x64_mem_t m);
void x64_mov_mr(x64_code_t* c, int size, x64_mem_t m, int r);
void x64_alu_ri(x64_code_t* c, int op, int size, int r, uint32_t imm);
void x64_alu_rr(x64_code_t* c, int op, int size, int dst, int src);
void x64_alu_rm(x64_code_t* c, int op, int size, int r, x64_mem_t m);
void x64_alu_mr(x64_code_t* c, int op, int size, x64_mem_t m, int r);
void x64_imul_rri(x64_code_t* c, int size, int dst, int src, uint32_t imm);
void x64_imul_rm(x64_code_t* c, int size, int r, x64_mem_t m);
void x64_shift_ri(x64_code_t* c, int op, int size, int r, uint8_t count);
void x64_shift_rcl(x64_code_t* c, int op, int size, int r);
void x64_shift_mcl(x64_code_t* c, int op, int size, x64_mem_t m);
void x64_not(x64_code_t* c, int size, int r);
void x64_neg(x64_code_t* c, int size, int r);
void x64_dec(x64_code_t* c, int size, int r);
void x64_cmov_rr(x64_code_t* c, int cc, int size, int dst, int src);
void x64_cmov_rm(x64_code_t* c, int cc, int size, int r, x64_mem_t m);
void x64_lea(x64_code_t* c, int r, x64_mem_t m);
void x64_xchg_rm(x64_code_t* c, int r, x64_mem_t m);
void x64_push(x64_code_t* c, int r);
void x64_pop(x64_code_t* c, int r);
void x64_pushfq(x64_code_t* c);
void x64_popfq(x64_code_t* c);
void x64_ret(x64_code_t* c);
void x64_call_m(x64_code_t* c, x64_mem_t m);
void x64_jmp_r(x64_code_t* c, int r);

/* How x64_jmp reaches: 2 bytes when the target is near enough, else 5; always 5; always 2
 * (an error when the target is not near enough) */
enum { X64_JMP_AUTO = 0, X64_JMP_NEAR = 1, X64_JMP_SHORT = 2 };
/* A jump to @target */
void x64_jmp(x64_code_t* c, uint64_t target, int mode);
/* A short jump forward, its target bound later (at most 127 bytes on, else an error): the
 * offset of its displacement byte */
size_t x64_jmp_fwd(x64_code_t* c);
size_t x64_jcc_fwd(x64_code_t* c, int cc);
/* The forward jump at @at lands here */
void x64_bind(x64_code_t* c, size_t at);

struct x64_insn;
/* A copy of the guest instruction @in (decoded by common/x64_decode.h from @bytes, at @from),
 * written to run where the code goes: byte for byte, but that a relative branch (in its own
 * form, short or near) reaches the same target and a rip-relative operand the same address.
 * 0 when the copy cannot reach (nothing is written, and no error set); also 0, with the error
 * set, when there is no room. */
int x64_relocate(x64_code_t* c, const uint8_t* bytes, const struct x64_insn* in, uint64_t from);

/* SSE and AVX on xmm registers (@l: 0) and ymm registers (@l: 1) */
void x64_vmovups_rm(x64_code_t* c, int l, int x, x64_mem_t m);
void x64_vmovups_mr(x64_code_t* c, int l, x64_mem_t m, int x);
void x64_vmovaps_rr(x64_code_t* c, int l, int dst, int src);
void x64_vmovq_xr(x64_code_t* c, int x, int r); /* xmm <- r64 */
void x64_vmovq_rx(x64_code_t* c, int r, int x); /* r64 <- xmm */
void x64_vcmpps(x64_code_t* c, int l, int dst, int src1, int src2, uint8_t pred);
void x64_vcmpss_rrr(x64_code_t* c, int dst, int src1, int src2, uint8_t pred);
void x64_vcmpss_rrm(x64_code_t* c, int dst, int src1, x64_mem_t m, uint8_t pred);
void x64_vpcmpeqd(x64_code_t* c, int l, int dst, int src1, int src2);
void x64_vpsrld_ri(x64_code_t* c, int l, int dst, int src, uint8_t count);
void x64_vpslld_ri(x64_code_t* c, int l, int dst, int src, uint8_t count);
void x64_vxorps_rrr(x64_code_t* c, int l, int dst, int src1, int src2);
void x64_vxorps_rrm(x64_code_t* c, int l, int dst, int src1, x64_mem_t m);
void x64_vrcpps(x64_code_t* c, int l, int dst, int src);
void x64_vrsqrtps(x64_code_t* c, int l, int dst, int src);
void x64_rcpps(x64_code_t* c, int dst, int src);
void x64_rsqrtps(x64_code_t* c, int dst, int src);
void x64_xorps(x64_code_t* c, int dst, int src);
void x64_vblendvps(x64_code_t* c, int l, int dst, int src1, int src2, int mask);
void x64_vpextrq(x64_code_t* c, int r, int x, uint8_t lane);
/* dst <- gather of floats at m (an xmm index), lanes set in @mask */
void x64_vgatherdps(x64_code_t* c, int l, int dst, x64_mem_t m, int mask);

#ifdef __cplusplus
}
#endif

#endif
