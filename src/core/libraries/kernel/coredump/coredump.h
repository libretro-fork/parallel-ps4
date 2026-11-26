/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

/* libSceCoredump, in libkernel (coredump.c): a game's handler for when it crashes, kept and
 * never called. Registered in kernel.cpp. */

#ifndef SHADPS4_LIBRARIES_KERNEL_COREDUMP_H
#define SHADPS4_LIBRARIES_KERNEL_COREDUMP_H

#include "common/types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef s32 PS4_SYSV_ABI (*OrbisCoredumpHandler)(void* common);

typedef struct OrbisCoredumpStopInfoCpu {
    void* thread;
    s32 reason_code;
} OrbisCoredumpStopInfoCpu;

typedef struct OrbisCoredumpStopInfoGpu {
    u64 timestamp;
} OrbisCoredumpStopInfoGpu;

typedef struct OrbisCoredumpThreadContextInfo {
    u64 rdi;
    u64 rsi;
    u64 rdx;
    u64 rcx;
    u64 r8;
    u64 r9;
    u64 rax;
    u64 rbx;
    u64 rbp;
    u64 r10;
    u64 r11;
    u64 r12;
    u64 r13;
    u64 r14;
    u64 r15;
    u64 rip;
    u64 rflags;
    u64 rsp;
} OrbisCoredumpThreadContextInfo;

s32 PS4_SYSV_ABI sceCoredumpRegisterCoredumpHandler(OrbisCoredumpHandler handler, u64 stack_size,
                                                    void* common);
s32 PS4_SYSV_ABI sceCoredumpUnregisterCoredumpHandler(void);

#ifdef __cplusplus
}
#endif

#endif
