/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

/* The emulator's notifications (a message with an icon, sliding in from a side), as
 * imgui/notifications_layer.cpp drew them: one at a time, the rest waiting their turn, each
 * fading out over its last second.
 *
 * Any thread queues one: it takes a free slot (compare-and-swap), decodes the icon into an
 * image slot (ovl_tex.c), and publishes it with a ticket that keeps the order. The overlay's
 * thread shows the oldest when none is showing. Nothing is allocated and nothing waits. */

#include <string.h>

#include <retro_atomic.h>

#include "ovl_layers.h"

#define NOTIFY_SLOTS 16
#define NOTIFY_TEXT 1024

enum { FREE, WRITING, READY, SHOWING };

typedef struct note {
    retro_atomic_int_t state;
    unsigned ticket;
    char message[NOTIFY_TEXT];
    float timer;
    int pos;
    int icon; /* image handle, 0 for none */
    int icon_background;
} note_t;

static note_t notes[NOTIFY_SLOTS];
static retro_atomic_int_t next_ticket;

/* The overlay thread's */
static note_t* showing;
static float elapsed_time;
static unsigned served; /* the next ticket to show */

int ovl_notify_queue(const char* message, float timer, int pos, const uint8_t* png, size_t png_size,
                     int icon_background) {
    int i;
    for (i = 0; i < NOTIFY_SLOTS; i++) {
        note_t* n = &notes[i];
        size_t len;
        if (retro_atomic_load_relaxed_int(&n->state) != FREE ||
            !retro_atomic_cas_int(&n->state, FREE, WRITING))
            continue;
        len = strlen(message);
        if (len >= NOTIFY_TEXT)
            len = NOTIFY_TEXT - 1;
        memcpy(n->message, message, len);
        n->message[len] = '\0';
        n->timer = timer;
        n->pos = pos;
        n->icon = ovl_tex_decode_png(png, png_size);
        n->icon_background = icon_background;
        n->ticket = (unsigned)retro_atomic_fetch_add_int(&next_ticket, 1);
        retro_atomic_store_release_int(&n->state, READY);
        return 1;
    }
    return 0;
}

/* The next in line, or none yet. Tickets are taken only by notifications that are then
 * published, so they follow on from one another: the next is the one with the ticket after the
 * last shown, which may still be being published (a frame later, then). A scan for the oldest
 * could miss one published while it ran and show a later one first. */
static void finish(note_t* n);

static note_t* take_next(void) {
    int i;
    for (i = 0; i < NOTIFY_SLOTS; i++) {
        note_t* n = &notes[i];
        if (retro_atomic_load_acquire_int(&n->state) != READY)
            continue;
        if (n->ticket == served) {
            served++;
            retro_atomic_store_relaxed_int(&n->state, SHOWING);
            return n;
        }
        /* Queued before a reset, which passed it over */
        if ((int)(n->ticket - served) < 0)
            finish(n);
    }
    return NULL;
}

static void finish(note_t* n) {
    if (n->icon)
        ovl_tex_release(n->icon);
    n->icon = 0;
    retro_atomic_store_release_int(&n->state, FREE);
}

void ovl_notify_draw(void) {
    note_t* n;
    ovl_vec2_t display, padding, window_size, pos, size;
    float adjust_w, adjust_h, wrap_width, text_height, progress, start_x, final_x, height;
    float fade;
    int iw, ih;
    if (!showing) {
        showing = take_next();
        elapsed_time = 0.0f;
    }
    n = showing;
    if (!n)
        return;

    display = ovl_display_size();
    n->timer -= ovl_delta_time();
    adjust_w = display.x / 1920;
    adjust_h = display.y / 1080;
    padding.x = padding.y = 8.0f; /* the style's window padding */

    wrap_width = 300 * adjust_w - padding.x * 2; /* 350 window size - 50 image size */
    text_height = ovl_calc_text_size(n->message, NULL, wrap_width).y;
    window_size.x = 350 * adjust_w;
    window_size.y = 70 * adjust_h;
    if (text_height + padding.y * 2.0f > window_size.y)
        window_size.y = text_height + padding.y * 2.0f;

    elapsed_time += ovl_delta_time();
    progress = elapsed_time / 0.5f;
    if (progress > 1.0f)
        progress = 1.0f;
    switch (n->pos) {
    case OVL_NOTIFY_TOP_LEFT:
    default:
        start_x = -window_size.x;
        final_x = 20 * adjust_w;
        height = 20 * adjust_h;
        break;
    case OVL_NOTIFY_TOP_RIGHT:
        start_x = display.x;
        final_x = display.x - window_size.x - 20 * adjust_w;
        height = 20 * adjust_h;
        break;
    case OVL_NOTIFY_BOTTOM_LEFT:
        start_x = -window_size.x;
        final_x = 20 * adjust_w;
        height = display.y - window_size.y - 20 * adjust_h;
        break;
    case OVL_NOTIFY_BOTTOM_RIGHT:
        start_x = display.x;
        final_x = display.x - window_size.x - 20 * adjust_w;
        height = display.y - window_size.y - 20 * adjust_h;
        break;
    }
    pos.x = start_x + (final_x - start_x) * progress;
    pos.y = height;

    /* Fading out over its last second */
    fade = n->timer <= 1.0f ? 1.0f - (1.0f - (n->timer / 1.0f)) : 1.0f;
    if (fade < 0.0f)
        fade = 0.0f;
    if (fade > 1.0f)
        fade = 1.0f;

    ovl_set_next_window_size(window_size, OVL_COND_ALWAYS);
    size.x = size.y = 0.0f;
    ovl_set_next_window_pos(pos, OVL_COND_ALWAYS, size);
    ovl_push_var_float(OVL_VAR_ALPHA, fade);
    if (ovl_begin("Notification Window",
                  OVL_WINDOW_NO_DECORATION | OVL_WINDOW_NO_SAVED_SETTINGS | OVL_WINDOW_NO_INPUTS)) {
        if (n->icon && ovl_tex_size(n->icon, &iw, &ih)) {
            ovl_vec2_t uv0, uv1, icon;
            icon.x = 50 * adjust_w;
            icon.y = 50 * adjust_h;
            if (n->icon_background) {
                ovl_vec2_t a = ovl_cursor_screen_pos(), b;
                b.x = a.x + icon.x;
                b.y = a.y + icon.y;
                ovl_add_rect_filled(ovl_window_draw_list(), a, b, OVL_COL(211, 211, 211, 255), 0.0f,
                                    0);
            }
            uv0.x = uv0.y = 0.0f;
            uv1.x = uv1.y = 1.0f;
            ovl_image((ovl_tex_t)n->icon, icon, uv0, uv1);
        }
        ovl_same_line(0.0f, -1.0f);
        ovl_text_wrapped(n->message, NULL);
    }
    ovl_pop_var(1);
    ovl_end();

    if (n->timer <= 0) {
        finish(n);
        showing = take_next();
        elapsed_time = 0.0f;
    }
}

const char* ovl_notify_showing_message(void) {
    return showing ? showing->message : NULL;
}

int ovl_notify_active(void) {
    int i;
    for (i = 0; i < NOTIFY_SLOTS; i++)
        if (retro_atomic_load_acquire_int(&notes[i].state) >= READY)
            return 1;
    return 0;
}

void ovl_notify_reset(void) {
    int i;
    showing = NULL;
    for (i = 0; i < NOTIFY_SLOTS; i++)
        if (retro_atomic_load_acquire_int(&notes[i].state) >= READY)
            finish(&notes[i]);
    served = (unsigned)retro_atomic_load_acquire_int(&next_ticket);
}
