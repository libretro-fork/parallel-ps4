/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <string.h>

#include "video_core/amdgpu/sample_locs.h"

/* Vulkan's own positions, in sixteenths of a pixel */
static const uint8_t standard_2[] = {12, 12, 4, 4};
static const uint8_t standard_4[] = {6, 2, 14, 6, 2, 10, 10, 14};
static const uint8_t standard_8[] = {9, 5, 7, 11, 13, 9, 5, 3, 3, 13, 1, 7, 11, 15, 15, 1};

/* One pixel's positions. A register holds four samples, a byte each: x in the
 * low four bits and y in the high four, both signed, in sixteenths of a pixel
 * from the pixel's centre. */
static void pixel_locs(const uint32_t* regs, unsigned samples, uint8_t* out) {
    unsigned i;
    for (i = 0; i < samples; i++) {
        const uint32_t b = (regs[i >> 2] >> ((i & 3) * 8)) & 0xffu;
        out[i * 2 + 0] = (uint8_t)(((b & 0xfu) ^ 8u));        /* signed nibble + 8 */
        out[i * 2 + 1] = (uint8_t)((((b >> 4) & 0xfu) ^ 8u)); /* signed nibble + 8 */
    }
}

unsigned amdgpu_sample_locs(const uint32_t regs[16], unsigned samples, unsigned max_grid,
                            uint8_t out[AMDGPU_SAMPLE_LOCS_MAX]) {
    const uint8_t* standard;
    const size_t bytes = (size_t)samples * 2;
    unsigned i, any = 0;
    if (samples == 2)
        standard = standard_2;
    else if (samples == 4)
        standard = standard_4;
    else if (samples == 8)
        standard = standard_8;
    else
        return 0;
    for (i = 0; i < 16; i++)
        any |= regs[i] != 0;
    if (!any)
        return 0;
    for (i = 0; i < 4; i++)
        pixel_locs(regs + i * 4, samples, out + i * bytes);
    if (max_grid >= 2 && (memcmp(out, out + bytes, bytes) || memcmp(out, out + bytes * 2, bytes) ||
                          memcmp(out, out + bytes * 3, bytes)))
        return 2;
    return memcmp(out, standard, bytes) ? 1 : 0;
}
