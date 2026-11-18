/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

/* The overlays drawn over the game's picture (the FPS counter, notifications, the system
 * dialogs a game opens), without ImGui: an immediate-mode interface built each frame, drawn in
 * one pass of textured triangles. It keeps ImGui's look: its dark style's colours and sizes,
 * its window and widget layout, its anti-aliased shapes, the same fonts at the same sizes, so
 * that what was drawn with ImGui is drawn the same.
 *
 * ovl_draw: draw lists (vertices, indices, commands, clip rectangles) and ImGui's shapes.
 * ovl_font: text, rasterized by libretro-common's rtt into one atlas as glyphs are first
 *           drawn, read in place from the fonts the core carries.
 * ovl_ui:   windows, layout and widgets, keyboard and gamepad navigation.
 *
 * Not thread-safe: one thread builds the frame (the presenter's). Memory is taken at
 * ovl_create and kept; a frame allocates nothing unless it draws more than any before it. */

#ifndef SHADPS4_OVL_H
#define SHADPS4_OVL_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct ovl_vec2 {
    float x, y;
} ovl_vec2_t;

typedef struct ovl_vec4 {
    float x, y, z, w;
} ovl_vec4_t;

/* A colour as ImGui packs it: R in the low byte, then G, B and A */
typedef uint32_t ovl_col_t;
#define OVL_COL(r, g, b, a)                                                                        \
    (((ovl_col_t)(a) << 24) | ((ovl_col_t)(b) << 16) | ((ovl_col_t)(g) << 8) | (ovl_col_t)(r))
#define OVL_COL_WHITE OVL_COL(255, 255, 255, 255)
ovl_col_t ovl_col_from_vec4(ovl_vec4_t c);
ovl_vec4_t ovl_vec4_from_col(ovl_col_t c);

/* A texture a draw samples: OVL_TEX_ATLAS for the font atlas (and untextured shapes), else
 * the renderer's own handle for an image */
typedef uint64_t ovl_tex_t;
#define OVL_TEX_ATLAS ((ovl_tex_t)0)

/* ImGui's vertex: position in pixels, texture coordinate, colour */
typedef struct ovl_vertex {
    float x, y;
    float u, v;
    ovl_col_t col;
} ovl_vertex_t;

/* A run of triangles with one texture and clip rectangle (x0, y0, x1, y1 in pixels) */
typedef struct ovl_cmd {
    ovl_tex_t tex;
    float clip[4];
    uint32_t idx_offset;
    uint32_t idx_count;
} ovl_cmd_t;

/* What a frame draws, in order, in one vertex and one index buffer */
typedef struct ovl_draw_data {
    const ovl_vertex_t* vtx;
    uint32_t vtx_count;
    const uint32_t* idx;
    uint32_t idx_count;
    const ovl_cmd_t* cmds;
    uint32_t cmd_count;
    float display_w, display_h;
} ovl_draw_data_t;

/* ------------------------------------------------------------------------------------------ */
/* Draw lists                                                                                  */

typedef struct ovl_draw_list ovl_draw_list_t;

/* Rounded corners, as ImGui's ImDrawFlags_RoundCorners* (0: all) */
#define OVL_CORNERS_TL 0x10u
#define OVL_CORNERS_TR 0x20u
#define OVL_CORNERS_BL 0x40u
#define OVL_CORNERS_BR 0x80u
#define OVL_CORNERS_NONE 0x100u
#define OVL_CORNERS_ALL 0xF0u
#define OVL_CLOSED 0x1u

void ovl_push_clip(ovl_draw_list_t* dl, ovl_vec2_t min, ovl_vec2_t max, int intersect);
void ovl_pop_clip(ovl_draw_list_t* dl);
void ovl_add_line(ovl_draw_list_t* dl, ovl_vec2_t a, ovl_vec2_t b, ovl_col_t col, float thickness);
void ovl_add_rect(ovl_draw_list_t* dl, ovl_vec2_t min, ovl_vec2_t max, ovl_col_t col,
                  float rounding, unsigned flags, float thickness);
void ovl_add_rect_filled(ovl_draw_list_t* dl, ovl_vec2_t min, ovl_vec2_t max, ovl_col_t col,
                         float rounding, unsigned flags);
void ovl_add_rect_filled_multicolor(ovl_draw_list_t* dl, ovl_vec2_t min, ovl_vec2_t max,
                                    ovl_col_t tl, ovl_col_t tr, ovl_col_t br, ovl_col_t bl);
void ovl_add_circle(ovl_draw_list_t* dl, ovl_vec2_t center, float radius, ovl_col_t col,
                    int segments, float thickness);
void ovl_add_circle_filled(ovl_draw_list_t* dl, ovl_vec2_t center, float radius, ovl_col_t col,
                           int segments);
void ovl_add_triangle_filled(ovl_draw_list_t* dl, ovl_vec2_t a, ovl_vec2_t b, ovl_vec2_t c,
                             ovl_col_t col);
void ovl_add_polyline(ovl_draw_list_t* dl, const ovl_vec2_t* points, int count, ovl_col_t col,
                      unsigned flags, float thickness);
void ovl_add_convex_poly_filled(ovl_draw_list_t* dl, const ovl_vec2_t* points, int count,
                                ovl_col_t col);
void ovl_add_image(ovl_draw_list_t* dl, ovl_tex_t tex, ovl_vec2_t min, ovl_vec2_t max,
                   ovl_vec2_t uv_min, ovl_vec2_t uv_max, ovl_col_t col);
void ovl_add_image_rounded(ovl_draw_list_t* dl, ovl_tex_t tex, ovl_vec2_t min, ovl_vec2_t max,
                           ovl_vec2_t uv_min, ovl_vec2_t uv_max, ovl_col_t col, float rounding,
                           unsigned flags);

/* ------------------------------------------------------------------------------------------ */
/* Fonts                                                                                       */

typedef struct ovl_font ovl_font_t;

/* The faces a font is made of, tried in turn for each character: bytes lent for as long as
 * the overlay lives (a TrueType file or collection, face @index) */
typedef struct ovl_face_desc {
    const uint8_t* data;
    size_t size;
    int index;
} ovl_face_desc_t;

/* A font of @faces at @size pixels (ImGui's pixel height: ascent to descent) */
ovl_font_t* ovl_font_add(const ovl_face_desc_t* faces, int count, float size);
float ovl_font_size(const ovl_font_t* font);
/* The size of @text (UTF-8, to @end or its end when null) at @size, wrapped at @wrap_width
 * when it is above 0, as ImGui's CalcTextSize */
ovl_vec2_t ovl_font_calc_text_size(ovl_font_t* font, float size, float wrap_width, const char* text,
                                   const char* end);
void ovl_add_text(ovl_draw_list_t* dl, ovl_font_t* font, float size, ovl_vec2_t pos, ovl_col_t col,
                  const char* text, const char* end, float wrap_width);

/* The atlas: OVL_ATLAS_SIZE square, two bytes a texel: the coverage, then 255 where a glyph,
 * line or the white block was placed (0 between them). Sampled as (G, G, G, R) it is ImGui's
 * RGBA atlas exactly, edges included. With the rectangle changed since it was last taken (x,
 * y, w, h) for the renderer to upload */
#define OVL_ATLAS_SIZE 2048
const uint8_t* ovl_atlas_pixels(void);
/* What changed since last taken: @cleared when the atlas was emptied (the copy is to be
 * cleared to 0 first), and the rectangle written since (w 0 when none). 0 when nothing */
int ovl_atlas_take_dirty(int rect[4], int* cleared);
/* Puts a rectangle back to be taken again (what could not be uploaded this frame) */
void ovl_atlas_mark_dirty(int x, int y, int w, int h);

/* ------------------------------------------------------------------------------------------ */
/* Images                                                                                      */

/* Images live in a fixed set of slots, OVL_TEX_DIM square at most, whose pixels (RGBA,
 * OVL_TEX_DIM x OVL_TEX_DIM, rows OVL_TEX_DIM pixels apart) the renderer lends when it starts.
 * A handle is a slot's number from 1, and the ovl_tex_t its draws sample; 0 is none. */
#define OVL_TEX_SLOTS 12
#define OVL_TEX_DIM 512

/* Icons (a save's icon0.png, 228 x 128) have cells of their own in one image, the icon atlas,
 * eight by eight cells: an icon up to 230 x 130 in each, with a texel all round it. Its memory
 * (RGBA, OVL_ICON_ATLAS_W x OVL_ICON_ATLAS_H) is lent by the renderer when it starts. An
 * icon's handle follows the slots'; their draws sample OVL_TEX_ICONS. */
#define OVL_ICON_CELLS 64
#define OVL_ICON_COLS 8
#define OVL_ICON_CELL_W 232
#define OVL_ICON_CELL_H 132
#define OVL_ICON_ATLAS_W (OVL_ICON_COLS * OVL_ICON_CELL_W)
#define OVL_ICON_ATLAS_H ((OVL_ICON_CELLS / OVL_ICON_COLS) * OVL_ICON_CELL_H)
#define OVL_TEX_ICONS ((ovl_tex_t)(OVL_TEX_SLOTS + OVL_ICON_CELLS + 1))

/* From any thread: decodes a PNG into a free slot, read where it lies; 0 when no slot is free,
 * it is not a PNG, or it is larger than a slot */
int ovl_tex_decode_png(const uint8_t* png, size_t size);
/* The same into an icon's cell, or a slot when it is larger than a cell or none is free */
int ovl_tex_decode_icon(const uint8_t* png, size_t size);
/* From any thread: the image's owner is done with it */
void ovl_tex_release(int handle);
/* The image's size, when it is there to draw (decoded, uploaded or about to be) */
int ovl_tex_size(int handle, int* w, int* h);

/* The renderer's: lends the slots' pixels; a decoded image to copy to its slot's texture
 * (the whole slot), 0 when none; and slots released, freed once @frames_in_flight frames
 * have passed since */
void ovl_tex_init(uint8_t* const* pixels, int count);
void ovl_tex_init_icons(uint8_t* atlas);
void ovl_tex_shutdown(void);
int ovl_tex_take_upload(void);
/* For an icon taken to upload: the part of the atlas to copy, x, y, w, h (its cell's, the
 * icon and its border); 0 for a slot's image, copied whole */
int ovl_tex_icon_rect(int handle, int rect[4]);
const uint8_t* ovl_tex_pixels(int handle);
void ovl_tex_collect(unsigned frame, unsigned frames_in_flight);

/* ------------------------------------------------------------------------------------------ */
/* Frames and input                                                                            */

/* Keys and buttons the overlays read, keyboard and gamepad as ImGui names them */
enum ovl_key {
    OVL_KEY_NONE = 0,
    OVL_KEY_LEFT,
    OVL_KEY_RIGHT,
    OVL_KEY_UP,
    OVL_KEY_DOWN,
    OVL_KEY_ENTER,
    OVL_KEY_ESCAPE,
    OVL_KEY_BACKSPACE,
    OVL_KEY_DELETE,
    OVL_KEY_TAB,
    OVL_KEY_SPACE,
    OVL_KEY_HOME,
    OVL_KEY_END,
    OVL_KEY_F10,
    OVL_KEY_PAD_FACE_DOWN,  /* cross */
    OVL_KEY_PAD_FACE_RIGHT, /* circle */
    OVL_KEY_PAD_FACE_LEFT,  /* square */
    OVL_KEY_PAD_FACE_UP,    /* triangle */
    OVL_KEY_PAD_DPAD_LEFT,
    OVL_KEY_PAD_DPAD_RIGHT,
    OVL_KEY_PAD_DPAD_UP,
    OVL_KEY_PAD_DPAD_DOWN,
    OVL_KEY_PAD_L1,
    OVL_KEY_PAD_R1,
    OVL_KEY_PAD_L2,
    OVL_KEY_PAD_R2,
    OVL_KEY_PAD_START,
    OVL_KEY_PAD_BACK,
    OVL_KEY_PAD_LSTICK_LEFT, /* analog: down past the dead zone, and how far */
    OVL_KEY_PAD_LSTICK_RIGHT,
    OVL_KEY_PAD_LSTICK_UP,
    OVL_KEY_PAD_LSTICK_DOWN,
    OVL_KEY_COUNT
};

/* Made once; @default_font is what text is drawn with until pushed otherwise */
int ovl_create(ovl_font_t* default_font, float font_scale);
void ovl_destroy(void);

/* Input for the next frame, as ImGui's AddKeyEvent and AddKeyAnalogEvent; a gamepad's
 * buttons move the focus and press only while one is connected (ImGui's HasGamepad) */
void ovl_key_event(enum ovl_key key, int down);
void ovl_key_analog_event(enum ovl_key key, int down, float value);
void ovl_set_gamepad_connected(int connected);
/* From any thread: the pad taken from the game while an overlay has it (the invitation prompt
 * focused, the keyboard showing), counted; whether any has */
void ovl_pad_capture_acquire(void);
void ovl_pad_capture_release(void);
int ovl_pad_captured(void);
void ovl_char_event(uint32_t codepoint);
void ovl_new_frame(float display_w, float display_h, float delta_time);
/* Ends the frame: what to draw, valid until the next ovl_new_frame */
const ovl_draw_data_t* ovl_render(void);
/* What the last ovl_render made, until the next ovl_new_frame */
const ovl_draw_data_t* ovl_draw_data(void);
double ovl_time(void);
float ovl_delta_time(void);
ovl_vec2_t ovl_display_size(void);

/* As ImGui's IsKeyPressed(key, repeat): on the press, and with @repeat after 0.275 s every
 * 0.050 s */
int ovl_is_key_pressed(enum ovl_key key, int repeat);
int ovl_is_key_down(enum ovl_key key);

/* Navigation, as ImGui's: whether a window that takes navigation has the focus (io.NavActive);
 * the focus cursor kept shown (NavCursorVisible); the focus put on an item, the last one for
 * 0, as the core's SetItemCurrentNavFocus did */
int ovl_nav_active(void);
void ovl_keep_nav_highlight(void);
void ovl_set_item_current_nav_focus(uint32_t id);
uint32_t ovl_last_item_id(void);

/* ------------------------------------------------------------------------------------------ */
/* Style                                                                                       */

/* ImGui's colours, in its order, and its dark style's values */
enum ovl_color {
    OVL_COLOR_TEXT,
    OVL_COLOR_TEXT_DISABLED,
    OVL_COLOR_WINDOW_BG,
    OVL_COLOR_CHILD_BG,
    OVL_COLOR_POPUP_BG,
    OVL_COLOR_BORDER,
    OVL_COLOR_BORDER_SHADOW,
    OVL_COLOR_FRAME_BG,
    OVL_COLOR_FRAME_BG_HOVERED,
    OVL_COLOR_FRAME_BG_ACTIVE,
    OVL_COLOR_TITLE_BG,
    OVL_COLOR_TITLE_BG_ACTIVE,
    OVL_COLOR_TITLE_BG_COLLAPSED,
    OVL_COLOR_MENU_BAR_BG,
    OVL_COLOR_SCROLLBAR_BG,
    OVL_COLOR_SCROLLBAR_GRAB,
    OVL_COLOR_SCROLLBAR_GRAB_HOVERED,
    OVL_COLOR_SCROLLBAR_GRAB_ACTIVE,
    OVL_COLOR_CHECK_MARK,
    OVL_COLOR_SLIDER_GRAB,
    OVL_COLOR_SLIDER_GRAB_ACTIVE,
    OVL_COLOR_BUTTON,
    OVL_COLOR_BUTTON_HOVERED,
    OVL_COLOR_BUTTON_ACTIVE,
    OVL_COLOR_HEADER,
    OVL_COLOR_HEADER_HOVERED,
    OVL_COLOR_HEADER_ACTIVE,
    OVL_COLOR_SEPARATOR,
    OVL_COLOR_SEPARATOR_HOVERED,
    OVL_COLOR_SEPARATOR_ACTIVE,
    OVL_COLOR_TAB,
    OVL_COLOR_TAB_HOVERED,
    OVL_COLOR_TAB_SELECTED,
    OVL_COLOR_PLOT_HISTOGRAM,
    OVL_COLOR_TEXT_SELECTED_BG,
    OVL_COLOR_NAV_CURSOR,
    OVL_COLOR_MODAL_WINDOW_DIM_BG,
    /* After ImGui's own order: a ticked checkbox's square, a text field's caret */
    OVL_COLOR_CHECKBOX_SELECTED_BG,
    OVL_COLOR_INPUT_TEXT_CURSOR,
    OVL_COLOR_COUNT
};

enum ovl_style_var {
    OVL_VAR_ALPHA,
    OVL_VAR_WINDOW_PADDING,
    OVL_VAR_WINDOW_ROUNDING,
    OVL_VAR_WINDOW_BORDER_SIZE,
    OVL_VAR_CHILD_ROUNDING,
    OVL_VAR_CHILD_BORDER_SIZE,
    OVL_VAR_FRAME_PADDING,
    OVL_VAR_FRAME_ROUNDING,
    OVL_VAR_FRAME_BORDER_SIZE,
    OVL_VAR_ITEM_SPACING,
    OVL_VAR_ITEM_INNER_SPACING,
    OVL_VAR_BUTTON_TEXT_ALIGN,
    OVL_VAR_COUNT
};

void ovl_push_color(enum ovl_color idx, ovl_vec4_t col);
void ovl_pop_color(int count);
void ovl_push_var_float(enum ovl_style_var var, float value);
void ovl_push_var_vec2(enum ovl_style_var var, ovl_vec2_t value);
void ovl_pop_var(int count);
ovl_vec4_t ovl_style_color(enum ovl_color idx);
/* A style colour with the current alpha applied, as ImGui's GetColorU32 */
ovl_col_t ovl_color_u32(enum ovl_color idx, float alpha_mul);
/* A colour with the current alpha applied, as ImGui's GetColorU32(ImVec4) */
ovl_col_t ovl_color_u32_vec4(ovl_vec4_t col);

/* ------------------------------------------------------------------------------------------ */
/* Windows                                                                                     */

#define OVL_WINDOW_NO_TITLE_BAR 0x1u
#define OVL_WINDOW_NO_RESIZE 0x2u
#define OVL_WINDOW_NO_MOVE 0x4u
#define OVL_WINDOW_NO_SCROLLBAR 0x8u
#define OVL_WINDOW_NO_SCROLL_WITH_MOUSE 0x10u
#define OVL_WINDOW_NO_COLLAPSE 0x20u
#define OVL_WINDOW_ALWAYS_AUTO_RESIZE 0x40u
#define OVL_WINDOW_NO_BACKGROUND 0x80u
#define OVL_WINDOW_NO_SAVED_SETTINGS 0x100u
#define OVL_WINDOW_NO_MOUSE_INPUTS 0x200u
#define OVL_WINDOW_NO_NAV_INPUTS 0x10000u
#define OVL_WINDOW_NO_NAV_FOCUS 0x20000u
#define OVL_WINDOW_NO_INPUTS 0x30200u
#define OVL_WINDOW_NO_FOCUS_ON_APPEARING 0x1000u
#define OVL_WINDOW_NO_BRING_TO_FRONT 0x2000u
#define OVL_WINDOW_NO_NAV 0x30000u
#define OVL_WINDOW_NO_DECORATION 0x2Bu
#define OVL_WINDOW_HORIZONTAL_SCROLLBAR 0x800u
#define OVL_WINDOW_MODAL 0x100000u

#define OVL_COND_ALWAYS 1
#define OVL_COND_ONCE 2
#define OVL_COND_APPEARING 8

void ovl_set_next_window_pos(ovl_vec2_t pos, int cond, ovl_vec2_t pivot);
void ovl_set_next_window_size(ovl_vec2_t size, int cond);
void ovl_set_next_window_bg_alpha(float alpha);
void ovl_set_next_window_focus(void);
/* 0 when the window is hidden (its contents are not to be built); ovl_end either way */
int ovl_begin(const char* name, unsigned flags);
void ovl_end(void);
/* ImGui's BeginChild: its child flags, and its window's */
#define OVL_CHILD_BORDERS 0x1u
#define OVL_CHILD_ALWAYS_USE_WINDOW_PADDING 0x2u
#define OVL_CHILD_NAV_FLATTENED 0x100u
int ovl_begin_child(const char* id, ovl_vec2_t size, unsigned child_flags, unsigned flags);
void ovl_end_child(void);

ovl_vec2_t ovl_window_pos(void);
ovl_vec2_t ovl_window_size(void);
ovl_vec2_t ovl_window_padding(void);
void ovl_set_window_pos(ovl_vec2_t pos);
void ovl_set_window_font_scale(float scale);
ovl_draw_list_t* ovl_window_draw_list(void);
ovl_draw_list_t* ovl_foreground_draw_list(void);
ovl_draw_list_t* ovl_background_draw_list(void);

/* ------------------------------------------------------------------------------------------ */
/* Layout                                                                                      */

ovl_vec2_t ovl_content_region_avail(void);
ovl_vec2_t ovl_cursor_pos(void);
void ovl_set_cursor_pos(ovl_vec2_t pos);
void ovl_set_cursor_pos_x(float x);
void ovl_set_cursor_pos_y(float y);
ovl_vec2_t ovl_cursor_screen_pos(void);
void ovl_set_cursor_screen_pos(ovl_vec2_t pos);
ovl_vec2_t ovl_cursor_start_pos(void);
void ovl_same_line(float offset_from_start_x, float spacing);
void ovl_new_line(void);
void ovl_spacing(void);
void ovl_dummy(ovl_vec2_t size);
void ovl_separator(void);
void ovl_indent(float width);
void ovl_unindent(float width);
void ovl_begin_group(void);
void ovl_end_group(void);
void ovl_push_text_wrap_pos(float wrap_local_pos_x);
void ovl_pop_text_wrap_pos(void);
void ovl_push_font(ovl_font_t* font);
void ovl_pop_font(void);
float ovl_font_current_size(void);
/* The font text is drawn with now (ImGui's GetFont), for text a layer places itself with
 * ovl_add_text and measures with ovl_font_calc_text_size; and the style's frame padding */
ovl_font_t* ovl_font_current(void);
ovl_vec2_t ovl_frame_padding(void);
float ovl_scrollbar_size(void);
float ovl_text_line_height(void);
float ovl_text_line_height_with_spacing(void);
float ovl_frame_height(void);
float ovl_frame_height_with_spacing(void);
ovl_vec2_t ovl_calc_text_size(const char* text, const char* end, float wrap_width);
/* The width text at @pos wraps at, for a wrap position as ovl_push_text_wrap_pos takes it
 * (0: the window's right edge; below 0: no wrapping, 0 returned), as ImGui's
 * CalcWrapWidthForPos */
float ovl_calc_wrap_width_for_pos(ovl_vec2_t pos, float wrap_pos_x);
void ovl_set_scroll_y(float y);
void ovl_set_scroll_x(float x);
float ovl_scroll_y(void);
float ovl_scroll_max_y(void);
void ovl_set_scroll_here_y(float center_ratio);

/* ------------------------------------------------------------------------------------------ */
/* Items                                                                                       */

uint32_t ovl_get_id(const char* str);
uint32_t ovl_get_id_int(int value);
void ovl_push_id_str(const char* str);
void ovl_push_id_int(int value);
void ovl_push_id_ptr(const void* ptr);
void ovl_pop_id(void);

void ovl_text(const char* text, const char* end);
void ovl_text_colored(ovl_vec4_t col, const char* text, const char* end);
void ovl_text_disabled(const char* text, const char* end);
void ovl_text_wrapped(const char* text, const char* end);
void ovl_bullet_text(const char* text, const char* end);
int ovl_button(const char* label, ovl_vec2_t size);
int ovl_invisible_button(const char* id, ovl_vec2_t size);
int ovl_selectable(const char* label, int selected, ovl_vec2_t size);
void ovl_image(ovl_tex_t tex, ovl_vec2_t size, ovl_vec2_t uv0, ovl_vec2_t uv1);
void ovl_progress_bar(float fraction, ovl_vec2_t size, const char* overlay);
/* ImGui's Checkbox: 1 when cross changed *@v */
int ovl_checkbox(const char* label, int* v);
/* ImGui's SetNextItemWidth, for the next item that takes a width (a text field): below 0, that
 * far short of the window's right edge */
void ovl_set_next_item_width(float width);
/* ImGui's InputTextWithHint, a line of text (UTF-8, @buf_size bytes with its end) in a frame,
 * @hint shown while it is empty (null: none). Cross on it makes it the active item: the caret
 * shows and blinks, the d-pad's left and right are its own, and cross again or circle lets it
 * go. The overlays are given no keyboard and no characters (the pad only), so nothing typed
 * reaches it: what it holds is what @buf holds. 1 when it changed @buf. A text up to
 * OVL_INPUT_TEXT_MAX bytes while it is active. */
#define OVL_INPUT_TEXT_MAX 4096
int ovl_input_text_with_hint(const char* label, const char* hint, char* buf, size_t buf_size);
/* A text shown in a text field's frame and never edited there: ImGui's InputTextEx with
 * ReadOnly, on an item navigation does not go to (ImGuiItemFlags_NoNav), so never the active
 * item. @hint shown while @text is empty (null: none). @size as ImGui's size_arg (0: the
 * item width, a line's height; with @multiline, eight lines'). With @multiline the text's own
 * line breaks are kept, the frame is a child window as ImGui's is, and a text taller than it
 * gets that window's scrollbar; it stays at its top. Unlike ImGui's, the text in it is at the
 * size of the window's text (ovl_set_window_font_scale), as on one line. */
void ovl_text_field(const char* label, const char* hint, const char* text, ovl_vec2_t size,
                    int multiline);
/* The same with the text moved left and up by @scroll pixels, this frame: for whoever keeps a
 * caret in view. The text starts ovl_frame_padding() inside the frame; a line is
 * ovl_text_line_height() high; on several lines a text taller than the frame loses
 * ovl_scrollbar_size() of its width to the scrollbar. */
void ovl_text_field_scrolled(const char* label, const char* hint, const char* text, ovl_vec2_t size,
                             int multiline, ovl_vec2_t scroll);

int ovl_is_item_hovered(void);
int ovl_is_item_active(void);
int ovl_is_item_clicked(void);
int ovl_is_item_focused(void);
ovl_vec2_t ovl_item_rect_min(void);
ovl_vec2_t ovl_item_rect_max(void);
void ovl_set_item_default_focus(void);

/* ImGui's internals, for widgets of the overlays' own: ItemSize, ItemAdd (0 when it is clipped
 * away, not to be drawn), ButtonBehavior (for the pad: hovered while focused), RenderNavCursor
 * (rounded as frames are) and RenderText */
void ovl_item_size(ovl_vec2_t size, float text_baseline_y);
int ovl_item_add(uint32_t id, ovl_vec2_t min, ovl_vec2_t max);
int ovl_button_behavior(uint32_t id, int* hovered, int* held);
void ovl_render_nav_cursor(ovl_vec2_t min, ovl_vec2_t max, uint32_t id);
void ovl_render_text(ovl_vec2_t pos, const char* text, const char* end);
void ovl_begin_disabled(int disabled);
void ovl_end_disabled(void);

#ifdef __cplusplus
}
#endif

#endif
