/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

/* The splash screen: the game's picture (sce_sys/pic1.png) on black, fitted to the screen, for
 * as long as the game shows it (until sceSystemServiceHideSplashScreen), as the core's ImGui
 * window "Display##game_display" drew it.
 *
 * The PNG is read where the game's files lie. Its size comes from its header when the
 * overlays are made, so the memory it decodes into (the renderer's, which the GPU copies
 * from) is made then; a thread of its own decodes it there while the game starts, as ImGui's
 * texture worker did, and the game's picture shows until it is done, as it did. The
 * renderer's thread only looks at whether it is done: nothing waits. */

#include <string.h>

#include <formats/image.h>
#include <formats/rpng.h>
#include <retro_atomic.h>
#include <rthreads/rthreads.h>

#include "ovl_layers.h"

/* Larger than any screen it is fitted to; a PS4's is 1920 x 1080 */
#define SPLASH_MAX_DIM 4096

enum { NONE, DECODING, DECODED, FAILED };

static struct {
    rpng_t* rpng; /* the decoding thread's, once it starts */
    size_t size;
    unsigned w, h;
    uint8_t* pixels;
    sthread_t* thread;
    retro_atomic_int_t state;
    retro_atomic_int_t stop;
} splash;

int ovl_splash_open(const uint8_t* png, size_t size, unsigned* w, unsigned* h) {
    struct rpng_info info;
    ovl_splash_close();
    if (!png || size == 0)
        return 0;
    splash.rpng = rpng_alloc();
    /* rpng only reads the bytes. The header is the first chunk: its size, for the memory */
    if (!splash.rpng || !rpng_set_buf_ptr(splash.rpng, (void*)png, size) ||
        !rpng_start(splash.rpng) || !rpng_iterate_image(splash.rpng) ||
        !rpng_get_info(splash.rpng, &info) || info.width == 0 || info.height == 0 ||
        info.width > SPLASH_MAX_DIM || info.height > SPLASH_MAX_DIM) {
        ovl_splash_close();
        return 0;
    }
    splash.size = size;
    splash.w = info.width;
    splash.h = info.height;
    *w = splash.w;
    *h = splash.h;
    return 1;
}

/* The rest of the PNG's chunks, then its pixels, as R, G, B, A bytes, straight into the
 * memory lent; stopping between steps when asked */
static void decode(void* arg) {
    rpng_t* rpng = splash.rpng;
    void* out = NULL;
    unsigned w = 0, h = 0;
    int ret = IMAGE_PROCESS_ERROR;
    (void)arg;
    while (rpng_iterate_image(rpng))
        if (retro_atomic_load_relaxed_int(&splash.stop))
            goto done;
    if (!rpng_set_output(rpng, (uint32_t*)splash.pixels, splash.w))
        goto done;
    do {
        ret = rpng_process_image(rpng, &out, splash.size, &w, &h, true);
    } while (ret == IMAGE_PROCESS_NEXT && !retro_atomic_load_relaxed_int(&splash.stop));
done:
    rpng_free(rpng);
    splash.rpng = NULL;
    /* The pixels are written before it says so */
    retro_atomic_store_release_int(
        &splash.state, ret == IMAGE_PROCESS_END && out == (void*)splash.pixels ? DECODED : FAILED);
}

int ovl_splash_start(uint8_t* pixels) {
    if (!splash.rpng || !pixels)
        return 0;
    splash.pixels = pixels;
    retro_atomic_store_relaxed_int(&splash.stop, 0);
    retro_atomic_store_release_int(&splash.state, DECODING);
    splash.thread = sthread_create(decode, NULL);
    if (!splash.thread) {
        rpng_free(splash.rpng);
        splash.rpng = NULL;
        retro_atomic_store_release_int(&splash.state, FAILED);
        return 0;
    }
    return 1;
}

int ovl_splash_decoded(void) {
    return retro_atomic_load_acquire_int(&splash.state) == DECODED;
}

int ovl_splash_decoding(void) {
    return retro_atomic_load_acquire_int(&splash.state) == DECODING;
}

void ovl_splash_stop(void) {
    retro_atomic_store_relaxed_int(&splash.stop, 1);
}

void ovl_splash_close(void) {
    if (splash.thread) {
        /* When the overlays go: a decode still going stops at its next step */
        ovl_splash_stop();
        sthread_join(splash.thread);
    }
    if (splash.rpng)
        rpng_free(splash.rpng);
    memset(&splash, 0, sizeof(splash));
    retro_atomic_store_release_int(&splash.state, NONE);
}

/* The presenter's FitImage: the picture at its own shape, as large as fits, centred */
void ovl_splash_quad(float display_w, float display_h, ovl_vertex_t v[4]) {
    const int32_t sw = (int32_t)display_w, sh = (int32_t)display_h;
    const float frame_aspect = (float)(int32_t)splash.w / (float)(int32_t)splash.h;
    const float swapchain_aspect = (float)sw / (float)sh;
    uint32_t dst_w = (uint32_t)sw, dst_h = (uint32_t)sh;
    float x0, y0, x1, y1;
    int i;
    if (frame_aspect > swapchain_aspect)
        dst_h = (uint32_t)(int32_t)((float)sw / frame_aspect);
    else
        dst_w = (uint32_t)(int32_t)((float)sh * frame_aspect);
    /* At the window's start, which is the screen's top left */
    x0 = (float)(int32_t)(((uint32_t)sw - dst_w) / 2);
    y0 = (float)(int32_t)(((uint32_t)sh - dst_h) / 2);
    x1 = x0 + (float)dst_w;
    y1 = y0 + (float)dst_h;
    /* As ImGui's AddImage */
    v[0].x = x0;
    v[0].y = y0;
    v[0].u = 0.0f;
    v[0].v = 0.0f;
    v[1].x = x1;
    v[1].y = y0;
    v[1].u = 1.0f;
    v[1].v = 0.0f;
    v[2].x = x1;
    v[2].y = y1;
    v[2].u = 1.0f;
    v[2].v = 1.0f;
    v[3].x = x0;
    v[3].y = y1;
    v[3].u = 0.0f;
    v[3].v = 1.0f;
    for (i = 0; i < 4; i++)
        v[i].col = 0xFFFFFFFFu;
}
