/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

/* TrueType through stb_truetype, configured as ImGui configured it (imgui_draw.cpp), so that
 * glyphs come out as ImGui made them, to the texel. The fonts are read in place. stb's memory
 * comes from an arena had at the start and emptied before each glyph: rendering allocates
 * nothing; a glyph needing more than the arena holds comes out empty. */

#include <math.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "ovl_internal.h"

#define ARENA_SIZE (1024u * 1024u)
#define MAX_TTF_FACES 32

static struct {
    unsigned char* mem;
    size_t used;
} arena;

static void* arena_alloc(size_t size) {
    void* p;
    size = (size + 15u) & ~(size_t)15u;
    if (!arena.mem || size > ARENA_SIZE - arena.used)
        return NULL;
    p = arena.mem + arena.used;
    arena.used += size;
    return p;
}

static float ttf_sqrt(float x) {
    return (float)sqrt(x);
}

static float ttf_fabs(float x) {
    return (float)fabs(x);
}

static float ttf_fmod(float x, float y) {
    return (float)fmod(x, y);
}

static float ttf_pow(float x, float y) {
    return (float)pow(x, y);
}

/* ImGui's ImFloor */
static float ttf_floor(float f) {
    return (float)((f >= 0 || (float)(int)f == f) ? (int)f : (int)f - 1);
}

#define STBTT_malloc(x, u) ((void)(u), arena_alloc(x))
#define STBTT_free(x, u) ((void)(u), (void)(x))
#define STBTT_assert(x) ((void)0)
#define STBTT_fmod(x, y) ttf_fmod(x, y)
#define STBTT_sqrt(x) ttf_sqrt(x)
#define STBTT_pow(x, y) ttf_pow(x, y)
#define STBTT_fabs(x) ttf_fabs(x)
#define STBTT_ifloor(x) ((int)ttf_floor(x))
#define STBTT_iceil(x) ((int)(float)ceil(x))
#if defined(__GNUC__)
#pragma GCC diagnostic ignored "-Wunused-function"
#endif
#define STBTT_STATIC
#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

static stbtt_fontinfo faces[MAX_TTF_FACES];
static int face_count;

int ovl_ttf_init(void) {
    if (arena.mem)
        return 1;
    arena.mem = (unsigned char*)malloc(ARENA_SIZE);
    return arena.mem != NULL;
}

void ovl_ttf_free(void) {
    free(arena.mem);
    arena.mem = NULL;
    arena.used = 0;
    face_count = 0;
}

int ovl_ttf_open(const uint8_t* data, size_t size, int index) {
    int offset;
    (void)size;
    if (face_count == MAX_TTF_FACES)
        return -1;
    offset = stbtt_GetFontOffsetForIndex(data, index);
    if (offset < 0 || !stbtt_InitFont(&faces[face_count], data, offset))
        return -1;
    return face_count++;
}

void ovl_ttf_vmetrics(int face, int* ascent, int* descent, int* line_gap) {
    stbtt_GetFontVMetrics(&faces[face], ascent, descent, line_gap);
}

float ovl_ttf_scale_for_pixel_height(int face, float pixels) {
    return stbtt_ScaleForPixelHeight(&faces[face], pixels);
}

int ovl_ttf_find_glyph(int face, uint32_t cp) {
    return stbtt_FindGlyphIndex(&faces[face], (int)cp);
}

void ovl_ttf_hmetrics(int face, int glyph, int* advance, int* lsb) {
    stbtt_GetGlyphHMetrics(&faces[face], glyph, advance, lsb);
}

void ovl_ttf_bitmap_box(int face, int glyph, float sx, float sy, int* x0, int* y0, int* x1,
                        int* y1) {
    stbtt_GetGlyphBitmapBoxSubpixel(&faces[face], glyph, sx, sy, 0.0f, 0.0f, x0, y0, x1, y1);
}

void ovl_ttf_render(int face, int glyph, uint8_t* out, int w, int h, int stride, float sx, float sy,
                    int oversample_h, float* sub_x) {
    float sub_y;
    arena.used = 0;
    memset(out, 0, (size_t)stride * (size_t)h);
    stbtt_MakeGlyphBitmapSubpixelPrefilter(&faces[face], out, w, h, stride, sx, sy, 0.0f, 0.0f,
                                           oversample_h, 1, sub_x, &sub_y, glyph);
}
