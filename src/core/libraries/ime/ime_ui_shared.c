/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <math.h>
#include <string.h>

#include <encodings/utf.h>

#include "core/libraries/ime/ime_ui_shared.h"
#include "input/controller.h"

#define UI_MIN(a, b) ((a) < (b) ? (a) : (b))
#define UI_MAX(a, b) ((a) > (b) ? (a) : (b))

/* StickNavDirection */
#define STICK_NAV_NONE 0
#define STICK_NAV_LEFT 1
#define STICK_NAV_RIGHT 2
#define STICK_NAV_UP 3
#define STICK_NAV_DOWN 4

#define kNavAxisThreshold 0.50f
#define kStickNavInitialDelaySlow 0.26
#define kStickNavInitialDelayFast 0.16
#define kStickNavRepeatSlow 0.20
#define kStickNavRepeatFast 0.11
#define kCaretBlinkCycleSec 1.20
#define kCaretBlinkOnSec 0.80
#define kAxisDeadzone 0.24f
/* pixels per second at full tilt */
#define kPanelMoveSpeed 900.0f

typedef struct VirtualButtonRepeatResult {
    bool pressed;
    bool repeat;
} VirtualButtonRepeatResult;

typedef struct StickNavPulseResult {
    bool left;
    bool right;
    bool up;
    bool down;
    bool left_repeat;
    bool right_repeat;
    bool up_repeat;
    bool down_repeat;
} StickNavPulseResult;

static char ime_clipboard[ORBIS_IME_MAX_TEXT_LENGTH * 4 + 1];

static int clamp_int(int v, int lo, int hi) {
    return v < lo ? lo : (hi < v ? hi : v);
}

static float clamp_float(float v, float lo, float hi) {
    return v < lo ? lo : (hi < v ? hi : v);
}

static ovl_vec2_t vec2(float x, float y) {
    ovl_vec2_t v;
    v.x = x;
    v.y = y;
    return v;
}

static bool IsCaretBlinkVisible(void) {
    return fmod(ovl_time(), kCaretBlinkCycleSec) <= kCaretBlinkOnSec;
}

static float ToAxisUnit(s32 axis) {
    return clamp_float(((float)axis - 128.0f) / 127.0f, -1.0f, 1.0f);
}

/* The state of the user's controller if it is connected, else of the first that is */
static bool ReadControllerState(OrbisUserServiceUserId user_id, input_state_t* out_state) {
    const int mapped = input_controller_index_from_user_id(user_id);
    int index;
    if (mapped >= 0 && mapped < INPUT_CONTROLLERS) {
        const input_state_t pad_state =
            input_controller_read_state(input_controller_get((unsigned)mapped));
        if (pad_state.connected) {
            *out_state = pad_state;
            return true;
        }
    }
    for (index = 0; index < INPUT_CONTROLLERS; ++index) {
        input_state_t pad_state;
        if (index == mapped) {
            continue;
        }
        pad_state = input_controller_read_state(input_controller_get((unsigned)index));
        if (pad_state.connected) {
            *out_state = pad_state;
            return true;
        }
    }
    return false;
}

const char* ImeClipboardText(void) {
    return ime_clipboard;
}

void SetImeClipboardText(const char* text, size_t length) {
    length = UI_MIN(length, sizeof(ime_clipboard) - 1);
    memcpy(ime_clipboard, text, length);
    ime_clipboard[length] = '\0';
}

int Utf8Step(const char* text, u32* codepoint) {
    const char* next = text;
    *codepoint = utf8_walk(&next);
    return (int)(next - text);
}

void Utf16ToUtf8(const char16_t* text, size_t length, char* out, size_t out_size) {
    ime_utf16_to_utf8((const unsigned short*)text, length, out, out_size);
}

void Utf8ToUtf16(const char* text, size_t length, char16_t* out, size_t out_size) {
    ime_utf8_to_utf16(text, length, (unsigned short*)out, out_size);
}

ovl_vec4_t BrightenColor(ovl_col_t color, float delta) {
    ovl_vec4_t out = ovl_vec4_from_col(color);
    out.x = clamp_float(out.x + delta, 0.0f, 1.0f);
    out.y = clamp_float(out.y + delta, 0.0f, 1.0f);
    out.z = clamp_float(out.z + delta, 0.0f, 1.0f);
    return out;
}

static ovl_col_t ScaleColorAlpha(ovl_col_t color, float alpha_scale) {
    ovl_vec4_t c = ovl_vec4_from_col(color);
    c.w = clamp_float(c.w * alpha_scale, 0.0f, 1.0f);
    return ovl_col_from_vec4(c);
}

SelectorFadeState MakeSelectorFadeState(void) {
    SelectorFadeState state;
    memset(&state, 0, sizeof(state));
    state.press_pulse_started_at = -1.0;
    return state;
}

void TriggerSelectorPressPulse(SelectorFadeState* state, double now) {
    state->press_pulse_started_at = now;
}

void UpdateSelectorFadeState(SelectorFadeState* state, ovl_vec2_t pos, ovl_vec2_t size, float inset,
                             float corner_radius, bool selected, double now) {
    const bool next_visible = selected && size.x > inset * 2.0f && size.y > inset * 2.0f;
    if (state->current_visible && !next_visible) {
        state->previous_min = state->current_min;
        state->previous_max = state->current_max;
        state->previous_corner_radius = state->current_corner_radius;
        state->previous_visible = true;
        state->previous_started_at = now;
    }

    state->current_visible = next_visible;
    if (!next_visible) {
        return;
    }
    state->current_min = vec2(pos.x + inset, pos.y + inset);
    state->current_max = vec2(pos.x + size.x - inset, pos.y + size.y - inset);
    state->current_corner_radius = UI_MAX(0.0f, corner_radius);
}

void DrawSelectorFadeState(const SelectorFadeState* state, ovl_draw_list_t* draw_list,
                           ovl_col_t overlay_color, ovl_col_t border_color, float border_thickness,
                           float fade_duration_sec, double now, float current_expand_px) {
    float expand;
    float corner_radius;
    ovl_vec2_t current_min;
    ovl_vec2_t current_max;
    if (!draw_list) {
        return;
    }

    if (state->previous_visible && fade_duration_sec > 0.0f) {
        const double elapsed = now - state->previous_started_at;
        if (elapsed >= 0.0 && elapsed < (double)fade_duration_sec) {
            const float alpha = 1.0f - (float)(elapsed / (double)fade_duration_sec);
            if (alpha > 0.0f) {
                ovl_add_rect_filled(draw_list, state->previous_min, state->previous_max,
                                    ScaleColorAlpha(overlay_color, alpha),
                                    state->previous_corner_radius, 0);
                ovl_add_rect(draw_list, state->previous_min, state->previous_max,
                             ScaleColorAlpha(border_color, alpha), state->previous_corner_radius, 0,
                             border_thickness);
            }
        }
    }

    if (!state->current_visible) {
        return;
    }
    expand = UI_MAX(0.0f, current_expand_px);
    current_min = vec2(state->current_min.x - expand, state->current_min.y - expand);
    current_max = vec2(state->current_max.x + expand, state->current_max.y + expand);
    corner_radius = state->current_corner_radius + expand;
    ovl_add_rect_filled(draw_list, current_min, current_max, overlay_color, corner_radius, 0);
    ovl_add_rect(draw_list, current_min, current_max, border_color, corner_radius, 0,
                 border_thickness);
}

ImeKbLayoutSelection ResolveInitialKbLayoutSelection(OrbisImeExtOption ext_option,
                                                     OrbisImePanelPriority panel_priority) {
    const bool set_priority = (ext_option & ORBIS_IME_EXT_OPTION_SET_PRIORITY) != 0;
    const bool shift_lock = set_priority && (ext_option & ORBIS_IME_EXT_OPTION_PRIORITY_SHIFT) != 0;
    ImeKbLayoutSelection selection;
    selection.family = IME_KB_LAYOUT_FAMILY_LATIN;
    selection.case_state = IME_KB_CASE_STATE_LOWER;
    selection.page = 0;
    if (set_priority) {
        switch (panel_priority) {
        case ORBIS_IME_PANEL_PRIORITY_SYMBOL:
            selection.family = IME_KB_LAYOUT_FAMILY_SYMBOLS;
            break;
        case ORBIS_IME_PANEL_PRIORITY_ACCENT:
            selection.family = IME_KB_LAYOUT_FAMILY_SPECIALS;
            break;
        case ORBIS_IME_PANEL_PRIORITY_ALPHABET:
        case ORBIS_IME_PANEL_PRIORITY_DEFAULT:
        default:
            selection.family = IME_KB_LAYOUT_FAMILY_LATIN;
            break;
        }
    }

    if (shift_lock && selection.family != IME_KB_LAYOUT_FAMILY_SYMBOLS) {
        /* SDK docs: PRIORITY_SHIFT starts the initial panel in Shift-lock state. */
        selection.case_state = IME_KB_CASE_STATE_CAPS_LOCK;
    }
    return selection;
}

void InitializeDefaultOskSelectionAnchor(const ImeKbLayoutSelection* layout_selection,
                                         OrbisImeExtOption ext_option, int* pending_row,
                                         int* pending_col, int* last_row, int* last_col) {
    const int kLatinLowerDefaultRow = 2;
    const int kLatinLowerDefaultCol = 4;
    const bool is_default_latin_lower = layout_selection->family == IME_KB_LAYOUT_FAMILY_LATIN &&
                                        layout_selection->case_state == IME_KB_CASE_STATE_LOWER;
    const bool game_requested_other =
        ((ext_option & ORBIS_IME_EXT_OPTION_SET_PRIORITY) != 0) && !is_default_latin_lower;
    if (!is_default_latin_lower || game_requested_other) {
        return;
    }

    *pending_row = kLatinLowerDefaultRow;
    *pending_col = kLatinLowerDefaultCol;
    *last_row = kLatinLowerDefaultRow;
    *last_col = kLatinLowerDefaultCol;
}

void CycleKeyboardCaseState(ImeKbLayoutSelection* selection) {
    switch (selection->case_state) {
    case IME_KB_CASE_STATE_LOWER:
        selection->case_state = IME_KB_CASE_STATE_UPPER;
        break;
    case IME_KB_CASE_STATE_UPPER:
        selection->case_state = IME_KB_CASE_STATE_CAPS_LOCK;
        break;
    case IME_KB_CASE_STATE_CAPS_LOCK:
    default:
        selection->case_state = IME_KB_CASE_STATE_LOWER;
        break;
    }
}

static void SetFamilyAndResetPage(ImeKbLayoutSelection* selection, ImeKbLayoutFamily* alpha_family,
                                  ImeKbLayoutFamily family) {
    selection->family = family;
    selection->page = 0;
    if (family == IME_KB_LAYOUT_FAMILY_LATIN || family == IME_KB_LAYOUT_FAMILY_SPECIALS) {
        *alpha_family = family;
    }
}

void ToggleKeyboardFamilyMode(ImeKbLayoutSelection* selection, ImeKbLayoutFamily* alpha_family,
                              ImeKbLayoutFamily target_family) {
    if (target_family == IME_KB_LAYOUT_FAMILY_SYMBOLS) {
        if (selection->family == IME_KB_LAYOUT_FAMILY_SYMBOLS) {
            SetFamilyAndResetPage(selection, alpha_family, *alpha_family);
        } else {
            if (selection->family == IME_KB_LAYOUT_FAMILY_LATIN ||
                selection->family == IME_KB_LAYOUT_FAMILY_SPECIALS) {
                *alpha_family = selection->family;
            }
            SetFamilyAndResetPage(selection, alpha_family, IME_KB_LAYOUT_FAMILY_SYMBOLS);
        }
        return;
    }

    if (selection->family == target_family) {
        SetFamilyAndResetPage(selection, alpha_family, IME_KB_LAYOUT_FAMILY_LATIN);
    } else {
        SetFamilyAndResetPage(selection, alpha_family, target_family);
    }
}

bool FocusKeyboardActionKeySelection(const ImeKbLayoutSelection* selection, ImeKbKeyAction action,
                                     int* out_row, int* out_col) {
    const ImeKbLayoutModel* layout = GetImeKeyboardLayout(selection);
    size_t i;
    if (!layout->keys || layout->key_count == 0) {
        return false;
    }

    for (i = 0; i < layout->key_count; ++i) {
        const ImeKbKeySpec* key = &layout->keys[i];
        if (key->action != action) {
            continue;
        }
        *out_row = (int)key->row;
        *out_col = (int)key->col;
        return true;
    }
    return false;
}

void FlipKeyboardModePage(ImeKbLayoutSelection* selection, int direction) {
    if (selection->family != IME_KB_LAYOUT_FAMILY_SYMBOLS &&
        selection->family != IME_KB_LAYOUT_FAMILY_SPECIALS) {
        return;
    }
    selection->page = (u8)(((int)selection->page + direction + 2) % 2);
}

int Utf16CountFromUtf8Range(const char* text, const char* end) {
    const char* range_end;
    const char* p = text;
    int utf16_units = 0;
    if (!text) {
        return 0;
    }
    range_end = end ? end : (text + strlen(text));
    while (p < range_end && *p) {
        u32 c = 0;
        const int step = Utf8Step(p, &c);
        if (step <= 0) {
            break;
        }
        utf16_units += (c > 0xFFFF) ? 2 : 1;
        p += step;
    }
    return utf16_units;
}

int Utf8ByteIndexFromUtf16Index(const char* text, int utf16_index) {
    const char* p = text;
    int count = 0;
    if (!text || utf16_index <= 0) {
        return 0;
    }
    while (*p && count < utf16_index) {
        u32 c = 0;
        const int step = Utf8Step(p, &c);
        int utf16_units;
        if (step <= 0) {
            break;
        }
        utf16_units = (c > 0xFFFF) ? 2 : 1;
        if (count + utf16_units > utf16_index) {
            break;
        }
        count += utf16_units;
        p += step;
    }
    return (int)(p - text);
}

void FollowCaretInTextField(ovl_vec2_t field_size, const char* text, int caret_byte, bool multiline,
                            ovl_vec2_t* scroll) {
    const ovl_vec2_t frame_padding = ovl_frame_padding();
    const float line_h = ovl_text_line_height();
    const int text_len = (int)strlen(text);
    const char* const caret_ptr = text + clamp_int(caret_byte, 0, text_len);
    const char* line_begin = text;
    int caret_line = 0;
    ovl_vec2_t text_size;
    float visible_h;
    float visible_w;
    float caret_x;
    if (multiline) {
        const char* p;
        for (p = text; p < caret_ptr; ++p) {
            if (*p == '\n') {
                line_begin = p + 1;
                ++caret_line;
            }
        }
    }
    text_size = ovl_calc_text_size(text, NULL, 0.0f);
    visible_h = UI_MAX(0.0f, field_size.y - frame_padding.y * 2.0f);
    visible_w = UI_MAX(0.0f, field_size.x - frame_padding.x * 2.0f);
    if (multiline) {
        const float caret_top = (float)caret_line * line_h;
        if (text_size.y > visible_h) {
            visible_w = UI_MAX(0.0f, visible_w - ovl_scrollbar_size());
        }
        if (caret_top < scroll->y) {
            scroll->y = caret_top;
        } else if (caret_top + line_h > scroll->y + visible_h) {
            scroll->y = caret_top + line_h - visible_h;
        }
        scroll->y =
            (float)floor(clamp_float(scroll->y, 0.0f, UI_MAX(0.0f, text_size.y - visible_h)));
    } else {
        scroll->y = 0.0f;
    }
    caret_x = ovl_calc_text_size(line_begin, caret_ptr, 0.0f).x;
    if (caret_x < scroll->x) {
        scroll->x = caret_x;
    } else if (caret_x > scroll->x + visible_w) {
        scroll->x = caret_x - visible_w;
    }
    scroll->x = (float)floor(clamp_float(scroll->x, 0.0f, UI_MAX(0.0f, text_size.x - visible_w)));
}

void DrawInactiveCaretOverlay(ovl_vec2_t frame_min, ovl_vec2_t frame_max, const char* text,
                              int caret_byte, bool multiline, ovl_vec2_t scroll) {
    const char* caret_ptr;
    ovl_vec2_t frame_padding;
    float left, right, top, bottom;
    float caret_x, caret_y, caret_bottom;
    if (!text) {
        return;
    }
    if (!IsCaretBlinkVisible()) {
        return;
    }

    caret_ptr = text + clamp_int(caret_byte, 0, (int)strlen(text));

    frame_padding = ovl_frame_padding();
    left = frame_min.x + frame_padding.x;
    right = frame_max.x - frame_padding.x;
    if (right <= left) {
        return;
    }

    top = frame_min.y + frame_padding.y;
    bottom = frame_max.y - frame_padding.y;
    if (bottom <= top) {
        return;
    }

    caret_y = top;
    caret_bottom = bottom;
    if (multiline) {
        const char* line_begin = text;
        int line_index = 0;
        const char* p = text;
        while (p < caret_ptr) {
            if (*p == '\n') {
                line_begin = p + 1;
                ++line_index;
            }
            ++p;
        }
        caret_x = left + ovl_calc_text_size(line_begin, caret_ptr, 0.0f).x - scroll.x;
        caret_y = top + (float)line_index * ovl_text_line_height() - scroll.y;
        caret_bottom = caret_y + ovl_text_line_height();
    } else {
        caret_x = left + ovl_calc_text_size(text, caret_ptr, 0.0f).x - scroll.x;
    }

    caret_x = clamp_float(caret_x, left, right);
    caret_y = clamp_float(caret_y, top, bottom);
    caret_bottom = clamp_float(caret_bottom, caret_y, bottom);
    if (caret_bottom <= caret_y) {
        return;
    }

    ovl_add_line(ovl_window_draw_list(), vec2(caret_x, caret_y), vec2(caret_x, caret_bottom),
                 ovl_color_u32(OVL_COLOR_TEXT, 1.0f), 1.5f);
}

static float ApplyAxisDeadzone(float v) {
    const float av = v < 0.0f ? -v : v;
    float scaled;
    if (av <= kAxisDeadzone) {
        return 0.0f;
    }
    scaled = clamp_float((av - kAxisDeadzone) / (1.0f - kAxisDeadzone), 0.0f, 1.0f);
    return v < 0.0f ? -scaled : scaled;
}

VirtualPadSnapshot ReadVirtualPadSnapshot(OrbisUserServiceUserId user_id, float delta_time) {
    VirtualPadSnapshot snapshot;
    float lx = 0.0f;
    float ly = 0.0f;
    float rx = 0.0f;
    float ry = 0.0f;
    float l2_analog = 0.0f;
    u32 buttons = 0;
    input_state_t state;
    memset(&state, 0, sizeof(state));
    if (ReadControllerState(user_id, &state)) {
        buttons = state.buttons;
        lx = ApplyAxisDeadzone(ToAxisUnit(state.axes[INPUT_AXIS_LEFT_X]));
        ly = ApplyAxisDeadzone(ToAxisUnit(state.axes[INPUT_AXIS_LEFT_Y]));
        rx = ApplyAxisDeadzone(ToAxisUnit(state.axes[INPUT_AXIS_RIGHT_X]));
        ry = ApplyAxisDeadzone(ToAxisUnit(state.axes[INPUT_AXIS_RIGHT_Y]));
        l2_analog = clamp_float((float)state.axes[INPUT_AXIS_TRIGGER_LEFT] / 255.0f, 0.0f, 1.0f);
    }

    snapshot.buttons = buttons;
    snapshot.left_stick = vec2(lx, ly);
    snapshot.panel_delta = vec2(0.0f, 0.0f);
    snapshot.l2_analog = l2_analog;
    if (delta_time > 0.0f) {
        snapshot.panel_delta =
            vec2(rx * kPanelMoveSpeed * delta_time, ry * kPanelMoveSpeed * delta_time);
    }
    return snapshot;
}

static int ResolveStickNavDirection(float lx, float ly, float* strength) {
    const float abs_x = lx < 0.0f ? -lx : lx;
    const float abs_y = ly < 0.0f ? -ly : ly;
    if (abs_x < kNavAxisThreshold && abs_y < kNavAxisThreshold) {
        *strength = 0.0f;
        return STICK_NAV_NONE;
    }
    if (abs_x >= abs_y) {
        *strength = abs_x;
        return (lx < 0.0f) ? STICK_NAV_LEFT : STICK_NAV_RIGHT;
    }
    *strength = abs_y;
    return (ly < 0.0f) ? STICK_NAV_UP : STICK_NAV_DOWN;
}

static VirtualButtonRepeatResult ProcessRepeatButton(bool down, bool edge_pressed, double now,
                                                     double repeat_delay, double repeat_rate,
                                                     bool* prev_down_state,
                                                     double* next_repeat_time) {
    VirtualButtonRepeatResult result;
    result.pressed = false;
    result.repeat = false;
    if (!down) {
        *prev_down_state = false;
        *next_repeat_time = 0.0;
        return result;
    }

    if (!*prev_down_state) {
        *prev_down_state = true;
        *next_repeat_time = now + repeat_delay;
        result.pressed = edge_pressed;
        return result;
    }

    if (edge_pressed) {
        *next_repeat_time = now + repeat_delay;
        result.pressed = true;
        return result;
    }

    if (repeat_rate <= 0.0) {
        return result;
    }

    if (now >= *next_repeat_time) {
        *next_repeat_time = now + repeat_rate;
        result.pressed = true;
        result.repeat = true;
    }
    return result;
}

static double lerp_double(double a, double b, double t) {
    return a + t * (b - a);
}

static StickNavPulseResult ProcessStickNavPulse(bool enabled, int direction, float strength,
                                                bool direction_edge_pressed, int* repeat_dir,
                                                double* next_repeat_time, double now) {
    StickNavPulseResult result;
    float t;
    double initial_delay;
    double repeat_interval;
    int prev_dir;
    bool pulse = false;
    bool pulse_repeat = false;
    memset(&result, 0, sizeof(result));
    if (!enabled || direction == STICK_NAV_NONE) {
        *repeat_dir = 0;
        *next_repeat_time = 0.0;
        return result;
    }

    t = clamp_float((strength - kNavAxisThreshold) / (1.0f - kNavAxisThreshold), 0.0f, 1.0f);
    initial_delay = lerp_double(kStickNavInitialDelaySlow, kStickNavInitialDelayFast, (double)t);
    repeat_interval = lerp_double(kStickNavRepeatSlow, kStickNavRepeatFast, (double)t);

    prev_dir = *repeat_dir;
    if (prev_dir != direction) {
        /* Treat direction changes before the pending repeat tick as fast-rotate input. */
        const bool fast_rotate_change = (prev_dir != 0) && (now < *next_repeat_time);
        *repeat_dir = direction;
        *next_repeat_time = now + initial_delay;
        pulse = (prev_dir == 0) || direction_edge_pressed || fast_rotate_change;
    } else if (direction_edge_pressed) {
        pulse = true;
        *next_repeat_time = now + initial_delay;
    } else if (now >= *next_repeat_time) {
        pulse = true;
        pulse_repeat = true;
        *next_repeat_time = now + repeat_interval;
    }

    if (!pulse) {
        return result;
    }

    switch (direction) {
    case STICK_NAV_LEFT:
        result.left = true;
        result.left_repeat = pulse_repeat;
        break;
    case STICK_NAV_RIGHT:
        result.right = true;
        result.right_repeat = pulse_repeat;
        break;
    case STICK_NAV_UP:
        result.up = true;
        result.up_repeat = pulse_repeat;
        break;
    case STICK_NAV_DOWN:
        result.down = true;
        result.down_repeat = pulse_repeat;
        break;
    default:
        break;
    }
    return result;
}

OskVirtualPadInputView MakeOskVirtualPadInputView(const OskPadInputFrame* frame) {
    OskVirtualPadInputView view;
    view.frame = frame;
    view.repeat_delay = (double)kKeyRepeatDelay;
    view.repeat_rate = (double)kKeyRepeatRate;
    return view;
}

bool OskVirtualPadDown(const OskVirtualPadInputView* view, u32 button) {
    return (view->frame->virtual_buttons & button) != 0;
}

bool OskVirtualPadPressed(const OskVirtualPadInputView* view, u32 button) {
    return (view->frame->virtual_buttons & button) != 0 &&
           (view->frame->prev_virtual_buttons & button) == 0;
}

bool OskVirtualPadRepeatPressed(const OskVirtualPadInputView* view, u32 button,
                                bool* prev_down_state, double* next_repeat_time, bool* out_repeat) {
    const VirtualButtonRepeatResult result = ProcessRepeatButton(
        OskVirtualPadDown(view, button), OskVirtualPadPressed(view, button), ovl_time(),
        view->repeat_delay, view->repeat_rate, prev_down_state, next_repeat_time);
    if (out_repeat) {
        *out_repeat = result.repeat;
    }
    return result.pressed;
}

static void ResetOskShortcutRepeatState(const OskShortcutRepeatState* state) {
    *state->prev_square_down = false;
    *state->prev_l1_down = false;
    *state->prev_r1_down = false;
    *state->l2_shortcut_armed = true;
    *state->square_next_repeat_time = 0.0;
    *state->l1_next_repeat_time = 0.0;
    *state->r1_next_repeat_time = 0.0;
    *state->triangle_next_repeat_time = 0.0;
}

static bool ConsumeTriggerShortcutPress(bool trigger_down, bool trigger_edge_pressed,
                                        float trigger_analog_value, bool* shortcut_armed) {
    /* Require clear release before accepting another trigger shortcut press. */
    const float kTriggerReleaseThreshold = 0.20f;
    if (!trigger_down && trigger_analog_value <= kTriggerReleaseThreshold) {
        *shortcut_armed = true;
    }
    if (!*shortcut_armed || !trigger_edge_pressed) {
        return false;
    }
    *shortcut_armed = false;
    return true;
}

OskShortcutActionResult EvaluateOskShortcutAction(bool allow_osk_shortcuts, bool menu_modal,
                                                  bool evaluate_action,
                                                  const OskPadInputFrame* panel_input,
                                                  const OskVirtualPadInputView* virtual_pad_input,
                                                  u32 prev_virtual_buttons,
                                                  ImeKbLayoutFamily layout_family,
                                                  const OskShortcutRepeatState* repeat_state) {
    OskShortcutActionResult result;
    bool l1_down, l1_edge_pressed, l1_repeat_pressed;
    bool square_down, square_edge_pressed, square_repeat_pressed;
    bool r1_repeat_pressed;
    bool clear_all_shortcut_pressed;
    bool l2_down, l2_edge_pressed, l2_pressed;
    float l2_analog;
    bool tri_down, tri_edge_pressed, tri_pressed, tri_space_pressed;
    bool prev_virtual_triangle_down;
    bool symbols_shortcut_pressed;
    result.action = IME_KB_KEY_ACTION_NONE;
    result.clear_all = false;
    if (!(allow_osk_shortcuts && !menu_modal)) {
        ResetOskShortcutRepeatState(repeat_state);
        return result;
    }
    if (!evaluate_action) {
        return result;
    }

    l1_down = OskVirtualPadDown(virtual_pad_input, ORBIS_PAD_BUTTON_L1);
    l1_edge_pressed = OskVirtualPadPressed(virtual_pad_input, ORBIS_PAD_BUTTON_L1);
    l1_repeat_pressed = OskVirtualPadRepeatPressed(virtual_pad_input, ORBIS_PAD_BUTTON_L1,
                                                   repeat_state->prev_l1_down,
                                                   repeat_state->l1_next_repeat_time, NULL);

    square_down = OskVirtualPadDown(virtual_pad_input, ORBIS_PAD_BUTTON_SQUARE);
    square_edge_pressed = OskVirtualPadPressed(virtual_pad_input, ORBIS_PAD_BUTTON_SQUARE);
    square_repeat_pressed = OskVirtualPadRepeatPressed(virtual_pad_input, ORBIS_PAD_BUTTON_SQUARE,
                                                       repeat_state->prev_square_down,
                                                       repeat_state->square_next_repeat_time, NULL);

    r1_repeat_pressed = OskVirtualPadRepeatPressed(virtual_pad_input, ORBIS_PAD_BUTTON_R1,
                                                   repeat_state->prev_r1_down,
                                                   repeat_state->r1_next_repeat_time, NULL);

    clear_all_shortcut_pressed =
        (l1_down && square_edge_pressed) || (square_down && l1_edge_pressed);

    l2_down = OskVirtualPadDown(virtual_pad_input, ORBIS_PAD_BUTTON_L2);
    l2_edge_pressed = OskVirtualPadPressed(virtual_pad_input, ORBIS_PAD_BUTTON_L2);
    l2_analog = UI_MAX(panel_input->l2_analog, l2_down ? 1.0f : 0.0f);
    l2_pressed = ConsumeTriggerShortcutPress(l2_down, l2_edge_pressed, l2_analog,
                                             repeat_state->l2_shortcut_armed);

    tri_down = OskVirtualPadDown(virtual_pad_input, ORBIS_PAD_BUTTON_TRIANGLE);
    tri_edge_pressed = OskVirtualPadPressed(virtual_pad_input, ORBIS_PAD_BUTTON_TRIANGLE);
    prev_virtual_triangle_down = (prev_virtual_buttons & ORBIS_PAD_BUTTON_TRIANGLE) != 0;
    tri_pressed = OskVirtualPadRepeatPressed(virtual_pad_input, ORBIS_PAD_BUTTON_TRIANGLE,
                                             &prev_virtual_triangle_down,
                                             repeat_state->triangle_next_repeat_time, NULL);
    tri_space_pressed = tri_pressed && !l2_down;

    symbols_shortcut_pressed = (l2_down && tri_edge_pressed) || (tri_down && l2_pressed);
    if (symbols_shortcut_pressed) {
        result.action = IME_KB_KEY_ACTION_SYMBOLS_MODE;
    } else if (tri_space_pressed) {
        result.action = IME_KB_KEY_ACTION_SPACE;
    } else if (layout_family != IME_KB_LAYOUT_FAMILY_SYMBOLS &&
               OskVirtualPadPressed(virtual_pad_input, ORBIS_PAD_BUTTON_L3)) {
        result.action = IME_KB_KEY_ACTION_SPECIALS_MODE;
    } else if (OskVirtualPadPressed(virtual_pad_input, ORBIS_PAD_BUTTON_R2)) {
        result.action = IME_KB_KEY_ACTION_DONE;
    } else if (clear_all_shortcut_pressed) {
        result.clear_all = true;
    } else if (square_repeat_pressed) {
        result.action = IME_KB_KEY_ACTION_BACKSPACE;
    } else if (l1_repeat_pressed) {
        result.action = IME_KB_KEY_ACTION_ARROW_LEFT;
    } else if (r1_repeat_pressed) {
        result.action = IME_KB_KEY_ACTION_ARROW_RIGHT;
    } else if (OskVirtualPadPressed(virtual_pad_input, ORBIS_PAD_BUTTON_R3)) {
        result.action = IME_KB_KEY_ACTION_SETTINGS;
    } else if (layout_family != IME_KB_LAYOUT_FAMILY_SYMBOLS && l2_pressed) {
        result.action = IME_KB_KEY_ACTION_SHIFT;
    }
    return result;
}

void ApplyOskPanelNavToKeyboardParams(ImeKbDrawParams* kb_params, bool allow_osk_shortcuts,
                                      const OskPadInputFrame* in) {
    kb_params->external_nav_left =
        allow_osk_shortcuts && (in->virtual_nav_left || in->stick_nav_left);
    kb_params->external_nav_right =
        allow_osk_shortcuts && (in->virtual_nav_right || in->stick_nav_right);
    kb_params->external_nav_up = allow_osk_shortcuts && (in->virtual_nav_up || in->stick_nav_up);
    kb_params->external_nav_down =
        allow_osk_shortcuts && (in->virtual_nav_down || in->stick_nav_down);
    kb_params->external_nav_left_repeat =
        allow_osk_shortcuts && ((in->virtual_nav_left && in->virtual_nav_left_repeat) ||
                                (in->stick_nav_left && in->stick_nav_left_repeat));
    kb_params->external_nav_right_repeat =
        allow_osk_shortcuts && ((in->virtual_nav_right && in->virtual_nav_right_repeat) ||
                                (in->stick_nav_right && in->stick_nav_right_repeat));
    kb_params->external_nav_up_repeat =
        allow_osk_shortcuts && ((in->virtual_nav_up && in->virtual_nav_up_repeat) ||
                                (in->stick_nav_up && in->stick_nav_up_repeat));
    kb_params->external_nav_down_repeat =
        allow_osk_shortcuts && ((in->virtual_nav_down && in->virtual_nav_down_repeat) ||
                                (in->stick_nav_down && in->stick_nav_down_repeat));
}

#define BUTTON_DOWN(buttons, mask) (((buttons) & (mask)) != 0)
#define BUTTON_PRESSED(frame, mask)                                                                \
    (BUTTON_DOWN((frame).virtual_buttons, mask) && !BUTTON_DOWN((frame).prev_virtual_buttons, mask))

static VirtualButtonRepeatResult ProcessVirtualPadButtonRepeat(const OskPadInputFrame* frame,
                                                               u32 button_mask, double now,
                                                               bool* prev_down_state,
                                                               double* next_repeat_time) {
    return ProcessRepeatButton(BUTTON_DOWN(frame->virtual_buttons, button_mask),
                               BUTTON_PRESSED(*frame, button_mask), now, (double)kKeyRepeatDelay,
                               (double)kKeyRepeatRate, prev_down_state, next_repeat_time);
}

OskPadInputFrame ComputeOskPadInputFrame(const VirtualPadSnapshot* virtual_pad,
                                         bool allow_osk_shortcuts, bool first_render,
                                         const OskPadInputState* state) {
    OskPadInputFrame frame;
    const double now = ovl_time();
    bool prev_virtual_cross_hold_down;
    VirtualButtonRepeatResult virtual_cross_repeat;
    VirtualButtonRepeatResult nav_left, nav_right, nav_up, nav_down;
    bool prev_cross_down;
    bool prev_lstick_left, prev_lstick_right, prev_lstick_up, prev_lstick_down;
    bool lstick_left_edge, lstick_right_edge, lstick_up_edge, lstick_down_edge;
    float stick_strength = 0.0f;
    int stick_dir;
    bool stick_dir_edge_pressed;
    StickNavPulseResult stick;

    memset(&frame, 0, sizeof(frame));
    frame.virtual_buttons = virtual_pad->buttons;
    frame.prev_virtual_buttons =
        first_render ? frame.virtual_buttons : *state->prev_virtual_buttons;
    frame.l2_analog = virtual_pad->l2_analog;

    frame.cross_down = BUTTON_DOWN(frame.virtual_buttons, ORBIS_PAD_BUTTON_CROSS);

    prev_virtual_cross_hold_down = BUTTON_DOWN(frame.prev_virtual_buttons, ORBIS_PAD_BUTTON_CROSS);
    virtual_cross_repeat =
        ProcessRepeatButton(frame.cross_down, BUTTON_PRESSED(frame, ORBIS_PAD_BUTTON_CROSS), now,
                            (double)kKeyRepeatDelay, (double)kKeyRepeatRate,
                            &prev_virtual_cross_hold_down, state->virtual_cross_next_repeat_time);

    prev_cross_down = first_render ? frame.cross_down : *state->prev_virtual_cross_down;
    frame.panel_activate_pressed_raw = frame.cross_down && !prev_cross_down;
    frame.panel_activate_repeat_raw = virtual_cross_repeat.repeat;

    nav_left = ProcessVirtualPadButtonRepeat(&frame, ORBIS_PAD_BUTTON_LEFT, now,
                                             state->prev_virtual_dpad_left_down,
                                             state->virtual_dpad_left_next_repeat_time);
    nav_right = ProcessVirtualPadButtonRepeat(&frame, ORBIS_PAD_BUTTON_RIGHT, now,
                                              state->prev_virtual_dpad_right_down,
                                              state->virtual_dpad_right_next_repeat_time);
    nav_up = ProcessVirtualPadButtonRepeat(&frame, ORBIS_PAD_BUTTON_UP, now,
                                           state->prev_virtual_dpad_up_down,
                                           state->virtual_dpad_up_next_repeat_time);
    nav_down = ProcessVirtualPadButtonRepeat(&frame, ORBIS_PAD_BUTTON_DOWN, now,
                                             state->prev_virtual_dpad_down_down,
                                             state->virtual_dpad_down_next_repeat_time);

    frame.virtual_lstick_dirs.left = virtual_pad->left_stick.x <= -kNavAxisThreshold;
    frame.virtual_lstick_dirs.right = virtual_pad->left_stick.x >= kNavAxisThreshold;
    frame.virtual_lstick_dirs.up = virtual_pad->left_stick.y <= -kNavAxisThreshold;
    frame.virtual_lstick_dirs.down = virtual_pad->left_stick.y >= kNavAxisThreshold;
    prev_lstick_left =
        first_render ? frame.virtual_lstick_dirs.left : *state->prev_virtual_lstick_left_down;
    prev_lstick_right =
        first_render ? frame.virtual_lstick_dirs.right : *state->prev_virtual_lstick_right_down;
    prev_lstick_up =
        first_render ? frame.virtual_lstick_dirs.up : *state->prev_virtual_lstick_up_down;
    prev_lstick_down =
        first_render ? frame.virtual_lstick_dirs.down : *state->prev_virtual_lstick_down_down;

    lstick_left_edge = frame.virtual_lstick_dirs.left && !prev_lstick_left;
    lstick_right_edge = frame.virtual_lstick_dirs.right && !prev_lstick_right;
    lstick_up_edge = frame.virtual_lstick_dirs.up && !prev_lstick_up;
    lstick_down_edge = frame.virtual_lstick_dirs.down && !prev_lstick_down;

    stick_dir = ResolveStickNavDirection(virtual_pad->left_stick.x, virtual_pad->left_stick.y,
                                         &stick_strength);
    switch (stick_dir) {
    case STICK_NAV_LEFT:
        stick_dir_edge_pressed = lstick_left_edge;
        break;
    case STICK_NAV_RIGHT:
        stick_dir_edge_pressed = lstick_right_edge;
        break;
    case STICK_NAV_UP:
        stick_dir_edge_pressed = lstick_up_edge;
        break;
    case STICK_NAV_DOWN:
        stick_dir_edge_pressed = lstick_down_edge;
        break;
    default:
        stick_dir_edge_pressed = false;
        break;
    }

    stick =
        ProcessStickNavPulse(allow_osk_shortcuts, stick_dir, stick_strength, stick_dir_edge_pressed,
                             state->left_stick_repeat_dir, state->left_stick_next_repeat_time, now);

    frame.virtual_nav_left = nav_left.pressed;
    frame.virtual_nav_right = nav_right.pressed;
    frame.virtual_nav_up = nav_up.pressed;
    frame.virtual_nav_down = nav_down.pressed;
    frame.virtual_nav_left_repeat = nav_left.repeat;
    frame.virtual_nav_right_repeat = nav_right.repeat;
    frame.virtual_nav_up_repeat = nav_up.repeat;
    frame.virtual_nav_down_repeat = nav_down.repeat;
    frame.stick_nav_left = stick.left;
    frame.stick_nav_right = stick.right;
    frame.stick_nav_up = stick.up;
    frame.stick_nav_down = stick.down;
    frame.stick_nav_left_repeat = stick.left_repeat;
    frame.stick_nav_right_repeat = stick.right_repeat;
    frame.stick_nav_up_repeat = stick.up_repeat;
    frame.stick_nav_down_repeat = stick.down_repeat;

    frame.nav_left = allow_osk_shortcuts && (frame.virtual_nav_left || frame.stick_nav_left);
    frame.nav_right = allow_osk_shortcuts && (frame.virtual_nav_right || frame.stick_nav_right);
    frame.nav_up = allow_osk_shortcuts && (frame.virtual_nav_up || frame.stick_nav_up);
    frame.nav_down = allow_osk_shortcuts && (frame.virtual_nav_down || frame.stick_nav_down);
    frame.nav_left_repeat =
        allow_osk_shortcuts && (frame.virtual_nav_left_repeat || frame.stick_nav_left_repeat);
    frame.nav_right_repeat =
        allow_osk_shortcuts && (frame.virtual_nav_right_repeat || frame.stick_nav_right_repeat);
    frame.nav_up_repeat =
        allow_osk_shortcuts && (frame.virtual_nav_up_repeat || frame.stick_nav_up_repeat);
    frame.nav_down_repeat =
        allow_osk_shortcuts && (frame.virtual_nav_down_repeat || frame.stick_nav_down_repeat);

    frame.virtual_control_input =
        allow_osk_shortcuts &&
        (nav_left.pressed || nav_right.pressed || nav_up.pressed || nav_down.pressed ||
         stick.left || stick.right || stick.up || stick.down ||
         BUTTON_PRESSED(frame, ORBIS_PAD_BUTTON_CROSS) ||
         BUTTON_PRESSED(frame, ORBIS_PAD_BUTTON_TRIANGLE) ||
         BUTTON_PRESSED(frame, ORBIS_PAD_BUTTON_SQUARE) ||
         BUTTON_PRESSED(frame, ORBIS_PAD_BUTTON_CIRCLE) ||
         BUTTON_PRESSED(frame, ORBIS_PAD_BUTTON_L1) || BUTTON_PRESSED(frame, ORBIS_PAD_BUTTON_R1) ||
         BUTTON_PRESSED(frame, ORBIS_PAD_BUTTON_L2) || BUTTON_PRESSED(frame, ORBIS_PAD_BUTTON_R2) ||
         BUTTON_PRESSED(frame, ORBIS_PAD_BUTTON_L3) || BUTTON_PRESSED(frame, ORBIS_PAD_BUTTON_R3));

    frame.osk_control_input =
        frame.virtual_control_input || (allow_osk_shortcuts && (frame.panel_activate_pressed_raw ||
                                                                frame.panel_activate_repeat_raw));
    return frame;
}

void CommitOskPadInputFrame(const OskPadInputFrame* frame, const OskPadInputState* state) {
    *state->prev_virtual_buttons = frame->virtual_buttons;
    *state->prev_virtual_cross_down = frame->cross_down;
    *state->prev_virtual_lstick_left_down = frame->virtual_lstick_dirs.left;
    *state->prev_virtual_lstick_right_down = frame->virtual_lstick_dirs.right;
    *state->prev_virtual_lstick_up_down = frame->virtual_lstick_dirs.up;
    *state->prev_virtual_lstick_down_down = frame->virtual_lstick_dirs.down;
}

void DisarmMenuActivate(bool* menu_activate_armed) {
    *menu_activate_armed = false;
}

void RearmMenuActivateOnRelease(bool activate_down, bool* menu_activate_armed) {
    if (!*menu_activate_armed && !activate_down) {
        *menu_activate_armed = true;
    }
}

bool ConsumeMenuActivatePress(bool panel_activate_pressed, bool opened_menu_this_frame,
                              bool* menu_activate_armed) {
    if (!*menu_activate_armed || opened_menu_this_frame || !panel_activate_pressed) {
        return false;
    }
    *menu_activate_armed = false;
    return true;
}

void OpenOskMainEditMenu(OskEditMenuPopup* popup, int* edit_menu_index, bool* menu_activate_armed) {
    *popup = OSK_EDIT_MENU_MAIN;
    *edit_menu_index = 0;
    DisarmMenuActivate(menu_activate_armed);
}

void OpenOskActionsEditMenu(OskEditMenuPopup* popup, int* edit_menu_index,
                            bool* menu_activate_armed) {
    *popup = OSK_EDIT_MENU_ACTIONS;
    *edit_menu_index = 0;
    DisarmMenuActivate(menu_activate_armed);
}

bool CloseOskEditMenuOnCancel(OskEditMenuPopup* popup, bool* cancel_pressed,
                              bool* menu_activate_armed) {
    if (*popup == OSK_EDIT_MENU_NONE || !*cancel_pressed) {
        return false;
    }
    *cancel_pressed = false;
    *popup = OSK_EDIT_MENU_NONE;
    *menu_activate_armed = true;
    return true;
}

static const char* const kMainMenuItems[3] = {"Select", "Select All", "Paste"};
static const char* const kActionMenuItems[2] = {"Copy", "Paste"};

/* Paste, the last item of either menu, needs something on the clipboard */
static bool EditMenuItemEnabled(bool is_main_menu, int index, bool clipboard_ready) {
    if ((is_main_menu && index == 2) || (!is_main_menu && index == 1)) {
        return clipboard_ready;
    }
    return true;
}

bool DrawAndHandleOskEditMenuPopup(OskEditMenuPopup* popup, int* edit_menu_index,
                                   const ImePanelMetrics* metrics, ovl_draw_list_t* draw,
                                   bool pointer_navigation_active, bool nav_up, bool nav_down,
                                   bool cross_down, bool panel_activate_pressed,
                                   bool opened_menu_this_frame, bool* menu_activate_armed,
                                   bool clipboard_ready, OskEditMenuApply apply_action, void* ctx) {
    bool is_main_menu;
    int item_count;
    float menu_w, menu_inner_pad, item_gap, item_h, menu_h;
    ovl_vec2_t menu_pos, menu_max;
    bool menu_activate;
    int i;
    if (*popup == OSK_EDIT_MENU_NONE || draw == NULL) {
        return false;
    }

    is_main_menu = *popup == OSK_EDIT_MENU_MAIN;
    item_count = is_main_menu ? 3 : 2;

    if (!pointer_navigation_active) {
        if (nav_up) {
            *edit_menu_index = (*edit_menu_index + item_count - 1) % item_count;
        } else if (nav_down) {
            *edit_menu_index = (*edit_menu_index + 1) % item_count;
        }
    }
    *edit_menu_index = clamp_int(*edit_menu_index, 0, item_count - 1);

    menu_w = UI_MIN(metrics->kb_size.x * 0.42f, 280.0f);
    menu_inner_pad = UI_MAX(6.0f, metrics->key_gap * 0.7f);
    item_gap = UI_MAX(3.0f, metrics->key_gap * 0.35f);
    item_h = UI_MAX(28.0f, metrics->key_h * 0.70f);
    menu_h =
        menu_inner_pad * 2.0f + item_h * (float)item_count + item_gap * (float)(item_count - 1);
    menu_pos = vec2(metrics->kb_pos.x + (metrics->kb_size.x - menu_w) * 0.5f,
                    metrics->kb_pos.y + (metrics->kb_size.y - menu_h) * 0.5f);
    menu_max = vec2(menu_pos.x + menu_w, menu_pos.y + menu_h);
    ovl_add_rect_filled(draw, menu_pos, menu_max, OVL_COL(16, 16, 16, 245), metrics->corner_radius,
                        0);
    ovl_add_rect(draw, menu_pos, menu_max, OVL_COL(100, 100, 100, 255), metrics->corner_radius, 0,
                 1.0f);

    RearmMenuActivateOnRelease(cross_down, menu_activate_armed);
    menu_activate = ConsumeMenuActivatePress(panel_activate_pressed, opened_menu_this_frame,
                                             menu_activate_armed);
    for (i = 0; i < item_count; ++i) {
        const float item_y = menu_pos.y + menu_inner_pad + i * (item_h + item_gap);
        const ovl_vec2_t item_pos = vec2(menu_pos.x + menu_inner_pad, item_y);
        const ovl_vec2_t item_size = vec2(menu_w - menu_inner_pad * 2.0f, item_h);
        const ovl_vec2_t item_max = vec2(item_pos.x + item_size.x, item_pos.y + item_size.y);
        const bool selected = i == *edit_menu_index;
        const bool enabled = EditMenuItemEnabled(is_main_menu, i, clipboard_ready);
        const ovl_col_t item_bg = !enabled   ? OVL_COL(30, 30, 30, 255)
                                  : selected ? OVL_COL(60, 96, 146, 255)
                                             : OVL_COL(45, 45, 45, 255);
        const char* label = is_main_menu ? kMainMenuItems[i] : kActionMenuItems[i];
        ovl_vec2_t text_size;
        ovl_add_rect_filled(draw, item_pos, item_max, item_bg, metrics->corner_radius * 0.6f, 0);
        ovl_add_rect(draw, item_pos, item_max, OVL_COL(90, 90, 90, 255),
                     metrics->corner_radius * 0.6f, 0, 1.0f);

        text_size = ovl_calc_text_size(label, NULL, 0.0f);
        ovl_add_text(draw, ovl_font_current(), ovl_font_current_size(),
                     vec2(item_pos.x + (item_size.x - text_size.x) * 0.5f,
                          item_pos.y + (item_size.y - text_size.y) * 0.5f),
                     enabled ? OVL_COL(232, 232, 232, 255) : OVL_COL(128, 128, 128, 255), label,
                     NULL, 0.0f);
    }

    if (menu_activate && EditMenuItemEnabled(is_main_menu, *edit_menu_index, clipboard_ready)) {
        const OskEditMenuPopup previous_popup = *popup;
        apply_action(ctx, previous_popup, *edit_menu_index);
        if (*popup != OSK_EDIT_MENU_NONE && *popup != previous_popup) {
            *edit_menu_index = 0;
        }
        if (*popup == OSK_EDIT_MENU_NONE) {
            *menu_activate_armed = true;
        }
    }
    return true;
}
