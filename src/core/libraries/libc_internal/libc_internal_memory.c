/* SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <string.h>

#include "core/libraries/libc_internal/libc_internal_memory.h"

void* PS4_SYSV_ABI internal_memset(void* s, int c, size_t n) {
    return memset(s, c, n);
}

void* PS4_SYSV_ABI internal_memcpy(void* dest, const void* src, size_t n) {
    return memcpy(dest, src, n);
}

s32 PS4_SYSV_ABI internal_memcpy_s(void* dest, size_t destsz, const void* src, size_t count) {
#ifdef _WIN64
    return memcpy_s(dest, destsz, src, count);
#else
    (void)destsz;
    memcpy(dest, src, count);
    return 0; /* ALL OK */
#endif
}

s32 PS4_SYSV_ABI internal_memcmp(const void* s1, const void* s2, size_t n) {
    return memcmp(s1, s2, n);
}

void* PS4_SYSV_ABI internal_memmove(void* dest, const void* src, size_t n) {
    return memmove(dest, src, n);
}

static u64 g_mspace_atomic_id_mask = 0;
static u64 g_mstate_table[64] = {0};

void PS4_SYSV_ABI sceLibcHeapGetTraceInfo(HeapInfoInfo* info) {
    info->mspace_atomic_id_mask = &g_mspace_atomic_id_mask;
    info->mstate_table = g_mstate_table;
    info->getSegmentInfo = 0;
}
