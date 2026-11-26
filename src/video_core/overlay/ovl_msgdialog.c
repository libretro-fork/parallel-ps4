/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

/* The system's message dialog (libSceMsgDialog), as the old msgdialog_ui.cpp drew
 * it with ImGui: a message centred in a window in the middle of the screen, over a progress bar
 * or one or two buttons, moved between with the d-pad and pressed with cross, circle going to
 * the button that cancels. And the error dialog (libSceErrorDialog), as ime/error_dialog.c
 * drew it: its message, larger, over ImGui's slowly shifting grey, and OK.
 *
 * Three of them: the game's message dialog, the emulator's own (ShowMsgDialog, the save data
 * errors), which waits its turn while the game's shows, and the error dialog; drawn in the
 * order they were opened, as ImGui's layers were.
 *
 * The game's thread opens one, changes its progress and message, and closes it; the overlay's
 * thread draws it and presses its buttons. What is shown goes from one to the other through
 * three buffers, the one between swapped by each side (triple buffering), so that neither waits
 * nor reads what the other writes; the progress is an atomic; a dialog is finished by whichever
 * side claims it first (compare-and-swap), its result published with it. Nothing is allocated. */

#include <math.h>
#include <string.h>

#include <retro_atomic.h>
#include <rthreads/retro_eventcount.h>

#include "ovl_layers.h"

#define MSG_TEXT 1024
#define BUTTON_TEXT 64
#define DIRTY 4

/* The status, CommonDialog::Status's values, and the claim between running and finished */
enum { NONE, INITIALIZED, RUNNING, FINISHED, CLOSING };

typedef struct msg_param {
    unsigned gen; /* which opening it is from */
    int mode, button_type, bar_type, sys_type;
    char msg[MSG_TEXT + 1];
    char button1[BUTTON_TEXT + 1], button2[BUTTON_TEXT + 1];
} msg_param_t;

typedef struct dialog {
    /* The game's thread's: what it set last, and the buffer it writes */
    msg_param_t staged;
    int back;
    /* The buffers; the one between, with DIRTY when the game's thread put it there since */
    msg_param_t bufs[3];
    retro_atomic_int_t middle;
    /* The overlay's thread's: the buffer it reads, and the opening it last drew */
    int front;
    unsigned drawn_gen;
    int drawn;               /* drawn at all */
    int shown, shown_before; /* drawn this frame, and the frame before */

    retro_atomic_int_t status;
    retro_atomic_int_t order; /* when it was opened, among them all */
    retro_atomic_int_t progress;
    retro_atomic_int_t result; /* button | result << 8 */
} dialog_t;

#define DIALOGS 3
static dialog_t dialogs[DIALOGS];
static retro_atomic_int_t opened;
static retro_eventcount_t finished; /* for ShowMsgDialog's wait */
static int finished_made;

static void copy_text(char* dst, size_t size, const char* src) {
    size_t n = src ? strlen(src) : 0;
    if (n >= size)
        n = size - 1;
    if (n)
        memcpy(dst, src, n);
    dst[n] = '\0';
}

/* The game's thread: what it staged, to the overlay's */
static void publish(dialog_t* d) {
    d->bufs[d->back] = d->staged;
    d->back = retro_atomic_exchange_int(&d->middle, d->back | DIRTY) & ~DIRTY;
}

void ovl_msg_dialog_init(void) {
    int i;
    memset(dialogs, 0, sizeof(dialogs));
    for (i = 0; i < DIALOGS; i++) {
        dialogs[i].back = 0;
        retro_atomic_store_relaxed_int(&dialogs[i].middle, 1);
        dialogs[i].front = 2;
    }
    if (!finished_made)
        finished_made = retro_eventcount_init(&finished);
}

void ovl_msg_dialog_shutdown(void) {
    int i;
    /* Anything still showing ends, its waiter let go */
    for (i = 0; i < DIALOGS; i++)
        ovl_msg_dialog_close(i, OVL_MSG_BUTTON_INVALID, OVL_MSG_RESULT_OK);
}

int ovl_msg_dialog_status(int which) {
    const int s = retro_atomic_load_acquire_int(&dialogs[which].status);
    return s == CLOSING ? RUNNING : s;
}

void ovl_msg_dialog_set_status(int which, int status) {
    retro_atomic_store_release_int(&dialogs[which].status, status);
}

void ovl_msg_dialog_open(int which, const ovl_msg_dialog_param_t* p) {
    dialog_t* d = &dialogs[which];
    msg_param_t* s = &d->staged;
    s->gen++;
    s->mode = p->mode;
    s->button_type = p->button_type;
    s->bar_type = p->bar_type;
    s->sys_type = p->sys_type;
    copy_text(s->msg, sizeof(s->msg), p->msg);
    copy_text(s->button1, sizeof(s->button1), p->button1);
    copy_text(s->button2, sizeof(s->button2), p->button2);
    retro_atomic_store_relaxed_int(&d->progress, 0);
    retro_atomic_store_relaxed_int(&d->result, 0);
    retro_atomic_store_relaxed_int(&d->order, retro_atomic_fetch_add_int(&opened, 1));
    publish(d);
    retro_atomic_store_release_int(&d->status, RUNNING);
}

void ovl_msg_dialog_set_message(int which, const char* msg) {
    dialog_t* d = &dialogs[which];
    copy_text(d->staged.msg, sizeof(d->staged.msg), msg);
    publish(d);
}

void ovl_msg_dialog_set_progress(int which, unsigned value) {
    retro_atomic_store_relaxed_int(&dialogs[which].progress, (int)value);
}

void ovl_msg_dialog_add_progress(int which, unsigned delta) {
    retro_atomic_fetch_add_int(&dialogs[which].progress, (int)delta);
}

/* Finished with @button and @result, by whichever side claims it first */
int ovl_msg_dialog_close(int which, int button, int result) {
    dialog_t* d = &dialogs[which];
    if (!retro_atomic_cas_int(&d->status, RUNNING, CLOSING))
        return 0;
    retro_atomic_store_relaxed_int(&d->result, (button & 0xFF) | (result << 8));
    retro_atomic_store_release_int(&d->status, FINISHED);
    if (finished_made)
        retro_eventcount_notify(&finished);
    return 1;
}

void ovl_msg_dialog_result(int which, int* button, int* result) {
    const int r = retro_atomic_load_relaxed_int(&dialogs[which].result);
    *button = r & 0xFF;
    *result = r >> 8;
}

void ovl_msg_dialog_wait(int which) {
    if (!finished_made)
        return;
    for (;;) {
        int key;
        int s = retro_atomic_load_acquire_int(&dialogs[which].status);
        if (s != RUNNING && s != CLOSING)
            return;
        key = retro_eventcount_prepare_wait(&finished);
        s = retro_atomic_load_acquire_int(&dialogs[which].status);
        if (s != RUNNING && s != CLOSING) {
            retro_eventcount_cancel_wait(&finished);
            return;
        }
        retro_eventcount_commit_wait(&finished, key);
    }
}

int ovl_msg_dialog_active(void) {
    int i;
    for (i = 0; i < DIALOGS; i++)
        if (retro_atomic_load_acquire_int(&dialogs[i].status) == RUNNING)
            return 1;
    return 0;
}

/* ------------------------------------------------------------------------------------------ */
/* Drawing, as the old msgdialog_ui.cpp */

#define BUTTON_W 100.0f
#define BUTTON_H 30.0f
#define PROGRESS_BAR_WIDTH 0.8f

static const struct {
    int count;
    const char* text1;
    const char* text2;
} user_buttons[] = {
    {1, "OK", NULL},     {2, "Yes", "No"},   {0, NULL, NULL},       {2, "OK", "Cancel"},
    {0, NULL, NULL},     {1, "Wait", NULL},  {2, "Wait", "Cancel"}, {2, "Yes", "No"},
    {2, "OK", "Cancel"}, {0xFF, NULL, NULL},
};

static const char* const system_texts[] = {
    "No product available in the store.",
    "PSN chat restriction.",
    "User-generated Media restriction",
    NULL,
    "Camera not connected.",
    "Warning: profile picture and name are not set",
};

/* The core's DrawCenteredText (imgui_std.h): measured as wrapped 40 short of @content's width,
 * wrapped at its width, centred in it; the cursor left past it */
void ovl_draw_centered_text(const char* text, const char* end, ovl_vec2_t content) {
    const ovl_vec2_t pos = ovl_cursor_pos();
    const ovl_vec2_t size = ovl_calc_text_size(text, end, content.x - 40.0f);
    ovl_vec2_t p, zero;
    ovl_push_text_wrap_pos(content.x);
    p.x = pos.x + (content.x - size.x) / 2.0f;
    p.y = pos.y + (content.y - size.y) / 2.0f;
    ovl_set_cursor_pos(p);
    ovl_text(text, end);
    ovl_pop_text_wrap_pos();
    p.x = pos.x + content.x;
    p.y = pos.y + content.y;
    ovl_set_cursor_pos(p);
    zero.x = zero.y = 0.0f;
    ovl_dummy(zero);
}

static ovl_vec2_t vec2(float x, float y) {
    ovl_vec2_t v;
    v.x = x;
    v.y = y;
    return v;
}

static void draw_user(int which, const msg_param_t* p, int first) {
    const ovl_vec2_t ws = ovl_window_size();
    const ovl_vec2_t button = vec2(BUTTON_W, BUTTON_H);
    const int type = p->button_type >= 0 && p->button_type <= OVL_MSG_TWO_BUTTONS ? p->button_type
                                                                                  : OVL_MSG_NONE;
    int count = user_buttons[type].count;
    const char* text1 = user_buttons[type].text1;
    const char* text2 = user_buttons[type].text2;
    const int focus_first = type < OVL_MSG_YESNO_FOCUS_NO;
    if (p->msg[0]) {
        const ovl_vec2_t avail = ovl_content_region_avail();
        ovl_draw_centered_text(p->msg, NULL, vec2(avail.x, avail.y - (15.0f + BUTTON_H)));
    }
    if (count == 0xFF) {
        /* Two buttons the game names */
        count = 2;
        text1 = p->button1;
        text2 = p->button2;
    }
    ovl_set_cursor_pos(vec2(ws.x / 2.0f - BUTTON_W / 2.0f * (float)count, ws.y - 10.0f - BUTTON_H));
    ovl_begin_group();
    if (count > 0) {
        /* The first button on the right: the second drawn first */
        if (count == 2) {
            ovl_push_id_int(2);
            if (ovl_button(text2, button)) {
                switch (type) {
                case OVL_MSG_OK_CANCEL:
                case OVL_MSG_WAIT_CANCEL:
                case OVL_MSG_OK_CANCEL_FOCUS_CANCEL:
                    ovl_msg_dialog_close(which, OVL_MSG_BUTTON_INVALID,
                                         OVL_MSG_RESULT_USER_CANCELED);
                    break;
                default:
                    ovl_msg_dialog_close(which, OVL_MSG_BUTTON_2, OVL_MSG_RESULT_OK);
                    break;
                }
            }
            if ((first || ovl_is_key_pressed(OVL_KEY_PAD_FACE_RIGHT, 1)) && !focus_first)
                ovl_set_item_current_nav_focus(0);
            ovl_pop_id();
            ovl_same_line(0.0f, -1.0f);
        }
        ovl_push_id_int(1);
        if (ovl_button(text1, button))
            ovl_msg_dialog_close(which, OVL_MSG_BUTTON_1, OVL_MSG_RESULT_OK);
        if ((first || ovl_is_key_pressed(OVL_KEY_PAD_FACE_RIGHT, 1)) && focus_first)
            ovl_set_item_current_nav_focus(0);
        ovl_pop_id();
        ovl_same_line(0.0f, -1.0f);
    }
    ovl_end_group();
}

static void draw_progress_bar(int which, const msg_param_t* p, int first) {
    const ovl_vec2_t avail = ovl_content_region_avail();
    const unsigned progress = (unsigned)retro_atomic_load_relaxed_int(&dialogs[which].progress);
    const int has_cancel = p->bar_type == OVL_MSG_BAR_PERCENTAGE_CANCEL;
    ovl_vec2_t ws;
    float bar_width;
    ovl_draw_centered_text(p->msg, NULL, avail);
    ws = ovl_window_size();
    ovl_set_cursor_pos(vec2(ws.x * ((1 - PROGRESS_BAR_WIDTH) / 2.0f), ws.y - 10.0f - BUTTON_H));
    bar_width = PROGRESS_BAR_WIDTH * ws.x;
    if (has_cancel)
        bar_width -= BUTTON_W - 10.0f;
    ovl_begin_group();
    ovl_progress_bar((float)progress / 100.0f, vec2(bar_width, BUTTON_H), NULL);
    if (has_cancel) {
        ovl_same_line(0.0f, -1.0f);
        if (ovl_button("Cancel", vec2(BUTTON_W, BUTTON_H)))
            ovl_msg_dialog_close(which, OVL_MSG_BUTTON_INVALID, OVL_MSG_RESULT_USER_CANCELED);
        if (first)
            ovl_set_item_current_nav_focus(0);
    }
    ovl_end_group();
}

static void draw_system_message(int which, const msg_param_t* p, int first) {
    const char* text =
        p->sys_type >= 0 && p->sys_type < (int)(sizeof(system_texts) / sizeof(system_texts[0]))
            ? system_texts[p->sys_type]
            : NULL;
    ovl_vec2_t ws;
    ovl_draw_centered_text(text ? text : "", NULL, ovl_content_region_avail());
    ws = ovl_window_size();
    ovl_set_cursor_pos(vec2(ws.x / 2.0f - BUTTON_W / 2.0f, ws.y - 10.0f - BUTTON_H));
    if (ovl_button("OK", vec2(BUTTON_W, BUTTON_H)))
        ovl_msg_dialog_close(which, OVL_MSG_BUTTON_OK, OVL_MSG_RESULT_OK);
    if (first)
        ovl_set_item_current_nav_focus(0);
}

/* The core's DrawPrettyBackground (imgui_std.h): the window's inside in four greys that shift
 * with time, eased (FastInFastOutCubic) */
static float fast_in_fast_out_cubic(float x) {
    const float c4 = 1.587401f; /* 4^(1/3) */
    const float c05 = 0.7937f;  /* 0.5^(1/3) */
    /* std::pow of floats there; in double here (C89), at most its last bit apart before the
     * grey it gives is truncated */
    return (float)pow((double)(c4 * x - c05), 3.0) + 0.5f;
}

static ovl_col_t gray(int v) {
    const unsigned char c = (unsigned char)v;
    return OVL_COL(c, c, c, 255);
}

void ovl_draw_pretty_background(void) {
    const double time = ovl_time() / 1.5f;
    const float x = ((float)cos(time) + 1.0f) / 2.0f;
    const float d = fast_in_fast_out_cubic(x);
    /* ImLerp of ints */
    const int top_left = (int)(0x13 + (0x05 - 0x13) * d);
    const int top_right = (int)(0x00 + (0x07 - 0x00) * d);
    const int bottom_right = (int)(0x03 + (0x27 - 0x03) * d);
    const int bottom_left = (int)(0x05 + (0x00 - 0x05) * d);
    const ovl_vec2_t pad = ovl_window_padding();
    const ovl_vec2_t cursor = ovl_cursor_screen_pos();
    const ovl_vec2_t avail = ovl_content_region_avail();
    ovl_vec2_t mn, mx;
    mn.x = cursor.x - pad.x;
    mn.y = cursor.y - pad.y;
    mx.x = mn.x + (avail.x + pad.x * 2.0f);
    mx.y = mn.y + (avail.y + pad.y * 2.0f);
    ovl_add_rect_filled_multicolor(ovl_window_draw_list(), mn, mx, gray(top_left), gray(top_right),
                                   gray(bottom_right), gray(bottom_left));
}

static void draw_error(int which, const msg_param_t* p, int first) {
    const ovl_vec2_t ws = ovl_window_size();
    ovl_vec2_t avail;
    ovl_draw_pretty_background();
    ovl_set_window_font_scale(1.3f);
    avail = ovl_content_region_avail();
    ovl_draw_centered_text(p->msg, NULL, vec2(avail.x, avail.y - (15.0f + BUTTON_H)));
    ovl_set_window_font_scale(1.0f);
    ovl_set_cursor_pos(vec2(ws.x / 2.0f - BUTTON_W / 2.0f, ws.y - 10.0f - BUTTON_H));
    if (ovl_button("OK", vec2(BUTTON_W, BUTTON_H)))
        ovl_msg_dialog_close(which, OVL_MSG_BUTTON_INVALID, OVL_MSG_RESULT_OK);
    if (first)
        ovl_set_item_current_nav_focus(0);
}

static void draw_dialog(int which) {
    dialog_t* d = &dialogs[which];
    const msg_param_t* p;
    ovl_vec2_t display, pos, size, pivot;
    int first;
    if (retro_atomic_load_acquire_int(&d->middle) & DIRTY)
        d->front = retro_atomic_exchange_int(&d->middle, d->front) & ~DIRTY;
    p = &d->bufs[d->front];
    /* Its first frame, as a new MsgDialogUi's; and the first after it waited its turn, so that
     * the focus comes back to it */
    first = !d->drawn || p->gen != d->drawn_gen || !d->shown_before;
    d->drawn = 1;
    d->drawn_gen = p->gen;
    d->shown = 1;

    display = ovl_display_size();
    size.x = display.x < 500.0f ? display.x : 500.0f;
    size.y = display.y < 300.0f ? display.y : 300.0f;
    pos.x = display.x / 2.0f;
    pos.y = display.y / 2.0f;
    pivot.x = pivot.y = 0.5f;
    ovl_set_next_window_pos(pos, OVL_COND_ALWAYS, pivot);
    ovl_set_next_window_size(size, OVL_COND_ALWAYS);
    if (first || !ovl_nav_active())
        ovl_set_next_window_focus();
    ovl_keep_nav_highlight();
    if (ovl_begin(which == OVL_MSG_DIALOG_ERROR ? "Error Dialog##ErrorDialog"
                                                : "Message Dialog##MessageDialog",
                  OVL_WINDOW_NO_DECORATION | OVL_WINDOW_NO_SAVED_SETTINGS)) {
        switch (p->mode) {
        case OVL_MSG_MODE_ERROR:
            draw_error(which, p, first);
            break;
        case OVL_MSG_MODE_USER:
            draw_user(which, p, first);
            break;
        case OVL_MSG_MODE_PROGRESS_BAR:
            draw_progress_bar(which, p, first);
            break;
        case OVL_MSG_MODE_SYSTEM:
            draw_system_message(which, p, first);
            break;
        }
    }
    ovl_end();
}

void ovl_msg_dialog_draw(void) {
    int which[DIALOGS], order[DIALOGS], n = 0, i, j;
    for (i = 0; i < DIALOGS; i++) {
        dialogs[i].shown_before = dialogs[i].shown;
        dialogs[i].shown = 0;
    }
    for (i = 0; i < DIALOGS; i++) {
        if (retro_atomic_load_acquire_int(&dialogs[i].status) != RUNNING)
            continue;
        /* The emulator's own waits for the game's */
        if (i == OVL_MSG_DIALOG_EMULATOR &&
            retro_atomic_load_acquire_int(&dialogs[OVL_MSG_DIALOG_GAME].status) == RUNNING)
            continue;
        which[n] = i;
        order[n] = retro_atomic_load_relaxed_int(&dialogs[i].order);
        n++;
    }
    /* In the order they were opened */
    for (i = 1; i < n; i++)
        for (j = i; j > 0 && order[j] - order[j - 1] < 0; j--) {
            int t = which[j];
            which[j] = which[j - 1];
            which[j - 1] = t;
            t = order[j];
            order[j] = order[j - 1];
            order[j - 1] = t;
        }
    for (i = 0; i < n; i++)
        draw_dialog(which[i]);
}
