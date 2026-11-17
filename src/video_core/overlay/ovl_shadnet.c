/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

/* shadNet's notifications (a friend request, a friend online, the server's word), as
 * imgui/shadnet_notifications_layer.cpp drew them: up to six in a window in the top right,
 * newest first, each a coloured tag over its text, fading in and out over its six seconds.
 *
 * Any thread queues one: it takes a free slot (compare-and-swap), copies the text, and
 * publishes it with a ticket that keeps the order. The overlay's thread takes them in that
 * order each frame, ages them, and frees their slots as they go. Nothing is allocated and
 * nothing waits. */

#include <string.h>

#include <retro_atomic.h>

#include "ovl_layers.h"

#define TOAST_SLOTS 16
#define TOAST_TEXT 512
#define MAX_TOASTS 6 /* the oldest dropped beyond this */

#define LIFE_TIME 6.0f /* seconds a toast stays */
#define FADE_IN 0.25f
#define FADE_OUT 0.5f

enum { FREE, WRITING, READY, SHOWN };

typedef struct toast {
    retro_atomic_int_t state;
    unsigned ticket;
    int kind;
    char text[TOAST_TEXT];
    float age; /* the overlay thread's */
} toast_t;

static toast_t toasts[TOAST_SLOTS];
static retro_atomic_int_t next_ticket;
static retro_atomic_int_t live; /* queued or shown */

/* The overlay thread's: those shown, oldest first, and the next ticket to take */
static toast_t* shown[MAX_TOASTS];
static int shown_count;
static unsigned served;

int ovl_shadnet_push(int kind, const char* text) {
    int i;
    for (i = 0; i < TOAST_SLOTS; i++) {
        toast_t* t = &toasts[i];
        size_t len;
        if (retro_atomic_load_relaxed_int(&t->state) != FREE ||
            !retro_atomic_cas_int(&t->state, FREE, WRITING))
            continue;
        len = text ? strlen(text) : 0;
        if (len >= TOAST_TEXT)
            len = TOAST_TEXT - 1;
        if (len)
            memcpy(t->text, text, len);
        t->text[len] = '\0';
        t->kind = kind;
        t->age = 0.0f;
        retro_atomic_fetch_add_int(&live, 1);
        t->ticket = (unsigned)retro_atomic_fetch_add_int(&next_ticket, 1);
        retro_atomic_store_release_int(&t->state, READY);
        return 1;
    }
    return 0;
}

int ovl_shadnet_active(void) {
    return retro_atomic_load_acquire_int(&live) > 0;
}

static void drop_oldest(void) {
    toast_t* t = shown[0];
    int i;
    for (i = 1; i < shown_count; i++)
        shown[i - 1] = shown[i];
    shown_count--;
    retro_atomic_store_release_int(&t->state, FREE);
    retro_atomic_fetch_add_int(&live, -1);
}

/* Those published since, in the order they were queued: each the ticket after the last taken,
 * which may still be being published (taken a frame later, then) */
static void take_new(void) {
    int found = 1;
    while (found) {
        int i;
        found = 0;
        for (i = 0; i < TOAST_SLOTS; i++) {
            toast_t* t = &toasts[i];
            if (retro_atomic_load_acquire_int(&t->state) != READY || t->ticket != served)
                continue;
            served++;
            retro_atomic_store_relaxed_int(&t->state, SHOWN);
            if (shown_count == MAX_TOASTS)
                drop_oldest();
            shown[shown_count++] = t;
            found = 1;
        }
    }
}

void ovl_shadnet_reset(void) {
    while (shown_count)
        drop_oldest();
}

static const struct {
    const char* tag;
    float r, g, b, a;
} kinds[] = {
    {"Friend request", 0.45f, 0.70f, 1.00f, 1.0f}, /* OVL_SHADNET_FRIEND_REQUEST */
    {"Friend", 0.35f, 0.85f, 0.40f, 1.0f},         /* OVL_SHADNET_FRIEND_NEW */
    {"Friend", 0.85f, 0.55f, 0.35f, 1.0f},         /* OVL_SHADNET_FRIEND_LOST */
    {"Online", 0.35f, 0.85f, 0.40f, 1.0f},         /* OVL_SHADNET_ONLINE */
    {"shadNet", 0.75f, 0.75f, 0.78f, 1.0f},        /* OVL_SHADNET_INFO */
    {"Server", 0.95f, 0.70f, 0.25f, 1.0f},         /* OVL_SHADNET_WARNING */
};

void ovl_shadnet_draw(void) {
    ovl_vec2_t display, pos, size, pivot;
    float scale, margin;
    int i;
    take_new();
    for (i = 0; i < shown_count; i++)
        shown[i]->age += ovl_delta_time();
    while (shown_count && shown[0]->age >= LIFE_TIME)
        drop_oldest();
    if (!shown_count)
        return;

    display = ovl_display_size();
    scale = display.x / 1920.0f;
    margin = 20.0f * scale;
    pos.x = display.x - margin;
    pos.y = margin;
    pivot.x = 1.0f;
    pivot.y = 0.0f;
    size.x = 320.0f * scale;
    size.y = 0.0f;
    ovl_set_next_window_pos(pos, OVL_COND_ALWAYS, pivot);
    ovl_set_next_window_size(size, OVL_COND_ALWAYS);
    ovl_set_next_window_bg_alpha(0.85f);
    if (ovl_begin("##shadnet_notifications",
                  OVL_WINDOW_NO_DECORATION | OVL_WINDOW_NO_INPUTS | OVL_WINDOW_NO_NAV |
                      OVL_WINDOW_NO_FOCUS_ON_APPEARING | OVL_WINDOW_NO_SAVED_SETTINGS |
                      OVL_WINDOW_NO_MOVE | OVL_WINDOW_ALWAYS_AUTO_RESIZE)) {
        /* Newest first */
        for (i = shown_count - 1; i >= 0; i--) {
            const toast_t* t = shown[i];
            const int k = t->kind >= 0 && t->kind < (int)(sizeof(kinds) / sizeof(kinds[0]))
                              ? t->kind
                              : OVL_SHADNET_INFO;
            const float remaining = LIFE_TIME - t->age;
            float alpha = 1.0f;
            char tag[32];
            ovl_vec4_t col;
            if (t->age < FADE_IN)
                alpha = t->age / FADE_IN;
            else if (remaining < FADE_OUT)
                alpha = remaining / FADE_OUT > 0.0f ? remaining / FADE_OUT : 0.0f;
            ovl_push_var_float(OVL_VAR_ALPHA, alpha);
            col.x = kinds[k].r;
            col.y = kinds[k].g;
            col.z = kinds[k].b;
            col.w = kinds[k].a;
            strcpy(tag, "[");
            strcat(tag, kinds[k].tag);
            strcat(tag, "]");
            ovl_text_colored(col, tag, NULL);
            ovl_push_text_wrap_pos(0.0f);
            ovl_text(t->text, NULL);
            ovl_pop_text_wrap_pos();
            ovl_pop_var(1);
            if (i > 0)
                ovl_separator();
        }
    }
    ovl_end();
}
