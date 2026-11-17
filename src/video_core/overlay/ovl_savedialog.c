/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

/* The save data dialog (libSceSaveDataDialog), as core/libraries/save_data/dialog/
 * savedatadialog_ui.cpp drew it with ImGui: a window in the middle of the screen, over ImGui's
 * slowly shifting grey, titled Save, Load or Delete; in it the game's saves, each its icon, its
 * title, subtitle, date and size, in a list the d-pad moves down and scrolls, a save chosen
 * with cross, Back below it; or one save over a message and its buttons, or a progress bar.
 *
 * The game's thread builds what is shown (savedatadialog_ui.cpp reads the saves) and opens it;
 * the overlay's thread draws it and presses its buttons. What it is goes from one to the other
 * through three buffers, the one between swapped by each side (triple buffering), so that
 * neither waits nor reads what the other writes. Its texts and saves are lent: the game's
 * thread builds them where the buffer it fills next says, which the overlay's thread reads only
 * once it is opened, and lets them go only when it builds there again; that buffer is never the
 * one drawn. The progress is an atomic; the dialog is finished by whichever side claims it
 * first (compare-and-swap), its result published with it. Nothing is allocated. */

#include <math.h>
#include <string.h>

#include <retro_atomic.h>

#include "ovl_layers.h"

#define DIRTY 4

/* The status, CommonDialog::Status's values, and the claim between running and finished */
enum { NONE, INITIALIZED, RUNNING, FINISHED, CLOSING };

typedef struct save_param {
    unsigned gen; /* which opening it is from */
    ovl_save_dialog_param_t p;
} save_param_t;

static struct {
    /* The game's thread's: the buffer it fills next, and the openings */
    int back;
    unsigned gen;
    save_param_t bufs[3];
    /* The one between, with DIRTY when the game's thread put it there since */
    retro_atomic_int_t middle;
    /* The overlay's thread's: the buffer it reads, and the opening it last drew */
    int front;
    unsigned drawn_gen;
    int drawn;

    retro_atomic_int_t status;
    retro_atomic_int_t progress;
    retro_atomic_int_t result; /* button | result << 8 | (the save chosen + 1) << 16 */
} dlg;

void ovl_save_dialog_init(void) {
    memset(&dlg, 0, sizeof(dlg));
    dlg.back = 0;
    retro_atomic_store_relaxed_int(&dlg.middle, 1);
    dlg.front = 2;
}

void ovl_save_dialog_shutdown(void) {
    ovl_save_dialog_close(OVL_SAVE_BUTTON_INVALID, OVL_SAVE_RESULT_OK);
}

int ovl_save_dialog_status(void) {
    const int s = retro_atomic_load_acquire_int(&dlg.status);
    return s == CLOSING ? RUNNING : s;
}

void ovl_save_dialog_set_status(int status) {
    retro_atomic_store_release_int(&dlg.status, status);
}

int ovl_save_dialog_back(void) {
    return dlg.back;
}

int ovl_save_dialog_open(const ovl_save_dialog_param_t* p) {
    const int opened = dlg.back;
    save_param_t* b = &dlg.bufs[opened];
    b->gen = ++dlg.gen;
    b->p = *p;
    retro_atomic_store_relaxed_int(&dlg.progress, 0);
    retro_atomic_store_relaxed_int(&dlg.result, 0);
    dlg.back = retro_atomic_exchange_int(&dlg.middle, opened | DIRTY) & ~DIRTY;
    retro_atomic_store_release_int(&dlg.status, RUNNING);
    return opened;
}

void ovl_save_dialog_set_progress(unsigned value) {
    retro_atomic_store_relaxed_int(&dlg.progress, (int)value);
}

void ovl_save_dialog_add_progress(unsigned delta) {
    retro_atomic_fetch_add_int(&dlg.progress, (int)delta);
}

static int finish(int button, int result, int item) {
    if (!retro_atomic_cas_int(&dlg.status, RUNNING, CLOSING))
        return 0;
    retro_atomic_store_relaxed_int(&dlg.result,
                                   (button & 0xFF) | (result & 0xFF) << 8 | (item + 1) << 16);
    retro_atomic_store_release_int(&dlg.status, FINISHED);
    return 1;
}

int ovl_save_dialog_close(int button, int result) {
    return finish(button, result, -1);
}

void ovl_save_dialog_result(int* button, int* result, int* item) {
    const int r = retro_atomic_load_relaxed_int(&dlg.result);
    *button = r & 0xFF;
    *result = (r >> 8) & 0xFF;
    *item = (r >> 16) - 1;
}

int ovl_save_dialog_active(void) {
    return retro_atomic_load_acquire_int(&dlg.status) == RUNNING;
}

/* ------------------------------------------------------------------------------------------ */
/* Drawing, as savedatadialog_ui.cpp */

#define ICON_W 152.0f
#define ICON_H 85.0f
#define ICON_PAD_X 8.0f
#define ICON_PAD_Y 2.0f
#define BUTTON_W 100.0f
#define BUTTON_H 30.0f
#define FOOTER_HEIGHT (BUTTON_H + 15.0f)
#define PROGRESS_BAR_WIDTH 0.8f
#define TEXT_SPACING 0.95f

static ovl_vec2_t vec2(float x, float y) {
    ovl_vec2_t v;
    v.x = x;
    v.y = y;
    return v;
}

static ovl_vec4_t vec4(float x, float y, float z, float w) {
    ovl_vec4_t v;
    v.x = x;
    v.y = y;
    v.z = z;
    v.w = w;
    return v;
}

/* As ImGui's ColorConvertU32ToFloat4, for PushStyleColor of an IM_COL32 */
static ovl_vec4_t vec4_of_col(unsigned r, unsigned g, unsigned b, unsigned a) {
    const float s = 1.0f / 255.0f;
    return vec4((float)r * s, (float)g * s, (float)b * s, (float)a * s);
}

static void text_at(ovl_vec2_t pos, float x, float y, const char* text) {
    if (text && text[0])
        ovl_render_text(vec2(pos.x + x, pos.y + y), text, NULL);
}

/* One save: a grey box, its icon (a lighter one for none), its texts beside it; when it can be
 * chosen, a button, outlined while focused */
static void draw_item(const ovl_save_dialog_param_t* p, int index, int clickable) {
    const ovl_save_item_t* item = &p->items[index];
    ovl_draw_list_t* dl = ovl_window_draw_list();
    const ovl_vec2_t avail = ovl_content_region_avail();
    const ovl_vec2_t outer = ovl_cursor_screen_pos();
    const ovl_vec2_t pos = vec2(outer.x + ICON_PAD_X, outer.y + ICON_PAD_Y);
    const ovl_vec2_t size = vec2(avail.x - ICON_PAD_X, ICON_H + ICON_PAD_Y);
    const ovl_vec2_t bb_min = outer;
    const ovl_vec2_t bb_max = vec2(outer.x + size.x + ICON_PAD_X, outer.y + size.y + ICON_PAD_Y);
    const uint32_t id = ovl_get_id_int(index);
    int hovered = 0, held;
    float pos_x, pos_y, font_size;

    ovl_item_size(vec2(size.x, size.y + ICON_PAD_Y * 2.0f), -1.0f);
    if (!ovl_item_add(id, bb_min, bb_max))
        return;
    ovl_add_rect_filled(dl, vec2(bb_min.x + ICON_PAD_X, bb_min.y + ICON_PAD_Y),
                        vec2(bb_max.x - ICON_PAD_X, bb_max.y - ICON_PAD_Y),
                        ovl_color_u32_vec4(vec4(0.3f, 0.3f, 0.3f, 0.3f)), 0.0f, 0);
    if (clickable) {
        if (ovl_button_behavior(id, &hovered, &held))
            finish(OVL_SAVE_BUTTON_INVALID, OVL_SAVE_RESULT_OK, index);
        ovl_render_nav_cursor(bb_min, bb_max, id);
    }
    if (item->icon)
        ovl_add_image(dl, (ovl_tex_t)item->icon, pos, vec2(pos.x + ICON_W, pos.y + ICON_H),
                      vec2(0.0f, 0.0f), vec2(1.0f, 1.0f), 0xFFFFFFFFu);
    else
        ovl_add_rect_filled(dl, pos, vec2(pos.x + ICON_W, pos.y + ICON_H),
                            ovl_color_u32_vec4(vec4(0.7f, 0.7f, 0.7f, 0.7f)), 0.0f, 0);

    pos_x = ICON_W + 5.0f;
    pos_y = 2.0f;
    if (item->title && item->title[0]) {
        ovl_set_window_font_scale(1.5f);
        text_at(pos, pos_x, pos_y, item->title);
        pos_y += ovl_font_current_size() * TEXT_SPACING;
    }
    ovl_set_window_font_scale(1.1f);
    font_size = ovl_font_current_size();

    if (item->corrupted) {
        /* Blinking red, as std::abs(std::sin(ImGui's time)) */
        const int bright = (int)fabs(sin(ovl_time()) * 0.15f * 255.0f);
        pos_y -= font_size * TEXT_SPACING * 0.3f;
        ovl_push_color(OVL_COLOR_TEXT, vec4_of_col((unsigned)(bright + 216), (unsigned)bright,
                                                   (unsigned)bright, 0xFF));
        text_at(pos, pos_x, pos_y, "Corrupted");
        ovl_pop_color(1);
        pos_y += font_size * TEXT_SPACING * 0.8f;
    }

    if (p->style == OVL_SAVE_STYLE_TITLE_SUBTITLE_DATESIZE) {
        text_at(pos, pos_x, pos_y, item->subtitle);
        pos_y += font_size * TEXT_SPACING;
    }

    {
        float width = 0.0f;
        if (item->date && item->date[0]) {
            width = ovl_calc_text_size(item->date, NULL, 0.0f).x + 15.0f;
            text_at(pos, pos_x, pos_y, item->date);
        }
        text_at(pos, pos_x + width, pos_y, item->size);
        pos_y += font_size * TEXT_SPACING;
    }

    if (p->style == OVL_SAVE_STYLE_TITLE_DATESIZE_SUBTITLE)
        text_at(pos, pos_x, pos_y, item->subtitle);

    ovl_set_window_font_scale(1.0f);

    if (hovered)
        ovl_add_rect(dl, bb_min, bb_max, ovl_color_u32(OVL_COLOR_BORDER, 1.0f), 0.0f, 0, 2.0f);
}

/* The one save a message is about: the first of the list, else the new one */
static void draw_front_item(const ovl_save_dialog_param_t* p) {
    if (p->item_count > p->has_new_item)
        draw_item(p, p->has_new_item, 0);
    else if (p->has_new_item)
        draw_item(p, 0, 0);
}

static void draw_list(const ovl_save_dialog_param_t* p, int first) {
    const float footer = 30.0f;
    ovl_vec2_t avail = ovl_content_region_avail();
    int i;
    avail.y -= footer + 1.0f;
    ovl_begin_child("##ScrollingRegion", avail, OVL_CHILD_NAV_FLATTENED, 0);
    for (i = 0; i < p->item_count; i++)
        draw_item(p, i, 1);
    if (first && p->focus >= 0)
        ovl_set_item_current_nav_focus(ovl_get_id_int(p->focus));
    ovl_end_child();

    ovl_separator();
    if (p->enable_back != 0) {
        const float pad = 7.0f;
        const ovl_vec2_t txt = ovl_calc_text_size("Back", NULL, 0.0f);
        const ovl_vec2_t button =
            vec2((txt.x > 100.0f ? txt.x : 100.0f) + pad * 2.0f, footer - pad);
        ovl_set_cursor_pos_x(ovl_content_region_avail().x - button.x);
        if (ovl_button("Back", button))
            finish(OVL_SAVE_BUTTON_INVALID, OVL_SAVE_RESULT_USER_CANCELED, -1);
        if (ovl_is_key_pressed(OVL_KEY_PAD_FACE_RIGHT, 1))
            ovl_set_item_current_nav_focus(0);
    }
}

/* Before firmware 4.50, OK alone and No answer as cancelled */
static void answer(const ovl_save_dialog_param_t* p, int cancels, int button) {
    if (cancels && p->old_firmware)
        finish(OVL_SAVE_BUTTON_INVALID, OVL_SAVE_RESULT_USER_CANCELED, -1);
    else
        finish(button, OVL_SAVE_RESULT_OK, -1);
}

static void draw_user(const ovl_save_dialog_param_t* p, int first) {
    const ovl_vec2_t ws = ovl_window_size();
    const ovl_vec2_t button = vec2(BUTTON_W, BUTTON_H);
    const int type = p->user_buttons;
    const int has_btn = type != OVL_SAVE_BUTTONS_NONE;
    draw_front_item(p);
    if (p->msg && p->msg[0]) {
        const ovl_vec2_t avail = ovl_content_region_avail();
        if (p->user_error)
            ovl_push_color(OVL_COLOR_TEXT, vec4(1.0f, 0.0f, 0.0f, 1.0f));
        ovl_draw_centered_text(p->msg, NULL,
                               vec2(avail.x, avail.y - (has_btn ? FOOTER_HEIGHT : 0.0f)));
        if (p->user_error)
            ovl_pop_color(1);
    }
    if (has_btn) {
        int count = 1;
        if (type == OVL_SAVE_BUTTONS_YESNO || type == OVL_SAVE_BUTTONS_OKCANCEL)
            ++count;
        ovl_set_cursor_pos(
            vec2(ws.x / 2.0f - BUTTON_W / 2.0f * (float)count, ws.y - FOOTER_HEIGHT + 5.0f));
        ovl_begin_group();
        if (type == OVL_SAVE_BUTTONS_YESNO) {
            if (ovl_button("Yes", button))
                finish(OVL_SAVE_BUTTON_YES, OVL_SAVE_RESULT_OK, -1);
            ovl_same_line(0.0f, -1.0f);
            if (ovl_button("No", button))
                answer(p, 1, OVL_SAVE_BUTTON_NO);
            if (first || ovl_is_key_pressed(OVL_KEY_PAD_FACE_RIGHT, 1))
                ovl_set_item_current_nav_focus(0);
        } else {
            if (ovl_button("OK", button))
                answer(p, type == OVL_SAVE_BUTTONS_OK, OVL_SAVE_BUTTON_OK);
            if (first)
                ovl_set_item_current_nav_focus(0);
            if (type == OVL_SAVE_BUTTONS_OKCANCEL) {
                ovl_same_line(0.0f, -1.0f);
                if (ovl_button("Cancel", button))
                    finish(OVL_SAVE_BUTTON_INVALID, OVL_SAVE_RESULT_USER_CANCELED, -1);
                if (ovl_is_key_pressed(OVL_KEY_PAD_FACE_RIGHT, 1))
                    ovl_set_item_current_nav_focus(0);
            }
        }
        ovl_end_group();
    }
}

static void draw_system_message(const ovl_save_dialog_param_t* p, int first) {
    const ovl_vec2_t button = vec2(BUTTON_W, BUTTON_H);
    ovl_vec2_t ws;
    int count = 1;
    draw_front_item(p);
    ws = ovl_window_size();
    if (p->msg && p->msg[0]) {
        const ovl_vec2_t avail = ovl_content_region_avail();
        ovl_draw_centered_text(p->msg, NULL, vec2(avail.x, avail.y - FOOTER_HEIGHT));
    }
    /* OK is drawn even when it is hidden, as it was; only the buttons' place counts it out */
    if (p->hide_ok)
        --count;
    if (p->show_no || p->show_cancel)
        ++count;
    ovl_set_cursor_pos(
        vec2(ws.x / 2.0f - BUTTON_W / 2.0f * (float)count, ws.y - FOOTER_HEIGHT + 5.0f));
    ovl_begin_group();
    if (ovl_button(p->show_no ? "Yes" : "OK", button))
        answer(p, p->return_cancel, OVL_SAVE_BUTTON_YES);
    ovl_same_line(0.0f, -1.0f);
    if (p->show_no) {
        if (ovl_button("No", button))
            answer(p, 1, OVL_SAVE_BUTTON_NO);
    } else if (p->show_cancel) {
        if (ovl_button("Cancel", button))
            finish(OVL_SAVE_BUTTON_INVALID, OVL_SAVE_RESULT_USER_CANCELED, -1);
    }
    if (first || ovl_is_key_pressed(OVL_KEY_PAD_FACE_RIGHT, 1))
        ovl_set_item_current_nav_focus(0);
    ovl_end_group();
}

static void draw_error_code(const ovl_save_dialog_param_t* p, int first) {
    ovl_vec2_t ws;
    draw_front_item(p);
    ws = ovl_window_size();
    if (p->msg && p->msg[0]) {
        const ovl_vec2_t avail = ovl_content_region_avail();
        ovl_draw_centered_text(p->msg, NULL, vec2(avail.x, avail.y - FOOTER_HEIGHT));
    }
    ovl_set_cursor_pos(vec2(ws.x / 2.0f - BUTTON_W / 2.0f, ws.y - FOOTER_HEIGHT + 5.0f));
    if (ovl_button("OK", vec2(BUTTON_W, BUTTON_H)))
        answer(p, 1, OVL_SAVE_BUTTON_OK);
    if (first)
        ovl_set_item_current_nav_focus(0);
}

static void draw_progress_bar(const ovl_save_dialog_param_t* p) {
    const ovl_vec2_t ws = ovl_window_size();
    const unsigned progress = (unsigned)retro_atomic_load_relaxed_int(&dlg.progress);
    draw_front_item(p);
    if (p->msg && p->msg[0]) {
        const ovl_vec2_t avail = ovl_content_region_avail();
        ovl_draw_centered_text(p->msg, NULL, vec2(avail.x, avail.y - FOOTER_HEIGHT));
    }
    ovl_set_cursor_pos(vec2(ws.x * ((1 - PROGRESS_BAR_WIDTH) / 2.0f), ws.y - FOOTER_HEIGHT + 5.0f));
    ovl_progress_bar((float)progress / 100.0f, vec2(PROGRESS_BAR_WIDTH * ws.x, BUTTON_H), NULL);
}

void ovl_save_dialog_draw(void) {
    const ovl_save_dialog_param_t* p;
    ovl_vec2_t display, size, pos, pivot;
    int first;
    if (retro_atomic_load_acquire_int(&dlg.status) != RUNNING)
        return;
    if (retro_atomic_load_acquire_int(&dlg.middle) & DIRTY)
        dlg.front = retro_atomic_exchange_int(&dlg.middle, dlg.front) & ~DIRTY;
    p = &dlg.bufs[dlg.front].p;
    /* Its first frame, as a new SaveDialogUi's */
    first = !dlg.drawn || dlg.bufs[dlg.front].gen != dlg.drawn_gen;
    dlg.drawn = 1;
    dlg.drawn_gen = dlg.bufs[dlg.front].gen;

    display = ovl_display_size();
    if (p->mode == OVL_SAVE_MODE_LIST) {
        size.x = display.x - 200.0f < 1100.0f ? display.x - 200.0f : 1100.0f;
        size.y = display.y - 100.0f < 700.0f ? display.y - 100.0f : 700.0f;
    } else {
        size.x = display.x < 600.0f ? display.x : 600.0f;
        size.y = display.y < 300.0f ? display.y : 300.0f;
    }
    pos.x = display.x / 2.0f;
    pos.y = display.y / 2.0f;
    pivot.x = pivot.y = 0.5f;
    ovl_set_next_window_pos(pos, OVL_COND_ALWAYS, pivot);
    ovl_set_next_window_size(size, OVL_COND_ALWAYS);
    if (first || !ovl_nav_active())
        ovl_set_next_window_focus();
    if (ovl_begin("Save Data Dialog##SaveDataDialog",
                  OVL_WINDOW_NO_DECORATION | OVL_WINDOW_NO_SAVED_SETTINGS)) {
        ovl_draw_pretty_background();

        ovl_separator();
        /* The title larger */
        ovl_set_window_font_scale(1.7f);
        switch (p->type) {
        case OVL_SAVE_TYPE_SAVE:
            ovl_text("Save", NULL);
            break;
        case OVL_SAVE_TYPE_LOAD:
            ovl_text("Load", NULL);
            break;
        case OVL_SAVE_TYPE_DELETE:
            ovl_text("Delete", NULL);
            break;
        }
        ovl_set_window_font_scale(1.0f);
        ovl_separator();

        ovl_begin_group();
        switch (p->mode) {
        case OVL_SAVE_MODE_LIST:
            draw_list(p, first);
            break;
        case OVL_SAVE_MODE_USER:
            draw_user(p, first);
            break;
        case OVL_SAVE_MODE_SYSTEM:
            draw_system_message(p, first);
            break;
        case OVL_SAVE_MODE_ERROR:
            draw_error_code(p, first);
            break;
        case OVL_SAVE_MODE_PROGRESS_BAR:
            draw_progress_bar(p);
            break;
        default:
            ovl_text_colored(vec4(1.0f, 0.0f, 0.0f, 1.0f), "!!!Unknown dialog mode!!!", NULL);
            break;
        }
        ovl_end_group();
    }
    ovl_end();
}
