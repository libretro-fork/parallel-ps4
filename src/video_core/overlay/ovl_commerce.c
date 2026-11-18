/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

/* The store's dialog (libSceNpCommerce's), as core/libraries/np/np_commerce's
 * np_commerce_ui.cpp drew it with ImGui: a window in the middle of the screen naming what the
 * game asked the store for (a category, a product, a code to redeem, a checkout, the downloads,
 * the subscription), the user, the service label, the targets, and one or two buttons: what
 * would have been bought, and Close, which has the focus so that a stray press buys nothing.
 * And the store's icon a game may show at the bottom of the screen: a blue square.
 *
 * The game's thread opens the dialog and may close it; the overlay's thread draws it and closes
 * it when a button is pressed. What is shown goes from one to the other through three buffers,
 * the one between swapped by each side, as the message dialog's; it is finished by whichever
 * side claims it first, its result published with it. The icon is three atomics. Nothing is
 * allocated. */

#include <stdio.h>
#include <string.h>

#include <retro_atomic.h>

#include "ovl_layers.h"

#define TARGET_TEXT 128 /* MAX_LABEL_LEN */
#define NAME_TEXT 64
#define DIRTY 4

/* The status, CommonDialog::Status's values, and the claim between running and finished */
enum { NONE, INITIALIZED, RUNNING, FINISHED, CLOSING };

typedef struct commerce_param {
    unsigned gen; /* which opening it is from */
    int mode;
    int user_id;
    unsigned service_label;
    int realtime_multiplay, other_features; /* of the subscription's features */
    char username[NAME_TEXT + 1];
    int target_count;
    char targets[OVL_COMMERCE_TARGETS][TARGET_TEXT + 1];
} commerce_param_t;

/* The game's thread's: what it opened last, and the buffer it writes */
static commerce_param_t staged;
static int back;
/* The buffers; the one between, with DIRTY when the game's thread put it there since */
static commerce_param_t bufs[3];
static retro_atomic_int_t middle;
/* The overlay's thread's: the buffer it reads and the opening it last drew */
static int front;
static unsigned drawn_gen;
static int drawn;

static retro_atomic_int_t status;
static retro_atomic_int_t result;

/* The store's icon */
static retro_atomic_int_t icon_shown;
static retro_atomic_int_t icon_pos;
static retro_atomic_int_t icon_layout;

static void copy_text(char* dst, size_t size, const char* src) {
    size_t n = 0;
    /* No further than it copies: a label the game did not end is read to its length at most */
    while (src && n < size - 1 && src[n] != '\0')
        n++;
    if (n)
        memcpy(dst, src, n);
    dst[n] = '\0';
}

void ovl_commerce_dialog_init(void) {
    memset(&staged, 0, sizeof(staged));
    memset(bufs, 0, sizeof(bufs));
    back = 0;
    retro_atomic_store_relaxed_int(&middle, 1);
    front = 2;
    drawn = 0;
    drawn_gen = 0;
}

void ovl_commerce_dialog_shutdown(void) {
    ovl_commerce_dialog_close(OVL_COMMERCE_RESULT_USER_CANCELED);
    ovl_store_icon_hide();
}

int ovl_commerce_dialog_status(void) {
    const int s = retro_atomic_load_acquire_int(&status);
    return s == CLOSING ? RUNNING : s;
}

void ovl_commerce_dialog_set_status(int s) {
    retro_atomic_store_release_int(&status, s);
}

void ovl_commerce_dialog_open(const ovl_commerce_dialog_param_t* p) {
    int i, n = p->target_count;
    if (n > OVL_COMMERCE_TARGETS)
        n = OVL_COMMERCE_TARGETS;
    if (n < 0 || !p->targets)
        n = 0;
    staged.gen++;
    staged.mode = p->mode;
    staged.user_id = p->user_id;
    staged.service_label = p->service_label;
    staged.realtime_multiplay = (p->features & 1u) != 0;
    staged.other_features = p->features != 0;
    copy_text(staged.username, sizeof(staged.username), p->username);
    for (i = 0; i < n; i++)
        copy_text(staged.targets[i], sizeof(staged.targets[i]), p->targets[i]);
    staged.target_count = n;
    /* Until a button says otherwise */
    retro_atomic_store_relaxed_int(&result, OVL_COMMERCE_RESULT_USER_CANCELED);
    bufs[back] = staged;
    back = retro_atomic_exchange_int(&middle, back | DIRTY) & ~DIRTY;
    retro_atomic_store_release_int(&status, RUNNING);
}

int ovl_commerce_dialog_close(int r) {
    if (!retro_atomic_cas_int(&status, RUNNING, CLOSING))
        return 0;
    retro_atomic_store_relaxed_int(&result, r);
    retro_atomic_store_release_int(&status, FINISHED);
    return 1;
}

int ovl_commerce_dialog_result(void) {
    return retro_atomic_load_relaxed_int(&result);
}

int ovl_commerce_dialog_active(void) {
    return retro_atomic_load_acquire_int(&status) == RUNNING;
}

void ovl_store_icon_show(int pos) {
    retro_atomic_store_release_int(&icon_pos, pos);
    retro_atomic_store_release_int(&icon_shown, 1);
}

void ovl_store_icon_hide(void) {
    retro_atomic_store_release_int(&icon_shown, 0);
}

void ovl_store_icon_set_layout(int layout) {
    retro_atomic_store_release_int(&icon_layout, layout);
}

int ovl_store_icon_active(void) {
    return retro_atomic_load_acquire_int(&icon_shown) != 0;
}

/* ------------------------------------------------------------------------------------------ */
/* Drawing, as np_commerce_ui.cpp */

#define BUTTON_W 160.0f
#define BUTTON_H 30.0f

static ovl_vec2_t vec2(float x, float y) {
    ovl_vec2_t v;
    v.x = x;
    v.y = y;
    return v;
}

static const char* mode_title(int mode) {
    switch (mode) {
    case OVL_COMMERCE_MODE_PRODUCT:
        return "Product";
    case OVL_COMMERCE_MODE_PRODUCT_CODE:
        return "Redeem Code";
    case OVL_COMMERCE_MODE_CHECKOUT:
        return "Purchase Confirmation";
    case OVL_COMMERCE_MODE_DOWNLOADLIST:
        return "My Downloads";
    case OVL_COMMERCE_MODE_PLUS:
        return "shadNet Plus";
    default:
        return "shadNet Store";
    }
}

static const char* mode_action_label(int mode) {
    switch (mode) {
    case OVL_COMMERCE_MODE_CATEGORY:
    case OVL_COMMERCE_MODE_PRODUCT:
    case OVL_COMMERCE_MODE_CHECKOUT:
        return "Simulate purchase";
    case OVL_COMMERCE_MODE_PRODUCT_CODE:
        return "Redeem code";
    case OVL_COMMERCE_MODE_PLUS:
        return "Join SN Plus";
    default:
        return NULL;
    }
}

static const char* mode_empty_text(int mode) {
    switch (mode) {
    case OVL_COMMERCE_MODE_CATEGORY:
        return "Top store category.";
    case OVL_COMMERCE_MODE_PRODUCT_CODE:
        return "Enter a promotion code.";
    case OVL_COMMERCE_MODE_DOWNLOADLIST:
        return "All downloadable content.";
    case OVL_COMMERCE_MODE_PLUS:
        return "Join shadNet Plus.";
    default:
        return "No product specified.";
    }
}

static const char* mode_description(int mode) {
    switch (mode) {
    case OVL_COMMERCE_MODE_CATEGORY:
        return "Browsing a store category.";
    case OVL_COMMERCE_MODE_PRODUCT:
        return "Viewing a product.";
    case OVL_COMMERCE_MODE_PRODUCT_CODE:
        return "Redeeming a promotion code.";
    case OVL_COMMERCE_MODE_CHECKOUT:
        return "Confirming a purchase.";
    case OVL_COMMERCE_MODE_DOWNLOADLIST:
        return "Viewing owned downloads.";
    case OVL_COMMERCE_MODE_PLUS:
        return "Joining shadNet Plus.";
    default:
        return "";
    }
}

/* A checkout's or the downloads' target, "[service:]product[-sku]": a line for the product,
 * and one each, indented, for the parts it has. 0 when it has neither part: drawn as it is */
static int draw_parsed_target(const char* target) {
    char line[TARGET_TEXT + 16];
    const char* rest = target;
    const char* colon = strchr(target, ':');
    const char* dash;
    size_t service_len = 0, product_len;
    if (colon) {
        service_len = (size_t)(colon - target);
        rest = colon + 1;
    }
    dash = strchr(rest, '-');
    product_len = dash ? (size_t)(dash - rest) : strlen(rest);
    if (!(dash && dash[1] != '\0') && service_len == 0)
        return 0;
    sprintf(line, "Product %.*s", (int)product_len, rest);
    ovl_bullet_text(line, NULL);
    if (dash && dash[1] != '\0') {
        ovl_indent(0.0f);
        sprintf(line, "SKU: %s", dash + 1);
        ovl_text(line, NULL);
        ovl_unindent(0.0f);
    }
    if (service_len != 0) {
        ovl_indent(0.0f);
        sprintf(line, "Service: %.*s", (int)service_len, target);
        ovl_text(line, NULL);
        ovl_unindent(0.0f);
    }
    return 1;
}

void ovl_commerce_dialog_draw(void) {
    const commerce_param_t* p;
    ovl_vec2_t display, pos, size, pivot;
    int first;
    if (retro_atomic_load_acquire_int(&status) != RUNNING)
        return;
    if (retro_atomic_load_acquire_int(&middle) & DIRTY)
        front = retro_atomic_exchange_int(&middle, front) & ~DIRTY;
    p = &bufs[front];
    /* Its first frame, as a new CommerceDialogUi's */
    first = !drawn || p->gen != drawn_gen;
    drawn = 1;
    drawn_gen = p->gen;

    display = ovl_display_size();
    size.x = display.x < 520.0f ? display.x : 520.0f;
    size.y = display.y < 380.0f ? display.y : 380.0f;
    pos.x = display.x / 2.0f;
    pos.y = display.y / 2.0f;
    pivot.x = pivot.y = 0.5f;
    ovl_set_next_window_pos(pos, OVL_COND_ALWAYS, pivot);
    ovl_set_next_window_size(size, OVL_COND_ALWAYS);
    if (first || !ovl_nav_active())
        ovl_set_next_window_focus();
    ovl_keep_nav_highlight();
    if (ovl_begin("CommerceDialog##NpCommerceDialog",
                  OVL_WINDOW_NO_DECORATION | OVL_WINDOW_NO_SAVED_SETTINGS)) {
        char line[TARGET_TEXT + NAME_TEXT + 64];
        const char* action = mode_action_label(p->mode);
        const float n = action ? 2.0f : 1.0f;
        ovl_vec2_t ws;
        float tw, x;
        int i;

        strcpy(line, mode_title(p->mode));
        if (p->target_count == 1 &&
            (p->mode == OVL_COMMERCE_MODE_PRODUCT || p->mode == OVL_COMMERCE_MODE_CATEGORY)) {
            strcat(line, ": ");
            strcat(line, p->targets[0]);
        }
        tw = ovl_calc_text_size(line, NULL, -1.0f).x;
        x = (ovl_window_size().x - tw) * 0.5f;
        ovl_set_cursor_pos_x(x > 10.0f ? x : 10.0f);
        ovl_text(line, NULL);
        ovl_separator();

        ovl_text_wrapped(mode_description(p->mode), NULL);
        ovl_spacing();

        /* The user calling */
        if (p->username[0])
            sprintf(line, "User: %s (id %d)", p->username, p->user_id);
        else
            sprintf(line, "User id: %d", p->user_id);
        ovl_text(line, NULL);
        /* The service label means something for a category, a product, a checkout, the
         * downloads */
        if (p->mode != OVL_COMMERCE_MODE_PRODUCT_CODE && p->mode != OVL_COMMERCE_MODE_PLUS) {
            sprintf(line, "Service label: %u", p->service_label);
            ovl_text(line, NULL);
        }
        if (p->mode == OVL_COMMERCE_MODE_PLUS) {
            sprintf(line, "PS Plus feature: %s",
                    !p->other_features      ? "none"
                    : p->realtime_multiplay ? "Realtime Multiplay"
                                            : "unknown");
            ovl_text(line, NULL);
        }

        ovl_spacing();
        if (p->target_count == 0) {
            ovl_text_wrapped(mode_empty_text(p->mode), NULL);
        } else {
            const int checkout_form =
                p->mode == OVL_COMMERCE_MODE_CHECKOUT || p->mode == OVL_COMMERCE_MODE_DOWNLOADLIST;
            ovl_text_disabled(p->target_count == 1 ? "Target:" : "Targets:", NULL);
            for (i = 0; i < p->target_count; i++) {
                if (checkout_form && draw_parsed_target(p->targets[i]))
                    continue;
                ovl_bullet_text(p->targets[i], NULL);
            }
        }

        ovl_spacing();

        ws = ovl_window_size();
        ovl_set_cursor_pos(vec2(ws.x / 2.0f - (BUTTON_W * n + 10.0f * (n - 1.0f)) / 2.0f,
                                ws.y - 10.0f - BUTTON_H));
        ovl_begin_group();
        if (action) {
            if (ovl_button(action, vec2(BUTTON_W, BUTTON_H)))
                ovl_commerce_dialog_close(OVL_COMMERCE_RESULT_PURCHASED);
            ovl_same_line(0.0f, -1.0f);
        }
        if (ovl_button("Close", vec2(BUTTON_W, BUTTON_H)))
            ovl_commerce_dialog_close(OVL_COMMERCE_RESULT_USER_CANCELED);
        /* The focus on Close: a stray press dismisses, never buys */
        if (first)
            ovl_set_item_current_nav_focus(0);
        ovl_end_group();
    }
    ovl_end();
}

void ovl_store_icon_draw(void) {
    static const ovl_vec4_t white = {1.0f, 1.0f, 1.0f, 1.0f};
    const float icon = 52.0f, pad = 16.0f;
    ovl_vec2_t ds, at;
    float scale, inset_x, inset_y;
    if (!retro_atomic_load_acquire_int(&icon_shown))
        return;
    ds = ovl_display_size();
    switch (retro_atomic_load_acquire_int(&icon_layout)) {
    case OVL_STORE_ICON_LAYOUT_FIXED_90:
        scale = 0.90f;
        break;
    case OVL_STORE_ICON_LAYOUT_FOLLOW_SAFE_AREA:
        scale = 0.95f;
        break;
    default:
        scale = 1.0f;
        break;
    }
    inset_x = ds.x * (1.0f - scale) * 0.5f;
    inset_y = ds.y * (1.0f - scale) * 0.5f;
    switch (retro_atomic_load_acquire_int(&icon_pos)) {
    case OVL_STORE_ICON_LEFT:
        at.x = inset_x + pad;
        break;
    case OVL_STORE_ICON_RIGHT:
        at.x = ds.x - inset_x - icon - pad;
        break;
    default:
        at.x = (ds.x - icon) * 0.5f;
        break;
    }
    at.y = ds.y - inset_y - icon - pad;

    ovl_set_next_window_pos(at, OVL_COND_ALWAYS, vec2(0.0f, 0.0f));
    ovl_set_next_window_size(vec2(icon, icon), OVL_COND_ALWAYS);
    if (ovl_begin("##PsStoreIcon", OVL_WINDOW_NO_DECORATION | OVL_WINDOW_NO_SAVED_SETTINGS |
                                       OVL_WINDOW_NO_INPUTS | OVL_WINDOW_NO_BACKGROUND |
                                       OVL_WINDOW_NO_FOCUS_ON_APPEARING | OVL_WINDOW_NO_NAV)) {
        ovl_draw_list_t* dl = ovl_window_draw_list();
        const ovl_vec2_t p0 = ovl_window_pos();
        const ovl_vec2_t p1 = vec2(p0.x + icon, p0.y + icon);
        const ovl_vec2_t ts = ovl_calc_text_size("SN", NULL, -1.0f);
        ovl_add_rect_filled(dl, p0, p1, OVL_COL(0, 55, 145, 230), 10.0f, 0);
        ovl_add_rect(dl, p0, p1, OVL_COL(255, 255, 255, 210), 10.0f, 0, 2.0f);
        /* White whatever the style's text is */
        ovl_push_color(OVL_COLOR_TEXT, white);
        ovl_render_text(vec2(p0.x + (icon - ts.x) * 0.5f, p0.y + (icon - ts.y) * 0.5f), "SN", NULL);
        ovl_pop_color(1);
    }
    ovl_end();
}
