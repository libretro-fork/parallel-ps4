/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

/* Windows, layout and widgets, laid out as ImGui lays them out (imgui.cpp, imgui_widgets.cpp):
 * the same window placement, padding, auto-fit and clipping, the same item cursor (ItemSize,
 * SameLine), the same frames and text placement, in ImGui's dark style. */

#include <float.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include <retro_atomic.h>

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
    int auto_fit[2];        /* frames left fitting each axis to the contents (AutoFitFramesX/Y) */
    int auto_fit_only_grows;
    int order; /* back to front */
    int appearing;
    ovl_vec2_t pos, size, size_full;
    ovl_vec2_t content_size; /* measured the frame before */
    ovl_vec2_t scroll, scroll_max;
    float scroll_target[2];       /* ImGui's ScrollTarget: FLT_MAX for none */
    float scroll_target_ratio[2]; /* and its ScrollTargetCenterRatio */
    int scrollbar_y;              /* a vertical scrollbar, ImGui's ScrollbarY */
    float deco_x2;                /* its width: ImGui's DecoOuterSizeX2 */
    float inner[4];               /* ImGui's InnerRect: inside the decorations */
    unsigned child_flags;
    uint32_t child_id;
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
    /* navigation, as ImGui's window keeps it */
    uint32_t nav_last_id;
    float nav_rect_rel[4];         /* the focused item, from the cursor's start */
    ovl_vec2_t nav_pref_pos_rel;   /* NavPreferredScoringPosRel */
    int nav_items, nav_items_next; /* NavLayersActiveMask: it has items to move to */
    int focus_order;
    float font_ref_size;
    int id_stack_base;
    /* The child begun last in it this frame: whether there is one, and its rectangle */
    int has_prev_child;
    float prev_child[4];
} window_t;

/* A result of a navigation request, as ImGui's ImGuiNavItemData */
typedef struct nav_item {
    window_t* window;
    uint32_t id;
    float rect_rel[4];
    float dist_box, dist_center, dist_axial;
} nav_item_t;

typedef struct key_data {
    unsigned char down;
    float duration, duration_prev; /* -1 when up */
    float analog;
} key_data_t;

enum { DIR_NONE = -1, DIR_LEFT, DIR_RIGHT, DIR_UP, DIR_DOWN };
enum { SRC_NONE, SRC_MOUSE, SRC_KEYBOARD, SRC_GAMEPAD };

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
    double time; /* as ImGui's, accumulated in double */
    float delta;
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

    /* navigation, as ImGui's (its main layer) */
    window_t* nav_window;
    uint32_t nav_id;
    int nav_id_is_alive;
    int nav_cursor_visible;      /* NavCursorVisible */
    int nav_highlight_under_nav; /* NavHighlightItemUnderNav */
    int nav_source;              /* NavInputSource */
    int nav_init_request, nav_init_from_move;
    nav_item_t nav_init_result;
    int nav_move_submitted, nav_move_scoring;
    int nav_move_dir;
    float nav_scoring_rect[4];
    nav_item_t nav_move_local, nav_move_other;
    uint32_t nav_activate_id, nav_activate_down_id, nav_activate_pressed_id;
    uint32_t nav_just_moved_to_id;
    uint32_t nav_highlight_activated_id;
    float nav_highlight_activated_timer;
    int nav_any_request;
    int nav_active; /* io.NavActive */
    int focus_order_next;

    /* the active item, as ImGui's ActiveId */
    uint32_t active_id, active_id_is_alive, active_id_previous_frame;
    window_t* active_id_window;
    int active_id_source;
    int active_id_just_activated;     /* ActiveIdIsJustActivated */
    unsigned active_id_using_nav_dir; /* ActiveIdUsingNavDirMask: a bit a direction */

    /* SetNextItemWidth */
    int next_item_width_set;
    float next_item_width;

    /* the text field being edited, as ImGui's InputTextState: one at a time */
    struct {
        uint32_t id;
        int text_len;
        int cursor, select_start, select_end; /* in bytes */
        float cursor_anim;
        float scroll_x;
        char text[OVL_INPUT_TEXT_MAX + 1];
    } input;

    /* input */
    key_data_t keys[MAX_KEYS];
    struct {
        unsigned char set, down;
        float analog;
    } key_events[MAX_KEYS];
    int gamepad_connected;

    /* the draw data */
    ovl_buf_t out_vtx, out_idx, out_cmds;
    ovl_draw_data_t draw_data;
} g;

static void item_size(ovl_vec2_t size, float text_baseline_y);
static int item_add(uint32_t id, ovl_vec2_t min, ovl_vec2_t max);
static void item_add_no_nav(uint32_t id, ovl_vec2_t min, ovl_vec2_t max);
static void render_nav_cursor(ovl_vec2_t min, ovl_vec2_t max, uint32_t id, float rounding);
static float lerpf(float a, float b, float t);
static ovl_vec2_t calc_next_scroll(const window_t* w);
static void set_scroll_from_pos_y(window_t* w, float local_y, float center_ratio);
static void set_scroll_x(window_t* w, float x);
static void set_scroll_y(window_t* w, float y);

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

/* ImGui's ImHashStr as the core built it (with SSE 4.2, so CRC32C: reflected 0x82F63B78) of the
 * string, "###" restarting it and left out */
static uint32_t crc_table[256];

static void crc_init(void) {
    uint32_t i, j;
    for (i = 0; i < 256; i++) {
        uint32_t c = i;
        for (j = 0; j < 8; j++)
            c = (c & 1) ? (c >> 1) ^ 0x82F63B78u : c >> 1;
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
        if (c == '#' && end - (const char*)p >= 2 && p[0] == '#' && p[1] == '#') {
            crc = ~seed0;
            p += 2;
            continue;
        }
        crc = (crc >> 8) ^ crc_table[(crc & 0xFF) ^ c];
    }
    return ~crc;
}

/* The top of the id stack, which a window starts at its own id, as ImGui's */
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
    /* ImLerp(FrameBg, FrameBgHovered, 0.65f) */
    c[OVL_COLOR_CHECKBOX_SELECTED_BG] =
        v4(lerpf(c[OVL_COLOR_FRAME_BG].x, c[OVL_COLOR_FRAME_BG_HOVERED].x, 0.65f),
           lerpf(c[OVL_COLOR_FRAME_BG].y, c[OVL_COLOR_FRAME_BG_HOVERED].y, 0.65f),
           lerpf(c[OVL_COLOR_FRAME_BG].z, c[OVL_COLOR_FRAME_BG_HOVERED].z, 0.65f),
           lerpf(c[OVL_COLOR_FRAME_BG].w, c[OVL_COLOR_FRAME_BG_HOVERED].w, 0.65f));
    c[OVL_COLOR_INPUT_TEXT_CURSOR] = c[OVL_COLOR_TEXT];
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
    ovl_key_analog_event(key, down, down ? 1.0f : 0.0f);
}

void ovl_key_analog_event(enum ovl_key key, int down, float value) {
    if (key <= OVL_KEY_NONE || key >= OVL_KEY_COUNT)
        return;
    g.key_events[key].set = 1;
    g.key_events[key].down = (unsigned char)(down != 0);
    g.key_events[key].analog = value;
}

static retro_atomic_int_t pad_capture;

void ovl_pad_capture_acquire(void) {
    retro_atomic_fetch_add_int(&pad_capture, 1);
}

void ovl_pad_capture_release(void) {
    int n = retro_atomic_load_acquire_int(&pad_capture);
    while (n > 0 && !retro_atomic_cas_int(&pad_capture, n, n - 1))
        n = retro_atomic_load_acquire_int(&pad_capture);
}

int ovl_pad_captured(void) {
    return retro_atomic_load_relaxed_int(&pad_capture) > 0;
}

void ovl_set_gamepad_connected(int connected) {
    g.gamepad_connected = connected != 0;
}

void ovl_char_event(uint32_t codepoint) {
    (void)codepoint;
}

static int is_gamepad_key(int key) {
    return key >= OVL_KEY_PAD_FACE_DOWN && key < OVL_KEY_COUNT;
}

int ovl_is_key_down(enum ovl_key key) {
    return key > OVL_KEY_NONE && key < OVL_KEY_COUNT && g.keys[key].down;
}

/* ImGui's CalcTypematicRepeatAmount */
static int repeat_amount(float t0, float t1, float delay, float rate) {
    int c0, c1;
    if (t1 == 0.0f)
        return 1;
    if (t0 >= t1)
        return 0;
    if (rate <= 0.0f)
        return t0 < delay && t1 >= delay;
    c0 = t0 < delay ? -1 : (int)((t0 - delay) / rate);
    c1 = t1 < delay ? -1 : (int)((t1 - delay) / rate);
    return c1 - c0;
}

/* ImGui's IsKeyPressed with a repeat rate: io.KeyRepeatDelay 0.275 s and KeyRepeatRate
 * 0.050 s, scaled as its RepeatRateNavMove (0.72, 0.80) when @nav_move */
static int key_pressed(int key, int repeat, int nav_move) {
    const key_data_t* k = &g.keys[key];
    const float delay = nav_move ? 0.275f * 0.72f : 0.275f;
    const float rate = nav_move ? 0.050f * 0.80f : 0.050f;
    const float t = k->duration;
    if (!k->down || t < 0.0f)
        return 0;
    if (t == 0.0f)
        return 1;
    return repeat && t > delay && repeat_amount(t - g.delta, t, delay, rate) > 0;
}

int ovl_is_key_pressed(enum ovl_key key, int repeat) {
    if (key <= OVL_KEY_NONE || key >= OVL_KEY_COUNT)
        return 0;
    return key_pressed(key, repeat, 0);
}

double ovl_time(void) {
    return g.time;
}

float ovl_delta_time(void) {
    return g.delta;
}

ovl_vec2_t ovl_display_size(void) {
    return g.display;
}

/* ------------------------------------------------------------------------------------------ */
/* Navigation, as ImGui 1.92's (imgui.cpp, KEYBOARD/GAMEPAD NAVIGATION): focus, moving between
 * items with the d-pad, pressing with cross, letting go with circle */

typedef struct rect {
    float x0, y0, x1, y1;
} rect_t;

static rect_t rect_of(const float* r) {
    rect_t o;
    o.x0 = r[0];
    o.y0 = r[1];
    o.x1 = r[2];
    o.y1 = r[3];
    return o;
}

static void rect_store(float* r, rect_t o) {
    r[0] = o.x0;
    r[1] = o.y0;
    r[2] = o.x1;
    r[3] = o.y1;
}

/* ImGui's WindowRectAbsToRel and back: from the window's cursor start */
static rect_t rect_abs_to_rel(const window_t* w, rect_t r) {
    r.x0 -= w->cursor_start.x;
    r.x1 -= w->cursor_start.x;
    r.y0 -= w->cursor_start.y;
    r.y1 -= w->cursor_start.y;
    return r;
}

static rect_t rect_rel_to_abs(const window_t* w, rect_t r) {
    r.x0 += w->cursor_start.x;
    r.x1 += w->cursor_start.x;
    r.y0 += w->cursor_start.y;
    r.y1 += w->cursor_start.y;
    return r;
}

static window_t* root_window(window_t* w) {
    while (w && w->is_child && w->parent)
        w = w->parent;
    return w;
}

/* RootWindowForNav: a window is its own, but for children flattened into their parent */
static window_t* root_for_nav(window_t* w) {
    while (w && w->is_child && w->parent && (w->child_flags & OVL_CHILD_NAV_FLATTENED))
        w = w->parent;
    return w;
}

static void nav_item_clear(nav_item_t* r) {
    r->window = NULL;
    r->id = 0;
    r->dist_box = r->dist_center = r->dist_axial = FLT_MAX;
}

static void nav_update_any_request(void) {
    g.nav_any_request = g.nav_move_scoring || g.nav_init_request;
}

static void clear_preferred_pos(int axis) {
    window_t* r;
    if (!g.nav_window)
        return;
    r = root_for_nav(g.nav_window);
    if (axis == 0)
        r->nav_pref_pos_rel.x = FLT_MAX;
    else
        r->nav_pref_pos_rel.y = FLT_MAX;
}

static void set_nav_window(window_t* w) {
    g.nav_window = w;
    g.nav_init_request = g.nav_move_submitted = g.nav_move_scoring = 0;
    nav_update_any_request();
}

static void set_nav_id(uint32_t id, rect_t rect_rel) {
    g.nav_id = id;
    g.nav_window->nav_last_id = id;
    rect_store(g.nav_window->nav_rect_rel, rect_rel);
    clear_preferred_pos(0);
    clear_preferred_pos(1);
}

/* ImGui's SetNavCursorVisibleAfterMove (navigation on for both, the cursor shown by itself) */
static void set_nav_cursor_visible_after_move(void) {
    if (g.nav_window && (g.nav_window->flags & OVL_WINDOW_NO_NAV_INPUTS))
        g.nav_cursor_visible = 0;
    else
        g.nav_cursor_visible = 1;
    g.nav_highlight_under_nav = 1;
}

static void set_active_id(uint32_t id, window_t* w) {
    g.active_id_just_activated = g.active_id != id;
    /* What the item before claimed is let go */
    g.active_id_using_nav_dir = 0;
    g.active_id = id;
    g.active_id_window = w;
    if (id)
        g.active_id_source =
            (g.nav_activate_id == id || g.nav_just_moved_to_id == id) ? g.nav_source : SRC_MOUSE;
    g.active_id_is_alive = id;
}

static void clear_active_id(void) {
    set_active_id(0, NULL);
}

/* ImGui's FocusWindow: the focus, and the window brought to the front */
static void focus_window(window_t* w) {
    window_t* root = root_window(w);
    if (g.nav_window != w) {
        set_nav_window(w);
        g.nav_id = w ? w->nav_last_id : 0;
        g.nav_id_is_alive = 0;
    }
    if (g.active_id && g.active_id_window && root_window(g.active_id_window) != root)
        clear_active_id();
    if (!w)
        return;
    root->focus_order = ++g.focus_order_next;
    if (!(w->flags & OVL_WINDOW_NO_BRING_TO_FRONT) && !(root->flags & OVL_WINDOW_NO_BRING_TO_FRONT))
        root->order = g.next_order++;
}

/* When the focused window has gone: the front-most window left that takes input, as ImGui's
 * FocusTopMostWindowUnderOne */
static void focus_top_most(void) {
    window_t* best = NULL;
    int i;
    for (i = 0; i < g.window_count; i++) {
        window_t* w = &g.windows[i];
        if (!w->used || w->is_child || !w->was_active)
            continue;
        if ((w->flags & (OVL_WINDOW_NO_MOUSE_INPUTS | OVL_WINDOW_NO_NAV_INPUTS)) ==
            (OVL_WINDOW_NO_MOUSE_INPUTS | OVL_WINDOW_NO_NAV_INPUTS))
            continue;
        if (!best || w->focus_order > best->focus_order)
            best = w;
    }
    focus_window(best);
}

/* ImGui's NavInitWindow: the first item to be focused, found as the items come */
static void nav_init_window(window_t* w) {
    rect_t zero;
    if (w->flags & OVL_WINDOW_NO_NAV_INPUTS) {
        g.nav_id = 0;
        return;
    }
    if (!w->is_child || w->nav_last_id == 0) {
        memset(&zero, 0, sizeof(zero));
        set_nav_id(0, zero);
        g.nav_init_request = 1;
        g.nav_init_from_move = 0;
        g.nav_init_result.id = 0;
        nav_update_any_request();
    } else {
        g.nav_id = w->nav_last_id;
    }
}

/* ImGui's SetFocusID */
static void set_focus_id(uint32_t id, window_t* w) {
    if (g.nav_window != w)
        set_nav_window(w);
    g.nav_id = id;
    w->nav_last_id = id;
    if (g.last_id == id) {
        rect_t r;
        r.x0 = g.last_min.x;
        r.y0 = g.last_min.y;
        r.x1 = g.last_max.x;
        r.y1 = g.last_max.y;
        rect_store(w->nav_rect_rel, rect_abs_to_rel(w, r));
    }
    if (id == g.active_id_is_alive)
        g.nav_id_is_alive = 1;
    if (g.active_id_source == SRC_KEYBOARD || g.active_id_source == SRC_GAMEPAD)
        g.nav_highlight_under_nav = 1;
    else
        g.nav_cursor_visible = 0;
    clear_preferred_pos(0);
    clear_preferred_pos(1);
}

static void nav_init_apply(void) {
    rect_t r;
    if (!g.nav_window)
        return;
    if (g.nav_id != g.nav_init_result.id)
        g.nav_just_moved_to_id = g.nav_init_result.id;
    r = rect_of(g.nav_init_result.rect_rel);
    set_nav_id(g.nav_init_result.id, r);
    g.nav_id_is_alive = 1;
    if (g.nav_init_from_move)
        set_nav_cursor_visible_after_move();
}

/* ImGui's ScrollToRectEx, for the vertical axis (the overlays' windows do not scroll across):
 * the item kept in view at the nearest edge, or centred when the window is appearing; the parent
 * scrolled too for an item in a child. Its scroll change. */
enum { SCROLL_KEEP_EDGE_Y = 1, SCROLL_KEEP_CENTER_Y = 2, SCROLL_ALWAYS_CENTER_Y = 4 };

static float scroll_to_rect_y(window_t* w, rect_t item, int flags) {
    const float sr_y0 = w->inner[1] - 1.0f, sr_y1 = w->inner[3] + 1.0f;
    const int in_flags = flags;
    int fully, can_fully;
    float delta;
    if (!(flags & (SCROLL_KEEP_EDGE_Y | SCROLL_KEEP_CENTER_Y | SCROLL_ALWAYS_CENTER_Y)))
        flags |= w->appearing ? SCROLL_ALWAYS_CENTER_Y : SCROLL_KEEP_EDGE_Y;
    fully = item.y0 >= sr_y0 && item.y1 <= sr_y1;
    can_fully = (item.y1 - item.y0) + g.style.item_spacing.y * 2.0f <= sr_y1 - sr_y0 ||
                (!w->is_child && w->auto_fit[1] > 0) || (w->flags & OVL_WINDOW_ALWAYS_AUTO_RESIZE);
    if ((flags & SCROLL_KEEP_EDGE_Y) && !fully) {
        if (item.y0 < sr_y0 || !can_fully)
            set_scroll_from_pos_y(w, item.y0 - g.style.item_spacing.y - w->pos.y, 0.0f);
        else if (item.y1 >= sr_y1)
            set_scroll_from_pos_y(w, item.y1 + g.style.item_spacing.y - w->pos.y, 1.0f);
    } else if (((flags & SCROLL_KEEP_CENTER_Y) && !fully) || (flags & SCROLL_ALWAYS_CENTER_Y)) {
        if (can_fully)
            set_scroll_from_pos_y(w, truncf_((item.y0 + item.y1) * 0.5f) - w->pos.y, 0.5f);
        else
            set_scroll_from_pos_y(w, item.y0 - w->pos.y, 0.0f);
    }
    delta = calc_next_scroll(w).y - w->scroll.y;
    if (w->is_child && w->parent) {
        int parent_flags = in_flags;
        if (parent_flags & (SCROLL_ALWAYS_CENTER_Y | SCROLL_KEEP_CENTER_Y))
            parent_flags = SCROLL_KEEP_EDGE_Y;
        item.y0 -= delta;
        item.y1 -= delta;
        delta += scroll_to_rect_y(w->parent, item, parent_flags);
    }
    return delta;
}

/* ImGui's NavMoveRequestApplyResult */
static void nav_move_apply(void) {
    nav_item_t* result = g.nav_move_local.id   ? &g.nav_move_local
                         : g.nav_move_other.id ? &g.nav_move_other
                                               : NULL;
    const int axis = (g.nav_move_dir == DIR_UP || g.nav_move_dir == DIR_DOWN) ? 1 : 0;
    ovl_vec2_t pref;
    rect_t r;
    if (!result) {
        if (g.nav_id)
            set_nav_cursor_visible_after_move();
        clear_preferred_pos(axis);
        return;
    }
    if (result != &g.nav_move_other && g.nav_move_other.id &&
        g.nav_move_other.window->parent == g.nav_window)
        if (g.nav_move_other.dist_box < result->dist_box ||
            (g.nav_move_other.dist_box == result->dist_box &&
             g.nav_move_other.dist_center < result->dist_center))
            result = &g.nav_move_other;
    /* The item scrolled into view */
    scroll_to_rect_y(result->window, rect_rel_to_abs(result->window, rect_of(result->rect_rel)), 0);
    if (g.nav_window != result->window)
        g.nav_window = result->window;
    if (g.active_id != result->id)
        clear_active_id();
    if (g.nav_id != result->id)
        g.nav_just_moved_to_id = result->id;
    pref = root_for_nav(g.nav_window)->nav_pref_pos_rel;
    r = rect_of(result->rect_rel);
    set_nav_id(result->id, r);
    if (axis == 0)
        pref.x = (r.x0 + r.x1) * 0.5f;
    else
        pref.y = (r.y0 + r.y1) * 0.5f;
    root_for_nav(g.nav_window)->nav_pref_pos_rel = pref;
    set_nav_cursor_visible_after_move();
}

/* ImGui's NavUpdateCancelRequest: circle (or escape) lets go of the active item, else of the
 * focused one */
static void nav_cancel(int gamepad) {
    if (!key_pressed(OVL_KEY_ESCAPE, 0, 0) &&
        !(gamepad && key_pressed(OVL_KEY_PAD_FACE_RIGHT, 0, 0)))
        return;
    if (g.active_id) {
        clear_active_id();
    } else if (g.nav_window && g.nav_window->is_child && root_for_nav(g.nav_window)->parent) {
        /* Out of a child window: the focus back on it, as its parent's item */
        window_t* child = root_for_nav(g.nav_window);
        window_t* parent = child->parent;
        rect_t r;
        r.x0 = child->pos.x;
        r.y0 = child->pos.y;
        r.x1 = child->pos.x + child->size.x;
        r.y1 = child->pos.y + child->size.y;
        focus_window(parent);
        set_nav_id(child->child_id, rect_abs_to_rel(parent, r));
        set_nav_cursor_visible_after_move();
    } else {
        g.nav_id = 0;
    }
}

/* ImGui's NavBiasScoringRect */
static void nav_bias_scoring_rect(rect_t* r, ovl_vec2_t* pref, int dir) {
    const ovl_vec2_t off = g.nav_window->cursor_start;
    if (pref->x == FLT_MAX)
        pref->x = minf(r->x0 + 1.0f, r->x1) - off.x;
    if (pref->y == FLT_MAX)
        pref->y = (r->y0 + r->y1) * 0.5f - off.y;
    if ((dir == DIR_UP || dir == DIR_DOWN) && pref->x != FLT_MAX)
        r->x0 = r->x1 = pref->x + off.x;
    else if ((dir == DIR_LEFT || dir == DIR_RIGHT) && pref->y != FLT_MAX)
        r->y0 = r->y1 = pref->y + off.y;
}

/* ImGui's NavUpdateCreateMoveRequest: a d-pad press, repeated while held */
static void nav_create_move(int gamepad) {
    window_t* w = g.nav_window;
    rect_t scoring;
    g.nav_move_dir = DIR_NONE;
    if (w && !(w->flags & OVL_WINDOW_NO_NAV_INPUTS)) {
        /* Not in a direction the active item uses itself (IsActiveIdUsingNavDir) */
        const unsigned used = g.active_id_using_nav_dir;
        if (!(used & (1u << DIR_LEFT)) && ((gamepad && key_pressed(OVL_KEY_PAD_DPAD_LEFT, 1, 1)) ||
                                           key_pressed(OVL_KEY_LEFT, 1, 1)))
            g.nav_move_dir = DIR_LEFT;
        if (!(used & (1u << DIR_RIGHT)) &&
            ((gamepad && key_pressed(OVL_KEY_PAD_DPAD_RIGHT, 1, 1)) ||
             key_pressed(OVL_KEY_RIGHT, 1, 1)))
            g.nav_move_dir = DIR_RIGHT;
        if (!(used & (1u << DIR_UP)) &&
            ((gamepad && key_pressed(OVL_KEY_PAD_DPAD_UP, 1, 1)) || key_pressed(OVL_KEY_UP, 1, 1)))
            g.nav_move_dir = DIR_UP;
        if (!(used & (1u << DIR_DOWN)) && ((gamepad && key_pressed(OVL_KEY_PAD_DPAD_DOWN, 1, 1)) ||
                                           key_pressed(OVL_KEY_DOWN, 1, 1)))
            g.nav_move_dir = DIR_DOWN;
    }
    if (g.nav_move_dir != DIR_NONE) {
        g.nav_move_submitted = g.nav_move_scoring = 1;
        nav_item_clear(&g.nav_move_local);
        nav_item_clear(&g.nav_move_other);
        nav_update_any_request();
    }
    /* Moving with nothing focused finds the first item */
    if (g.nav_move_submitted && g.nav_id == 0) {
        g.nav_init_request = g.nav_init_from_move = 1;
        g.nav_init_result.id = 0;
        g.nav_cursor_visible = 1;
        nav_update_any_request();
    }
    /* With the gamepad, from within what is visible of the window */
    if (g.nav_move_submitted && g.nav_source == SRC_GAMEPAD && w) {
        rect_t inner, nav;
        const ovl_vec2_t next = calc_next_scroll(w);
        inner.x0 = w->inner[0] - 1.0f;
        inner.y0 = w->inner[1] - 1.0f;
        inner.x1 = w->inner[2] + 1.0f;
        inner.y1 = w->inner[3] + 1.0f;
        inner = rect_abs_to_rel(w, inner);
        /* As the window will be scrolled to */
        inner.x0 += next.x - w->scroll.x;
        inner.x1 += next.x - w->scroll.x;
        inner.y0 += next.y - w->scroll.y;
        inner.y1 += next.y - w->scroll.y;
        nav = rect_of(w->nav_rect_rel);
        if (!(nav.x0 >= inner.x0 && nav.y0 >= inner.y0 && nav.x1 <= inner.x1 &&
              nav.y1 <= inner.y1)) {
            const float pad_x = minf(inner.x1 - inner.x0, w->font_ref_size * 0.5f);
            const float pad_y = minf(inner.y1 - inner.y0, w->font_ref_size * 0.5f);
            inner.x0 += pad_x;
            inner.x1 -= pad_x;
            inner.y0 += pad_y;
            inner.y1 -= pad_y;
            /* ImRect::ClipWithFull */
            nav.x0 = minf(maxf(nav.x0, inner.x0), inner.x1);
            nav.y0 = minf(maxf(nav.y0, inner.y0), inner.y1);
            nav.x1 = minf(maxf(nav.x1, inner.x0), inner.x1);
            nav.y1 = minf(maxf(nav.y1, inner.y0), inner.y1);
            rect_store(w->nav_rect_rel, nav);
            g.nav_id = 0;
        }
    }
    /* The scoring rectangle: the focused item's, narrowed across the move */
    memset(&scoring, 0, sizeof(scoring));
    if (w) {
        rect_t nav = rect_of(w->nav_rect_rel);
        if (nav.x0 > nav.x1 || nav.y0 > nav.y1)
            memset(&nav, 0, sizeof(nav));
        scoring = rect_rel_to_abs(w, nav);
        if (g.nav_move_submitted)
            nav_bias_scoring_rect(&scoring, &root_for_nav(w)->nav_pref_pos_rel, g.nav_move_dir);
    }
    rect_store(g.nav_scoring_rect, scoring);
}

/* ImGui's NavUpdate, at the start of a frame */
static void nav_update(void) {
    static const int pad_source_keys[] = {
        OVL_KEY_PAD_FACE_RIGHT, OVL_KEY_PAD_FACE_LEFT, OVL_KEY_PAD_FACE_UP, OVL_KEY_PAD_FACE_DOWN,
        OVL_KEY_PAD_DPAD_RIGHT, OVL_KEY_PAD_DPAD_LEFT, OVL_KEY_PAD_DPAD_UP, OVL_KEY_PAD_DPAD_DOWN};
    static const int kb_source_keys[] = {OVL_KEY_SPACE, OVL_KEY_ENTER, OVL_KEY_ESCAPE,
                                         OVL_KEY_RIGHT, OVL_KEY_LEFT,  OVL_KEY_UP,
                                         OVL_KEY_DOWN};
    const int gamepad = g.gamepad_connected;
    int i;
    if (gamepad && g.nav_source != SRC_GAMEPAD)
        for (i = 0; i < (int)(sizeof(pad_source_keys) / sizeof(pad_source_keys[0])); i++)
            if (g.keys[pad_source_keys[i]].down)
                g.nav_source = SRC_GAMEPAD;
    if (g.nav_source != SRC_KEYBOARD)
        for (i = 0; i < (int)(sizeof(kb_source_keys) / sizeof(kb_source_keys[0])); i++)
            if (g.keys[kb_source_keys[i]].down)
                g.nav_source = SRC_KEYBOARD;

    /* What the last frame's requests found */
    g.nav_just_moved_to_id = 0;
    if (g.nav_init_result.id)
        nav_init_apply();
    g.nav_init_request = g.nav_init_from_move = 0;
    g.nav_init_result.id = 0;
    if (g.nav_move_submitted)
        nav_move_apply();
    g.nav_move_submitted = g.nav_move_scoring = 0;

    g.nav_active = g.nav_window && !(g.nav_window->flags & OVL_WINDOW_NO_NAV_INPUTS);
    nav_cancel(gamepad);

    /* Pressing the focused item: cross (or space; enter) */
    g.nav_activate_id = g.nav_activate_down_id = g.nav_activate_pressed_id = 0;
    if (g.nav_id && g.nav_cursor_visible && g.nav_window &&
        !(g.nav_window->flags & OVL_WINDOW_NO_NAV_INPUTS)) {
        const int down =
            g.keys[OVL_KEY_SPACE].down || (gamepad && g.keys[OVL_KEY_PAD_FACE_DOWN].down);
        const int pressed = down && (key_pressed(OVL_KEY_SPACE, 0, 0) ||
                                     (gamepad && key_pressed(OVL_KEY_PAD_FACE_DOWN, 0, 0)));
        const int enter = key_pressed(OVL_KEY_ENTER, 0, 0);
        if (g.active_id == 0 && pressed)
            g.nav_activate_id = g.nav_id;
        if ((g.active_id == 0 || g.active_id == g.nav_id) && enter)
            g.nav_activate_id = g.nav_id;
        if ((g.active_id == 0 || g.active_id == g.nav_id) && (down || enter))
            g.nav_activate_down_id = g.nav_id;
        if ((g.active_id == 0 || g.active_id == g.nav_id) && (pressed || enter)) {
            g.nav_activate_pressed_id = g.nav_id;
            g.nav_highlight_activated_id = g.nav_id;
            g.nav_highlight_activated_timer = 0.10f;
        }
    }
    if (g.nav_window && (g.nav_window->flags & OVL_WINDOW_NO_NAV_INPUTS))
        g.nav_cursor_visible = 0;
    if (g.nav_highlight_activated_timer > 0.0f)
        g.nav_highlight_activated_timer = maxf(0.0f, g.nav_highlight_activated_timer - g.delta);
    if (g.nav_highlight_activated_timer == 0.0f)
        g.nav_highlight_activated_id = 0;

    nav_create_move(gamepad);
    nav_update_any_request();
    g.nav_id_is_alive = 0;

    /* A window with nothing to move to scrolls with the d-pad; any with the left stick */
    if (g.nav_window && !(g.nav_window->flags & OVL_WINDOW_NO_NAV_INPUTS)) {
        window_t* w = g.nav_window;
        const float speed = (float)floor(w->font_ref_size * 100.0f * g.delta + 0.5f);
        if (!w->nav_items && w->scroll_max.y > 0.0f && g.nav_move_dir != DIR_NONE) {
            if (g.nav_move_dir == DIR_LEFT || g.nav_move_dir == DIR_RIGHT)
                set_scroll_x(
                    w, truncf_(w->scroll.x + (g.nav_move_dir == DIR_LEFT ? -1.0f : 1.0f) * speed));
            else
                set_scroll_y(
                    w, truncf_(w->scroll.y + (g.nav_move_dir == DIR_UP ? -1.0f : 1.0f) * speed));
        }
        if (gamepad) {
            const float dy =
                g.keys[OVL_KEY_PAD_LSTICK_DOWN].analog - g.keys[OVL_KEY_PAD_LSTICK_UP].analog;
            const float tweak = g.keys[OVL_KEY_PAD_L1].down   ? 1.0f / 10.0f
                                : g.keys[OVL_KEY_PAD_R1].down ? 10.0f
                                                              : 1.0f;
            if (dy != 0.0f)
                set_scroll_y(w, truncf_(w->scroll.y + dy * speed * tweak));
        }
    }
}

int ovl_nav_active(void) {
    return g.nav_active;
}

void ovl_keep_nav_highlight(void) {
    g.nav_cursor_visible = 1;
}

void ovl_new_frame(float display_w, float display_h, float delta_time) {
    int i;
    ovl_atlas_begin_frame();
    g.frame++;
    g.display = v2(display_w, display_h);
    g.delta = delta_time > 0.0f ? delta_time : 1.0f / 60.0f;
    g.time += (double)g.delta;

    /* As ImGui's NewFrame: the active item let go once it is no longer submitted */
    if (g.active_id && g.active_id_is_alive != g.active_id &&
        g.active_id_previous_frame == g.active_id)
        clear_active_id();
    g.active_id_previous_frame = g.active_id;
    g.active_id_is_alive = 0;
    g.active_id_just_activated = 0;
    if (g.active_id == 0)
        g.active_id_using_nav_dir = 0;

    /* The input, as ImGui's UpdateKeyboardInputs: a gamepad's only while one is connected */
    for (i = 1; i < MAX_KEYS; i++) {
        key_data_t* k = &g.keys[i];
        if (g.key_events[i].set) {
            k->down = g.key_events[i].down;
            k->analog = g.key_events[i].analog;
            g.key_events[i].set = 0;
        }
        if (!g.gamepad_connected && is_gamepad_key(i)) {
            k->down = 0;
            k->analog = 0.0f;
        }
        k->duration_prev = k->duration;
        k->duration = k->down ? (k->duration < 0.0f ? 0.0f : k->duration + g.delta) : -1.0f;
    }
    nav_update();

    for (i = 0; i < g.window_count; i++) {
        window_t* w = &g.windows[i];
        w->was_active = w->active;
        w->active = 0;
    }
    /* The focused window gone, the focus goes to the front-most one left */
    if (g.nav_window && !g.nav_window->was_active)
        focus_top_most();

    ovl_draw_list_reset(&g.background, display_w, display_h);
    ovl_draw_list_reset(&g.foreground, display_w, display_h);
    g.stack_count = 0;
    g.current = NULL;
    g.color_count = g.var_count = g.font_count = g.id_count = g.wrap_count = 0;
    g.disabled_count = 0;
    update_font_size();
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
    w->auto_fit[0] = w->auto_fit[1] = 2;
    w->auto_fit_only_grows = !(flags & OVL_WINDOW_ALWAYS_AUTO_RESIZE);
    w->scroll_target[0] = w->scroll_target[1] = FLT_MAX;
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

/* ImGui's CalcWindowAutoFitSize for a window without a title bar: the contents and the padding
 * on the axes fitted (@mask: 1 x, 2 y), the size it had on the others, within the display less
 * its safe area */
static ovl_vec2_t auto_fit_size(const window_t* w, ovl_vec2_t content, int mask) {
    ovl_vec2_t s, mn, mx;
    s.x = (mask & 1) ? content.x + w->padding.x * 2.0f : w->size.x;
    s.y = (mask & 2) ? content.y + w->padding.y * 2.0f : w->size.y;
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

/* ImGui's CalcNextScrollFromScrollTargetAndClamp */
static ovl_vec2_t calc_next_scroll(const window_t* w) {
    float s[2];
    int axis;
    const float deco[2] = {w->deco_x2, 0.0f};
    const float size_full[2] = {w->size_full.x, w->size_full.y};
    const float max[2] = {w->scroll_max.x, w->scroll_max.y};
    s[0] = w->scroll.x;
    s[1] = w->scroll.y;
    for (axis = 0; axis < 2; axis++) {
        if (w->scroll_target[axis] < FLT_MAX)
            s[axis] = w->scroll_target[axis] -
                      w->scroll_target_ratio[axis] * (size_full[axis] - deco[axis]);
        /* ImRound64 */
        s[axis] = (float)(long)(maxf(s[axis], 0.0f) + 0.5f);
        if (!w->skip)
            s[axis] = minf(s[axis], max[axis]);
    }
    return v2(s[0], s[1]);
}

/* ImGui's SetScrollFromPosY and SetScrollY */
static void set_scroll_from_pos_y(window_t* w, float local_y, float center_ratio) {
    w->scroll_target[1] = truncf_(local_y + w->scroll.y);
    w->scroll_target_ratio[1] = center_ratio;
}

static void set_scroll_y(window_t* w, float y) {
    w->scroll_target[1] = y;
    w->scroll_target_ratio[1] = 0.0f;
}

static void set_scroll_x(window_t* w, float x) {
    w->scroll_target[0] = x;
    w->scroll_target_ratio[0] = 0.0f;
}

/* ImGui's Scrollbar(ImGuiAxis_Y) and ScrollbarEx, as drawn (the overlays have no mouse to drag
 * it with) */
static void draw_scrollbar_y(window_t* w, ovl_draw_list_t* dl) {
    const float border = (float)floor(w->border_size * 0.5f + 0.5f);
    float f[4], b[4], frame_w, frame_h, alpha = 1.0f, pad, size_v, grab_min, grab_h, grab_v;
    long size_visible, size_contents, win_size, scroll_max;
    float scroll_ratio;
    /* GetWindowScrollbarRect: below no title bar, the top border */
    f[0] = maxf(w->pos.x, w->pos.x + w->size.x - border - w->deco_x2);
    f[1] = w->inner[1] + border;
    f[2] = w->pos.x + w->size.x - border;
    f[3] = w->inner[3] - border;
    frame_w = f[2] - f[0];
    frame_h = f[3] - f[1];
    if (frame_w <= 0.0f || frame_h <= 0.0f)
        return;
    if (frame_h < frame_w)
        alpha = minf(maxf(frame_h / maxf(frame_w * 2.0f, 1.0f), 0.0f), 1.0f);
    if (alpha <= 0.0f)
        return;
    pad = truncf_(minf(2.0f, minf(frame_w, frame_h) * 0.5f));
    b[0] = f[0] + pad;
    b[1] = f[1] + pad;
    b[2] = f[2] - pad;
    b[3] = f[3] - pad;
    size_v = b[3] - b[1];
    if (size_v < 1.0f)
        return;
    size_visible = (long)(w->inner[3] - w->inner[1]);
    size_contents = (long)(w->content_size.y + w->padding.y * 2.0f);
    win_size = size_contents > size_visible ? size_contents : size_visible;
    if (win_size < 1)
        win_size = 1;
    grab_min = minf(b[3] - b[1], g.style.grab_min_size);
    grab_h =
        (float)(int)minf(maxf(size_v * ((float)size_visible / (float)win_size), grab_min), size_v);
    scroll_max = size_contents - size_visible > 1 ? size_contents - size_visible : 1;
    scroll_ratio = minf(maxf((float)(long)w->scroll.y / (float)scroll_max, 0.0f), 1.0f);
    grab_v = scroll_ratio * (size_v - grab_h) / size_v;
    ovl_add_rect_filled(dl, v2(f[0], f[1]), v2(f[2], f[3]),
                        ovl_color_u32(OVL_COLOR_SCROLLBAR_BG, 1.0f), w->rounding, OVL_CORNERS_ALL);
    {
        const float y0 = b[1] + (b[3] - b[1]) * grab_v;
        ovl_add_rect_filled(dl, v2(b[0], y0), v2(b[2], y0 + grab_h),
                            ovl_color_u32(OVL_COLOR_SCROLLBAR_GRAB, alpha),
                            g.style.scrollbar_rounding, 0);
    }
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
    w->region_max.x =
        w->pos.x - w->scroll.x + w->padding.x + (w->size.x - w->padding.x * 2.0f - w->deco_x2);
    w->region_max.y = w->pos.y - w->scroll.y + w->padding.y + (w->size.y - w->padding.y * 2.0f);
    w->work_max_x = truncf_(w->inner[0] - w->scroll.x + maxf(w->padding.x, w->border_size)) +
                    maxf(0.0f, w->size.x - w->padding.x * 2.0f - w->deco_x2);
    /* ImGui's InnerClipRect: up to the middle of the border, inside the scrollbar */
    w->clip[0] = (float)floor(0.5f + w->inner[0] + w->border_size * 0.5f);
    w->clip[1] = (float)floor(0.5f + w->inner[1] + w->border_size * 0.5f);
    w->clip[2] = (float)floor(w->inner[2] - w->border_size * 0.5f);
    w->clip[3] = (float)floor(w->inner[3] - w->border_size * 0.5f);
    /* within the display (or the parent's clip, which the draw list's push intersects) */
    w->clip[0] = maxf(w->clip[0], 0.0f);
    w->clip[1] = maxf(w->clip[1], 0.0f);
    w->clip[2] = minf(w->clip[2], g.display.x);
    w->clip[3] = minf(w->clip[3], g.display.y);
}

static int begin_window(const char* name, unsigned flags, window_t* parent, ovl_vec2_t child_size,
                        int child_border) {
    window_t* w = find_window(name);
    int just_created = 0, want_focus, auto_fitted = 0;
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
    w->id_stack_base = g.id_count;
    if (g.id_count < MAX_STACK)
        g.ids[g.id_count++] = w->id;
    update_font_size();

    if (parent) {
        /* ImGui's: a child has the window padding with a border, or when asked */
        w->padding = (child_border & (OVL_CHILD_BORDERS | OVL_CHILD_ALWAYS_USE_WINDOW_PADDING))
                         ? g.style.window_padding
                         : v2(0, 0);
        w->border_size = (child_border & OVL_CHILD_BORDERS) ? g.style.child_border_size : 0.0f;
        w->child_flags = (unsigned)child_border;
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
        /* ImGui's SetWindowSize: an axis asked as 0 or less is fitted to the contents, as one
         * not asked for is */
        const int x_set = size_set && g.next.size.x > 0.0f;
        const int y_set = size_set && g.next.size.y > 0.0f;
        int fit_x_always, fit_y_always, fit_x, fit_y;
        w->padding = g.style.window_padding;
        w->border_size = g.style.window_border_size;
        w->rounding = g.style.window_rounding;
        if (size_set) {
            w->auto_fit[0] = x_set ? 0 : 2;
            w->auto_fit[1] = y_set ? 0 : 2;
            if (x_set)
                w->size_full.x = truncf_(g.next.size.x);
            else
                w->auto_fit_only_grows = 0;
            if (y_set)
                w->size_full.y = truncf_(g.next.size.y);
            else
                w->auto_fit_only_grows = 0;
        }
        fit_x_always = !x_set && (flags & OVL_WINDOW_ALWAYS_AUTO_RESIZE);
        fit_y_always = !y_set && (flags & OVL_WINDOW_ALWAYS_AUTO_RESIZE);
        fit_x = fit_x_always || (!x_set && w->auto_fit[0] > 0);
        fit_y = fit_y_always || (!y_set && w->auto_fit[1] > 0);
        if (fit_x || fit_y) {
            const ovl_vec2_t fit =
                auto_fit_size(w, w->content_size, (fit_x ? 1 : 0) | (fit_y ? 2 : 0));
            auto_fitted = 1;
            if (fit_x)
                w->size_full.x =
                    fit_x_always || !w->auto_fit_only_grows ? fit.x : maxf(w->size_full.x, fit.x);
            if (fit_y)
                w->size_full.y =
                    fit_y_always || !w->auto_fit_only_grows ? fit.y : maxf(w->size_full.y, fit.y);
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
        if (w->auto_fit[0] > 0)
            w->auto_fit[0]--;
        if (w->auto_fit[1] > 0)
            w->auto_fit[1]--;
        w->child_flags = 0;
    }
    /* ImGui's scrollbar visibility: what the contents needed last frame against the room the
     * window had then (its size now when it was just made or fitted) */
    {
        const int use_current =
            just_created || (!parent && (auto_fitted || (flags & OVL_WINDOW_ALWAYS_AUTO_RESIZE)));
        const float avail_y = use_current ? w->size_full.y : w->inner[3] - w->inner[1];
        const float needed_y = just_created ? 0.0f : w->content_size.y + w->padding.y * 2.0f;
        w->scrollbar_y = !(flags & OVL_WINDOW_NO_SCROLLBAR) && needed_y > avail_y;
        w->deco_x2 = w->scrollbar_y ? g.style.scrollbar_size : 0.0f;
    }
    w->inner[0] = w->pos.x;
    w->inner[1] = w->pos.y;
    w->inner[2] = w->pos.x + w->size.x - w->deco_x2;
    w->inner[3] = w->pos.y + w->size.y;
    /* SetNextWindowFocus */
    if (g.next.focus)
        focus_window(w);
    want_focus = !parent && w->appearing && !(flags & OVL_WINDOW_NO_FOCUS_ON_APPEARING);
    bg_alpha = 1.0f;
    w->bg_alpha_set = g.next.bg_alpha_set;
    if (g.next.bg_alpha_set)
        bg_alpha = g.next.bg_alpha;
    memset(&g.next, 0, sizeof(g.next));

    /* Scrolling, to what was asked last frame, kept within what there is to scroll, as ImGui's
     * CalcNextScrollFromScrollTargetAndClamp */
    w->scroll_max.x =
        maxf(0.0f, w->content_size.x + w->padding.x * 2.0f - (w->inner[2] - w->inner[0]));
    w->scroll_max.y =
        maxf(0.0f, w->content_size.y + w->padding.y * 2.0f - (w->inner[3] - w->inner[1]));
    w->scroll = calc_next_scroll(w);
    w->scroll_target[0] = w->scroll_target[1] = FLT_MAX;

    ovl_draw_list_reset(&w->dl, g.display.x, g.display.y);
    if (parent) {
        /* Drawn into its parent's list, clipped to its own rectangle */
        ovl_push_clip(&w->dl, w->pos, v2(w->pos.x + w->size.x, w->pos.y + w->size.y), 0);
    }
    setup_drawing_context(w);
    w->font_ref_size = g.font_size;
    w->nav_items = w->nav_items_next;
    w->nav_items_next = 0;
    /* A window appearing takes the focus, its first item to be focused (ImGui's Begin, once the
     * cursor's start is known) */
    if (want_focus) {
        focus_window(w);
        if (w == g.nav_window)
            nav_init_window(w);
    }

    /* As ImGui: a window begun with the style's alpha at 0 is not drawn this frame */
    w->skip = g.style.alpha <= 0.0f;
    if (w->hidden_frames == 0 && !w->skip) {
        /* A child's background, scrollbar and border go into its parent's list, as ImGui's
         * (render_decorations_in_parent): under what the parent draws after it, where the child's
         * own list is over all of the parent's. Unless the parent has drawn nothing yet, or the
         * child begun before this one overlaps it. */
        ovl_draw_list_t* deco = &w->dl;
        ovl_vec2_t mx;
        mx.x = w->pos.x + w->size.x;
        mx.y = w->pos.y + w->size.y;
        if (parent) {
            const int overlapping = parent->has_prev_child && w->pos.x < parent->prev_child[2] &&
                                    mx.x > parent->prev_child[0] &&
                                    w->pos.y < parent->prev_child[3] &&
                                    mx.y > parent->prev_child[1];
            if (parent->dl.vtx.size != 0 && !overlapping)
                deco = &parent->dl;
            parent->has_prev_child = 1;
            parent->prev_child[0] = w->pos.x;
            parent->prev_child[1] = w->pos.y;
            parent->prev_child[2] = mx.x;
            parent->prev_child[3] = mx.y;
        }
        w->has_prev_child = 0;
        if (!(flags & OVL_WINDOW_NO_BACKGROUND)) {
            ovl_col_t bg = parent ? ovl_color_u32(OVL_COLOR_CHILD_BG, 1.0f)
                                  : ovl_color_u32(OVL_COLOR_WINDOW_BG, 1.0f);
            if (w->bg_alpha_set) {
                bg = (bg & 0x00FFFFFFu) |
                     ((ovl_col_t)(minf(maxf(bg_alpha, 0.0f), 1.0f) * 255.0f + 0.5f) << 24);
            }
            ovl_add_rect_filled(deco, w->pos, mx, bg, w->rounding, 0);
        }
        if (w->scrollbar_y)
            draw_scrollbar_y(w, deco);
        /* No border on a window without a background, as ImGui (RenderWindowOuterBorders) */
        if (w->border_size > 0.0f && !(flags & OVL_WINDOW_NO_BACKGROUND))
            ovl_add_rect(deco, w->pos, mx, ovl_color_u32(OVL_COLOR_BORDER, 1.0f), w->rounding, 0,
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
    g.id_count = w->id_stack_base;
    g.stack_count--;
    g.current = g.stack_count ? g.stack[g.stack_count - 1] : NULL;
    update_font_size();
}

void ovl_end(void) {
    end_window();
}

int ovl_begin_child(const char* id, ovl_vec2_t size, unsigned child_flags, unsigned flags) {
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
    ok = begin_window(name, flags | OVL_WINDOW_NO_TITLE_BAR, parent, size, (int)child_flags);
    g.current->order = parent->order;
    g.current->child_id = hid;
    /* Entered when it is pressed as its parent's item, if it has items or scrolls and is not
     * flattened into the parent (ImGui's BeginChildEx): the focus goes to it and to an item of
     * its own. An id of no item is made the active one, so that the press does not also press
     * the item the focus lands on; it lapses by itself. */
    {
        window_t* child = g.current;
        const uint32_t entering = hash_str("##Child", NULL, hid);
        if (g.active_id == entering)
            clear_active_id();
        if (g.nav_activate_id == hid && !(child_flags & OVL_CHILD_NAV_FLATTENED) &&
            (child->nav_items || child->scroll_max.y > 0.0f)) {
            focus_window(child);
            nav_init_window(child);
            set_active_id(entering, child);
            g.active_id_source = g.nav_source;
        }
    }
    return ok;
}

void ovl_end_child(void) {
    window_t* child = g.current;
    window_t* parent;
    ovl_vec2_t size, mx;
    if (!child || !child->is_child)
        return;
    size = child->size;
    end_window();
    parent = g.current;
    if (!parent)
        return;
    mx.x = parent->cursor_pos.x + size.x;
    mx.y = parent->cursor_pos.y + size.y;
    {
        const ovl_vec2_t mn = parent->cursor_pos;
        item_size(size, -1.0f);
        /* An item of its parent's: one to move to when it has items or scrolls and is not
         * flattened into it, else not one (its items reached directly when flattened) */
        if ((child->nav_items || child->scroll_max.y > 0.0f) &&
            !(child->child_flags & OVL_CHILD_NAV_FLATTENED)) {
            item_add(child->child_id, mn, mx);
            render_nav_cursor(mn, mx, child->child_id, -1.0f);
        } else {
            item_add_no_nav(child->child_id, mn, mx);
            if (child->child_flags & OVL_CHILD_NAV_FLATTENED)
                parent->nav_items_next |= child->nav_items_next;
        }
    }
}

ovl_vec2_t ovl_window_pos(void) {
    return g.current ? g.current->pos : v2(0, 0);
}

ovl_vec2_t ovl_window_padding(void) {
    return g.current ? g.current->padding : v2(0, 0);
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
    if (g.current)
        set_scroll_y(g.current, y);
}

void ovl_set_scroll_x(float x) {
    if (g.current)
        set_scroll_x(g.current, x);
}

float ovl_scroll_y(void) {
    return g.current ? g.current->scroll.y : 0.0f;
}

float ovl_scroll_max_y(void) {
    return g.current ? g.current->scroll_max.y : 0.0f;
}

/* ImGui's SetScrollHereY */
void ovl_set_scroll_here_y(float center_ratio) {
    window_t* w = g.current;
    float spacing, target;
    if (!w)
        return;
    spacing = maxf(w->padding.y, g.style.item_spacing.y);
    target = lerpf(w->cursor_prev_line.y - spacing,
                   w->cursor_prev_line.y + w->prev_line_size.y + spacing, center_ratio);
    set_scroll_from_pos_y(w, target - w->pos.y, center_ratio);
}

/* ------------------------------------------------------------------------------------------ */
/* Ids */

uint32_t ovl_get_id(const char* str) {
    return hash_str(str, NULL, id_seed());
}

/* ImGui's GetID(int) */
uint32_t ovl_get_id_int(int value) {
    return hash_data(&value, sizeof(value), id_seed());
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

/* ImGui's NavScoreItemDistInterval */
static float dist_interval(float cand_min, float cand_max, float curr_min, float curr_max) {
    if (cand_max < curr_min)
        return cand_max - curr_min;
    if (curr_max < cand_min)
        return cand_min - curr_max;
    return 0.0f;
}

static int dir_quadrant(float dx, float dy) {
    if (fabs(dx) > fabs(dy))
        return dx > 0.0f ? DIR_RIGHT : DIR_LEFT;
    return dy > 0.0f ? DIR_DOWN : DIR_UP;
}

static float lerpf(float a, float b, float t) {
    return a + (b - a) * t;
}

/* ImGui's NavScoreItem: whether @cand beats the best so far in the direction moved */
static int nav_score_item(nav_item_t* result, rect_t cand) {
    const rect_t curr = rect_of(g.nav_scoring_rect);
    float dbx, dby, dist_box, dcx, dcy, dist_center;
    int quadrant, new_best = 0;
    /* Entering a flattened child from its parent: its items as clipped to it */
    if (g.current && g.current->parent == g.nav_window && g.current->is_child) {
        const float* c = ovl_draw_list_clip(&g.current->dl);
        if (!(cand.x0 < c[2] && cand.x1 > c[0] && cand.y0 < c[3] && cand.y1 > c[1]))
            return 0;
        cand.x0 = minf(maxf(cand.x0, c[0]), c[2]);
        cand.y0 = minf(maxf(cand.y0, c[1]), c[3]);
        cand.x1 = minf(maxf(cand.x1, c[0]), c[2]);
        cand.y1 = minf(maxf(cand.y1, c[1]), c[3]);
    }
    dbx = dist_interval(cand.x0, cand.x1, curr.x0, curr.x1);
    dby = dist_interval(lerpf(cand.y0, cand.y1, 0.2f), lerpf(cand.y0, cand.y1, 0.8f),
                        lerpf(curr.y0, curr.y1, 0.2f), lerpf(curr.y0, curr.y1, 0.8f));
    if (dby != 0.0f && dbx != 0.0f)
        dbx = (dbx / 1000.0f) + (dbx > 0.0f ? 1.0f : -1.0f);
    dist_box = (float)fabs(dbx) + (float)fabs(dby);
    dcx = (cand.x0 + cand.x1) - (curr.x0 + curr.x1);
    dcy = (cand.y0 + cand.y1) - (curr.y0 + curr.y1);
    dist_center = (float)fabs(dcx) + (float)fabs(dcy);
    if (dbx != 0.0f || dby != 0.0f)
        quadrant = dir_quadrant(dbx, dby);
    else if (dcx != 0.0f || dcy != 0.0f)
        quadrant = dir_quadrant(dcx, dcy);
    else
        quadrant = g.last_id < g.nav_id ? DIR_LEFT : DIR_RIGHT;
    if (quadrant == g.nav_move_dir) {
        if (dist_box < result->dist_box) {
            result->dist_box = dist_box;
            result->dist_center = dist_center;
            return 1;
        }
        if (dist_box == result->dist_box) {
            if (dist_center < result->dist_center) {
                result->dist_center = dist_center;
                new_best = 1;
            } else if (dist_center == result->dist_center) {
                if (((g.nav_move_dir == DIR_UP || g.nav_move_dir == DIR_DOWN) ? dby : dbx) < 0.0f)
                    new_best = 1;
            }
        }
    }
    /* (Its axial fallback is for menu bars only) */
    return new_best;
}

static void nav_apply_item(nav_item_t* result) {
    rect_t r;
    r.x0 = g.last_min.x;
    r.y0 = g.last_min.y;
    r.x1 = g.last_max.x;
    r.y1 = g.last_max.y;
    result->window = g.current;
    result->id = g.last_id;
    rect_store(result->rect_rel, rect_abs_to_rel(g.current, r));
}

/* ImGui's NavProcessItem: the item for a request to focus the first item or to move, and the
 * focused item's rectangle kept */
static void nav_process_item(void) {
    window_t* w = g.current;
    const uint32_t id = g.last_id;
    const int disabled = g.disabled_count > 0;
    rect_t bb;
    bb.x0 = g.last_min.x;
    bb.y0 = g.last_min.y;
    bb.x1 = g.last_max.x;
    bb.y1 = g.last_max.y;
    if (g.nav_init_request && !disabled) {
        nav_apply_item(&g.nav_init_result);
        g.nav_init_request = 0;
        nav_update_any_request();
    }
    if (g.nav_move_scoring && !disabled && !(w->flags & OVL_WINDOW_NO_NAV_INPUTS) &&
        g.nav_id != id) {
        nav_item_t* result = w == g.nav_window ? &g.nav_move_local : &g.nav_move_other;
        if (nav_score_item(result, bb))
            nav_apply_item(result);
    }
    if (g.nav_id == id) {
        if (g.nav_window != w)
            set_nav_window(w);
        g.nav_id_is_alive = 1;
        rect_store(w->nav_rect_rel, rect_abs_to_rel(w, bb));
    }
}

/* ImGui's ItemAdd: an item kept alive and offered to navigation; 0 when it is clipped away
 * (and neither active nor focused), not to be drawn */
static int item_add(uint32_t id, ovl_vec2_t min, ovl_vec2_t max) {
    window_t* w = g.current;
    const float* c;
    g.next_item_width_set = 0; /* for the item that asked: this one, or none */
    g.last_id = id;
    g.last_min = min;
    g.last_max = max;
    g.last_hovered = g.last_active = g.last_clicked = 0;
    if (!w)
        return 0;
    if (id) {
        if (g.active_id == id)
            g.active_id_is_alive = id;
        w->nav_items_next = 1;
        if ((g.nav_id == id || g.nav_any_request) && g.nav_window &&
            root_for_nav(g.nav_window) == root_for_nav(w) &&
            (w == g.nav_window ||
             ((w->child_flags | g.nav_window->child_flags) & OVL_CHILD_NAV_FLATTENED)))
            nav_process_item();
    }
    c = ovl_draw_list_clip(&w->dl);
    if (!(min.y < c[3] && max.y > c[1] && min.x < c[2] && max.x > c[0]))
        if (id == 0 || (id != g.active_id && id != g.active_id_previous_frame && id != g.nav_id &&
                        id != g.nav_activate_id))
            return 0;
    return 1;
}

/* ItemAdd for an item navigation does not go to (ImGuiItemFlags_NoNav): kept alive only */
static void item_add_no_nav(uint32_t id, ovl_vec2_t min, ovl_vec2_t max) {
    g.last_id = id;
    g.last_min = min;
    g.last_max = max;
    g.last_hovered = g.last_active = g.last_clicked = 0;
    if (id && g.active_id == id)
        g.active_id_is_alive = id;
}

/* ImGui's RenderNavCursor around the focused item */
static void render_nav_cursor(ovl_vec2_t min, ovl_vec2_t max, uint32_t id, float rounding) {
    window_t* w = g.current;
    const float thickness = 2.0f, distance = 4.0f;
    const float* c;
    float r[4];
    int fully_visible;
    if (!w || id != g.nav_id || !g.nav_cursor_visible)
        return;
    if (rounding < 0.0f)
        rounding = g.style.frame_rounding;
    c = ovl_draw_list_clip(&w->dl);
    /* ImRect::ClipWith, then expanded */
    r[0] = maxf(min.x, c[0]) - distance;
    r[1] = maxf(min.y, c[1]) - distance;
    r[2] = minf(max.x, c[2]) + distance;
    r[3] = minf(max.y, c[3]) + distance;
    fully_visible = r[0] >= c[0] && r[1] >= c[1] && r[2] <= c[2] && r[3] <= c[3];
    if (!fully_visible)
        ovl_push_clip(&w->dl, v2(r[0], r[1]), v2(r[2], r[3]), 0);
    ovl_add_rect(&w->dl, v2(r[0], r[1]), v2(r[2], r[3]), ovl_color_u32(OVL_COLOR_NAV_CURSOR, 1.0f),
                 rounding, 0, thickness);
    if (!fully_visible)
        ovl_pop_clip(&w->dl);
}

/* ImGui's ButtonBehavior, for the gamepad and keyboard (the overlays have no mouse): shown as
 * hovered while focused, pressed by cross, held while it is down */
static int button_behavior(uint32_t id, int* hovered, int* held) {
    window_t* w = g.current;
    const int disabled = g.disabled_count > 0;
    int pressed = 0;
    *hovered = *held = 0;
    if (!disabled) {
        if (g.nav_id == id && g.nav_cursor_visible && g.nav_highlight_under_nav)
            *hovered = 1;
        if (g.nav_activate_down_id == id &&
            (g.nav_activate_id == id || g.nav_activate_pressed_id == id)) {
            pressed = 1;
            set_active_id(id, w);
            g.active_id_source = g.nav_source;
            set_focus_id(id, w);
        }
    }
    if (g.active_id == id) {
        if (g.active_id_source == SRC_MOUSE) {
            clear_active_id();
            g.nav_cursor_visible = 0;
        } else if (g.nav_activate_down_id == id) {
            *held = 1;
        } else {
            clear_active_id();
        }
    }
    if (g.nav_highlight_activated_id == id && !disabled)
        *hovered = 1;
    g.last_hovered = *hovered;
    g.last_active = *held;
    g.last_clicked = pressed;
    return pressed;
}

/* ImGui's internals, for widgets of the overlays' own */
void ovl_item_size(ovl_vec2_t size, float text_baseline_y) {
    if (g.current)
        item_size(size, text_baseline_y);
}

int ovl_item_add(uint32_t id, ovl_vec2_t min, ovl_vec2_t max) {
    return item_add(id, min, max);
}

int ovl_button_behavior(uint32_t id, int* hovered, int* held) {
    *hovered = *held = 0;
    return g.current ? button_behavior(id, hovered, held) : 0;
}

void ovl_render_nav_cursor(ovl_vec2_t min, ovl_vec2_t max, uint32_t id) {
    render_nav_cursor(min, max, id, -1.0f);
}

int ovl_is_item_hovered(void) {
    return g.last_hovered;
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

uint32_t ovl_last_item_id(void) {
    return g.last_id;
}

/* ImGui's SetItemDefaultFocus: the last item is the one a window appearing focuses */
void ovl_set_item_default_focus(void) {
    window_t* w = g.current;
    if (!w || g.nav_window != root_for_nav(w) || !(g.nav_init_request || g.nav_init_result.id))
        return;
    g.nav_init_request = 0;
    nav_apply_item(&g.nav_init_result);
    nav_update_any_request();
}

/* The core's SetItemCurrentNavFocus (imgui_std.h): the focus put on an item, the cursor shown,
 * and the window's first item no longer sought */
void ovl_set_item_current_nav_focus(uint32_t id) {
    if (!g.current)
        return;
    set_focus_id(id ? id : g.last_id, g.current);
    g.nav_init_result.id = 0;
    g.nav_init_result.window = NULL;
    g.nav_cursor_visible = 1;
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

/* ImGui's RenderText: at @pos in the current font and size, the style's text colour */
void ovl_render_text(ovl_vec2_t pos, const char* text, const char* end) {
    if (text && (end ? end != text : text[0] != '\0'))
        render_text(pos, text, end, 0.0f, ovl_color_u32(OVL_COLOR_TEXT, 1.0f));
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
    if (!item_add(id, pos, mx))
        return 0;
    pressed = button_behavior(id, &hovered, &held);
    ci = (held && hovered) ? OVL_COLOR_BUTTON_ACTIVE
         : hovered         ? OVL_COLOR_BUTTON_HOVERED
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

/* ImGui's RenderFrame: the fill, and the frame's border when the style has one */
static void render_frame(ovl_vec2_t mn, ovl_vec2_t mx, ovl_col_t fill, float rounding) {
    window_t* w = g.current;
    const float bs = g.style.frame_border_size;
    ovl_add_rect_filled(&w->dl, mn, mx, fill, rounding, 0);
    if (bs > 0.0f) {
        ovl_add_rect(&w->dl, v2(mn.x + 1.0f, mn.y + 1.0f), v2(mx.x + 1.0f, mx.y + 1.0f),
                     ovl_color_u32(OVL_COLOR_BORDER_SHADOW, 1.0f), rounding, 0, bs);
        ovl_add_rect(&w->dl, mn, mx, ovl_color_u32(OVL_COLOR_BORDER, 1.0f), rounding, 0, bs);
    }
}

/* ImGui's RenderCheckMark */
static void render_check_mark(ovl_draw_list_t* dl, ovl_vec2_t pos, ovl_col_t col, float sz) {
    const float thickness = maxf(sz / 5.0f, 1.0f);
    ovl_vec2_t pts[3];
    float third, bx, by;
    sz -= thickness * 0.5f;
    pos.x += thickness * 0.25f;
    pos.y += thickness * 0.25f;
    third = sz / 3.0f;
    bx = pos.x + third;
    by = pos.y + sz - third * 0.5f;
    pts[0] = v2(bx - third, by - third);
    pts[1] = v2(bx, by);
    pts[2] = v2(bx + third * 2.0f, by - third * 2.0f);
    ovl_add_polyline(dl, pts, 3, col, 0, thickness);
}

int ovl_checkbox(const char* label, int* v) {
    window_t* w = g.current;
    const char* lend;
    ovl_vec2_t label_size, pos, total_max, check_max;
    uint32_t id;
    float square;
    int hovered, held, pressed;
    if (!w)
        return 0;
    lend = label_end(label);
    id = hash_str(label, NULL, id_seed());
    label_size = ovl_calc_text_size(label, lend, 0.0f);
    square = ovl_frame_height();
    pos = w->cursor_pos;
    total_max.x =
        pos.x + square + (label_size.x > 0.0f ? g.style.item_inner_spacing.x + label_size.x : 0.0f);
    total_max.y = pos.y + label_size.y + g.style.frame_padding.y * 2.0f;
    item_size(v2(total_max.x - pos.x, total_max.y - pos.y), g.style.frame_padding.y);
    if (!item_add(id, pos, total_max))
        return 0;
    pressed = button_behavior(id, &hovered, &held);
    if (pressed)
        *v = !*v;
    check_max = v2(pos.x + square, pos.y + square);
    render_nav_cursor(pos, total_max, id, -1.0f);
    render_frame(pos, check_max,
                 ovl_color_u32((held && hovered) ? OVL_COLOR_FRAME_BG_ACTIVE
                               : hovered         ? OVL_COLOR_FRAME_BG_HOVERED
                               : *v              ? OVL_COLOR_CHECKBOX_SELECTED_BG
                                                 : OVL_COLOR_FRAME_BG,
                               1.0f),
                 g.style.frame_rounding);
    if (*v) {
        const float pad = maxf(1.0f, truncf_(square / 6.0f));
        render_check_mark(&w->dl, v2(pos.x + pad, pos.y + pad),
                          ovl_color_u32(OVL_COLOR_CHECK_MARK, 1.0f), square - pad * 2.0f);
    }
    if (label_size.x > 0.0f)
        render_text(v2(check_max.x + g.style.item_inner_spacing.x, pos.y + g.style.frame_padding.y),
                    label, lend, 0.0f, ovl_color_u32(OVL_COLOR_TEXT, 1.0f));
    return pressed;
}

void ovl_set_next_item_width(float width) {
    g.next_item_width_set = 1;
    g.next_item_width = width;
}

/* ImGui's CalcItemWidth: what was set for the next item, or the window's default (65% of a
 * window with a width of its own, 16 font heights of one that fits its contents) */
static float calc_item_width(void) {
    const window_t* w = g.current;
    float width;
    if (g.next_item_width_set)
        width = g.next_item_width;
    else if (w->size.x > 0.0f && !(w->flags & OVL_WINDOW_ALWAYS_AUTO_RESIZE))
        width = truncf_(w->size.x * 0.65f);
    else
        width = truncf_(g.font_size * 16.0f);
    if (width < 0.0f)
        width = maxf(1.0f, ovl_content_region_avail().x + width);
    return truncf_(width);
}

/* ImGui's InputTextEx for one line, as far as the pad reaches it (the overlays are given no
 * keyboard, characters or mouse): made active by cross, its text selected and the caret shown,
 * blinking; left and right its own while it is; let go by cross again (validated) or by circle
 * (navigation's cancel). Nothing edits the text, so none is written back. */
int ovl_input_text_with_hint(const char* label, const char* hint, char* buf, size_t buf_size) {
    window_t* w = g.current;
    const char* lend;
    const char* display;
    const char* display_end;
    ovl_vec2_t label_size, frame_size, frame_min, frame_max, total_max, draw_pos;
    uint32_t id;
    int state, requested, select_all = 0, clear = 0, render_cursor, render_selection, from_state;
    int displaying_hint;
    float clip[4], cursor_x = 0.0f;
    ovl_col_t text_col;
    (void)buf_size;
    if (!w)
        return 0;
    lend = label_end(label);
    id = hash_str(label, NULL, id_seed());
    label_size = ovl_calc_text_size(label, lend, 0.0f);
    frame_size = calc_item_size(v2(0.0f, 0.0f), calc_item_width(),
                                label_size.y + g.style.frame_padding.y * 2.0f);
    frame_min = w->cursor_pos;
    frame_max = v2(frame_min.x + frame_size.x, frame_min.y + frame_size.y);
    total_max =
        v2(frame_max.x + (label_size.x > 0.0f ? g.style.item_inner_spacing.x + label_size.x : 0.0f),
           frame_max.y);
    item_size(v2(total_max.x - frame_min.x, frame_size.y), g.style.frame_padding.y);
    if (!item_add(id, frame_min, total_max))
        return 0;

    state = g.input.id == id;
    requested = g.active_id != id && g.nav_activate_id == id;
    if (requested) {
        size_t len = strlen(buf);
        int recycle;
        if (len > OVL_INPUT_TEXT_MAX)
            len = OVL_INPUT_TEXT_MAX;
        /* The caret kept if it comes back to the same field with the same text */
        recycle = state && g.input.text_len == (int)len && memcmp(g.input.text, buf, len) == 0;
        g.input.id = id;
        g.input.text_len = (int)len;
        memcpy(g.input.text, buf, len);
        g.input.text[len] = '\0';
        g.input.cursor_anim = -0.30f;
        g.input.scroll_x = 0.0f;
        if (!recycle)
            g.input.cursor = g.input.select_start = g.input.select_end = 0;
        /* From navigation, everything is selected */
        select_all = 1;
        state = 1;
        set_active_id(id, w);
        g.active_id_source = g.nav_source;
        set_focus_id(id, w);
        focus_window(w);
        set_nav_cursor_visible_after_move();
    }
    if (g.active_id == id) {
        g.active_id_using_nav_dir |= (1u << DIR_LEFT) | (1u << DIR_RIGHT);
        if (g.input.cursor > g.input.text_len)
            g.input.cursor = g.input.text_len;
        if (g.input.select_start > g.input.text_len)
            g.input.select_start = g.input.text_len;
        if (g.input.select_end > g.input.text_len)
            g.input.select_end = g.input.text_len;
    }
    render_cursor = g.active_id == id;
    render_selection =
        state && (g.input.select_start != g.input.select_end || select_all) && render_cursor;
    from_state = render_cursor && state;
    displaying_hint = hint && (from_state ? g.input.text : buf)[0] == '\0';
    if (g.active_id == id) {
        if (select_all) {
            g.input.select_start = 0;
            g.input.cursor = g.input.select_end = g.input.text_len;
        }
        /* Cross again validates it: let go (not the press that made it active) */
        if (!g.active_id_just_activated && g.gamepad_connected &&
            key_pressed(OVL_KEY_PAD_FACE_DOWN, 0, 0))
            clear = 1;
        render_selection |= g.input.select_start != g.input.select_end && render_cursor;
    }
    if (g.active_id == id && clear)
        clear_active_id();

    render_nav_cursor(frame_min, frame_max, id, -1.0f);
    render_frame(frame_min, frame_max, ovl_color_u32(OVL_COLOR_FRAME_BG, 1.0f),
                 g.style.frame_rounding);
    draw_pos = v2(frame_min.x + g.style.frame_padding.x, frame_min.y + g.style.frame_padding.y);
    clip[0] = frame_min.x;
    clip[1] = frame_min.y;
    clip[2] = frame_max.x;
    clip[3] = frame_max.y;
    if (displaying_hint) {
        display = hint;
        display_end = hint + strlen(hint);
    } else if (from_state) {
        display = g.input.text;
        display_end = display + g.input.text_len;
    } else {
        display = buf;
        display_end = buf + strlen(buf);
    }
    if (render_cursor && state)
        cursor_x = ovl_font_calc_text_size(current_font(), g.font_size, 0.0f, display,
                                           display + (displaying_hint ? 0 : g.input.cursor))
                       .x;
    text_col = ovl_color_u32(displaying_hint ? OVL_COLOR_TEXT_DISABLED : OVL_COLOR_TEXT, 1.0f);
    ovl_push_clip(&w->dl, v2(clip[0], clip[1]), v2(clip[2], clip[3]), 1);
    if (render_selection && !displaying_hint) {
        const int a =
            g.input.select_start < g.input.select_end ? g.input.select_start : g.input.select_end;
        const int b =
            g.input.select_start < g.input.select_end ? g.input.select_end : g.input.select_start;
        if (a < b) {
            /* The selection's box: a pixel above the line to two below it */
            ovl_vec2_t mn, mx;
            mn.x = draw_pos.x - g.input.scroll_x + ovl_calc_text_size(display, display + a, 0.0f).x;
            mn.y = draw_pos.y - 1.0f;
            mx.x = mn.x + ovl_calc_text_size(display + a, display + b, 0.0f).x;
            mx.y = draw_pos.y + 2.0f + g.font_size;
            mn.x = minf(maxf(mn.x, clip[0]), clip[2]);
            mx.x = minf(maxf(mx.x, clip[0]), clip[2]);
            mn.y = minf(maxf(mn.y, clip[1]), clip[3]);
            mx.y = minf(maxf(mx.y, clip[1]), clip[3]);
            ovl_add_rect_filled(&w->dl, mn, mx, ovl_color_u32(OVL_COLOR_TEXT_SELECTED_BG, 1.0f),
                                0.0f, 0);
        }
    }
    if (display != display_end && (text_col & 0xFF000000u))
        render_text(v2(draw_pos.x - g.input.scroll_x * (float)(render_cursor || render_selection),
                       draw_pos.y),
                    display, display_end, 0.0f, text_col);
    if (render_cursor) {
        const float x = truncf_(draw_pos.x + cursor_x - g.input.scroll_x);
        const float y = truncf_(draw_pos.y + g.font_size);
        const float y0 = y - g.font_size + 0.5f, y1 = y - 1.5f;
        g.input.cursor_anim += g.delta;
        if ((g.input.cursor_anim <= 0.0f || fmod(g.input.cursor_anim, 1.20f) <= 0.80f) &&
            x < clip[2] && x + 1.0f > clip[0] && y0 < clip[3] && y1 > clip[1]) {
            /* ImDrawList::AddLineV */
            ovl_vec2_t pts[2];
            pts[0] = v2(x + 0.5f, y0 + 0.5f);
            pts[1] = v2(x + 0.5f, y1 + 0.5f);
            ovl_add_polyline(&w->dl, pts, 2, ovl_color_u32(OVL_COLOR_INPUT_TEXT_CURSOR, 1.0f), 0,
                             1.0f);
        }
    }
    ovl_pop_clip(&w->dl);
    if (label_size.x > 0.0f)
        render_text(
            v2(frame_max.x + g.style.item_inner_spacing.x, frame_min.y + g.style.frame_padding.y),
            label, lend, 0.0f, ovl_color_u32(OVL_COLOR_TEXT, 1.0f));
    return 0;
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
    if (!item_add(id, pos, mx))
        return 0;
    return button_behavior(id, &hovered, &held);
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
    if (!item_add(id, mn, mx))
        return 0;
    pressed = button_behavior(id, &hovered, &held);
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
