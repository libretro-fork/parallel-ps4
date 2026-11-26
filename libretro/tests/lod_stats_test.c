/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

/* The GPU's texture detail statistics (src/video_core/amdgpu/lod_stats.h): the packet's
 * buffer as the game's own Gnm call writes it, a bank bound and a bank not, the count's cap,
 * and the banks starting anew at each report. */

#include <stdio.h>

#include "video_core/amdgpu/lod_stats.h"

static int failed;
#define CHECK(c, what)                                                                             \
    do {                                                                                           \
        if (!(c)) {                                                                                \
            printf("FAIL: %s\n", what);                                                            \
            failed = 1;                                                                            \
        }                                                                                          \
    } while (0)

int main(void) {
    uint64_t entries[LOD_STATS_BANKS];
    uint64_t address = 0;
    uint32_t words[3];
    unsigned i;

    /* A buffer of 0x1040 bytes at 0x12_3456_7880: two reports */
    words[0] = 0x1040 - 0x20;
    words[1] = (uint32_t)(0x1234567880ull >> 6);
    words[2] = 0xbfc00u | (uint32_t)((0x1234567880ull >> 38) & 3u);
    CHECK(lod_stats_packet(words, &address) == 2, "a buffer of two reports");
    CHECK(address == 0x1234567880ull, "its address, the bits above 38 too");
    words[0] = 0x840 - 0x20;
    CHECK(lod_stats_packet(words, &address) == 1, "a buffer of one");
    words[0] = 0x83f - 0x20;
    CHECK(lod_stats_packet(words, &address) == 0, "a byte short of one: none");
    words[0] = 0;
    CHECK(lod_stats_packet(words, &address) == 0, "no buffer: none");

    lod_stats_collect(entries);
    for (i = 0; i < LOD_STATS_BANKS; i++)
        CHECK(entries[i] == (uint64_t)0xf << 56, "nothing bound: not sampled");

    lod_stats_mark(7);
    lod_stats_mark(7);
    lod_stats_mark(255);
    lod_stats_mark(256 + 3); /* a bank is 8 bits */
    for (i = 0; i < 70000; i++)
        lod_stats_mark(9);
    lod_stats_collect(entries);
    CHECK(entries[7] == (((uint64_t)LOD_STATS_MIP << 56) | ((uint64_t)512 << 32)),
          "bound twice: sampled at the mip, counted 512");
    CHECK((entries[255] >> 56) == LOD_STATS_MIP && (entries[3] >> 56) == LOD_STATS_MIP,
          "the last bank, and one past it wraps");
    CHECK(((entries[9] >> 32) & 0xffffff) == 0xffffff && (entries[9] >> 56) == LOD_STATS_MIP,
          "the count stops at 24 bits and stays out of the mip's");
    CHECK(entries[8] == (uint64_t)0xf << 56, "its neighbour: not sampled");
    CHECK((entries[7] & 0xffffffffu) == 0, "the low word is clear");

    lod_stats_collect(entries);
    CHECK(entries[7] == (uint64_t)0xf << 56, "the next report starts anew");

    if (!failed)
        printf("lod_stats: all passed\n");
    return failed;
}
