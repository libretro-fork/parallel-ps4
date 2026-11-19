/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

/* A rendered glyph into a game's surface (libSceFont's RenderCharGlyphImage and its kin). */

#ifndef SHADPS4_FONT_BLIT_H
#define SHADPS4_FONT_BLIT_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* The glyph's coverage @cov, @glyph_w bytes a row, has its top left at (@dest_x, @dest_y) of
 * the surface @dst (@width_byte bytes a row, @bpp bytes a pixel: 1, or 4 and the coverage in
 * each). The surface's pixels of rows @start_row up to @end_row and columns @start_col up to
 * @end_col are written, all of them within the glyph: the caller has cut the glyph's rectangle
 * to the surface's. Read straight from @cov, the face's own bitmap: no copy of it is made. */
void font_glyph_blit(unsigned char* dst, size_t width_byte, int bpp, const unsigned char* cov,
                     int glyph_w, int dest_x, int dest_y, int start_row, int end_row, int start_col,
                     int end_col);

#ifdef __cplusplus
}
#endif

#endif
