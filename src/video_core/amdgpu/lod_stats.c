/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <retro_atomic.h>

#include "video_core/amdgpu/lod_stats.h"

/* How often each bank was bound since the last report */
static retro_atomic_int_t g_uses[LOD_STATS_BANKS];

void lod_stats_mark(unsigned bank) {
    retro_atomic_inc_int(&g_uses[bank & (LOD_STATS_BANKS - 1)]);
}

void lod_stats_collect(uint64_t entries[LOD_STATS_BANKS]) {
    unsigned i;
    for (i = 0; i < LOD_STATS_BANKS; i++) {
        const unsigned uses = (unsigned)retro_atomic_exchange_int(&g_uses[i], 0);
        if (uses != 0) {
            /* 24 bits of count, a bind worth 256 */
            const uint64_t count = uses > 0xffffu ? 0xffffffu : (uint64_t)uses * 256u;
            entries[i] = ((uint64_t)LOD_STATS_MIP << 56) | (count << 32);
        } else {
            entries[i] = (uint64_t)0xf << 56;
        }
    }
}

unsigned lod_stats_packet(const uint32_t words[3], uint64_t* address) {
    const uint64_t size = (uint64_t)words[0] + 0x20;
    *address = ((uint64_t)words[1] << 6) | ((uint64_t)(words[2] & 3u) << 38);
    if (size < LOD_STATS_REPORT_OFFSET + LOD_STATS_REPORT_BYTES) {
        return 0;
    }
    return (unsigned)((size - LOD_STATS_REPORT_OFFSET) / LOD_STATS_REPORT_BYTES);
}
