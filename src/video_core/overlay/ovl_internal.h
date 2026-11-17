/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

/* What the overlay's parts share (ovl.h) */

#ifndef SHADPS4_OVL_INTERNAL_H
#define SHADPS4_OVL_INTERNAL_H

#include "ovl.h"

#ifdef __cplusplus
extern "C" {
#endif

/* A growing array: kept from frame to frame, grown when a frame needs more */
typedef struct ovl_buf {
    void* data;
    size_t size;     /* in elements */
    size_t capacity; /* in elements */
    size_t elem;     /* bytes an element */
} ovl_buf_t;

void ovl_buf_init(ovl_buf_t* b, size_t elem);
void ovl_buf_free(ovl_buf_t* b);
/* Room for @count more; 0 when there is no memory */
int ovl_buf_reserve(ovl_buf_t* b, size_t count);
/* @count more elements at the end, uninitialized; null when there is no memory */
void* ovl_buf_grow(ovl_buf_t* b, size_t count);

struct ovl_draw_list {
    ovl_buf_t vtx;  /* ovl_vertex_t */
    ovl_buf_t idx;  /* uint32_t */
    ovl_buf_t cmds; /* ovl_cmd_t */
    ovl_buf_t clip; /* float[4] each */
    ovl_buf_t path; /* ovl_vec2_t */
    ovl_buf_t temp; /* ovl_vec2_t */
    ovl_tex_t tex;
    float fringe;
};

/* The widths of line baked into the atlas, as ImGui's IM_DRAWLIST_TEX_LINES_WIDTH_MAX */
#define OVL_TEX_LINES_MAX 32

/* What every draw list reads: the white texel, the baked lines, the circle tables */
typedef struct ovl_draw_shared {
    ovl_vec2_t white_uv;
    ovl_vec4_t tex_uv_lines[OVL_TEX_LINES_MAX + 1]; /* u0, v, u1, v of each width's row */
    ovl_vec2_t arc_fast[48];
    unsigned char circle_segments[64];
    float circle_max_error;
    float clip_full[4];
} ovl_draw_shared_t;

extern ovl_draw_shared_t ovl_shared;

void ovl_draw_shared_init(float white_u, float white_v);
void ovl_draw_list_init(ovl_draw_list_t* dl);
void ovl_draw_list_free(ovl_draw_list_t* dl);
/* Empties @dl for a new frame, clipped to the display */
void ovl_draw_list_reset(ovl_draw_list_t* dl, float display_w, float display_h);
void ovl_draw_list_set_texture(ovl_draw_list_t* dl, ovl_tex_t tex);
const float* ovl_draw_list_clip(const ovl_draw_list_t* dl);

/* Path building, as ImGui's Path* */
void ovl_path_clear(ovl_draw_list_t* dl);
void ovl_path_line_to(ovl_draw_list_t* dl, ovl_vec2_t p);
void ovl_path_arc_to_fast(ovl_draw_list_t* dl, ovl_vec2_t center, float radius, int a_min_of_12,
                          int a_max_of_12);
void ovl_path_rect(ovl_draw_list_t* dl, ovl_vec2_t a, ovl_vec2_t b, float rounding, unsigned flags);
void ovl_path_fill_convex(ovl_draw_list_t* dl, ovl_col_t col);
void ovl_path_stroke(ovl_draw_list_t* dl, ovl_col_t col, unsigned flags, float thickness);

/* Room for @idx_count indices and @vtx_count vertices; the first new vertex's index, or -1 */
long ovl_prim_reserve(ovl_draw_list_t* dl, size_t idx_count, size_t vtx_count, ovl_vertex_t** vtx,
                      uint32_t** idx);
void ovl_prim_rect_uv(ovl_draw_list_t* dl, ovl_vec2_t a, ovl_vec2_t c, ovl_vec2_t uv_a,
                      ovl_vec2_t uv_c, ovl_col_t col);

/* Fonts: drawing a glyph run into a draw list at a scale */
float ovl_font_line_height(const ovl_font_t* font, float size);

/* The atlas, made with the first font (ovl_font.c) */
int ovl_atlas_init(void);
void ovl_atlas_free(void);
/* Starts the atlas again when the frame before filled it */
void ovl_atlas_begin_frame(void);

/* TrueType (ovl_ttf.c): stb_truetype as ImGui uses it, faces read in place */
int ovl_ttf_init(void);
void ovl_ttf_free(void);
/* A handle for face @index of @data, or -1 */
int ovl_ttf_open(const uint8_t* data, size_t size, int index);
void ovl_ttf_vmetrics(int face, int* ascent, int* descent, int* line_gap);
float ovl_ttf_scale_for_pixel_height(int face, float pixels);
int ovl_ttf_find_glyph(int face, uint32_t cp);
void ovl_ttf_hmetrics(int face, int glyph, int* advance, int* lsb);
void ovl_ttf_bitmap_box(int face, int glyph, float sx, float sy, int* x0, int* y0, int* x1,
                        int* y1);
/* Renders @glyph into @out (@w x @h), oversampled @oversample_h times across and box-filtered
 * as stbtt_MakeGlyphBitmapSubpixelPrefilter; @sub_x the shift to place it by */
void ovl_ttf_render(int face, int glyph, uint8_t* out, int w, int h, int stride, float sx, float sy,
                    int oversample_h, float* sub_x);

#ifdef __cplusplus
}
#endif

#endif
