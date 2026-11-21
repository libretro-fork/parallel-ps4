/* SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef SHADPS4_COMMON_TYPES_H
#define SHADPS4_COMMON_TYPES_H

/* The core's integer names, for C and C++ alike. */

#include <stdint.h>

typedef int8_t s8;
typedef int16_t s16;
typedef int32_t s32;
typedef int64_t s64;

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef float f32;
typedef double f64;

typedef uintptr_t VAddr;
typedef uintptr_t PAddr;

/* The game's calling convention, for the functions it calls */
#if defined(__GNUC__) && (defined(__x86_64__) || defined(_M_X64))
#define PS4_SYSV_ABI __attribute__((sysv_abi))
#else
#define PS4_SYSV_ABI
#endif

#ifdef __cplusplus
// UDLs for memory size values
constexpr unsigned long long operator""_KB(unsigned long long x) {
    return 1024ULL * x;
}
constexpr unsigned long long operator""_MB(unsigned long long x) {
    return 1024_KB * x;
}
constexpr unsigned long long operator""_GB(unsigned long long x) {
    return 1024_MB * x;
}
#endif

#endif
