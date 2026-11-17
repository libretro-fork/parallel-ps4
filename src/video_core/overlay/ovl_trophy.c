/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

/* The trophy popup, as core/libraries/np/trophy_ui.cpp drew it with ImGui: the trophy's type
 * icon, "Trophy earned!" and its name, and its icon, sliding in from the side the settings
 * say, for as long as they say, fading out over the last second, with the trophy sound.
 * A trophy unlocked while one shows takes its place, its animation starting again, as there.
 *
 * The game's thread queues it (its icons decoded into image slots there, ovl_tex.c) in a free
 * slot taken by compare-and-swap, with a ticket; the overlay's thread shows the newest.
 * Nothing is allocated and nothing waits. */

#include <math.h>
#include <string.h>

#include <retro_atomic.h>

#include "core/emulator_settings.h"
#include "libretro/audio_mix.h"
#include "ovl_layers.h"

#define TROPHY_SLOTS 8
#define TROPHY_NAME 512

enum { FREE, WRITING, READY };
enum { SIDE_RIGHT, SIDE_LEFT, SIDE_TOP, SIDE_BOTTOM };

typedef struct trophy {
    retro_atomic_int_t state;
    unsigned ticket;
    char name[TROPHY_NAME];
    int icon; /* image handles, 0 for none */
    int type_icon;
} trophy_t;

static trophy_t trophies[TROPHY_SLOTS];
static retro_atomic_int_t next_ticket;
static retro_atomic_int_t showing; /* read from any thread */

/* The sound, made once before the game runs (trophy_ui.cpp) */
static const float* sound;
static unsigned sound_frames;

/* What shows, the overlay thread's */
static struct {
    int active;
    char name[TROPHY_NAME];
    int icon, type_icon;
    int side;
    double timer;
    float elapsed;
    int taken;       /* one has been taken, ever */
    unsigned ticket; /* the last taken's */
} shown;

void ovl_trophy_set_sound(const float* frames, unsigned count) {
    sound = frames;
    sound_frames = count;
}

int ovl_trophy_queue(const char* name, const uint8_t* icon_png, size_t icon_size,
                     const uint8_t* type_png, size_t type_size) {
    int i;
    if (emu_settings_get_trophy_popup_disabled())
        return 0;
    for (i = 0; i < TROPHY_SLOTS; i++) {
        trophy_t* t = &trophies[i];
        size_t len;
        if (retro_atomic_load_relaxed_int(&t->state) != FREE ||
            !retro_atomic_cas_int(&t->state, FREE, WRITING))
            continue;
        len = strlen(name);
        if (len >= TROPHY_NAME)
            len = TROPHY_NAME - 1;
        memcpy(t->name, name, len);
        t->name[len] = '\0';
        t->icon = ovl_tex_decode_png(icon_png, icon_size);
        t->type_icon = ovl_tex_decode_png(type_png, type_size);
        t->ticket = (unsigned)retro_atomic_fetch_add_int(&next_ticket, 1);
        retro_atomic_store_release_int(&t->state, READY);
        return 1;
    }
    return 0;
}

static void release_shown(void) {
    if (shown.icon)
        ovl_tex_release(shown.icon);
    if (shown.type_icon)
        ovl_tex_release(shown.type_icon);
    shown.icon = shown.type_icon = 0;
    shown.active = 0;
    retro_atomic_store_release_int(&showing, 0);
}

static void discard(trophy_t* t) {
    ovl_tex_release(t->icon);
    ovl_tex_release(t->type_icon);
    retro_atomic_store_release_int(&t->state, FREE);
}

/* The newest queued takes the place of what shows; older ones go unseen, as do any found
 * later that were queued before what shows (published while an earlier look went by) */
static void take_newest(void) {
    trophy_t* best = NULL;
    const char* side;
    int i;
    for (i = 0; i < TROPHY_SLOTS; i++) {
        trophy_t* t = &trophies[i];
        if (retro_atomic_load_acquire_int(&t->state) != READY)
            continue;
        if (shown.taken && (int)(t->ticket - shown.ticket) < 0) {
            discard(t);
            continue;
        }
        if (best && (int)(t->ticket - best->ticket) < 0) {
            discard(t);
            continue;
        }
        if (best)
            discard(best);
        best = t;
    }
    if (!best)
        return;
    release_shown();
    memcpy(shown.name, best->name, sizeof(shown.name));
    shown.icon = best->icon;
    shown.type_icon = best->type_icon;
    shown.ticket = best->ticket;
    shown.taken = 1;
    retro_atomic_store_release_int(&best->state, FREE);
    side = emu_settings_get_trophy_notification_side();
    shown.side = strcmp(side, "top") == 0      ? SIDE_TOP
                 : strcmp(side, "left") == 0   ? SIDE_LEFT
                 : strcmp(side, "bottom") == 0 ? SIDE_BOTTOM
                                               : SIDE_RIGHT;
    shown.timer = emu_settings_get_trophy_notification_duration();
    shown.elapsed = 0.0f;
    shown.active = 1;
    retro_atomic_store_release_int(&showing, 1);
    /* Louder than the game, as standalone plays it */
    if (sound_frames)
        audio_mix_play_clip(sound, sound_frames,
                            (float)emu_settings_get_volume_slider() * 0.01f * 1.2f);
}

/* A grey square where an image has not come, as ImGui's placeholder */
static void placeholder(float adjust_h) {
    ovl_vec4_t grey;
    ovl_vec2_t a = ovl_cursor_screen_pos(), b;
    grey.x = grey.y = grey.z = grey.w = 0.7f;
    b.x = a.x + 50.0f * adjust_h;
    b.y = a.y + 50.0f * adjust_h;
    ovl_add_rect_filled(ovl_window_draw_list(), a, b, ovl_color_u32_vec4(grey), 0.0f, 0);
}

void ovl_trophy_draw(void) {
    static const char earned[] = "Trophy earned!\n";
    char line[sizeof(earned) + TROPHY_NAME];
    ovl_vec2_t display, window_size, pos, zero, icon, uv0, uv1;
    float adjust_w, adjust_h, progress, start_x, start_y, final_x, final_y, fade, wrap_width;
    int iw, ih;
    take_newest();
    if (!shown.active)
        return;

    display = ovl_display_size();
    adjust_w = display.x / 1920;
    adjust_h = display.y / 1080;
    window_size.x = 350 * adjust_w < display.x ? 350 * adjust_w : display.x;
    window_size.y = 70 * adjust_h < display.y ? 70 * adjust_h : display.y;

    shown.elapsed += ovl_delta_time();
    progress = shown.elapsed / 0.5f;
    if (progress > 1.0f)
        progress = 1.0f;
    switch (shown.side) {
    case SIDE_TOP:
        start_x = (display.x - window_size.x) * 0.5f;
        start_y = -window_size.y;
        final_x = start_x;
        final_y = 20 * adjust_h;
        break;
    case SIDE_LEFT:
        start_x = -window_size.x;
        start_y = 50 * adjust_h;
        final_x = 20 * adjust_w;
        final_y = start_y;
        break;
    case SIDE_BOTTOM:
        start_x = (display.x - window_size.x) * 0.5f;
        start_y = display.y;
        final_x = start_x;
        final_y = display.y - window_size.y - 20 * adjust_h;
        break;
    case SIDE_RIGHT:
    default:
        start_x = display.x;
        start_y = 50 * adjust_h;
        final_x = display.x - window_size.x - 20 * adjust_w;
        final_y = start_y;
        break;
    }
    pos.x = start_x + (final_x - start_x) * progress;
    pos.y = start_y + (final_y - start_y) * progress;

    shown.timer -= ovl_delta_time();
    /* Fading in as it slides in, out over its last second */
    if (shown.timer <= 1.0f) {
        const float fade_out_time = (float)(1.0f - (shown.timer / 1.0f));
        fade = 1.0f - fade_out_time;
    } else
        fade = progress;
    if (fade < 0.0f)
        fade = 0.0f;
    if (fade > 1.0f)
        fade = 1.0f;

    zero.x = zero.y = 0.0f;
    ovl_set_next_window_size(window_size, OVL_COND_ALWAYS);
    ovl_set_next_window_pos(pos, OVL_COND_ALWAYS, zero);
    ovl_push_var_float(OVL_VAR_ALPHA, fade);
    if (ovl_begin("Trophy Window",
                  OVL_WINDOW_NO_DECORATION | OVL_WINDOW_NO_SAVED_SETTINGS | OVL_WINDOW_NO_INPUTS)) {
        icon.x = 50 * adjust_w;
        icon.y = 50 * adjust_h;
        uv0.x = uv0.y = 0.0f;
        uv1.x = uv1.y = 1.0f;

        /* The type's icon */
        if (shown.type_icon && ovl_tex_size(shown.type_icon, &iw, &ih)) {
            ovl_set_cursor_pos_y((window_size.y * 0.5f) - (25 * adjust_h));
            ovl_image((ovl_tex_t)shown.type_icon, icon, uv0, uv1);
            ovl_same_line(0.0f, -1.0f);
        } else {
            placeholder(adjust_h);
            ovl_indent(60);
        }

        /* The trophy's name, smaller for a long one */
        wrap_width =
            ovl_calc_wrap_width_for_pos(ovl_cursor_screen_pos(), window_size.x - (60 * adjust_w));
        ovl_set_window_font_scale((float)(1.2 * adjust_h));
        if (ovl_calc_text_size(shown.name, NULL, 0.0f).x > wrap_width) {
            ovl_set_cursor_pos_y(5 * adjust_h);
            if (ovl_calc_text_size(shown.name, NULL, 0.0f).x > wrap_width * 2)
                ovl_set_window_font_scale((float)(0.95 * adjust_h));
            else
                ovl_set_window_font_scale((float)(1.1 * adjust_h));
        } else {
            strcpy(line, earned);
            strcat(line, shown.name);
            ovl_set_cursor_pos_y(
                (float)((window_size.y - ovl_calc_text_size(line, NULL, 0.0f).y) * 0.5));
        }
        if (shown.side == SIDE_TOP || shown.side == SIDE_BOTTOM) {
            const float centered =
                (window_size.x - ovl_calc_text_size(shown.name, NULL, 0.0f).x) * 0.5f;
            ovl_set_cursor_pos_x(centered > 10.0f * adjust_w ? centered : 10.0f * adjust_w);
        }
        strcpy(line, earned);
        strcat(line, shown.name);
        ovl_push_text_wrap_pos(window_size.x - (60 * adjust_w));
        ovl_text_wrapped(line, NULL);
        ovl_same_line(window_size.x - (60 * adjust_w), -1.0f);

        /* The trophy's icon */
        if (shown.icon && ovl_tex_size(shown.icon, &iw, &ih)) {
            ovl_set_cursor_pos_y((window_size.y * 0.5f) - (25 * adjust_h));
            ovl_image((ovl_tex_t)shown.icon, icon, uv0, uv1);
        } else {
            placeholder(adjust_h);
        }
    }
    ovl_end();
    ovl_pop_var(1);

    if (shown.timer <= 0)
        release_shown();
}

int ovl_trophy_active(void) {
    int i;
    if (retro_atomic_load_acquire_int(&showing))
        return 1;
    for (i = 0; i < TROPHY_SLOTS; i++)
        if (retro_atomic_load_acquire_int(&trophies[i].state) == READY)
            return 1;
    return 0;
}

void ovl_trophy_reset(void) {
    int i;
    release_shown();
    for (i = 0; i < TROPHY_SLOTS; i++)
        if (retro_atomic_load_acquire_int(&trophies[i].state) == READY) {
            ovl_tex_release(trophies[i].icon);
            ovl_tex_release(trophies[i].type_icon);
            retro_atomic_store_release_int(&trophies[i].state, FREE);
        }
}
