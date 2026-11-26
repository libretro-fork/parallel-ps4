/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

/* The on-screen keyboard's keys: the layouts (letters, symbols, accented letters), the sizes of
 * the panel's parts, the colours, and the grid of keys as it is drawn and moved about in. Used
 * by libSceIme's and libSceImeDialog's keyboards. */

#ifndef SHADPS4_LIBRARIES_IME_KB_LAYOUT_H
#define SHADPS4_LIBRARIES_IME_KB_LAYOUT_H

#include <stddef.h>

#include <boolean.h>

#include "core/libraries/ime/ime_common.h"
#include "video_core/overlay/ovl.h"

#ifdef __cplusplus
extern "C" {
#endif

/* The most rows and columns a grid of keys has (the layouts have 6 or 7 rows of 10) */
#define IME_KB_GRID_ROWS_MAX 12
#define IME_KB_GRID_COLS_MAX 16

typedef struct ImeViewportMetrics {
    ovl_vec2_t size;
    ovl_vec2_t offset;
    float scale_x;
    float scale_y;
    float ui_scale;
    float base_w;
    float base_h;
} ImeViewportMetrics;

/* Made by MakeImeKbGridLayout: 10 columns, 6 rows */
typedef struct ImeKbGridLayout {
    ovl_vec2_t pos;
    ovl_vec2_t size;
    float key_gap_x;
    float key_gap_y;
    /* Base row height used for non-fixed rows. */
    float key_h;
    /* Optional fixed-size bottom rows (used by accents/specials to preserve function-row
     * height). */
    float bottom_row_h;
    int fixed_bottom_rows;
    int cols;
    int rows;
    float corner_radius;
} ImeKbGridLayout;

/* Standardized OSK selection indexing:
 * - Keyboard grid uses zero-based row/column indexing.
 * - Top panel row placement is driven by ImeTopPanelLayoutConfig's row/row_span.
 * - Keyboard rows follow active ImeKbLayoutModel's rows.
 * - Function row count is driven by ImeKbLayoutModel's function_rows. */
#define IME_SELECTION_DEFAULT_TOP_PANEL_ROW 0
#define IME_SELECTION_DEFAULT_TOP_PANEL_ROWS 1
#define IME_SELECTION_DEFAULT_FUNCTION_ROWS 2
#define IME_SELECTION_PANEL_MIN_ROW 0
#define IME_SELECTION_KEYBOARD_MIN_ROW 0

/* The panel row the top panel is on, and the rows it takes, from its configuration */
int ImeSelectionPanelTopRowFromConfig(int top_panel_row);
int ImeSelectionPanelTopRowsFromConfig(int top_panel_rows);
/* The panel rows the keyboard's first and last rows are on */
int ImeSelectionPanelKeyboardMinRowForTopPanel(int top_panel_row, int top_panel_rows);
int ImeSelectionPanelKeyboardMaxRowForKeyboardRows(int keyboard_rows, int top_panel_row,
                                                   int top_panel_rows);
/* The rows at the bottom of a keyboard of @keyboard_rows that hold its function keys */
int ImeSelectionResolveFunctionRows(int keyboard_rows, int configured_function_rows);
/* The keyboard row a panel row is, within the keyboard */
int ImeSelectionPanelToKeyboardRow(int panel_row, int keyboard_rows, int top_panel_row,
                                   int top_panel_rows);

/* ImeKbLayoutFamily */
#define IME_KB_LAYOUT_FAMILY_LATIN 0
#define IME_KB_LAYOUT_FAMILY_SYMBOLS 1
#define IME_KB_LAYOUT_FAMILY_SPECIALS 2
typedef u8 ImeKbLayoutFamily;

/* ImeKbCaseState */
#define IME_KB_CASE_STATE_LOWER 0
#define IME_KB_CASE_STATE_UPPER 1
#define IME_KB_CASE_STATE_CAPS_LOCK 2
typedef u8 ImeKbCaseState;

/* ImeKbLayoutId */
#define IME_KB_LAYOUT_ID_LATIN_LOWER 0
#define IME_KB_LAYOUT_ID_LATIN_UPPER 1
#define IME_KB_LAYOUT_ID_LATIN_CAPS_LOCK 2
#define IME_KB_LAYOUT_ID_SYMBOLS_PAGE1 3
#define IME_KB_LAYOUT_ID_SYMBOLS_PAGE2 4
#define IME_KB_LAYOUT_ID_SPECIALS_PAGE1 5
#define IME_KB_LAYOUT_ID_SPECIALS_PAGE2 6
typedef u8 ImeKbLayoutId;

/* All zero is the Latin layout in lower case, its first page */
typedef struct ImeKbLayoutSelection {
    ImeKbLayoutFamily family;
    ImeKbCaseState case_state;
    u8 page;
} ImeKbLayoutSelection;

/* ImeKbKeyAction. NONE is a disabled key slot: it is not selectable and should not expose any
 * visible label. */
#define IME_KB_KEY_ACTION_NONE 0
#define IME_KB_KEY_ACTION_CHARACTER 1
#define IME_KB_KEY_ACTION_SHIFT 2
#define IME_KB_KEY_ACTION_SYMBOLS_MODE 3
#define IME_KB_KEY_ACTION_SPECIALS_MODE 4
#define IME_KB_KEY_ACTION_SPACE 5
#define IME_KB_KEY_ACTION_BACKSPACE 6
#define IME_KB_KEY_ACTION_ARROW_LEFT 7
#define IME_KB_KEY_ACTION_ARROW_RIGHT 8
#define IME_KB_KEY_ACTION_ARROW_UP 9
#define IME_KB_KEY_ACTION_ARROW_DOWN 10
#define IME_KB_KEY_ACTION_KEYBOARD 11
#define IME_KB_KEY_ACTION_MENU 12
#define IME_KB_KEY_ACTION_SETTINGS 13
#define IME_KB_KEY_ACTION_NEW_LINE 14
#define IME_KB_KEY_ACTION_DONE 15
#define IME_KB_KEY_ACTION_PAGE_PREV 16
#define IME_KB_KEY_ACTION_PAGE_NEXT 17
typedef u8 ImeKbKeyAction;

/* ImeKbKeyGlyph */
#define IME_KB_KEY_GLYPH_NONE 0
#define IME_KB_KEY_GLYPH_BACKSPACE 1
#define IME_KB_KEY_GLYPH_ARROW_LEFT 2
#define IME_KB_KEY_GLYPH_ARROW_RIGHT 3
#define IME_KB_KEY_GLYPH_ARROW_UP 4
#define IME_KB_KEY_GLYPH_ARROW_DOWN 5
#define IME_KB_KEY_GLYPH_SHIFT_OUTLINE 6
#define IME_KB_KEY_GLYPH_SHIFT_FILLED 7
#define IME_KB_KEY_GLYPH_CAPS_LOCK_FILLED 8
typedef u8 ImeKbKeyGlyph;

typedef struct ImeKbKeySpec {
    u8 row;
    u8 col;
    u8 col_span;
    u8 row_span;
    /* Disabled key contract:
     * - action == IME_KB_KEY_ACTION_NONE
     * - label == NULL
     * - hotkey_label == NULL
     * - glyph == IME_KB_KEY_GLYPH_NONE
     * This means the key loses both label visibility and functionality. */
    const char* label;
    const char* hotkey_label;
    ImeKbKeyAction action;
    ImeKbKeyGlyph glyph;
} ImeKbKeySpec;

typedef struct ImeKbLayoutModel {
    const ImeKbKeySpec* keys;
    size_t key_count;
    u8 cols;
    u8 rows;
    u8 function_rows;
} ImeKbLayoutModel;

/* ImeTopPanelElementId */
#define IME_TOP_PANEL_ELEMENT_ID_PREDICTION 0
#define IME_TOP_PANEL_ELEMENT_ID_CLOSE 1
typedef u8 ImeTopPanelElementId;

typedef struct ImeTopPanelElementSpec {
    ImeTopPanelElementId id;
    u8 col;
    u8 col_span;
} ImeTopPanelElementSpec;

typedef struct ImeTopPanelLayoutConfig {
    const ImeTopPanelElementSpec* elements;
    size_t element_count;
    u8 cols;
    u8 row;
    u8 row_span;
} ImeTopPanelLayoutConfig;

/* Mirrors OrbisImeParamExtended color buckets so UI styling can be themed through one config
 * and optionally overridden by game-provided SET_COLOR. Made by GetDefaultImeStyleConfig. */
typedef struct ImeStyleConfig {
    OrbisImeColor color_base;
    OrbisImeColor color_line;
    OrbisImeColor color_text_field;
    OrbisImeColor color_preedit;
    OrbisImeColor color_button_default;
    OrbisImeColor color_button_function;
    OrbisImeColor color_button_symbol;
    OrbisImeColor color_text;
    OrbisImeColor color_special;
} ImeStyleConfig;

/* Shared edge-wrap hold state for controller navigation. It tracks last successful move
 * direction/time and active wrap hold window. Made by MakeImeEdgeWrapNavState. */
typedef struct ImeEdgeWrapNavState {
    int last_step_row;
    int last_step_col;
    double last_step_time;
    int hold_step_row;
    int hold_step_col;
    double hold_release_time;
    bool hold_active;
} ImeEdgeWrapNavState;

ImeEdgeWrapNavState MakeImeEdgeWrapNavState(void);
void ResetImeEdgeWrapHold(ImeEdgeWrapNavState* state);
void ResetImeEdgeWrapNav(ImeEdgeWrapNavState* state);
bool ShouldDelayImeEdgeWrap(ImeEdgeWrapNavState* state, int step_row, int step_col,
                            bool repeat_hint, bool wraps, double now, double hold_delay_sec,
                            double repeat_window_sec);
void CommitImeEdgeWrapStep(ImeEdgeWrapNavState* state, int step_row, int step_col, double now);

/* The key that covers a cell of a layout's grid, NULL for none */
const ImeKbKeySpec* ResolveImeKeyboardKeyAt(const ImeKbLayoutModel* layout, int row, int col);
bool DoesImeKeyboardStepCrossGridEdge(int from_row, int from_col, int step_row, int step_col,
                                      int grid_rows, int grid_cols);
/* Whether moving from a cell by a step comes to its next key by way of the grid's edge */
bool DoesImeKeyboardNavigationWrap(const ImeKbLayoutModel* layout, int from_row, int from_col,
                                   int step_row, int step_col);

/* Made by MakeImeKbDrawParams */
typedef struct ImeKbDrawParams {
    ImeKbLayoutSelection selection;
    const ImeKbLayoutModel* layout_model;
    OrbisImeLanguage supported_languages;
    OrbisImeEnterLabel enter_label;
    bool show_selection_highlight;
    bool allow_nav_input;
    bool use_lstick_key_nav;
    bool allow_activate_input;
    bool external_nav_left;
    bool external_nav_right;
    bool external_nav_up;
    bool external_nav_down;
    bool external_nav_left_repeat;
    bool external_nav_right_repeat;
    bool external_nav_up_repeat;
    bool external_nav_down_repeat;
    bool external_activate_pressed;
    bool external_activate_repeat;
    bool reset_nav_state;
    int requested_selected_row;
    int requested_selected_col;
    ovl_col_t key_bg_default;
    ovl_col_t key_bg_function;
    ovl_col_t key_bg_symbol;
    ovl_col_t key_border;
    ovl_col_t key_done;
    ovl_col_t key_text;
    ovl_col_t key_hotkey_text;
} ImeKbDrawParams;

/* What DrawImeKeyboardGrid says of the frame; it sets every field */
typedef struct ImeKbDrawState {
    bool done_pressed;
    ImeKbKeyAction pressed_action;
    const char* pressed_label;
    u16 pressed_keycode;
    char16_t pressed_character;
    /* Logical grid cursor. If the cursor cell is disabled, selected_center points to the
     * visible nearest enabled fallback key used for drawing and activation. */
    int selected_row;
    int selected_col;
    ovl_vec2_t selected_center;
} ImeKbDrawState;

typedef struct ImePanelMetricsConfig {
    float panel_w;
    float panel_h;
    bool multiline;
    bool show_title;
    float base_font_size;
    ovl_vec2_t window_pos;
} ImePanelMetricsConfig;

typedef struct ImePanelMetrics {
    float panel_w;
    float panel_h;
    float padding_x;
    float padding_bottom;
    float label_h;
    float input_h;
    float predict_h;
    float close_w;
    float keys_h;
    float key_gap;
    float key_h;
    float corner_radius;
    float label_font_scale;
    float input_font_scale;
    float key_font_scale;
    float predict_gap;
    ovl_vec2_t input_pos_local;
    ovl_vec2_t input_size;
    ovl_vec2_t input_pos_screen;
    ovl_vec2_t predict_pos;
    ovl_vec2_t predict_size;
    ovl_vec2_t close_pos;
    ovl_vec2_t close_size;
    ovl_vec2_t kb_pos;
    ovl_vec2_t kb_size;
} ImePanelMetrics;

/* The selector's pulse as a key is pressed: how long, and how far it grows, in border widths */
#define kSelectorPressPulseDurationSec 0.2f
#define kSelectorPressPulseExpandBorderFactor 1.0f
/* How far the pulse has grown at @now: a half sine over its duration, 0 outside it */
float ComputePressPulseExpand(double pulse_started_at, double now, float pulse_duration_sec,
                              float max_expand_px);

/* The size of the picture the presenter last output, 0 by 0 before the first. Defined in
 * vk_presenter.cpp. */
void presenter_output_resolution(u32* width, u32* height);

ImeViewportMetrics ComputeImeViewportMetrics(bool use_over2k);
ImePanelMetrics ComputeImePanelMetrics(const ImePanelMetricsConfig* config);
ImeKbLayoutId ResolveImeKeyboardLayoutId(const ImeKbLayoutSelection* selection);
const ImeKbLayoutModel* GetImeKeyboardLayoutById(ImeKbLayoutId id);
const ImeKbLayoutModel* GetImeKeyboardLayout(const ImeKbLayoutSelection* selection);
/* Every layout has the one top panel: the prediction, then the close button */
const ImeTopPanelLayoutConfig* GetImeTopPanelLayoutConfig(const ImeKbLayoutSelection* selection);
ImeStyleConfig GetDefaultImeStyleConfig(void);
ImeStyleConfig ResolveImeStyleConfig(const OrbisImeParamExtended* extended);
ovl_col_t ImeColorToCol(const OrbisImeColor* color);
ovl_vec4_t ImeColorToVec4(const OrbisImeColor* color);
ImeKbGridLayout MakeImeKbGridLayout(void);
ImeKbDrawParams MakeImeKbDrawParams(void);
void ApplyImeStyleToKeyboardDrawParams(const ImeStyleConfig* style, ImeKbDrawParams* params);

/* Draws the grid of keys into the current window, moves its cursor by the pad and the
 * @params' own moves, and says in @state what was pressed. A grid of more than
 * IME_KB_GRID_ROWS_MAX by IME_KB_GRID_COLS_MAX is not drawn. */
void DrawImeKeyboardGrid(const ImeKbGridLayout* layout, const ImeKbDrawParams* params,
                         ImeKbDrawState* state);

#ifdef __cplusplus
}
#endif

#endif
