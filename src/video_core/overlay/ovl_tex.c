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
 * texels repeat would have sampled there, so that what is drawn is the same. */

#include <stdlib.h>
#include <string.h>

#include <formats/image.h>
#include <formats/rpng.h>
#include <retro_atomic.h>

#include "ovl_internal.h"

enum { FREE, DECODING, READY, LIVE, RELEASED, RETIRING };

typedef struct tex_slot {
    retro_atomic_int_t state;
    uint8_t* pixels; /* OVL_TEX_DIM x OVL_TEX_DIM, RGBA */
    int w, h;
    unsigned retire_frame; /* the renderer's */
} tex_slot_t;

static tex_slot_t slots[OVL_TEX_SLOTS];
static int slot_count;

void ovl_tex_init(uint8_t* const* pixels, int count) {
    int i;
    if (count > OVL_TEX_SLOTS)
        count = OVL_TEX_SLOTS;
    for (i = 0; i < count; i++) {
        slots[i].pixels = pixels[i];
        slots[i].w = slots[i].h = 0;
        retro_atomic_store_release_int(&slots[i].state, FREE);
    }
    slot_count = count;
}

void ovl_tex_shutdown(void) {
    slot_count = 0;
}

static tex_slot_t* slot_of(int handle) {
    return handle > 0 && handle <= slot_count ? &slots[handle - 1] : NULL;
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

/* Decodes @png into @s's pixels, as R, G, B, A bytes; 0 when it is not a PNG rpng reads or
 * it is larger than a slot */
static int decode(tex_slot_t* s, const uint8_t* png, size_t size) {
    rpng_t* rpng = rpng_alloc();
    struct rpng_info info;
    void* out = NULL;
    unsigned w = 0, h = 0;
    int ret, ok = 0;
    /* rpng only reads the bytes */
    if (!rpng || !rpng_set_buf_ptr(rpng, (void*)png, size) || !rpng_start(rpng))
        goto done;
    while (rpng_iterate_image(rpng)) {
    }
    if (!rpng_get_info(rpng, &info) || info.width == 0 || info.height == 0 ||
        info.width > OVL_TEX_DIM || info.height > OVL_TEX_DIM)
        goto done;
    if (!rpng_set_output(rpng, (uint32_t*)s->pixels, OVL_TEX_DIM))
        goto done;
    do {
        ret = rpng_process_image(rpng, &out, size, &w, &h, true);
    } while (ret == IMAGE_PROCESS_NEXT);
    if (ret != IMAGE_PROCESS_END || out != (void*)s->pixels)
        goto done;
    s->w = (int)info.width;
    s->h = (int)info.height;
    wrap_edges(s);
    ok = 1;
done:
    rpng_free(rpng);
    return ok;
}

int ovl_tex_decode_png(const uint8_t* png, size_t size) {
    int i;
    if (!png || size == 0)
        return 0;
    for (i = 0; i < slot_count; i++) {
        tex_slot_t* s = &slots[i];
        if (retro_atomic_load_relaxed_int(&s->state) != FREE ||
            !retro_atomic_cas_int(&s->state, FREE, DECODING))
            continue;
        if (!decode(s, png, size)) {
            retro_atomic_store_release_int(&s->state, FREE);
            return 0;
        }
        retro_atomic_store_release_int(&s->state, READY);
        return i + 1;
    }
    return 0;
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

int ovl_tex_uv_scale(ovl_tex_t tex, float* su, float* sv) {
    tex_slot_t* s = slot_of((int)tex);
    if (!s || s->w == 0)
        return 0;
    *su = (float)s->w / OVL_TEX_DIM;
    *sv = (float)s->h / OVL_TEX_DIM;
    return 1;
}

int ovl_tex_take_upload(void) {
    int i;
    for (i = 0; i < slot_count; i++) {
        tex_slot_t* s = &slots[i];
        if (retro_atomic_load_acquire_int(&s->state) == READY &&
            retro_atomic_cas_int(&s->state, READY, LIVE))
            return i + 1;
    }
    return 0;
}

void ovl_tex_collect(unsigned frame, unsigned frames_in_flight) {
    int i;
    for (i = 0; i < slot_count; i++) {
        tex_slot_t* s = &slots[i];
        const int state = retro_atomic_load_acquire_int(&s->state);
        if (state == RELEASED) {
            s->retire_frame = frame;
            retro_atomic_store_relaxed_int(&s->state, RETIRING);
        } else if (state == RETIRING && frame - s->retire_frame >= frames_in_flight) {
            s->w = s->h = 0;
            retro_atomic_store_release_int(&s->state, FREE);
        }
    }
}
