/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

/* Draw lists, with ImGui's shapes made as ImGui makes them (imgui_draw.cpp): the same paths,
 * the same anti-aliased fringes, so that they look the same */

#include <math.h>
#if defined(__SSE__) || defined(__x86_64__) || defined(_M_X64) ||                                  \
    (defined(_M_IX86_FP) && _M_IX86_FP >= 1)
#include <xmmintrin.h>
#endif
#include <stdlib.h>
#include <string.h>

#include "ovl_internal.h"

#define OVL_PI 3.14159265358979323846f
#define ARC_FAST_SAMPLES 48
#define CIRCLE_SEGMENTS_MIN 4
#define CIRCLE_SEGMENTS_MAX 512

ovl_draw_shared_t ovl_shared;

/* ------------------------------------------------------------------------------------------ */

void ovl_buf_init(ovl_buf_t* b, size_t elem) {
    b->data = NULL;
    b->size = 0;
    b->capacity = 0;
    b->elem = elem;
}

void ovl_buf_free(ovl_buf_t* b) {
    free(b->data);
    b->data = NULL;
    b->size = b->capacity = 0;
}

int ovl_buf_reserve(ovl_buf_t* b, size_t count) {
    size_t want = b->size + count, cap;
    void* data;
    if (want <= b->capacity)
        return 1;
    cap = b->capacity ? b->capacity : 64;
    while (cap < want)
        cap *= 2;
    data = realloc(b->data, cap * b->elem);
    if (!data)
        return 0;
    b->data = data;
    b->capacity = cap;
    return 1;
}

void* ovl_buf_grow(ovl_buf_t* b, size_t count) {
    void* at;
    if (!ovl_buf_reserve(b, count))
        return NULL;
    at = (char*)b->data + b->size * b->elem;
    b->size += count;
    return at;
}

/* ------------------------------------------------------------------------------------------ */

static int circle_segment_count(float radius, float max_error) {
    int n;
    float r = radius;
    if (r <= 0.0f)
        return ARC_FAST_SAMPLES;
    n = (int)(float)ceil(OVL_PI / (float)acos(1.0f - (max_error < r ? max_error : r) / r));
    n = (n + 1) / 2 * 2;
    if (n < CIRCLE_SEGMENTS_MIN)
        n = CIRCLE_SEGMENTS_MIN;
    if (n > CIRCLE_SEGMENTS_MAX)
        n = CIRCLE_SEGMENTS_MAX;
    return n;
}

void ovl_draw_shared_init(float white_u, float white_v) {
    int i;
    ovl_shared.white_uv.x = white_u;
    ovl_shared.white_uv.y = white_v;
    for (i = 0; i < ARC_FAST_SAMPLES; i++) {
        const float a = ((float)i * 2.0f * OVL_PI) / (float)ARC_FAST_SAMPLES;
        ovl_shared.arc_fast[i].x = (float)cos(a);
        ovl_shared.arc_fast[i].y = (float)sin(a);
    }
    /* ImGui's style.CircleTessellationMaxError */
    ovl_shared.circle_max_error = 0.30f;
    for (i = 0; i < 64; i++)
        ovl_shared.circle_segments[i] =
            (unsigned char)(i > 0 ? circle_segment_count((float)i, ovl_shared.circle_max_error)
                                  : ARC_FAST_SAMPLES);
}

static int calc_circle_segments(float radius) {
    const int idx = (int)(radius + 0.999f);
    if (idx >= 0 && idx < 64)
        return ovl_shared.circle_segments[idx];
    return circle_segment_count(radius, ovl_shared.circle_max_error);
}

void ovl_draw_list_init(ovl_draw_list_t* dl) {
    ovl_buf_init(&dl->vtx, sizeof(ovl_vertex_t));
    ovl_buf_init(&dl->idx, sizeof(uint32_t));
    ovl_buf_init(&dl->cmds, sizeof(ovl_cmd_t));
    ovl_buf_init(&dl->clip, sizeof(float) * 4);
    ovl_buf_init(&dl->path, sizeof(ovl_vec2_t));
    ovl_buf_init(&dl->temp, sizeof(ovl_vec2_t));
    dl->tex = OVL_TEX_ATLAS;
    dl->fringe = 1.0f;
}

void ovl_draw_list_free(ovl_draw_list_t* dl) {
    ovl_buf_free(&dl->vtx);
    ovl_buf_free(&dl->idx);
    ovl_buf_free(&dl->cmds);
    ovl_buf_free(&dl->clip);
    ovl_buf_free(&dl->path);
    ovl_buf_free(&dl->temp);
}

const float* ovl_draw_list_clip(const ovl_draw_list_t* dl) {
    if (dl->clip.size == 0)
        return ovl_shared.clip_full;
    return (const float*)dl->clip.data + (dl->clip.size - 1) * 4;
}

/* The command new triangles go into: the last one when it has the same texture and clip */
static ovl_cmd_t* current_cmd(ovl_draw_list_t* dl) {
    ovl_cmd_t* cmd = dl->cmds.size ? (ovl_cmd_t*)dl->cmds.data + dl->cmds.size - 1 : NULL;
    const float* clip = ovl_draw_list_clip(dl);
    if (cmd && cmd->tex == dl->tex && memcmp(cmd->clip, clip, sizeof(cmd->clip)) == 0)
        return cmd;
    if (cmd && cmd->idx_count == 0) {
        /* An empty one is changed rather than left behind */
        cmd->tex = dl->tex;
        memcpy(cmd->clip, clip, sizeof(cmd->clip));
        return cmd;
    }
    cmd = (ovl_cmd_t*)ovl_buf_grow(&dl->cmds, 1);
    if (!cmd)
        return NULL;
    cmd->tex = dl->tex;
    memcpy(cmd->clip, clip, sizeof(cmd->clip));
    cmd->idx_offset = (uint32_t)dl->idx.size;
    cmd->idx_count = 0;
    return cmd;
}

void ovl_draw_list_reset(ovl_draw_list_t* dl, float display_w, float display_h) {
    dl->vtx.size = dl->idx.size = dl->cmds.size = dl->clip.size = dl->path.size = 0;
    dl->tex = OVL_TEX_ATLAS;
    ovl_shared.clip_full[0] = 0.0f;
    ovl_shared.clip_full[1] = 0.0f;
    ovl_shared.clip_full[2] = display_w;
    ovl_shared.clip_full[3] = display_h;
}

void ovl_draw_list_set_texture(ovl_draw_list_t* dl, ovl_tex_t tex) {
    dl->tex = tex;
}

void ovl_push_clip(ovl_draw_list_t* dl, ovl_vec2_t min, ovl_vec2_t max, int intersect) {
    float r[4], *at;
    const float* cur = ovl_draw_list_clip(dl);
    r[0] = min.x;
    r[1] = min.y;
    r[2] = max.x;
    r[3] = max.y;
    if (intersect) {
        if (r[0] < cur[0])
            r[0] = cur[0];
        if (r[1] < cur[1])
            r[1] = cur[1];
        if (r[2] > cur[2])
            r[2] = cur[2];
        if (r[3] > cur[3])
            r[3] = cur[3];
    }
    if (r[2] < r[0])
        r[2] = r[0];
    if (r[3] < r[1])
        r[3] = r[1];
    at = (float*)ovl_buf_grow(&dl->clip, 1);
    if (at)
        memcpy(at, r, sizeof(r));
}

void ovl_pop_clip(ovl_draw_list_t* dl) {
    if (dl->clip.size)
        dl->clip.size--;
}

long ovl_prim_reserve(ovl_draw_list_t* dl, size_t idx_count, size_t vtx_count, ovl_vertex_t** vtx,
                      uint32_t** idx) {
    ovl_cmd_t* cmd = current_cmd(dl);
    long base;
    if (!cmd || !ovl_buf_reserve(&dl->vtx, vtx_count) || !ovl_buf_reserve(&dl->idx, idx_count))
        return -1;
    base = (long)dl->vtx.size;
    *vtx = (ovl_vertex_t*)ovl_buf_grow(&dl->vtx, vtx_count);
    *idx = (uint32_t*)ovl_buf_grow(&dl->idx, idx_count);
    cmd->idx_count += (uint32_t)idx_count;
    return base;
}

static void put_vtx(ovl_vertex_t* v, float x, float y, ovl_vec2_t uv, ovl_col_t col) {
    v->x = x;
    v->y = y;
    v->u = uv.x;
    v->v = uv.y;
    v->col = col;
}

void ovl_prim_rect_uv(ovl_draw_list_t* dl, ovl_vec2_t a, ovl_vec2_t c, ovl_vec2_t uv_a,
                      ovl_vec2_t uv_c, ovl_col_t col) {
    ovl_vertex_t* v;
    uint32_t* i;
    ovl_vec2_t uv_b, uv_d;
    const long base = ovl_prim_reserve(dl, 6, 4, &v, &i);
    if (base < 0)
        return;
    uv_b.x = uv_c.x;
    uv_b.y = uv_a.y;
    uv_d.x = uv_a.x;
    uv_d.y = uv_c.y;
    i[0] = (uint32_t)base;
    i[1] = (uint32_t)base + 1;
    i[2] = (uint32_t)base + 2;
    i[3] = (uint32_t)base;
    i[4] = (uint32_t)base + 2;
    i[5] = (uint32_t)base + 3;
    put_vtx(&v[0], a.x, a.y, uv_a, col);
    put_vtx(&v[1], c.x, a.y, uv_b, col);
    put_vtx(&v[2], c.x, c.y, uv_c, col);
    put_vtx(&v[3], a.x, c.y, uv_d, col);
}

static void prim_rect(ovl_draw_list_t* dl, ovl_vec2_t a, ovl_vec2_t c, ovl_col_t col) {
    ovl_prim_rect_uv(dl, a, c, ovl_shared.white_uv, ovl_shared.white_uv, col);
}

/* ImGui's IM_NORMALIZE2F_OVER_ZERO and IM_FIXNORMAL2F */
/* ImGui's ImRsqrt: SSE's approximation where ImGui uses it, so that the shapes come out the
 * same to the bit */
static float rsqrt(float x) {
#if defined(__SSE__) || defined(__x86_64__) || defined(_M_X64) ||                                  \
    (defined(_M_IX86_FP) && _M_IX86_FP >= 1)
    return _mm_cvtss_f32(_mm_rsqrt_ss(_mm_set_ss(x)));
#else
    return 1.0f / (float)sqrt(x);
#endif
}

static void normalize_over_zero(float* x, float* y) {
    const float d2 = *x * *x + *y * *y;
    if (d2 > 0.0f) {
        const float inv = rsqrt(d2);
        *x *= inv;
        *y *= inv;
    }
}

static void fix_normal(float* x, float* y) {
    float d2 = *x * *x + *y * *y;
    if (d2 > 0.000001f) {
        float inv = 1.0f / d2;
        if (inv > 100.0f)
            inv = 100.0f;
        *x *= inv;
        *y *= inv;
    }
}

/* ------------------------------------------------------------------------------------------ */
/* Lines and fills, ImGui's anti-aliased geometry (AddPolyline without the baked line texture,
 * AddConvexPolyFilled) */

void ovl_add_polyline(ovl_draw_list_t* dl, const ovl_vec2_t* points, int points_count,
                      ovl_col_t col, unsigned flags, float thickness) {
    const int closed = (flags & OVL_CLOSED) != 0;
    const ovl_vec2_t uv = ovl_shared.white_uv;
    const float aa = dl->fringe;
    const ovl_col_t col_trans = col & 0x00FFFFFFu;
    int count, thick, use_tex, i, i1, idx_count, vtx_count, integer_thickness;
    ovl_vec2_t *normals, *tpoints;
    ovl_vertex_t* v;
    uint32_t* ix;
    long base;
    unsigned idx1;
    if (points_count < 2 || (col & 0xFF000000u) == 0)
        return;
    count = closed ? points_count : points_count - 1;
    thick = thickness > aa;
    if (thickness < 1.0f)
        thickness = 1.0f;
    /* Whole-pixel widths are drawn as ImGui draws them: two vertices a point, sampling the
     * atlas's baked line of that width for the anti-aliasing */
    integer_thickness = (int)thickness;
    use_tex = integer_thickness < OVL_TEX_LINES_MAX &&
              thickness - (float)integer_thickness <= 0.00001f && aa == 1.0f;
    idx_count = use_tex ? count * 6 : thick ? count * 18 : count * 12;
    vtx_count = use_tex ? points_count * 2 : thick ? points_count * 4 : points_count * 3;
    dl->temp.size = 0;
    normals =
        (ovl_vec2_t*)ovl_buf_grow(&dl->temp, (size_t)points_count * ((use_tex || !thick) ? 3 : 5));
    if (!normals)
        return;
    tpoints = normals + points_count;
    base = ovl_prim_reserve(dl, (size_t)idx_count, (size_t)vtx_count, &v, &ix);
    if (base < 0)
        return;
    for (i1 = 0; i1 < count; i1++) {
        const int i2 = (i1 + 1) == points_count ? 0 : i1 + 1;
        float dx = points[i2].x - points[i1].x, dy = points[i2].y - points[i1].y;
        normalize_over_zero(&dx, &dy);
        normals[i1].x = dy;
        normals[i1].y = -dx;
    }
    if (!closed)
        normals[points_count - 1] = normals[points_count - 2];

    if (use_tex || !thick) {
        const float half = use_tex ? thickness * 0.5f + 1.0f : aa;
        const unsigned step = use_tex ? 2u : 3u;
        if (!closed) {
            tpoints[0].x = points[0].x + normals[0].x * half;
            tpoints[0].y = points[0].y + normals[0].y * half;
            tpoints[1].x = points[0].x - normals[0].x * half;
            tpoints[1].y = points[0].y - normals[0].y * half;
            tpoints[(points_count - 1) * 2 + 0].x =
                points[points_count - 1].x + normals[points_count - 1].x * half;
            tpoints[(points_count - 1) * 2 + 0].y =
                points[points_count - 1].y + normals[points_count - 1].y * half;
            tpoints[(points_count - 1) * 2 + 1].x =
                points[points_count - 1].x - normals[points_count - 1].x * half;
            tpoints[(points_count - 1) * 2 + 1].y =
                points[points_count - 1].y - normals[points_count - 1].y * half;
        }
        idx1 = (unsigned)base;
        for (i1 = 0; i1 < count; i1++) {
            const int i2 = (i1 + 1) == points_count ? 0 : i1 + 1;
            const unsigned idx2 = (i1 + 1) == points_count ? (unsigned)base : idx1 + step;
            float dm_x = (normals[i1].x + normals[i2].x) * 0.5f;
            float dm_y = (normals[i1].y + normals[i2].y) * 0.5f;
            fix_normal(&dm_x, &dm_y);
            dm_x *= half;
            dm_y *= half;
            tpoints[i2 * 2 + 0].x = points[i2].x + dm_x;
            tpoints[i2 * 2 + 0].y = points[i2].y + dm_y;
            tpoints[i2 * 2 + 1].x = points[i2].x - dm_x;
            tpoints[i2 * 2 + 1].y = points[i2].y - dm_y;
            if (use_tex) {
                ix[0] = idx2 + 0;
                ix[1] = idx1 + 0;
                ix[2] = idx1 + 1;
                ix[3] = idx2 + 1;
                ix[4] = idx1 + 1;
                ix[5] = idx2 + 0;
                ix += 6;
                idx1 = idx2;
                continue;
            }
            ix[0] = idx2 + 0;
            ix[1] = idx1 + 0;
            ix[2] = idx1 + 2;
            ix[3] = idx1 + 2;
            ix[4] = idx2 + 2;
            ix[5] = idx2 + 0;
            ix[6] = idx2 + 1;
            ix[7] = idx1 + 1;
            ix[8] = idx1 + 0;
            ix[9] = idx1 + 0;
            ix[10] = idx2 + 0;
            ix[11] = idx2 + 1;
            ix += 12;
            idx1 = idx2;
        }
        if (use_tex) {
            const ovl_vec4_t t = ovl_shared.tex_uv_lines[integer_thickness];
            ovl_vec2_t uv0, uv1;
            uv0.x = t.x;
            uv0.y = t.y;
            uv1.x = t.z;
            uv1.y = t.w;
            for (i = 0; i < points_count; i++) {
                put_vtx(&v[0], tpoints[i * 2 + 0].x, tpoints[i * 2 + 0].y, uv0, col);
                put_vtx(&v[1], tpoints[i * 2 + 1].x, tpoints[i * 2 + 1].y, uv1, col);
                v += 2;
            }
        } else
            for (i = 0; i < points_count; i++) {
                put_vtx(&v[0], points[i].x, points[i].y, uv, col);
                put_vtx(&v[1], tpoints[i * 2 + 0].x, tpoints[i * 2 + 0].y, uv, col_trans);
                put_vtx(&v[2], tpoints[i * 2 + 1].x, tpoints[i * 2 + 1].y, uv, col_trans);
                v += 3;
            }
    } else {
        const float half_inner = (thickness - aa) * 0.5f;
        if (!closed) {
            const int last = points_count - 1;
            const float o = half_inner + aa;
            tpoints[0].x = points[0].x + normals[0].x * o;
            tpoints[0].y = points[0].y + normals[0].y * o;
            tpoints[1].x = points[0].x + normals[0].x * half_inner;
            tpoints[1].y = points[0].y + normals[0].y * half_inner;
            tpoints[2].x = points[0].x - normals[0].x * half_inner;
            tpoints[2].y = points[0].y - normals[0].y * half_inner;
            tpoints[3].x = points[0].x - normals[0].x * o;
            tpoints[3].y = points[0].y - normals[0].y * o;
            tpoints[last * 4 + 0].x = points[last].x + normals[last].x * o;
            tpoints[last * 4 + 0].y = points[last].y + normals[last].y * o;
            tpoints[last * 4 + 1].x = points[last].x + normals[last].x * half_inner;
            tpoints[last * 4 + 1].y = points[last].y + normals[last].y * half_inner;
            tpoints[last * 4 + 2].x = points[last].x - normals[last].x * half_inner;
            tpoints[last * 4 + 2].y = points[last].y - normals[last].y * half_inner;
            tpoints[last * 4 + 3].x = points[last].x - normals[last].x * o;
            tpoints[last * 4 + 3].y = points[last].y - normals[last].y * o;
        }
        idx1 = (unsigned)base;
        for (i1 = 0; i1 < count; i1++) {
            const int i2 = (i1 + 1) == points_count ? 0 : i1 + 1;
            const unsigned idx2 = (i1 + 1) == points_count ? (unsigned)base : idx1 + 4;
            float dm_x = (normals[i1].x + normals[i2].x) * 0.5f;
            float dm_y = (normals[i1].y + normals[i2].y) * 0.5f;
            float out_x, out_y, in_x, in_y;
            fix_normal(&dm_x, &dm_y);
            out_x = dm_x * (half_inner + aa);
            out_y = dm_y * (half_inner + aa);
            in_x = dm_x * half_inner;
            in_y = dm_y * half_inner;
            tpoints[i2 * 4 + 0].x = points[i2].x + out_x;
            tpoints[i2 * 4 + 0].y = points[i2].y + out_y;
            tpoints[i2 * 4 + 1].x = points[i2].x + in_x;
            tpoints[i2 * 4 + 1].y = points[i2].y + in_y;
            tpoints[i2 * 4 + 2].x = points[i2].x - in_x;
            tpoints[i2 * 4 + 2].y = points[i2].y - in_y;
            tpoints[i2 * 4 + 3].x = points[i2].x - out_x;
            tpoints[i2 * 4 + 3].y = points[i2].y - out_y;
            ix[0] = idx2 + 1;
            ix[1] = idx1 + 1;
            ix[2] = idx1 + 2;
            ix[3] = idx1 + 2;
            ix[4] = idx2 + 2;
            ix[5] = idx2 + 1;
            ix[6] = idx2 + 1;
            ix[7] = idx1 + 1;
            ix[8] = idx1 + 0;
            ix[9] = idx1 + 0;
            ix[10] = idx2 + 0;
            ix[11] = idx2 + 1;
            ix[12] = idx2 + 2;
            ix[13] = idx1 + 2;
            ix[14] = idx1 + 3;
            ix[15] = idx1 + 3;
            ix[16] = idx2 + 3;
            ix[17] = idx2 + 2;
            ix += 18;
            idx1 = idx2;
        }
        for (i = 0; i < points_count; i++) {
            put_vtx(&v[0], tpoints[i * 4 + 0].x, tpoints[i * 4 + 0].y, uv, col_trans);
            put_vtx(&v[1], tpoints[i * 4 + 1].x, tpoints[i * 4 + 1].y, uv, col);
            put_vtx(&v[2], tpoints[i * 4 + 2].x, tpoints[i * 4 + 2].y, uv, col);
            put_vtx(&v[3], tpoints[i * 4 + 3].x, tpoints[i * 4 + 3].y, uv, col_trans);
            v += 4;
        }
    }
}

void ovl_add_convex_poly_filled(ovl_draw_list_t* dl, const ovl_vec2_t* points, int points_count,
                                ovl_col_t col) {
    const ovl_vec2_t uv = ovl_shared.white_uv;
    const float aa = dl->fringe;
    const ovl_col_t col_trans = col & 0x00FFFFFFu;
    ovl_vec2_t* normals;
    ovl_vertex_t* v;
    uint32_t* ix;
    long base;
    unsigned inner, outer;
    int i, i0, i1;
    if (points_count < 3 || (col & 0xFF000000u) == 0)
        return;
    dl->temp.size = 0;
    normals = (ovl_vec2_t*)ovl_buf_grow(&dl->temp, (size_t)points_count);
    if (!normals)
        return;
    base = ovl_prim_reserve(dl, (size_t)(points_count - 2) * 3 + (size_t)points_count * 6,
                            (size_t)points_count * 2, &v, &ix);
    if (base < 0)
        return;
    inner = (unsigned)base;
    outer = (unsigned)base + 1;
    for (i = 2; i < points_count; i++) {
        ix[0] = inner;
        ix[1] = inner + (unsigned)((i - 1) << 1);
        ix[2] = inner + (unsigned)(i << 1);
        ix += 3;
    }
    for (i0 = points_count - 1, i1 = 0; i1 < points_count; i0 = i1++) {
        float dx = points[i1].x - points[i0].x, dy = points[i1].y - points[i0].y;
        normalize_over_zero(&dx, &dy);
        normals[i0].x = dy;
        normals[i0].y = -dx;
    }
    for (i0 = points_count - 1, i1 = 0; i1 < points_count; i0 = i1++) {
        float dm_x = (normals[i0].x + normals[i1].x) * 0.5f;
        float dm_y = (normals[i0].y + normals[i1].y) * 0.5f;
        fix_normal(&dm_x, &dm_y);
        dm_x *= aa * 0.5f;
        dm_y *= aa * 0.5f;
        put_vtx(&v[0], points[i1].x - dm_x, points[i1].y - dm_y, uv, col);
        put_vtx(&v[1], points[i1].x + dm_x, points[i1].y + dm_y, uv, col_trans);
        v += 2;
        ix[0] = inner + (unsigned)(i1 << 1);
        ix[1] = inner + (unsigned)(i0 << 1);
        ix[2] = outer + (unsigned)(i0 << 1);
        ix[3] = outer + (unsigned)(i0 << 1);
        ix[4] = outer + (unsigned)(i1 << 1);
        ix[5] = inner + (unsigned)(i1 << 1);
        ix += 6;
    }
}

/* ------------------------------------------------------------------------------------------ */
/* Paths */

void ovl_path_clear(ovl_draw_list_t* dl) {
    dl->path.size = 0;
}

void ovl_path_line_to(ovl_draw_list_t* dl, ovl_vec2_t p) {
    ovl_vec2_t* at = (ovl_vec2_t*)ovl_buf_grow(&dl->path, 1);
    if (at)
        *at = p;
}

static void path_arc_fast_ex(ovl_draw_list_t* dl, ovl_vec2_t center, float radius, int a_min,
                             int a_max, int a_step) {
    int range, next_step, samples, extra = 0, idx, a;
    ovl_vec2_t* out;
    if (radius < 0.5f) {
        ovl_path_line_to(dl, center);
        return;
    }
    if (a_step <= 0)
        a_step = ARC_FAST_SAMPLES / calc_circle_segments(radius / dl->fringe);
    if (a_step < 1)
        a_step = 1;
    if (a_step > ARC_FAST_SAMPLES / 4)
        a_step = ARC_FAST_SAMPLES / 4;
    range = a_max > a_min ? a_max - a_min : a_min - a_max;
    next_step = a_step;
    samples = range + 1;
    if (a_step > 1) {
        const int overstep = range % a_step;
        samples = range / a_step + 1;
        if (overstep > 0) {
            extra = 1;
            samples++;
            if (range > 0)
                a_step -= (a_step - overstep) / 2;
        }
    }
    out = (ovl_vec2_t*)ovl_buf_grow(&dl->path, (size_t)samples);
    if (!out)
        return;
    idx = a_min;
    if (idx < 0 || idx >= ARC_FAST_SAMPLES) {
        idx %= ARC_FAST_SAMPLES;
        if (idx < 0)
            idx += ARC_FAST_SAMPLES;
    }
    if (a_max >= a_min) {
        for (a = a_min; a <= a_max; a += a_step, idx += a_step, a_step = next_step) {
            if (idx >= ARC_FAST_SAMPLES)
                idx -= ARC_FAST_SAMPLES;
            out->x = center.x + ovl_shared.arc_fast[idx].x * radius;
            out->y = center.y + ovl_shared.arc_fast[idx].y * radius;
            out++;
        }
    } else {
        for (a = a_min; a >= a_max; a -= a_step, idx -= a_step, a_step = next_step) {
            if (idx < 0)
                idx += ARC_FAST_SAMPLES;
            out->x = center.x + ovl_shared.arc_fast[idx].x * radius;
            out->y = center.y + ovl_shared.arc_fast[idx].y * radius;
            out++;
        }
    }
    if (extra) {
        int m = a_max % ARC_FAST_SAMPLES;
        if (m < 0)
            m += ARC_FAST_SAMPLES;
        out->x = center.x + ovl_shared.arc_fast[m].x * radius;
        out->y = center.y + ovl_shared.arc_fast[m].y * radius;
    }
}

void ovl_path_arc_to_fast(ovl_draw_list_t* dl, ovl_vec2_t center, float radius, int a_min_of_12,
                          int a_max_of_12) {
    if (radius < 0.5f) {
        ovl_path_line_to(dl, center);
        return;
    }
    path_arc_fast_ex(dl, center, radius, a_min_of_12 * ARC_FAST_SAMPLES / 12,
                     a_max_of_12 * ARC_FAST_SAMPLES / 12, 0);
}

void ovl_path_rect(ovl_draw_list_t* dl, ovl_vec2_t a, ovl_vec2_t b, float rounding,
                   unsigned flags) {
    if (rounding >= 0.5f) {
        float w = (float)fabs(b.x - a.x), h = (float)fabs(b.y - a.y);
        const unsigned top = OVL_CORNERS_TL | OVL_CORNERS_TR,
                       bottom = OVL_CORNERS_BL | OVL_CORNERS_BR;
        const unsigned left = OVL_CORNERS_TL | OVL_CORNERS_BL,
                       right = OVL_CORNERS_TR | OVL_CORNERS_BR;
        if ((flags & (OVL_CORNERS_ALL | OVL_CORNERS_NONE)) == 0)
            flags |= OVL_CORNERS_ALL;
        w = w * (((flags & top) == top || (flags & bottom) == bottom) ? 0.5f : 1.0f) - 1.0f;
        h = h * (((flags & left) == left || (flags & right) == right) ? 0.5f : 1.0f) - 1.0f;
        if (rounding > w)
            rounding = w;
        if (rounding > h)
            rounding = h;
    }
    if (rounding < 0.5f || (flags & (OVL_CORNERS_ALL | OVL_CORNERS_NONE)) == OVL_CORNERS_NONE) {
        ovl_vec2_t p;
        ovl_path_line_to(dl, a);
        p.x = b.x;
        p.y = a.y;
        ovl_path_line_to(dl, p);
        ovl_path_line_to(dl, b);
        p.x = a.x;
        p.y = b.y;
        ovl_path_line_to(dl, p);
    } else {
        const float tl = (flags & OVL_CORNERS_TL) ? rounding : 0.0f;
        const float tr = (flags & OVL_CORNERS_TR) ? rounding : 0.0f;
        const float br = (flags & OVL_CORNERS_BR) ? rounding : 0.0f;
        const float bl = (flags & OVL_CORNERS_BL) ? rounding : 0.0f;
        ovl_vec2_t c;
        c.x = a.x + tl;
        c.y = a.y + tl;
        ovl_path_arc_to_fast(dl, c, tl, 6, 9);
        c.x = b.x - tr;
        c.y = a.y + tr;
        ovl_path_arc_to_fast(dl, c, tr, 9, 12);
        c.x = b.x - br;
        c.y = b.y - br;
        ovl_path_arc_to_fast(dl, c, br, 0, 3);
        c.x = a.x + bl;
        c.y = b.y - bl;
        ovl_path_arc_to_fast(dl, c, bl, 3, 6);
    }
}

void ovl_path_fill_convex(ovl_draw_list_t* dl, ovl_col_t col) {
    ovl_add_convex_poly_filled(dl, (const ovl_vec2_t*)dl->path.data, (int)dl->path.size, col);
    dl->path.size = 0;
}

void ovl_path_stroke(ovl_draw_list_t* dl, ovl_col_t col, unsigned flags, float thickness) {
    ovl_add_polyline(dl, (const ovl_vec2_t*)dl->path.data, (int)dl->path.size, col, flags,
                     thickness);
    dl->path.size = 0;
}

/* ------------------------------------------------------------------------------------------ */
/* Shapes */

void ovl_add_line(ovl_draw_list_t* dl, ovl_vec2_t a, ovl_vec2_t b, ovl_col_t col, float thickness) {
    ovl_vec2_t p[2];
    if ((col & 0xFF000000u) == 0)
        return;
    p[0].x = a.x + 0.5f;
    p[0].y = a.y + 0.5f;
    p[1].x = b.x + 0.5f;
    p[1].y = b.y + 0.5f;
    ovl_add_polyline(dl, p, 2, col, 0, thickness);
}

void ovl_add_rect(ovl_draw_list_t* dl, ovl_vec2_t min, ovl_vec2_t max, ovl_col_t col,
                  float rounding, unsigned flags, float thickness) {
    if ((col & 0xFF000000u) == 0)
        return;
    min.x += 0.5f;
    min.y += 0.5f;
    max.x -= 0.5f;
    max.y -= 0.5f;
    ovl_path_rect(dl, min, max, rounding, flags);
    ovl_path_stroke(dl, col, OVL_CLOSED, thickness);
}

void ovl_add_rect_filled(ovl_draw_list_t* dl, ovl_vec2_t min, ovl_vec2_t max, ovl_col_t col,
                         float rounding, unsigned flags) {
    if ((col & 0xFF000000u) == 0)
        return;
    if (rounding < 0.5f || (flags & (OVL_CORNERS_ALL | OVL_CORNERS_NONE)) == OVL_CORNERS_NONE) {
        prim_rect(dl, min, max, col);
    } else {
        ovl_path_rect(dl, min, max, rounding, flags);
        ovl_path_fill_convex(dl, col);
    }
}

void ovl_add_rect_filled_multicolor(ovl_draw_list_t* dl, ovl_vec2_t min, ovl_vec2_t max,
                                    ovl_col_t tl, ovl_col_t tr, ovl_col_t br, ovl_col_t bl) {
    ovl_vertex_t* v;
    uint32_t* i;
    long base;
    const ovl_vec2_t uv = ovl_shared.white_uv;
    if (((tl | tr | br | bl) & 0xFF000000u) == 0)
        return;
    base = ovl_prim_reserve(dl, 6, 4, &v, &i);
    if (base < 0)
        return;
    i[0] = (uint32_t)base;
    i[1] = (uint32_t)base + 1;
    i[2] = (uint32_t)base + 2;
    i[3] = (uint32_t)base;
    i[4] = (uint32_t)base + 2;
    i[5] = (uint32_t)base + 3;
    put_vtx(&v[0], min.x, min.y, uv, tl);
    put_vtx(&v[1], max.x, min.y, uv, tr);
    put_vtx(&v[2], max.x, max.y, uv, br);
    put_vtx(&v[3], min.x, max.y, uv, bl);
}

static void path_circle(ovl_draw_list_t* dl, ovl_vec2_t center, float radius, int segments) {
    if (segments <= 0) {
        path_arc_fast_ex(dl, center, radius, 0, ARC_FAST_SAMPLES, 0);
        if (dl->path.size)
            dl->path.size--;
    } else {
        int i;
        float a_max;
        if (segments < 3)
            segments = 3;
        if (segments > CIRCLE_SEGMENTS_MAX)
            segments = CIRCLE_SEGMENTS_MAX;
        a_max = (OVL_PI * 2.0f) * ((float)segments - 1.0f) / (float)segments;
        for (i = 0; i <= segments - 1; i++) {
            const float a = ((float)i / (float)(segments - 1)) * a_max;
            ovl_vec2_t p;
            p.x = center.x + (float)cos(a) * radius;
            p.y = center.y + (float)sin(a) * radius;
            ovl_path_line_to(dl, p);
        }
    }
}

void ovl_add_circle(ovl_draw_list_t* dl, ovl_vec2_t center, float radius, ovl_col_t col,
                    int segments, float thickness) {
    if ((col & 0xFF000000u) == 0 || radius < 0.5f)
        return;
    path_circle(dl, center, radius - 0.5f, segments);
    ovl_path_stroke(dl, col, OVL_CLOSED, thickness);
}

void ovl_add_circle_filled(ovl_draw_list_t* dl, ovl_vec2_t center, float radius, ovl_col_t col,
                           int segments) {
    if ((col & 0xFF000000u) == 0 || radius < 0.5f)
        return;
    path_circle(dl, center, radius, segments);
    ovl_path_fill_convex(dl, col);
}

void ovl_add_triangle_filled(ovl_draw_list_t* dl, ovl_vec2_t a, ovl_vec2_t b, ovl_vec2_t c,
                             ovl_col_t col) {
    if ((col & 0xFF000000u) == 0)
        return;
    ovl_path_line_to(dl, a);
    ovl_path_line_to(dl, b);
    ovl_path_line_to(dl, c);
    ovl_path_fill_convex(dl, col);
}

void ovl_add_image(ovl_draw_list_t* dl, ovl_tex_t tex, ovl_vec2_t min, ovl_vec2_t max,
                   ovl_vec2_t uv_min, ovl_vec2_t uv_max, ovl_col_t col) {
    const ovl_tex_t was = dl->tex;
    if ((col & 0xFF000000u) == 0)
        return;
    dl->tex = tex;
    ovl_prim_rect_uv(dl, min, max, uv_min, uv_max, col);
    dl->tex = was;
}

void ovl_add_image_rounded(ovl_draw_list_t* dl, ovl_tex_t tex, ovl_vec2_t min, ovl_vec2_t max,
                           ovl_vec2_t uv_min, ovl_vec2_t uv_max, ovl_col_t col, float rounding,
                           unsigned flags) {
    const ovl_tex_t was = dl->tex;
    size_t first;
    size_t i;
    float sx, sy;
    if ((col & 0xFF000000u) == 0)
        return;
    if (rounding < 0.5f || (flags & (OVL_CORNERS_ALL | OVL_CORNERS_NONE)) == OVL_CORNERS_NONE) {
        ovl_add_image(dl, tex, min, max, uv_min, uv_max, col);
        return;
    }
    dl->tex = tex;
    first = dl->vtx.size;
    ovl_path_rect(dl, min, max, rounding, flags);
    ovl_path_fill_convex(dl, col);
    /* ImGui's ShadeVertsLinearUV: each new vertex's coordinate from its position */
    sx = max.x != min.x ? (uv_max.x - uv_min.x) / (max.x - min.x) : 0.0f;
    sy = max.y != min.y ? (uv_max.y - uv_min.y) / (max.y - min.y) : 0.0f;
    for (i = first; i < dl->vtx.size; i++) {
        ovl_vertex_t* v = (ovl_vertex_t*)dl->vtx.data + i;
        float u = uv_min.x + (v->x - min.x) * sx, w = uv_min.y + (v->y - min.y) * sy;
        const float lo_u = uv_min.x < uv_max.x ? uv_min.x : uv_max.x;
        const float hi_u = uv_min.x < uv_max.x ? uv_max.x : uv_min.x;
        const float lo_v = uv_min.y < uv_max.y ? uv_min.y : uv_max.y;
        const float hi_v = uv_min.y < uv_max.y ? uv_max.y : uv_min.y;
        v->u = u < lo_u ? lo_u : (u > hi_u ? hi_u : u);
        v->v = w < lo_v ? lo_v : (w > hi_v ? hi_v : w);
    }
    dl->tex = was;
}

ovl_col_t ovl_col_from_vec4(ovl_vec4_t c) {
    unsigned r, g, b, a;
    r = (unsigned)((c.x < 0.0f ? 0.0f : (c.x > 1.0f ? 1.0f : c.x)) * 255.0f + 0.5f);
    g = (unsigned)((c.y < 0.0f ? 0.0f : (c.y > 1.0f ? 1.0f : c.y)) * 255.0f + 0.5f);
    b = (unsigned)((c.z < 0.0f ? 0.0f : (c.z > 1.0f ? 1.0f : c.z)) * 255.0f + 0.5f);
    a = (unsigned)((c.w < 0.0f ? 0.0f : (c.w > 1.0f ? 1.0f : c.w)) * 255.0f + 0.5f);
    return OVL_COL(r, g, b, a);
}

ovl_vec4_t ovl_vec4_from_col(ovl_col_t c) {
    ovl_vec4_t v;
    v.x = (float)(c & 0xFF) / 255.0f;
    v.y = (float)((c >> 8) & 0xFF) / 255.0f;
    v.z = (float)((c >> 16) & 0xFF) / 255.0f;
    v.w = (float)((c >> 24) & 0xFF) / 255.0f;
    return v;
}
