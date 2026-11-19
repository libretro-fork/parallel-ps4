/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "core/libraries/font/font_blit.h"

#include <string.h>

void font_glyph_blit(unsigned char* dst, size_t width_byte, int bpp, const unsigned char* cov,
                     int glyph_w, int dest_x, int dest_y, int start_row, int end_row, int start_col,
                     int end_col) {
    int row;
    if (end_col <= start_col)
        return;
    for (row = start_row; row < end_row; row++) {
        const unsigned char* src =
            cov + (size_t)(row - dest_y) * (size_t)glyph_w + (size_t)(start_col - dest_x);
        unsigned char* out = dst + (size_t)row * width_byte + (size_t)start_col * (size_t)bpp;
        if (bpp == 1) {
            memcpy(out, src, (size_t)(end_col - start_col));
        } else {
            int n = end_col - start_col;
            for (; n > 0; n--, src++, out += bpp) {
                const unsigned char c = *src;
                out[0] = c;
                out[1] = c;
                out[2] = c;
                out[3] = c;
            }
        }
    }
}
