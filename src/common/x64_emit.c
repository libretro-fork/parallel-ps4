/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <string.h>

#include "common/x64_decode.h"
#include "common/x64_emit.h"

x64_mem_t x64_m(int base, int32_t disp) {
    x64_mem_t m;
    m.base = (int8_t)base;
    m.index = -1;
    m.scale = 1;
    m.seg = X64_SEG_NONE;
    m.rip = 0;
    m.disp = disp;
    m.target = 0;
    return m;
}

x64_mem_t x64_mi(int base, int index, int scale, int32_t disp) {
    x64_mem_t m = x64_m(base, disp);
    m.index = (int8_t)index;
    m.scale = (uint8_t)scale;
    return m;
}

x64_mem_t x64_mrip(uint64_t target) {
    x64_mem_t m = x64_m(-1, 0);
    m.rip = 1;
    m.target = target;
    return m;
}

x64_mem_t x64_mabs(int32_t address) {
    return x64_m(-1, address);
}

void x64_init(x64_code_t* c, void* mem, size_t cap) {
    c->p = (uint8_t*)mem;
    c->size = 0;
    c->cap = cap;
    c->error = 0;
}

uint8_t* x64_curr(const x64_code_t* c) {
    return c->p + c->size;
}

/* Room for @n more bytes; an instruction that does not fit is not begun */
static int room(x64_code_t* c, size_t n) {
    if (c->error || c->cap - c->size < n) {
        c->error = 1;
        return 0;
    }
    return 1;
}

static void b(x64_code_t* c, uint8_t v) {
    c->p[c->size++] = v;
}

static void d32(x64_code_t* c, uint32_t v) {
    memcpy(c->p + c->size, &v, 4);
    c->size += 4;
}

static void d64(x64_code_t* c, uint64_t v) {
    memcpy(c->p + c->size, &v, 8);
    c->size += 8;
}

static int is_s8(int64_t v) {
    return v >= -128 && v <= 127;
}

/* The longest an instruction gets, with its immediates */
#define MAX_INSN 16

void x64_db(x64_code_t* c, const void* bytes, size_t n) {
    if (!room(c, n)) {
        return;
    }
    memcpy(c->p + c->size, bytes, n);
    c->size += n;
}

void x64_dq(x64_code_t* c, uint64_t v) {
    if (room(c, 8)) {
        d64(c, v);
    }
}

void x64_nop(x64_code_t* c, size_t n) {
    /* Xbyak's multi-byte no-ops, the longest that fits first */
    static const uint8_t nops[9][9] = {
        {0x90},
        {0x66, 0x90},
        {0x0F, 0x1F, 0x00},
        {0x0F, 0x1F, 0x40, 0x00},
        {0x0F, 0x1F, 0x44, 0x00, 0x00},
        {0x66, 0x0F, 0x1F, 0x44, 0x00, 0x00},
        {0x0F, 0x1F, 0x80, 0x00, 0x00, 0x00, 0x00},
        {0x0F, 0x1F, 0x84, 0x00, 0x00, 0x00, 0x00, 0x00},
        {0x66, 0x0F, 0x1F, 0x84, 0x00, 0x00, 0x00, 0x00, 0x00},
    };
    if (!room(c, n)) {
        return;
    }
    while (n > 0) {
        const size_t len = n > 9 ? 9 : n;
        memcpy(c->p + c->size, nops[len - 1], len);
        c->size += len;
        n -= len;
    }
}

/* The register field and r/m of an instruction's ModRM: a register, or memory */
typedef struct rm {
    int mem;
    int r;
    x64_mem_t m;
} rm_t;

static rm_t rm_reg(int r) {
    rm_t o;
    o.mem = 0;
    o.r = r;
    o.m = x64_m(-1, 0);
    return o;
}

static rm_t rm_mem(x64_mem_t m) {
    rm_t o;
    o.mem = 1;
    o.r = -1;
    o.m = m;
    return o;
}

/* REX's X and B of @o */
static int rex_xb(const rm_t* o) {
    int v = 0;
    if (!o->mem) {
        return (o->r & 8) ? 1 : 0;
    }
    if (!o->m.rip && o->m.index >= 0 && (o->m.index & 8)) {
        v |= 2;
    }
    if (!o->m.rip && o->m.base >= 0 && (o->m.base & 8)) {
        v |= 1;
    }
    return v;
}

/* ModRM, SIB and displacement of @o with @reg in the register field; @imm the bytes that
 * follow (a rip-relative offset is from the end of the instruction) */
static void modrm(x64_code_t* c, int reg, const rm_t* o, int imm) {
    const x64_mem_t* m = &o->m;
    int base, mod;
    reg &= 7;
    if (!o->mem) {
        b(c, (uint8_t)(0xC0 | (reg << 3) | (o->r & 7)));
        return;
    }
    if (m->rip) {
        const uint64_t end = (uint64_t)(uintptr_t)(c->p + c->size) + 5 + (uint64_t)imm;
        b(c, (uint8_t)((reg << 3) | 5));
        d32(c, (uint32_t)(m->target - end));
        return;
    }
    if (m->base < 0) {
        /* No base: [index * scale + disp32], or [disp32] through a SIB with no index */
        const int index = m->index < 0 ? 4 : (m->index & 7);
        const int ss = m->scale == 8 ? 3 : m->scale == 4 ? 2 : m->scale == 2 ? 1 : 0;
        b(c, (uint8_t)((reg << 3) | 4));
        b(c, (uint8_t)((ss << 6) | (index << 3) | 5));
        d32(c, (uint32_t)m->disp);
        return;
    }
    base = m->base & 7;
    if (m->disp == 0 && base != 5) {
        mod = 0;
    } else if (is_s8(m->disp)) {
        mod = 1;
    } else {
        mod = 2;
    }
    if (m->index >= 0) {
        const int ss = m->scale == 8 ? 3 : m->scale == 4 ? 2 : m->scale == 2 ? 1 : 0;
        b(c, (uint8_t)((mod << 6) | (reg << 3) | 4));
        b(c, (uint8_t)((ss << 6) | ((m->index & 7) << 3) | base));
    } else if (base == 4) {
        b(c, (uint8_t)((mod << 6) | (reg << 3) | 4));
        b(c, 0x24);
    } else {
        b(c, (uint8_t)((mod << 6) | (reg << 3) | base));
    }
    if (mod == 1) {
        b(c, (uint8_t)(int8_t)m->disp);
    } else if (mod == 2) {
        d32(c, (uint32_t)m->disp);
    }
}

/* An 8-bit register that needs a REX to be named (spl, bpl, sil, dil) */
static int byte_rex(int size, int r) {
    return size == 8 && r >= 4 && r <= 7;
}

/* A legacy instruction: segment, 0x66 for 16 bits, REX, the @n opcode bytes, ModRM */
static void legacy(x64_code_t* c, int size, int reg, const rm_t* o, const uint8_t* op, int n,
                   int imm) {
    int rex = 0;
    int i;
    if (o->mem && o->m.seg) {
        b(c, o->m.seg);
    }
    if (size == 16) {
        b(c, 0x66);
    }
    if (size == 64) {
        rex |= 8;
    }
    if (reg >= 0 && (reg & 8)) {
        rex |= 4;
    }
    rex |= rex_xb(o);
    if (rex || (reg >= 0 && byte_rex(size, reg)) || (!o->mem && byte_rex(size, o->r))) {
        b(c, (uint8_t)(0x40 | rex));
    }
    for (i = 0; i < n; i++) {
        b(c, op[i]);
    }
    modrm(c, reg < 0 ? 0 : reg, o, imm);
}

/* The same with a /digit in the register field */
static void legacy_digit(x64_code_t* c, int size, int digit, const rm_t* o, const uint8_t* op,
                         int n, int imm) {
    int rex = 0;
    int i;
    if (o->mem && o->m.seg) {
        b(c, o->m.seg);
    }
    if (size == 16) {
        b(c, 0x66);
    }
    if (size == 64) {
        rex |= 8;
    }
    rex |= rex_xb(o);
    if (rex || (!o->mem && byte_rex(size, o->r))) {
        b(c, (uint8_t)(0x40 | rex));
    }
    for (i = 0; i < n; i++) {
        b(c, op[i]);
    }
    modrm(c, digit, o, imm);
}

void x64_mov_ri(x64_code_t* c, int size, int r, uint64_t imm) {
    if (!room(c, MAX_INSN)) {
        return;
    }
    if (size == 64 && (imm >> 32) == 0) {
        size = 32; /* mov r32, imm32 zero-extends */
    } else if (size == 64 && (int64_t)imm == (int64_t)(int32_t)(uint32_t)imm) {
        static const uint8_t op = 0xC7;
        const rm_t o = rm_reg(r);
        legacy_digit(c, 64, 0, &o, &op, 1, 4);
        d32(c, (uint32_t)imm);
        return;
    }
    if (size == 64 || (r & 8) || byte_rex(size, r)) {
        b(c, (uint8_t)(0x40 | (size == 64 ? 8 : 0) | ((r & 8) ? 1 : 0)));
    }
    b(c, (uint8_t)((size == 8 ? 0xB0 : 0xB8) | (r & 7)));
    if (size == 8) {
        b(c, (uint8_t)imm);
    } else if (size == 32) {
        d32(c, (uint32_t)imm);
    } else {
        d64(c, imm);
    }
}

void x64_mov_rr(x64_code_t* c, int size, int dst, int src) {
    const uint8_t op = size == 8 ? 0x88 : 0x89;
    const rm_t o = rm_reg(dst);
    if (room(c, MAX_INSN)) {
        legacy(c, size, src, &o, &op, 1, 0);
    }
}

void x64_mov_rm(x64_code_t* c, int size, int r, x64_mem_t m) {
    const uint8_t op = size == 8 ? 0x8A : 0x8B;
    const rm_t o = rm_mem(m);
    if (room(c, MAX_INSN)) {
        legacy(c, size, r, &o, &op, 1, 0);
    }
}

void x64_mov_mr(x64_code_t* c, int size, x64_mem_t m, int r) {
    const uint8_t op = size == 8 ? 0x88 : 0x89;
    const rm_t o = rm_mem(m);
    if (room(c, MAX_INSN)) {
        legacy(c, size, r, &o, &op, 1, 0);
    }
}

void x64_alu_ri(x64_code_t* c, int alu, int size, int r, uint32_t imm) {
    const rm_t o = rm_reg(r);
    if (!room(c, MAX_INSN)) {
        return;
    }
    if (is_s8((int32_t)imm)) {
        static const uint8_t op = 0x83;
        legacy_digit(c, size, alu, &o, &op, 1, 1);
        b(c, (uint8_t)imm);
        return;
    }
    if (r == X64_RAX) {
        /* The accumulator's short form */
        if (size == 64) {
            b(c, 0x48);
        }
        b(c, (uint8_t)((alu << 3) | 5));
    } else {
        static const uint8_t op = 0x81;
        legacy_digit(c, size, alu, &o, &op, 1, 4);
    }
    d32(c, imm);
}

void x64_alu_rr(x64_code_t* c, int alu, int size, int dst, int src) {
    const uint8_t op = (uint8_t)((alu << 3) | 1);
    const rm_t o = rm_reg(dst);
    if (room(c, MAX_INSN)) {
        legacy(c, size, src, &o, &op, 1, 0);
    }
}

void x64_alu_rm(x64_code_t* c, int alu, int size, int r, x64_mem_t m) {
    const uint8_t op = (uint8_t)((alu << 3) | 3);
    const rm_t o = rm_mem(m);
    if (room(c, MAX_INSN)) {
        legacy(c, size, r, &o, &op, 1, 0);
    }
}

void x64_alu_mr(x64_code_t* c, int alu, int size, x64_mem_t m, int r) {
    const uint8_t op = (uint8_t)((alu << 3) | 1);
    const rm_t o = rm_mem(m);
    if (room(c, MAX_INSN)) {
        legacy(c, size, r, &o, &op, 1, 0);
    }
}

void x64_imul_rri(x64_code_t* c, int size, int dst, int src, uint32_t imm) {
    const rm_t o = rm_reg(src);
    if (!room(c, MAX_INSN)) {
        return;
    }
    if (is_s8((int32_t)imm)) {
        static const uint8_t op = 0x6B;
        legacy(c, size, dst, &o, &op, 1, 1);
        b(c, (uint8_t)imm);
    } else {
        static const uint8_t op = 0x69;
        legacy(c, size, dst, &o, &op, 1, 4);
        d32(c, imm);
    }
}

void x64_imul_rm(x64_code_t* c, int size, int r, x64_mem_t m) {
    static const uint8_t op[2] = {0x0F, 0xAF};
    const rm_t o = rm_mem(m);
    if (room(c, MAX_INSN)) {
        legacy(c, size, r, &o, op, 2, 0);
    }
}

void x64_shift_ri(x64_code_t* c, int sh, int size, int r, uint8_t count) {
    const rm_t o = rm_reg(r);
    if (!room(c, MAX_INSN)) {
        return;
    }
    if (count == 1) {
        static const uint8_t op = 0xD1;
        legacy_digit(c, size, sh, &o, &op, 1, 0);
    } else {
        static const uint8_t op = 0xC1;
        legacy_digit(c, size, sh, &o, &op, 1, 1);
        b(c, count);
    }
}

void x64_shift_rcl(x64_code_t* c, int sh, int size, int r) {
    static const uint8_t op = 0xD3;
    const rm_t o = rm_reg(r);
    if (room(c, MAX_INSN)) {
        legacy_digit(c, size, sh, &o, &op, 1, 0);
    }
}

void x64_shift_mcl(x64_code_t* c, int sh, int size, x64_mem_t m) {
    static const uint8_t op = 0xD3;
    const rm_t o = rm_mem(m);
    if (room(c, MAX_INSN)) {
        legacy_digit(c, size, sh, &o, &op, 1, 0);
    }
}

static void unary(x64_code_t* c, uint8_t op, int digit, int size, int r) {
    const rm_t o = rm_reg(r);
    if (room(c, MAX_INSN)) {
        legacy_digit(c, size, digit, &o, &op, 1, 0);
    }
}

void x64_not(x64_code_t* c, int size, int r) {
    unary(c, 0xF7, 2, size, r);
}

void x64_neg(x64_code_t* c, int size, int r) {
    unary(c, 0xF7, 3, size, r);
}

void x64_dec(x64_code_t* c, int size, int r) {
    unary(c, 0xFF, 1, size, r);
}

void x64_cmov_rr(x64_code_t* c, int cc, int size, int dst, int src) {
    uint8_t op[2];
    const rm_t o = rm_reg(src);
    op[0] = 0x0F;
    op[1] = (uint8_t)(0x40 | cc);
    if (room(c, MAX_INSN)) {
        legacy(c, size, dst, &o, op, 2, 0);
    }
}

void x64_cmov_rm(x64_code_t* c, int cc, int size, int r, x64_mem_t m) {
    uint8_t op[2];
    const rm_t o = rm_mem(m);
    op[0] = 0x0F;
    op[1] = (uint8_t)(0x40 | cc);
    if (room(c, MAX_INSN)) {
        legacy(c, size, r, &o, op, 2, 0);
    }
}

void x64_lea(x64_code_t* c, int r, x64_mem_t m) {
    static const uint8_t op = 0x8D;
    const rm_t o = rm_mem(m);
    if (room(c, MAX_INSN)) {
        legacy(c, 64, r, &o, &op, 1, 0);
    }
}

void x64_xchg_rm(x64_code_t* c, int r, x64_mem_t m) {
    static const uint8_t op = 0x87;
    const rm_t o = rm_mem(m);
    if (room(c, MAX_INSN)) {
        legacy(c, 64, r, &o, &op, 1, 0);
    }
}

void x64_push(x64_code_t* c, int r) {
    if (!room(c, 2)) {
        return;
    }
    if (r & 8) {
        b(c, 0x41);
    }
    b(c, (uint8_t)(0x50 | (r & 7)));
}

void x64_pop(x64_code_t* c, int r) {
    if (!room(c, 2)) {
        return;
    }
    if (r & 8) {
        b(c, 0x41);
    }
    b(c, (uint8_t)(0x58 | (r & 7)));
}

void x64_pushfq(x64_code_t* c) {
    if (room(c, 1)) {
        b(c, 0x9C);
    }
}

void x64_popfq(x64_code_t* c) {
    if (room(c, 1)) {
        b(c, 0x9D);
    }
}

void x64_ret(x64_code_t* c) {
    if (room(c, 1)) {
        b(c, 0xC3);
    }
}

void x64_call_m(x64_code_t* c, x64_mem_t m) {
    static const uint8_t op = 0xFF;
    const rm_t o = rm_mem(m);
    if (room(c, MAX_INSN)) {
        legacy_digit(c, 32, 2, &o, &op, 1, 0);
    }
}

void x64_jmp_r(x64_code_t* c, int r) {
    static const uint8_t op = 0xFF;
    const rm_t o = rm_reg(r);
    if (room(c, MAX_INSN)) {
        legacy_digit(c, 32, 4, &o, &op, 1, 0);
    }
}

void x64_jmp(x64_code_t* c, uint64_t target, int mode) {
    const uint64_t at = (uint64_t)(uintptr_t)x64_curr(c);
    const int64_t short_off = (int64_t)(target - (at + 2));
    if (!room(c, mode == X64_JMP_SHORT ? 2 : 5)) {
        return;
    }
    if (mode == X64_JMP_SHORT && !is_s8(short_off)) {
        c->error = 1;
        return;
    }
    if (mode != X64_JMP_NEAR && is_s8(short_off)) {
        b(c, 0xEB);
        b(c, (uint8_t)(int8_t)short_off);
        return;
    }
    b(c, 0xE9);
    d32(c, (uint32_t)(target - (at + 5)));
}

size_t x64_jmp_fwd(x64_code_t* c) {
    if (!room(c, 2)) {
        return 0;
    }
    b(c, 0xEB);
    b(c, 0);
    return c->size - 1;
}

size_t x64_jcc_fwd(x64_code_t* c, int cc) {
    if (!room(c, 2)) {
        return 0;
    }
    b(c, (uint8_t)(0x70 | cc));
    b(c, 0);
    return c->size - 1;
}

void x64_bind(x64_code_t* c, size_t at) {
    size_t rel;
    if (c->error || at == 0) {
        return;
    }
    rel = c->size - (at + 1);
    if (rel > 127) {
        c->error = 1; /* too far for a short jump */
        return;
    }
    c->p[at] = (uint8_t)rel;
}

/* A VEX instruction: @pp (0 none, 1 66, 2 F3, 3 F2), @mm (1 0F, 2 0F38, 3 0F3A), @w, @l, the
 * register field @reg, the second source @vvvv, the r/m @o, then @imm immediate bytes */
static void vex(x64_code_t* c, int pp, int mm, int w, int l, int reg, int vvvv, const rm_t* o,
                uint8_t op, int imm) {
    const int xb = rex_xb(o);
    const int r = (reg & 8) ? 0 : 0x80;
    const int x = (xb & 2) ? 0 : 0x40;
    const int bb = (xb & 1) ? 0 : 0x20;
    const int tail = ((~vvvv & 15) << 3) | (l ? 4 : 0) | pp;
    if (o->mem && o->m.seg) {
        b(c, o->m.seg);
    }
    if (mm == 1 && !w && x && bb) {
        b(c, 0xC5);
        b(c, (uint8_t)(r | tail));
    } else {
        b(c, 0xC4);
        b(c, (uint8_t)(r | x | bb | mm));
        b(c, (uint8_t)((w ? 0x80 : 0) | tail));
    }
    b(c, op);
    modrm(c, reg, o, imm);
}

void x64_vmovups_rm(x64_code_t* c, int l, int x, x64_mem_t m) {
    const rm_t o = rm_mem(m);
    if (room(c, MAX_INSN)) {
        vex(c, 0, 1, 0, l, x, 0, &o, 0x10, 0);
    }
}

void x64_vmovups_mr(x64_code_t* c, int l, x64_mem_t m, int x) {
    const rm_t o = rm_mem(m);
    if (room(c, MAX_INSN)) {
        vex(c, 0, 1, 0, l, x, 0, &o, 0x11, 0);
    }
}

void x64_vmovaps_rr(x64_code_t* c, int l, int dst, int src) {
    const rm_t o = rm_reg(src);
    if (room(c, MAX_INSN)) {
        vex(c, 0, 1, 0, l, dst, 0, &o, 0x28, 0);
    }
}

void x64_vmovq_xr(x64_code_t* c, int x, int r) {
    const rm_t o = rm_reg(r);
    if (room(c, MAX_INSN)) {
        vex(c, 1, 1, 1, 0, x, 0, &o, 0x6E, 0);
    }
}

void x64_vmovq_rx(x64_code_t* c, int r, int x) {
    const rm_t o = rm_reg(r);
    if (room(c, MAX_INSN)) {
        vex(c, 1, 1, 1, 0, x, 0, &o, 0x7E, 0);
    }
}

void x64_vcmpps(x64_code_t* c, int l, int dst, int src1, int src2, uint8_t pred) {
    const rm_t o = rm_reg(src2);
    if (room(c, MAX_INSN)) {
        vex(c, 0, 1, 0, l, dst, src1, &o, 0xC2, 1);
        b(c, pred);
    }
}

void x64_vcmpss_rrr(x64_code_t* c, int dst, int src1, int src2, uint8_t pred) {
    const rm_t o = rm_reg(src2);
    if (room(c, MAX_INSN)) {
        vex(c, 2, 1, 0, 0, dst, src1, &o, 0xC2, 1);
        b(c, pred);
    }
}

void x64_vcmpss_rrm(x64_code_t* c, int dst, int src1, x64_mem_t m, uint8_t pred) {
    const rm_t o = rm_mem(m);
    if (room(c, MAX_INSN)) {
        vex(c, 2, 1, 0, 0, dst, src1, &o, 0xC2, 1);
        b(c, pred);
    }
}

void x64_vpcmpeqd(x64_code_t* c, int l, int dst, int src1, int src2) {
    const rm_t o = rm_reg(src2);
    if (room(c, MAX_INSN)) {
        vex(c, 1, 1, 0, l, dst, src1, &o, 0x76, 0);
    }
}

void x64_vpsrld_ri(x64_code_t* c, int l, int dst, int src, uint8_t count) {
    const rm_t o = rm_reg(src);
    if (room(c, MAX_INSN)) {
        vex(c, 1, 1, 0, l, 2, dst, &o, 0x72, 1);
        b(c, count);
    }
}

void x64_vpslld_ri(x64_code_t* c, int l, int dst, int src, uint8_t count) {
    const rm_t o = rm_reg(src);
    if (room(c, MAX_INSN)) {
        vex(c, 1, 1, 0, l, 6, dst, &o, 0x72, 1);
        b(c, count);
    }
}

void x64_vxorps_rrr(x64_code_t* c, int l, int dst, int src1, int src2) {
    const rm_t o = rm_reg(src2);
    if (room(c, MAX_INSN)) {
        vex(c, 0, 1, 0, l, dst, src1, &o, 0x57, 0);
    }
}

void x64_vxorps_rrm(x64_code_t* c, int l, int dst, int src1, x64_mem_t m) {
    const rm_t o = rm_mem(m);
    if (room(c, MAX_INSN)) {
        vex(c, 0, 1, 0, l, dst, src1, &o, 0x57, 0);
    }
}

void x64_vrcpps(x64_code_t* c, int l, int dst, int src) {
    const rm_t o = rm_reg(src);
    if (room(c, MAX_INSN)) {
        vex(c, 0, 1, 0, l, dst, 0, &o, 0x53, 0);
    }
}

void x64_vrsqrtps(x64_code_t* c, int l, int dst, int src) {
    const rm_t o = rm_reg(src);
    if (room(c, MAX_INSN)) {
        vex(c, 0, 1, 0, l, dst, 0, &o, 0x52, 0);
    }
}

static void sse_rr(x64_code_t* c, uint8_t op2, int dst, int src) {
    uint8_t op[2];
    const rm_t o = rm_reg(src);
    op[0] = 0x0F;
    op[1] = op2;
    if (room(c, MAX_INSN)) {
        legacy(c, 32, dst, &o, op, 2, 0);
    }
}

void x64_rcpps(x64_code_t* c, int dst, int src) {
    sse_rr(c, 0x53, dst, src);
}

void x64_rsqrtps(x64_code_t* c, int dst, int src) {
    sse_rr(c, 0x52, dst, src);
}

void x64_xorps(x64_code_t* c, int dst, int src) {
    sse_rr(c, 0x57, dst, src);
}

void x64_vblendvps(x64_code_t* c, int l, int dst, int src1, int src2, int mask) {
    const rm_t o = rm_reg(src2);
    if (room(c, MAX_INSN)) {
        vex(c, 1, 3, 0, l, dst, src1, &o, 0x4A, 1);
        b(c, (uint8_t)(mask << 4));
    }
}

void x64_vpextrq(x64_code_t* c, int r, int x, uint8_t lane) {
    const rm_t o = rm_reg(r);
    if (room(c, MAX_INSN)) {
        vex(c, 1, 3, 1, 0, x, 0, &o, 0x16, 1);
        b(c, lane);
    }
}

void x64_vgatherdps(x64_code_t* c, int l, int dst, x64_mem_t m, int mask) {
    const rm_t o = rm_mem(m);
    if (room(c, MAX_INSN)) {
        vex(c, 1, 2, 0, l, dst, mask, &o, 0x92, 0);
    }
}

static int fits_s32(int64_t v) {
    return v >= -2147483647 - 1 && v <= 2147483647;
}

int x64_relocate(x64_code_t* c, const uint8_t* bytes, const x64_insn_t* in, uint64_t from) {
    const uint64_t at = (uint64_t)(uintptr_t)x64_curr(c);
    if (in->rel) {
        /* A relative branch keeps its form (as Zydis's encoder relocated it): its offset moved */
        const uint64_t target = x64_target(in, from);
        const int64_t off = (int64_t)(target - (at + in->len));
        if (in->imm_size == 1 && !is_s8(off) && in->enc == X64_ENC_LEGACY &&
            in->map == X64_MAP_ONE && ((in->op >= 0x70 && in->op <= 0x7F) || in->op == 0xEB) &&
            !in->p66 && !in->p67 && !in->lock && !in->rep && !in->rex && !in->fsgs) {
            /* A short Jcc or JMP that no longer reaches: its near form (a branch hint, 2E or 3E,
             * left behind). LOOP and JRCXZ have none. */
            const unsigned n = in->op == 0xEB ? 5 : 6;
            const int64_t near_off = (int64_t)(target - (at + n));
            if (!fits_s32(near_off))
                return 0;
            if (!room(c, n))
                return 0;
            if (in->op == 0xEB) {
                b(c, 0xE9);
            } else {
                b(c, 0x0F);
                b(c, (uint8_t)(0x80 | (in->op & 0x0F)));
            }
            d32(c, (uint32_t)near_off);
            return 1;
        }
        if ((in->imm_size == 1 && !is_s8(off)) ||
            (in->imm_size == 2 && (off < -32768 || off > 32767)) ||
            (in->imm_size == 4 && !fits_s32(off)))
            return 0;
        if (!room(c, in->len))
            return 0;
        memcpy(c->p + c->size, bytes, in->len);
        memcpy(c->p + c->size + in->imm_off, &off, in->imm_size); /* little endian */
        c->size += in->len;
        return 1;
    }
    if (in->rip) {
        const uint64_t target = x64_target(in, from);
        const int64_t disp = (int64_t)(target - (at + in->len));
        int32_t d;
        if (!fits_s32(disp))
            return 0;
        if (!room(c, in->len))
            return 0;
        d = (int32_t)disp;
        memcpy(c->p + c->size, bytes, in->len);
        memcpy(c->p + c->size + in->disp_off, &d, 4);
        c->size += in->len;
        return 1;
    }
    if (!room(c, in->len))
        return 0;
    memcpy(c->p + c->size, bytes, in->len);
    c->size += in->len;
    return 1;
}
