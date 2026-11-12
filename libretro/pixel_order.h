/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

/* Ten-bit pixels from the order the GPU frames hold them in to libretro's. */

#ifndef SHADPS4_LIBRETRO_PIXEL_ORDER_H
#define SHADPS4_LIBRETRO_PIXEL_ORDER_H

#include <stddef.h>
#include <stdint.h>

/* @count pixels of A2B10G10R10 (R in bits 0-9, B in 20-29: the frames' format)
 * to XRGB2101010 (B in bits 0-9, R in 20-29: libretro's HDR10 and ten-bit
 * formats), from @src to @dst: R and B change places, G and the top two bits
 * stay. Nothing is rounded. */
static void pixel_order_xrgb2101010(uint32_t *dst, const uint32_t *src,
                                    size_t count) {
  size_t i;
  for (i = 0; i < count; i++) {
    const uint32_t p = src[i];
    dst[i] = (p & 0xc00ffc00u) | ((p & 0x3ffu) << 20) | ((p >> 20) & 0x3ffu);
  }
}

#endif
