/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

/* Windows, layout and widgets, laid out as ImGui lays them out (imgui.cpp, imgui_widgets.cpp):
 * the same window placement, padding, auto-fit and clipping, the same item cursor (ItemSize,
 * SameLine), the same frames and text placement, in ImGui's dark style. */

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "ovl_internal.h"

#define MAX_WINDOWS 32
#define MAX_STACK 64
#define MAX_GROUPS 16
#define MAX_KEYS OVL_KEY_COUNT

typedef struct group_state {
    ovl_vec2_t cursor_pos, cursor_max, prev_line_size;
    float indent, prev_line_text_base, curr_line_text_base;
    ovl_vec2_t backup_curr_line_size;
} group_state_t;

typedef struct window {
    uint32_t id;
    char name[64];
    unsigned flags;
    int used;               /* made */
    int active, was_active; /* begun this frame, the frame before */
    int hidden_frames;      /* frames left to measure before it is drawn */
    int skip;               /* not drawn this frame, nor its contents built (style alpha 0) */
    int auto_fit_frames;    /* frames left fitting the contents (ImGui's AutoFitFrames) */
    int auto_fit_only_grows;
    int order; /* back to front */
    int appearing;
    ovl_vec2_t pos, size, size_full;
    ovl_vec2_t content_size; /* measured the frame before */
    ovl_vec2_t scroll, scroll_max, scroll_target;
    int scroll_target_set;
    ovl_vec2_t padding;
    float border_size, rounding;
    float font_scale;
    float bg_alpha;
    int bg_alpha_set;
    int is_child;
    struct window* parent;
    ovl_draw_list_t dl;
    float clip[4]; /* the inner clip rectangle */
    /* the drawing context */
    ovl_vec2_t cursor_pos, cursor_start, cursor_max, cursor_prev_line;
    ovl_vec2_t curr_line_size, prev_line_size;
    float curr_line_text_base, prev_line_text_base;
    int is_same_line;
    float indent;
    float group_offset;
    float text_wrap_pos;
    group_state_t groups[MAX_GROUPS];
    int group_count;
    ovl_vec2_t region_max; /* the content region's bottom right */
    float work_max_x;      /* the work rectangle's right */
} window_t;

typedef struct color_mod {
    enum ovl_color idx;
    ovl_vec4_t backup;
} color_mod_t;

typedef struct var_mod {
    enum ovl_style_var var;
    float backup[2];
} var_mod_t;

typedef struct style {
    float alpha, disabled_alpha;
    ovl_vec2_t window_padding;
    float window_rounding, window_border_size;
    ovl_vec2_t window_min_size;
    float child_rounding, child_border_size;
    ovl_vec2_t frame_padding;
    float frame_rounding, frame_border_size;
    ovl_vec2_t item_spacing, item_inner_spacing;
    float indent_spacing;
    float scrollbar_size, scrollbar_rounding, grab_min_size;
    ovl_vec2_t button_text_align;
    ovl_vec4_t colors[OVL_COLOR_COUNT];
} style_t;

static struct {
    int created;
    style_t style;
    float time, delta;
    ovl_vec2_t display;
    int frame;

    window_t windows[MAX_WINDOWS];
    int window_count;
    window_t* stack[MAX_STACK];
    int stack_count;
    window_t* current;
    int next_order;

    ovl_draw_list_t background, foreground;

    color_mod_t colors[MAX_STACK];
    int color_count;
    var_mod_t vars[MAX_STACK];
    int var_count;
    ovl_font_t* fonts[MAX_STACK];
    int font_count;
    ovl_font_t* default_font;
    float font_scale; /* ImGui's FontScaleMain */
    float font_size;  /* the current text size, rounded as ImGui rounds it */
    uint32_t ids[MAX_STACK];
    int id_count;
    float wrap_stack[MAX_STACK];
    int wrap_count;
    int disabled_count;
    float disabled_backup_alpha[MAX_STACK];

    /* SetNextWindow* */
    struct {
        int pos_cond, size_cond, focus, bg_alpha_set;
        ovl_vec2_t pos, pivot, size;
        float bg_alpha;
    } next;

    /* the last item */
    uint32_t last_id;
    ovl_vec2_t last_min, last_max;
    int last_hovered, last_active, last_clicked;

    /* navigation */
    uint32_t nav_id;     /* the focused item */
    uint32_t nav_window; /* the window it is in */
    int nav_cursor_visible;
    int nav_activate; /* the focused item is pressed this frame */
    int nav_activate_down;
    int nav_move_dir; /* 0 none, 1 left, 2 right, 3 up, 4 down */
    uint32_t nav_best_id;
    float nav_best_score;
    uint32_t nav_first_id; /* the first focusable item of the nav window this frame */
    int nav_seen;          /* the focused item was submitted this frame */
    uint32_t nav_default_focus;
    int focus_here; /* SetKeyboardFocusHere: items to skip */

    /* input */
    unsigned char key_down[MAX_KEYS];
    float key_down_time[MAX_KEYS];
    unsigned char key_pressed_event[MAX_KEYS];

    /* the draw data */
    ovl_buf_t out_vtx, out_idx, out_cmds;
    ovl_draw_data_t draw_data;
} g;

/* ------------------------------------------------------------------------------------------ */
/* Helpers */

static ovl_vec2_t v2(float x, float y) {
    ovl_vec2_t r;
    r.x = x;
    r.y = y;
    return r;
}

static ovl_vec4_t v4(float x, float y, float z, float w) {
    ovl_vec4_t r;
    r.x = x;
    r.y = y;
    r.z = z;
    r.w = w;
    return r;
}

static float maxf(float a, float b) {
    return a > b ? a : b;
}

static float minf(float a, float b) {
    return a < b ? a : b;
}

static float truncf_(float v) {
    return (float)(int)v;
}

/* ImGui's ImHashStr: CRC32 (reflected 0xEDB88320) of the string, "###" restarting it */
static uint32_t crc_table[256];

static void crc_init(void) {
    uint32_t i, j;
    for (i = 0; i < 256; i++) {
        uint32_t c = i;
        for (j = 0; j < 8; j++)
            c = (c & 1) ? (c >> 1) ^ 0xEDB88320u : c >> 1;
        crc_table[i] = c;
    }
}

static uint32_t hash_data(const void* data, size_t size, uint32_t seed) {
    uint32_t crc = ~seed;
    const unsigned char* p = (const unsigned char*)data;
    while (size--)
        crc = (crc >> 8) ^ crc_table[(crc & 0xFF) ^ *p++];
    return ~crc;
}

static uint32_t hash_str(const char* s, const char* end, uint32_t seed) {
    const uint32_t seed0 = seed;
    uint32_t crc = ~seed;
    const unsigned char* p = (const unsigned char*)s;
    if (!end)
        end = s + strlen(s);
    while ((const char*)p < end) {
        unsigned char c = *p++;
        if (c == '#' && (const char*)p + 1 < end + 0 && p[0] == '#' && p[1] == '#')
            crc = ~seed0;
        crc = (crc >> 8) ^ crc_table[(crc & 0xFF) ^ c];
    }
    return ~crc;
}

static uint32_t id_seed(void) {
    return g.id_count ? g.ids[g.id_count - 1] : 0;
}

/* The text of a label: up to "##" */
static const char* label_end(const char* label) {
    const char* e = label;
    while (*e && !(e[0] == '#' && e[1] == '#'))
        e++;
    return e;
}

/* ------------------------------------------------------------------------------------------ */
/* Style */

static void style_dark(style_t* s) {
    ovl_vec4_t* c = s->colors;
    s->alpha = 1.0f;
    s->disabled_alpha = 0.60f;
    s->window_padding = v2(8, 8);
    /* The core pushed a rounding of 6 for every window (imgui_core.cpp, Initialize) */
    s->window_rounding = 6.0f;
    s->window_border_size = 1.0f;
    s->window_min_size = v2(32, 32);
    s->child_rounding = 0.0f;
    s->child_border_size = 1.0f;
    s->frame_padding = v2(4, 3);
    s->frame_rounding = 0.0f;
    s->frame_border_size = 0.0f;
    s->item_spacing = v2(8, 4);
    s->item_inner_spacing = v2(4, 4);
    s->indent_spacing = 21.0f;
    s->scrollbar_size = 14.0f;
    s->scrollbar_rounding = 9.0f;
    s->grab_min_size = 12.0f;
    s->button_text_align = v2(0.5f, 0.5f);
    /* ImGui::StyleColorsDark */
    c[OVL_COLOR_TEXT] = v4(1.00f, 1.00f, 1.00f, 1.00f);
    c[OVL_COLOR_TEXT_DISABLED] = v4(0.50f, 0.50f, 0.50f, 1.00f);
    c[OVL_COLOR_WINDOW_BG] = v4(0.06f, 0.06f, 0.06f, 0.94f);
    c[OVL_COLOR_CHILD_BG] = v4(0.00f, 0.00f, 0.00f, 0.00f);
    c[OVL_COLOR_POPUP_BG] = v4(0.08f, 0.08f, 0.08f, 0.94f);
    c[OVL_COLOR_BORDER] = v4(0.43f, 0.43f, 0.50f, 0.50f);
    c[OVL_COLOR_BORDER_SHADOW] = v4(0.00f, 0.00f, 0.00f, 0.00f);
    c[OVL_COLOR_FRAME_BG] = v4(0.16f, 0.29f, 0.48f, 0.54f);
    c[OVL_COLOR_FRAME_BG_HOVERED] = v4(0.26f, 0.59f, 0.98f, 0.40f);
    c[OVL_COLOR_FRAME_BG_ACTIVE] = v4(0.26f, 0.59f, 0.98f, 0.67f);
    c[OVL_COLOR_TITLE_BG] = v4(0.04f, 0.04f, 0.04f, 1.00f);
    c[OVL_COLOR_TITLE_BG_ACTIVE] = v4(0.16f, 0.29f, 0.48f, 1.00f);
    c[OVL_COLOR_TITLE_BG_COLLAPSED] = v4(0.00f, 0.00f, 0.00f, 0.51f);
    c[OVL_COLOR_MENU_BAR_BG] = v4(0.14f, 0.14f, 0.14f, 1.00f);
    c[OVL_COLOR_SCROLLBAR_BG] = v4(0.02f, 0.02f, 0.02f, 0.53f);
    c[OVL_COLOR_SCROLLBAR_GRAB] = v4(0.31f, 0.31f, 0.31f, 1.00f);
    c[OVL_COLOR_SCROLLBAR_GRAB_HOVERED] = v4(0.41f, 0.41f, 0.41f, 1.00f);
    c[OVL_COLOR_SCROLLBAR_GRAB_ACTIVE] = v4(0.51f, 0.51f, 0.51f, 1.00f);
    c[OVL_COLOR_CHECK_MARK] = v4(0.26f, 0.59f, 0.98f, 1.00f);
    c[OVL_COLOR_SLIDER_GRAB] = v4(0.24f, 0.52f, 0.88f, 1.00f);
    c[OVL_COLOR_SLIDER_GRAB_ACTIVE] = v4(0.26f, 0.59f, 0.98f, 1.00f);
    c[OVL_COLOR_BUTTON] = v4(0.26f, 0.59f, 0.98f, 0.40f);
    c[OVL_COLOR_BUTTON_HOVERED] = v4(0.26f, 0.59f, 0.98f, 1.00f);
    c[OVL_COLOR_BUTTON_ACTIVE] = v4(0.06f, 0.53f, 0.98f, 1.00f);
    c[OVL_COLOR_HEADER] = v4(0.26f, 0.59f, 0.98f, 0.31f);
    c[OVL_COLOR_HEADER_HOVERED] = v4(0.26f, 0.59f, 0.98f, 0.80f);
    c[OVL_COLOR_HEADER_ACTIVE] = v4(0.26f, 0.59f, 0.98f, 1.00f);
    c[OVL_COLOR_SEPARATOR] = c[OVL_COLOR_BORDER];
    c[OVL_COLOR_SEPARATOR_HOVERED] = v4(0.10f, 0.40f, 0.75f, 0.78f);
    c[OVL_COLOR_SEPARATOR_ACTIVE] = v4(0.10f, 0.40f, 0.75f, 1.00f);
    c[OVL_COLOR_TAB] = v4(0.18f, 0.35f, 0.58f, 0.86f);
    c[OVL_COLOR_TAB_HOVERED] = c[OVL_COLOR_HEADER_HOVERED];
    c[OVL_COLOR_TAB_SELECTED] = v4(0.20f, 0.41f, 0.68f, 1.00f);
    c[OVL_COLOR_PLOT_HISTOGRAM] = v4(0.90f, 0.70f, 0.00f, 1.00f);
    c[OVL_COLOR_TEXT_SELECTED_BG] = v4(0.26f, 0.59f, 0.98f, 0.35f);
    c[OVL_COLOR_NAV_CURSOR] = v4(0.26f, 0.59f, 0.98f, 1.00f);
    c[OVL_COLOR_MODAL_WINDOW_DIM_BG] = v4(0.80f, 0.80f, 0.80f, 0.35f);
}

ovl_vec4_t ovl_style_color(enum ovl_color idx) {
    return g.style.colors[idx];
}

ovl_col_t ovl_color_u32_vec4(ovl_vec4_t col) {
    col.w *= g.style.alpha;
    return ovl_col_from_vec4(col);
}

ovl_col_t ovl_color_u32(enum ovl_color idx, float alpha_mul) {
    ovl_vec4_t c = g.style.colors[idx];
    c.w *= g.style.alpha * alpha_mul;
    return ovl_col_from_vec4(c);
}

static ovl_col_t color_vec(ovl_vec4_t c) {
    c.w *= g.style.alpha;
    return ovl_col_from_vec4(c);
}

void ovl_push_color(enum ovl_color idx, ovl_vec4_t col) {
    if (g.color_count == MAX_STACK)
        return;
    g.colors[g.color_count].idx = idx;
    g.colors[g.color_count].backup = g.style.colors[idx];
    g.color_count++;
    g.style.colors[idx] = col;
}

void ovl_pop_color(int count) {
    while (count-- > 0 && g.color_count > 0) {
        g.color_count--;
        g.style.colors[g.colors[g.color_count].idx] = g.colors[g.color_count].backup;
    }
}

static float* var_ptr(enum ovl_style_var var, int* n) {
    *n = 1;
    switch (var) {
    case OVL_VAR_ALPHA:
        return &g.style.alpha;
    case OVL_VAR_WINDOW_PADDING:
        *n = 2;
        return &g.style.window_padding.x;
    case OVL_VAR_WINDOW_ROUNDING:
        return &g.style.window_rounding;
    case OVL_VAR_WINDOW_BORDER_SIZE:
        return &g.style.window_border_size;
    case OVL_VAR_CHILD_ROUNDING:
        return &g.style.child_rounding;
    case OVL_VAR_CHILD_BORDER_SIZE:
        return &g.style.child_border_size;
    case OVL_VAR_FRAME_PADDING:
        *n = 2;
        return &g.style.frame_padding.x;
    case OVL_VAR_FRAME_ROUNDING:
        return &g.style.frame_rounding;
    case OVL_VAR_FRAME_BORDER_SIZE:
        return &g.style.frame_border_size;
    case OVL_VAR_ITEM_SPACING:
        *n = 2;
        return &g.style.item_spacing.x;
    case OVL_VAR_ITEM_INNER_SPACING:
        *n = 2;
        return &g.style.item_inner_spacing.x;
    case OVL_VAR_BUTTON_TEXT_ALIGN:
        *n = 2;
        return &g.style.button_text_align.x;
    default:
        return NULL;
    }
}

static void push_var(enum ovl_style_var var, float x, float y) {
    int n;
    float* p = var_ptr(var, &n);
    if (!p || g.var_count == MAX_STACK)
        return;
    g.vars[g.var_count].var = var;
    g.vars[g.var_count].backup[0] = p[0];
    g.vars[g.var_count].backup[1] = n == 2 ? p[1] : 0.0f;
    g.var_count++;
    p[0] = x;
    if (n == 2)
        p[1] = y;
}

void ovl_push_var_float(enum ovl_style_var var, float value) {
    push_var(var, value, value);
}

void ovl_push_var_vec2(enum ovl_style_var var, ovl_vec2_t value) {
    push_var(var, value.x, value.y);
}

void ovl_pop_var(int count) {
    while (count-- > 0 && g.var_count > 0) {
        int n;
        float* p;
        g.var_count--;
        p = var_ptr(g.vars[g.var_count].var, &n);
        p[0] = g.vars[g.var_count].backup[0];
        if (n == 2)
            p[1] = g.vars[g.var_count].backup[1];
    }
}

/* ------------------------------------------------------------------------------------------ */
/* Fonts */

static ovl_font_t* current_font(void) {
    return g.font_count ? g.fonts[g.font_count - 1] : g.default_font;
}

/* ImGui's UpdateCurrentFontSize: the font's size, the global and the window's scale, rounded */
static void update_font_size(void) {
    ovl_font_t* f = current_font();
    float size = f ? ovl_font_size(f) : 13.0f;
    if (g.current)
        size *= g.current->font_scale;
    size *= g.font_scale;
    size = (float)(int)(size + 0.5f);
    g.font_size = size < 1.0f ? 1.0f : size > 512.0f ? 512.0f : size;
}

void ovl_push_font(ovl_font_t* font) {
    if (g.font_count < MAX_STACK)
        g.fonts[g.font_count++] = font;
    update_font_size();
}

void ovl_pop_font(void) {
    if (g.font_count)
        g.font_count--;
    update_font_size();
}

float ovl_font_current_size(void) {
    return g.font_size;
}

float ovl_text_line_height(void) {
    return g.font_size;
}

float ovl_text_line_height_with_spacing(void) {
    return g.font_size + g.style.item_spacing.y;
}

float ovl_frame_height(void) {
    return g.font_size + g.style.frame_padding.y * 2.0f;
}

float ovl_frame_height_with_spacing(void) {
    return ovl_frame_height() + g.style.item_spacing.y;
}

ovl_vec2_t ovl_calc_text_size(const char* text, const char* end, float wrap_width) {
    ovl_vec2_t r;
    if (!current_font())
        return v2(0, 0);
    if (!end)
        end = text + strlen(text);
    if (text == end)
        return v2(0.0f, g.font_size);
    r = ovl_font_calc_text_size(current_font(), g.font_size, wrap_width, text, end);
    /* ImGui rounds the width up, so that text does not clip (CalcTextSize) */
    r.x = (float)(int)(r.x + 0.99999f);
    return r;
}

void ovl_set_window_font_scale(float scale) {
    if (!g.current)
        return;
    g.current->font_scale = scale;
    update_font_size();
}

/* ------------------------------------------------------------------------------------------ */
/* Frames and input */

/* A draw list with room for a window of dialog had now, so that frames allocate nothing */
static void draw_list_make(ovl_draw_list_t* dl) {
    ovl_draw_list_init(dl);
    ovl_buf_reserve(&dl->vtx, 2048);
    ovl_buf_reserve(&dl->idx, 3072);
    ovl_buf_reserve(&dl->cmds, 32);
    ovl_buf_reserve(&dl->clip, 16);
    ovl_buf_reserve(&dl->path, 256);
    ovl_buf_reserve(&dl->temp, 1024);
}

int ovl_create(ovl_font_t* default_font, float font_scale) {
    int i;
    if (g.created)
        return 1;
    if (!ovl_atlas_init())
        return 0;
    crc_init();
    style_dark(&g.style);
    g.default_font = default_font;
    g.font_scale = font_scale;
    for (i = 0; i < MAX_WINDOWS; i++)
        draw_list_make(&g.windows[i].dl);
    draw_list_make(&g.background);
    draw_list_make(&g.foreground);
    ovl_buf_init(&g.out_vtx, sizeof(ovl_vertex_t));
    ovl_buf_init(&g.out_idx, sizeof(uint32_t));
    ovl_buf_init(&g.out_cmds, sizeof(ovl_cmd_t));
    /* Room for a frame of dialogs up front */
    ovl_buf_reserve(&g.out_vtx, 65536);
    ovl_buf_reserve(&g.out_idx, 98304);
    ovl_buf_reserve(&g.out_cmds, 1024);
    g.created = 1;
    return 1;
}

void ovl_destroy(void) {
    int i;
    if (!g.created)
        return;
    for (i = 0; i < MAX_WINDOWS; i++)
        ovl_draw_list_free(&g.windows[i].dl);
    ovl_draw_list_free(&g.background);
    ovl_draw_list_free(&g.foreground);
    ovl_buf_free(&g.out_vtx);
    ovl_buf_free(&g.out_idx);
    ovl_buf_free(&g.out_cmds);
    ovl_atlas_free();
    memset(&g, 0, sizeof(g));
}

void ovl_key_event(enum ovl_key key, int down) {
    if (key <= OVL_KEY_NONE || key >= OVL_KEY_COUNT)
        return;
    if (down && !g.key_down[key])
        g.key_pressed_event[key] = 1;
    g.key_down[key] = (unsigned char)(down != 0);
}

void ovl_char_event(uint32_t codepoint) {
    (void)codepoint;
}

int ovl_is_key_down(enum ovl_key key) {
    return key > OVL_KEY_NONE && key < OVL_KEY_COUNT && g.key_down[key];
}

/* As ImGui's IsKeyPressed: on the press, then repeated after 0.275 s every 0.050 s */
int ovl_is_key_pressed(enum ovl_key key, int repeat) {
    float t, t0;
    if (key <= OVL_KEY_NONE || key >= OVL_KEY_COUNT)
        return 0;
    if (g.key_pressed_event[key])
        return 1;
    if (!repeat || !g.key_down[key])
        return 0;
    t = g.key_down_time[key];
    t0 = t - g.delta;
    if (t <= 0.275f)
        return 0;
    return (int)((t - 0.275f) / 0.050f) > (int)((t0 - 0.275f) / 0.050f) || t0 < 0.275f;
}

float ovl_time(void) {
    return g.time;
}

float ovl_delta_time(void) {
    return g.delta;
}

ovl_vec2_t ovl_display_size(void) {
    return g.display;
}

static int nav_input(int left, int right, int up, int down) {
    if (ovl_is_key_pressed((enum ovl_key)left, 1))
        return 1;
    if (ovl_is_key_pressed((enum ovl_key)right, 1))
        return 2;
    if (ovl_is_key_pressed((enum ovl_key)up, 1))
        return 3;
    if (ovl_is_key_pressed((enum ovl_key)down, 1))
        return 4;
    return 0;
}

void ovl_new_frame(float display_w, float display_h, float delta_time) {
    int i;
    ovl_atlas_begin_frame();
    g.frame++;
    g.display = v2(display_w, display_h);
    g.delta = delta_time > 0.0f ? delta_time : 1.0f / 60.0f;
    g.time += g.delta;
    for (i = 0; i < MAX_KEYS; i++)
        g.key_down_time[i] = g.key_down[i] ? g.key_down_time[i] + g.delta : 0.0f;
    for (i = 0; i < g.window_count; i++) {
        window_t* w = &g.windows[i];
        w->was_active = w->active;
        w->active = 0;
    }
    ovl_draw_list_reset(&g.background, display_w, display_h);
    ovl_draw_list_reset(&g.foreground, display_w, display_h);
    g.stack_count = 0;
    g.current = NULL;
    g.color_count = g.var_count = g.font_count = g.id_count = g.wrap_count = 0;
    g.disabled_count = 0;
    update_font_size();

    /* Navigation: the move asked for this frame resolves among the items submitted */
    g.nav_move_dir = nav_input(OVL_KEY_LEFT, OVL_KEY_RIGHT, OVL_KEY_UP, OVL_KEY_DOWN);
    if (!g.nav_move_dir)
        g.nav_move_dir = nav_input(OVL_KEY_PAD_DPAD_LEFT, OVL_KEY_PAD_DPAD_RIGHT,
                                   OVL_KEY_PAD_DPAD_UP, OVL_KEY_PAD_DPAD_DOWN);
    if (g.nav_move_dir)
        g.nav_cursor_visible = 1;
    g.nav_activate = ovl_is_key_pressed(OVL_KEY_ENTER, 0) ||
                     ovl_is_key_pressed(OVL_KEY_PAD_FACE_DOWN, 0) ||
                     ovl_is_key_pressed(OVL_KEY_SPACE, 0);
    g.nav_activate_down = ovl_is_key_down(OVL_KEY_ENTER) ||
                          ovl_is_key_down(OVL_KEY_PAD_FACE_DOWN) || ovl_is_key_down(OVL_KEY_SPACE);
    g.nav_best_id = 0;
    g.nav_best_score = 1e30f;
    g.nav_first_id = 0;
    g.nav_seen = 0;
    g.nav_default_focus = 0;
    memset(g.key_pressed_event, 0, sizeof(g.key_pressed_event));
}

/* ------------------------------------------------------------------------------------------ */
/* Windows */

static window_t* find_window(const char* name) {
    const uint32_t id = hash_str(name, NULL, 0);
    int i;
    for (i = 0; i < g.window_count; i++)
        if (g.windows[i].used && g.windows[i].id == id)
            return &g.windows[i];
    return NULL;
}

static window_t* create_window(const char* name, unsigned flags) {
    window_t* w = NULL;
    int i;
    for (i = 0; i < g.window_count; i++)
        if (!g.windows[i].used) {
            w = &g.windows[i];
            break;
        }
    if (!w) {
        if (g.window_count == MAX_WINDOWS) {
            /* The longest unused: none drawn this frame or the one before */
            for (i = 0; i < g.window_count; i++)
                if (!g.windows[i].active && !g.windows[i].was_active) {
                    w = &g.windows[i];
                    break;
                }
            if (!w)
                return NULL;
        } else {
            w = &g.windows[g.window_count++];
        }
    }
    {
        ovl_draw_list_t dl = w->dl;
        memset(w, 0, sizeof(*w));
        w->dl = dl;
    }
    w->used = 1;
    w->id = hash_str(name, NULL, 0);
    strncpy(w->name, name, sizeof(w->name) - 1);
    w->flags = flags;
    w->pos = v2(60, 60);
    w->font_scale = 1.0f;
    w->order = g.next_order++;
    /* ImGui's CreateNewWindow: fitted for two frames, and kept fitting when it resizes itself */
    w->auto_fit_frames = 2;
    w->auto_fit_only_grows = !(flags & OVL_WINDOW_ALWAYS_AUTO_RESIZE);
    return w;
}

void ovl_set_next_window_pos(ovl_vec2_t pos, int cond, ovl_vec2_t pivot) {
    g.next.pos = pos;
    g.next.pivot = pivot;
    g.next.pos_cond = cond ? cond : OVL_COND_ALWAYS;
}

void ovl_set_next_window_size(ovl_vec2_t size, int cond) {
    g.next.size = size;
    g.next.size_cond = cond ? cond : OVL_COND_ALWAYS;
}

void ovl_set_next_window_bg_alpha(float alpha) {
    g.next.bg_alpha = alpha;
    g.next.bg_alpha_set = 1;
}

void ovl_set_next_window_focus(void) {
    g.next.focus = 1;
}

static int cond_applies(int cond, const window_t* w, int just_created) {
    if (!cond)
        return 0;
    if (cond & OVL_COND_ALWAYS)
        return 1;
    if ((cond & OVL_COND_ONCE) && just_created)
        return 1;
    if ((cond & OVL_COND_APPEARING) && w->appearing)
        return 1;
    return 0;
}

/* ImGui's CalcWindowMinSize */
static ovl_vec2_t min_size(const window_t* w) {
    ovl_vec2_t mn;
    if (w->flags & OVL_WINDOW_ALWAYS_AUTO_RESIZE)
        mn = v2(4.0f, 4.0f);
    else
        mn = g.style.window_min_size;
    mn.y = maxf(mn.y, maxf(0.0f, g.style.window_rounding - 1.0f));
    return mn;
}

/* ImGui's CalcWindowAutoFitSize for a window without a title bar: the contents and the padding,
 * within the display less its safe area */
static ovl_vec2_t auto_fit_size(const window_t* w, ovl_vec2_t content) {
    ovl_vec2_t s, mn, mx;
    s.x = content.x + w->padding.x * 2.0f;
    s.y = content.y + w->padding.y * 2.0f;
    mn = min_size(w);
    mx = v2(g.display.x - 6.0f, g.display.y - 6.0f);
    s.x = minf(maxf(s.x, minf(mn.x, mx.x)), mx.x);
    s.y = minf(maxf(s.y, minf(mn.y, mx.y)), mx.y);
    /* A scrollbar to come widens it */
    if (!(w->flags & OVL_WINDOW_NO_SCROLLBAR)) {
        const ovl_vec2_t c = v2(maxf(s.x, mn.x), maxf(s.y, mn.y));
        if ((w->flags & OVL_WINDOW_HORIZONTAL_SCROLLBAR) && c.x < content.x + w->padding.x * 2.0f)
            s.y += g.style.scrollbar_size;
        if (c.y < content.y + w->padding.y * 2.0f)
            s.x += g.style.scrollbar_size;
    }
    return s;
}

static void setup_drawing_context(window_t* w) {
    w->cursor_start.x = w->pos.x + w->padding.x - w->scroll.x;
    w->cursor_start.y = w->pos.y + w->padding.y - w->scroll.y;
    w->cursor_pos = w->cursor_prev_line = w->cursor_max = w->cursor_start;
    w->curr_line_size = w->prev_line_size = v2(0, 0);
    w->curr_line_text_base = w->prev_line_text_base = 0.0f;
    w->is_same_line = 0;
    w->indent = w->padding.x - w->scroll.x;
    w->group_offset = 0.0f;
    w->group_count = 0;
    w->text_wrap_pos = -1.0f;
    w->region_max.x = w->pos.x - w->scroll.x + w->padding.x + (w->size.x - w->padding.x * 2.0f);
    w->region_max.y = w->pos.y - w->scroll.y + w->padding.y + (w->size.y - w->padding.y * 2.0f);
    w->work_max_x = truncf_(w->pos.x - w->scroll.x + maxf(w->padding.x, w->border_size)) +
                    maxf(0.0f, w->size.x - w->padding.x * 2.0f);
    /* ImGui's InnerClipRect: up to the middle of the border */
    w->clip[0] = (float)floor(0.5f + w->pos.x + w->border_size * 0.5f);
    w->clip[1] = (float)floor(0.5f + w->pos.y + w->border_size * 0.5f);
    w->clip[2] = (float)floor(w->pos.x + w->size.x - w->border_size * 0.5f);
    w->clip[3] = (float)floor(w->pos.y + w->size.y - w->border_size * 0.5f);
    /* within the display (or the parent's clip, which the draw list's push intersects) */
    w->clip[0] = maxf(w->clip[0], 0.0f);
    w->clip[1] = maxf(w->clip[1], 0.0f);
    w->clip[2] = minf(w->clip[2], g.display.x);
    w->clip[3] = minf(w->clip[3], g.display.y);
}

static int begin_window(const char* name, unsigned flags, window_t* parent, ovl_vec2_t child_size,
                        int child_border) {
    window_t* w = find_window(name);
    int just_created = 0;
    float bg_alpha;
    if (!w) {
        w = create_window(name, flags);
        if (!w)
            return 0;
        just_created = 1;
    }
    w->appearing = !w->was_active;
    if (w->hidden_frames > 0)
        w->hidden_frames--;
    /* As ImGui: a new window is measured for a frame before it is drawn, unless sized */
    if (just_created && !parent &&
        !(g.next.size_cond && g.next.size.x > 0.0f && g.next.size.y > 0.0f))
        w->hidden_frames = 1;
    w->flags = flags;
    w->active = 1;
    w->is_child = parent != NULL;
    w->parent = parent;
    if (g.stack_count < MAX_STACK)
        g.stack[g.stack_count++] = w;
    g.current = w;
    update_font_size();

    if (parent) {
        w->padding = (flags & 0x40000000u) ? g.style.window_padding : v2(0, 0);
        w->border_size = child_border ? g.style.child_border_size : 0.0f;
        w->rounding = g.style.child_rounding;
        w->pos = parent->cursor_pos;
        /* A child's size: given, or what is left of its parent's for 0, less for below 0 */
        {
            ovl_vec2_t avail;
            avail.x = maxf(parent->region_max.x - parent->cursor_pos.x, 1.0f);
            avail.y = maxf(parent->region_max.y - parent->cursor_pos.y, 1.0f);
            w->size.x = child_size.x > 0.0f   ? child_size.x
                        : child_size.x < 0.0f ? maxf(4.0f, avail.x + child_size.x)
                                              : avail.x;
            w->size.y = child_size.y > 0.0f   ? child_size.y
                        : child_size.y < 0.0f ? maxf(4.0f, avail.y + child_size.y)
                                              : avail.y;
            w->size.x = truncf_(w->size.x);
            w->size.y = truncf_(w->size.y);
        }
        w->size_full = w->size;
        w->hidden_frames = parent->hidden_frames;
    } else {
        /* In ImGui's order (Begin): the size asked for, else fitted to the contents of the
         * frame before; then the position asked for, at its pivot of that size; else kept
         * within the display */
        const int size_set = cond_applies(g.next.size_cond, w, just_created);
        const int pos_set = cond_applies(g.next.pos_cond, w, just_created);
        w->padding = g.style.window_padding;
        w->border_size = g.style.window_border_size;
        w->rounding = g.style.window_rounding;
        if (size_set) {
            w->size_full.x = truncf_(g.next.size.x);
            w->size_full.y = truncf_(g.next.size.y);
            w->auto_fit_frames = 0;
        } else if ((flags & OVL_WINDOW_ALWAYS_AUTO_RESIZE) || w->auto_fit_frames > 0) {
            const ovl_vec2_t fit = auto_fit_size(w, w->content_size);
            if ((flags & OVL_WINDOW_ALWAYS_AUTO_RESIZE) || !w->auto_fit_only_grows)
                w->size_full = fit;
            else
                w->size_full = v2(maxf(w->size_full.x, fit.x), maxf(w->size_full.y, fit.y));
        }
        {
            const ovl_vec2_t mn = min_size(w);
            w->size_full.x = maxf(w->size_full.x, mn.x);
            w->size_full.y = maxf(w->size_full.y, mn.y);
        }
        w->size = w->size_full;
        if (pos_set) {
            w->pos.x = truncf_(g.next.pos.x - w->size.x * g.next.pivot.x);
            w->pos.y = truncf_(g.next.pos.y - w->size.y * g.next.pivot.y);
        } else if (g.display.x > 0.0f && g.display.y > 0.0f) {
            /* ImGui's ClampWindowPos: at least DisplayWindowPadding (19) of it on the display */
            w->pos.x = minf(maxf(w->pos.x, 19.0f - w->size.x), g.display.x - 19.0f);
            w->pos.y = minf(maxf(w->pos.y, 19.0f - w->size.y), g.display.y - 19.0f);
        }
        w->pos.x = truncf_(w->pos.x);
        w->pos.y = truncf_(w->pos.y);
        if (w->auto_fit_frames > 0)
            w->auto_fit_frames--;
        if (g.next.focus || (w->appearing && !(flags & OVL_WINDOW_NO_FOCUS_ON_APPEARING))) {
            w->order = g.next_order++;
            if (!(flags & 0x30000u))
                g.nav_window = w->id;
        }
    }
    bg_alpha = 1.0f;
    w->bg_alpha_set = g.next.bg_alpha_set;
    if (g.next.bg_alpha_set)
        bg_alpha = g.next.bg_alpha;
    memset(&g.next, 0, sizeof(g.next));

    /* Scrolling, to what was asked last frame, kept within what there is to scroll */
    w->scroll_max.x = maxf(0.0f, w->content_size.x + w->padding.x * 2.0f - w->size.x);
    w->scroll_max.y = maxf(0.0f, w->content_size.y + w->padding.y * 2.0f - w->size.y);
    if (w->scroll_target_set) {
        w->scroll = w->scroll_target;
        w->scroll_target_set = 0;
    }
    w->scroll.x = (float)floor(minf(maxf(w->scroll.x, 0.0f), w->scroll_max.x));
    w->scroll.y = (float)floor(minf(maxf(w->scroll.y, 0.0f), w->scroll_max.y));

    ovl_draw_list_reset(&w->dl, g.display.x, g.display.y);
    if (parent) {
        /* Drawn into its parent's list, clipped to its own rectangle */
        ovl_push_clip(&w->dl, w->pos, v2(w->pos.x + w->size.x, w->pos.y + w->size.y), 0);
    }
    setup_drawing_context(w);

    /* As ImGui: a window begun with the style's alpha at 0 is not drawn this frame */
    w->skip = g.style.alpha <= 0.0f;
    if (w->hidden_frames == 0 && !w->skip) {
        ovl_vec2_t mx;
        mx.x = w->pos.x + w->size.x;
        mx.y = w->pos.y + w->size.y;
        if (!(flags & OVL_WINDOW_NO_BACKGROUND)) {
            ovl_col_t bg = parent ? ovl_color_u32(OVL_COLOR_CHILD_BG, 1.0f)
                                  : ovl_color_u32(OVL_COLOR_WINDOW_BG, 1.0f);
            if (w->bg_alpha_set) {
                bg = (bg & 0x00FFFFFFu) |
                     ((ovl_col_t)(minf(maxf(bg_alpha, 0.0f), 1.0f) * 255.0f + 0.5f) << 24);
            }
            ovl_add_rect_filled(&w->dl, w->pos, mx, bg, w->rounding, 0);
        }
        if (w->border_size > 0.0f)
            ovl_add_rect(&w->dl, w->pos, mx, ovl_color_u32(OVL_COLOR_BORDER, 1.0f), w->rounding, 0,
                         w->border_size);
    }
    ovl_push_clip(&w->dl, v2(w->clip[0], w->clip[1]), v2(w->clip[2], w->clip[3]), parent != NULL);
    /* Hidden windows still lay out their items, to be measured */
    return !w->skip;
}

int ovl_begin(const char* name, unsigned flags) {
    return begin_window(name, flags, NULL, v2(0, 0), 0);
}

static void end_window(void) {
    window_t* w = g.current;
    if (!w)
        return;
    /* A window skipped keeps the size of what it held (CalcWindowContentSizes) */
    if (!w->skip) {
        w->content_size.x = truncf_(w->cursor_max.x - w->cursor_start.x);
        w->content_size.y = truncf_(w->cursor_max.y - w->cursor_start.y);
    }
    g.stack_count--;
    g.current = g.stack_count ? g.stack[g.stack_count - 1] : NULL;
    update_font_size();
}

void ovl_end(void) {
    end_window();
}

int ovl_begin_child(const char* id, ovl_vec2_t size, int border, unsigned flags) {
    char name[64];
    window_t* parent = g.current;
    uint32_t hid;
    int ok;
    if (!parent)
        return 0;
    hid = hash_str(id, NULL, id_seed());
    /* Named after the parent, as ImGui names children ("parent/child_XXXXXXXX") */
    {
        const char* hex = "0123456789ABCDEF";
        size_t n = strlen(parent->name);
        int i;
        if (n > 40)
            n = 40;
        memcpy(name, parent->name, n);
        name[n++] = '/';
        for (i = 7; i >= 0; i--)
            name[n++] = hex[(hid >> (i * 4)) & 0xF];
        name[n] = 0;
    }
    ok = begin_window(name, flags, parent, size, border);
    g.current->order = parent->order;
    return ok;
}

void ovl_end_child(void) {
    window_t* child = g.current;
    window_t* parent;
    ovl_vec2_t size;
    if (!child || !child->is_child)
        return;
    size = child->size;
    end_window();
    parent = g.current;
    if (!parent)
        return;
    /* The child is an item of its parent's */
    {
        ovl_vec2_t mx;
        mx.x = parent->cursor_pos.x + size.x;
        mx.y = parent->cursor_pos.y + size.y;
        g.last_min = parent->cursor_pos;
        g.last_max = mx;
    }
    {
        /* ItemSize */
        const float line_y1 =
            parent->is_same_line ? parent->cursor_prev_line.y : parent->cursor_pos.y;
        const float line_h =
            maxf(parent->curr_line_size.y, parent->cursor_pos.y - line_y1 + size.y);
        parent->cursor_prev_line.x = parent->cursor_pos.x + size.x;
        parent->cursor_prev_line.y = line_y1;
        parent->cursor_pos.x = truncf_(parent->pos.x + parent->indent + parent->group_offset);
        parent->cursor_pos.y = truncf_(line_y1 + line_h + g.style.item_spacing.y);
        parent->cursor_max.x = maxf(parent->cursor_max.x, parent->cursor_prev_line.x);
        parent->cursor_max.y =
            maxf(parent->cursor_max.y, parent->cursor_pos.y - g.style.item_spacing.y);
        parent->prev_line_size.y = line_h;
        parent->curr_line_size.y = 0.0f;
        parent->prev_line_text_base = parent->curr_line_text_base;
        parent->curr_line_text_base = 0.0f;
        parent->is_same_line = 0;
    }
}

ovl_vec2_t ovl_window_pos(void) {
    return g.current ? g.current->pos : v2(0, 0);
}

ovl_vec2_t ovl_window_size(void) {
    return g.current ? g.current->size : v2(0, 0);
}

void ovl_set_window_pos(ovl_vec2_t pos) {
    window_t* w = g.current;
    ovl_vec2_t d;
    if (!w)
        return;
    pos.x = truncf_(pos.x);
    pos.y = truncf_(pos.y);
    d.x = pos.x - w->pos.x;
    d.y = pos.y - w->pos.y;
    if (d.x == 0.0f && d.y == 0.0f)
        return;
    /* As ImGui's SetWindowPos: the cursor moves with it; what is drawn already, its clip and
     * its work rectangle stay where they were until the next frame */
    w->pos = pos;
    w->cursor_pos.x += d.x;
    w->cursor_pos.y += d.y;
    w->cursor_max.x += d.x;
    w->cursor_max.y += d.y;
    w->cursor_start.x += d.x;
    w->cursor_start.y += d.y;
}

ovl_draw_list_t* ovl_window_draw_list(void) {
    return g.current ? &g.current->dl : &g.foreground;
}

ovl_draw_list_t* ovl_foreground_draw_list(void) {
    return &g.foreground;
}

ovl_draw_list_t* ovl_background_draw_list(void) {
    return &g.background;
}

/* ------------------------------------------------------------------------------------------ */
/* Layout */

/* ImGui's ItemSize */
static void item_size(ovl_vec2_t size, float text_baseline_y) {
    window_t* w = g.current;
    float offset, line_y1, line_h;
    if (!w)
        return;
    offset = text_baseline_y >= 0.0f ? maxf(0.0f, w->curr_line_text_base - text_baseline_y) : 0.0f;
    line_y1 = w->is_same_line ? w->cursor_prev_line.y : w->cursor_pos.y;
    line_h = maxf(w->curr_line_size.y, w->cursor_pos.y - line_y1 + size.y + offset);
    w->cursor_prev_line.x = w->cursor_pos.x + size.x;
    w->cursor_prev_line.y = line_y1;
    w->cursor_pos.x = truncf_(w->pos.x + w->indent + w->group_offset);
    w->cursor_pos.y = truncf_(line_y1 + line_h + g.style.item_spacing.y);
    w->cursor_max.x = maxf(w->cursor_max.x, w->cursor_prev_line.x);
    w->cursor_max.y = maxf(w->cursor_max.y, w->cursor_pos.y - g.style.item_spacing.y);
    w->prev_line_size.y = line_h;
    w->curr_line_size.y = 0.0f;
    w->prev_line_text_base = maxf(w->curr_line_text_base, text_baseline_y);
    w->curr_line_text_base = 0.0f;
    w->is_same_line = 0;
}

ovl_vec2_t ovl_content_region_avail(void) {
    window_t* w = g.current;
    if (!w)
        return v2(0, 0);
    return v2(w->region_max.x - w->cursor_pos.x, w->region_max.y - w->cursor_pos.y);
}

ovl_vec2_t ovl_cursor_pos(void) {
    window_t* w = g.current;
    if (!w)
        return v2(0, 0);
    return v2(w->cursor_pos.x - w->pos.x + w->scroll.x, w->cursor_pos.y - w->pos.y + w->scroll.y);
}

void ovl_set_cursor_pos(ovl_vec2_t pos) {
    window_t* w = g.current;
    if (!w)
        return;
    w->cursor_pos.x = w->pos.x - w->scroll.x + pos.x;
    w->cursor_pos.y = w->pos.y - w->scroll.y + pos.y;
    w->cursor_max.x = maxf(w->cursor_max.x, w->cursor_pos.x);
    w->cursor_max.y = maxf(w->cursor_max.y, w->cursor_pos.y);
}

void ovl_set_cursor_pos_x(float x) {
    window_t* w = g.current;
    if (!w)
        return;
    w->cursor_pos.x = w->pos.x - w->scroll.x + x;
    w->cursor_max.x = maxf(w->cursor_max.x, w->cursor_pos.x);
}

void ovl_set_cursor_pos_y(float y) {
    window_t* w = g.current;
    if (!w)
        return;
    w->cursor_pos.y = w->pos.y - w->scroll.y + y;
    w->cursor_max.y = maxf(w->cursor_max.y, w->cursor_pos.y);
}

ovl_vec2_t ovl_cursor_screen_pos(void) {
    return g.current ? g.current->cursor_pos : v2(0, 0);
}

void ovl_set_cursor_screen_pos(ovl_vec2_t pos) {
    window_t* w = g.current;
    if (!w)
        return;
    w->cursor_pos = pos;
    w->cursor_max.x = maxf(w->cursor_max.x, pos.x);
    w->cursor_max.y = maxf(w->cursor_max.y, pos.y);
}

ovl_vec2_t ovl_cursor_start_pos(void) {
    window_t* w = g.current;
    if (!w)
        return v2(0, 0);
    return v2(w->cursor_start.x - w->pos.x, w->cursor_start.y - w->pos.y);
}

void ovl_same_line(float offset_from_start_x, float spacing) {
    window_t* w = g.current;
    if (!w)
        return;
    if (offset_from_start_x != 0.0f) {
        if (spacing < 0.0f)
            spacing = 0.0f;
        w->cursor_pos.x = w->pos.x - w->scroll.x + offset_from_start_x + spacing +
                          (w->group_count ? w->indent + w->group_offset : 0.0f);
        w->cursor_pos.y = w->cursor_prev_line.y;
    } else {
        if (spacing < 0.0f)
            spacing = g.style.item_spacing.x;
        w->cursor_pos.x = w->cursor_prev_line.x + spacing;
        w->cursor_pos.y = w->cursor_prev_line.y;
    }
    w->curr_line_size = w->prev_line_size;
    w->curr_line_text_base = w->prev_line_text_base;
    w->is_same_line = 1;
}

void ovl_new_line(void) {
    window_t* w = g.current;
    if (!w)
        return;
    if (w->curr_line_size.y > 0.0f)
        item_size(v2(0, 0), -1.0f);
    else
        item_size(v2(0.0f, g.font_size), -1.0f);
}

void ovl_spacing(void) {
    item_size(v2(0, 0), -1.0f);
}

void ovl_dummy(ovl_vec2_t size) {
    window_t* w = g.current;
    if (!w)
        return;
    g.last_min = w->cursor_pos;
    g.last_max = v2(w->cursor_pos.x + size.x, w->cursor_pos.y + size.y);
    item_size(size, -1.0f);
}

void ovl_separator(void) {
    window_t* w = g.current;
    float x1, x2, thickness = 1.0f;
    if (!w)
        return;
    /* ImGui's SeparatorEx, horizontal: from the cursor to the work rectangle's right */
    x1 = w->cursor_pos.x;
    x2 = w->work_max_x;
    {
        const float y1 = w->cursor_pos.y;
        ovl_add_rect_filled(&w->dl, v2(x1, y1), v2(x2, y1 + thickness),
                            ovl_color_u32(OVL_COLOR_SEPARATOR, 1.0f), 0.0f, 0);
        g.last_min = v2(x1, y1);
        g.last_max = v2(x2, y1 + thickness);
        item_size(v2(0.0f, thickness), -1.0f);
    }
}

void ovl_indent(float width) {
    window_t* w = g.current;
    if (!w)
        return;
    w->indent += width != 0.0f ? width : g.style.indent_spacing;
    w->cursor_pos.x = w->pos.x + w->indent + w->group_offset;
}

void ovl_unindent(float width) {
    window_t* w = g.current;
    if (!w)
        return;
    w->indent -= width != 0.0f ? width : g.style.indent_spacing;
    w->cursor_pos.x = w->pos.x + w->indent + w->group_offset;
}

void ovl_begin_group(void) {
    window_t* w = g.current;
    group_state_t* gs;
    if (!w || w->group_count == MAX_GROUPS)
        return;
    gs = &w->groups[w->group_count++];
    gs->cursor_pos = w->cursor_pos;
    gs->cursor_max = w->cursor_max;
    gs->indent = w->indent;
    gs->prev_line_size = w->prev_line_size;
    gs->backup_curr_line_size = w->curr_line_size;
    gs->prev_line_text_base = w->prev_line_text_base;
    gs->curr_line_text_base = w->curr_line_text_base;
    w->group_offset = w->cursor_pos.x - w->pos.x - w->indent;
    w->cursor_max = w->cursor_pos;
    w->curr_line_size.y = 0.0f;
}

void ovl_end_group(void) {
    window_t* w = g.current;
    group_state_t* gs;
    ovl_vec2_t min, max, size;
    if (!w || w->group_count == 0)
        return;
    gs = &w->groups[--w->group_count];
    min = gs->cursor_pos;
    max.x = maxf(w->cursor_max.x, min.x);
    max.y = maxf(w->cursor_max.y, min.y);
    w->cursor_pos = gs->cursor_pos;
    w->cursor_max.x = maxf(gs->cursor_max.x, max.x);
    w->cursor_max.y = maxf(gs->cursor_max.y, max.y);
    w->indent = gs->indent;
    w->group_offset =
        w->group_count ? w->groups[w->group_count - 1].cursor_pos.x - w->pos.x - w->indent : 0.0f;
    w->curr_line_size = gs->backup_curr_line_size;
    w->curr_line_text_base = maxf(w->prev_line_text_base, gs->curr_line_text_base);
    size.x = max.x - min.x;
    size.y = max.y - min.y;
    item_size(size, -1.0f);
    g.last_min = min;
    g.last_max = max;
}

void ovl_push_text_wrap_pos(float wrap_local_pos_x) {
    window_t* w = g.current;
    if (!w || g.wrap_count == MAX_STACK)
        return;
    g.wrap_stack[g.wrap_count++] = w->text_wrap_pos;
    w->text_wrap_pos = wrap_local_pos_x;
}

void ovl_pop_text_wrap_pos(void) {
    window_t* w = g.current;
    if (!w || g.wrap_count == 0)
        return;
    w->text_wrap_pos = g.wrap_stack[--g.wrap_count];
}

/* ImGui's CalcWrapWidthForPos */
static float wrap_width_for(ovl_vec2_t pos, float wrap_pos_x) {
    window_t* w = g.current;
    if (wrap_pos_x < 0.0f || !w)
        return 0.0f;
    if (wrap_pos_x == 0.0f)
        wrap_pos_x = w->work_max_x;
    else if (wrap_pos_x > 0.0f)
        wrap_pos_x += w->pos.x - w->scroll.x;
    return maxf(wrap_pos_x - pos.x, 1.0f);
}

float ovl_calc_wrap_width_for_pos(ovl_vec2_t pos, float wrap_pos_x) {
    return wrap_width_for(pos, wrap_pos_x);
}

void ovl_set_scroll_y(float y) {
    if (!g.current)
        return;
    g.current->scroll_target.x = g.current->scroll.x;
    g.current->scroll_target.y = y;
    g.current->scroll_target_set = 1;
}

void ovl_set_scroll_x(float x) {
    if (!g.current)
        return;
    g.current->scroll_target.x = x;
    g.current->scroll_target.y = g.current->scroll.y;
    g.current->scroll_target_set = 1;
}

float ovl_scroll_y(void) {
    return g.current ? g.current->scroll.y : 0.0f;
}

float ovl_scroll_max_y(void) {
    return g.current ? g.current->scroll_max.y : 0.0f;
}

void ovl_set_scroll_here_y(float center_ratio) {
    window_t* w = g.current;
    float target;
    if (!w)
        return;
    target = w->cursor_prev_line.y - w->pos.y + w->scroll.y - center_ratio * w->size.y;
    ovl_set_scroll_y(target);
}

/* ------------------------------------------------------------------------------------------ */
/* Ids */

uint32_t ovl_get_id(const char* str) {
    return hash_str(str, NULL, id_seed());
}

void ovl_push_id_str(const char* str) {
    if (g.id_count < MAX_STACK)
        g.ids[g.id_count] = hash_str(str, NULL, id_seed()), g.id_count++;
}

void ovl_push_id_int(int value) {
    if (g.id_count < MAX_STACK)
        g.ids[g.id_count] = hash_data(&value, sizeof(value), id_seed()), g.id_count++;
}

void ovl_push_id_ptr(const void* ptr) {
    if (g.id_count < MAX_STACK)
        g.ids[g.id_count] = hash_data(&ptr, sizeof(ptr), id_seed()), g.id_count++;
}

void ovl_pop_id(void) {
    if (g.id_count)
        g.id_count--;
}

/* ------------------------------------------------------------------------------------------ */
/* Items and navigation */

/* An item with an id: kept for navigation, and whether it is focused or pressed */
static int item_add(uint32_t id, ovl_vec2_t min, ovl_vec2_t max) {
    window_t* w = g.current;
    g.last_id = id;
    g.last_min = min;
    g.last_max = max;
    g.last_hovered = g.last_active = g.last_clicked = 0;
    if (!w || !id || g.disabled_count)
        return 1;
    if (w->id == g.nav_window || (w->parent && w->parent->id == g.nav_window)) {
        if (g.focus_here > 0) {
            if (--g.focus_here == 0) {
                g.nav_id = id;
                g.nav_cursor_visible = 1;
            }
        }
        if (!g.nav_first_id)
            g.nav_first_id = id;
        if (id == g.nav_id)
            g.nav_seen = 1;
        if (g.nav_move_dir && g.nav_id && id != g.nav_id) {
            /* Scored against the focused item's rectangle from the frame before */
            static ovl_vec2_t focused_min, focused_max;
            (void)focused_min;
            (void)focused_max;
        }
    }
    return 1;
}

/* Rectangles of the focusable items, from the frame before, for moving between them */
#define NAV_ITEMS 256
static struct {
    uint32_t id;
    ovl_vec2_t min, max;
} nav_items[2][NAV_ITEMS];
static int nav_item_count[2];

static void nav_record(uint32_t id, ovl_vec2_t min, ovl_vec2_t max) {
    const int cur = g.frame & 1;
    if (nav_item_count[cur] < NAV_ITEMS) {
        nav_items[cur][nav_item_count[cur]].id = id;
        nav_items[cur][nav_item_count[cur]].min = min;
        nav_items[cur][nav_item_count[cur]].max = max;
        nav_item_count[cur]++;
    }
}

/* The best item to move to from the focused one in the direction asked, among those of the
 * frame before: nearest along the direction, preferring overlap across it (ImGui's
 * NavScoreItem, simplified) */
static uint32_t nav_resolve(int dir) {
    const int prev = (g.frame & 1) ^ 1;
    int i, have = 0;
    ovl_vec2_t fmin = v2(0, 0), fmax = v2(0, 0);
    uint32_t best = 0;
    float best_score = 1e30f;
    for (i = 0; i < nav_item_count[prev]; i++)
        if (nav_items[prev][i].id == g.nav_id) {
            fmin = nav_items[prev][i].min;
            fmax = nav_items[prev][i].max;
            have = 1;
        }
    if (!have)
        return 0;
    for (i = 0; i < nav_item_count[prev]; i++) {
        const ovl_vec2_t mn = nav_items[prev][i].min, mx = nav_items[prev][i].max;
        float along, across, score;
        if (nav_items[prev][i].id == g.nav_id)
            continue;
        if (dir == 1 || dir == 2) {
            along = dir == 1 ? fmin.x - mx.x : mn.x - fmax.x;
            if (dir == 1 ? mn.x >= fmin.x : mx.x <= fmax.x)
                continue;
            across = maxf(0.0f, maxf(mn.y - fmax.y, fmin.y - mx.y));
        } else {
            along = dir == 3 ? fmin.y - mx.y : mn.y - fmax.y;
            if (dir == 3 ? mn.y >= fmin.y : mx.y <= fmax.y)
                continue;
            across = maxf(0.0f, maxf(mn.x - fmax.x, fmin.x - mx.x));
        }
        score = maxf(along, 0.0f) + across * 4.0f;
        if (score < best_score) {
            best_score = score;
            best = nav_items[prev][i].id;
        }
    }
    return best;
}

/* ImGui's RenderNavCursor around a focused item */
static void render_nav_cursor(ovl_vec2_t min, ovl_vec2_t max, uint32_t id, float rounding) {
    window_t* w = g.current;
    const float thickness = 2.0f, distance = 3.0f + thickness * 0.5f;
    if (id != g.nav_id || !g.nav_cursor_visible || !w)
        return;
    if (rounding < 0.0f)
        rounding = g.style.frame_rounding;
    ovl_push_clip(&w->dl, v2(w->clip[0] - 4.0f, w->clip[1] - 4.0f),
                  v2(w->clip[2] + 4.0f, w->clip[3] + 4.0f), 0);
    ovl_add_rect(&w->dl, v2(min.x - distance, min.y - distance),
                 v2(max.x + distance, max.y + distance), ovl_color_u32(OVL_COLOR_NAV_CURSOR, 1.0f),
                 rounding, 0, thickness);
    ovl_pop_clip(&w->dl);
}

/* Focus and press for a focusable item, from the keyboard or gamepad */
static int button_behavior(uint32_t id, ovl_vec2_t min, ovl_vec2_t max, int* hovered, int* held) {
    window_t* w = g.current;
    int pressed = 0;
    *hovered = *held = 0;
    item_add(id, min, max);
    if (!w || g.disabled_count)
        return 0;
    if (w->id == g.nav_window || (w->parent && w->parent->id == g.nav_window)) {
        nav_record(id, min, max);
        if (id == g.nav_id) {
            *held = g.nav_activate_down;
            pressed = g.nav_activate;
        }
    }
    g.last_hovered = *hovered;
    g.last_active = *held;
    g.last_clicked = pressed;
    return pressed;
}

int ovl_is_item_hovered(void) {
    return g.last_hovered || (g.last_id && g.last_id == g.nav_id && g.nav_cursor_visible);
}

int ovl_is_item_active(void) {
    return g.last_active;
}

int ovl_is_item_clicked(void) {
    return g.last_clicked;
}

int ovl_is_item_focused(void) {
    return g.last_id && g.last_id == g.nav_id;
}

ovl_vec2_t ovl_item_rect_min(void) {
    return g.last_min;
}

ovl_vec2_t ovl_item_rect_max(void) {
    return g.last_max;
}

void ovl_set_item_default_focus(void) {
    if (g.current && g.current->appearing && g.last_id) {
        g.nav_id = g.last_id;
        g.nav_window =
            g.current->is_child && g.current->parent ? g.current->parent->id : g.current->id;
    }
}

void ovl_set_keyboard_focus_here(int offset) {
    g.focus_here = offset + 1;
}

void ovl_begin_disabled(int disabled) {
    if (g.disabled_count < MAX_STACK)
        g.disabled_backup_alpha[g.disabled_count] = g.style.alpha;
    if (disabled && g.disabled_count == 0)
        g.style.alpha *= g.style.disabled_alpha;
    g.disabled_count += disabled ? 1 : 0;
    if (!disabled && g.disabled_count < MAX_STACK)
        g.disabled_backup_alpha[g.disabled_count] = -1.0f;
}

void ovl_end_disabled(void) {
    if (g.disabled_count > 0) {
        g.disabled_count--;
        if (g.disabled_count == 0)
            g.style.alpha = g.disabled_backup_alpha[0];
    }
}

/* ------------------------------------------------------------------------------------------ */
/* Widgets */

static void render_text(ovl_vec2_t pos, const char* text, const char* end, float wrap_width,
                        ovl_col_t col) {
    if (!g.current || !current_font())
        return;
    ovl_add_text(&g.current->dl, current_font(), g.font_size, pos, col, text, end, wrap_width);
}

/* ImGui's RenderTextClipped: aligned in [min, max], clipped to @clip_max */
static void render_text_clipped(ovl_vec2_t min, ovl_vec2_t max, const char* text, const char* end,
                                ovl_vec2_t text_size, ovl_vec2_t align, ovl_col_t col,
                                const float* clip) {
    ovl_vec2_t pos = min;
    window_t* w = g.current;
    int need_clip;
    if (align.x > 0.0f)
        pos.x = maxf(pos.x, pos.x + (max.x - pos.x - text_size.x) * align.x);
    if (align.y > 0.0f)
        pos.y = maxf(pos.y, pos.y + (max.y - pos.y - text_size.y) * align.y);
    need_clip = clip && (pos.x + text_size.x >= clip[2] || pos.y + text_size.y >= clip[3]);
    if (need_clip)
        ovl_push_clip(&w->dl, v2(clip[0], clip[1]), v2(clip[2], clip[3]), 1);
    render_text(pos, text, end, 0.0f, col);
    if (need_clip)
        ovl_pop_clip(&w->dl);
}

/* As ImGui's ItemAdd for an item without an id: one wholly outside the window's clip rectangle
 * is laid out but not drawn */
static int item_clipped(ovl_vec2_t mn, ovl_vec2_t mx) {
    const float* c = ovl_draw_list_clip(&g.current->dl);
    return !(mn.y < c[3] && mx.y > c[1] && mn.x < c[2] && mx.x > c[0]);
}

void ovl_text(const char* text, const char* end) {
    window_t* w = g.current;
    ovl_vec2_t pos, size;
    float wrap_width;
    if (!w)
        return;
    if (!end)
        end = text + strlen(text);
    pos.x = w->cursor_pos.x;
    pos.y = w->cursor_pos.y + w->curr_line_text_base;
    wrap_width = wrap_width_for(w->cursor_pos, w->text_wrap_pos);
    size = ovl_calc_text_size(text, end, wrap_width);
    g.last_id = 0;
    g.last_min = pos;
    g.last_max = v2(pos.x + size.x, pos.y + size.y);
    item_size(size, 0.0f);
    if (item_clipped(g.last_min, g.last_max))
        return;
    render_text(pos, text, end, wrap_width, ovl_color_u32(OVL_COLOR_TEXT, 1.0f));
}

void ovl_text_colored(ovl_vec4_t col, const char* text, const char* end) {
    ovl_push_color(OVL_COLOR_TEXT, col);
    ovl_text(text, end);
    ovl_pop_color(1);
}

void ovl_text_disabled(const char* text, const char* end) {
    ovl_push_color(OVL_COLOR_TEXT, g.style.colors[OVL_COLOR_TEXT_DISABLED]);
    ovl_text(text, end);
    ovl_pop_color(1);
}

void ovl_text_wrapped(const char* text, const char* end) {
    window_t* w = g.current;
    int pushed = 0;
    if (!w)
        return;
    if (w->text_wrap_pos < 0.0f) {
        ovl_push_text_wrap_pos(0.0f);
        pushed = 1;
    }
    ovl_text(text, end);
    if (pushed)
        ovl_pop_text_wrap_pos();
}

void ovl_bullet_text(const char* text, const char* end) {
    window_t* w = g.current;
    ovl_vec2_t pos, size, text_size;
    const float line_h = g.font_size;
    if (!w)
        return;
    if (!end)
        end = text + strlen(text);
    text_size = ovl_calc_text_size(text, end, 0.0f);
    pos = w->cursor_pos;
    size.x =
        g.font_size + (text_size.x > 0.0f ? text_size.x + g.style.frame_padding.x * 2.0f : 0.0f);
    size.y = line_h;
    item_size(size, 0.0f);
    {
        ovl_vec2_t c;
        c.x = pos.x + g.style.frame_padding.x + g.font_size * 0.5f;
        c.y = pos.y + line_h * 0.5f;
        ovl_add_circle_filled(&w->dl, c, g.font_size * 0.20f, ovl_color_u32(OVL_COLOR_TEXT, 1.0f),
                              8);
    }
    render_text(v2(pos.x + g.font_size + g.style.frame_padding.x * 2.0f, pos.y), text, end, 0.0f,
                ovl_color_u32(OVL_COLOR_TEXT, 1.0f));
}

/* ImGui's CalcItemSize */
static ovl_vec2_t calc_item_size(ovl_vec2_t size, float default_w, float default_h) {
    window_t* w = g.current;
    if (size.x == 0.0f)
        size.x = default_w;
    else if (size.x < 0.0f)
        size.x = maxf(4.0f, w->region_max.x - w->cursor_pos.x + size.x);
    if (size.y == 0.0f)
        size.y = default_h;
    else if (size.y < 0.0f)
        size.y = maxf(4.0f, w->region_max.y - w->cursor_pos.y + size.y);
    return size;
}

int ovl_button(const char* label, ovl_vec2_t size_arg) {
    window_t* w = g.current;
    const char* lend;
    ovl_vec2_t label_size, pos, size, mx;
    uint32_t id;
    int hovered, held, pressed;
    enum ovl_color ci;
    if (!w)
        return 0;
    lend = label_end(label);
    id = hash_str(label, NULL, id_seed());
    label_size = ovl_calc_text_size(label, lend, 0.0f);
    pos = w->cursor_pos;
    if (g.style.frame_padding.y < w->curr_line_text_base)
        pos.y += w->curr_line_text_base - g.style.frame_padding.y;
    size = calc_item_size(size_arg, label_size.x + g.style.frame_padding.x * 2.0f,
                          label_size.y + g.style.frame_padding.y * 2.0f);
    mx = v2(pos.x + size.x, pos.y + size.y);
    item_size(size, g.style.frame_padding.y);
    pressed = button_behavior(id, pos, mx, &hovered, &held);
    ci = (held && hovered) || held ? OVL_COLOR_BUTTON_ACTIVE
         : hovered                 ? OVL_COLOR_BUTTON_HOVERED
                                   : OVL_COLOR_BUTTON;
    render_nav_cursor(pos, mx, id, -1.0f);
    ovl_add_rect_filled(&w->dl, pos, mx, ovl_color_u32(ci, 1.0f), g.style.frame_rounding, 0);
    if (g.style.frame_border_size > 0.0f)
        ovl_add_rect(&w->dl, pos, mx, ovl_color_u32(OVL_COLOR_BORDER, 1.0f), g.style.frame_rounding,
                     0, g.style.frame_border_size);
    {
        float clip[4];
        clip[0] = pos.x;
        clip[1] = pos.y;
        clip[2] = mx.x;
        clip[3] = mx.y;
        render_text_clipped(v2(pos.x + g.style.frame_padding.x, pos.y + g.style.frame_padding.y),
                            v2(mx.x - g.style.frame_padding.x, mx.y - g.style.frame_padding.y),
                            label, lend, label_size, g.style.button_text_align,
                            ovl_color_u32(OVL_COLOR_TEXT, 1.0f), clip);
    }
    return pressed;
}

int ovl_invisible_button(const char* str_id, ovl_vec2_t size_arg) {
    window_t* w = g.current;
    ovl_vec2_t pos, size, mx;
    uint32_t id;
    int hovered, held;
    if (!w)
        return 0;
    id = hash_str(str_id, NULL, id_seed());
    pos = w->cursor_pos;
    size = calc_item_size(size_arg, 0.0f, 0.0f);
    mx = v2(pos.x + size.x, pos.y + size.y);
    item_size(size, -1.0f);
    return button_behavior(id, pos, mx, &hovered, &held);
}

int ovl_selectable(const char* label, int selected, ovl_vec2_t size_arg) {
    window_t* w = g.current;
    const char* lend;
    ovl_vec2_t label_size, pos, size, mn, mx;
    uint32_t id;
    int hovered, held, pressed;
    if (!w)
        return 0;
    lend = label_end(label);
    id = hash_str(label, NULL, id_seed());
    label_size = ovl_calc_text_size(label, lend, 0.0f);
    size.x = size_arg.x != 0.0f ? size_arg.x : label_size.x;
    size.y = size_arg.y != 0.0f ? size_arg.y : label_size.y;
    pos = w->cursor_pos;
    pos.y += w->curr_line_text_base;
    item_size(size, 0.0f);
    /* The selection covers the spacing around it, and the whole width when no width is given */
    mn.x = pos.x - truncf_(g.style.item_spacing.x * 0.5f);
    mn.y = pos.y - truncf_(g.style.item_spacing.y * 0.5f);
    mx.x = (size_arg.x == 0.0f ? w->region_max.x : pos.x + size.x) +
           (g.style.item_spacing.x - truncf_(g.style.item_spacing.x * 0.5f));
    mx.y = pos.y + size.y + (g.style.item_spacing.y - truncf_(g.style.item_spacing.y * 0.5f));
    pressed = button_behavior(id, mn, mx, &hovered, &held);
    if (selected || hovered || held || id == g.nav_id) {
        enum ovl_color ci = held && hovered ? OVL_COLOR_HEADER_ACTIVE
                            : hovered || (id == g.nav_id && g.nav_cursor_visible)
                                ? OVL_COLOR_HEADER_HOVERED
                                : OVL_COLOR_HEADER;
        if (selected || hovered || held || (id == g.nav_id && g.nav_cursor_visible))
            ovl_add_rect_filled(&w->dl, mn, mx, ovl_color_u32(ci, 1.0f), 0.0f, 0);
    }
    render_nav_cursor(mn, mx, id, 0.0f);
    render_text(pos, label, lend, 0.0f, ovl_color_u32(OVL_COLOR_TEXT, 1.0f));
    return pressed;
}

void ovl_image(ovl_tex_t tex, ovl_vec2_t size, ovl_vec2_t uv0, ovl_vec2_t uv1) {
    window_t* w = g.current;
    ovl_vec2_t pos, mx;
    if (!w)
        return;
    pos = w->cursor_pos;
    mx = v2(pos.x + size.x, pos.y + size.y);
    item_size(size, -1.0f);
    g.last_id = 0;
    g.last_min = pos;
    g.last_max = mx;
    if (item_clipped(pos, mx))
        return;
    ovl_add_image(&w->dl, tex, pos, mx, uv0, uv1, color_vec(v4(1, 1, 1, 1)));
}

void ovl_progress_bar(float fraction, ovl_vec2_t size_arg, const char* overlay) {
    window_t* w = g.current;
    ovl_vec2_t pos, size, mx, fill_max;
    char buf[32];
    if (!w)
        return;
    pos = w->cursor_pos;
    size = calc_item_size(size_arg, w->region_max.x - pos.x,
                          g.font_size + g.style.frame_padding.y * 2.0f);
    mx = v2(pos.x + size.x, pos.y + size.y);
    item_size(size, g.style.frame_padding.y);
    g.last_id = 0;
    g.last_min = pos;
    g.last_max = mx;
    if (fraction < 0.0f)
        fraction = 0.0f;
    if (fraction > 1.0f)
        fraction = 1.0f;
    ovl_add_rect_filled(&w->dl, pos, mx, ovl_color_u32(OVL_COLOR_FRAME_BG, 1.0f),
                        g.style.frame_rounding, 0);
    {
        const float bs = g.style.frame_border_size;
        const ovl_vec2_t imn = v2(pos.x + bs, pos.y + bs), imx = v2(mx.x - bs, mx.y - bs);
        fill_max = v2(imn.x + (imx.x - imn.x) * fraction, imx.y);
        if (fill_max.x > imn.x)
            ovl_add_rect_filled(&w->dl, imn, fill_max,
                                ovl_color_u32(OVL_COLOR_PLOT_HISTOGRAM, 1.0f),
                                g.style.frame_rounding, 0);
    }
    if (!overlay) {
        int pct = (int)(fraction * 100.0f + 0.01f), n = 0;
        char tmp[8];
        do {
            tmp[n++] = (char)('0' + pct % 10);
            pct /= 10;
        } while (pct && n < 4);
        {
            int i = 0;
            while (n)
                buf[i++] = tmp[--n];
            buf[i++] = '%';
            buf[i] = 0;
        }
        overlay = buf;
    }
    {
        const ovl_vec2_t ts = ovl_calc_text_size(overlay, NULL, 0.0f);
        float clip[4];
        ovl_vec2_t tpos;
        tpos.x =
            minf(fill_max.x + g.style.item_spacing.x, mx.x - ts.x - g.style.item_inner_spacing.x);
        tpos.x = maxf(tpos.x, pos.x);
        tpos.y = pos.y;
        clip[0] = pos.x;
        clip[1] = pos.y;
        clip[2] = mx.x;
        clip[3] = mx.y;
        if (ts.x > 0.0f)
            render_text_clipped(tpos, mx, overlay, NULL, ts, v2(0.0f, 0.5f),
                                ovl_color_u32(OVL_COLOR_TEXT, 1.0f), clip);
    }
}

/* ------------------------------------------------------------------------------------------ */
/* Rendering */

static int window_compare(const void* a, const void* b) {
    const window_t* wa = *(const window_t* const*)a;
    const window_t* wb = *(const window_t* const*)b;
    return wa->order - wb->order;
}

static void append_list(const ovl_draw_list_t* dl) {
    const uint32_t base = (uint32_t)g.out_vtx.size;
    const uint32_t ibase = (uint32_t)g.out_idx.size;
    ovl_vertex_t* v;
    uint32_t* ix;
    ovl_cmd_t* c;
    size_t i;
    if (dl->idx.size == 0)
        return;
    v = (ovl_vertex_t*)ovl_buf_grow(&g.out_vtx, dl->vtx.size);
    ix = (uint32_t*)ovl_buf_grow(&g.out_idx, dl->idx.size);
    c = (ovl_cmd_t*)ovl_buf_grow(&g.out_cmds, dl->cmds.size);
    if (!v || !ix || !c)
        return;
    memcpy(v, dl->vtx.data, dl->vtx.size * sizeof(ovl_vertex_t));
    for (i = 0; i < dl->idx.size; i++)
        ix[i] = ((const uint32_t*)dl->idx.data)[i] + base;
    for (i = 0; i < dl->cmds.size; i++) {
        c[i] = ((const ovl_cmd_t*)dl->cmds.data)[i];
        c[i].idx_offset += ibase;
    }
}

const ovl_draw_data_t* ovl_draw_data(void) {
    return &g.draw_data;
}

const ovl_draw_data_t* ovl_render(void) {
    window_t* order[MAX_WINDOWS];
    int n = 0, i;
    /* Navigation settles: a move goes to the best item found, a focus lost goes to the first */
    if (g.nav_move_dir) {
        const uint32_t to = nav_resolve(g.nav_move_dir);
        if (to)
            g.nav_id = to;
    }
    if (g.nav_window && (!g.nav_id || !g.nav_seen) && !g.nav_move_dir) {
        /* The focused item went: the first of the window instead */
        if (g.nav_first_id)
            g.nav_id = g.nav_first_id;
    }
    nav_item_count[(g.frame & 1) ^ 1] = 0;
    {
        /* The window the focus is in went: none */
        int alive = 0;
        for (i = 0; i < g.window_count; i++)
            if (g.windows[i].used && g.windows[i].active && g.windows[i].id == g.nav_window)
                alive = 1;
        if (!alive) {
            g.nav_window = 0;
            g.nav_id = 0;
            g.nav_cursor_visible = 0;
        }
    }

    g.out_vtx.size = g.out_idx.size = g.out_cmds.size = 0;
    append_list(&g.background);
    for (i = 0; i < g.window_count; i++) {
        window_t* w = &g.windows[i];
        if (w->used && w->active && !w->is_child && w->hidden_frames == 0 && !w->skip)
            order[n++] = w;
    }
    qsort(order, (size_t)n, sizeof(order[0]), window_compare);
    for (i = 0; i < n; i++) {
        int j;
        append_list(&order[i]->dl);
        /* Its children after it, in the order they were begun */
        for (j = 0; j < g.window_count; j++) {
            window_t* c = &g.windows[j];
            if (c->used && c->active && c->is_child && c->parent == order[i])
                append_list(&c->dl);
        }
    }
    append_list(&g.foreground);
    g.draw_data.vtx = (const ovl_vertex_t*)g.out_vtx.data;
    g.draw_data.vtx_count = (uint32_t)g.out_vtx.size;
    g.draw_data.idx = (const uint32_t*)g.out_idx.data;
    g.draw_data.idx_count = (uint32_t)g.out_idx.size;
    g.draw_data.cmds = (const ovl_cmd_t*)g.out_cmds.data;
    g.draw_data.cmd_count = (uint32_t)g.out_cmds.size;
    g.draw_data.display_w = g.display.x;
    g.draw_data.display_h = g.display.y;
    return &g.draw_data;
}
