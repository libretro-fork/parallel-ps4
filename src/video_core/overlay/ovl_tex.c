/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

/* The overlays' images: a fixed set of slots, each lent its pixels by the renderer when it
 * starts (memory the GPU copies from), so that a PNG decodes straight to where the GPU reads
 * it and nothing is allocated for an image during play.
 *
 * A slot goes FREE -> DECODING (taken by the thread decoding, compare-and-swap) -> READY
 * (decoded) -> LIVE (the renderer copied it to its image) -> RELEASED (its owner is done) ->
 * RETIRING (the renderer waits for frames that may still draw it) -> FREE. Any thread may
 * decode and release; the renderer's thread does the rest. No locks.
 *
 * ImGui drew an image as a texture of its own size sampled with repeat; here it lies in the
 * top left of a slot, drawn with its texture coordinates scaled to it, and the slot's texels
 * past its right and bottom edges, and in the slot's last row and column, are copies of the
 * texels repeat would have sampled there, so that what is drawn is the same.
 *
 * Icons (the save data dialog's, one a save) have cells of their own in one image, the icon
 * atlas, its memory lent the same way: each icon in its cell with a texel all round it, copies
 * of those repeat would have sampled past each edge, so that what is drawn is the same as from
 * a texture of its own; their draws sample the one atlas, so a list of them is one draw. An
 * icon too large for a cell takes a slot. */

#include <stdlib.h>
#include <string.h>

#include <formats/image.h>
#include <formats/rpng.h>
#include <retro_atomic.h>

#include "ovl_internal.h"

enum { FREE, DECODING, READY, LIVE, RELEASED, RETIRING };

typedef struct tex_slot {
    retro_atomic_int_t state;
    uint8_t* pixels; /* a slot's top left, or an icon's in its cell; RGBA */
    int stride;      /* pixels from a row to the next */
    int max_w, max_h;
    int cell_x, cell_y; /* an icon's cell in the atlas */
    int w, h;
    unsigned retire_frame; /* the renderer's */
} tex_slot_t;

/* The slots, then the icons' cells */
static tex_slot_t slots[OVL_TEX_SLOTS + OVL_ICON_CELLS];
static int slot_count, icon_count;

void ovl_tex_init(uint8_t* const* pixels, int count) {
    int i;
    if (count > OVL_TEX_SLOTS)
        count = OVL_TEX_SLOTS;
    for (i = 0; i < count; i++) {
        slots[i].pixels = pixels[i];
        slots[i].stride = OVL_TEX_DIM;
        slots[i].max_w = slots[i].max_h = OVL_TEX_DIM;
        slots[i].w = slots[i].h = 0;
        retro_atomic_store_release_int(&slots[i].state, FREE);
    }
    slot_count = count;
}

void ovl_tex_init_icons(uint8_t* atlas) {
    int i;
    icon_count = atlas ? OVL_ICON_CELLS : 0;
    for (i = 0; i < icon_count; i++) {
        tex_slot_t* s = &slots[OVL_TEX_SLOTS + i];
        s->cell_x = (i % OVL_ICON_COLS) * OVL_ICON_CELL_W;
        s->cell_y = (i / OVL_ICON_COLS) * OVL_ICON_CELL_H;
        s->stride = OVL_ICON_ATLAS_W;
        s->pixels =
            atlas + ((size_t)(s->cell_y + 1) * OVL_ICON_ATLAS_W + (size_t)s->cell_x + 1) * 4;
        s->max_w = OVL_ICON_CELL_W - 2;
        s->max_h = OVL_ICON_CELL_H - 2;
        s->w = s->h = 0;
        retro_atomic_store_release_int(&s->state, FREE);
    }
}

void ovl_tex_shutdown(void) {
    slot_count = icon_count = 0;
}

static int is_icon(int handle) {
    return handle > OVL_TEX_SLOTS && handle <= OVL_TEX_SLOTS + icon_count;
}

static tex_slot_t* slot_of(int handle) {
    return (handle > 0 && handle <= slot_count) || is_icon(handle) ? &slots[handle - 1] : NULL;
}

/* The texels repeat samples just past the image, where the slot has room for them */
static void wrap_edges(tex_slot_t* s) {
    uint32_t* px = (uint32_t*)s->pixels;
    const int w = s->w, h = s->h;
    int y;
    if (w < OVL_TEX_DIM)
        for (y = 0; y < h; y++) {
            px[(size_t)y * OVL_TEX_DIM + (size_t)w] = px[(size_t)y * OVL_TEX_DIM];
            if (w < OVL_TEX_DIM - 1)
                px[(size_t)y * OVL_TEX_DIM + OVL_TEX_DIM - 1] =
                    px[(size_t)y * OVL_TEX_DIM + (size_t)w - 1];
        }
    if (h < OVL_TEX_DIM) {
        const int right = w < OVL_TEX_DIM ? w + 1 : w;
        memcpy(px + (size_t)h * OVL_TEX_DIM, px, (size_t)right * 4);
        if (w < OVL_TEX_DIM - 1)
            px[(size_t)h * OVL_TEX_DIM + OVL_TEX_DIM - 1] = px[(size_t)w - 1];
        if (h < OVL_TEX_DIM - 1) {
            memcpy(px + (size_t)(OVL_TEX_DIM - 1) * OVL_TEX_DIM, px + (size_t)(h - 1) * OVL_TEX_DIM,
                   (size_t)right * 4);
            if (w < OVL_TEX_DIM - 1)
                px[(size_t)(OVL_TEX_DIM - 1) * OVL_TEX_DIM + OVL_TEX_DIM - 1] =
                    px[(size_t)(h - 1) * OVL_TEX_DIM + (size_t)w - 1];
        }
    }
}

/* An icon's border in its cell: past each edge, the texels of the opposite edge, as repeat
 * samples them; the corners the opposite corners */
static void wrap_cell(tex_slot_t* s) {
    uint32_t* px = (uint32_t*)s->pixels;
    const size_t stride = (size_t)s->stride, w = (size_t)s->w, h = (size_t)s->h;
    size_t y;
    for (y = 0; y < h; y++) {
        uint32_t* row = px + y * stride;
        row[-1] = row[w - 1];
        row[w] = row[0];
    }
    /* Rows whole, their borders now in place */
    memcpy(px - stride - 1, px + (h - 1) * stride - 1, (w + 2) * 4);
    memcpy(px + h * stride - 1, px - 1, (w + 2) * 4);
}

/* A PNG's chunks read up to its pixels, its size in @info; null when it is not a PNG rpng
 * reads. rpng only reads the bytes. */
static rpng_t* open_png(const uint8_t* png, size_t size, struct rpng_info* info) {
    rpng_t* rpng;
    if (!png || size == 0 || !(rpng = rpng_alloc()))
        return NULL;
    if (!rpng_set_buf_ptr(rpng, (void*)png, size) || !rpng_start(rpng)) {
        rpng_free(rpng);
        return NULL;
    }
    while (rpng_iterate_image(rpng)) {
    }
    if (!rpng_get_info(rpng, info) || info->width == 0 || info->height == 0) {
        rpng_free(rpng);
        return NULL;
    }
    return rpng;
}

/* Its pixels into @s's, as R, G, B, A bytes, the edges repeat samples past them after */
static int decode(tex_slot_t* s, rpng_t* rpng, size_t size, const struct rpng_info* info) {
    void* out = NULL;
    unsigned w = 0, h = 0;
    int ret;
    if (!rpng_set_output(rpng, (uint32_t*)s->pixels, (unsigned)s->stride))
        return 0;
    do {
        ret = rpng_process_image(rpng, &out, size, &w, &h, true);
    } while (ret == IMAGE_PROCESS_NEXT);
    if (ret != IMAGE_PROCESS_END || out != (void*)s->pixels)
        return 0;
    s->w = (int)info->width;
    s->h = (int)info->height;
    if (s->stride == OVL_TEX_DIM)
        wrap_edges(s);
    else
        wrap_cell(s);
    return 1;
}

/* A free one of slots[first, first + count) it fits, taken for decoding; -1 for none */
static int claim(int first, int count, const struct rpng_info* info) {
    int i;
    for (i = first; i < first + count; i++) {
        tex_slot_t* s = &slots[i];
        if ((int)info->width > s->max_w || (int)info->height > s->max_h)
            return -1;
        if (retro_atomic_load_relaxed_int(&s->state) == FREE &&
            retro_atomic_cas_int(&s->state, FREE, DECODING))
            return i;
    }
    return -1;
}

static int decode_into(int first, int count, const uint8_t* png, size_t size) {
    struct rpng_info info;
    rpng_t* rpng = open_png(png, size, &info);
    int i = -1;
    if (!rpng)
        return 0;
    if (first > 0)
        i = claim(first, count, &info);
    /* An icon too large for a cell, or none free: a slot */
    if (i < 0)
        i = claim(0, slot_count, &info);
    if (i >= 0) {
        if (decode(&slots[i], rpng, size, &info)) {
            retro_atomic_store_release_int(&slots[i].state, READY);
        } else {
            retro_atomic_store_release_int(&slots[i].state, FREE);
            i = -1;
        }
    }
    rpng_free(rpng);
    return i + 1;
}

int ovl_tex_decode_png(const uint8_t* png, size_t size) {
    return decode_into(0, 0, png, size);
}

int ovl_tex_decode_icon(const uint8_t* png, size_t size) {
    return decode_into(OVL_TEX_SLOTS, icon_count, png, size);
}

void ovl_tex_release(int handle) {
    tex_slot_t* s = slot_of(handle);
    int state;
    if (!s)
        return;
    state = retro_atomic_load_acquire_int(&s->state);
    /* The renderer may make it LIVE meanwhile */
    while ((state == READY || state == LIVE) && !retro_atomic_cas_int(&s->state, state, RELEASED))
        state = retro_atomic_load_acquire_int(&s->state);
}

const uint8_t* ovl_tex_pixels(int handle) {
    tex_slot_t* s = slot_of(handle);
    return s ? s->pixels : NULL;
}

int ovl_tex_size(int handle, int* w, int* h) {
    tex_slot_t* s = slot_of(handle);
    int state;
    if (!s)
        return 0;
    state = retro_atomic_load_acquire_int(&s->state);
    if (state != READY && state != LIVE)
        return 0;
    *w = s->w;
    *h = s->h;
    return 1;
}

int ovl_tex_uv_rect(ovl_tex_t tex, float uv[4], ovl_tex_t* draw_tex) {
    tex_slot_t* s = slot_of((int)tex);
    if (!s || s->w == 0)
        return 0;
    if (is_icon((int)tex)) {
        uv[0] = (float)(s->cell_x + 1) / OVL_ICON_ATLAS_W;
        uv[1] = (float)(s->cell_y + 1) / OVL_ICON_ATLAS_H;
        uv[2] = (float)s->w / OVL_ICON_ATLAS_W;
        uv[3] = (float)s->h / OVL_ICON_ATLAS_H;
        *draw_tex = OVL_TEX_ICONS;
    } else {
        uv[0] = uv[1] = 0.0f;
        uv[2] = (float)s->w / OVL_TEX_DIM;
        uv[3] = (float)s->h / OVL_TEX_DIM;
        *draw_tex = tex;
    }
    return 1;
}

int ovl_tex_take_upload(void) {
    int i;
    for (i = 0; i < OVL_TEX_SLOTS + icon_count; i++) {
        tex_slot_t* s = &slots[i];
        if (i >= slot_count && i < OVL_TEX_SLOTS)
            continue;
        if (retro_atomic_load_acquire_int(&s->state) == READY &&
            retro_atomic_cas_int(&s->state, READY, LIVE))
            return i + 1;
    }
    return 0;
}

int ovl_tex_icon_rect(int handle, int rect[4]) {
    tex_slot_t* s;
    if (!is_icon(handle))
        return 0;
    s = &slots[handle - 1];
    rect[0] = s->cell_x;
    rect[1] = s->cell_y;
    rect[2] = s->w + 2;
    rect[3] = s->h + 2;
    return 1;
}

void ovl_tex_collect(unsigned frame, unsigned frames_in_flight) {
    int i;
    for (i = 0; i < OVL_TEX_SLOTS + icon_count; i++) {
        tex_slot_t* s = &slots[i];
        int state;
        if (i >= slot_count && i < OVL_TEX_SLOTS)
            continue;
        state = retro_atomic_load_acquire_int(&s->state);
        if (state == RELEASED) {
            s->retire_frame = frame;
            retro_atomic_store_relaxed_int(&s->state, RETIRING);
        } else if (state == RETIRING && frame - s->retire_frame >= frames_in_flight) {
            s->w = s->h = 0;
            retro_atomic_store_release_int(&s->state, FREE);
        }
    }
}
