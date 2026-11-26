/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

/* What libSceIme's on-screen keyboard and libSceImeDialog's have in common around the grid of
 * keys (ime_kb_layout.h): the pad as the panel reads it, with its repeats and shortcuts; the
 * selector's fade; the caret in the text field; the edit menu; text between UTF-8 and the
 * console's UTF-16; and the keyboard's clipboard. */

#ifndef SHADPS4_LIBRARIES_IME_UI_SHARED_H
#define SHADPS4_LIBRARIES_IME_UI_SHARED_H

#include <stddef.h>

#include <boolean.h>

#include "common/types.h"
#include "core/libraries/ime/ime_kb_layout.h"
#include "core/libraries/ime/ime_support.h"
#include "core/libraries/pad/pad.h"
#include "core/libraries/system/userservice.h"
#include "video_core/overlay/ovl.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct VirtualPadSnapshot {
    u32 buttons;
    ovl_vec2_t left_stick;
    ovl_vec2_t panel_delta;
    float l2_analog;
} VirtualPadSnapshot;

#define kPanelEdgeWrapHoldDelaySec 0.5
#define kRepeatIntentWindowSec 0.45
#define kSelectorFadeOutDurationSec 0.2f

/* A press held repeats after this long and then this often, in seconds (ImGui's) */
#define kKeyRepeatDelay 0.275f
#define kKeyRepeatRate 0.050f

ovl_vec4_t BrightenColor(ovl_col_t color, float delta);
/* The character at @text (UTF-8, not its end) into @codepoint; the bytes it takes */
int Utf8Step(const char* text, u32* codepoint);
/* The UTF-16 units of @text up to @end, or to its end when @end is NULL */
int Utf16CountFromUtf8Range(const char* text, const char* end);
int Utf8ByteIndexFromUtf16Index(const char* text, int utf16_index);
/* ime_support.c's conversions, for the console's char16_t */
void Utf16ToUtf8(const char16_t* text, size_t length, char* out, size_t out_size);
void Utf8ToUtf16(const char* text, size_t length, char16_t* out, size_t out_size);
/* The keyboard's own clipboard: what Copy took, for Paste (empty: nothing yet) */
const char* ImeClipboardText(void);
/* @length bytes of @text, which need not end there */
void SetImeClipboardText(const char* text, size_t length);
/* How far a text field's text is moved left and up (ovl_text_field_scrolled) for its caret to
 * be in view: @scroll as it was, moved only as far as the caret needs */
void FollowCaretInTextField(ovl_vec2_t field_size, const char* text, int caret_byte, bool multiline,
                            ovl_vec2_t* scroll);
/* The caret over a text field's text, moved by @scroll with it */
void DrawInactiveCaretOverlay(ovl_vec2_t frame_min, ovl_vec2_t frame_max, const char* text,
                              int caret_byte, bool multiline, ovl_vec2_t scroll);

/* Made by MakeSelectorFadeState */
typedef struct SelectorFadeState {
    ovl_vec2_t current_min;
    ovl_vec2_t current_max;
    float current_corner_radius;
    bool current_visible;

    ovl_vec2_t previous_min;
    ovl_vec2_t previous_max;
    float previous_corner_radius;
    bool previous_visible;
    double previous_started_at;
    double press_pulse_started_at;
} SelectorFadeState;

SelectorFadeState MakeSelectorFadeState(void);
void TriggerSelectorPressPulse(SelectorFadeState* state, double now);
void UpdateSelectorFadeState(SelectorFadeState* state, ovl_vec2_t pos, ovl_vec2_t size, float inset,
                             float corner_radius, bool selected, double now);
void DrawSelectorFadeState(const SelectorFadeState* state, ovl_draw_list_t* draw_list,
                           ovl_col_t overlay_color, ovl_col_t border_color, float border_thickness,
                           float fade_duration_sec, double now, float current_expand_px);

ImeKbLayoutSelection ResolveInitialKbLayoutSelection(OrbisImeExtOption ext_option,
                                                     OrbisImePanelPriority panel_priority);
void InitializeDefaultOskSelectionAnchor(const ImeKbLayoutSelection* layout_selection,
                                         OrbisImeExtOption ext_option, int* pending_row,
                                         int* pending_col, int* last_row, int* last_col);
VirtualPadSnapshot ReadVirtualPadSnapshot(OrbisUserServiceUserId user_id, float delta_time);

/* The pad's state a keyboard keeps from frame to frame, where the keyboard keeps it */
typedef struct OskPadInputState {
    u32* prev_virtual_buttons;
    bool* prev_virtual_cross_down;
    bool* prev_virtual_lstick_left_down;
    bool* prev_virtual_lstick_right_down;
    bool* prev_virtual_lstick_up_down;
    bool* prev_virtual_lstick_down_down;
    int* left_stick_repeat_dir;
    double* left_stick_next_repeat_time;
    double* virtual_cross_next_repeat_time;
    bool* prev_virtual_dpad_left_down;
    bool* prev_virtual_dpad_right_down;
    bool* prev_virtual_dpad_up_down;
    bool* prev_virtual_dpad_down_down;
    double* virtual_dpad_left_next_repeat_time;
    double* virtual_dpad_right_next_repeat_time;
    double* virtual_dpad_up_next_repeat_time;
    double* virtual_dpad_down_next_repeat_time;
} OskPadInputState;

typedef struct OskStickDirs {
    bool left;
    bool right;
    bool up;
    bool down;
} OskStickDirs;

/* Made by ComputeOskPadInputFrame, which sets every field */
typedef struct OskPadInputFrame {
    u32 virtual_buttons;
    u32 prev_virtual_buttons;
    OskStickDirs virtual_lstick_dirs;
    bool cross_down;
    float l2_analog;
    bool panel_activate_pressed_raw;
    bool panel_activate_repeat_raw;
    bool virtual_nav_left;
    bool virtual_nav_right;
    bool virtual_nav_up;
    bool virtual_nav_down;
    bool virtual_nav_left_repeat;
    bool virtual_nav_right_repeat;
    bool virtual_nav_up_repeat;
    bool virtual_nav_down_repeat;
    bool stick_nav_left;
    bool stick_nav_right;
    bool stick_nav_up;
    bool stick_nav_down;
    bool stick_nav_left_repeat;
    bool stick_nav_right_repeat;
    bool stick_nav_up_repeat;
    bool stick_nav_down_repeat;
    bool nav_left;
    bool nav_right;
    bool nav_up;
    bool nav_down;
    bool nav_left_repeat;
    bool nav_right_repeat;
    bool nav_up_repeat;
    bool nav_down_repeat;
    bool virtual_control_input;
    bool osk_control_input;
} OskPadInputFrame;

/* A frame's pad as buttons are asked of it. Made by MakeOskVirtualPadInputView; a button is an
 * ORBIS_PAD_BUTTON_* bit. */
typedef struct OskVirtualPadInputView {
    const OskPadInputFrame* frame;
    double repeat_delay;
    double repeat_rate;
} OskVirtualPadInputView;

OskVirtualPadInputView MakeOskVirtualPadInputView(const OskPadInputFrame* frame);
bool OskVirtualPadDown(const OskVirtualPadInputView* view, u32 button);
bool OskVirtualPadPressed(const OskVirtualPadInputView* view, u32 button);
/* Pressed, or held long enough to repeat; @out_repeat (NULL: not wanted) says which */
bool OskVirtualPadRepeatPressed(const OskVirtualPadInputView* view, u32 button,
                                bool* prev_down_state, double* next_repeat_time, bool* out_repeat);

typedef struct OskShortcutRepeatState {
    bool* prev_square_down;
    bool* prev_l1_down;
    bool* prev_r1_down;
    bool* l2_shortcut_armed;
    double* square_next_repeat_time;
    double* l1_next_repeat_time;
    double* r1_next_repeat_time;
    double* triangle_next_repeat_time;
} OskShortcutRepeatState;

typedef struct OskShortcutActionResult {
    ImeKbKeyAction action;
    bool clear_all;
} OskShortcutActionResult;

OskShortcutActionResult EvaluateOskShortcutAction(bool allow_osk_shortcuts, bool menu_modal,
                                                  bool evaluate_action,
                                                  const OskPadInputFrame* panel_input,
                                                  const OskVirtualPadInputView* virtual_pad_input,
                                                  u32 prev_virtual_buttons,
                                                  ImeKbLayoutFamily layout_family,
                                                  const OskShortcutRepeatState* repeat_state);

void CycleKeyboardCaseState(ImeKbLayoutSelection* selection);
void ToggleKeyboardFamilyMode(ImeKbLayoutSelection* selection, ImeKbLayoutFamily* alpha_family,
                              ImeKbLayoutFamily target_family);
bool FocusKeyboardActionKeySelection(const ImeKbLayoutSelection* selection, ImeKbKeyAction action,
                                     int* out_row, int* out_col);
void FlipKeyboardModePage(ImeKbLayoutSelection* selection, int direction);

/* The panel's moves, as the grid of keys takes them */
void ApplyOskPanelNavToKeyboardParams(ImeKbDrawParams* kb_params, bool allow_osk_shortcuts,
                                      const OskPadInputFrame* panel_input);

OskPadInputFrame ComputeOskPadInputFrame(const VirtualPadSnapshot* virtual_pad,
                                         bool allow_osk_shortcuts, bool first_render,
                                         const OskPadInputState* state);
void CommitOskPadInputFrame(const OskPadInputFrame* frame, const OskPadInputState* state);

void DisarmMenuActivate(bool* menu_activate_armed);
void RearmMenuActivateOnRelease(bool activate_down, bool* menu_activate_armed);
bool ConsumeMenuActivatePress(bool panel_activate_pressed, bool opened_menu_this_frame,
                              bool* menu_activate_armed);

/* OskEditMenuPopup: the edit menu that is open */
#define OSK_EDIT_MENU_NONE 0
#define OSK_EDIT_MENU_MAIN 1
#define OSK_EDIT_MENU_ACTIONS 2
typedef u8 OskEditMenuPopup;

void OpenOskMainEditMenu(OskEditMenuPopup* popup, int* edit_menu_index, bool* menu_activate_armed);
void OpenOskActionsEditMenu(OskEditMenuPopup* popup, int* edit_menu_index,
                            bool* menu_activate_armed);
bool CloseOskEditMenuOnCancel(OskEditMenuPopup* popup, bool* cancel_pressed,
                              bool* menu_activate_armed);

/* An item of the edit menu was chosen: @source_popup is the menu, @action_index the item */
typedef void (*OskEditMenuApply)(void* ctx, OskEditMenuPopup source_popup, int action_index);

/* Draws the open edit menu over the keys and moves in it. @apply_action may open the other menu
 * or close this one through @popup. False when none is open. */
bool DrawAndHandleOskEditMenuPopup(OskEditMenuPopup* popup, int* edit_menu_index,
                                   const ImePanelMetrics* metrics, ovl_draw_list_t* draw,
                                   bool pointer_navigation_active, bool nav_up, bool nav_down,
                                   bool cross_down, bool panel_activate_pressed,
                                   bool opened_menu_this_frame, bool* menu_activate_armed,
                                   bool clipboard_ready, OskEditMenuApply apply_action, void* ctx);

#ifdef __cplusplus
}
#endif

#endif
