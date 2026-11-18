/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

/* The profile dialog (libSceNpProfileDialog), as core/libraries/np/np_profile_dialog's
 * np_profile_dialog_ui.cpp drew it with ImGui: the screen dimmed, and in its middle a card
 * with a blue header naming what was asked (a profile, a friend request, the block list, a
 * report), a round placeholder for the player's picture, the player's online id or account
 * number, and Close. It fades in.
 *
 * The game's thread opens it and may close it; the overlay's thread draws it and closes it
 * when Close is pressed. What is shown goes from one to the other through three buffers, the
 * one between swapped by each side, as the message dialog's; it is finished by whichever side
 * claims it first, its result published with it. Nothing is allocated. */

#include <string.h>

#include <retro_atomic.h>

#include "ovl_layers.h"

#define ID_TEXT 32
#define DIRTY 4

/* The status, CommonDialog::Status's values, and the claim between running and finished */
enum { NONE, INITIALIZED, RUNNING, FINISHED, CLOSING };

typedef struct profile_param {
    unsigned gen; /* which opening it is from */
    int mode;
    char id[ID_TEXT + 1];
} profile_param_t;

/* The game's thread's: what it opened last, and the buffer it writes */
static profile_param_t staged;
static int back;
/* The buffers; the one between, with DIRTY when the game's thread put it there since */
static profile_param_t bufs[3];
static retro_atomic_int_t middle;
/* The overlay's thread's: the buffer it reads, the opening it last drew, how far it has faded
 * in */
static int front;
static unsigned drawn_gen;
static int drawn;
static float open_alpha;

static retro_atomic_int_t status;
static retro_atomic_int_t result;

void ovl_profile_dialog_init(void) {
    memset(&staged, 0, sizeof(staged));
    memset(bufs, 0, sizeof(bufs));
    back = 0;
    retro_atomic_store_relaxed_int(&middle, 1);
    front = 2;
    drawn = 0;
    drawn_gen = 0;
    open_alpha = 0.0f;
}

void ovl_profile_dialog_shutdown(void) {
    ovl_profile_dialog_close(OVL_PROFILE_RESULT_USER_CANCELED);
}

int ovl_profile_dialog_status(void) {
    const int s = retro_atomic_load_acquire_int(&status);
    return s == CLOSING ? RUNNING : s;
}

void ovl_profile_dialog_set_status(int s) {
    retro_atomic_store_release_int(&status, s);
}

/* @value in decimal, as "%" PRIu64 */
static void format_u64(char* dst, uint64_t value) {
    char digits[20];
    int n = 0;
    do {
        digits[n++] = (char)('0' + (int)(value % 10u));
        value /= 10u;
    } while (value != 0);
    while (n)
        *dst++ = digits[--n];
    *dst = '\0';
}

void ovl_profile_dialog_open(const ovl_profile_dialog_param_t* p) {
    staged.gen++;
    staged.mode = p->mode;
    if (p->has_account_id) {
        format_u64(staged.id, p->account_id);
    } else {
        size_t n = p->online_id ? strlen(p->online_id) : 0;
        if (n > ID_TEXT)
            n = ID_TEXT;
        if (n)
            memcpy(staged.id, p->online_id, n);
        staged.id[n] = '\0';
    }
    retro_atomic_store_relaxed_int(&result, 0);
    bufs[back] = staged;
    back = retro_atomic_exchange_int(&middle, back | DIRTY) & ~DIRTY;
    retro_atomic_store_release_int(&status, RUNNING);
}

int ovl_profile_dialog_close(int user_action) {
    if (!retro_atomic_cas_int(&status, RUNNING, CLOSING))
        return 0;
    retro_atomic_store_relaxed_int(&result, user_action);
    retro_atomic_store_release_int(&status, FINISHED);
    return 1;
}

int ovl_profile_dialog_result(void) {
    return retro_atomic_load_relaxed_int(&result);
}

int ovl_profile_dialog_active(void) {
    return retro_atomic_load_acquire_int(&status) == RUNNING;
}

/* ------------------------------------------------------------------------------------------ */
/* Drawing, as np_profile_dialog_ui.cpp */

#define CARD_WIDTH 480.0f
#define CARD_HEIGHT 330.0f
#define HEADER_HEIGHT 44.0f
#define FOOTER_HEIGHT 56.0f
#define AVATAR_RADIUS 44.0f
#define AVATAR_REL_X (CARD_WIDTH * 0.5f)
#define AVATAR_REL_Y (HEADER_HEIGHT + AVATAR_RADIUS + 16.0f)
#define FADE_SPEED 8.0f
#define CLOSE_BUTTON_W 110.0f
#define CLOSE_BUTTON_H 34.0f

/* Its colours, the console's */
static const ovl_vec4_t col_overlay = {0.00f, 0.00f, 0.00f, 0.65f};
static const ovl_vec4_t col_header_top = {0.00f, 0.34f, 0.62f, 1.00f};
static const ovl_vec4_t col_header_bot = {0.00f, 0.22f, 0.42f, 1.00f};
static const ovl_vec4_t col_card_bg = {0.11f, 0.11f, 0.12f, 1.00f};
static const ovl_vec4_t col_separator = {0.22f, 0.22f, 0.24f, 1.00f};
static const ovl_vec4_t col_avatar_bg = {0.18f, 0.18f, 0.20f, 1.00f};
static const ovl_vec4_t col_avatar_outline = {0.00f, 0.44f, 0.75f, 1.00f};
static const ovl_vec4_t col_text_primary = {1.00f, 1.00f, 1.00f, 1.00f};
static const ovl_vec4_t col_text_secondary = {0.65f, 0.65f, 0.68f, 1.00f};
static const ovl_vec4_t col_button_normal = {0.18f, 0.18f, 0.20f, 1.00f};
static const ovl_vec4_t col_button_hovered = {0.00f, 0.44f, 0.75f, 1.00f};
static const ovl_vec4_t col_button_active = {0.00f, 0.33f, 0.60f, 1.00f};
static const ovl_vec4_t col_button_border = {0.32f, 0.32f, 0.35f, 1.00f};

static ovl_vec2_t vec2(float x, float y) {
    ovl_vec2_t v;
    v.x = x;
    v.y = y;
    return v;
}

/* The colour faded, as a vector (what the style is given: ImGui's PushStyleColor of a packed
 * colour unpacks it) */
static ovl_vec4_t faded(ovl_vec4_t col, float alpha) {
    col.w *= alpha;
    return ovl_vec4_from_col(ovl_color_u32_vec4(col));
}

static ovl_col_t alpha_scale(ovl_vec4_t col, float alpha) {
    col.w *= alpha;
    return ovl_color_u32_vec4(col);
}

static ovl_col_t col32(int r, int g, int b, int a) {
    return OVL_COL((unsigned)r & 0xFFu, (unsigned)g & 0xFFu, (unsigned)b & 0xFFu,
                   (unsigned)a & 0xFFu);
}

static void draw_card_shadow(ovl_draw_list_t* dl, ovl_vec2_t min, ovl_vec2_t max, float alpha) {
    int i;
    for (i = 0; i < 6; ++i) {
        const float spread = (float)(i + 1) * 3.5f;
        const float a = alpha * (0.28f - (float)i * 0.04f);
        if (a <= 0.0f)
            break;
        ovl_add_rect_filled(dl, vec2(min.x - spread, min.y + spread),
                            vec2(max.x + spread, max.y + spread * 1.5f),
                            col32(0, 0, 0, (int)(a * 255.0f)), 8.0f + spread, 0);
    }
}

static void draw_avatar_placeholder(ovl_draw_list_t* dl, ovl_vec2_t center, float radius,
                                    float alpha) {
    const ovl_col_t bg = alpha_scale(col_avatar_bg, alpha);
    const ovl_col_t outline = alpha_scale(col_avatar_outline, alpha);
    const ovl_col_t silhouette = col32(90, 90, 96, (int)(alpha * 255.0f));
    ovl_add_circle_filled(dl, center, radius, bg, 64);
    ovl_add_circle_filled(dl, vec2(center.x, center.y - radius * 0.18f), radius * 0.32f, silhouette,
                          32);
    ovl_add_circle_filled(dl, vec2(center.x, center.y + radius * 0.45f), radius * 0.55f, silhouette,
                          48);
    /* The outline last, over the silhouette */
    ovl_add_circle(dl, center, radius, outline, 64, 2.5f);
}

static const char* mode_title(int mode) {
    switch (mode) {
    case OVL_PROFILE_MODE_FRIEND_REQUEST:
        return "Friend Request";
    case OVL_PROFILE_MODE_ADD_TO_BLOCK_LIST:
        return "Block List";
    case OVL_PROFILE_MODE_GRIEF_REPORT:
        return "Report";
    default:
        return "Profile";
    }
}

static const char* mode_subtitle(int mode) {
    switch (mode) {
    case OVL_PROFILE_MODE_FRIEND_REQUEST:
        return "Send Friend Request";
    case OVL_PROFILE_MODE_ADD_TO_BLOCK_LIST:
        return "Add to Block List";
    case OVL_PROFILE_MODE_GRIEF_REPORT:
        return "Report Player";
    default:
        return "Player Profile";
    }
}

void ovl_profile_dialog_draw(void) {
    const profile_param_t* p;
    ovl_vec2_t display, card_min, card_max, zero;
    int first;
    float a;
    if (retro_atomic_load_acquire_int(&status) != RUNNING)
        return;
    if (retro_atomic_load_acquire_int(&middle) & DIRTY)
        front = retro_atomic_exchange_int(&middle, front) & ~DIRTY;
    p = &bufs[front];
    /* Its first frame, as a new NpProfileDialogUi's: faded in from nothing */
    first = !drawn || p->gen != drawn_gen;
    drawn = 1;
    drawn_gen = p->gen;
    if (first)
        open_alpha = 0.0f;
    open_alpha += ovl_delta_time() * FADE_SPEED;
    if (open_alpha > 1.0f)
        open_alpha = 1.0f;
    a = open_alpha;

    display = ovl_display_size();
    card_min = vec2((display.x - CARD_WIDTH) * 0.5f, (display.y - CARD_HEIGHT) * 0.5f);
    card_max = vec2(card_min.x + CARD_WIDTH, card_min.y + CARD_HEIGHT);
    zero = vec2(0.0f, 0.0f);

    /* The screen dimmed */
    ovl_set_next_window_pos(zero, OVL_COND_ALWAYS, zero);
    ovl_set_next_window_size(display, OVL_COND_ALWAYS);
    ovl_push_color(OVL_COLOR_WINDOW_BG, faded(col_overlay, a));
    ovl_push_var_vec2(OVL_VAR_WINDOW_PADDING, zero);
    ovl_begin("##NpProfileDialogOverlay", OVL_WINDOW_NO_DECORATION | OVL_WINDOW_NO_SAVED_SETTINGS |
                                              OVL_WINDOW_NO_MOVE | OVL_WINDOW_NO_INPUTS |
                                              OVL_WINDOW_NO_NAV | OVL_WINDOW_NO_BRING_TO_FRONT |
                                              OVL_WINDOW_NO_FOCUS_ON_APPEARING);
    ovl_end();
    ovl_pop_var(1);
    ovl_pop_color(1);

    ovl_set_next_window_pos(card_min, OVL_COND_ALWAYS, zero);
    ovl_set_next_window_size(vec2(CARD_WIDTH, CARD_HEIGHT), OVL_COND_ALWAYS);
    if (first || !ovl_nav_active())
        ovl_set_next_window_focus();
    ovl_keep_nav_highlight();

    ovl_push_color(OVL_COLOR_WINDOW_BG, faded(col_card_bg, a));
    ovl_push_color(OVL_COLOR_BORDER, faded(col_separator, a));
    ovl_push_color(OVL_COLOR_TEXT, faded(col_text_primary, a));
    ovl_push_color(OVL_COLOR_BUTTON, faded(col_button_normal, a));
    ovl_push_color(OVL_COLOR_BUTTON_HOVERED, faded(col_button_hovered, a));
    ovl_push_color(OVL_COLOR_BUTTON_ACTIVE, faded(col_button_active, a));
    ovl_push_var_vec2(OVL_VAR_WINDOW_PADDING, zero);
    ovl_push_var_float(OVL_VAR_WINDOW_ROUNDING, 6.0f);
    ovl_push_var_float(OVL_VAR_FRAME_ROUNDING, 4.0f);
    ovl_push_var_float(OVL_VAR_WINDOW_BORDER_SIZE, 1.0f);

    if (ovl_begin("##NpProfileDialog",
                  OVL_WINDOW_NO_DECORATION | OVL_WINDOW_NO_SAVED_SETTINGS | OVL_WINDOW_NO_MOVE)) {
        ovl_draw_list_t* dl = ovl_window_draw_list();
        const ovl_vec2_t win = ovl_window_pos();
        const ovl_vec2_t hdr_min = win;
        const ovl_vec2_t hdr_max = vec2(win.x + CARD_WIDTH, win.y + HEADER_HEIGHT);
        const float footer_top = win.y + CARD_HEIGHT - FOOTER_HEIGHT;
        const float id_y = AVATAR_REL_Y + AVATAR_RADIUS + 14.0f;
        const float footer_y = CARD_HEIGHT - FOOTER_HEIGHT;
        const char* title = mode_title(p->mode);
        const char* subtitle = mode_subtitle(p->mode);
        const ovl_col_t top = alpha_scale(col_header_top, a);
        const ovl_col_t bot = alpha_scale(col_header_bot, a);
        ovl_vec2_t size, id_size;

        draw_card_shadow(dl, card_min, card_max, a);

        ovl_add_rect_filled_multicolor(dl, hdr_min, hdr_max, top, top, bot, bot);
        ovl_add_rect_filled(dl, vec2(hdr_min.x, hdr_max.y - 6.0f), hdr_max, bot, 0.0f, 0);
        ovl_add_line(dl, vec2(hdr_min.x + 6.0f, hdr_min.y + 1.0f),
                     vec2(hdr_max.x - 6.0f, hdr_min.y + 1.0f),
                     col32(255, 255, 255, (int)(a * 40.0f)), 1.0f);

        /* The footer's separator */
        ovl_add_line(dl, vec2(win.x, footer_top), vec2(win.x + CARD_WIDTH, footer_top),
                     alpha_scale(col_separator, a), 1.0f);

        draw_avatar_placeholder(dl, vec2(win.x + AVATAR_REL_X, win.y + AVATAR_REL_Y), AVATAR_RADIUS,
                                a);

        /* The header's title */
        size = ovl_calc_text_size(title, NULL, -1.0f);
        ovl_set_cursor_pos(vec2((CARD_WIDTH - size.x) * 0.5f, (HEADER_HEIGHT - size.y) * 0.5f));
        ovl_text(title, NULL);

        /* The online id, or the account's number */
        id_size = ovl_calc_text_size(p->id, NULL, -1.0f);
        ovl_set_cursor_pos(vec2((CARD_WIDTH - id_size.x) * 0.5f, id_y));
        ovl_text(p->id, NULL);

        /* What was asked */
        ovl_push_color(OVL_COLOR_TEXT, faded(col_text_secondary, a));
        size = ovl_calc_text_size(subtitle, NULL, -1.0f);
        ovl_set_cursor_pos(vec2((CARD_WIDTH - size.x) * 0.5f, id_y + id_size.y + 5.0f));
        ovl_text(subtitle, NULL);
        ovl_pop_color(1);

        ovl_push_color(OVL_COLOR_BORDER, faded(col_button_border, a));
        ovl_push_var_float(OVL_VAR_FRAME_BORDER_SIZE, 1.0f);
        ovl_set_cursor_pos(vec2(CARD_WIDTH - CLOSE_BUTTON_W - 16.0f,
                                footer_y + (FOOTER_HEIGHT - CLOSE_BUTTON_H) * 0.5f));
        if (ovl_button("Close##btn", vec2(CLOSE_BUTTON_W, CLOSE_BUTTON_H)))
            ovl_profile_dialog_close(OVL_PROFILE_RESULT_USER_CANCELED);
        ovl_pop_var(1);
        ovl_pop_color(1);

        if (first)
            ovl_set_item_current_nav_focus(0);
    }
    ovl_end();

    ovl_pop_var(4);
    ovl_pop_color(6);
}
