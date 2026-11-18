// SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <algorithm>
#include <cmath>
#include <cstring>
#include <cwchar>
#include <limits>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "common/assert.h"
#include "common/logging/log.h"
#include "core/debug_state.h"
#include "core/libraries/error_codes.h"
#include "core/libraries/ime/ime_dialog.h"
#include "core/libraries/ime/ime_dialog_ui.h"
#include "core/libraries/ime/ime_kb_layout.h"
#include "core/libraries/ime/ime_ui_shared.h"
#include "core/libraries/pad/pad.h"
#include "core/memory.h"
#include "core/tls.h"
#include "video_core/overlay/ovl.h"
#include "video_core/overlay/ovl_layers.h"

namespace Libraries::ImeDialog {
namespace {
constexpr ovl_col_t kSelectorBorderColor = OVL_COL(248, 248, 248, 255);
constexpr ovl_col_t kSelectorOverlayColor = OVL_COL(255, 255, 255, 18);
constexpr float kSelectorBorderThickness = 2.0f;
constexpr float kSelectorInnerMargin = 2.0f;
constexpr const char* kSelectorInputId = "##ImeDialogSelectorInput";
constexpr const char* kSelectorPredictId = "##ImeDialogSelectorPredict";
constexpr const char* kSelectorCloseId = "##ImeDialogSelectorClose";

using Libraries::Ime::BrightenColor;
using Libraries::Ime::CommitOskPadInputFrame;
using Libraries::Ime::ComputeOskPadInputFrame;
using Libraries::Ime::DrawInactiveCaretOverlay;
using Libraries::Ime::ImeClipboardText;
using Libraries::Ime::ImeLayer;
using Libraries::Ime::kPanelEdgeWrapHoldDelaySec;
using Libraries::Ime::kRepeatIntentWindowSec;
using Libraries::Ime::OskPadInputFrame;
using Libraries::Ime::OskPadInputState;
using Libraries::Ime::OskVirtualPadInputView;
using Libraries::Ime::ReadVirtualPadSnapshot;
using Libraries::Ime::SetImeClipboardText;
using Libraries::Ime::Utf16CountFromUtf8Range;
using Libraries::Ime::Utf16ToUtf8;
using Libraries::Ime::Utf8ByteIndexFromUtf16Index;
using Libraries::Ime::Utf8Step;
using Libraries::Ime::Utf8ToUtf16;
using Libraries::Ime::VirtualPadSnapshot;

bool IsMappedGuestBuffer(const void* ptr, size_t bytes) {
    if (!ptr || bytes == 0) {
        return false;
    }
    auto* memory = ::Core::Memory::Instance();
    if (!memory) {
        return false;
    }
    return memory->IsValidMapping(reinterpret_cast<VAddr>(ptr), bytes);
}

size_t BoundedUtf16Length(const char16_t* text, size_t max_len) {
    if (!text || max_len == 0) {
        return 0;
    }
    for (size_t i = 0; i < max_len; ++i) {
        if (text[i] == u'\0') {
            return i;
        }
    }
    return max_len;
}

} // namespace

ImeDialogState::ImeDialogState()
    : input_changed(false), user_id(-1), is_multi_line(false), is_numeric(false),
      fixed_position(false), type(OrbisImeType::Default),
      supported_languages(static_cast<OrbisImeLanguage>(0)),
      enter_label(OrbisImeEnterLabel::Default), text_filter(nullptr), keyboard_filter(nullptr),
      max_text_length(ORBIS_IME_DIALOG_MAX_TEXT_LENGTH), text_buffer(nullptr), original_text(),
      title(), placeholder(), current_text() {}

ImeDialogState::ImeDialogState(const OrbisImeDialogParam* param,
                               const OrbisImeParamExtended* extended) {
    LOG_INFO(Lib_ImeDialog, "param={}, text_buffer={}", static_cast<const void*>(param),
             static_cast<void*>(param ? param->input_text_buffer : nullptr));
    if (!param) {
        LOG_ERROR(Lib_ImeDialog, "param==nullptr, returning without init");
        return;
    }

    user_id = param->user_id;
    is_multi_line = True(param->option & OrbisImeOption::MULTILINE);
    fixed_position = True(param->option & OrbisImeOption::FIXED_POSITION);
    use_over2k = True(param->option & OrbisImeOption::USE_OVER_2K_COORDINATES);
    is_numeric = param->type == OrbisImeType::Number;
    type = param->type;
    supported_languages = param->supported_languages;
    enter_label = param->enter_label;
    text_filter = param->filter;
    keyboard_filter = extended ? extended->ext_keyboard_filter : nullptr;
    ext_option = extended ? extended->option : OrbisImeExtOption::DEFAULT;
    disable_device = extended ? extended->disable_device : OrbisImeDisableDevice::DEFAULT;
    panel_priority = extended ? extended->priority : OrbisImePanelPriority::Default;
    style_config = Libraries::Ime::ResolveImeStyleConfig(extended);
    max_text_length = param->max_text_length;
    text_buffer = param->input_text_buffer;
    LOG_INFO(Lib_ImeDialog,
             "ImeDialogState: option=0x{:X} (multiline={}, password={}, ext_kbd={}, fixed_pos={}, "
             "over2k={}), enter_label={}, type={}",
             static_cast<u32>(param->option), is_multi_line,
             True(param->option & OrbisImeOption::PASSWORD),
             True(param->option & OrbisImeOption::EXT_KEYBOARD),
             True(param->option & OrbisImeOption::FIXED_POSITION),
             True(param->option & OrbisImeOption::USE_OVER_2K_COORDINATES),
             static_cast<u32>(enter_label), static_cast<u32>(type));
    LOG_DEBUG(Lib_ImeDialog,
              "ImeDialogState: user_id={}, type={}, enter_label={}, multiline={}, numeric={}, "
              "max_len={}, text_filter={}, keyboard_filter={}",
              static_cast<u32>(user_id), static_cast<u32>(type), static_cast<u32>(enter_label),
              is_multi_line, is_numeric, max_text_length, (void*)text_filter,
              (void*)keyboard_filter);

    if (param->title) {
        std::size_t title_len = std::char_traits<char16_t>::length(param->title);
        title.resize(title_len * 4 + 1);
        title[title_len * 4] = '\0';

        if (!ConvertOrbisToUTF8(param->title, title_len, &title[0], title_len * 4 + 1)) {
            LOG_ERROR(Lib_ImeDialog, "Failed to convert title to utf8 encoding");
        }
    }

    if (param->placeholder) {
        std::size_t placeholder_len = std::char_traits<char16_t>::length(param->placeholder);
        placeholder.resize(placeholder_len * 4 + 1);
        placeholder[placeholder_len * 4] = '\0';

        if (!ConvertOrbisToUTF8(param->placeholder, placeholder_len, &placeholder[0],
                                placeholder_len * 4 + 1)) {
            LOG_ERROR(Lib_ImeDialog, "Failed to convert placeholder to utf8 encoding");
        }
    }

    std::size_t text_len = 0;
    if (text_buffer) {
        const size_t bytes = (static_cast<size_t>(max_text_length) + 1) * sizeof(char16_t);
        if (IsMappedGuestBuffer(text_buffer, bytes)) {
            text_len = BoundedUtf16Length(text_buffer, max_text_length);
        } else {
            LOG_ERROR(Lib_ImeDialog, "ImeDialogState: input_text_buffer not mapped");
        }
    }
    if (text_len > max_text_length) {
        text_len = max_text_length;
    }
    original_text.resize(static_cast<std::size_t>(max_text_length) + 1, u'\0');
    if (text_buffer) {
        for (std::size_t i = 0; i < text_len; ++i) {
            original_text[i] = text_buffer[i];
        }
    }
    if (text_buffer) {
        if (!ConvertOrbisToUTF8(text_buffer, text_len, current_text.begin(),
                                ORBIS_IME_DIALOG_MAX_TEXT_LENGTH * 4 + 1)) {
            LOG_ERROR(Lib_ImeDialog, "Failed to convert text to utf8 encoding");
        }
    }
    caret_index = Utf16CountFromUtf8Range(
        current_text.begin(), current_text.begin() + static_cast<int>(current_text.size()));
    caret_byte_index = static_cast<int>(current_text.size());
    caret_dirty = true;
    panel_layout_valid = (sceImeDialogGetPanelPositionAndForm(&panel_layout) == ORBIS_OK);
    if (extended) {
        (void)sceImeDialogGetPanelSizeExtended(param, extended, &panel_req_width,
                                               &panel_req_height);
    } else {
        (void)sceImeDialogGetPanelSize(param, &panel_req_width, &panel_req_height);
    }
    LOG_DEBUG(Lib_ImeDialog, "ImeDialogState: initial_text_len={}", current_text.size());
}

ImeDialogState::ImeDialogState(ImeDialogState&& other) noexcept
    : input_changed(other.input_changed), caret_index(other.caret_index),
      caret_byte_index(other.caret_byte_index), caret_dirty(other.caret_dirty),
      use_over2k(other.use_over2k), panel_layout(other.panel_layout),
      panel_layout_valid(other.panel_layout_valid), panel_req_width(other.panel_req_width),
      panel_req_height(other.panel_req_height), ext_option(other.ext_option),
      disable_device(other.disable_device), panel_priority(other.panel_priority),
      style_config(other.style_config), user_id(other.user_id), is_multi_line(other.is_multi_line),
      is_numeric(other.is_numeric), fixed_position(other.fixed_position), type(other.type),
      supported_languages(other.supported_languages), enter_label(other.enter_label),
      text_filter(other.text_filter), keyboard_filter(other.keyboard_filter),
      max_text_length(other.max_text_length), text_buffer(other.text_buffer),
      original_text(std::move(other.original_text)), title(std::move(other.title)),
      placeholder(std::move(other.placeholder)), current_text(other.current_text) {

    other.text_buffer = nullptr;
}

ImeDialogState& ImeDialogState::operator=(ImeDialogState&& other) {
    if (this != &other) {
        input_changed = other.input_changed;
        caret_index = other.caret_index;
        caret_byte_index = other.caret_byte_index;
        caret_dirty = other.caret_dirty;
        use_over2k = other.use_over2k;
        panel_layout = other.panel_layout;
        panel_layout_valid = other.panel_layout_valid;
        panel_req_width = other.panel_req_width;
        panel_req_height = other.panel_req_height;
        ext_option = other.ext_option;
        disable_device = other.disable_device;
        panel_priority = other.panel_priority;
        style_config = other.style_config;
        user_id = other.user_id;
        is_multi_line = other.is_multi_line;
        is_numeric = other.is_numeric;
        fixed_position = other.fixed_position;
        type = other.type;
        supported_languages = other.supported_languages;
        enter_label = other.enter_label;
        text_filter = other.text_filter;
        keyboard_filter = other.keyboard_filter;
        max_text_length = other.max_text_length;
        text_buffer = other.text_buffer;
        original_text = std::move(other.original_text);
        title = std::move(other.title);
        placeholder = std::move(other.placeholder);
        current_text = other.current_text;

        other.text_buffer = nullptr;
    }

    return *this;
}

bool ImeDialogState::CopyTextToOrbisBuffer(bool use_original) {
    if (!text_buffer) {
        LOG_DEBUG(Lib_ImeDialog, "CopyTextToOrbisBuffer: no text_buffer");
        return false;
    }

    if (use_original) {
        const std::size_t count =
            original_text.empty() ? 0 : static_cast<std::size_t>(max_text_length) + 1;
        if (count > 0) {
            std::copy(original_text.begin(), original_text.end(), text_buffer);
        }
        LOG_DEBUG(Lib_ImeDialog, "CopyTextToOrbisBuffer: restored original");
        return true;
    }

    const std::size_t utf8_len = current_text.size();
    const bool ok = ConvertUTF8ToOrbis(current_text.begin(), utf8_len, text_buffer,
                                       static_cast<std::size_t>(max_text_length) + 1);
    LOG_DEBUG(Lib_ImeDialog, "CopyTextToOrbisBuffer: {}", ok ? "ok" : "failed");
    return ok;
}

bool ImeDialogState::NormalizeNewlines() {
    if (current_text.size() == 0) {
        return false;
    }
    std::string src = current_text.to_string();
    std::string out;
    out.reserve(src.size());
    bool changed = false;
    for (size_t i = 0; i < src.size(); ++i) {
        const char ch = src[i];
        if (ch == '\r') {
            if (i + 1 < src.size() && src[i + 1] == '\n') {
                ++i;
            }
            out.push_back('\n');
            changed = true;
        } else {
            out.push_back(ch);
        }
    }
    if (changed) {
        current_text.FromString(out);
    }
    return changed;
}

bool ImeDialogState::ClampCurrentTextToMaxLen() {
    if (current_text.size() == 0 || max_text_length == 0) {
        return false;
    }
    const int utf16_len = Utf16CountFromUtf8Range(
        current_text.begin(), current_text.begin() + static_cast<int>(current_text.size()));
    if (utf16_len <= static_cast<int>(max_text_length)) {
        return false;
    }
    std::vector<char16_t> utf16(static_cast<size_t>(max_text_length) + 1, u'\0');
    Utf8ToUtf16(current_text.begin(), current_text.size(), utf16.data(), utf16.size());
    size_t len = BoundedUtf16Length(utf16.data(), static_cast<size_t>(max_text_length));
    std::string out;
    out.resize(len * 4 + 1, '\0');
    Utf16ToUtf8(utf16.data(), len, out.data(), out.size());
    out.resize(std::strlen(out.c_str()));
    current_text.FromString(out);
    return true;
}

bool ImeDialogState::CallTextFilter() {
    if (!text_filter || !input_changed) {
        return true;
    }

    input_changed = false;

    char16_t src_text[ORBIS_IME_DIALOG_MAX_TEXT_LENGTH + 1] = {0};
    u32 src_text_length = 0;
    char16_t out_text[ORBIS_IME_DIALOG_MAX_TEXT_LENGTH + 1] = {0};
    u32 out_text_length = ORBIS_IME_DIALOG_MAX_TEXT_LENGTH;

    if (!ConvertUTF8ToOrbis(current_text.begin(), current_text.size(), src_text,
                            ORBIS_IME_DIALOG_MAX_TEXT_LENGTH + 1)) {
        LOG_ERROR(Lib_ImeDialog, "Failed to convert text to orbis encoding");
        return false;
    }
    src_text_length = static_cast<u32>(
        BoundedUtf16Length(src_text, static_cast<size_t>(ORBIS_IME_DIALOG_MAX_TEXT_LENGTH)));

    int ret = text_filter(out_text, &out_text_length, src_text, src_text_length);

    if (ret != 0) {
        return false;
    }

    if (!ConvertOrbisToUTF8(out_text, out_text_length, current_text.begin(),
                            ORBIS_IME_DIALOG_MAX_TEXT_LENGTH * 4)) {
        LOG_ERROR(Lib_ImeDialog, "Failed to convert text to utf8 encoding");
        return false;
    }

    const bool changed = NormalizeNewlines() | ClampCurrentTextToMaxLen();
    const int new_len = Utf16CountFromUtf8Range(
        current_text.begin(), current_text.begin() + static_cast<int>(current_text.size()));
    if (caret_index > new_len) {
        caret_index = new_len;
        caret_dirty = true;
    } else if (changed) {
        caret_dirty = true;
    }

    CopyTextToOrbisBuffer(false);
    return true;
}

bool ImeDialogState::CallKeyboardFilter(const OrbisImeKeycode* src_keycode, u16* out_keycode,
                                        u32* out_status) {
    if (!keyboard_filter) {
        return true;
    }

    int ret = keyboard_filter(src_keycode, out_keycode, out_status, nullptr);
    return ret == 0;
}

bool ImeDialogState::ConvertOrbisToUTF8(const char16_t* orbis_text, std::size_t orbis_text_len,
                                        char* utf8_text, std::size_t utf8_text_len) {
    Utf16ToUtf8(orbis_text, orbis_text_len, utf8_text, utf8_text_len);

    return true;
}

bool ImeDialogState::ConvertUTF8ToOrbis(const char* utf8_text, std::size_t utf8_text_len,
                                        char16_t* orbis_text, std::size_t orbis_text_len) {
    Utf8ToUtf16(utf8_text, utf8_text_len, orbis_text, orbis_text_len);

    return true;
}

ImeDialogUi::ImeDialogUi(ImeDialogState* state, OrbisImeDialogStatus* status,
                         OrbisImeDialogResult* result)
    : state(state), status(status), result(result) {

    if (state && *status == OrbisImeDialogStatus::Running) {
        kb_layout_selection = Libraries::Ime::ResolveInitialKbLayoutSelection(
            state->ext_option, state->panel_priority);
        last_nav_layout_selection = kb_layout_selection;
        nav_layout_selection_initialized = true;
        kb_alpha_family =
            (kb_layout_selection.family == Libraries::Ime::ImeKbLayoutFamily::Specials)
                ? Libraries::Ime::ImeKbLayoutFamily::Specials
                : Libraries::Ime::ImeKbLayoutFamily::Latin;
        Libraries::Ime::InitializeDefaultOskSelectionAnchor(
            kb_layout_selection, state->ext_option, pending_keyboard_row, pending_keyboard_col,
            last_keyboard_selected_row, last_keyboard_selected_col);
        ImeLayer::Add(this);
    }
}

ImeDialogUi::~ImeDialogUi() {
    Free();
}

ImeDialogUi::ImeDialogUi(ImeDialogUi&& other) noexcept {
    *this = std::move(other);
}

ImeDialogUi& ImeDialogUi::operator=(ImeDialogUi&& other) {
    // Neither is drawn while one becomes the other
    Free();
    other.Free();

    state = other.state;
    status = other.status;
    result = other.result;
    first_render = other.first_render;
    accept_armed = other.accept_armed;
    pointer_navigation_active = other.pointer_navigation_active;
    edit_menu_popup = other.edit_menu_popup;
    menu_activate_armed = other.menu_activate_armed;
    l2_shortcut_armed = other.l2_shortcut_armed;
    text_select_mode = other.text_select_mode;
    prev_virtual_cross_down = other.prev_virtual_cross_down;
    prev_virtual_lstick_left_down = other.prev_virtual_lstick_left_down;
    prev_virtual_lstick_right_down = other.prev_virtual_lstick_right_down;
    prev_virtual_lstick_up_down = other.prev_virtual_lstick_up_down;
    prev_virtual_lstick_down_down = other.prev_virtual_lstick_down_down;
    left_stick_repeat_dir = other.left_stick_repeat_dir;
    left_stick_next_repeat_time = other.left_stick_next_repeat_time;
    virtual_cross_next_repeat_time = other.virtual_cross_next_repeat_time;
    virtual_triangle_next_repeat_time = other.virtual_triangle_next_repeat_time;
    prev_virtual_buttons = other.prev_virtual_buttons;
    prev_virtual_square_down = other.prev_virtual_square_down;
    prev_virtual_l1_down = other.prev_virtual_l1_down;
    prev_virtual_r1_down = other.prev_virtual_r1_down;
    prev_virtual_dpad_left_down = other.prev_virtual_dpad_left_down;
    prev_virtual_dpad_right_down = other.prev_virtual_dpad_right_down;
    prev_virtual_dpad_up_down = other.prev_virtual_dpad_up_down;
    prev_virtual_dpad_down_down = other.prev_virtual_dpad_down_down;
    virtual_square_next_repeat_time = other.virtual_square_next_repeat_time;
    virtual_l1_next_repeat_time = other.virtual_l1_next_repeat_time;
    virtual_r1_next_repeat_time = other.virtual_r1_next_repeat_time;
    virtual_dpad_left_next_repeat_time = other.virtual_dpad_left_next_repeat_time;
    virtual_dpad_right_next_repeat_time = other.virtual_dpad_right_next_repeat_time;
    virtual_dpad_up_next_repeat_time = other.virtual_dpad_up_next_repeat_time;
    virtual_dpad_down_next_repeat_time = other.virtual_dpad_down_next_repeat_time;
    panel_vertical_nav_state = other.panel_vertical_nav_state;
    panel_position_initialized = other.panel_position_initialized;
    panel_layout_anchor_initialized = other.panel_layout_anchor_initialized;
    panel_position = other.panel_position;
    panel_layout_anchor = other.panel_layout_anchor;
    input_scroll = other.input_scroll;
    input_cursor_utf16 = other.input_cursor_utf16;
    input_cursor_byte = other.input_cursor_byte;
    input_selection_start_byte = other.input_selection_start_byte;
    input_selection_end_byte = other.input_selection_end_byte;
    text_select_anchor_utf16 = other.text_select_anchor_utf16;
    text_select_focus_utf16 = other.text_select_focus_utf16;
    top_virtual_col = other.top_virtual_col;
    panel_selection = other.panel_selection;
    pending_keyboard_row = other.pending_keyboard_row;
    pending_keyboard_col = other.pending_keyboard_col;
    last_keyboard_selected_row = other.last_keyboard_selected_row;
    last_keyboard_selected_col = other.last_keyboard_selected_col;
    edit_menu_index = other.edit_menu_index;
    kb_layout_selection = other.kb_layout_selection;
    last_nav_layout_selection = other.last_nav_layout_selection;
    nav_layout_selection_initialized = other.nav_layout_selection_initialized;
    kb_alpha_family = other.kb_alpha_family;
    other.state = nullptr;
    other.status = nullptr;
    other.result = nullptr;
    other.menu_activate_armed = true;
    other.l2_shortcut_armed = true;
    other.prev_virtual_lstick_left_down = false;
    other.prev_virtual_lstick_right_down = false;
    other.prev_virtual_lstick_up_down = false;
    other.prev_virtual_lstick_down_down = false;
    other.left_stick_repeat_dir = 0;
    other.left_stick_next_repeat_time = 0.0;
    other.virtual_cross_next_repeat_time = 0.0;
    other.virtual_triangle_next_repeat_time = 0.0;
    other.prev_virtual_buttons = 0;
    other.prev_virtual_square_down = false;
    other.prev_virtual_l1_down = false;
    other.prev_virtual_r1_down = false;
    other.prev_virtual_dpad_left_down = false;
    other.prev_virtual_dpad_right_down = false;
    other.prev_virtual_dpad_up_down = false;
    other.prev_virtual_dpad_down_down = false;
    other.virtual_square_next_repeat_time = 0.0;
    other.virtual_l1_next_repeat_time = 0.0;
    other.virtual_r1_next_repeat_time = 0.0;
    other.virtual_dpad_left_next_repeat_time = 0.0;
    other.virtual_dpad_right_next_repeat_time = 0.0;
    other.virtual_dpad_up_next_repeat_time = 0.0;
    other.virtual_dpad_down_next_repeat_time = 0.0;
    Libraries::Ime::ResetImeEdgeWrapNav(other.panel_vertical_nav_state);
    other.nav_layout_selection_initialized = false;
    other.panel_layout_anchor_initialized = false;
    other.panel_layout_anchor = {};
    other.kb_alpha_family = Libraries::Ime::ImeKbLayoutFamily::Latin;

    if (state && *status == OrbisImeDialogStatus::Running) {
        ImeLayer::Add(this);
    }

    return *this;
}

void ImeDialogUi::Free() {
    ImeLayer::Remove(this);
}

void ImeDialogUi::FinishDialog(OrbisImeDialogEndStatus endstatus, bool restore_original,
                               const char* reason) {
    if (!status || !result || !state) {
        return;
    }
    state->CopyTextToOrbisBuffer(restore_original);
    *status = OrbisImeDialogStatus::Finished;
    result->endstatus = endstatus;
    LOG_INFO(Lib_ImeDialog, "ImeDialog {} -> status=Finished", reason ? reason : "Done");
    Free();
}

void ImeDialogUi::Draw() {
    if (!state) {
        return;
    }

    if (!status || *status != OrbisImeDialogStatus::Running) {
        Free();
        return;
    }

    const VirtualPadSnapshot virtual_pad = ReadVirtualPadSnapshot(state->user_id, ovl_delta_time());

    OrbisImePositionAndForm layout = state->panel_layout;
    const bool has_layout = state->panel_layout_valid;
    const auto viewport = Libraries::Ime::ComputeImeViewportMetrics(state->use_over2k);
    const ovl_vec2_t viewport_size = viewport.size;
    const ovl_vec2_t viewport_offset = viewport.offset;
    const float scale_x = viewport.scale_x;
    const float scale_y = viewport.scale_y;
    const float ui_scale = viewport.ui_scale;

    ovl_vec2_t window_size;
    const bool has_panel_size = (has_layout && layout.width > 0 && layout.height > 0) ||
                                (state->panel_req_width > 0 && state->panel_req_height > 0);
    if (has_layout && layout.width > 0 && layout.height > 0) {
        window_size = {layout.width * scale_x, layout.height * scale_y};
    } else if (state->panel_req_width > 0 && state->panel_req_height > 0) {
        window_size = {static_cast<float>(state->panel_req_width) * scale_x,
                       static_cast<float>(state->panel_req_height) * scale_y};
    } else {
        window_size = {std::min(std::max(0.0f, viewport_size.x - 40.0f), 640.0f),
                       std::min(std::max(0.0f, viewport_size.y - 40.0f), 420.0f)};
    }
    if (!has_panel_size) {
        window_size.x = std::max(window_size.x, 320.0f);
        window_size.y = std::max(window_size.y, 240.0f);
    }

    const float panel_w = window_size.x;
    const float panel_h = window_size.y;

    float base_x = 0.0f;
    float base_y = 0.0f;
    if (has_layout) {
        base_x = viewport_offset.x + layout.posx * scale_x;
        base_y = viewport_offset.y + layout.posy * scale_y;
        if (layout.horizontal_alignment == OrbisImeHorizontalAlignment::Center) {
            base_x -= window_size.x * 0.5f;
        } else if (layout.horizontal_alignment == OrbisImeHorizontalAlignment::Right) {
            base_x -= window_size.x;
        }
        if (layout.vertical_alignment == OrbisImeVerticalAlignment::Center) {
            base_y -= window_size.y * 0.5f;
        } else if (layout.vertical_alignment == OrbisImeVerticalAlignment::Bottom) {
            base_y -= window_size.y;
        }
    } else {
        base_x = viewport_offset.x + (viewport_size.x - window_size.x) * 0.5f;
        base_y = viewport_offset.y + (viewport_size.y - window_size.y) * 0.5f;
    }
    const float min_x = viewport_offset.x;
    const float max_x = viewport_offset.x + std::max(0.0f, viewport_size.x - window_size.x);
    const float min_y = viewport_offset.y;
    const float max_y = viewport_offset.y + std::max(0.0f, viewport_size.y - window_size.y);
    base_x = std::clamp(base_x, min_x, max_x);
    base_y = std::clamp(base_y, min_y, max_y);
    const ovl_vec2_t layout_anchor{base_x, base_y};
    if (!panel_layout_anchor_initialized) {
        panel_layout_anchor = layout_anchor;
        panel_layout_anchor_initialized = true;
    }
    const ovl_vec2_t layout_anchor_delta{layout_anchor.x - panel_layout_anchor.x,
                                         layout_anchor.y - panel_layout_anchor.y};

    if (!panel_position_initialized) {
        panel_position = layout_anchor;
        panel_position_initialized = true;
    }
    if (state->fixed_position) {
        panel_position = layout_anchor;
    } else {
        // The right stick moves the panel
        panel_position.x += layout_anchor_delta.x;
        panel_position.y += layout_anchor_delta.y;
        panel_position.x += virtual_pad.panel_delta.x;
        panel_position.y += virtual_pad.panel_delta.y;
    }
    panel_layout_anchor = layout_anchor;
    panel_position.x = std::clamp(panel_position.x, min_x, max_x);
    panel_position.y = std::clamp(panel_position.y, min_y, max_y);
    ovl_set_next_window_pos(panel_position, OVL_COND_ALWAYS, {0.0f, 0.0f});
    ovl_set_next_window_size(window_size, OVL_COND_ALWAYS);

    if (first_render || !ovl_nav_active()) {
        ovl_set_next_window_focus();
    }

    const auto lock_window_scroll = []() {
        ovl_set_scroll_x(0.0f);
        ovl_set_scroll_y(0.0f);
    };

    const unsigned window_flags = OVL_WINDOW_NO_DECORATION | OVL_WINDOW_NO_SAVED_SETTINGS |
                                  OVL_WINDOW_NO_SCROLLBAR | OVL_WINDOW_NO_SCROLL_WITH_MOUSE |
                                  OVL_WINDOW_NO_MOVE | OVL_WINDOW_NO_NAV_INPUTS;
    if (ovl_begin("IME Dialog##ImeDialog", window_flags)) {
        lock_window_scroll();
        ovl_keep_nav_highlight();
        ovl_draw_pretty_background();
        const Libraries::Ime::ImePanelMetricsConfig metrics_cfg{
            .panel_w = panel_w,
            .panel_h = panel_h,
            .multiline = state->is_multi_line,
            .show_title = !state->title.empty(),
            .base_font_size = ovl_font_current_size(),
            .window_pos = ovl_window_pos(),
        };
        const Libraries::Ime::ImePanelMetrics metrics =
            Libraries::Ime::ComputeImePanelMetrics(metrics_cfg);

        const bool controller_shortcuts_disabled =
            True(state->disable_device & OrbisImeDisableDevice::CONTROLLER);
        const bool allow_osk_shortcuts = !controller_shortcuts_disabled;
        OskPadInputState panel_pad_state{
            prev_virtual_buttons,
            prev_virtual_cross_down,
            prev_virtual_lstick_left_down,
            prev_virtual_lstick_right_down,
            prev_virtual_lstick_up_down,
            prev_virtual_lstick_down_down,
            left_stick_repeat_dir,
            left_stick_next_repeat_time,
            virtual_cross_next_repeat_time,
            prev_virtual_dpad_left_down,
            prev_virtual_dpad_right_down,
            prev_virtual_dpad_up_down,
            prev_virtual_dpad_down_down,
            virtual_dpad_left_next_repeat_time,
            virtual_dpad_right_next_repeat_time,
            virtual_dpad_up_next_repeat_time,
            virtual_dpad_down_next_repeat_time,
        };
        const OskPadInputFrame panel_input = ComputeOskPadInputFrame(
            virtual_pad, allow_osk_shortcuts, first_render, panel_pad_state);

        const OskVirtualPadInputView virtual_pad_input(panel_input);
        const bool cross_down = panel_input.cross_down;
        const bool panel_activate_pressed_raw = panel_input.panel_activate_pressed_raw;
        const bool panel_activate_repeat_raw = panel_input.panel_activate_repeat_raw;
        const bool nav_left = panel_input.nav_left;
        const bool nav_right = panel_input.nav_right;
        const bool nav_up = panel_input.nav_up;
        const bool nav_down = panel_input.nav_down;
        const bool nav_left_repeat = panel_input.nav_left_repeat;
        const bool nav_right_repeat = panel_input.nav_right_repeat;
        const bool nav_up_repeat = panel_input.nav_up_repeat;
        const bool nav_down_repeat = panel_input.nav_down_repeat;
        const bool osk_control_input = panel_input.osk_control_input;

        const double nav_now = ovl_time();
        const bool cancel_shortcut_pressed =
            allow_osk_shortcuts &&
            virtual_pad_input.Pressed(Libraries::Pad::OrbisPadButtonDataOffset::Circle);
        if (osk_control_input) {
            pointer_navigation_active = false;
        }
        if (!accept_armed) {
            if (!cross_down) {
                accept_armed = true;
                LOG_DEBUG(Lib_ImeDialog, "ImeDialog: accept armed");
            }
        }
        const bool panel_activate_pressed =
            allow_osk_shortcuts && accept_armed &&
            (panel_activate_pressed_raw || panel_activate_repeat_raw);

        using SelectionIndex = Libraries::Ime::ImeSelectionGridIndex;
        const auto& selected_kb_layout = Libraries::Ime::GetImeKeyboardLayout(kb_layout_selection);
        const bool nav_layout_changed =
            nav_layout_selection_initialized &&
            (last_nav_layout_selection.family != kb_layout_selection.family ||
             last_nav_layout_selection.case_state != kb_layout_selection.case_state ||
             last_nav_layout_selection.page != kb_layout_selection.page);
        if (nav_layout_changed) {
            Libraries::Ime::ResetImeEdgeWrapNav(panel_vertical_nav_state);
        }
        last_nav_layout_selection = kb_layout_selection;
        nav_layout_selection_initialized = true;
        const int keyboard_min_col = 0;
        const int keyboard_min_row = 0;
        const int keyboard_cols = std::max(1, static_cast<int>(selected_kb_layout.cols));
        const int keyboard_rows = std::max(1, static_cast<int>(selected_kb_layout.rows));
        const int keyboard_max_col = keyboard_cols - 1;
        const int keyboard_max_row = keyboard_rows - 1;
        const auto keyboard_vertical_wraps_from = [&](int from_row, int from_col, int step_row) {
            return Libraries::Ime::DoesImeKeyboardNavigationWrap(selected_kb_layout, from_row,
                                                                 from_col, step_row, 0);
        };

        const auto& top_layout_cfg =
            Libraries::Ime::GetImeTopPanelLayoutConfig(kb_layout_selection);
        const int top_panel_row =
            SelectionIndex::PanelTopRowFromConfig(static_cast<int>(top_layout_cfg.row));
        const int top_panel_rows =
            SelectionIndex::PanelTopRowsFromConfig(static_cast<int>(top_layout_cfg.row_span));
        const int keyboard_panel_min_row =
            SelectionIndex::PanelKeyboardMinRowForTopPanel(top_panel_row, top_panel_rows);
        const int keyboard_panel_max_row = SelectionIndex::PanelKeyboardMaxRowForKeyboardRows(
            keyboard_rows, top_panel_row, top_panel_rows);
        const int top_col_min = 0;
        const int top_cols_cfg = std::max(1, static_cast<int>(top_layout_cfg.cols));
        const int top_col_max = top_col_min + top_cols_cfg - 1;

        const auto top_to_keyboard_col = [&](int top_col) {
            const int clamped_top = std::clamp(top_col, top_col_min, top_col_max);
            if (keyboard_cols <= 1 || top_cols_cfg <= 1) {
                return keyboard_min_col;
            }
            const int top_offset = clamped_top - top_col_min;
            return keyboard_min_col + (top_offset * (keyboard_cols - 1)) / (top_cols_cfg - 1);
        };
        const auto keyboard_to_top_col = [&](int keyboard_col) {
            const int clamped_kb = std::clamp(keyboard_col, keyboard_min_col, keyboard_max_col);
            if (top_cols_cfg <= 1 || keyboard_cols <= 1) {
                return top_col_min;
            }
            const int kb_offset = clamped_kb - keyboard_min_col;
            return top_col_min + (kb_offset * (top_cols_cfg - 1)) / (keyboard_cols - 1);
        };

        struct TopNavElement {
            PanelSelectionTarget target = PanelSelectionTarget::Prediction;
            int min_col = 0;
            int max_col = 0;
        };

        std::vector<int> top_col_to_element_index(static_cast<std::size_t>(top_cols_cfg), -1);
        std::vector<TopNavElement> top_elements;
        top_elements.reserve(static_cast<std::size_t>(top_cols_cfg));

        const auto append_top_element = [&](Libraries::Ime::ImeTopPanelElementId id, int min_col,
                                            int max_col) {
            PanelSelectionTarget target = PanelSelectionTarget::Prediction;
            switch (id) {
            case Libraries::Ime::ImeTopPanelElementId::Prediction:
                target = PanelSelectionTarget::Prediction;
                break;
            case Libraries::Ime::ImeTopPanelElementId::Close:
                target = PanelSelectionTarget::Close;
                break;
            default:
                return;
            }

            if (static_cast<int>(top_elements.size()) >= top_cols_cfg) {
                return;
            }
            if (max_col < min_col) {
                return;
            }

            const int clamped_min = std::clamp(min_col, top_col_min, top_col_max);
            const int clamped_max = std::clamp(max_col, top_col_min, top_col_max);
            if (clamped_max < clamped_min) {
                return;
            }

            top_elements.push_back({target, clamped_min, clamped_max});
            const int element_index = static_cast<int>(top_elements.size()) - 1;
            for (int col = clamped_min; col <= clamped_max; ++col) {
                top_col_to_element_index[static_cast<std::size_t>(col - top_col_min)] =
                    element_index;
            }
        };

        for (std::size_t i = 0; i < top_layout_cfg.element_count; ++i) {
            const auto& spec = top_layout_cfg.elements[i];
            const int min_col = std::clamp(static_cast<int>(spec.col), top_col_min, top_col_max);
            const int span = std::max(1, static_cast<int>(spec.col_span));
            const int max_col = std::clamp(min_col + span - 1, top_col_min, top_col_max);
            append_top_element(spec.id, min_col, max_col);
        }
        if (top_elements.empty()) {
            append_top_element(Libraries::Ime::ImeTopPanelElementId::Prediction, top_col_min,
                               top_col_max);
        }

        const auto element_index_for_col = [&](int col) {
            if (col < top_col_min || col > top_col_max) {
                return -1;
            }
            return top_col_to_element_index[static_cast<std::size_t>(col - top_col_min)];
        };
        const auto element_index_for_target = [&](PanelSelectionTarget target) {
            for (int i = 0; i < static_cast<int>(top_elements.size()); ++i) {
                if (top_elements[static_cast<std::size_t>(i)].target == target) {
                    return i;
                }
            }
            return -1;
        };
        const auto set_top_selection = [&](PanelSelectionTarget target, int preferred_col) {
            panel_selection = target;
            const int target_idx = element_index_for_target(target);
            if (target_idx < 0) {
                top_virtual_col = std::clamp(preferred_col, top_col_min, top_col_max);
                return;
            }
            const auto& element = top_elements[static_cast<std::size_t>(target_idx)];
            top_virtual_col = std::clamp(preferred_col, element.min_col, element.max_col);
        };
        const auto top_col_for_selection = [&](PanelSelectionTarget target) {
            const int target_idx = element_index_for_target(target);
            const int clamped_col = std::clamp(top_virtual_col, top_col_min, top_col_max);
            if (target_idx < 0) {
                return clamped_col;
            }
            const auto& element = top_elements[static_cast<std::size_t>(target_idx)];
            return std::clamp(clamped_col, element.min_col, element.max_col);
        };
        const bool menu_modal = (edit_menu_popup != EditMenuPopup::None);
        const int keyboard_row_before_panel_nav_raw =
            (pending_keyboard_row >= keyboard_min_row && pending_keyboard_row <= keyboard_max_row)
                ? pending_keyboard_row
                : last_keyboard_selected_row;
        const int keyboard_col_before_panel_nav_raw =
            (pending_keyboard_col >= keyboard_min_col && pending_keyboard_col <= keyboard_max_col)
                ? pending_keyboard_col
                : last_keyboard_selected_col;
        const int keyboard_row_before_panel_nav =
            std::clamp(keyboard_row_before_panel_nav_raw, keyboard_min_row, keyboard_max_row);
        const int keyboard_col_before_panel_nav =
            std::clamp(keyboard_col_before_panel_nav_raw, keyboard_min_col, keyboard_max_col);
        bool entered_top_from_keyboard = false;
        const auto move_keyboard_edge_to_top = [&](int wrap_dir_y, int keyboard_col) {
            const int top_col = keyboard_to_top_col(keyboard_col);
            const int element_idx = element_index_for_col(top_col);
            if (element_idx >= 0) {
                const auto& element = top_elements[static_cast<std::size_t>(element_idx)];
                set_top_selection(element.target, top_col);
            } else if (!top_elements.empty()) {
                const auto& first_element = top_elements[0];
                set_top_selection(first_element.target, first_element.min_col);
            }
            Libraries::Ime::CommitImeEdgeWrapStep(panel_vertical_nav_state, wrap_dir_y, 0, nav_now);
            entered_top_from_keyboard = true;
        };
        if (!menu_modal && !text_select_mode && !pointer_navigation_active &&
            panel_selection == PanelSelectionTarget::Keyboard) {
            const int wrap_dir_y =
                (nav_up && keyboard_vertical_wraps_from(keyboard_row_before_panel_nav,
                                                        keyboard_col_before_panel_nav, -1))
                    ? -1
                    : ((nav_down && keyboard_vertical_wraps_from(keyboard_row_before_panel_nav,
                                                                 keyboard_col_before_panel_nav, 1))
                           ? 1
                           : 0);
            if (wrap_dir_y != 0) {
                const bool hold_before_wrap_to_top = wrap_dir_y > 0;
                const bool wrap_repeat = nav_down_repeat;
                if (!hold_before_wrap_to_top ||
                    !Libraries::Ime::ShouldDelayImeEdgeWrap(
                        panel_vertical_nav_state, wrap_dir_y, 0, wrap_repeat, true, nav_now,
                        kPanelEdgeWrapHoldDelaySec, kRepeatIntentWindowSec)) {
                    move_keyboard_edge_to_top(wrap_dir_y, keyboard_col_before_panel_nav);
                }
            }
        }

        ovl_set_window_font_scale(std::max(ui_scale, metrics.input_font_scale));

        if (first_render) {
            const auto game_res = DebugState.game_resolution;
            const auto out_res = DebugState.output_resolution;
            const float req_w = has_layout ? static_cast<float>(layout.width)
                                           : static_cast<float>(state->panel_req_width);
            const float req_h = has_layout ? static_cast<float>(layout.height)
                                           : static_cast<float>(state->panel_req_height);
            LOG_INFO(Lib_ImeDialog,
                     "ImeDialog UI metrics: game_res={}x{}, out_res={}x{}, viewport_pos=({}, {}), "
                     "viewport_size=({}, {}), base={}x{}, scale=({:.4f}, {:.4f}), "
                     "panel_req=({}, {}), panel_scaled=({}, {})",
                     game_res.first, game_res.second, out_res.first, out_res.second,
                     viewport_offset.x, viewport_offset.y, viewport_size.x, viewport_size.y,
                     viewport.base_w, viewport.base_h, scale_x, scale_y, req_w, req_h,
                     window_size.x, window_size.y);
        }

        if (!state->title.empty()) {
            ovl_set_cursor_pos_y(0.0f);
            ovl_set_cursor_pos_x(metrics.padding_x);
            ovl_set_window_font_scale(metrics.label_font_scale);
            ovl_text(state->title.data(), nullptr);
            ovl_set_window_font_scale(std::max(ui_scale, metrics.input_font_scale));
        }

        DrawInputText(metrics, state->is_multi_line);
        static std::unordered_map<u32, Libraries::Ime::SelectorFadeState> s_selector_fade_states;
        const auto draw_selector = [&](const char* selector_id, ovl_vec2_t pos, ovl_vec2_t size,
                                       bool selected, bool pulse_triggered) {
            auto& fade_state = s_selector_fade_states[ovl_get_id(selector_id)];
            const double now = ovl_time();
            if (selected && pulse_triggered) {
                Libraries::Ime::TriggerSelectorPressPulse(fade_state, now);
            }
            const float selector_corner_radius =
                std::max(0.0f, metrics.corner_radius - kSelectorInnerMargin);
            Libraries::Ime::UpdateSelectorFadeState(fade_state, pos, size, kSelectorInnerMargin,
                                                    selector_corner_radius, selected, now);
            const float press_pulse_expand =
                selected ? Libraries::Ime::ComputePressPulseExpand(
                               fade_state.press_pulse_started_at, now,
                               Libraries::Ime::kSelectorPressPulseDurationSec,
                               kSelectorBorderThickness *
                                   Libraries::Ime::kSelectorPressPulseExpandBorderFactor)
                         : 0.0f;
            Libraries::Ime::DrawSelectorFadeState(
                fade_state, ovl_window_draw_list(), kSelectorOverlayColor, kSelectorBorderColor,
                kSelectorBorderThickness, Libraries::Ime::kSelectorFadeOutDurationSec, now,
                press_pulse_expand);
        };
        draw_selector(kSelectorInputId, metrics.input_pos_screen, metrics.input_size, false,
                      panel_selection == PanelSelectionTarget::Input && panel_activate_pressed_raw);

        auto* draw = ovl_window_draw_list();
        const ovl_col_t pane_bg = Libraries::Ime::ImeColorToCol(state->style_config.color_base);
        ovl_add_rect_filled(draw, metrics.predict_pos,
                            {metrics.predict_pos.x + metrics.predict_size.x,
                             metrics.predict_pos.y + metrics.predict_size.y},
                            pane_bg, metrics.corner_radius, 0);
        draw_selector(kSelectorPredictId, metrics.predict_pos, metrics.predict_size,
                      panel_selection == PanelSelectionTarget::Prediction,
                      panel_selection == PanelSelectionTarget::Prediction &&
                          panel_activate_pressed_raw);
        const ovl_col_t close_button_bg =
            Libraries::Ime::ImeColorToCol(state->style_config.color_button_function);
        ovl_add_rect_filled(draw, metrics.close_pos,
                            {metrics.close_pos.x + metrics.close_size.x,
                             metrics.close_pos.y + metrics.close_size.y},
                            ovl_color_u32_vec4(BrightenColor(close_button_bg, 0.0f)), 0.0f, 0);
        constexpr const char* kCloseLabel = "\xE2\x9C\x95";
        const ovl_vec2_t close_label_size = ovl_calc_text_size(kCloseLabel, nullptr, 0.0f);
        const float close_pad_y = std::max(1.0f, metrics.close_size.y * 0.04f);
        const ovl_vec2_t close_label_pos{metrics.close_pos.x +
                                             (metrics.close_size.x - close_label_size.x) * 0.5f,
                                         metrics.close_pos.y + close_pad_y};
        ovl_add_text(draw, ovl_font_current(), ovl_font_current_size(), close_label_pos,
                     Libraries::Ime::ImeColorToCol(state->style_config.color_text), kCloseLabel,
                     nullptr, 0.0f);
        bool cancel_pressed = cancel_shortcut_pressed;
        draw_selector(kSelectorCloseId, metrics.close_pos, metrics.close_size,
                      panel_selection == PanelSelectionTarget::Close,
                      panel_selection == PanelSelectionTarget::Close && panel_activate_pressed_raw);
        if (!cancel_pressed && panel_selection == PanelSelectionTarget::Close &&
            panel_activate_pressed) {
            cancel_pressed = true;
        }

        ovl_set_cursor_screen_pos(metrics.kb_pos);

        bool entered_keyboard_from_top = false;
        const auto move_top_navigation = [&](int dir_x, int dir_y) {
            const PanelSelectionTarget current = panel_selection;
            int col = top_col_for_selection(current);
            int origin_element_idx = element_index_for_col(col);
            if (origin_element_idx < 0) {
                origin_element_idx = element_index_for_target(current);
            }

            if (dir_x != 0) {
                if (origin_element_idx >= 0) {
                    const int span = top_col_max - top_col_min + 1;
                    bool crossed_wrap = false;
                    for (int step = 1; step <= span; ++step) {
                        const int next_col_raw = col + dir_x * step;
                        crossed_wrap = crossed_wrap || next_col_raw < top_col_min ||
                                       next_col_raw > top_col_max;
                        const int next_col =
                            top_col_min + (col - top_col_min + dir_x * step + span) % span;
                        const int next_element_idx = element_index_for_col(next_col);
                        if (next_element_idx < 0 || next_element_idx == origin_element_idx) {
                            continue;
                        }
                        const bool repeat_hint = dir_x < 0 ? nav_left_repeat : nav_right_repeat;
                        if (Libraries::Ime::ShouldDelayImeEdgeWrap(
                                panel_vertical_nav_state, 0, dir_x, repeat_hint, crossed_wrap,
                                nav_now, kPanelEdgeWrapHoldDelaySec, kRepeatIntentWindowSec)) {
                            return;
                        }
                        const auto& next_element =
                            top_elements[static_cast<std::size_t>(next_element_idx)];
                        set_top_selection(next_element.target, next_col);
                        Libraries::Ime::CommitImeEdgeWrapStep(panel_vertical_nav_state, 0, dir_x,
                                                              nav_now);
                        return;
                    }
                }
            }

            if (origin_element_idx >= 0) {
                if (dir_y > 0) {
                    pending_keyboard_row = SelectionIndex::PanelToKeyboardRow(
                        keyboard_panel_min_row, keyboard_rows, top_panel_row, top_panel_rows);
                    pending_keyboard_col = top_to_keyboard_col(col);
                    panel_selection = PanelSelectionTarget::Keyboard;
                    entered_keyboard_from_top = true;
                    Libraries::Ime::CommitImeEdgeWrapStep(panel_vertical_nav_state, dir_y, 0,
                                                          nav_now);
                    return;
                }
                if (dir_y < 0) {
                    if (Libraries::Ime::ShouldDelayImeEdgeWrap(
                            panel_vertical_nav_state, dir_y, 0, nav_up_repeat, true, nav_now,
                            kPanelEdgeWrapHoldDelaySec, kRepeatIntentWindowSec)) {
                        return;
                    }
                    pending_keyboard_row = SelectionIndex::PanelToKeyboardRow(
                        keyboard_panel_max_row, keyboard_rows, top_panel_row, top_panel_rows);
                    pending_keyboard_col = top_to_keyboard_col(col);
                    panel_selection = PanelSelectionTarget::Keyboard;
                    entered_keyboard_from_top = true;
                    Libraries::Ime::CommitImeEdgeWrapStep(panel_vertical_nav_state, dir_y, 0,
                                                          nav_now);
                    return;
                }
            }
        };
        if (!menu_modal && !text_select_mode && !pointer_navigation_active &&
            !entered_top_from_keyboard && panel_selection != PanelSelectionTarget::Keyboard) {
            if (nav_left) {
                move_top_navigation(-1, 0);
            } else if (nav_right) {
                move_top_navigation(1, 0);
            } else if (nav_up) {
                move_top_navigation(0, -1);
            } else if (nav_down) {
                move_top_navigation(0, 1);
            }
        }

        bool accept_pressed = false;
        const auto text_length_utf16 = [&]() {
            const char* text = state->current_text.begin();
            const int byte_len = static_cast<int>(state->current_text.size());
            return Utf16CountFromUtf8Range(text, text ? (text + byte_len) : nullptr);
        };
        const auto sync_text_buffers = [&]() {
            state->input_changed = true;
            (void)state->CopyTextToOrbisBuffer(false);
        };
        const auto apply_selection_state = [&]() {
            const int len = text_length_utf16();
            const int caret = std::clamp(
                (text_select_focus_utf16 >= 0)
                    ? text_select_focus_utf16
                    : ((state->caret_index >= 0) ? state->caret_index : input_cursor_utf16),
                0, len);
            const int anchor =
                text_select_mode ? std::clamp(text_select_anchor_utf16, 0, len) : caret;
            const int focus =
                text_select_mode ? std::clamp(text_select_focus_utf16, 0, len) : caret;
            const char* text = state->current_text.begin();
            const int anchor_byte = Utf8ByteIndexFromUtf16Index(text ? text : "", anchor);
            const int focus_byte = Utf8ByteIndexFromUtf16Index(text ? text : "", focus);
            input_cursor_utf16 = focus;
            input_cursor_byte = focus_byte;
            input_selection_start_byte = std::min(anchor_byte, focus_byte);
            input_selection_end_byte = std::max(anchor_byte, focus_byte);
            state->caret_index = focus;
            state->caret_byte_index = focus_byte;
            state->caret_dirty = false;
        };
        const auto apply_text_edit = [&](int start_utf16, int end_utf16,
                                         const char* insert_utf8) -> bool {
            const std::string current = state->current_text.to_string();
            const char* current_text = current.c_str();
            const int len_utf16 = Utf16CountFromUtf8Range(
                current_text, current_text + static_cast<int>(current.size()));
            const int start = std::clamp(start_utf16, 0, len_utf16);
            const int end = std::clamp(end_utf16, start, len_utf16);
            const int removed_utf16 = end - start;
            const int available_utf16 =
                static_cast<int>(state->max_text_length) - (len_utf16 - removed_utf16);

            std::string insert_clamped{};
            if (insert_utf8 && insert_utf8[0] != '\0' && available_utf16 > 0) {
                const char* p = insert_utf8;
                int used_utf16 = 0;
                while (*p && used_utf16 < available_utf16) {
                    u32 codepoint = 0;
                    const int step = Utf8Step(p, &codepoint);
                    if (step <= 0) {
                        break;
                    }
                    const int units = (codepoint > 0xFFFF) ? 2 : 1;
                    if (used_utf16 + units > available_utf16) {
                        break;
                    }
                    insert_clamped.append(p, static_cast<std::size_t>(step));
                    p += step;
                    used_utf16 += units;
                }
            }

            if (removed_utf16 == 0 && insert_clamped.empty()) {
                return false;
            }

            const int start_byte = Utf8ByteIndexFromUtf16Index(current_text, start);
            const int end_byte = Utf8ByteIndexFromUtf16Index(current_text, end);
            std::string updated{};
            updated.reserve(current.size() - static_cast<std::size_t>(end_byte - start_byte) +
                            insert_clamped.size());
            updated.append(current, 0, static_cast<std::size_t>(start_byte));
            updated.append(insert_clamped);
            updated.append(current, static_cast<std::size_t>(end_byte), std::string::npos);

            state->current_text.FromString(updated);
            if (state->is_multi_line) {
                (void)state->NormalizeNewlines();
            }
            (void)state->ClampCurrentTextToMaxLen();
            sync_text_buffers();

            const int inserted_utf16 = Utf16CountFromUtf8Range(
                insert_clamped.c_str(),
                insert_clamped.c_str() + static_cast<int>(insert_clamped.size()));
            const int new_caret_utf16 = std::clamp(start + inserted_utf16, 0, text_length_utf16());
            text_select_mode = false;
            text_select_anchor_utf16 = new_caret_utf16;
            text_select_focus_utf16 = new_caret_utf16;
            apply_selection_state();
            return true;
        };
        const auto selection_utf16_range = [&]() -> std::pair<int, int> {
            const int len = text_length_utf16();
            if (!text_select_mode) {
                const int caret = std::clamp(state->caret_index, 0, len);
                return {caret, caret};
            }
            const int anchor = std::clamp(text_select_anchor_utf16, 0, len);
            const int focus = std::clamp(text_select_focus_utf16, 0, len);
            return {std::min(anchor, focus), std::max(anchor, focus)};
        };
        const auto insert_text_at_caret = [&](const char* suffix) {
            const auto [sel_start, sel_end] = selection_utf16_range();
            return apply_text_edit(sel_start, sel_end, suffix);
        };
        const auto backspace_at_caret = [&]() {
            const auto [sel_start, sel_end] = selection_utf16_range();
            if (sel_end > sel_start) {
                return apply_text_edit(sel_start, sel_end, "");
            }
            if (sel_end <= 0) {
                return false;
            }
            const std::string current = state->current_text.to_string();
            const char* text = current.c_str();
            const int caret_byte = Utf8ByteIndexFromUtf16Index(text, sel_end);
            int prev_byte = caret_byte;
            do {
                --prev_byte;
            } while (prev_byte > 0 &&
                     (static_cast<unsigned char>(text[prev_byte]) & 0xC0u) == 0x80u);
            const int prev_utf16 = Utf16CountFromUtf8Range(text, text + prev_byte);
            return apply_text_edit(prev_utf16, sel_end, "");
        };
        const auto clear_all_text = [&]() {
            const int len = text_length_utf16();
            if (len <= 0) {
                return false;
            }
            // Matches libSceIme backend all-delete behavior (sceImeBackendAllDeleteConvertString):
            // clear the whole editable string in one operation.
            return apply_text_edit(0, len, "");
        };

        Libraries::Ime::ImeKbGridLayout kb_layout{};
        kb_layout.pos = metrics.kb_pos;
        kb_layout.size = metrics.kb_size;
        kb_layout.key_gap_x = metrics.key_gap;
        kb_layout.key_gap_y = metrics.key_gap;
        kb_layout.cols = keyboard_cols;
        kb_layout.rows = keyboard_rows;
        const int layout_rows = std::max(1, kb_layout.rows);
        kb_layout.fixed_bottom_rows = SelectionIndex::ResolveFunctionRows(
            layout_rows, static_cast<int>(selected_kb_layout.function_rows));
        if (kb_layout.fixed_bottom_rows > 0) {
            kb_layout.bottom_row_h = std::max(8.0f, metrics.key_h);
            const int typing_rows = std::max(1, layout_rows - kb_layout.fixed_bottom_rows);
            const float typing_area_h =
                kb_layout.size.y - kb_layout.key_gap_y * static_cast<float>(layout_rows - 1) -
                kb_layout.bottom_row_h * static_cast<float>(kb_layout.fixed_bottom_rows);
            const float computed_typing_key_h = typing_area_h / static_cast<float>(typing_rows);
            kb_layout.key_h = std::max(8.0f, computed_typing_key_h);
        } else {
            kb_layout.fixed_bottom_rows = 0;
            kb_layout.bottom_row_h = 0.0f;
            const float computed_key_h =
                (kb_layout.size.y - kb_layout.key_gap_y * static_cast<float>(layout_rows - 1)) /
                static_cast<float>(layout_rows);
            kb_layout.key_h = std::max(8.0f, computed_key_h);
        }
        kb_layout.corner_radius = metrics.corner_radius;

        Libraries::Ime::ImeKbDrawParams kb_params{};
        kb_params.selection = kb_layout_selection;
        kb_params.layout_model = &selected_kb_layout;
        kb_params.supported_languages = state->supported_languages;
        kb_params.enter_label = state->enter_label;
        kb_params.show_selection_highlight = (panel_selection == PanelSelectionTarget::Keyboard);
        kb_params.allow_nav_input = allow_osk_shortcuts && !menu_modal && !text_select_mode &&
                                    (panel_selection == PanelSelectionTarget::Keyboard) &&
                                    !entered_keyboard_from_top;
        kb_params.use_lstick_key_nav = false;
        kb_params.allow_activate_input = allow_osk_shortcuts && accept_armed && !menu_modal &&
                                         !text_select_mode &&
                                         (panel_selection == PanelSelectionTarget::Keyboard);
        ApplyOskPanelNavToKeyboardParams(kb_params, allow_osk_shortcuts, panel_input);
        kb_params.external_activate_pressed = panel_activate_pressed;
        kb_params.external_activate_repeat =
            allow_osk_shortcuts && accept_armed && panel_activate_repeat_raw;
        kb_params.reset_nav_state = nav_layout_changed;
        kb_params.requested_selected_row = pending_keyboard_row;
        kb_params.requested_selected_col = pending_keyboard_col;
        Libraries::Ime::ApplyImeStyleToKeyboardDrawParams(state->style_config, kb_params);
        pending_keyboard_row = -1;
        pending_keyboard_col = -1;

        Libraries::Ime::ImeKbDrawState kb_state{};
        ovl_set_window_font_scale(metrics.key_font_scale);
        Libraries::Ime::DrawImeKeyboardGrid(kb_layout, kb_params, kb_state);
        ovl_set_window_font_scale(metrics.input_font_scale);
        if (kb_state.selected_row >= 0 && kb_state.selected_col >= 0) {
            last_keyboard_selected_row = kb_state.selected_row;
            last_keyboard_selected_col = kb_state.selected_col;
        }
        const auto consume_temporary_uppercase = [&](bool typed_character) {
            if (typed_character &&
                kb_layout_selection.case_state == Libraries::Ime::ImeKbCaseState::Upper) {
                kb_layout_selection.case_state = Libraries::Ime::ImeKbCaseState::Lower;
            }
        };
        const auto has_clipboard_text = [&]() {
            const char* text = ImeClipboardText();
            return text && text[0] != '\0';
        };
        const auto get_selection_byte_range = [&]() {
            const int text_len = static_cast<int>(state->current_text.size());
            const int sel_start = std::clamp(input_selection_start_byte, 0, text_len);
            const int sel_end = std::clamp(input_selection_end_byte, 0, text_len);
            return std::pair{std::min(sel_start, sel_end), std::max(sel_start, sel_end)};
        };
        const auto copy_selected_text = [&]() {
            const std::string current = state->current_text.to_string();
            const auto [sel_start, sel_end] = get_selection_byte_range();
            if (sel_end > sel_start) {
                const std::string selection =
                    current.substr(static_cast<std::size_t>(sel_start),
                                   static_cast<std::size_t>(sel_end - sel_start));
                SetImeClipboardText(selection.c_str());
            } else {
                SetImeClipboardText(current.c_str());
            }
        };
        const auto collapse_selection_to_caret = [&](int caret_utf16) {
            const int len = text_length_utf16();
            const int clamped_caret = std::clamp(caret_utf16, 0, len);
            text_select_mode = false;
            text_select_anchor_utf16 = clamped_caret;
            text_select_focus_utf16 = clamped_caret;
            apply_selection_state();
            panel_selection = PanelSelectionTarget::Keyboard;
        };
        const auto move_text_caret = [&](int delta_utf16, bool preserve_selection) {
            const int len = text_length_utf16();
            int base = text_select_mode ? text_select_focus_utf16 : input_cursor_utf16;
            if (base < 0) {
                base = input_cursor_utf16;
            }
            base = std::clamp(base, 0, len);
            const int next = std::clamp(base + delta_utf16, 0, len);
            if (preserve_selection && text_select_mode) {
                text_select_focus_utf16 = next;
                apply_selection_state();
                panel_selection = PanelSelectionTarget::Keyboard;
                return next != base;
            }
            collapse_selection_to_caret(next);
            return next != base;
        };
        const auto move_text_caret_to_boundary = [&](bool to_end, bool preserve_selection) {
            const int len = text_length_utf16();
            int base = text_select_mode ? text_select_focus_utf16 : input_cursor_utf16;
            if (base < 0) {
                base = input_cursor_utf16;
            }
            base = std::clamp(base, 0, len);
            const int next = to_end ? len : 0;
            if (preserve_selection && text_select_mode) {
                text_select_focus_utf16 = next;
                apply_selection_state();
                panel_selection = PanelSelectionTarget::Keyboard;
                return next != base;
            }
            collapse_selection_to_caret(next);
            return next != base;
        };
        const auto begin_text_selection_from_caret = [&]() {
            text_select_mode = true;
            const int len = text_length_utf16();
            const int caret = std::clamp(input_cursor_utf16, 0, len);
            text_select_anchor_utf16 = caret;
            text_select_focus_utf16 = caret;
            apply_selection_state();
            panel_selection = PanelSelectionTarget::Keyboard;
        };
        const auto select_all_text = [&]() {
            text_select_mode = true;
            const int len = text_length_utf16();
            text_select_anchor_utf16 = 0;
            text_select_focus_utf16 = len;
            apply_selection_state();
            panel_selection = PanelSelectionTarget::Keyboard;
        };
        const auto open_main_menu = [&]() {
            Libraries::Ime::OpenOskMainEditMenu(edit_menu_popup, edit_menu_index,
                                                menu_activate_armed);
        };
        const auto open_actions_menu = [&]() {
            Libraries::Ime::OpenOskActionsEditMenu(edit_menu_popup, edit_menu_index,
                                                   menu_activate_armed);
        };
        const auto apply_main_menu_action = [&](int action_index) {
            switch (action_index) {
            case 0: // Select
                begin_text_selection_from_caret();
                edit_menu_popup = EditMenuPopup::None;
                break;
            case 1: // Select All
                select_all_text();
                open_actions_menu();
                break;
            case 2: // Paste
                if (has_clipboard_text()) {
                    (void)insert_text_at_caret(ImeClipboardText());
                }
                edit_menu_popup = EditMenuPopup::None;
                break;
            default:
                break;
            }
        };
        const auto apply_actions_menu_action = [&](int action_index) {
            switch (action_index) {
            case 0: // Copy
                copy_selected_text();
                collapse_selection_to_caret(text_select_focus_utf16 >= 0 ? text_select_focus_utf16
                                                                         : input_cursor_utf16);
                edit_menu_popup = EditMenuPopup::None;
                break;
            case 1: // Paste
                if (has_clipboard_text()) {
                    (void)insert_text_at_caret(ImeClipboardText());
                }
                edit_menu_popup = EditMenuPopup::None;
                break;
            default:
                break;
            }
        };

        bool opened_menu_this_frame = false;
        Libraries::Ime::OskShortcutRepeatState shortcut_repeat_state{
            prev_virtual_square_down,
            prev_virtual_l1_down,
            prev_virtual_r1_down,
            l2_shortcut_armed,
            virtual_square_next_repeat_time,
            virtual_l1_next_repeat_time,
            virtual_r1_next_repeat_time,
            virtual_triangle_next_repeat_time};
        const Libraries::Ime::OskShortcutActionResult shortcut_action =
            Libraries::Ime::EvaluateOskShortcutAction(
                allow_osk_shortcuts, menu_modal,
                kb_state.pressed_action == Libraries::Ime::ImeKbKeyAction::None, panel_input,
                virtual_pad_input, prev_virtual_buttons, kb_layout_selection.family,
                shortcut_repeat_state);
        bool keyboard_action_from_hotkey = false;
        if (shortcut_action.clear_all) {
            (void)clear_all_text();
        } else if (shortcut_action.action != Libraries::Ime::ImeKbKeyAction::None) {
            kb_state.pressed_action = shortcut_action.action;
            keyboard_action_from_hotkey = true;
        }
        switch (kb_state.pressed_action) {
        case Libraries::Ime::ImeKbKeyAction::Character:
            consume_temporary_uppercase(insert_text_at_caret(kb_state.pressed_label));
            break;
        case Libraries::Ime::ImeKbKeyAction::Shift:
            Libraries::Ime::CycleKeyboardCaseState(kb_layout_selection);
            break;
        case Libraries::Ime::ImeKbKeyAction::SymbolsMode:
            Libraries::Ime::ToggleKeyboardFamilyMode(kb_layout_selection, kb_alpha_family,
                                                     Libraries::Ime::ImeKbLayoutFamily::Symbols);
            if (!keyboard_action_from_hotkey &&
                Libraries::Ime::FocusKeyboardActionKeySelection(
                    kb_layout_selection, Libraries::Ime::ImeKbKeyAction::SymbolsMode,
                    pending_keyboard_row, pending_keyboard_col)) {
                last_keyboard_selected_row = pending_keyboard_row;
                last_keyboard_selected_col = pending_keyboard_col;
                panel_selection = PanelSelectionTarget::Keyboard;
            }
            break;
        case Libraries::Ime::ImeKbKeyAction::SpecialsMode:
            Libraries::Ime::ToggleKeyboardFamilyMode(kb_layout_selection, kb_alpha_family,
                                                     Libraries::Ime::ImeKbLayoutFamily::Specials);
            if (!keyboard_action_from_hotkey &&
                Libraries::Ime::FocusKeyboardActionKeySelection(
                    kb_layout_selection, Libraries::Ime::ImeKbKeyAction::SpecialsMode,
                    pending_keyboard_row, pending_keyboard_col)) {
                last_keyboard_selected_row = pending_keyboard_row;
                last_keyboard_selected_col = pending_keyboard_col;
                panel_selection = PanelSelectionTarget::Keyboard;
            }
            break;
        case Libraries::Ime::ImeKbKeyAction::ArrowLeft:
            (void)move_text_caret(-1, text_select_mode);
            break;
        case Libraries::Ime::ImeKbKeyAction::ArrowRight:
            (void)move_text_caret(1, text_select_mode);
            break;
        case Libraries::Ime::ImeKbKeyAction::ArrowUp:
            if (!state->is_multi_line) {
                // Single-line OSK behavior: up jumps caret to the beginning.
                (void)move_text_caret_to_boundary(false, text_select_mode);
            }
            break;
        case Libraries::Ime::ImeKbKeyAction::ArrowDown:
            if (!state->is_multi_line) {
                // Single-line OSK behavior: down jumps caret to the end.
                (void)move_text_caret_to_boundary(true, text_select_mode);
            }
            break;
        case Libraries::Ime::ImeKbKeyAction::PagePrev:
            Libraries::Ime::FlipKeyboardModePage(kb_layout_selection, -1);
            break;
        case Libraries::Ime::ImeKbKeyAction::PageNext:
            Libraries::Ime::FlipKeyboardModePage(kb_layout_selection, 1);
            break;
        case Libraries::Ime::ImeKbKeyAction::Space:
            (void)insert_text_at_caret(" ");
            break;
        case Libraries::Ime::ImeKbKeyAction::Backspace:
            (void)backspace_at_caret();
            break;
        case Libraries::Ime::ImeKbKeyAction::NewLine:
            if (state->is_multi_line) {
                (void)insert_text_at_caret("\n");
            } else {
                accept_pressed = true;
            }
            break;
        case Libraries::Ime::ImeKbKeyAction::Menu:
            if (edit_menu_popup == EditMenuPopup::None) {
                open_main_menu();
                opened_menu_this_frame = true;
            } else {
                edit_menu_popup = EditMenuPopup::None;
                menu_activate_armed = true;
            }
            break;
        case Libraries::Ime::ImeKbKeyAction::Settings:
            pointer_navigation_active = !pointer_navigation_active;
            if (!pointer_navigation_active) {
                panel_selection = PanelSelectionTarget::Keyboard;
            }
            break;
        case Libraries::Ime::ImeKbKeyAction::Done:
            accept_pressed = true;
            break;
        default:
            break;
        }
        if (kb_state.done_pressed) {
            accept_pressed = true;
        }

        if (text_select_mode && edit_menu_popup == EditMenuPopup::None &&
            !pointer_navigation_active) {
            int delta = 0;
            if (nav_left || nav_up) {
                delta = -1;
            } else if (nav_right || nav_down) {
                delta = 1;
            }
            if (delta != 0) {
                const int len = text_length_utf16();
                if (text_select_anchor_utf16 < 0 || text_select_focus_utf16 < 0) {
                    const int caret = std::clamp(input_cursor_utf16, 0, len);
                    text_select_anchor_utf16 = caret;
                    text_select_focus_utf16 = caret;
                }
                text_select_focus_utf16 = std::clamp(text_select_focus_utf16 + delta, 0, len);
                apply_selection_state();
            } else if (panel_activate_pressed) {
                open_actions_menu();
                opened_menu_this_frame = true;
            }
        }

        if (Libraries::Ime::CloseOskEditMenuOnCancel(edit_menu_popup, cancel_pressed,
                                                     menu_activate_armed)) {
            // Popup closed.
        } else if (text_select_mode && cancel_pressed) {
            cancel_pressed = false;
            text_select_mode = false;
            const int len = text_length_utf16();
            const int caret = std::clamp(text_select_focus_utf16, 0, len);
            text_select_anchor_utf16 = caret;
            text_select_focus_utf16 = caret;
            apply_selection_state();
        }

        if (edit_menu_popup != EditMenuPopup::None) {
            const bool clipboard_ready = has_clipboard_text();
            const auto previous_popup = edit_menu_popup;
            (void)Libraries::Ime::DrawAndHandleOskEditMenuPopup(
                edit_menu_popup, edit_menu_index, metrics, draw, pointer_navigation_active, nav_up,
                nav_down, cross_down, panel_activate_pressed, opened_menu_this_frame,
                menu_activate_armed, clipboard_ready,
                [&](const EditMenuPopup source_popup, const int action_index) {
                    if (source_popup == EditMenuPopup::Main) {
                        apply_main_menu_action(action_index);
                    } else {
                        apply_actions_menu_action(action_index);
                    }
                });
            if (edit_menu_popup != EditMenuPopup::None && edit_menu_popup != previous_popup) {
                opened_menu_this_frame = true;
            }
        }

        ovl_dummy({metrics.kb_size.x, metrics.kb_size.y + metrics.padding_bottom});

        if (accept_pressed) {
            LOG_INFO(Lib_ImeDialog, "ImeDialog OK text(len={}): \"{}\"", state->current_text.size(),
                     state->current_text.begin());
            FinishDialog(OrbisImeDialogEndStatus::Ok, false, "OK");
        } else if (cancel_pressed) {
            FinishDialog(OrbisImeDialogEndStatus::UserCanceled, true, "Cancel");
        }
        CommitOskPadInputFrame(panel_input, panel_pad_state);
        lock_window_scroll();
        ovl_set_window_font_scale(1.0f);
    }
    ovl_end();

    first_render = false;
}

void ImeDialogUi::DrawInputText(const Libraries::Ime::ImePanelMetrics& metrics, bool multiline) {
    const ovl_vec2_t input_size = metrics.input_size;
    ovl_set_cursor_pos(metrics.input_pos_local);

    if (state->caret_dirty && !text_select_mode) {
        const char* text = state->current_text.begin();
        const int len_utf16 =
            Utf16CountFromUtf8Range(text, text + static_cast<int>(state->current_text.size()));
        int caret_utf16 = state->caret_index;
        if (caret_utf16 < 0) {
            caret_utf16 = 0;
        } else if (caret_utf16 > len_utf16) {
            caret_utf16 = len_utf16;
        }
        const int caret_byte = Utf8ByteIndexFromUtf16Index(text, caret_utf16);
        state->caret_index = caret_utf16;
        state->caret_byte_index = caret_byte;
        input_cursor_utf16 = caret_utf16;
        input_cursor_byte = caret_byte;
        input_selection_start_byte = caret_byte;
        input_selection_end_byte = caret_byte;
        text_select_anchor_utf16 = caret_utf16;
        text_select_focus_utf16 = caret_utf16;
        state->caret_dirty = false;
    }

    // The field only shows the text: the keys below are what changes it
    const char* placeholder = state->placeholder.empty() ? nullptr : state->placeholder.data();
    ovl_push_color(OVL_COLOR_FRAME_BG,
                   Libraries::Ime::ImeColorToVec4(state->style_config.color_text_field));
    ovl_push_color(OVL_COLOR_TEXT, Libraries::Ime::ImeColorToVec4(state->style_config.color_text));
    // The caret kept in view: the text moved left, and up on several lines, as far as it takes
    Libraries::Ime::FollowCaretInTextField(input_size, state->current_text.begin(),
                                           input_cursor_byte, multiline, input_scroll);
    ovl_text_field_scrolled("##ImeDialogInput", placeholder, state->current_text.begin(),
                            input_size, multiline, input_scroll);
    ovl_pop_color(2);
    DrawInactiveCaretOverlay(ovl_item_rect_min(), ovl_item_rect_max(), state->current_text.begin(),
                             input_cursor_byte, input_selection_start_byte,
                             input_selection_end_byte, multiline, input_scroll);
}

} // namespace Libraries::ImeDialog
