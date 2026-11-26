/* SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

/* libSceIme's and libSceImeDialog's types and constants. An enumeration is the integer the
 * game's structures hold it in, and its values are macros: some do not fit a C89 enum (the
 * errors, the 64-bit language flags). */

#ifndef SHADPS4_LIBRARIES_IME_COMMON_H
#define SHADPS4_LIBRARIES_IME_COMMON_H

#include "common/enum_names.h"
#include "common/types.h"
#include "core/libraries/rtc/rtc.h"
#include "core/libraries/system/userservice.h"

#ifndef __cplusplus
/* A UTF-16 code unit: C++'s own type there */
typedef u16 char16_t;
#endif

#define ORBIS_IME_MAX_TEXT_LENGTH 2048u
#define ORBIS_IME_DIALOG_MAX_TEXT_LENGTH 2048u

typedef u32 OrbisImeError;
#define ORBIS_IME_ERROR_OK 0x0u

/* ImeDialog library */
#define ORBIS_IME_ERROR_BUSY 0x80bc0001u
#define ORBIS_IME_ERROR_NOT_OPENED 0x80bc0002u
#define ORBIS_IME_ERROR_NO_MEMORY 0x80bc0003u
#define ORBIS_IME_ERROR_CONNECTION_FAILED 0x80bc0004u
#define ORBIS_IME_ERROR_TOO_MANY_REQUESTS 0x80bc0005u
#define ORBIS_IME_ERROR_INVALID_TEXT 0x80bc0006u
#define ORBIS_IME_ERROR_EVENT_OVERFLOW 0x80bc0007u
#define ORBIS_IME_ERROR_NOT_ACTIVE 0x80bc0008u
#define ORBIS_IME_ERROR_IME_SUSPENDING 0x80bc0009u
#define ORBIS_IME_ERROR_DEVICE_IN_USE 0x80bc000au
#define ORBIS_IME_ERROR_INVALID_USER_ID 0x80bc0010u
#define ORBIS_IME_ERROR_INVALID_TYPE 0x80bc0011u
#define ORBIS_IME_ERROR_INVALID_SUPPORTED_LANGUAGES 0x80bc0012u
#define ORBIS_IME_ERROR_INVALID_ENTER_LABEL 0x80bc0013u
#define ORBIS_IME_ERROR_INVALID_INPUT_METHOD 0x80bc0014u
#define ORBIS_IME_ERROR_INVALID_OPTION 0x80bc0015u
#define ORBIS_IME_ERROR_INVALID_MAX_TEXT_LENGTH 0x80bc0016u
#define ORBIS_IME_ERROR_INVALID_INPUT_TEXT_BUFFER 0x80bc0017u
#define ORBIS_IME_ERROR_INVALID_POSX 0x80bc0018u
#define ORBIS_IME_ERROR_INVALID_POSY 0x80bc0019u
#define ORBIS_IME_ERROR_INVALID_HORIZONTALIGNMENT 0x80bc001au
#define ORBIS_IME_ERROR_INVALID_VERTICALALIGNMENT 0x80bc001bu
#define ORBIS_IME_ERROR_INVALID_EXTENDED 0x80bc001cu
#define ORBIS_IME_ERROR_INVALID_KEYBOARD_TYPE 0x80bc001du
#define ORBIS_IME_ERROR_INVALID_WORK 0x80bc0020u
#define ORBIS_IME_ERROR_INVALID_ARG 0x80bc0021u
#define ORBIS_IME_ERROR_INVALID_HANDLER 0x80bc0022u
#define ORBIS_IME_ERROR_NO_RESOURCE_ID 0x80bc0023u
#define ORBIS_IME_ERROR_INVALID_MODE 0x80bc0024u
#define ORBIS_IME_ERROR_INVALID_PARAM 0x80bc0030u
#define ORBIS_IME_ERROR_INVALID_ADDRESS 0x80bc0031u
#define ORBIS_IME_ERROR_INVALID_RESERVED 0x80bc0032u
#define ORBIS_IME_ERROR_INVALID_TIMING 0x80bc0033u
#define ORBIS_IME_ERROR_INTERNAL 0x80bc00ffu

/* Ime library */
#define ORBIS_IME_ERROR_DIALOG_INVALID_TITLE 0x80bc0101u
#define ORBIS_IME_ERROR_DIALOG_NOT_RUNNING 0x80bc0105u
#define ORBIS_IME_ERROR_DIALOG_NOT_FINISHED 0x80bc0106u
#define ORBIS_IME_ERROR_DIALOG_NOT_IN_USE 0x80bc0107u

typedef u32 OrbisImeOption;
#define ORBIS_IME_OPTION_DEFAULT 0u
#define ORBIS_IME_OPTION_MULTILINE 1u
#define ORBIS_IME_OPTION_NO_AUTO_CAPITALIZATION 2u
#define ORBIS_IME_OPTION_PASSWORD 4u
#define ORBIS_IME_OPTION_LANGUAGES_FORCED 8u
#define ORBIS_IME_OPTION_EXT_KEYBOARD 16u
#define ORBIS_IME_OPTION_NO_LEARNING 32u
#define ORBIS_IME_OPTION_FIXED_POSITION 64u
#define ORBIS_IME_OPTION_DISABLE_COPY_PASTE 128u
#define ORBIS_IME_OPTION_DISABLE_RESUME 256u
#define ORBIS_IME_OPTION_DISABLE_AUTO_SPACE 512u
#define ORBIS_IME_OPTION_DISABLE_POSITION_ADJUSTMENT 2048u
#define ORBIS_IME_OPTION_EXPANDED_PREEDIT_BUFFER 4096u
#define ORBIS_IME_OPTION_USE_JAPANESE_EISUU_KEY_AS_CAPSLOCK 8192u
#define ORBIS_IME_OPTION_USE_OVER_2K_COORDINATES 16384u
#define kValidImeOptionMask ((u32)enum_names_mask(&enum_names_OrbisImeOption))

typedef u32 OrbisImeExtOption;
#define ORBIS_IME_EXT_OPTION_DEFAULT 0x00000000u
#define ORBIS_IME_EXT_OPTION_SET_COLOR 0x00000001u
#define ORBIS_IME_EXT_OPTION_SET_PRIORITY 0x00000002u
#define ORBIS_IME_EXT_OPTION_PRIORITY_SHIFT 0x00000004u
#define ORBIS_IME_EXT_OPTION_PRIORITY_FULL_WIDTH 0x00000008u
#define ORBIS_IME_EXT_OPTION_PRIORITY_FIXED_PANEL 0x00000010u
#define ORBIS_IME_EXT_OPTION_DISABLE_POINTER 0x00000040u
#define ORBIS_IME_EXT_OPTION_ENABLE_ADDITIONAL_DICTIONARY 0x00000080u
#define ORBIS_IME_EXT_OPTION_DISABLE_STARTUP_SE 0x00000100u
#define ORBIS_IME_EXT_OPTION_DISABLE_LIST_FOR_EXT_KEYBOARD 0x00000200u
#define ORBIS_IME_EXT_OPTION_HIDE_KEYPANEL_IF_EXT_KEYBOARD 0x00000400u
#define ORBIS_IME_EXT_OPTION_INIT_EXT_KEYBOARD_MODE 0x00000800u

#define ORBIS_IME_EXT_OPTION_ENABLE_ACCESSIBILITY 0x00001000u                /* ImeDialog unly */
#define ORBIS_IME_EXT_OPTION_ACCESSIBILITY_PANEL_FORCED 0x00002000u          /* ImeDialog only */
#define ORBIS_IME_EXT_OPTION_ADDITIONAL_DICTIONARY_PRIORITY_MODE 0x00004000u /* ImeDialog only */

#define kValidImeExtOptionMask                                                                     \
    (ORBIS_IME_EXT_OPTION_SET_COLOR | ORBIS_IME_EXT_OPTION_SET_PRIORITY |                          \
     ORBIS_IME_EXT_OPTION_PRIORITY_SHIFT | ORBIS_IME_EXT_OPTION_PRIORITY_FULL_WIDTH |              \
     ORBIS_IME_EXT_OPTION_PRIORITY_FIXED_PANEL | ORBIS_IME_EXT_OPTION_DISABLE_POINTER |            \
     ORBIS_IME_EXT_OPTION_ENABLE_ADDITIONAL_DICTIONARY | ORBIS_IME_EXT_OPTION_DISABLE_STARTUP_SE | \
     ORBIS_IME_EXT_OPTION_DISABLE_LIST_FOR_EXT_KEYBOARD |                                          \
     ORBIS_IME_EXT_OPTION_HIDE_KEYPANEL_IF_EXT_KEYBOARD |                                          \
     ORBIS_IME_EXT_OPTION_INIT_EXT_KEYBOARD_MODE)

#define kValidImeDialogExtOptionMask ((u32)enum_names_mask(&enum_names_OrbisImeExtOption))

typedef u64 OrbisImeLanguage;
#define ORBIS_IME_LANGUAGE_DANISH ((u64)0x00000001u)
#define ORBIS_IME_LANGUAGE_GERMAN ((u64)0x00000002u)
#define ORBIS_IME_LANGUAGE_ENGLISH_US ((u64)0x00000004u)
#define ORBIS_IME_LANGUAGE_SPANISH ((u64)0x00000008u)
#define ORBIS_IME_LANGUAGE_FRENCH ((u64)0x00000010u)
#define ORBIS_IME_LANGUAGE_ITALIAN ((u64)0x00000020u)
#define ORBIS_IME_LANGUAGE_DUTCH ((u64)0x00000040u)
#define ORBIS_IME_LANGUAGE_NORWEGIAN ((u64)0x00000080u)
#define ORBIS_IME_LANGUAGE_POLISH ((u64)0x00000100u)
#define ORBIS_IME_LANGUAGE_PORTUGUESE_PT ((u64)0x00000200u)
#define ORBIS_IME_LANGUAGE_RUSSIAN ((u64)0x00000400u)
#define ORBIS_IME_LANGUAGE_FINNISH ((u64)0x00000800u)
#define ORBIS_IME_LANGUAGE_SWEDISH ((u64)0x00001000u)
#define ORBIS_IME_LANGUAGE_JAPANESE ((u64)0x00002000u)
#define ORBIS_IME_LANGUAGE_KOREAN ((u64)0x00004000u)
#define ORBIS_IME_LANGUAGE_SIMPLIFIED_CHINESE ((u64)0x00008000u)
#define ORBIS_IME_LANGUAGE_TRADITIONAL_CHINESE ((u64)0x00010000u)
#define ORBIS_IME_LANGUAGE_PORTUGUESE_BR ((u64)0x00020000u)
#define ORBIS_IME_LANGUAGE_ENGLISH_GB ((u64)0x00040000u)
#define ORBIS_IME_LANGUAGE_TURKISH ((u64)0x00080000u)
#define ORBIS_IME_LANGUAGE_SPANISH_LA ((u64)0x00100000u)
#define ORBIS_IME_LANGUAGE_ARABIC ((u64)0x01000000u)
#define ORBIS_IME_LANGUAGE_FRENCH_CA ((u64)0x02000000u)
#define ORBIS_IME_LANGUAGE_THAI ((u64)0x04000000u)
#define ORBIS_IME_LANGUAGE_CZECH ((u64)0x08000000u)
#define ORBIS_IME_LANGUAGE_GREEK ((u64)0x10000000u)
#define ORBIS_IME_LANGUAGE_INDONESIAN ((u64)0x20000000u)
#define ORBIS_IME_LANGUAGE_VIETNAMESE ((u64)0x40000000u)
#define ORBIS_IME_LANGUAGE_ROMANIAN ((u64)0x80000000u)
#define ORBIS_IME_LANGUAGE_HUNGARIAN (((u64)0x00000001u << 32) | 0x00000000u)

#define kValidOrbisImeLanguageMask ((u64)enum_names_mask(&enum_names_OrbisImeLanguage))

typedef u32 OrbisImeDisableDevice;
#define ORBIS_IME_DISABLE_DEVICE_DEFAULT 0x00000000u
#define ORBIS_IME_DISABLE_DEVICE_CONTROLLER 0x00000001u
#define ORBIS_IME_DISABLE_DEVICE_EXT_KEYBOARD 0x00000002u
#define ORBIS_IME_DISABLE_DEVICE_REMOTE_OSK 0x00000004u
#define kValidOrbisImeDisableDeviceMask ((u32)enum_names_mask(&enum_names_OrbisImeDisableDevice))

typedef u32 OrbisImeInputMethodState;
#define ORBIS_IME_INPUT_METHOD_STATE_PREEDIT 0x01000000u
#define ORBIS_IME_INPUT_METHOD_STATE_SELECTED 0x02000000u
#define ORBIS_IME_INPUT_METHOD_STATE_NATIVE 0x04000000u
#define ORBIS_IME_INPUT_METHOD_STATE_NATIVE2 0x08000000u
#define ORBIS_IME_INPUT_METHOD_STATE_FULL_WIDTH 0x10000000u
#define kValidOrbisImeInputMethodStateMask                                                         \
    ((u32)enum_names_mask(&enum_names_OrbisImeInputMethodState))

typedef u32 OrbisImeInitExtKeyboardMode;
#define ORBIS_IME_INIT_EXT_KEYBOARD_MODE_ISABLE_ARABIC_INDIC_NUMERALS 0x00000001u
#define ORBIS_IME_INIT_EXT_KEYBOARD_MODE_ENABLE_FORMAT_CHARACTERS 0x00000002u
#define ORBIS_IME_INIT_EXT_KEYBOARD_MODE_INPUT_METHOD_STATE_NATIVE 0x04000000u
#define ORBIS_IME_INIT_EXT_KEYBOARD_MODE_INPUT_METHOD_STATE_NATIVE2 0x08000000u
#define ORBIS_IME_INIT_EXT_KEYBOARD_MODE_INPUT_METHOD_STATE_FULL_WIDTH 0x10000000u
#define kValidOrbisImeInitExtKeyboardModeMask                                                      \
    ((u32)enum_names_mask(&enum_names_OrbisImeInitExtKeyboardMode))

typedef u32 OrbisImeKeycodeState;
#define ORBIS_IME_KEYCODE_STATE_KEYCODE_VALID 0x00000001u
#define ORBIS_IME_KEYCODE_STATE_CHARACTER_VALID 0x00000002u
#define ORBIS_IME_KEYCODE_STATE_WITH_IME 0x00000004u
#define ORBIS_IME_KEYCODE_STATE_FROM_OSK 0x00000008u
#define ORBIS_IME_KEYCODE_STATE_FROM_OSK_SHORTCUT 0x00000010u
#define ORBIS_IME_KEYCODE_STATE_FROM_IME_OPERATION 0x00000020u
#define ORBIS_IME_KEYCODE_STATE_REPLACE_CHARACTER 0x00000040u
#define ORBIS_IME_KEYCODE_STATE_CONTINUOUS_EVENT 0x00000080u
#define ORBIS_IME_KEYCODE_STATE_MODIFIER_L_CTRL 0x00000100u
#define ORBIS_IME_KEYCODE_STATE_MODIFIER_L_SHIFT 0x00000200u
#define ORBIS_IME_KEYCODE_STATE_MODIFIER_L_ALT 0x00000400u
#define ORBIS_IME_KEYCODE_STATE_MODIFIER_L_GUI 0x00000800u
#define ORBIS_IME_KEYCODE_STATE_MODIFIER_R_CTRL 0x00001000u
#define ORBIS_IME_KEYCODE_STATE_MODIFIER_R_SHIFT 0x00002000u
#define ORBIS_IME_KEYCODE_STATE_MODIFIER_R_ALT 0x00004000u
#define ORBIS_IME_KEYCODE_STATE_MODIFIER_R_GUI 0x00008000u
#define ORBIS_IME_KEYCODE_STATE_LED_NUM_LOCK 0x00010000u
#define ORBIS_IME_KEYCODE_STATE_LED_CAPS_LOCK 0x00020000u
#define ORBIS_IME_KEYCODE_STATE_LED_SCROLL_LOCK 0x00040000u
#define ORBIS_IME_KEYCODE_STATE_RESERVED1 0x00080000u
#define ORBIS_IME_KEYCODE_STATE_RESERVED2 0x00100000u
#define ORBIS_IME_KEYCODE_STATE_FROM_IME_INPUT 0x00200000u
#define kValidOrbisImeKeycodeStateMask ((u32)enum_names_mask(&enum_names_OrbisImeKeycodeState))

typedef u32 OrbisImeKeyboardOption;
#define ORBIS_IME_KEYBOARD_OPTION_DEFAULT 0u
#define ORBIS_IME_KEYBOARD_OPTION_REPEAT 1u
#define ORBIS_IME_KEYBOARD_OPTION_REPEAT_EACH_KEY 2u
#define ORBIS_IME_KEYBOARD_OPTION_ADD_OSK 4u
#define ORBIS_IME_KEYBOARD_OPTION_EFFECTIVE_WITH_IME 8u
#define ORBIS_IME_KEYBOARD_OPTION_DISABLE_RESUME 16u
#define ORBIS_IME_KEYBOARD_OPTION_DISABLE_CAPSLOCK_WITHOUT_SHIFT 32u
#define kValidOrbisImeKeyboardOptionMask ((u32)enum_names_mask(&enum_names_OrbisImeKeyboardOption))

typedef u32 OrbisImeKeyboardMode;
#define ORBIS_IME_KEYBOARD_MODE_AUTO 0u
#define ORBIS_IME_KEYBOARD_MODE_MANUAL 1u
#define ORBIS_IME_KEYBOARD_MODE_ALPHABET 0u
#define ORBIS_IME_KEYBOARD_MODE_NATIVE 2u
#define ORBIS_IME_KEYBOARD_MODE_PART 4u
#define ORBIS_IME_KEYBOARD_MODE_KATAKANA 8u
#define ORBIS_IME_KEYBOARD_MODE_HKANA 16u
#define ORBIS_IME_KEYBOARD_MODE_ARABIC_INDIC_NUMERALS 32u
#define ORBIS_IME_KEYBOARD_MODE_DISABLE_FORMAT_CHARACTERS 64u

typedef u32 OrbisImeType;
#define ORBIS_IME_TYPE_DEFAULT 0u
#define ORBIS_IME_TYPE_BASIC_LATIN 1u
#define ORBIS_IME_TYPE_URL 2u
#define ORBIS_IME_TYPE_MAIL 3u
#define ORBIS_IME_TYPE_NUMBER 4u

typedef u32 OrbisImeHorizontalAlignment;
#define ORBIS_IME_HORIZONTAL_ALIGNMENT_LEFT 0u
#define ORBIS_IME_HORIZONTAL_ALIGNMENT_CENTER 1u
#define ORBIS_IME_HORIZONTAL_ALIGNMENT_RIGHT 2u

typedef u32 OrbisImeVerticalAlignment;
#define ORBIS_IME_VERTICAL_ALIGNMENT_TOP 0u
#define ORBIS_IME_VERTICAL_ALIGNMENT_CENTER 1u
#define ORBIS_IME_VERTICAL_ALIGNMENT_BOTTOM 2u

typedef u32 OrbisImeEnterLabel;
#define ORBIS_IME_ENTER_LABEL_DEFAULT 0u
#define ORBIS_IME_ENTER_LABEL_SEND 1u
#define ORBIS_IME_ENTER_LABEL_SEARCH 2u
#define ORBIS_IME_ENTER_LABEL_GO 3u

typedef u32 OrbisImeInputMethod;
#define ORBIS_IME_INPUT_METHOD_DEFAULT 0u

typedef u32 OrbisImeEventId;
#define ORBIS_IME_EVENT_ID_OPEN 0u
#define ORBIS_IME_EVENT_ID_UPDATE_TEXT 1u
#define ORBIS_IME_EVENT_ID_UPDATE_CARET 2u
#define ORBIS_IME_EVENT_ID_CHANGE_SIZE 3u
#define ORBIS_IME_EVENT_ID_PRESS_CLOSE 4u
#define ORBIS_IME_EVENT_ID_PRESS_ENTER 5u
#define ORBIS_IME_EVENT_ID_ABORT 6u
#define ORBIS_IME_EVENT_ID_CANDIDATE_LIST_START 7u
#define ORBIS_IME_EVENT_ID_CANDIDATE_LIST_END 8u
#define ORBIS_IME_EVENT_ID_CANDIDATE_WORD 9u
#define ORBIS_IME_EVENT_ID_CANDIDATE_INDEX 10u
#define ORBIS_IME_EVENT_ID_CANDIDATE_DONE 11u
#define ORBIS_IME_EVENT_ID_CANDIDATE_CANCEL 12u
#define ORBIS_IME_EVENT_ID_CHANGE_DEVICE 14u
#define ORBIS_IME_EVENT_ID_JUMP_TO_NEXT_OBJECT 15u
#define ORBIS_IME_EVENT_ID_JUMP_TO_BEFORE_OBJECT 16u
#define ORBIS_IME_EVENT_ID_CHANGE_WINDOW_TYPE 17u

#define ORBIS_IME_EVENT_ID_CHANGE_INPUT_METHOD_STATE 18u

#define ORBIS_IME_EVENT_ID_KEYBOARD_OPEN 256u
#define ORBIS_IME_EVENT_ID_KEYBOARD_KEYCODE_DOWN 257u
#define ORBIS_IME_EVENT_ID_KEYBOARD_KEYCODE_UP 258u
#define ORBIS_IME_EVENT_ID_KEYBOARD_KEYCODE_REPEAT 259u
#define ORBIS_IME_EVENT_ID_KEYBOARD_CONNECTION 260u
#define ORBIS_IME_EVENT_ID_KEYBOARD_DISCONNECTION 261u
#define ORBIS_IME_EVENT_ID_KEYBOARD_ABORT 262u

typedef u32 OrbisImeKeyboardType;
#define ORBIS_IME_KEYBOARD_TYPE_NONE 0u
#define ORBIS_IME_KEYBOARD_TYPE_DANISH 1u
#define ORBIS_IME_KEYBOARD_TYPE_GERMAN 2u
#define ORBIS_IME_KEYBOARD_TYPE_GERMAN_SW 3u
#define ORBIS_IME_KEYBOARD_TYPE_ENGLISH_US 4u
#define ORBIS_IME_KEYBOARD_TYPE_ENGLISH_GB 5u
#define ORBIS_IME_KEYBOARD_TYPE_SPANISH 6u
#define ORBIS_IME_KEYBOARD_TYPE_SPANISH_LA 7u
#define ORBIS_IME_KEYBOARD_TYPE_FINNISH 8u
#define ORBIS_IME_KEYBOARD_TYPE_FRENCH 9u
#define ORBIS_IME_KEYBOARD_TYPE_FRENCH_BR 10u
#define ORBIS_IME_KEYBOARD_TYPE_FRENCH_CA 11u
#define ORBIS_IME_KEYBOARD_TYPE_FRENCH_SW 12u
#define ORBIS_IME_KEYBOARD_TYPE_ITALIAN 13u
#define ORBIS_IME_KEYBOARD_TYPE_DUTCH 14u
#define ORBIS_IME_KEYBOARD_TYPE_NORWEGIAN 15u
#define ORBIS_IME_KEYBOARD_TYPE_POLISH 16u
#define ORBIS_IME_KEYBOARD_TYPE_PORTUGUESE_BR 17u
#define ORBIS_IME_KEYBOARD_TYPE_PORTUGUESE_PT 18u
#define ORBIS_IME_KEYBOARD_TYPE_RUSSIAN 19u
#define ORBIS_IME_KEYBOARD_TYPE_SWEDISH 20u
#define ORBIS_IME_KEYBOARD_TYPE_TURKISH 21u
#define ORBIS_IME_KEYBOARD_TYPE_JAPANESE_ROMAN 22u
#define ORBIS_IME_KEYBOARD_TYPE_JAPANESE_KANA 23u
#define ORBIS_IME_KEYBOARD_TYPE_KOREAN 24u
#define ORBIS_IME_KEYBOARD_TYPE_SM_CHINESE 25u
#define ORBIS_IME_KEYBOARD_TYPE_TR_CHINESE_ZY 26u
#define ORBIS_IME_KEYBOARD_TYPE_TR_CHINESE_PY_HK 27u
#define ORBIS_IME_KEYBOARD_TYPE_TR_CHINESE_PY_TW 28u
#define ORBIS_IME_KEYBOARD_TYPE_TR_CHINESE_CG 29u
#define ORBIS_IME_KEYBOARD_TYPE_ARABIC_AR 30u
#define ORBIS_IME_KEYBOARD_TYPE_THAI 31u
#define ORBIS_IME_KEYBOARD_TYPE_CZECH 32u
#define ORBIS_IME_KEYBOARD_TYPE_GREEK 33u
#define ORBIS_IME_KEYBOARD_TYPE_INDONESIAN 34u
#define ORBIS_IME_KEYBOARD_TYPE_VIETNAMESE 35u
#define ORBIS_IME_KEYBOARD_TYPE_ROMANIAN 36u
#define ORBIS_IME_KEYBOARD_TYPE_HUNGARIAN 37u

typedef u32 OrbisImeKeyboardDeviceType;
#define ORBIS_IME_KEYBOARD_DEVICE_TYPE_KEYBOARD 0u
#define ORBIS_IME_KEYBOARD_DEVICE_TYPE_OSK 1u

typedef u32 OrbisImeKeyboardStatus;
#define ORBIS_IME_KEYBOARD_STATUS_DISCONNECTED 0u
#define ORBIS_IME_KEYBOARD_STATUS_CONNECTED 1u

typedef u32 OrbisImeDeviceType;
#define ORBIS_IME_DEVICE_TYPE_NONE 0u
#define ORBIS_IME_DEVICE_TYPE_CONTROLLER 1u
#define ORBIS_IME_DEVICE_TYPE_EXT_KEYBOARD 2u
#define ORBIS_IME_DEVICE_TYPE_REMOTE_OSK 3u

typedef u32 OrbisImePanelPriority;
#define ORBIS_IME_PANEL_PRIORITY_DEFAULT 0u
#define ORBIS_IME_PANEL_PRIORITY_ALPHABET 1u
#define ORBIS_IME_PANEL_PRIORITY_SYMBOL 2u
#define ORBIS_IME_PANEL_PRIORITY_ACCENT 3u

typedef struct OrbisImeRect {
    f32 x;
    f32 y;
    u32 width;
    u32 height;
} OrbisImeRect;

typedef struct OrbisImeColor {
    u8 r;
    u8 g;
    u8 b;
    u8 a;
} OrbisImeColor;

typedef u32 OrbisImeTextAreaMode;
#define ORBIS_IME_TEXT_AREA_MODE_DISABLE 0u
#define ORBIS_IME_TEXT_AREA_MODE_EDIT 1u
#define ORBIS_IME_TEXT_AREA_MODE_PREEDIT 2u
#define ORBIS_IME_TEXT_AREA_MODE_SELECT 3u

typedef struct OrbisImeTextAreaProperty {
    OrbisImeTextAreaMode mode;
    u32 index;
    s32 length;
} OrbisImeTextAreaProperty;

typedef struct OrbisImeEditText {
    char16_t* str;
    u32 caret_index;
    u32 area_num;
    OrbisImeTextAreaProperty text_area[4];
} OrbisImeEditText;

typedef struct OrbisImeKeycode {
    u16 keycode;
    char16_t character;
    u32 status;
    OrbisImeKeyboardType type;
    OrbisUserServiceUserId user_id;
    u32 resource_id;
    OrbisRtcTick timestamp;
} OrbisImeKeycode;

typedef struct OrbisImeKeyboardResourceIdArray {
    OrbisUserServiceUserId user_id;
    u32 resource_id[5];
} OrbisImeKeyboardResourceIdArray;

typedef struct OrbisImeKeyboardInfo {
    OrbisUserServiceUserId user_id;
    OrbisImeKeyboardDeviceType device;
    OrbisImeKeyboardType type;
    u32 repeat_delay;
    u32 repeat_rate;
    OrbisImeKeyboardStatus status;
    s8 reserved[12];
} OrbisImeKeyboardInfo;

typedef u32 OrbisImeCaretMovementDirection;
#define ORBIS_IME_CARET_MOVEMENT_DIRECTION_STILL 0u
#define ORBIS_IME_CARET_MOVEMENT_DIRECTION_LEFT 1u
#define ORBIS_IME_CARET_MOVEMENT_DIRECTION_RIGHT 2u
#define ORBIS_IME_CARET_MOVEMENT_DIRECTION_UP 3u
#define ORBIS_IME_CARET_MOVEMENT_DIRECTION_DOWN 4u
#define ORBIS_IME_CARET_MOVEMENT_DIRECTION_HOME 5u
#define ORBIS_IME_CARET_MOVEMENT_DIRECTION_END 6u
#define ORBIS_IME_CARET_MOVEMENT_DIRECTION_PAGE_UP 7u
#define ORBIS_IME_CARET_MOVEMENT_DIRECTION_PAGE_DOWN 8u
#define ORBIS_IME_CARET_MOVEMENT_DIRECTION_TOP 9u
#define ORBIS_IME_CARET_MOVEMENT_DIRECTION_BOTTOM 10u

typedef u32 OrbisImePanelType;
#define ORBIS_IME_PANEL_TYPE_HIDE 0u
#define ORBIS_IME_PANEL_TYPE_OSK 1u
#define ORBIS_IME_PANEL_TYPE_DIALOG 2u
#define ORBIS_IME_PANEL_TYPE_CANDIDATE 3u
#define ORBIS_IME_PANEL_TYPE_EDIT 4u
#define ORBIS_IME_PANEL_TYPE_EDIT_AND_CANDIDATE 5u
#define ORBIS_IME_PANEL_TYPE_ACCESSIBILITY 6u

typedef struct OrbisImePositionAndForm {
    OrbisImePanelType type;
    f32 posx;
    f32 posy;
    OrbisImeHorizontalAlignment horizontal_alignment;
    OrbisImeVerticalAlignment vertical_alignment;
    u32 width;
    u32 height;
} OrbisImePositionAndForm;

typedef struct OrbisImeTextGeometry {
    f32 x;
    f32 y;
    u32 width;
    u32 height;
} OrbisImeTextGeometry;

typedef union OrbisImeEventParam {
    OrbisImeRect rect;
    OrbisImeEditText text;
    OrbisImeCaretMovementDirection caret_move;
    OrbisImeKeycode keycode;
    OrbisImeKeyboardResourceIdArray resource_id_array;
    char16_t* candidate_word;
    s32 candidate_index;
    OrbisImeDeviceType device_type;
    OrbisImePanelType panel_type;
    u32 input_method_state;
    s8 reserved[64];
} OrbisImeEventParam;

typedef struct OrbisImeEvent {
    OrbisImeEventId id;
    OrbisImeEventParam param;
} OrbisImeEvent;

typedef int(PS4_SYSV_ABI* OrbisImeExtKeyboardFilter)(const OrbisImeKeycode* srcKeycode,
                                                     u16* outKeycode, u32* outStatus,
                                                     void* reserved);

typedef int(PS4_SYSV_ABI* OrbisImeTextFilter)(char16_t* outText, u32* outTextLength,
                                              const char16_t* srcText, u32 srcTextLength);

typedef void(PS4_SYSV_ABI* OrbisImeEventHandler)(void* arg, const OrbisImeEvent* e);

typedef struct OrbisImeKeyboardParam {
    OrbisImeKeyboardOption option;
    s8 reserved1[4];
    void* arg;
    OrbisImeEventHandler handler;
    s8 reserved2[8];
} OrbisImeKeyboardParam;

typedef struct OrbisImeParam {
    OrbisUserServiceUserId user_id;
    OrbisImeType type;
    OrbisImeLanguage supported_languages;
    OrbisImeEnterLabel enter_label;
    OrbisImeInputMethod input_method;
    OrbisImeTextFilter filter;
    OrbisImeOption option;
    u32 maxTextLength;
    char16_t* inputTextBuffer;
    f32 posx;
    f32 posy;
    OrbisImeHorizontalAlignment horizontal_alignment;
    OrbisImeVerticalAlignment vertical_alignment;
    void* work;
    void* arg;
    OrbisImeEventHandler handler;
    s8 reserved[8];
} OrbisImeParam;

typedef struct OrbisImeCaret {
    f32 x;
    f32 y;
    u32 height;
    u32 index;
} OrbisImeCaret;

typedef struct OrbisImeDialogParam {
    OrbisUserServiceUserId user_id;
    OrbisImeType type;
    OrbisImeLanguage supported_languages;
    OrbisImeEnterLabel enter_label;
    OrbisImeInputMethod input_method;
    OrbisImeTextFilter filter;
    OrbisImeOption option;
    u32 max_text_length;
    char16_t* input_text_buffer;
    f32 posx;
    f32 posy;
    OrbisImeHorizontalAlignment horizontal_alignment;
    OrbisImeVerticalAlignment vertical_alignment;
    const char16_t* placeholder;
    const char16_t* title;
    s8 reserved[16];
} OrbisImeDialogParam;

typedef struct OrbisImeParamExtended {
    OrbisImeExtOption option;
    OrbisImeColor color_base;
    OrbisImeColor color_line;
    OrbisImeColor color_text_field;
    OrbisImeColor color_preedit;
    OrbisImeColor color_button_default;
    OrbisImeColor color_button_function;
    OrbisImeColor color_button_symbol;
    OrbisImeColor color_text;
    OrbisImeColor color_special;
    OrbisImePanelPriority priority;
    char* additional_dictionary_path;
    OrbisImeExtKeyboardFilter ext_keyboard_filter;
    OrbisImeDisableDevice disable_device;
    u32 ext_keyboard_mode;
    s8 reserved[60];
} OrbisImeParamExtended;

#endif
