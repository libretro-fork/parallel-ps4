/* SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef SHADPS4_COMMON_VA_CTX_H
#define SHADPS4_COMMON_VA_CTX_H

/* A game's variable arguments, as the System V x86-64 ABI passes them: the six integer
 * registers and eight vector registers a variadic HLE function is given (VA_ARGS), saved the way
 * a va_list's register save area holds them (VA_CTX), and read one after another (va_arg_*),
 * from the registers and then from the stack.
 * https://stackoverflow.com/questions/4958384/what-is-the-format-of-the-x86-64-va-list-structure
 *
 * C89 and C++: C++ has the context as Common::VaCtx, the same structure, and vaArgPtr<T>. */

#include <stddef.h>
#include <string.h>

#include <retro_inline.h>

#include "common/arch.h"
#include "common/types.h"

#ifdef ARCH_X86_64
#include <xmmintrin.h>
#else
/* FIXME */
typedef __int128 __m128;
#endif

#define VA_ARGS                                                                                    \
    uint64_t rdi, uint64_t rsi, uint64_t rdx, uint64_t rcx, uint64_t r8, uint64_t r9,              \
        uint64_t overflow_arg_area, __m128 xmm0, __m128 xmm1, __m128 xmm2, __m128 xmm3,            \
        __m128 xmm4, __m128 xmm5, __m128 xmm6, __m128 xmm7

/* The va_list: how far into the integer and vector registers it has read, and where the
 * stack's arguments go on */
typedef struct va_ctx_list {
    u32 gp_offset;
    u32 fp_offset;
    void* overflow_arg_area;
    void* reg_save_area;
} va_ctx_list_t;

/* The register save area: 16-byte aligned, by its vector registers */
typedef struct va_ctx_regs {
    u64 gp[6];
    __m128 fp[8];
} va_ctx_regs_t;

typedef struct va_ctx {
    va_ctx_regs_t reg_save_area;
    va_ctx_list_t list;
} va_ctx_t;

/* Fills @ctx with the registers VA_ARGS was given; the stack's arguments start at
 * &overflow_arg_area */
#define VA_CTX_FILL(ctx)                                                                           \
    do {                                                                                           \
        (ctx).reg_save_area.gp[0] = rdi;                                                           \
        (ctx).reg_save_area.gp[1] = rsi;                                                           \
        (ctx).reg_save_area.gp[2] = rdx;                                                           \
        (ctx).reg_save_area.gp[3] = rcx;                                                           \
        (ctx).reg_save_area.gp[4] = r8;                                                            \
        (ctx).reg_save_area.gp[5] = r9;                                                            \
        (ctx).reg_save_area.fp[0] = xmm0;                                                          \
        (ctx).reg_save_area.fp[1] = xmm1;                                                          \
        (ctx).reg_save_area.fp[2] = xmm2;                                                          \
        (ctx).reg_save_area.fp[3] = xmm3;                                                          \
        (ctx).reg_save_area.fp[4] = xmm4;                                                          \
        (ctx).reg_save_area.fp[5] = xmm5;                                                          \
        (ctx).reg_save_area.fp[6] = xmm6;                                                          \
        (ctx).reg_save_area.fp[7] = xmm7;                                                          \
        (ctx).list.reg_save_area = &(ctx).reg_save_area;                                           \
        (ctx).list.gp_offset = (u32)offsetof(va_ctx_regs_t, gp);                                   \
        (ctx).list.fp_offset = (u32)offsetof(va_ctx_regs_t, fp);                                   \
        (ctx).list.overflow_arg_area = &overflow_arg_area;                                         \
    } while (0)

/* The next 8 bytes of the integer registers or, once they are all read, of the stack */
static INLINE u64 va_arg_gp_u64(va_ctx_list_t* l) {
    u64 v;
    if (l->gp_offset <= 40) {
        memcpy(&v, (const u8*)l->reg_save_area + l->gp_offset, sizeof(v));
        l->gp_offset += 8;
    } else {
        memcpy(&v, l->overflow_arg_area, sizeof(v));
        l->overflow_arg_area = (u8*)l->overflow_arg_area + 8;
    }
    return v;
}

/* An int: the low 4 bytes of the next 8 */
static INLINE int va_arg_int(va_ctx_list_t* l) {
    return (int)(s32)(u32)va_arg_gp_u64(l);
}

/* A long long, or the game's long (8 bytes, whatever the host's long is) */
static INLINE s64 va_arg_s64(va_ctx_list_t* l) {
    return (s64)va_arg_gp_u64(l);
}

/* A double: the next vector register's low 8 bytes or, once they are all read, the stack's */
static INLINE double va_arg_double(va_ctx_list_t* l) {
    double v;
    if (l->fp_offset <= 160) {
        memcpy(&v, (const u8*)l->reg_save_area + l->fp_offset, sizeof(v));
        l->fp_offset += 16;
    } else {
        memcpy(&v, l->overflow_arg_area, sizeof(v));
        l->overflow_arg_area = (u8*)l->overflow_arg_area + 8;
    }
    return v;
}

/* A pointer */
static INLINE void* va_arg_ptr(va_ctx_list_t* l) {
    return (void*)(uintptr_t)va_arg_gp_u64(l);
}

#ifdef __cplusplus

namespace Common {

/// The context, for C++: the C structure, by a name of its own that can be declared ahead
struct VaCtx : va_ctx_t {};

template <class T>
T* vaArgPtr(va_ctx_list_t* l) {
    return static_cast<T*>(va_arg_ptr(l));
}

} // namespace Common

#define VA_CTX(ctx)                                                                                \
    ::Common::VaCtx ctx{};                                                                         \
    VA_CTX_FILL(ctx)

#else

#define VA_CTX(ctx)                                                                                \
    va_ctx_t ctx;                                                                                  \
    memset(&(ctx), 0, sizeof(ctx));                                                                \
    VA_CTX_FILL(ctx)

#endif

#endif
