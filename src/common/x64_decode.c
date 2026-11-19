/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <string.h>

#include "common/x64_decode.h"

/* A definition: what an opcode path reaches. Its record (below); its immediates' sizes at each
 * operand width; its mnemonic; its operand definitions (the first of dec_opdefs, how many, how
 * many shown) and its category. */
struct dec_def {
    uint32_t rec;
    uint16_t imm;
    uint16_t mnemonic;
    uint16_t ops;
    uint16_t pfx; /* P_ below */
    uint8_t category, nops, nvis;
};
/* The prefixes a definition takes (as the formatter names them), and a far branch */
#define P_REP 0x01u
#define P_REPE 0x02u
#define P_REPNE 0x04u
#define P_BND 0x08u
#define P_XACQUIRE 0x10u
#define P_XRELEASE 0x20u
#define P_NOTRACK 0x40u
#define P_HLE_WITHOUT_LOCK 0x80u
#define P_FAR 0x100u
/* A form: a definition at an operand width (eosz: 0 16, 1 32, 2 64 bits), with its operand
 * width and its address width without and with 67 */
struct dec_form {
    uint16_t def;
    uint8_t eosz, opw, asw0, asw1;
};
/* A register: its class, number in the class, width and the 64-bit register it is part of */
struct dec_reg {
    uint8_t cls, id;
    uint16_t width, largest;
};

/* What an opcode path decodes to (common/x64_decode_tables.h): for each opcode of each map, a
 * row of its forms by prefixes, REX.W / VEX.W and VEX.L; each one form, or a table of forms by
 * ModRM (memory or register, its reg field, and where it matters its rm field) */
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
#define R_SEGMENT 0x80000000u      /* FS and GS prefixes name the memory operands' segment */

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
    const struct dec_form* form;
    const struct dec_def* def;
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
            if (c == 0x64 || c == 0x65) {
                in->fsgs = c;
                in->notrack = 0;
            } else if (c == 0x3E && !in->fsgs) {
                in->notrack = 1;
            }
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
        e = 0;
    } else {
        e = dec_rows[dec_op_row[em][in->op]][v];
        if (em == 0 && ((in->op == 0x90 && b) || (in->op == 0xE3 && in->p67))) {
            const uint16_t alt = in->op == 0x90 ? dec_alt90[v] : dec_altE3[v]; /* XCHG; JECXZ */
            if (alt)
                e = (uint16_t)(alt | 0x8000);
        }
        if (e & 0x8000) {
            e &= 0x7FFF;
        } else {
            uint8_t m;
            NEED(1);
            m = p[at];
            e = dec_mtabs[e][(m >> 6 == 3 ? 8 : 0) + (m >> 3 & 7)];
            if (e & 0x8000)
                e = dec_subs[e & 0x7FFF][m & 7];
        }
        form = &dec_forms[e];
        def = &dec_defs[form->def];
        rec = def->rec | (e ? R_VALID : 0);
        rec |= (unsigned)(def->imm >> (3 * form->eosz) & 7) << 4;
        rec |= (unsigned)(def->imm >> (9 + 2 * form->eosz) & 3) << 9;
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
        e = now3d_form[in->w ? 2 : in->p66 ? 1 : 0][is_reg][in->op];
        if (!e)
            return 0;
    }
    form = &dec_forms[e];
    def = &dec_defs[form->def];
    in->form = e;
    in->mnemonic = def->mnemonic;
    in->category = def->category;
    in->operand_width = form->opw;
    in->address_width = in->p67 ? form->asw1 : form->asw0;
    in->operand_count = def->nops;
    in->operand_count_visible = def->nvis;
    in->has_rex = (uint8_t)(in->enc == X64_ENC_LEGACY && in->rex);
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

/* Operands: Zydis's semantic operand types and implicit operands */
enum {
    T_UNUSED,
    T_IMPLICIT_REG,
    T_IMPLICIT_MEM,
    T_IMPLICIT_IMM1,
    T_GPR8,
    T_GPR16,
    T_GPR32,
    T_GPR64,
    T_GPR16_32_64,
    T_GPR32_32_64,
    T_GPR16_32_32,
    T_GPR_ASZ,
    T_FPR,
    T_MMX,
    T_XMM,
    T_YMM,
    T_ZMM,
    T_TMM,
    T_BND,
    T_SREG,
    T_CR,
    T_DR,
    T_MASK,
    T_MEM,
    T_MEM_VSIBX,
    T_MEM_VSIBY,
    T_MEM_VSIBZ,
    T_IMM,
    T_REL,
    T_PTR,
    T_AGEN,
    T_MOFFS,
    T_MIB
};
enum { I_STATIC, I_GPR_OSZ, I_GPR_ASZ, I_IP_ASZ, I_IP_SSZ, I_GPR_SSZ, I_FLAGS_SSZ };
enum { B_AGPR_REG, B_AGPR_RM, B_AAX, B_ADX, B_ABX, B_ASI, B_ADI, B_SSP, B_SBP };
/* How an operand is named */
enum { E_NONE, E_MODRM_REG, E_MODRM_RM, E_OPCODE, E_NDSNDD, E_IS4 };
/* Register classes */
enum {
    C_INVALID,
    C_GPR8,
    C_GPR16,
    C_GPR32,
    C_GPR64,
    C_X87,
    C_MMX,
    C_XMM,
    C_YMM,
    C_ZMM,
    C_TMM,
    C_FLAGS,
    C_IP,
    C_SEGMENT,
    C_TABLE,
    C_TEST,
    C_CONTROL,
    C_DEBUG,
    C_MASK,
    C_BOUND
};

static unsigned reg_encode(unsigned cls, unsigned id) {
    return id < dec_class_count[cls] ? dec_class_first[cls] + id : X64_REG_NONE;
}

static unsigned width_class(unsigned bits) {
    return bits == 16 ? C_GPR16 : bits == 32 ? C_GPR32 : C_GPR64;
}

unsigned x64_operands(const x64_insn_t* in, x64_operand_t out[X64_MAX_OPERANDS]) {
    const struct dec_form* form = &dec_forms[in->form];
    const struct dec_def* def = &dec_defs[form->def];
    const uint32_t(*od)[3] = &dec_opdefs[def->ops];
    const unsigned rec = def->rec;
    const unsigned r = in->rex >> 2 & 1, x = in->rex >> 1 & 1, b = in->rex & 1;
    const unsigned is_reg = in->has_modrm && (in->mod == 3 || (rec & R_FORCE_REG));
    const unsigned kreg = R_REG(rec) & 15, krm = R_RM(rec) & 15;
    const unsigned rm = in->modrm & 7;
    const unsigned id_reg = ((in->modrm >> 3 & 7) | r << 3) & mask_reg[kreg];
    const unsigned id_rm = is_reg ? (rm | b << 3) & mask_rm[krm] : rm | b << 3;
    const unsigned id_base = (in->has_sib ? (in->sib & 7u) : rm) | b << 3;
    const unsigned id_index = (in->sib >> 3 & 7u) | x << 3;
    const unsigned asw = in->address_width;
    unsigned i, imm_id = 0;
    memset(out, 0, sizeof(*out) * def->nops);
    for (i = 0; i < def->nops; i++) {
        x64_operand_t* o = &out[i];
        const unsigned w0 = od[i][0], type = w0 & 63, enc = w0 >> 12 & 63;
        const unsigned size = form->eosz == 0   ? (od[i][1] & 0xFFFF)
                              : form->eosz == 1 ? od[i][1] >> 16
                                                : (od[i][2] & 0xFFFF);
        const unsigned extra = od[i][2] >> 16;
        unsigned cls = C_INVALID;
        o->visibility = (uint8_t)(w0 >> 6 & 3);
        o->actions = (uint8_t)(w0 >> 8 & 15);
        switch (type) {
        case T_IMPLICIT_REG: {
            const unsigned v = extra & 1023;
            o->type = X64_OPERAND_REGISTER;
            switch (extra >> 10) {
            case I_STATIC:
                o->reg = (uint16_t)v;
                break;
            case I_GPR_OSZ:
                o->reg = (uint16_t)reg_encode(C_GPR16 + form->eosz, v);
                break;
            case I_GPR_ASZ:
                o->reg = (uint16_t)reg_encode(width_class(asw), v);
                break;
            case I_IP_ASZ:
                o->reg = asw == 32 ? X64_REG_EIP : X64_REG_RIP;
                break;
            case I_IP_SSZ:
                o->reg = X64_REG_RIP;
                break;
            case I_GPR_SSZ:
                o->reg = (uint16_t)reg_encode(C_GPR64, v);
                break;
            default:
                o->reg = X64_REG_RFLAGS;
                break;
            }
            break;
        }
        case T_IMPLICIT_MEM: {
            const unsigned acls = width_class(asw);
            o->type = X64_OPERAND_MEMORY;
            o->mem.type = X64_MEMOP_MEM;
            switch (extra & 15) {
            case B_AGPR_REG:
                o->mem.base = (uint16_t)reg_encode(acls, id_reg);
                break;
            case B_AGPR_RM:
                o->mem.base = (uint16_t)reg_encode(acls, id_rm);
                break;
            case B_AAX:
                o->mem.base = (uint16_t)reg_encode(acls, 0);
                break;
            case B_ADX:
                o->mem.base = (uint16_t)reg_encode(acls, 2);
                break;
            case B_ABX:
                o->mem.base = (uint16_t)reg_encode(acls, 3);
                break;
            case B_ASI:
                o->mem.base = (uint16_t)reg_encode(acls, 6);
                break;
            case B_ADI:
                o->mem.base = (uint16_t)reg_encode(acls, 7);
                break;
            case B_SSP:
                o->mem.base = X64_REG_RSP;
                break;
            default:
                o->mem.base = X64_REG_RBP;
                break;
            }
            if (extra >> 4 & 7)
                o->mem.segment = (uint16_t)reg_encode(C_SEGMENT, (extra >> 4 & 7) - 1);
            break;
        }
        case T_IMPLICIT_IMM1:
            o->type = X64_OPERAND_IMMEDIATE;
            o->size = 8;
            o->imm.value = 1;
            break;
        default:
            break;
        }
        if (o->type)
            goto finalize;
        o->encoding = (uint8_t)enc;
        switch (type) {
        case T_GPR8:
            cls = C_GPR8;
            break;
        case T_GPR16:
            cls = C_GPR16;
            break;
        case T_GPR32:
            cls = C_GPR32;
            break;
        case T_GPR64:
            cls = C_GPR64;
            break;
        case T_GPR16_32_64:
            cls = in->operand_width == 16 ? C_GPR16 : in->operand_width == 32 ? C_GPR32 : C_GPR64;
            break;
        case T_GPR32_32_64:
            cls = in->operand_width == 64 ? C_GPR64 : C_GPR32;
            break;
        case T_GPR16_32_32:
            cls = in->operand_width == 16 ? C_GPR16 : C_GPR32;
            break;
        case T_GPR_ASZ:
            cls = width_class(asw);
            break;
        case T_FPR:
            cls = C_X87;
            break;
        case T_MMX:
            cls = C_MMX;
            break;
        case T_XMM:
            cls = C_XMM;
            break;
        case T_YMM:
            cls = C_YMM;
            break;
        case T_ZMM:
            cls = C_ZMM;
            break;
        case T_TMM:
            cls = C_TMM;
            break;
        case T_BND:
            cls = C_BOUND;
            break;
        case T_SREG:
            cls = C_SEGMENT;
            break;
        case T_CR:
            cls = C_CONTROL;
            break;
        case T_DR:
            cls = C_DEBUG;
            break;
        case T_MASK:
            cls = C_MASK;
            break;
        default:
            break;
        }
        if (cls) {
            unsigned id = 0;
            switch (enc) {
            case E_MODRM_REG:
                id = id_reg;
                break;
            case E_MODRM_RM:
                id = id_rm;
                break;
            case E_OPCODE:
                id = (in->op & 0x0F) > 7 ? (in->op & 0x0F) - 8u : (in->op & 0x0Fu);
                id |= b << 3;
                break;
            case E_NDSNDD:
                id = in->vvvv;
                break;
            case E_IS4:
                id = (unsigned)(in->imm >> 4 & 0x0F);
                break;
            default:
                break;
            }
            o->type = X64_OPERAND_REGISTER;
            if (cls == C_GPR8)
                o->reg =
                    (uint16_t)(in->has_rex && id >= 4 ? X64_REG_SPL + (id - 4) : X64_REG_AL + id);
            else
                o->reg = (uint16_t)reg_encode(cls, id);
            goto finalize;
        }
        switch (type) {
        case T_MEM:
        case T_MEM_VSIBX:
        case T_MEM_VSIBY:
        case T_MEM_VSIBZ:
        case T_AGEN:
        case T_MIB: {
            const unsigned vcls = type == T_MEM_VSIBX   ? C_XMM
                                  : type == T_MEM_VSIBY ? C_YMM
                                  : type == T_MEM_VSIBZ ? C_ZMM
                                                        : 0;
            const unsigned acls = asw == 32 ? C_GPR32 : C_GPR64;
            unsigned dsize = 0;
            o->type = X64_OPERAND_MEMORY;
            o->mem.type = vcls             ? X64_MEMOP_VSIB
                          : type == T_AGEN ? X64_MEMOP_AGEN
                          : type == T_MIB  ? X64_MEMOP_MIB
                                           : X64_MEMOP_MEM;
            if (type == T_AGEN || type == T_MIB)
                o->actions = 0;
            o->mem.base = (uint16_t)reg_encode(acls, id_base);
            if (in->mod == 0 && rm == 5) {
                o->mem.base = asw == 32 ? X64_REG_EIP : X64_REG_RIP;
                dsize = 4;
            } else if (in->mod == 1) {
                dsize = 1;
            } else if (in->mod == 2) {
                dsize = 4;
            }
            if (rm == 4) {
                o->mem.index = (uint16_t)reg_encode(vcls ? vcls : acls, id_index);
                o->mem.scale = (uint8_t)(1u << (in->sib >> 6));
                if (o->mem.index == (asw == 32 ? X64_REG_ESP : X64_REG_RSP)) {
                    o->mem.index = X64_REG_NONE;
                    o->mem.scale = 0;
                }
                if (o->mem.base == X64_REG_RBP || o->mem.base == X64_REG_R13 ||
                    (asw == 32 && o->mem.base == X64_REG_EBP)) {
                    if (in->mod == 0)
                        o->mem.base = X64_REG_NONE;
                    dsize = in->mod == 1 ? 1 : 4;
                }
            }
            if (dsize) {
                o->mem.disp_size = (uint8_t)dsize;
                o->mem.disp = in->disp;
            }
            break;
        }
        case T_PTR:
            o->type = X64_OPERAND_POINTER;
            o->ptr.offset = (uint32_t)in->imm;
            o->ptr.segment = (uint16_t)in->imm2;
            break;
        case T_MOFFS:
            o->type = X64_OPERAND_MEMORY;
            o->mem.type = X64_MEMOP_MEM;
            o->mem.disp_size = in->disp_size;
            o->mem.disp = in->disp;
            break;
        case T_IMM:
        case T_REL:
            o->type = X64_OPERAND_IMMEDIATE;
            o->size = (uint16_t)(size * 8);
            if (imm_id == 0) {
                o->imm.value = enc == E_IS4 ? (in->imm & 0x0F) : in->imm;
                o->imm.is_signed = (uint8_t)((rec & R_IMM0_SIGNED) != 0);
                o->imm.is_relative = (uint8_t)((rec & R_IMM0_REL) != 0);
            } else {
                o->imm.value = in->imm2;
            }
            imm_id++;
            break;
        default:
            break;
        }
    finalize:
        if (o->type == X64_OPERAND_MEMORY) {
            if (!(w0 >> 18 & 1) && (rec & R_SEGMENT) && in->fsgs) {
                o->mem.segment = in->fsgs == 0x64 ? X64_REG_FS : X64_REG_GS;
            } else if (o->mem.segment == X64_REG_NONE) {
                const unsigned base = o->mem.base;
                o->mem.segment =
                    (base == X64_REG_RSP || base == X64_REG_RBP || base == X64_REG_ESP ||
                     base == X64_REG_EBP || base == X64_REG_SP || base == X64_REG_BP)
                        ? X64_REG_SS
                        : X64_REG_DS;
            }
        }
        switch (o->type) {
        case X64_OPERAND_REGISTER:
            o->size = (uint16_t)(size ? size * 8 : dec_regs[o->reg].width);
            break;
        case X64_OPERAND_MEMORY:
            o->size = (uint16_t)(o->mem.type == X64_MEMOP_AGEN ? asw : size * 8);
            break;
        case X64_OPERAND_POINTER:
            o->size = (uint16_t)(in->imm_size * 8 + in->imm2_size * 8);
            break;
        default:
            break;
        }
    }
    return def->nops;
}

int x64_operand_address(const x64_insn_t* in, const x64_operand_t* o, uint64_t address,
                        uint64_t* out) {
    if (o->type == X64_OPERAND_MEMORY) {
        if (!o->mem.disp_size)
            return 0;
        if (o->mem.base == X64_REG_EIP) {
            *out = (uint32_t)((uint32_t)address + in->len + (uint32_t)o->mem.disp);
            return 1;
        }
        if (o->mem.base == X64_REG_RIP) {
            *out = address + in->len + (uint64_t)o->mem.disp;
            return 1;
        }
        if (o->mem.base == X64_REG_NONE && o->mem.index == X64_REG_NONE) {
            *out = in->address_width == 32 ? (uint64_t)o->mem.disp & 0xFFFFFFFFu
                                           : (uint64_t)o->mem.disp;
            return 1;
        }
        return 0;
    }
    if (o->type == X64_OPERAND_IMMEDIATE && o->imm.is_signed && o->imm.is_relative) {
        *out = address + in->len + o->imm.value;
        return 1;
    }
    return 0;
}

const char* x64_mnemonic_name(unsigned mnemonic) {
    return mnemonic < X64_MN_COUNT ? dec_mnemonic_names[mnemonic] : "invalid";
}

const char* x64_register_name(unsigned reg) {
    return reg < X64_REG_COUNT ? dec_reg_names[reg] : "none";
}

unsigned x64_register_largest(unsigned reg) {
    return reg < X64_REG_COUNT ? dec_regs[reg].largest : X64_REG_NONE;
}

unsigned x64_register_width(unsigned reg) {
    return reg < X64_REG_COUNT ? dec_regs[reg].width : 0;
}

/* The text, written as it goes; the length it would have whatever room there is */
typedef struct text {
    char* p;
    size_t size, len;
} text_t;

static void put(text_t* t, const char* s) {
    for (; *s; s++, t->len++)
        if (t->len + 1 < t->size)
            t->p[t->len] = *s;
}

/* 0x and upper case hex digits, at least @digits of them */
static void put_hex(text_t* t, uint64_t v, unsigned digits) {
    char d[17];
    unsigned n = 0;
    do {
        d[n++] = "0123456789ABCDEF"[v & 15];
        v >>= 4;
    } while (v);
    while (n < digits && n < 16)
        d[n++] = '0';
    put(t, "0x");
    while (n) {
        char c[2];
        c[0] = d[--n];
        c[1] = 0;
        put(t, c);
    }
}

static void put_address(text_t* t, uint64_t address, unsigned address_width) {
    if (address_width == 32)
        put_hex(t, address & 0xFFFFFFFFu, 8);
    else
        put_hex(t, address, 16);
}

/* The size a memory operand is written with, when its other operands do not give it */
static unsigned explicit_size(const x64_insn_t* in, const x64_operand_t* ops, unsigned i) {
    if (ops[i].mem.type == X64_MEMOP_AGEN)
        return 0;
    switch (i) {
    case 0:
        if (in->operand_count_visible < 2)
            return 0;
        if (ops[1].type == X64_OPERAND_UNUSED || ops[1].type == X64_OPERAND_IMMEDIATE ||
            ops[0].size != ops[1].size ||
            (ops[1].type == X64_OPERAND_REGISTER && ops[1].visibility == X64_VISIBILITY_IMPLICIT &&
             ops[1].reg == X64_REG_CL))
            return ops[0].size;
        return 0;
    case 1:
    case 2:
        return ops[i - 1].size != ops[i].size ? ops[i].size : 0;
    default:
        return 0;
    }
}

size_t x64_format(const x64_insn_t* in, const x64_operand_t* ops, uint64_t address, char* buf,
                  size_t size) {
    const struct dec_def* def = &dec_defs[dec_forms[in->form].def];
    const unsigned pfx = def->pfx;
    text_t t;
    unsigned i, shown = 0;
    t.p = buf;
    t.size = size;
    t.len = 0;
    /* Prefixes */
    if (in->enc == X64_ENC_LEGACY) {
        const unsigned lock = in->lock && (def->rec & R_LOCK);
        const unsigned hle = lock || (pfx & P_HLE_WITHOUT_LOCK);
        unsigned rep = 0, repe = 0, repne = 0, bnd = 0, xacquire = 0, xrelease = 0;
        if (in->rep == 0xF2) {
            if (pfx & P_REPNE)
                repne = 1;
            else if ((pfx & P_XACQUIRE) && hle)
                xacquire = 1;
            else if (pfx & P_BND)
                bnd = 1;
        } else if (in->rep == 0xF3) {
            if (pfx & P_REP)
                rep = 1;
            else if (pfx & P_REPE)
                repe = 1;
            else if ((pfx & P_XRELEASE) && hle)
                xrelease = 1;
        }
        if (xacquire)
            put(&t, "xacquire ");
        if (xrelease)
            put(&t, "xrelease ");
        if (lock)
            put(&t, "lock ");
        if (bnd)
            put(&t, "bnd ");
        if ((pfx & P_NOTRACK) && in->notrack)
            put(&t, "notrack ");
        if (rep)
            put(&t, "rep ");
        else if (repe)
            put(&t, "repe ");
        else if (repne)
            put(&t, "repne ");
    }
    put(&t, x64_mnemonic_name(in->mnemonic));
    if (pfx & P_FAR)
        put(&t, " far");
    for (i = 0; i < in->operand_count_visible; i++) {
        const x64_operand_t* o = &ops[i];
        put(&t, shown++ ? ", " : " ");
        switch (o->type) {
        case X64_OPERAND_REGISTER:
            put(&t, x64_register_name(o->reg));
            break;
        case X64_OPERAND_MEMORY: {
            const unsigned base = o->mem.base, index = o->mem.index;
            if (o->mem.type != X64_MEMOP_MIB) {
                switch (explicit_size(in, ops, i)) {
                case 8:
                    put(&t, "byte ptr ");
                    break;
                case 16:
                    put(&t, "word ptr ");
                    break;
                case 32:
                    put(&t, "dword ptr ");
                    break;
                case 48:
                    put(&t, "fword ptr ");
                    break;
                case 64:
                    put(&t, "qword ptr ");
                    break;
                case 80:
                    put(&t, "tbyte ptr ");
                    break;
                case 128:
                    put(&t, "xmmword ptr ");
                    break;
                case 256:
                    put(&t, "ymmword ptr ");
                    break;
                case 512:
                    put(&t, "zmmword ptr ");
                    break;
                default:
                    break;
                }
            }
            if (o->mem.segment == X64_REG_ES || o->mem.segment == X64_REG_CS ||
                o->mem.segment == X64_REG_FS || o->mem.segment == X64_REG_GS) {
                put(&t, x64_register_name(o->mem.segment));
                put(&t, ":");
            }
            put(&t, "[");
            if (o->mem.disp_size && index == X64_REG_NONE &&
                (base == X64_REG_NONE || base == X64_REG_EIP || base == X64_REG_RIP)) {
                uint64_t a = 0;
                x64_operand_address(in, o, address, &a);
                put_address(&t, a, in->address_width);
            } else {
                if (base != X64_REG_NONE)
                    put(&t, x64_register_name(base));
                if (index != X64_REG_NONE) {
                    if (base != X64_REG_NONE)
                        put(&t, "+");
                    put(&t, x64_register_name(index));
                    if (o->mem.scale && o->mem.type != X64_MEMOP_MIB) {
                        char sc[3];
                        sc[0] = '*';
                        sc[1] = (char)('0' + o->mem.scale);
                        sc[2] = 0;
                        put(&t, sc);
                    }
                }
                if (base == X64_REG_NONE && index == X64_REG_NONE) {
                    uint64_t a = 0;
                    x64_operand_address(in, o, address, &a);
                    put_address(&t, a, in->address_width);
                } else if (o->mem.disp_size && o->mem.disp) {
                    if (o->mem.disp < 0) {
                        put(&t, "-");
                        put_hex(&t, (uint64_t)0 - (uint64_t)o->mem.disp, 2);
                    } else {
                        put(&t, "+");
                        put_hex(&t, (uint64_t)o->mem.disp, 2);
                    }
                }
            }
            put(&t, "]");
            break;
        }
        case X64_OPERAND_POINTER:
            put_hex(&t, o->ptr.segment, 4);
            put(&t, ":");
            put_hex(&t, o->ptr.offset, in->operand_width == 16 ? 4 : 8);
            break;
        case X64_OPERAND_IMMEDIATE:
            if (o->imm.is_relative) {
                uint64_t a = 0;
                x64_operand_address(in, o, address, &a);
                put_address(&t, a, in->address_width);
            } else {
                switch (in->operand_width) {
                case 8:
                    put_hex(&t, o->imm.value & 0xFF, 2);
                    break;
                case 16:
                    put_hex(&t, o->imm.value & 0xFFFF, 2);
                    break;
                case 32:
                    put_hex(&t, o->imm.value & 0xFFFFFFFFu, 2);
                    break;
                default:
                    put_hex(&t, o->imm.value, 2);
                    break;
                }
            }
            break;
        default:
            break;
        }
    }
    if (size)
        buf[t.len < size ? t.len : size - 1] = 0;
    return t.len;
}
