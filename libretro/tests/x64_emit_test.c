/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

/* The x86-64 encoder (src/common/x64_emit.c): each instruction form the core writes at run time
 * (SRT walkers, symbol stubs, CPU patches and their trampolines) against the bytes Xbyak wrote
 * for it, which the encoder replaces. The bytes below were made by Xbyak 7 from the same
 * instructions, once, when the encoder was written; it was also compared with Xbyak over 229,636
 * register, immediate and memory combinations then, all byte for byte the same. Also: an
 * instruction that does not fit is not written and sets the error, and nothing after it is; a
 * forward jump bound too far for its byte is an error. */

#include <stdio.h>
#include <string.h>

#include "common/x64_decode.h"
#include "common/x64_emit.h"

struct x64_case {
    void (*emit)(x64_code_t* c);
    size_t len;
    const char* bytes;
};

/* What Xbyak wrote, generated from Xbyak's own calls beside each */
static void case_0(x64_code_t* c) {
    x64_mov_ri(c, 64, X64_RDI, 5);
}
static void case_1(x64_code_t* c) {
    x64_mov_ri(c, 64, X64_RAX, 0xFFFFFFFFFFFFull);
}
static void case_2(x64_code_t* c) {
    x64_mov_ri(c, 64, X64_R10, 0xFFFFFFFFFFFFull);
}
static void case_3(x64_code_t* c) {
    x64_mov_ri(c, 64, X64_RDX, 0xFFFFFFFFFFFFFFFFull);
}
static void case_4(x64_code_t* c) {
    x64_mov_ri(c, 64, X64_R9, 0xFFFFFFFF80000000ull);
}
static void case_5(x64_code_t* c) {
    x64_mov_ri(c, 32, X64_RCX, 0x12345678);
}
static void case_6(x64_code_t* c) {
    x64_mov_ri(c, 32, X64_R10, 1);
}
static void case_7(x64_code_t* c) {
    x64_mov_rr(c, 8, X64_RCX, X64_RBX);
}
static void case_8(x64_code_t* c) {
    x64_mov_rr(c, 8, X64_RCX, X64_RSI);
}
static void case_9(x64_code_t* c) {
    x64_mov_rr(c, 32, X64_R10, X64_R10);
}
static void case_10(x64_code_t* c) {
    x64_mov_rr(c, 64, X64_RCX, X64_R9);
}
static void case_11(x64_code_t* c) {
    x64_mov_rm(c, 32, X64_RAX, x64_m(X64_RSI, 0x40));
}
static void case_12(x64_code_t* c) {
    x64_mov_rm(c, 32, X64_R10, x64_m(X64_RDI, 0x400));
}
static void case_13(x64_code_t* c) {
    x64_mov_rm(c, 64, X64_RDI, x64_mi(X64_RDI, X64_R10, 1, 0));
}
static void case_14(x64_code_t* c) {
    x64_mov_rm(c, 32, X64_R10, x64_mi(X64_RDI, X64_R10, 1, 0));
}
static void case_15(x64_code_t* c) {
    x64_mov_rm(c, 64, X64_RAX, x64_m(X64_RSP, 0));
}
static void case_16(x64_code_t* c) {
    x64_mov_rm(c, 64, X64_RAX, x64_m(X64_R12, 8));
}
static void case_17(x64_code_t* c) {
    x64_mov_rm(c, 64, X64_RAX, x64_m(X64_RBP, 0));
}
static void case_18(x64_code_t* c) {
    x64_mov_rm(c, 64, X64_RAX, x64_m(X64_R13, 0));
}
static void case_19(x64_code_t* c) {
    x64_mov_rm(c, 64, X64_R11, x64_mi(X64_RBX, X64_R14, 8, -0x1000));
}
static void case_20(x64_code_t* c) {
    x64_mov_mr(c, 32, x64_m(X64_RSI, 0x44), X64_R10);
}
static void case_21(x64_code_t* c) {
    x64_mov_mr(c, 64, x64_m(X64_R15, 0x80), X64_RDX);
}
static void case_22(x64_code_t* c) {
    {x64_mem_t m = x64_mabs(0x1480); m.seg = X64_SEG_GS; x64_mov_rm(c, 64, X64_RDI, m);};
}
static void case_23(x64_code_t* c) {
    {x64_mem_t m = x64_m(X64_RAX, 0x10); m.seg = X64_SEG_GS; x64_mov_rm(c, 64, X64_RBX, m);};
}
static void case_24(x64_code_t* c) {
    {x64_mem_t m = x64_mabs(0x28); m.seg = X64_SEG_GS; x64_alu_rm(c, X64_CMP, 64, X64_R8, m);};
}
static void case_25(x64_code_t* c) {
    x64_alu_ri(c, X64_ADD, 32, X64_R10, 4);
}
static void case_26(x64_code_t* c) {
    x64_alu_ri(c, X64_ADD, 64, X64_RSP, 8);
}
static void case_27(x64_code_t* c) {
    x64_alu_ri(c, X64_AND, 32, X64_RAX, 0x12345);
}
static void case_28(x64_code_t* c) {
    x64_alu_ri(c, X64_AND, 64, X64_RDI, 0x12345);
}
static void case_29(x64_code_t* c) {
    x64_alu_ri(c, X64_SUB, 32, X64_R10, 0x80);
}
static void case_30(x64_code_t* c) {
    x64_alu_ri(c, X64_XOR, 32, X64_R10, 0xFFFFFFFF);
}
static void case_31(x64_code_t* c) {
    x64_alu_ri(c, X64_OR, 64, X64_RAX, 0x3F);
}
static void case_32(x64_code_t* c) {
    x64_alu_rr(c, X64_CMP, 32, X64_R10, X64_RCX);
}
static void case_33(x64_code_t* c) {
    x64_alu_rr(c, X64_AND, 64, X64_RDI, X64_R10);
}
static void case_34(x64_code_t* c) {
    x64_alu_rr(c, X64_XOR, 64, X64_RBX, X64_RAX);
}
static void case_35(x64_code_t* c) {
    x64_alu_rm(c, X64_ADD, 32, X64_R10, x64_m(X64_RSP, 0));
}
static void case_36(x64_code_t* c) {
    x64_alu_mr(c, X64_AND, 32, x64_m(X64_RSP, 0), X64_RDX);
}
static void case_37(x64_code_t* c) {
    x64_imul_rri(c, 32, X64_R10, X64_R10, 3);
}
static void case_38(x64_code_t* c) {
    x64_imul_rri(c, 32, X64_RAX, X64_RAX, 0x10000);
}
static void case_39(x64_code_t* c) {
    x64_imul_rm(c, 32, X64_R10, x64_m(X64_RSP, 0));
}
static void case_40(x64_code_t* c) {
    x64_shift_ri(c, X64_SHL, 32, X64_R10, 2);
}
static void case_41(x64_code_t* c) {
    x64_shift_ri(c, X64_SHR, 64, X64_RAX, 1);
}
static void case_42(x64_code_t* c) {
    x64_shift_rcl(c, X64_SHL, 64, X64_RDX);
}
static void case_43(x64_code_t* c) {
    x64_shift_mcl(c, X64_SHR, 32, x64_m(X64_RSP, 0));
}
static void case_44(x64_code_t* c) {
    x64_not(c, 64, X64_RBX);
}
static void case_45(x64_code_t* c) {
    x64_neg(c, 32, X64_R10);
}
static void case_46(x64_code_t* c) {
    x64_dec(c, 32, X64_RDX);
}
static void case_47(x64_code_t* c) {
    x64_cmov_rr(c, X64_CC_A, 32, X64_R10, X64_RCX);
}
static void case_48(x64_code_t* c) {
    x64_cmov_rm(c, X64_CC_B, 32, X64_R10, x64_m(X64_RSP, 0));
}
static void case_49(x64_code_t* c) {
    {x64_push(c, X64_RDI); x64_push(c, X64_R10); x64_pop(c, X64_R10); x64_pop(c, X64_RDI); x64_pushfq(c); x64_popfq(c); x64_ret(c);};
}
static void case_50(x64_code_t* c) {
    x64_lea(c, X64_RSP, x64_m(X64_RSP, -128));
}
static void case_51(x64_code_t* c) {
    x64_lea(c, X64_RSP, x64_m(X64_RSP, 288));
}
static void case_52(x64_code_t* c) {
    x64_xchg_rm(c, X64_R11, x64_m(X64_RSP, 0));
}
static void case_53(x64_code_t* c) {
    x64_call_m(c, x64_m(X64_RSP, -136));
}
static void case_54(x64_code_t* c) {
    x64_jmp_r(c, X64_RAX);
}
static void case_55(x64_code_t* c) {
    x64_nop(c, 15);
}
static void case_56(x64_code_t* c) {
    x64_vmovups_mr(c, 1, x64_m(X64_RSP, 32), 9);
}
static void case_57(x64_code_t* c) {
    x64_vmovups_rm(c, 1, 3, x64_m(X64_RSP, 128));
}
static void case_58(x64_code_t* c) {
    x64_vmovups_rm(c, 0, 12, x64_m(X64_RAX, 0));
}
static void case_59(x64_code_t* c) {
    x64_vmovups_rm(c, 0, 2, x64_mi(X64_R9, X64_RDX, 2, 0x40));
}
static void case_60(x64_code_t* c) {
    x64_vmovaps_rr(c, 0, 1, 14);
}
static void case_61(x64_code_t* c) {
    x64_vmovq_rx(c, X64_RAX, 3);
}
static void case_62(x64_code_t* c) {
    x64_vmovq_xr(c, 11, X64_RCX);
}
static void case_63(x64_code_t* c) {
    x64_vpextrq(c, X64_RDX, 9, 1);
}
static void case_64(x64_code_t* c) {
    x64_vcmpps(c, 1, 4, 5, 5, 3);
}
static void case_65(x64_code_t* c) {
    x64_vcmpss_rrr(c, 0, 1, 10, 0);
}
static void case_66(x64_code_t* c) {
    x64_vcmpss_rrm(c, 8, 1, x64_m(X64_RDI, 4), 1);
}
static void case_67(x64_code_t* c) {
    x64_vpcmpeqd(c, 0, 6, 6, 6);
}
static void case_68(x64_code_t* c) {
    x64_vpsrld_ri(c, 0, 7, 13, 11);
}
static void case_69(x64_code_t* c) {
    x64_vpslld_ri(c, 1, 12, 12, 24);
}
static void case_70(x64_code_t* c) {
    x64_vxorps_rrr(c, 0, 0, 0, 15);
}
static void case_71(x64_code_t* c) {
    x64_vrcpps(c, 1, 2, 10);
}
static void case_72(x64_code_t* c) {
    x64_vrsqrtps(c, 0, 9, 1);
}
static void case_73(x64_code_t* c) {
    x64_rcpps(c, 9, 1);
}
static void case_74(x64_code_t* c) {
    x64_rsqrtps(c, 2, 12);
}
static void case_75(x64_code_t* c) {
    x64_xorps(c, 2, 3);
}
static void case_76(x64_code_t* c) {
    x64_vblendvps(c, 1, 1, 1, 11, 4);
}
static void case_77(x64_code_t* c) {
    x64_vgatherdps(c, 0, 1, x64_mi(X64_RAX, 3, 1, 0), 2);
}
static void case_78(x64_code_t* c) {
    x64_vgatherdps(c, 1, 9, x64_mi(X64_RCX, 10, 4, 0), 12);
}
static void case_79(x64_code_t* c) {
    {x64_nop(c, 4); x64_jmp(c, (uint64_t)(uintptr_t)(c->p + 100), X64_JMP_AUTO);};
}
static void case_80(x64_code_t* c) {
    {x64_nop(c, 4); x64_jmp(c, (uint64_t)(uintptr_t)(c->p + 100), X64_JMP_NEAR);};
}
static void case_81(x64_code_t* c) {
    {x64_nop(c, 4); x64_jmp(c, (uint64_t)(uintptr_t)(c->p + 0x10000), X64_JMP_AUTO);};
}
static void case_82(x64_code_t* c) {
    {x64_nop(c, 4); x64_jmp(c, (uint64_t)(uintptr_t)(c->p + 100), X64_JMP_SHORT);};
}
static void case_83(x64_code_t* c) {
    {x64_nop(c, 200); x64_jmp(c, (uint64_t)(uintptr_t)(c->p + 10), X64_JMP_AUTO);};
}
static void case_84(x64_code_t* c) {
    {size_t lz, end; x64_alu_ri(c, X64_AND, 64, X64_RCX, 0x3F); lz = x64_jcc_fwd(c, X64_CC_Z); x64_mov_ri(c, 64, X64_RDX, 1); x64_shift_rcl(c, X64_SHL, 64, X64_RDX); x64_dec(c, 64, X64_RDX); end = x64_jmp_fwd(c); x64_bind(c, lz); x64_mov_ri(c, 64, X64_RDX, 0xFFFFFFFFFFFFFFFFull); x64_bind(c, end);};
}
static void case_85(x64_code_t* c) {
    {size_t done; uint8_t* at = x64_curr(c); x64_vxorps_rrm(c, 0, 3, 3, x64_mrip((uint64_t)(uintptr_t)(at + 10))); done = x64_jmp_fwd(c); x64_dq(c, 0xFFFFFFFF); x64_dq(c, 0); x64_bind(c, done);};
}
static void case_86(x64_code_t* c) {
    x64_mov_rm(c, 64, X64_RAX, x64_mrip((uint64_t)(uintptr_t)(c->p + 0x1000)));
}
static void case_87(x64_code_t* c) {
    {x64_mov_ri(c, 64, X64_RDI, 0x1234); x64_mov_ri(c, 64, X64_RAX, 0x7f1234567890ull); x64_jmp_r(c, X64_RAX);};
}

static const struct x64_case cases[] = {
    {case_0, 5, "\xbf\x05\x00\x00\x00"},
    {case_1, 10, "\x48\xb8\xff\xff\xff\xff\xff\xff\x00\x00"},
    {case_2, 10, "\x49\xba\xff\xff\xff\xff\xff\xff\x00\x00"},
    {case_3, 7, "\x48\xc7\xc2\xff\xff\xff\xff"},
    {case_4, 7, "\x49\xc7\xc1\x00\x00\x00\x80"},
    {case_5, 5, "\xb9\x78\x56\x34\x12"},
    {case_6, 6, "\x41\xba\x01\x00\x00\x00"},
    {case_7, 2, "\x88\xd9"},
    {case_8, 3, "\x40\x88\xf1"},
    {case_9, 3, "\x45\x89\xd2"},
    {case_10, 3, "\x4c\x89\xc9"},
    {case_11, 3, "\x8b\x46\x40"},
    {case_12, 7, "\x44\x8b\x97\x00\x04\x00\x00"},
    {case_13, 4, "\x4a\x8b\x3c\x17"},
    {case_14, 4, "\x46\x8b\x14\x17"},
    {case_15, 4, "\x48\x8b\x04\x24"},
    {case_16, 5, "\x49\x8b\x44\x24\x08"},
    {case_17, 4, "\x48\x8b\x45\x00"},
    {case_18, 4, "\x49\x8b\x45\x00"},
    {case_19, 8, "\x4e\x8b\x9c\xf3\x00\xf0\xff\xff"},
    {case_20, 4, "\x44\x89\x56\x44"},
    {case_21, 7, "\x49\x89\x97\x80\x00\x00\x00"},
    {case_22, 9, "\x65\x48\x8b\x3c\x25\x80\x14\x00\x00"},
    {case_23, 5, "\x65\x48\x8b\x58\x10"},
    {case_24, 9, "\x65\x4c\x3b\x04\x25\x28\x00\x00\x00"},
    {case_25, 4, "\x41\x83\xc2\x04"},
    {case_26, 4, "\x48\x83\xc4\x08"},
    {case_27, 5, "\x25\x45\x23\x01\x00"},
    {case_28, 7, "\x48\x81\xe7\x45\x23\x01\x00"},
    {case_29, 7, "\x41\x81\xea\x80\x00\x00\x00"},
    {case_30, 4, "\x41\x83\xf2\xff"},
    {case_31, 4, "\x48\x83\xc8\x3f"},
    {case_32, 3, "\x41\x39\xca"},
    {case_33, 3, "\x4c\x21\xd7"},
    {case_34, 3, "\x48\x31\xc3"},
    {case_35, 4, "\x44\x03\x14\x24"},
    {case_36, 3, "\x21\x14\x24"},
    {case_37, 4, "\x45\x6b\xd2\x03"},
    {case_38, 6, "\x69\xc0\x00\x00\x01\x00"},
    {case_39, 5, "\x44\x0f\xaf\x14\x24"},
    {case_40, 4, "\x41\xc1\xe2\x02"},
    {case_41, 3, "\x48\xd1\xe8"},
    {case_42, 3, "\x48\xd3\xe2"},
    {case_43, 3, "\xd3\x2c\x24"},
    {case_44, 3, "\x48\xf7\xd3"},
    {case_45, 3, "\x41\xf7\xda"},
    {case_46, 2, "\xff\xca"},
    {case_47, 4, "\x44\x0f\x47\xd1"},
    {case_48, 5, "\x44\x0f\x42\x14\x24"},
    {case_49, 9, "\x57\x41\x52\x41\x5a\x5f\x9c\x9d\xc3"},
    {case_50, 5, "\x48\x8d\x64\x24\x80"},
    {case_51, 8, "\x48\x8d\xa4\x24\x20\x01\x00\x00"},
    {case_52, 4, "\x4c\x87\x1c\x24"},
    {case_53, 7, "\xff\x94\x24\x78\xff\xff\xff"},
    {case_54, 2, "\xff\xe0"},
    {case_55, 15, "\x66\x0f\x1f\x84\x00\x00\x00\x00\x00\x66\x0f\x1f\x44\x00\x00"},
    {case_56, 6, "\xc5\x7c\x11\x4c\x24\x20"},
    {case_57, 9, "\xc5\xfc\x10\x9c\x24\x80\x00\x00\x00"},
    {case_58, 4, "\xc5\x78\x10\x20"},
    {case_59, 7, "\xc4\xc1\x78\x10\x54\x51\x40"},
    {case_60, 5, "\xc4\xc1\x78\x28\xce"},
    {case_61, 5, "\xc4\xe1\xf9\x7e\xd8"},
    {case_62, 5, "\xc4\x61\xf9\x6e\xd9"},
    {case_63, 6, "\xc4\x63\xf9\x16\xca\x01"},
    {case_64, 5, "\xc5\xd4\xc2\xe5\x03"},
    {case_65, 6, "\xc4\xc1\x72\xc2\xc2\x00"},
    {case_66, 6, "\xc5\x72\xc2\x47\x04\x01"},
    {case_67, 4, "\xc5\xc9\x76\xf6"},
    {case_68, 6, "\xc4\xc1\x41\x72\xd5\x0b"},
    {case_69, 6, "\xc4\xc1\x1d\x72\xf4\x18"},
    {case_70, 5, "\xc4\xc1\x78\x57\xc7"},
    {case_71, 5, "\xc4\xc1\x7c\x53\xd2"},
    {case_72, 4, "\xc5\x78\x52\xc9"},
    {case_73, 4, "\x44\x0f\x53\xc9"},
    {case_74, 4, "\x41\x0f\x52\xd4"},
    {case_75, 3, "\x0f\x57\xd3"},
    {case_76, 6, "\xc4\xc3\x75\x4a\xcb\x40"},
    {case_77, 6, "\xc4\xe2\x69\x92\x0c\x18"},
    {case_78, 6, "\xc4\x22\x1d\x92\x0c\x91"},
    {case_79, 6, "\x0f\x1f\x40\x00\xeb\x5e"},
    {case_80, 9, "\x0f\x1f\x40\x00\xe9\x5b\x00\x00\x00"},
    {case_81, 9, "\x0f\x1f\x40\x00\xe9\xf7\xff\x00\x00"},
    {case_82, 6, "\x0f\x1f\x40\x00\xeb\x5e"},
    {case_83, 205, "\x66\x0f\x1f\x84\x00\x00\x00\x00\x00\x66\x0f\x1f\x84\x00\x00\x00\x00\x00\x66\x0f\x1f\x84\x00\x00\x00\x00\x00\x66\x0f\x1f\x84\x00\x00\x00\x00\x00\x66\x0f\x1f\x84\x00\x00\x00\x00\x00\x66\x0f\x1f\x84\x00\x00\x00\x00\x00\x66\x0f\x1f\x84\x00\x00\x00\x00\x00\x66\x0f\x1f\x84\x00\x00\x00\x00\x00\x66\x0f\x1f\x84\x00\x00\x00\x00\x00\x66\x0f\x1f\x84\x00\x00\x00\x00\x00\x66\x0f\x1f\x84\x00\x00\x00\x00\x00\x66\x0f\x1f\x84\x00\x00\x00\x00\x00\x66\x0f\x1f\x84\x00\x00\x00\x00\x00\x66\x0f\x1f\x84\x00\x00\x00\x00\x00\x66\x0f\x1f\x84\x00\x00\x00\x00\x00\x66\x0f\x1f\x84\x00\x00\x00\x00\x00\x66\x0f\x1f\x84\x00\x00\x00\x00\x00\x66\x0f\x1f\x84\x00\x00\x00\x00\x00\x66\x0f\x1f\x84\x00\x00\x00\x00\x00\x66\x0f\x1f\x84\x00\x00\x00\x00\x00\x66\x0f\x1f\x84\x00\x00\x00\x00\x00\x66\x0f\x1f\x84\x00\x00\x00\x00\x00\x66\x90\xe9\x3d\xff\xff\xff"},
    {case_84, 26, "\x48\x83\xe1\x3f\x74\x0d\xba\x01\x00\x00\x00\x48\xd3\xe2\x48\xff\xca\xeb\x07\x48\xc7\xc2\xff\xff\xff\xff"},
    {case_85, 26, "\xc5\xe0\x57\x1d\x02\x00\x00\x00\xeb\x10\xff\xff\xff\xff\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"},
    {case_86, 7, "\x48\x8b\x05\xf9\x0f\x00\x00"},
    {case_87, 17, "\xbf\x34\x12\x00\x00\x48\xb8\x90\x78\x56\x34\x12\x7f\x00\x00\xff\xe0"},
};

static int failures;

static void expect(const char* what, int ok) {
    if (!ok) {
        printf("x64_emit: %s\n", what);
        failures++;
    }
}

int main(void) {
    static uint8_t code[1 << 17];
    x64_code_t c;
    size_t i, at;
    char what[64];
    for (i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        x64_init(&c, code, sizeof(code));
        cases[i].emit(&c);
        sprintf(what, "case %u: not the bytes Xbyak wrote", (unsigned)i);
        expect(what, !c.error && c.size == cases[i].len && memcmp(code, cases[i].bytes, c.size) == 0);
    }

    /* Room for 5 bytes: a 10-byte move does not fit, and nothing is written after it */
    x64_init(&c, code, 5);
    memset(code, 0xCC, 16);
    x64_mov_ri(&c, 64, X64_RAX, 0x123456789abcdefull);
    x64_ret(&c);
    expect("an instruction that does not fit: an error, nothing written",
           c.error && c.size == 0 && code[0] == 0xCC);

    /* A short forward jump bound 200 bytes on */
    x64_init(&c, code, sizeof(code));
    at = x64_jmp_fwd(&c);
    x64_nop(&c, 200);
    x64_bind(&c, at);
    expect("a forward jump too far for its byte: an error", c.error);

    /* A jump that must be short, to a target 200 bytes on */
    x64_init(&c, code, sizeof(code));
    x64_jmp(&c, (uint64_t)(uintptr_t)(code + 200), X64_JMP_SHORT);
    expect("a short jump too far: an error, nothing written", c.error && c.size == 0);

    /* Relocation: a guest instruction copied to run at code[0], from where it lay (@from) */
    {
        struct reloc_case {
            const char* what;
            const char* bytes;
            unsigned n;
            int64_t from; /* from code[0] */
            int ok;
            const char* expect; /* the copy, the offset or displacement patched in */
            unsigned expect_n;
        };
        static const struct reloc_case rc[] = {
            {"a plain instruction: byte for byte", "\x66\x2e\x0f\x1f\x84\x00\x00\x00\x00\x00",
             10, 0x100000, 1, "\x66\x2e\x0f\x1f\x84\x00\x00\x00\x00\x00", 10},
            {"a rip-relative operand: the same address", "\x48\x8b\x05\x10\x00\x00\x00", 7,
             0x1000, 1, "\x48\x8b\x05\x10\x10\x00\x00", 7},
            {"a rip-relative operand out of reach: no copy", "\x48\x8b\x05\x10\x00\x00\x00", 7,
             0x100000000ll, 0, "", 0},
            {"a short Jcc in reach: still short", "\x74\x10", 2, 0x40, 1, "\x74\x50", 2},
            {"a short Jcc out of reach: its near form", "\x74\x10", 2, 0x1000, 1,
             "\x0f\x84\x0c\x10\x00\x00", 6},
            {"a short JMP out of reach: its near form", "\xeb\xfe", 2, -0x1000, 1,
             "\xe9\xfb\xef\xff\xff", 5},
            {"a hinted short Jcc out of reach: near, the hint left", "\x3e\x75\x00", 3, 0x80, 1,
             "\x0f\x85\x7d\x00\x00\x00", 6},
            {"a short Jcc with 66 out of reach: no copy", "\x66\x74\x10", 3, 0x1000, 0, "", 0},
            {"LOOP out of reach: no copy", "\xe2\xfe", 2, 0x1000, 0, "", 0},
            {"a short Jcc beyond 2 GB: no copy", "\x74\x10", 2, 0x100000000ll, 0, "", 0},
            {"a near Jcc: still near", "\x0f\x84\x00\x01\x00\x00", 6, 0x2000, 1,
             "\x0f\x84\x00\x21\x00\x00", 6},
            {"a call: the same target", "\xe8\xfb\xff\xff\xff", 5, -0x1000, 1,
             "\xe8\xfb\xef\xff\xff", 5},
            {"LOOP in reach", "\xe2\xfe", 2, 0x10, 1, "\xe2\x0e", 2},
            {"JECXZ (67) in reach", "\x67\xe3\x00", 3, -0x20, 1, "\x67\xe3\xe0", 3},
        };
        static uint8_t guest[16];
        for (i = 0; i < sizeof(rc) / sizeof(rc[0]); i++) {
            x64_insn_t in;
            int ok;
            memcpy(guest, rc[i].bytes, rc[i].n);
            x64_init(&c, code, sizeof(code));
            ok = x64_decode(guest, rc[i].n, &in) == rc[i].n &&
                 x64_relocate(&c, guest, &in, (uint64_t)(uintptr_t)code + (uint64_t)rc[i].from);
            expect(rc[i].what, ok == rc[i].ok && !c.error && c.size == rc[i].expect_n &&
                                   memcmp(code, rc[i].expect, rc[i].expect_n) == 0);
        }
        /* No room: an error, nothing written */
        memcpy(guest, "\x48\x8b\x05\x10\x00\x00\x00", 7);
        x64_init(&c, code, 4);
        {
            x64_insn_t in;
            x64_decode(guest, 7, &in);
            expect("a copy that does not fit: an error",
                   !x64_relocate(&c, guest, &in, (uint64_t)(uintptr_t)code) && c.error &&
                       c.size == 0);
        }
    }

    if (failures == 0) {
        printf("x64_emit: all passed\n");
    }
    return failures != 0;
}
