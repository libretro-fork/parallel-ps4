/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef AMDGPU_SAMPLE_LOCS_H
#define AMDGPU_SAMPLE_LOCS_H

#include <stddef.h>
#include <stdint.h>

#include <retro_common_api.h>

RETRO_BEGIN_DECLS

/* Most a pass can hold: four pixels of eight samples, an x and a y for each */
#define AMDGPU_SAMPLE_LOCS_MAX (4 * 8 * 2)

/* Where the game has put the samples of a pixel (the sixteen
 * PA_SC_AA_SAMPLE_LOCS_PIXEL registers: four for each pixel of a two by two
 * block, X0Y0, X1Y0, X0Y1, X1Y1), for a pass of @samples samples (2, 4 or 8).
 *
 * Returns the side of the block the positions repeat over: 2 when the four
 * pixels differ and @max_grid allows it, else 1 (the first pixel's then stand
 * for all); or 0 when there is nothing to set: no position programmed, or
 * exactly the positions Vulkan uses by itself.
 *
 * @out gets, pixel after pixel (x first, then y) and sample after sample, an
 * x and a y in sixteenths of a pixel from its top left, 0 to 15. */
unsigned amdgpu_sample_locs(const uint32_t regs[16], unsigned samples, unsigned max_grid,
                            uint8_t out[AMDGPU_SAMPLE_LOCS_MAX]);

RETRO_END_DECLS

#endif
