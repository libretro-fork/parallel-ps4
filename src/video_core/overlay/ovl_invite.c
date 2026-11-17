/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

/* The prompt for a session invitation, the emulator's stand-in for the system software's, as
 * imgui/invitation_prompt_layer.cpp drew it: a window at the top in the middle, up to four
 * invitations in it, each with Accept and Decline. Holding Share (Back) for 0.6 seconds focuses
 * it, the pad's input then the prompt's and not the game's; circle gives it back.
 *
 * Invitations come and go from any thread (one received, one taken through the game's own
 * dialog, an answer done): each such change takes a free slot (compare-and-swap) and is
 * published with a ticket that keeps the order. The overlay's thread applies them in that order
 * each frame to the prompts it keeps. A choice made on screen is handed to the core
 * (ovl_invite_chosen), which answers the server on a thread of its own, the prompt showing it
 * busy until it is removed. Nothing is allocated and nothing waits. */

#include <stdio.h>
#include <string.h>

#include <retro_atomic.h>

#include "ovl_layers.h"

#define OP_SLOTS 16
#define ID_TEXT 128
#define MAX_PROMPTS 4           /* the oldest dropped beyond this */
#define FOCUS_HOLD_SECONDS 0.6f /* Share held this long focuses the prompt */

enum { FREE, WRITING, READY };
enum { OP_PUSH, OP_REMOVE };

typedef struct prompt {
    int user_id;
    char invitation_id[ID_TEXT];
    char session_id[ID_TEXT];
    char from_npid[ID_TEXT];
    int busy; /* answered, waiting for the server */
} prompt_t;

typedef struct op {
    retro_atomic_int_t state;
    unsigned ticket;
    int type;
    prompt_t p;
} op_t;

static op_t ops[OP_SLOTS];
static retro_atomic_int_t next_ticket;
static retro_atomic_int_t live; /* changes queued, or prompts shown */

/* The overlay thread's */
static prompt_t prompts[MAX_PROMPTS];
static int prompt_count;
static unsigned served;
static int focused;
static float back_held;

static void copy_text(char* dst, const char* src) {
    size_t n = src ? strlen(src) : 0;
    if (n >= ID_TEXT)
        n = ID_TEXT - 1;
    if (n)
        memcpy(dst, src, n);
    dst[n] = '\0';
}

static int queue(int type, int user_id, const char* invitation_id, const char* session_id,
                 const char* from_npid) {
    int i;
    for (i = 0; i < OP_SLOTS; i++) {
        op_t* o = &ops[i];
        if (retro_atomic_load_relaxed_int(&o->state) != FREE ||
            !retro_atomic_cas_int(&o->state, FREE, WRITING))
            continue;
        o->type = type;
        o->p.user_id = user_id;
        copy_text(o->p.invitation_id, invitation_id);
        copy_text(o->p.session_id, session_id);
        copy_text(o->p.from_npid, from_npid);
        o->p.busy = 0;
        retro_atomic_fetch_add_int(&live, 1);
        o->ticket = (unsigned)retro_atomic_fetch_add_int(&next_ticket, 1);
        retro_atomic_store_release_int(&o->state, READY);
        return 1;
    }
    return 0;
}

int ovl_invite_push(int user_id, const char* invitation_id, const char* session_id,
                    const char* from_npid) {
    return queue(OP_PUSH, user_id, invitation_id, session_id, from_npid);
}

int ovl_invite_remove(const char* invitation_id) {
    return queue(OP_REMOVE, 0, invitation_id, NULL, NULL);
}

int ovl_invite_active(void) {
    return retro_atomic_load_acquire_int(&live) > 0;
}

static void remove_at(int i) {
    for (; i + 1 < prompt_count; i++)
        prompts[i] = prompts[i + 1];
    prompt_count--;
    retro_atomic_fetch_add_int(&live, -1);
}

static void remove_id(const char* id) {
    int i = 0;
    while (i < prompt_count)
        if (strcmp(prompts[i].invitation_id, id) == 0)
            remove_at(i);
        else
            i++;
}

/* The changes published since, in the order they were made: each the ticket after the last
 * applied, which may still be being published (applied a frame later, then) */
static void apply_changes(void) {
    int found = 1;
    while (found) {
        int i;
        found = 0;
        for (i = 0; i < OP_SLOTS; i++) {
            op_t* o = &ops[i];
            if (retro_atomic_load_acquire_int(&o->state) != READY || o->ticket != served)
                continue;
            served++;
            /* A re-sent invitation replaces the older prompt rather than stacking another */
            remove_id(o->p.invitation_id);
            if (o->type == OP_PUSH) {
                if (prompt_count == MAX_PROMPTS)
                    remove_at(0);
                prompts[prompt_count++] = o->p;
                retro_atomic_fetch_add_int(&live, 1);
            }
            retro_atomic_store_release_int(&o->state, FREE);
            retro_atomic_fetch_add_int(&live, -1);
            found = 1;
        }
    }
}

static void set_focused(int f) {
    if (focused == f)
        return;
    focused = f;
    if (f)
        ovl_pad_capture_acquire();
    else
        ovl_pad_capture_release();
}

void ovl_invite_reset(void) {
    set_focused(0);
    while (prompt_count)
        remove_at(0);
}

static ovl_vec2_t vec2(float x, float y) {
    ovl_vec2_t v;
    v.x = x;
    v.y = y;
    return v;
}

void ovl_invite_draw(void) {
    ovl_vec2_t display;
    float scale, width, margin;
    unsigned flags;
    int i;
    apply_changes();
    if (!prompt_count) {
        set_focused(0);
        return;
    }

    if (!focused) {
        if (ovl_is_key_down(OVL_KEY_PAD_BACK)) {
            back_held += ovl_delta_time();
            if (back_held >= FOCUS_HOLD_SECONDS) {
                back_held = 0.0f;
                set_focused(1);
            }
        } else {
            back_held = 0.0f;
        }
    } else if (ovl_is_key_pressed(OVL_KEY_PAD_FACE_RIGHT, 0) ||
               ovl_is_key_pressed(OVL_KEY_ESCAPE, 0)) {
        /* Circle gives the pad back to the game */
        set_focused(0);
    }

    display = ovl_display_size();
    scale = display.x / 1920.0f;
    width = 440.0f * scale;
    margin = 20.0f * scale;
    ovl_set_next_window_pos(vec2(display.x / 2.0f, margin), OVL_COND_ALWAYS, vec2(0.5f, 0.0f));
    ovl_set_next_window_size(vec2(width, 0.0f), OVL_COND_ALWAYS);
    ovl_set_next_window_bg_alpha(focused ? 0.97f : 0.85f);
    flags = OVL_WINDOW_NO_DECORATION | OVL_WINDOW_NO_SAVED_SETTINGS | OVL_WINDOW_NO_MOVE |
            OVL_WINDOW_ALWAYS_AUTO_RESIZE;
    if (focused)
        ovl_set_next_window_focus();
    else
        flags |= OVL_WINDOW_NO_NAV | OVL_WINDOW_NO_FOCUS_ON_APPEARING;

    if (ovl_begin("##invitation_prompts", flags)) {
        /* The prompts as they were when the frame began: one answered is still drawn */
        const int count = prompt_count;
        for (i = 0; i < count; i++) {
            prompt_t* p = &prompts[i];
            const int busy = p->busy;
            char line[ID_TEXT + 32];
            ovl_vec4_t col;
            if (i > 0)
                ovl_separator();
            col.x = 0.45f;
            col.y = 0.70f;
            col.z = 1.00f;
            col.w = 1.0f;
            ovl_text_colored(col, "[Invitation]", NULL);
            ovl_same_line(0.0f, -1.0f);
            ovl_push_text_wrap_pos(0.0f);
            sprintf(line, "%s invited you to a session", p->from_npid);
            ovl_text(line, NULL);
            ovl_pop_text_wrap_pos();

            ovl_begin_disabled(busy);
            ovl_push_id_str(p->invitation_id);
            if (ovl_button(busy ? "Joining..." : "Accept", vec2(120.0f * scale, 0.0f))) {
                p->busy = 1;
                ovl_invite_chosen(p->user_id, p->invitation_id, p->from_npid, 1);
            }
            /* Navigation lands on the first prompt's Accept */
            if (focused && i == 0)
                ovl_set_item_default_focus();
            ovl_same_line(0.0f, -1.0f);
            if (ovl_button("Decline", vec2(120.0f * scale, 0.0f))) {
                p->busy = 1;
                ovl_invite_chosen(p->user_id, p->invitation_id, p->from_npid, 0);
            }
            ovl_pop_id();
            ovl_end_disabled();
        }

        ovl_spacing();
        ovl_text_disabled(focused ? "D-Pad: select   Cross: confirm   Circle/Esc: back to game"
                                  : "Hold Share/Back to respond, or click",
                          NULL);
    }
    ovl_end();
}
