/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

/* amdgpu_sample_locs: the game's sample position registers as positions */

#include <stdio.h>
#include <string.h>

#include "../../src/video_core/amdgpu/sample_locs.h"

static int failures = 0;

#define CHECK(cond)                                                                            \
    do {                                                                                       \
        if (!(cond)) {                                                                         \
            printf("FAIL %s (line %d)\n", #cond, __LINE__);                                    \
            ++failures;                                                                        \
        }                                                                                      \
    } while (0)

/* A sample's byte: x and y in sixteenths from the pixel's centre, -8 to 7 */
static uint32_t loc(int x, int y) {
    return ((uint32_t)x & 0xfu) | (((uint32_t)y & 0xfu) << 4);
}

int main(void) {
    uint32_t regs[16];
    uint8_t out[AMDGPU_SAMPLE_LOCS_MAX];
    unsigned i;

    /* Nothing programmed */
    memset(regs, 0, sizeof(regs));
    CHECK(amdgpu_sample_locs(regs, 2, 2, out) == 0);

    /* Vulkan's own two positions on every pixel: nothing to set */
    for (i = 0; i < 4; i++)
        regs[i * 4] = loc(4, 4) | loc(-4, -4) << 8;
    CHECK(amdgpu_sample_locs(regs, 2, 2, out) == 0);
    CHECK(amdgpu_sample_locs(regs, 2, 1, out) == 0);

    /* The same on every pixel but not Vulkan's: one pixel's worth */
    for (i = 0; i < 4; i++)
        regs[i * 4] = loc(-4, 0) | loc(4, 0) << 8;
    CHECK(amdgpu_sample_locs(regs, 2, 2, out) == 1);
    CHECK(out[0] == 4 && out[1] == 8 && out[2] == 12 && out[3] == 8);

    /* A checkerboard: sample 0 left on the first row, right on the second */
    regs[0] = regs[4] = loc(-4, 0) | loc(4, 0) << 8;
    regs[8] = regs[12] = loc(4, 0) | loc(-4, 0) << 8;
    CHECK(amdgpu_sample_locs(regs, 2, 2, out) == 2);
    CHECK(out[0] == 4 && out[1] == 8 && out[2] == 12 && out[3] == 8);     /* X0Y0 */
    CHECK(out[4] == 4 && out[5] == 8 && out[6] == 12 && out[7] == 8);     /* X1Y0 */
    CHECK(out[8] == 12 && out[9] == 8 && out[10] == 4 && out[11] == 8);   /* X0Y1 */
    CHECK(out[12] == 12 && out[13] == 8 && out[14] == 4 && out[15] == 8); /* X1Y1 */
    /* A device that cannot vary them by pixel gets the first pixel's */
    CHECK(amdgpu_sample_locs(regs, 2, 1, out) == 1);
    CHECK(out[0] == 4 && out[1] == 8 && out[2] == 12 && out[3] == 8);

    /* The ends of the range, and samples past the fourth in the second register */
    memset(regs, 0, sizeof(regs));
    for (i = 0; i < 4; i++) {
        regs[i * 4] = loc(-8, 7) | loc(7, -8) << 8 | loc(1, 2) << 16 | loc(3, 4) << 24;
        regs[i * 4 + 1] = loc(-1, -2) | loc(-3, -4) << 8 | loc(5, 6) << 16 | loc(-5, -6) << 24;
    }
    CHECK(amdgpu_sample_locs(regs, 8, 2, out) == 1);
    CHECK(out[0] == 0 && out[1] == 15 && out[2] == 15 && out[3] == 0);
    CHECK(out[8] == 7 && out[9] == 6 && out[14] == 3 && out[15] == 2);

    /* Vulkan's own four and eight */
    for (i = 0; i < 4; i++) {
        regs[i * 4] = loc(-2, -6) | loc(6, -2) << 8 | loc(-6, 2) << 16 | loc(2, 6) << 24;
        regs[i * 4 + 1] = 0;
    }
    CHECK(amdgpu_sample_locs(regs, 4, 2, out) == 0);
    for (i = 0; i < 4; i++) {
        regs[i * 4] = loc(1, -3) | loc(-1, 3) << 8 | loc(5, 1) << 16 | loc(-3, -5) << 24;
        regs[i * 4 + 1] = loc(-5, 5) | loc(-7, -1) << 8 | loc(3, 7) << 16 | loc(7, -7) << 24;
    }
    CHECK(amdgpu_sample_locs(regs, 8, 2, out) == 0);

    /* One sample has no positions to set */
    CHECK(amdgpu_sample_locs(regs, 1, 2, out) == 0);

    if (failures) {
        printf("sample_locs: %d failed\n", failures);
        return 1;
    }
    printf("sample_locs: all passed\n");
    return 0;
}
