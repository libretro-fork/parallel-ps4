/* SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef SHADPS4_LIBC_INTERNAL_MEMORY_H
#define SHADPS4_LIBC_INTERNAL_MEMORY_H

/* libSceLibcInternal's memory functions: the host's, and the heap's trace info, which shows
 * an empty heap. Defined in libc_internal_memory.c, registered in libc_internal.cpp. */

#include <stddef.h>

#include <retro_common_api.h>

#include "common/types.h"

RETRO_BEGIN_DECLS

typedef struct HeapInfoInfo {
    u64 size;
    u32 flag;
    u32 getSegmentInfo;
    u64* mspace_atomic_id_mask;
    u64* mstate_table;
} HeapInfoInfo;

void* PS4_SYSV_ABI internal_memset(void* s, int c, size_t n);
void* PS4_SYSV_ABI internal_memcpy(void* dest, const void* src, size_t n);
s32 PS4_SYSV_ABI internal_memcpy_s(void* dest, size_t destsz, const void* src, size_t count);
s32 PS4_SYSV_ABI internal_memcmp(const void* s1, const void* s2, size_t n);
void* PS4_SYSV_ABI internal_memmove(void* dest, const void* src, size_t n);
void PS4_SYSV_ABI sceLibcHeapGetTraceInfo(HeapInfoInfo* info);

RETRO_END_DECLS

#endif
