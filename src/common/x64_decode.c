/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <string.h>

#include "common/x64_decode.h"

/* What an opcode path decodes to (common/x64_decode_tables.h): for each opcode of each map, a
 * row of its forms by prefixes, REX.W / VEX.W and VEX.L; each form one record, or a table of
 * records by ModRM (memory or register, its reg field, and where it matters its rm field) */
#include "common/x64_decode_tables.h"

/* A record */
#define R_VALID 0x1u
#define R_MODRM 0x2u
#define R_FORCE_REG 0x4u /* ModRM always names registers: no SIB, no displacement */
#define R_MOFFS 0x8u     /* a 64-bit (with 67: 32-bit) address */
#define R_IMM0(r) ((r) >> 4 & 7u)
#define R_IMM0_SIGNED 0x80u
#define R_IMM0_REL 0x100u
#define R_IMM1(r) ((r) >> 9 & 3u)
#define R_LOCK 0x800u              /* takes F0 */
#define R_REG(r) ((r) >> 12 & 31u) /* what ModRM.reg names */
#define R_RM(r) ((r) >> 17 & 31u)  /* what ModRM.rm names: a register kind, or a memory kind */
#define R_NDS(r) ((r) >> 22 & 31u) /* what VEX.vvvv names */
#define R_GATHER 0x8000000u        /* destination, index and mask must differ */
#define R_NSSM 0x20000000u         /* destination and both sources must differ */
#define R_CHECK 0x40000000u        /* register checks that can fail */

/* Register kinds */
enum { K_NONE, K_GPR, K_X87, K_MMX, K_VR, K_TMM, K_SEG, K_TEST, K_CR, K_DR, K_MASK, K_BND };
/* Memory kinds */
enum { M_VSIB = 4 };

/* The bits of a register number each kind reads, and how many registers it has */
static const uint8_t mask_reg[12] = {0, 31, 7, 7, 31, 31, 7, 7, 15, 15, 31, 15};
static const uint8_t mask_rm[12] = {0, 15, 7, 7, 31, 15, 7, 7, 15, 15, 7, 15};
static const uint8_t avail[12] = {255, 16, 8, 8, 32, 8, 6, 8, 16, 8, 8, 4};
/* CR0, CR2, CR3, CR4 and CR8 */
static const uint8_t cr_ok[16] = {1, 0, 1, 1, 1, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0};
static const uint8_t imm_bytes[8] = {0, 1, 2, 4, 8, 0, 0, 0};

static uint64_t rd(const uint8_t* p, unsigned n) {
    switch (n) {
    case 1:
        return p[0];
    case 2:
        return (uint64_t)p[0] | (uint64_t)p[1] << 8;
    case 4:
        return (uint64_t)p[0] | (uint64_t)p[1] << 8 | (uint64_t)p[2] << 16 | (uint64_t)p[3] << 24;
    default:
        return (uint64_t)p[0] | (uint64_t)p[1] << 8 | (uint64_t)p[2] << 16 | (uint64_t)p[3] << 24 |
               (uint64_t)p[4] << 32 | (uint64_t)p[5] << 40 | (uint64_t)p[6] << 48 |
               (uint64_t)p[7] << 56;
    }
}

static uint64_t sx(uint64_t v, unsigned n) {
    const unsigned s = 64 - 8 * n;
    return n && n < 8 ? (uint64_t)((int64_t)(v << s) >> s) : v;
}

/* Legacy prefixes: 1 F0, 2 F2, 3 F3, 4 66, 5 67, 6 a segment, 7 REX */
static const uint8_t prefix_of[256] = {
    /* 00 */ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    /* 10 */ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    /* 20 */ 0, 0, 0, 0, 0, 0, 6, 0, 0, 0, 0, 0, 0, 0, 6, 0,
    /* 30 */ 0, 0, 0, 0, 0, 0, 6, 0, 0, 0, 0, 0, 0, 0, 6, 0,
    /* 40 */ 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
    /* 50 */ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    /* 60 */ 0, 0, 0, 0, 6, 6, 4, 5, 0, 0, 0, 0, 0, 0, 0, 0,
    /* 70 */ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    /* 80 */ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    /* 90 */ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    /* A0 */ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    /* B0 */ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    /* C0 */ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    /* D0 */ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    /* E0 */ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    /* F0 */ 1, 0, 2, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

/* The form's row index for the legacy prefixes: none, 66, F3, 66 with F3, F2, 66 with F2 */
static const uint8_t legacy_class[3][2] = {{0, 1}, {4, 5}, {2, 3}};

/* Nothing is read past the bytes there are, nor past 15 of them: an instruction that would need
 * more is refused, as Zydis refuses it */
#define NEED(n)                                                                                    \
    do {                                                                                           \
        if (at + (n) > max)                                                                        \
            return 0;                                                                              \
    } while (0)

unsigned x64_decode(const uint8_t* p, size_t avail_bytes, x64_insn_t* in) {
    const unsigned max = avail_bytes < 15 ? (unsigned)avail_bytes : 15;
    unsigned at = 0, em, v, rec, r = 0, x = 0, b = 0, is_reg = 0, kreg, krm = 0, knds;
    unsigned id_reg, id_rm, id_nds, id_index = 0;
    int now3d = 0;
    uint16_t e;
    memset(in, 0, sizeof(*in));
    in->base = in->index = -1;
    in->scale = 1;
    /* Legacy prefixes; a REX counts only right before the opcode */
    for (;;) {
        uint8_t c, k;
        NEED(1);
        c = p[at];
        k = prefix_of[c];
        if (!k)
            break;
        switch (k) {
        case 1:
            in->lock = 1;
            break;
        case 2:
        case 3:
            in->rep = c;
            break;
        case 4:
            in->p66 = 1;
            break;
        case 5:
            in->p67 = 1;
            break;
        case 6:
            in->seg = c;
            break;
        }
        in->rex = k == 7 ? c : 0;
        at++;
    }
    in->op = p[at++];
    /* VEX (C4, C5) and XOP (8F with a map of 8 or more), never after REX, F0, 66, F2 or F3.
     * EVEX and MVEX (62) are refused: AVX-512 and Knights Corner, which no PS4 game has. */
    if (in->op == 0xC4 || in->op == 0xC5 || in->op == 0x62 ||
        (in->op == 0x8F && at < max && (p[at] & 0x1F) >= 8)) {
        unsigned need = in->op == 0xC5 ? 1 : in->op == 0x62 ? 3 : 2;
        const uint8_t* q = p + at;
        if (in->op == 0x62 || in->rex || in->lock || in->p66 || in->rep)
            return 0;
        NEED(need + 1);
        if (in->op == 0xC5) {
            r = !(q[0] & 0x80);
            in->map = 1;
            in->vvvv = (uint8_t)(~q[0] >> 3 & 15);
            in->vl = (uint8_t)(q[0] >> 2 & 1);
            in->pp = (uint8_t)(q[0] & 3);
        } else {
            r = !(q[0] & 0x80);
            x = !(q[0] & 0x40);
            b = !(q[0] & 0x20);
            in->map = (uint8_t)(q[0] & 0x1F);
            in->w = (uint8_t)(q[1] >> 7);
            in->vvvv = (uint8_t)(~q[1] >> 3 & 15);
            in->vl = (uint8_t)(q[1] >> 2 & 1);
            in->pp = (uint8_t)(q[1] & 3);
        }
        if (in->op == 0x8F) {
            if (in->map > 10)
                return 0;
            in->enc = X64_ENC_XOP;
            em = in->map;
        } else {
            if (in->map > 3)
                return 0;
            in->enc = X64_ENC_VEX;
            em = 4 + in->map;
        }
        in->rex = (uint8_t)(0x40 | in->w << 3 | r << 2 | x << 1 | b);
        at += need;
        in->op = p[at++];
        v = (unsigned)in->w * 8 + in->vl * 4 + in->pp;
    } else {
        in->enc = X64_ENC_LEGACY;
        em = 0;
        if (in->op == 0x0F) {
            NEED(1);
            in->op = p[at++];
            em = 1;
            if (in->op == 0x0F) {
                if (in->lock)
                    return 0;
                now3d = 1;
            } else if (in->op == 0x38 || in->op == 0x3A) {
                em = in->op == 0x38 ? 2 : 3;
                NEED(1);
                in->op = p[at++];
            }
        }
        in->map = (uint8_t)(now3d ? X64_MAP_3DNOW : em);
        in->w = (uint8_t)(in->rex >> 3 & 1);
        r = in->rex >> 2 & 1;
        x = in->rex >> 1 & 1;
        b = in->rex & 1;
        v = (unsigned)in->w * 6 + legacy_class[in->rep & 3 ? (in->rep & 1) + 1 : 0][in->p66];
    }
    /* The form */
    if (now3d) {
        rec = R_VALID | R_MODRM | K_MMX << 12 | K_MMX << 17; /* every one: mm, mm/m64 */
    } else {
        e = dec_rows[dec_op_row[em][in->op]][v];
        if (e & 0x8000) {
            rec = dec_recs[e & 0x7FFF];
        } else {
            uint8_t m;
            NEED(1);
            m = p[at];
            e = dec_mtabs[e][(m >> 6 == 3 ? 8 : 0) + (m >> 3 & 7)];
            if (e & 0x8000)
                e = dec_subs[e & 0x7FFF][m & 7];
            rec = dec_recs[e];
        }
    }
    if (!(rec & R_VALID))
        return 0;
    /* ModRM, SIB and displacement */
    if (rec & R_MODRM) {
        NEED(1);
        in->has_modrm = 1;
        in->modrm = p[at++];
        in->mod = (uint8_t)(in->modrm >> 6);
        is_reg = in->mod == 3 || (rec & R_FORCE_REG);
        if (!is_reg) {
            unsigned base = in->modrm & 7;
            if (base == 4) {
                NEED(1);
                in->has_sib = 1;
                in->sib = p[at++];
                in->scale = (uint8_t)(1u << (in->sib >> 6));
                id_index = in->sib >> 3 & 7;
                base = in->sib & 7;
                if (base == 5 && in->mod == 0) {
                    in->disp_size = 4;
                } else {
                    in->base = (int8_t)(base | b << 3);
                }
            } else if (base == 5 && in->mod == 0) {
                in->rip = 1;
                in->disp_size = 4;
            } else {
                in->base = (int8_t)(base | b << 3);
            }
            if (in->mod == 1)
                in->disp_size = 1;
            else if (in->mod == 2)
                in->disp_size = 4;
        }
    }
    if (rec & R_MOFFS)
        in->disp_size = (uint8_t)(in->p67 ? 4 : 8);
    if (in->disp_size) {
        NEED(in->disp_size);
        in->disp_off = (uint8_t)at;
        in->disp = (int64_t)sx(rd(p + at, in->disp_size), in->disp_size);
        at += in->disp_size;
    }
    /* The immediates */
    in->imm_size = imm_bytes[R_IMM0(rec)];
    if (in->imm_size) {
        NEED(in->imm_size);
        in->imm_off = (uint8_t)at;
        in->imm = rd(p + at, in->imm_size);
        if (rec & (R_IMM0_SIGNED | R_IMM0_REL))
            in->imm = sx(in->imm, in->imm_size);
        in->rel = (uint8_t)((rec & R_IMM0_REL) != 0);
        at += in->imm_size;
    }
    in->imm2_size = imm_bytes[R_IMM1(rec)];
    if (in->imm2_size) {
        NEED(in->imm2_size);
        in->imm2 = rd(p + at, in->imm2_size);
        at += in->imm2_size;
    }
    /* What the form allows */
    if (in->lock && !(rec & R_LOCK))
        return 0;
    knds = R_NDS(rec);
    if (!knds && in->vvvv)
        return 0;
    krm = R_RM(rec);
    if (rec & R_CHECK) {
        kreg = R_REG(rec);
        if (!is_reg) {
            if ((krm >> 3 & 1) && in->mod == 0 && (in->modrm & 7) == 5)
                return 0; /* no rip-relative operand */
            krm &= 7;
        }
        id_reg = ((in->modrm >> 3 & 7) | r << 3) & mask_reg[kreg & 15];
        id_rm = is_reg ? ((in->modrm & 7) | b << 3) & mask_rm[krm & 15] : 0;
        id_nds = in->vvvv;
        id_index |= x << 3;
        if (id_reg >= avail[kreg & 15] || id_nds >= avail[knds & 15] ||
            (is_reg && id_rm >= avail[krm & 15]))
            return 0;
        if ((kreg & 15) == K_CR && !cr_ok[id_reg])
            return 0;
        if (is_reg && (krm & 15) == K_CR && !cr_ok[id_rm])
            return 0;
        /* No MOV to CS */
        if (((kreg >> 4) && id_reg == 1) || (is_reg && (krm >> 4) && id_rm == 1))
            return 0;
        if ((rec & R_GATHER) && (id_reg == id_index || id_reg == id_nds || id_index == id_nds))
            return 0;
        if ((rec & R_NSSM) && (id_reg == id_nds || id_reg == id_rm || id_nds == id_rm))
            return 0;
    } else {
        id_index |= x << 3;
        if (!is_reg)
            krm &= 7;
    }
    /* 3DNow!: the opcode follows */
    if (now3d) {
        NEED(1);
        in->op = p[at++];
        if (!(now3d_ok[is_reg][in->op >> 3] >> (in->op & 7) & 1))
            return 0;
    }
    in->reg = (uint8_t)((in->modrm >> 3 & 7) | r << 3);
    in->rm = (uint8_t)((in->modrm & 7) | b << 3);
    if (in->has_sib && (id_index != 4 || (!is_reg && krm == M_VSIB)))
        in->index = (int8_t)id_index;
    in->len = (uint8_t)at;
    return at;
}

uint64_t x64_target(const x64_insn_t* in, uint64_t address) {
    const uint64_t end = address + in->len;
    if (in->rel)
        return end + in->imm;
    return end + (uint64_t)in->disp;
}
