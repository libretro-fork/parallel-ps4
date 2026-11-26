/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <limits.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "core/libraries/ime/ime_kb_layout.h"

#define KB_MIN(a, b) ((a) < (b) ? (a) : (b))
#define KB_MAX(a, b) ((a) > (b) ? (a) : (b))

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

#define KEY(r, c, label, action)                                                                   \
    {r, c, 1, 1, label, NULL, IME_KB_KEY_ACTION_##action, IME_KB_KEY_GLYPH_NONE}
#define KEYHOT(r, c, label, hotkey, action)                                                        \
    {r, c, 1, 1, label, hotkey, IME_KB_KEY_ACTION_##action, IME_KB_KEY_GLYPH_NONE}
#define KEYSPAN(r, c, cs, rs, label, action)                                                       \
    {r, c, cs, rs, label, NULL, IME_KB_KEY_ACTION_##action, IME_KB_KEY_GLYPH_NONE}
#define KEYSPANHOT(r, c, cs, rs, label, hotkey, action)                                            \
    {r, c, cs, rs, label, hotkey, IME_KB_KEY_ACTION_##action, IME_KB_KEY_GLYPH_NONE}
#define KEYGLYPH(r, c, glyph, action)                                                              \
    {r, c, 1, 1, NULL, NULL, IME_KB_KEY_ACTION_##action, IME_KB_KEY_GLYPH_##glyph}
#define KEYHOTGLYPH(r, c, hotkey, glyph, action)                                                   \
    {r, c, 1, 1, NULL, hotkey, IME_KB_KEY_ACTION_##action, IME_KB_KEY_GLYPH_##glyph}
#define KEYSPANGLYPH(r, c, cs, rs, glyph, action)                                                  \
    {r, c, cs, rs, NULL, NULL, IME_KB_KEY_ACTION_##action, IME_KB_KEY_GLYPH_##glyph}
#define KEYSPANHOTGLYPH(r, c, cs, rs, hotkey, glyph, action)                                       \
    {r, c, cs, rs, NULL, hotkey, IME_KB_KEY_ACTION_##action, IME_KB_KEY_GLYPH_##glyph}

#define kPanelBaseW 793.0f
#define kPanelBaseHSingle 528.0f
#define kPanelBaseHMulti 628.0f
#define kLabelH 57.0f
#define kInputHSingle 50.0f
#define kInputHMulti 151.0f
#define kPredictH 53.0f
#define kPredictW 740.0f
#define kCloseW 53.0f
#define kKeysH 316.0f
#define kKeyGap 9.0f
#define kPadX 26.0f
#define kPadBottomSingle 26.0f
#define kPadBottomMulti 26.0f
#define kKeyFontRatio 0.042f
#define kTypingKeyLabelScale 1.60f
#define kFunctionKeyLabelScale 1.26f
#define kHotkeyLabelScale 0.74f
#define kCornerRatio 0.004f
#define kSingleLineTextFill 0.85f
#define kMultiLineTextFill 0.85f
#define kMultiLineVisibleLines 4
#define kKeyRows 6
#define kSpecialsKeyRows 7
#define kKeyCols 10
static const char kCurrencyDollar[] = "$";
static const char kCurrencyEuro[] = "\xE2\x82\xAC";
static const char kCurrencyPound[] = "\xC2\xA3";
static const char kCurrencyYen[] = "\xC2\xA5";
static const char kCurrencyWon[] = "\xE2\x82\xA9";
static const char kHotkeyTriangleRounded[] = "\xE2\x96\xB3";
static const char kHotkeySquareRounded[] = "\xE2\x96\xA2";
static const char kHotkeyL2TriangleRounded[] = "L2+\xE2\x96\xB3";
static const char kArrowLabelUp[] = "\xE2\x96\xB2";
static const char kArrowLabelRight[] = "\xE2\x96\xB6";
static const char kArrowLabelDown[] = "\xE2\x96\xBC";
static const char kArrowLabelLeft[] = "\xE2\x97\x80";
static const char kLanguageSwitchLabel[] = "\xE2\x8C\xA8";
static const char kControlModeGyroLabel[] = "\xE2\x98\x89/\xE2\x8C\x96";
/* SCE_IME keycodes follow USB HID usage IDs. */
#define kCodeSym1 0x003A       /* SCE_IME_KEYCODE_F1 */
#define kCodeAccents 0x003B    /* SCE_IME_KEYCODE_F2 */
#define kCodeLetters 0x003C    /* SCE_IME_KEYCODE_F3 */
#define kCodeKeyboard 0x003D   /* SCE_IME_KEYCODE_F4 */
#define kCodeSettings 0x003E   /* SCE_IME_KEYCODE_F5 */
#define kCodeOptions 0x0076    /* SCE_IME_KEYCODE_MENU */
#define kCodeArrowUp 0x0052    /* SCE_IME_KEYCODE_UPARROW */
#define kCodeArrowDown 0x0051  /* SCE_IME_KEYCODE_DOWNARROW */
#define kCodeArrowLeft 0x0050  /* SCE_IME_KEYCODE_LEFTARROW */
#define kCodeArrowRight 0x004F /* SCE_IME_KEYCODE_RIGHTARROW */
#define kCodeSpace 0x002C      /* SCE_IME_KEYCODE_SPACEBAR */
#define kCodeBackspace 0x002A  /* SCE_IME_KEYCODE_BACKSPACE */
#define kCodeReturn 0x0028     /* SCE_IME_KEYCODE_RETURN */
#define kCodeLeftShift 0x00E1  /* SCE_IME_KEYCODE_LEFTSHIFT */

#define kMaskDollarPreferred                                                                       \
    (ORBIS_IME_LANGUAGE_ENGLISH_US | ORBIS_IME_LANGUAGE_SPANISH_LA |                               \
     ORBIS_IME_LANGUAGE_PORTUGUESE_BR)
#define kMaskPoundPreferred ORBIS_IME_LANGUAGE_ENGLISH_GB
#define kMaskYenPreferred ORBIS_IME_LANGUAGE_JAPANESE
#define kMaskWonPreferred ORBIS_IME_LANGUAGE_KOREAN

/* The currency a set of languages has first among the symbols */
#define CURRENCY_EURO 0
#define CURRENCY_DOLLAR 1
#define CURRENCY_POUND 2
#define CURRENCY_YEN 3
#define CURRENCY_WON 4

static u16 Utf8FirstCodeUnit(const char* text) {
    unsigned char c0;
    if (!text || text[0] == '\0') {
        return 0;
    }
    c0 = (unsigned char)text[0];
    if ((c0 & 0x80u) == 0) {
        return (u16)c0;
    }
    if ((c0 & 0xE0u) == 0xC0u && text[1] != '\0') {
        const unsigned char c1 = (unsigned char)text[1];
        if ((c1 & 0xC0u) == 0x80u) {
            return (u16)(((c0 & 0x1Fu) << 6) | (c1 & 0x3Fu));
        }
        return 0;
    }
    if ((c0 & 0xF0u) == 0xE0u && text[1] != '\0' && text[2] != '\0') {
        const unsigned char c1 = (unsigned char)text[1];
        const unsigned char c2 = (unsigned char)text[2];
        if ((c1 & 0xC0u) == 0x80u && (c2 & 0xC0u) == 0x80u) {
            return (u16)(((c0 & 0x0Fu) << 12) | ((c1 & 0x3Fu) << 6) | (c2 & 0x3Fu));
        }
        return 0;
    }
    return 0;
}

static u16 ResolveAsciiHidKeycode(u16 code_unit) {
    if (code_unit >= 'a' && code_unit <= 'z') {
        return (u16)(0x0004 + (code_unit - 'a')); /* SCE_IME_KEYCODE_A... */
    }
    if (code_unit >= 'A' && code_unit <= 'Z') {
        return (u16)(0x0004 + (code_unit - 'A')); /* SCE_IME_KEYCODE_A... */
    }
    if (code_unit >= '1' && code_unit <= '9') {
        return (u16)(0x001E + (code_unit - '1')); /* SCE_IME_KEYCODE_1... */
    }
    if (code_unit == '0') {
        return 0x0027; /* SCE_IME_KEYCODE_0 */
    }

    switch (code_unit) {
    case ' ':
        return kCodeSpace;
    case '\b':
        return kCodeBackspace;
    case '\r':
    case '\n':
        return kCodeReturn;
    case '-':
    case '_':
        return 0x002D; /* SCE_IME_KEYCODE_MINUS */
    case '=':
    case '+':
        return 0x002E; /* SCE_IME_KEYCODE_EQUAL */
    case '[':
    case '{':
        return 0x002F; /* SCE_IME_KEYCODE_LEFTBRACKET */
    case ']':
    case '}':
        return 0x0030; /* SCE_IME_KEYCODE_RIGHTBRACKET */
    case '\\':
    case '|':
        return 0x0031; /* SCE_IME_KEYCODE_BACKSLASH */
    case ';':
    case ':':
        return 0x0033; /* SCE_IME_KEYCODE_SEMICOLON */
    case '\'':
    case '"':
        return 0x0034; /* SCE_IME_KEYCODE_SINGLEQUOTE */
    case '`':
    case '~':
        return 0x0035; /* SCE_IME_KEYCODE_BACKQUOTE */
    case ',':
    case '<':
        return 0x0036; /* SCE_IME_KEYCODE_COMMA */
    case '.':
    case '>':
        return 0x0037; /* SCE_IME_KEYCODE_PERIOD */
    case '/':
    case '?':
        return 0x0038; /* SCE_IME_KEYCODE_SLASH */
    case '!':
        return 0x001E; /* Shift+1 */
    case '@':
        return 0x001F; /* Shift+2 */
    case '#':
        return 0x0020; /* Shift+3 */
    case '$':
        return 0x0021; /* Shift+4 */
    case '%':
        return 0x0022; /* Shift+5 */
    case '^':
        return 0x0023; /* Shift+6 */
    case '&':
        return 0x0024; /* Shift+7 */
    case '*':
        return 0x0025; /* Shift+8 */
    case '(':
        return 0x0026; /* Shift+9 */
    case ')':
        return 0x0027; /* Shift+0 */
    default:
        return 0;
    }
}

static u16 ResolveImeKeycode(const ImeKbKeySpec* key, const char* label_override) {
    switch (key->action) {
    case IME_KB_KEY_ACTION_CHARACTER: {
        const u16 code_unit = Utf8FirstCodeUnit(label_override ? label_override : key->label);
        return ResolveAsciiHidKeycode(code_unit);
    }
    case IME_KB_KEY_ACTION_SPACE:
        return kCodeSpace;
    case IME_KB_KEY_ACTION_BACKSPACE:
        return kCodeBackspace;
    case IME_KB_KEY_ACTION_NEW_LINE:
    case IME_KB_KEY_ACTION_DONE:
        return kCodeReturn;
    case IME_KB_KEY_ACTION_ARROW_UP:
        return kCodeArrowUp;
    case IME_KB_KEY_ACTION_ARROW_DOWN:
        return kCodeArrowDown;
    case IME_KB_KEY_ACTION_ARROW_LEFT:
    case IME_KB_KEY_ACTION_PAGE_PREV:
        return kCodeArrowLeft;
    case IME_KB_KEY_ACTION_ARROW_RIGHT:
    case IME_KB_KEY_ACTION_PAGE_NEXT:
        return kCodeArrowRight;
    case IME_KB_KEY_ACTION_SYMBOLS_MODE: {
        const char* mode_label = label_override ? label_override : key->label;
        if (mode_label && strcmp(mode_label, "ABC") == 0) {
            return kCodeLetters;
        }
        return kCodeSym1;
    }
    case IME_KB_KEY_ACTION_SPECIALS_MODE: {
        const char* mode_label = label_override ? label_override : key->label;
        if (mode_label && strcmp(mode_label, "ABC") == 0) {
            return kCodeLetters;
        }
        return kCodeAccents;
    }
    case IME_KB_KEY_ACTION_KEYBOARD:
        return kCodeKeyboard;
    case IME_KB_KEY_ACTION_MENU:
        return kCodeOptions;
    case IME_KB_KEY_ACTION_SETTINGS:
        return kCodeSettings;
    case IME_KB_KEY_ACTION_SHIFT:
        return kCodeLeftShift;
    case IME_KB_KEY_ACTION_NONE:
    default:
        return 0;
    }
}

static char16_t ResolveImeCharacter(const ImeKbKeySpec* key, const char* label_override) {
    switch (key->action) {
    case IME_KB_KEY_ACTION_CHARACTER:
        return (char16_t)Utf8FirstCodeUnit(label_override ? label_override : key->label);
    case IME_KB_KEY_ACTION_SPACE:
        return ' ';
    case IME_KB_KEY_ACTION_NEW_LINE:
    case IME_KB_KEY_ACTION_DONE:
        return '\r';
    default:
        return 0;
    }
}

static int ResolveCurrencyProfile(OrbisImeLanguage supported_languages) {
    const u64 mask = supported_languages;
    if ((mask & kMaskWonPreferred) != 0) {
        return CURRENCY_WON;
    }
    if ((mask & kMaskYenPreferred) != 0) {
        return CURRENCY_YEN;
    }
    if ((mask & kMaskPoundPreferred) != 0) {
        return CURRENCY_POUND;
    }
    if ((mask & kMaskDollarPreferred) != 0) {
        return CURRENCY_DOLLAR;
    }
    return CURRENCY_EURO;
}

/* The currencies in the order a profile has them, by CURRENCY_* */
static const char* const kCurrencyLabels[5][4] = {
    {kCurrencyEuro, kCurrencyPound, kCurrencyYen, kCurrencyWon},
    {kCurrencyDollar, kCurrencyEuro, kCurrencyPound, kCurrencyYen},
    {kCurrencyPound, kCurrencyEuro, kCurrencyDollar, kCurrencyYen},
    {kCurrencyYen, kCurrencyEuro, kCurrencyDollar, kCurrencyPound},
    {kCurrencyWon, kCurrencyEuro, kCurrencyDollar, kCurrencyYen},
};

static const char* ResolveSymbolOverrideLabel(const ImeKbLayoutSelection* selection,
                                              OrbisImeLanguage supported_languages,
                                              const ImeKbKeySpec* key, const char* fallback_label) {
    if (selection->family != IME_KB_LAYOUT_FAMILY_SYMBOLS ||
        key->action != IME_KB_KEY_ACTION_CHARACTER || key->row != 3) {
        return fallback_label;
    }
    /* Keep one locale-aware currency slot on symbols page 1 and preserve the rest of the page
     * for PS4 punctuation/quote marks. */
    if ((selection->page % 2) == 0 && key->col == 5) {
        return kCurrencyLabels[ResolveCurrencyProfile(supported_languages)][0];
    }
    return fallback_label;
}

static const char* ResolveShiftOverrideLabel(const ImeKbLayoutSelection* selection,
                                             const ImeKbKeySpec* key, const char* fallback_label) {
    if (key->action != IME_KB_KEY_ACTION_SHIFT || !fallback_label || fallback_label[0] == '\0') {
        return fallback_label;
    }
    switch (selection->case_state) {
    case IME_KB_CASE_STATE_UPPER:
    case IME_KB_CASE_STATE_CAPS_LOCK:
        return "SHIFT";
    case IME_KB_CASE_STATE_LOWER:
    default:
        return "shift";
    }
}

static ImeKbKeyGlyph ResolveShiftOverrideGlyph(const ImeKbLayoutSelection* selection,
                                               const ImeKbKeySpec* key,
                                               ImeKbKeyGlyph fallback_glyph) {
    if (key->action != IME_KB_KEY_ACTION_SHIFT) {
        return fallback_glyph;
    }
    switch (selection->case_state) {
    case IME_KB_CASE_STATE_UPPER:
        return IME_KB_KEY_GLYPH_SHIFT_FILLED;
    case IME_KB_CASE_STATE_CAPS_LOCK:
        return IME_KB_KEY_GLYPH_CAPS_LOCK_FILLED;
    case IME_KB_CASE_STATE_LOWER:
    default:
        return IME_KB_KEY_GLYPH_SHIFT_OUTLINE;
    }
}

static const char* GetEnterLabel(OrbisImeEnterLabel label) {
    switch (label) {
    case ORBIS_IME_ENTER_LABEL_GO:
        return "Go";
    case ORBIS_IME_ENTER_LABEL_SEARCH:
        return "Search";
    case ORBIS_IME_ENTER_LABEL_SEND:
        return "Send";
    case ORBIS_IME_ENTER_LABEL_DEFAULT:
    default:
        return "Done";
    }
}

static const ImeKbKeySpec kLatinLowerKeys[55] = {
    KEY(0, 0, "1", CHARACTER),
    KEY(0, 1, "2", CHARACTER),
    KEY(0, 2, "3", CHARACTER),
    KEY(0, 3, "4", CHARACTER),
    KEY(0, 4, "5", CHARACTER),
    KEY(0, 5, "6", CHARACTER),
    KEY(0, 6, "7", CHARACTER),
    KEY(0, 7, "8", CHARACTER),
    KEY(0, 8, "9", CHARACTER),
    KEY(0, 9, "0", CHARACTER),

    KEY(1, 0, "q", CHARACTER),
    KEY(1, 1, "w", CHARACTER),
    KEY(1, 2, "e", CHARACTER),
    KEY(1, 3, "r", CHARACTER),
    KEY(1, 4, "t", CHARACTER),
    KEY(1, 5, "y", CHARACTER),
    KEY(1, 6, "u", CHARACTER),
    KEY(1, 7, "i", CHARACTER),
    KEY(1, 8, "o", CHARACTER),
    KEY(1, 9, "p", CHARACTER),

    KEY(2, 0, "a", CHARACTER),
    KEY(2, 1, "s", CHARACTER),
    KEY(2, 2, "d", CHARACTER),
    KEY(2, 3, "f", CHARACTER),
    KEY(2, 4, "g", CHARACTER),
    KEY(2, 5, "h", CHARACTER),
    KEY(2, 6, "j", CHARACTER),
    KEY(2, 7, "k", CHARACTER),
    KEY(2, 8, "l", CHARACTER),
    KEY(2, 9, "'", CHARACTER),

    KEY(3, 0, "z", CHARACTER),
    KEY(3, 1, "x", CHARACTER),
    KEY(3, 2, "c", CHARACTER),
    KEY(3, 3, "v", CHARACTER),
    KEY(3, 4, "b", CHARACTER),
    KEY(3, 5, "n", CHARACTER),
    KEY(3, 6, "m", CHARACTER),
    KEY(3, 7, ",", CHARACTER),
    KEY(3, 8, ".", CHARACTER),
    KEY(3, 9, "?", CHARACTER),

    KEYHOTGLYPH(4, 0, "L2", SHIFT_OUTLINE, SHIFT),
    KEYHOT(4, 1, "@#:", kHotkeyL2TriangleRounded, SYMBOLS_MODE),
    KEYHOT(4, 2, "\xC3\xA0", "L3", SPECIALS_MODE),
    KEYSPANHOT(4, 3, 4, 1, "Space", kHotkeyTriangleRounded, SPACE),
    KEY(4, 7, NULL, NONE),
    KEYSPANHOTGLYPH(4, 8, 2, 1, kHotkeySquareRounded, BACKSPACE, BACKSPACE),

    KEYGLYPH(5, 0, ARROW_DOWN, ARROW_DOWN),
    KEYGLYPH(5, 1, ARROW_UP, ARROW_UP),
    KEYHOTGLYPH(5, 2, "L1", ARROW_LEFT, ARROW_LEFT),
    KEYHOTGLYPH(5, 3, "R1", ARROW_RIGHT, ARROW_RIGHT),
    KEY(5, 4, kLanguageSwitchLabel, KEYBOARD),
    KEY(5, 5, "...", MENU),
    KEYHOT(5, 6, kControlModeGyroLabel, "R3", SETTINGS),
    KEY(5, 7, NULL, NONE),
    KEYSPANHOT(5, 8, 2, 1, NULL, "R2", DONE),
};

static const ImeKbKeySpec kLatinUpperKeys[55] = {
    KEY(0, 0, "1", CHARACTER),
    KEY(0, 1, "2", CHARACTER),
    KEY(0, 2, "3", CHARACTER),
    KEY(0, 3, "4", CHARACTER),
    KEY(0, 4, "5", CHARACTER),
    KEY(0, 5, "6", CHARACTER),
    KEY(0, 6, "7", CHARACTER),
    KEY(0, 7, "8", CHARACTER),
    KEY(0, 8, "9", CHARACTER),
    KEY(0, 9, "0", CHARACTER),

    KEY(1, 0, "Q", CHARACTER),
    KEY(1, 1, "W", CHARACTER),
    KEY(1, 2, "E", CHARACTER),
    KEY(1, 3, "R", CHARACTER),
    KEY(1, 4, "T", CHARACTER),
    KEY(1, 5, "Y", CHARACTER),
    KEY(1, 6, "U", CHARACTER),
    KEY(1, 7, "I", CHARACTER),
    KEY(1, 8, "O", CHARACTER),
    KEY(1, 9, "P", CHARACTER),

    KEY(2, 0, "A", CHARACTER),
    KEY(2, 1, "S", CHARACTER),
    KEY(2, 2, "D", CHARACTER),
    KEY(2, 3, "F", CHARACTER),
    KEY(2, 4, "G", CHARACTER),
    KEY(2, 5, "H", CHARACTER),
    KEY(2, 6, "J", CHARACTER),
    KEY(2, 7, "K", CHARACTER),
    KEY(2, 8, "L", CHARACTER),
    KEY(2, 9, "\"", CHARACTER),

    KEY(3, 0, "Z", CHARACTER),
    KEY(3, 1, "X", CHARACTER),
    KEY(3, 2, "C", CHARACTER),
    KEY(3, 3, "V", CHARACTER),
    KEY(3, 4, "B", CHARACTER),
    KEY(3, 5, "N", CHARACTER),
    KEY(3, 6, "M", CHARACTER),
    KEY(3, 7, "-", CHARACTER),
    KEY(3, 8, "_", CHARACTER),
    KEY(3, 9, "/", CHARACTER),

    KEYHOTGLYPH(4, 0, "L2", SHIFT_OUTLINE, SHIFT),
    KEYHOT(4, 1, "@#:", kHotkeyL2TriangleRounded, SYMBOLS_MODE),
    KEYHOT(4, 2, "\xC3\xA0", "L3", SPECIALS_MODE),
    KEYSPANHOT(4, 3, 4, 1, "Space", kHotkeyTriangleRounded, SPACE),
    KEY(4, 7, NULL, NONE),
    KEYSPANHOTGLYPH(4, 8, 2, 1, kHotkeySquareRounded, BACKSPACE, BACKSPACE),

    KEYGLYPH(5, 0, ARROW_DOWN, ARROW_DOWN),
    KEYGLYPH(5, 1, ARROW_UP, ARROW_UP),
    KEYHOTGLYPH(5, 2, "L1", ARROW_LEFT, ARROW_LEFT),
    KEYHOTGLYPH(5, 3, "R1", ARROW_RIGHT, ARROW_RIGHT),
    KEY(5, 4, kLanguageSwitchLabel, KEYBOARD),
    KEY(5, 5, "...", MENU),
    KEYHOT(5, 6, kControlModeGyroLabel, "R3", SETTINGS),
    KEY(5, 7, NULL, NONE),
    KEYSPANHOT(5, 8, 2, 1, NULL, "R2", DONE),
};

static const ImeKbKeySpec kSymbolsPage1Keys[51] = {
    KEY(0, 0, "!", CHARACTER),
    KEY(0, 1, "?", CHARACTER),
    KEY(0, 2, "\"", CHARACTER),
    KEY(0, 3, "'", CHARACTER),
    KEY(0, 4, "#", CHARACTER),
    KEY(0, 5, "%", CHARACTER),
    KEY(0, 6, "(", CHARACTER),
    KEY(0, 7, ")", CHARACTER),
    KEY(0, 8, "()", CHARACTER),
    KEY(0, 9, "/", CHARACTER),

    KEY(1, 0, "-", CHARACTER),
    KEY(1, 1, "_", CHARACTER),
    KEY(1, 2, ",", CHARACTER),
    KEY(1, 3, ".", CHARACTER),
    KEY(1, 4, ":", CHARACTER),
    KEY(1, 5, ";", CHARACTER),
    KEY(1, 6, "*", CHARACTER),
    KEY(1, 7, "+", CHARACTER),
    KEY(1, 8, "=", CHARACTER),
    KEY(1, 9, "&", CHARACTER),

    KEY(2, 0, "<", CHARACTER),
    KEY(2, 1, ">", CHARACTER),
    KEY(2, 2, "@", CHARACTER),
    KEY(2, 3, "[", CHARACTER),
    KEY(2, 4, "]", CHARACTER),
    KEY(2, 5, "[]", CHARACTER),
    KEY(2, 6, "{", CHARACTER),
    KEY(2, 7, "}", CHARACTER),
    KEY(2, 8, "{}", CHARACTER),
    KEYSPAN(2, 9, 1, 2, ">", PAGE_NEXT),

    KEY(3, 0, "\\", CHARACTER),
    KEY(3, 1, "|", CHARACTER),
    KEY(3, 2, "^", CHARACTER),
    KEY(3, 3, "`", CHARACTER),
    KEY(3, 4, "$", CHARACTER),
    KEY(3, 5, "\xE2\x82\xAC", CHARACTER),
    KEY(3, 6, "\xC2\xB4", CHARACTER),
    KEY(3, 7, "\xE2\x80\x98", CHARACTER),
    KEY(3, 8, "\xE2\x80\x99", CHARACTER),

    KEYHOT(4, 1, "ABC", kHotkeyL2TriangleRounded, SYMBOLS_MODE),
    KEYSPANHOT(4, 3, 4, 1, "Space", kHotkeyTriangleRounded, SPACE),
    KEYSPANHOTGLYPH(4, 8, 2, 1, kHotkeySquareRounded, BACKSPACE, BACKSPACE),

    KEYGLYPH(5, 0, ARROW_DOWN, ARROW_DOWN),
    KEYGLYPH(5, 1, ARROW_UP, ARROW_UP),
    KEYHOTGLYPH(5, 2, "L1", ARROW_LEFT, ARROW_LEFT),
    KEYHOTGLYPH(5, 3, "R1", ARROW_RIGHT, ARROW_RIGHT),
    KEY(5, 4, kLanguageSwitchLabel, KEYBOARD),
    KEY(5, 5, "...", MENU),
    KEYHOT(5, 6, kControlModeGyroLabel, "R3", SETTINGS),
    KEY(5, 7, NULL, NONE),
    KEYSPANHOT(5, 8, 2, 1, NULL, "R2", DONE),
};

static const ImeKbKeySpec kSymbolsPage2Keys[51] = {
    KEY(0, 0, "\xE2\x80\x9A", CHARACTER),
    KEY(0, 1, "\xE2\x80\x9C", CHARACTER),
    KEY(0, 2, "\xE2\x80\x9D", CHARACTER),
    KEY(0, 3, "\xE2\x80\x9E", CHARACTER),
    KEY(0, 4, "~", CHARACTER),
    KEY(0, 5, "\xC2\xA1", CHARACTER),
    KEY(0, 6, "\xC2\xA1!", CHARACTER),
    KEY(0, 7, "\xC2\xBF", CHARACTER),
    KEY(0, 8, "\xC2\xBF?", CHARACTER),
    KEY(0, 9, "\xE2\x80\xB9", CHARACTER),

    KEY(1, 0, "\xE2\x80\xBA", CHARACTER),
    KEY(1, 1, "\xC2\xAB", CHARACTER),
    KEY(1, 2, "\xC2\xBB", CHARACTER),
    KEY(1, 3, "\xC2\xB0", CHARACTER),
    KEY(1, 4, "\xC2\xAA", CHARACTER),
    KEY(1, 5, "\xC2\xBA", CHARACTER),
    KEY(1, 6, "\xC3\x97", CHARACTER),
    KEY(1, 7, "\xC3\xB7", CHARACTER),
    KEY(1, 8, "\xC2\xA4", CHARACTER),
    KEY(1, 9, "\xC2\xA2", CHARACTER),

    KEY(2, 0, "\xC2\xA5", CHARACTER),
    KEY(2, 1, "\xC2\xA3", CHARACTER),
    KEY(2, 2, "\xE2\x82\xA9", CHARACTER),
    KEY(2, 3, "\xC2\xA7", CHARACTER),
    KEY(2, 4, "\xC2\xA6", CHARACTER),
    KEY(2, 5, "\xC2\xB5", CHARACTER),
    KEY(2, 6, "\xC2\xAC", CHARACTER),
    KEY(2, 7, "\xC2\xB9", CHARACTER),
    KEY(2, 8, "\xC2\xB2", CHARACTER),
    KEYSPAN(2, 9, 1, 2, "<", PAGE_PREV),

    KEY(3, 0, "\xC2\xB3", CHARACTER),
    KEY(3, 1, "\xC2\xBC", CHARACTER),
    KEY(3, 2, "\xC2\xBD", CHARACTER),
    KEY(3, 3, "\xC2\xBE", CHARACTER),
    KEY(3, 4, "\xE2\x84\x96", CHARACTER),
    KEY(3, 5, "\xC2\xB7", CHARACTER),
    KEY(3, 6, NULL, NONE),
    KEY(3, 7, NULL, NONE),
    KEY(3, 8, NULL, NONE),

    KEYHOT(4, 1, "ABC", kHotkeyL2TriangleRounded, SYMBOLS_MODE),
    KEYSPANHOT(4, 3, 4, 1, "Space", kHotkeyTriangleRounded, SPACE),
    KEYSPANHOTGLYPH(4, 8, 2, 1, kHotkeySquareRounded, BACKSPACE, BACKSPACE),

    KEYGLYPH(5, 0, ARROW_DOWN, ARROW_DOWN),
    KEYGLYPH(5, 1, ARROW_UP, ARROW_UP),
    KEYHOTGLYPH(5, 2, "L1", ARROW_LEFT, ARROW_LEFT),
    KEYHOTGLYPH(5, 3, "R1", ARROW_RIGHT, ARROW_RIGHT),
    KEY(5, 4, kLanguageSwitchLabel, KEYBOARD),
    KEY(5, 5, "...", MENU),
    KEYHOT(5, 6, kControlModeGyroLabel, "R3", SETTINGS),
    KEY(5, 7, NULL, NONE),
    KEYSPANHOT(5, 8, 2, 1, NULL, "R2", DONE),
};

static const ImeKbKeySpec kSpecialsPage1Keys[65] = {
    KEY(0, 0, "à", CHARACTER),
    KEY(0, 1, "á", CHARACTER),
    KEY(0, 2, "â", CHARACTER),
    KEY(0, 3, "ã", CHARACTER),
    KEY(0, 4, "ä", CHARACTER),
    KEY(0, 5, "å", CHARACTER),
    KEY(0, 6, "ą", CHARACTER),
    KEY(0, 7, "æ", CHARACTER),
    KEY(0, 8, "ç", CHARACTER),
    KEY(0, 9, "ć", CHARACTER),

    KEY(1, 0, "è", CHARACTER),
    KEY(1, 1, "é", CHARACTER),
    KEY(1, 2, "ê", CHARACTER),
    KEY(1, 3, "ë", CHARACTER),
    KEY(1, 4, "ę", CHARACTER),
    KEY(1, 5, "ğ", CHARACTER),
    KEY(1, 6, "ì", CHARACTER),
    KEY(1, 7, "í", CHARACTER),
    KEY(1, 8, "î", CHARACTER),
    KEY(1, 9, "ï", CHARACTER),

    KEY(2, 0, "ı", CHARACTER),
    KEY(2, 1, "ł", CHARACTER),
    KEY(2, 2, "ñ", CHARACTER),
    KEY(2, 3, "ń", CHARACTER),
    KEY(2, 4, "ò", CHARACTER),
    KEY(2, 5, "ó", CHARACTER),
    KEY(2, 6, "ô", CHARACTER),
    KEY(2, 7, "õ", CHARACTER),
    KEY(2, 8, "ö", CHARACTER),
    KEY(2, 9, "ø", CHARACTER),

    KEY(3, 0, "œ", CHARACTER),
    KEY(3, 1, "ś", CHARACTER),
    KEY(3, 2, "ş", CHARACTER),
    KEY(3, 3, "š", CHARACTER),
    KEY(3, 4, "ß", CHARACTER),
    KEY(3, 5, "ù", CHARACTER),
    KEY(3, 6, "ú", CHARACTER),
    KEY(3, 7, "û", CHARACTER),
    KEY(3, 8, "ü", CHARACTER),
    KEY(3, 9, "ý", CHARACTER),

    KEY(4, 0, "ÿ", CHARACTER),
    KEY(4, 1, "ź", CHARACTER),
    KEY(4, 2, "ż", CHARACTER),
    KEY(4, 3, "ž", CHARACTER),
    KEY(4, 4, "ð", CHARACTER),
    KEY(4, 5, "þ", CHARACTER),
    KEY(4, 6, NULL, NONE),
    KEY(4, 7, NULL, NONE),
    KEY(4, 8, NULL, NONE),
    KEY(4, 9, NULL, NONE),

    KEYHOTGLYPH(5, 0, "L2", SHIFT_OUTLINE, SHIFT),
    KEYHOT(5, 1, "@#:", kHotkeyL2TriangleRounded, SYMBOLS_MODE),
    KEYHOT(5, 2, "ABC", "L3", SPECIALS_MODE),
    KEYSPANHOT(5, 3, 4, 1, "Space", kHotkeyTriangleRounded, SPACE),
    KEY(5, 7, NULL, NONE),
    KEYSPANHOTGLYPH(5, 8, 2, 1, kHotkeySquareRounded, BACKSPACE, BACKSPACE),

    KEYGLYPH(6, 0, ARROW_DOWN, ARROW_DOWN),
    KEYGLYPH(6, 1, ARROW_UP, ARROW_UP),
    KEYHOTGLYPH(6, 2, "L1", ARROW_LEFT, ARROW_LEFT),
    KEYHOTGLYPH(6, 3, "R1", ARROW_RIGHT, ARROW_RIGHT),
    KEY(6, 4, kLanguageSwitchLabel, KEYBOARD),
    KEY(6, 5, "...", MENU),
    KEYHOT(6, 6, kControlModeGyroLabel, "R3", SETTINGS),
    KEY(6, 7, NULL, NONE),
    KEYSPANHOT(6, 8, 2, 1, NULL, "R2", DONE),
};

static const ImeKbKeySpec kSpecialsPage1UpperKeys[65] = {
    KEY(0, 0, "À", CHARACTER),
    KEY(0, 1, "Á", CHARACTER),
    KEY(0, 2, "Â", CHARACTER),
    KEY(0, 3, "Ã", CHARACTER),
    KEY(0, 4, "Ä", CHARACTER),
    KEY(0, 5, "Å", CHARACTER),
    KEY(0, 6, "Ą", CHARACTER),
    KEY(0, 7, "Æ", CHARACTER),
    KEY(0, 8, "Ç", CHARACTER),
    KEY(0, 9, "Ć", CHARACTER),

    KEY(1, 0, "È", CHARACTER),
    KEY(1, 1, "É", CHARACTER),
    KEY(1, 2, "Ê", CHARACTER),
    KEY(1, 3, "Ë", CHARACTER),
    KEY(1, 4, "Ę", CHARACTER),
    KEY(1, 5, "Ğ", CHARACTER),
    KEY(1, 6, "Ì", CHARACTER),
    KEY(1, 7, "Í", CHARACTER),
    KEY(1, 8, "Î", CHARACTER),
    KEY(1, 9, "Ï", CHARACTER),

    KEY(2, 0, "İ", CHARACTER),
    KEY(2, 1, "Ł", CHARACTER),
    KEY(2, 2, "Ñ", CHARACTER),
    KEY(2, 3, "Ń", CHARACTER),
    KEY(2, 4, "Ò", CHARACTER),
    KEY(2, 5, "Ó", CHARACTER),
    KEY(2, 6, "Ô", CHARACTER),
    KEY(2, 7, "Õ", CHARACTER),
    KEY(2, 8, "Ö", CHARACTER),
    KEY(2, 9, "Ø", CHARACTER),

    KEY(3, 0, "Œ", CHARACTER),
    KEY(3, 1, "Ś", CHARACTER),
    KEY(3, 2, "Ş", CHARACTER),
    KEY(3, 3, "Š", CHARACTER),
    KEY(3, 4, "ß", CHARACTER),
    KEY(3, 5, "Ù", CHARACTER),
    KEY(3, 6, "Ú", CHARACTER),
    KEY(3, 7, "Û", CHARACTER),
    KEY(3, 8, "Ü", CHARACTER),
    KEY(3, 9, "Ý", CHARACTER),

    KEY(4, 0, "Ÿ", CHARACTER),
    KEY(4, 1, "Ź", CHARACTER),
    KEY(4, 2, "Ż", CHARACTER),
    KEY(4, 3, "Ž", CHARACTER),
    KEY(4, 4, "Ð", CHARACTER),
    KEY(4, 5, "Þ", CHARACTER),
    KEY(4, 6, NULL, NONE),
    KEY(4, 7, NULL, NONE),
    KEY(4, 8, NULL, NONE),
    KEY(4, 9, NULL, NONE),

    KEYHOTGLYPH(5, 0, "L2", SHIFT_OUTLINE, SHIFT),
    KEYHOT(5, 1, "@#:", kHotkeyL2TriangleRounded, SYMBOLS_MODE),
    KEYHOT(5, 2, "ABC", "L3", SPECIALS_MODE),
    KEYSPANHOT(5, 3, 4, 1, "Space", kHotkeyTriangleRounded, SPACE),
    KEY(5, 7, NULL, NONE),
    KEYSPANHOTGLYPH(5, 8, 2, 1, kHotkeySquareRounded, BACKSPACE, BACKSPACE),

    KEYGLYPH(6, 0, ARROW_DOWN, ARROW_DOWN),
    KEYGLYPH(6, 1, ARROW_UP, ARROW_UP),
    KEYHOTGLYPH(6, 2, "L1", ARROW_LEFT, ARROW_LEFT),
    KEYHOTGLYPH(6, 3, "R1", ARROW_RIGHT, ARROW_RIGHT),
    KEY(6, 4, kLanguageSwitchLabel, KEYBOARD),
    KEY(6, 5, "...", MENU),
    KEYHOT(6, 6, kControlModeGyroLabel, "R3", SETTINGS),
    KEY(6, 7, NULL, NONE),
    KEYSPANHOT(6, 8, 2, 1, NULL, "R2", DONE),
};

static const ImeKbKeySpec kSpecialsPage2Keys[55] = {
    KEY(0, 0, "ÿ", CHARACTER),
    KEY(0, 1, "ź", CHARACTER),
    KEY(0, 2, "ż", CHARACTER),
    KEY(0, 3, "ž", CHARACTER),
    KEY(0, 4, "ð", CHARACTER),
    KEY(0, 5, "þ", CHARACTER),
    KEY(0, 6, NULL, NONE),
    KEY(0, 7, NULL, NONE),
    KEY(0, 8, NULL, NONE),
    KEY(0, 9, NULL, NONE),

    KEY(1, 0, NULL, NONE),
    KEY(1, 1, NULL, NONE),
    KEY(1, 2, NULL, NONE),
    KEY(1, 3, NULL, NONE),
    KEY(1, 4, NULL, NONE),
    KEY(1, 5, NULL, NONE),
    KEY(1, 6, NULL, NONE),
    KEY(1, 7, NULL, NONE),
    KEY(1, 8, NULL, NONE),
    KEY(1, 9, NULL, NONE),

    KEY(2, 0, NULL, NONE),
    KEY(2, 1, NULL, NONE),
    KEY(2, 2, NULL, NONE),
    KEY(2, 3, NULL, NONE),
    KEY(2, 4, NULL, NONE),
    KEY(2, 5, NULL, NONE),
    KEY(2, 6, NULL, NONE),
    KEY(2, 7, NULL, NONE),
    KEY(2, 8, NULL, NONE),
    KEY(2, 9, NULL, NONE),

    KEY(3, 0, NULL, NONE),
    KEY(3, 1, NULL, NONE),
    KEY(3, 2, NULL, NONE),
    KEY(3, 3, NULL, NONE),
    KEY(3, 4, NULL, NONE),
    KEY(3, 5, NULL, NONE),
    KEY(3, 6, NULL, NONE),
    KEY(3, 7, NULL, NONE),
    KEY(3, 8, NULL, NONE),
    KEY(3, 9, NULL, NONE),

    KEYHOTGLYPH(5, 0, "L2", SHIFT_OUTLINE, SHIFT),
    KEYHOT(5, 1, "@#:", kHotkeyL2TriangleRounded, SYMBOLS_MODE),
    KEYHOT(5, 2, "ABC", "L3", SPECIALS_MODE),
    KEYSPANHOT(5, 3, 4, 1, "Space", kHotkeyTriangleRounded, SPACE),
    KEY(5, 7, NULL, NONE),
    KEYSPANHOTGLYPH(5, 8, 2, 1, kHotkeySquareRounded, BACKSPACE, BACKSPACE),

    KEYGLYPH(6, 0, ARROW_DOWN, ARROW_DOWN),
    KEYGLYPH(6, 1, ARROW_UP, ARROW_UP),
    KEYHOTGLYPH(6, 2, "L1", ARROW_LEFT, ARROW_LEFT),
    KEYHOTGLYPH(6, 3, "R1", ARROW_RIGHT, ARROW_RIGHT),
    KEY(6, 4, kLanguageSwitchLabel, KEYBOARD),
    KEY(6, 5, "...", MENU),
    KEYHOT(6, 6, kControlModeGyroLabel, "R3", SETTINGS),
    KEY(6, 7, NULL, NONE),
    KEYSPANHOT(6, 8, 2, 1, NULL, "R2", DONE),
};

static const ImeKbKeySpec kSpecialsPage2UpperKeys[55] = {
    KEY(0, 0, "Ÿ", CHARACTER),
    KEY(0, 1, "Ź", CHARACTER),
    KEY(0, 2, "Ż", CHARACTER),
    KEY(0, 3, "Ž", CHARACTER),
    KEY(0, 4, "Ð", CHARACTER),
    KEY(0, 5, "Þ", CHARACTER),
    KEY(0, 6, NULL, NONE),
    KEY(0, 7, NULL, NONE),
    KEY(0, 8, NULL, NONE),
    KEY(0, 9, NULL, NONE),

    KEY(1, 0, NULL, NONE),
    KEY(1, 1, NULL, NONE),
    KEY(1, 2, NULL, NONE),
    KEY(1, 3, NULL, NONE),
    KEY(1, 4, NULL, NONE),
    KEY(1, 5, NULL, NONE),
    KEY(1, 6, NULL, NONE),
    KEY(1, 7, NULL, NONE),
    KEY(1, 8, NULL, NONE),
    KEY(1, 9, NULL, NONE),

    KEY(2, 0, NULL, NONE),
    KEY(2, 1, NULL, NONE),
    KEY(2, 2, NULL, NONE),
    KEY(2, 3, NULL, NONE),
    KEY(2, 4, NULL, NONE),
    KEY(2, 5, NULL, NONE),
    KEY(2, 6, NULL, NONE),
    KEY(2, 7, NULL, NONE),
    KEY(2, 8, NULL, NONE),
    KEY(2, 9, NULL, NONE),

    KEY(3, 0, NULL, NONE),
    KEY(3, 1, NULL, NONE),
    KEY(3, 2, NULL, NONE),
    KEY(3, 3, NULL, NONE),
    KEY(3, 4, NULL, NONE),
    KEY(3, 5, NULL, NONE),
    KEY(3, 6, NULL, NONE),
    KEY(3, 7, NULL, NONE),
    KEY(3, 8, NULL, NONE),
    KEY(3, 9, NULL, NONE),

    KEYHOTGLYPH(5, 0, "L2", SHIFT_OUTLINE, SHIFT),
    KEYHOT(5, 1, "@#:", kHotkeyL2TriangleRounded, SYMBOLS_MODE),
    KEYHOT(5, 2, "ABC", "L3", SPECIALS_MODE),
    KEYSPANHOT(5, 3, 4, 1, "Space", kHotkeyTriangleRounded, SPACE),
    KEY(5, 7, NULL, NONE),
    KEYSPANHOTGLYPH(5, 8, 2, 1, kHotkeySquareRounded, BACKSPACE, BACKSPACE),

    KEYGLYPH(6, 0, ARROW_DOWN, ARROW_DOWN),
    KEYGLYPH(6, 1, ARROW_UP, ARROW_UP),
    KEYHOTGLYPH(6, 2, "L1", ARROW_LEFT, ARROW_LEFT),
    KEYHOTGLYPH(6, 3, "R1", ARROW_RIGHT, ARROW_RIGHT),
    KEY(6, 4, kLanguageSwitchLabel, KEYBOARD),
    KEY(6, 5, "...", MENU),
    KEYHOT(6, 6, kControlModeGyroLabel, "R3", SETTINGS),
    KEY(6, 7, NULL, NONE),
    KEYSPANHOT(6, 8, 2, 1, NULL, "R2", DONE),
};

#define KEY_COUNT(keys) (sizeof(keys) / sizeof((keys)[0]))
#define MODEL(keys, rows)                                                                          \
    {keys, KEY_COUNT(keys), kKeyCols, rows, IME_SELECTION_DEFAULT_FUNCTION_ROWS}

static const ImeKbLayoutModel kLatinLowerModel = MODEL(kLatinLowerKeys, kKeyRows);
static const ImeKbLayoutModel kLatinUpperModel = MODEL(kLatinUpperKeys, kKeyRows);
static const ImeKbLayoutModel kSymbolsPage1Model = MODEL(kSymbolsPage1Keys, kKeyRows);
static const ImeKbLayoutModel kSymbolsPage2Model = MODEL(kSymbolsPage2Keys, kKeyRows);
static const ImeKbLayoutModel kSpecialsPage1Model = MODEL(kSpecialsPage1Keys, kSpecialsKeyRows);
static const ImeKbLayoutModel kSpecialsPage2Model = MODEL(kSpecialsPage2Keys, kSpecialsKeyRows);
static const ImeKbLayoutModel kSpecialsPage1UpperModel =
    MODEL(kSpecialsPage1UpperKeys, kSpecialsKeyRows);
static const ImeKbLayoutModel kSpecialsPage2UpperModel =
    MODEL(kSpecialsPage2UpperKeys, kSpecialsKeyRows);

static const ImeTopPanelElementSpec kTopPanelDefaultElements[2] = {
    {IME_TOP_PANEL_ELEMENT_ID_PREDICTION, 0, 9},
    {IME_TOP_PANEL_ELEMENT_ID_CLOSE, 9, 1},
};

static const ImeTopPanelLayoutConfig kTopPanelDefaultLayout = {
    kTopPanelDefaultElements,
    2,
    10,
    IME_SELECTION_DEFAULT_TOP_PANEL_ROW,
    IME_SELECTION_DEFAULT_TOP_PANEL_ROWS,
};

#undef KEY
#undef KEYHOT
#undef KEYSPAN
#undef KEYSPANHOT
#undef KEYGLYPH
#undef KEYHOTGLYPH
#undef KEYSPANGLYPH
#undef KEYSPANHOTGLYPH

int ImeSelectionPanelTopRowFromConfig(int top_panel_row) {
    return KB_MAX(IME_SELECTION_PANEL_MIN_ROW, top_panel_row);
}

int ImeSelectionPanelTopRowsFromConfig(int top_panel_rows) {
    return KB_MAX(1, top_panel_rows);
}

static int KeyboardMaxRowForRows(int keyboard_rows) {
    return KB_MAX(0, keyboard_rows - 1);
}

int ImeSelectionPanelKeyboardMinRowForTopPanel(int top_panel_row, int top_panel_rows) {
    return ImeSelectionPanelTopRowFromConfig(top_panel_row) +
           ImeSelectionPanelTopRowsFromConfig(top_panel_rows);
}

int ImeSelectionPanelKeyboardMaxRowForKeyboardRows(int keyboard_rows, int top_panel_row,
                                                   int top_panel_rows) {
    return ImeSelectionPanelKeyboardMinRowForTopPanel(top_panel_row, top_panel_rows) +
           KeyboardMaxRowForRows(KB_MAX(1, keyboard_rows));
}

int ImeSelectionResolveFunctionRows(int keyboard_rows, int configured_function_rows) {
    return keyboard_rows > 2 ? KB_MIN(KB_MAX(0, configured_function_rows), keyboard_rows - 1) : 0;
}

int ImeSelectionPanelToKeyboardRow(int panel_row, int keyboard_rows, int top_panel_row,
                                   int top_panel_rows) {
    return clamp_int(panel_row -
                         ImeSelectionPanelKeyboardMinRowForTopPanel(top_panel_row, top_panel_rows),
                     IME_SELECTION_KEYBOARD_MIN_ROW, KeyboardMaxRowForRows(keyboard_rows));
}

ImeEdgeWrapNavState MakeImeEdgeWrapNavState(void) {
    ImeEdgeWrapNavState state;
    memset(&state, 0, sizeof(state));
    state.last_step_time = -1.0;
    return state;
}

void ResetImeEdgeWrapHold(ImeEdgeWrapNavState* state) {
    state->hold_step_row = 0;
    state->hold_step_col = 0;
    state->hold_release_time = 0.0;
    state->hold_active = false;
}

void ResetImeEdgeWrapNav(ImeEdgeWrapNavState* state) {
    state->last_step_row = 0;
    state->last_step_col = 0;
    state->last_step_time = -1.0;
    ResetImeEdgeWrapHold(state);
}

bool ShouldDelayImeEdgeWrap(ImeEdgeWrapNavState* state, int step_row, int step_col,
                            bool repeat_hint, bool wraps, double now, double hold_delay_sec,
                            double repeat_window_sec) {
    bool same_wrap_direction;
    (void)repeat_window_sec;
    if (step_row == 0 && step_col == 0) {
        return false;
    }
    if (state->hold_active &&
        (state->hold_step_row != step_row || state->hold_step_col != step_col)) {
        ResetImeEdgeWrapHold(state);
    }
    if (!wraps) {
        return false;
    }
    same_wrap_direction =
        state->hold_active && state->hold_step_row == step_row && state->hold_step_col == step_col;
    if (!(repeat_hint || same_wrap_direction)) {
        return false;
    }
    if (!same_wrap_direction) {
        state->hold_step_row = step_row;
        state->hold_step_col = step_col;
        state->hold_release_time = now + hold_delay_sec;
        state->hold_active = true;
    }
    return now < state->hold_release_time;
}

void CommitImeEdgeWrapStep(ImeEdgeWrapNavState* state, int step_row, int step_col, double now) {
    state->last_step_row = step_row;
    state->last_step_col = step_col;
    state->last_step_time = now;
    ResetImeEdgeWrapHold(state);
}

const ImeKbKeySpec* ResolveImeKeyboardKeyAt(const ImeKbLayoutModel* layout, int row, int col) {
    const int grid_cols = KB_MAX(1, (int)layout->cols);
    const int grid_rows = KB_MAX(1, (int)layout->rows);
    const ImeKbKeySpec* resolved = NULL;
    size_t i;
    if (row < 0 || row >= grid_rows || col < 0 || col >= grid_cols) {
        return NULL;
    }
    if (!layout->keys || layout->key_count == 0) {
        return NULL;
    }
    for (i = 0; i < layout->key_count; ++i) {
        const ImeKbKeySpec* key = &layout->keys[i];
        int row_start, col_start, row_end, col_end;
        if (key->col_span == 0 || key->row_span == 0) {
            continue;
        }
        if (key->row >= grid_rows || key->col >= grid_cols) {
            continue;
        }
        row_start = (int)key->row;
        col_start = (int)key->col;
        row_end = KB_MIN(grid_rows, row_start + KB_MAX(1, (int)key->row_span));
        col_end = KB_MIN(grid_cols, col_start + KB_MAX(1, (int)key->col_span));
        if (row >= row_start && row < row_end && col >= col_start && col < col_end) {
            /* Match DrawImeKeyboardGrid occupancy behavior: later keys override earlier ones. */
            resolved = key;
        }
    }
    return resolved;
}

bool DoesImeKeyboardStepCrossGridEdge(int from_row, int from_col, int step_row, int step_col,
                                      int grid_rows, int grid_cols) {
    int next_row, next_col;
    if (step_row == 0 && step_col == 0) {
        return false;
    }
    if (from_row < 0 || from_row >= grid_rows || from_col < 0 || from_col >= grid_cols) {
        return false;
    }
    next_row = from_row + step_row;
    next_col = from_col + step_col;
    return next_row < 0 || next_row >= grid_rows || next_col < 0 || next_col >= grid_cols;
}

bool DoesImeKeyboardNavigationWrap(const ImeKbLayoutModel* layout, int from_row, int from_col,
                                   int step_row, int step_col) {
    const int grid_cols = KB_MAX(1, (int)layout->cols);
    const int grid_rows = KB_MAX(1, (int)layout->rows);
    const int max_steps = KB_MAX(1, grid_rows * grid_cols);
    const ImeKbKeySpec* origin_key;
    int row = from_row;
    int col = from_col;
    bool crossed_wrap = false;
    int i;
    if (step_row == 0 && step_col == 0) {
        return false;
    }
    if (from_row < 0 || from_row >= grid_rows || from_col < 0 || from_col >= grid_cols) {
        return false;
    }
    origin_key = ResolveImeKeyboardKeyAt(layout, from_row, from_col);
    if (origin_key && origin_key->action == IME_KB_KEY_ACTION_NONE) {
        origin_key = NULL;
    }
    for (i = 0; i < max_steps; ++i) {
        const ImeKbKeySpec* candidate_key;
        crossed_wrap = crossed_wrap || DoesImeKeyboardStepCrossGridEdge(
                                           row, col, step_row, step_col, grid_rows, grid_cols);
        row = (row + step_row + grid_rows) % grid_rows;
        col = (col + step_col + grid_cols) % grid_cols;
        candidate_key = ResolveImeKeyboardKeyAt(layout, row, col);
        if (!candidate_key || candidate_key->action == IME_KB_KEY_ACTION_NONE) {
            continue;
        }
        if (origin_key && candidate_key == origin_key) {
            continue;
        }
        return crossed_wrap;
    }
    return false;
}

float ComputePressPulseExpand(double pulse_started_at, double now, float pulse_duration_sec,
                              float max_expand_px) {
    double elapsed;
    float t, envelope;
    if (pulse_started_at < 0.0 || pulse_duration_sec <= 0.0f || max_expand_px <= 0.0f) {
        return 0.0f;
    }
    elapsed = now - pulse_started_at;
    if (elapsed < 0.0 || elapsed >= (double)pulse_duration_sec) {
        return 0.0f;
    }
    t = (float)(elapsed / (double)pulse_duration_sec);
    envelope = (float)sin((double)(t * 3.14159265358979323846f));
    return KB_MAX(0.0f, envelope) * max_expand_px;
}

ImeViewportMetrics ComputeImeViewportMetrics(bool use_over2k) {
    ImeViewportMetrics metrics;
    const ovl_vec2_t base_size = ovl_display_size();
    u32 out_w = 0, out_h = 0;
    metrics.base_w = use_over2k ? 3840.0f : 1920.0f;
    metrics.base_h = use_over2k ? 2160.0f : 1080.0f;
    metrics.size = base_size;
    metrics.offset = vec2(0.0f, 0.0f);

    presenter_output_resolution(&out_w, &out_h);
    if (out_w != 0 && out_h != 0) {
        const float viewport_w = (float)out_w;
        const float viewport_h = (float)out_h;
        float offset_x = (base_size.x - viewport_w) * 0.5f;
        float offset_y = (base_size.y - viewport_h) * 0.5f;
        if (offset_x < 0.0f) {
            offset_x = 0.0f;
        }
        if (offset_y < 0.0f) {
            offset_y = 0.0f;
        }
        metrics.size = vec2(viewport_w, viewport_h);
        metrics.offset = vec2(offset_x, offset_y);
    }

    metrics.scale_x = metrics.size.x / metrics.base_w;
    metrics.scale_y = metrics.size.y / metrics.base_h;
    metrics.ui_scale = KB_MIN(metrics.scale_x, metrics.scale_y);
    return metrics;
}

ImePanelMetrics ComputeImePanelMetrics(const ImePanelMetricsConfig* config) {
    ImePanelMetrics metrics;
    const bool multi = config->multiline;
    float panel_scale, base_font_size, remaining_gap;
    memset(&metrics, 0, sizeof(metrics));
    metrics.panel_w = config->panel_w;
    metrics.panel_h = config->panel_h;
    metrics.padding_x = metrics.panel_w * (kPadX / kPanelBaseW);
    metrics.padding_bottom = metrics.panel_h * (multi ? (kPadBottomMulti / kPanelBaseHMulti)
                                                      : (kPadBottomSingle / kPanelBaseHSingle));

    panel_scale = metrics.panel_w / kPanelBaseW;
    metrics.label_h = config->show_title ? (kLabelH * panel_scale) : metrics.padding_bottom;
    metrics.input_h = metrics.panel_h * (multi ? (kInputHMulti / kPanelBaseHMulti)
                                               : (kInputHSingle / kPanelBaseHSingle));
    metrics.predict_h = metrics.panel_h *
                        (multi ? (kPredictH / kPanelBaseHMulti) : (kPredictH / kPanelBaseHSingle));
    metrics.close_w = metrics.panel_w * (kCloseW / kPanelBaseW);
    metrics.keys_h =
        metrics.panel_h * (multi ? (kKeysH / kPanelBaseHMulti) : (kKeysH / kPanelBaseHSingle));
    metrics.key_gap = metrics.panel_w * (kKeyGap / kPanelBaseW);
    metrics.corner_radius = metrics.panel_w * kCornerRatio;

    base_font_size = config->base_font_size > 0.0f ? config->base_font_size : 1.0f;
    metrics.label_font_scale = (metrics.label_h * 0.85f) / base_font_size;
    metrics.input_font_scale =
        (metrics.input_h *
         (multi ? (kMultiLineTextFill / (float)kMultiLineVisibleLines) : kSingleLineTextFill)) /
        base_font_size;
    metrics.key_font_scale = (metrics.panel_h * kKeyFontRatio) / base_font_size;

    metrics.input_pos_local = vec2(metrics.padding_x, metrics.label_h);
    metrics.input_size = vec2(metrics.panel_w - metrics.padding_x * 2.0f, metrics.input_h);
    metrics.input_pos_screen = vec2(config->window_pos.x + metrics.input_pos_local.x,
                                    config->window_pos.y + metrics.input_pos_local.y);

    remaining_gap = metrics.panel_h - (metrics.label_h + metrics.input_h + metrics.predict_h +
                                       metrics.keys_h + metrics.padding_bottom);
    metrics.predict_gap = KB_MAX(0.0f, remaining_gap * 0.5f);

    metrics.predict_pos = vec2(config->window_pos.x, config->window_pos.y + metrics.label_h +
                                                         metrics.input_h + metrics.predict_gap);
    metrics.predict_size = vec2(metrics.panel_w * (kPredictW / kPanelBaseW), metrics.predict_h);
    metrics.close_pos =
        vec2(config->window_pos.x + metrics.panel_w - metrics.close_w, metrics.predict_pos.y);
    metrics.close_size = vec2(metrics.close_w, metrics.predict_h);

    metrics.kb_pos = vec2(config->window_pos.x + metrics.padding_x,
                          metrics.predict_pos.y + metrics.predict_h + metrics.predict_gap);
    metrics.kb_size = vec2(metrics.panel_w - metrics.padding_x * 2.0f, metrics.keys_h);

    metrics.key_h = (metrics.kb_size.y - metrics.key_gap * (float)(kKeyRows - 1)) / kKeyRows;
    if (metrics.key_h < 8.0f) {
        metrics.key_h = 8.0f;
    }
    return metrics;
}

ImeKbLayoutId ResolveImeKeyboardLayoutId(const ImeKbLayoutSelection* selection) {
    switch (selection->family) {
    case IME_KB_LAYOUT_FAMILY_LATIN:
        switch (selection->case_state) {
        case IME_KB_CASE_STATE_UPPER:
            return IME_KB_LAYOUT_ID_LATIN_UPPER;
        case IME_KB_CASE_STATE_CAPS_LOCK:
            return IME_KB_LAYOUT_ID_LATIN_CAPS_LOCK;
        case IME_KB_CASE_STATE_LOWER:
        default:
            return IME_KB_LAYOUT_ID_LATIN_LOWER;
        }
    case IME_KB_LAYOUT_FAMILY_SYMBOLS:
        return (selection->page % 2 == 0) ? IME_KB_LAYOUT_ID_SYMBOLS_PAGE1
                                          : IME_KB_LAYOUT_ID_SYMBOLS_PAGE2;
    case IME_KB_LAYOUT_FAMILY_SPECIALS:
        return (selection->page % 2 == 0) ? IME_KB_LAYOUT_ID_SPECIALS_PAGE1
                                          : IME_KB_LAYOUT_ID_SPECIALS_PAGE2;
    default:
        return IME_KB_LAYOUT_ID_LATIN_LOWER;
    }
}

const ImeKbLayoutModel* GetImeKeyboardLayoutById(ImeKbLayoutId id) {
    switch (id) {
    case IME_KB_LAYOUT_ID_LATIN_UPPER:
    case IME_KB_LAYOUT_ID_LATIN_CAPS_LOCK:
        return &kLatinUpperModel;
    case IME_KB_LAYOUT_ID_SYMBOLS_PAGE1:
        return &kSymbolsPage1Model;
    case IME_KB_LAYOUT_ID_SYMBOLS_PAGE2:
        return &kSymbolsPage2Model;
    case IME_KB_LAYOUT_ID_SPECIALS_PAGE1:
        return &kSpecialsPage1Model;
    case IME_KB_LAYOUT_ID_SPECIALS_PAGE2:
        return &kSpecialsPage2Model;
    case IME_KB_LAYOUT_ID_LATIN_LOWER:
    default:
        return &kLatinLowerModel;
    }
}

const ImeKbLayoutModel* GetImeKeyboardLayout(const ImeKbLayoutSelection* selection) {
    if (selection->family == IME_KB_LAYOUT_FAMILY_SPECIALS &&
        selection->case_state != IME_KB_CASE_STATE_LOWER) {
        return (selection->page % 2) == 0 ? &kSpecialsPage1UpperModel : &kSpecialsPage2UpperModel;
    }
    return GetImeKeyboardLayoutById(ResolveImeKeyboardLayoutId(selection));
}

const ImeTopPanelLayoutConfig* GetImeTopPanelLayoutConfig(const ImeKbLayoutSelection* selection) {
    (void)selection;
    return &kTopPanelDefaultLayout;
}

ovl_col_t ImeColorToCol(const OrbisImeColor* color) {
    return OVL_COL(color->r, color->g, color->b, color->a);
}

ovl_vec4_t ImeColorToVec4(const OrbisImeColor* color) {
    return ovl_vec4_from_col(ImeColorToCol(color));
}

static OrbisImeColor rgba(u8 r, u8 g, u8 b, u8 a) {
    OrbisImeColor c;
    c.r = r;
    c.g = g;
    c.b = b;
    c.a = a;
    return c;
}

ImeStyleConfig GetDefaultImeStyleConfig(void) {
    ImeStyleConfig style;
    style.color_base = rgba(18, 18, 18, 255);
    style.color_line = rgba(70, 70, 70, 255);
    style.color_text_field = rgba(22, 37, 60, 255);
    style.color_preedit = rgba(35, 35, 35, 255);
    style.color_button_default = rgba(35, 35, 35, 255);
    style.color_button_function = rgba(60, 60, 60, 255);
    style.color_button_symbol = rgba(78, 78, 78, 255);
    style.color_text = rgba(230, 230, 230, 255);
    style.color_special = rgba(30, 90, 170, 255);
    return style;
}

ImeStyleConfig ResolveImeStyleConfig(const OrbisImeParamExtended* extended) {
    ImeStyleConfig style = GetDefaultImeStyleConfig();
    if (!extended || (extended->option & ORBIS_IME_EXT_OPTION_SET_COLOR) == 0) {
        return style;
    }
    style.color_base = extended->color_base;
    style.color_line = extended->color_line;
    style.color_text_field = extended->color_text_field;
    style.color_preedit = extended->color_preedit;
    style.color_button_default = extended->color_button_default;
    style.color_button_function = extended->color_button_function;
    style.color_button_symbol = extended->color_button_symbol;
    style.color_text = extended->color_text;
    style.color_special = extended->color_special;
    return style;
}

ImeKbGridLayout MakeImeKbGridLayout(void) {
    ImeKbGridLayout layout;
    memset(&layout, 0, sizeof(layout));
    layout.cols = 10;
    layout.rows = 6;
    return layout;
}

ImeKbDrawParams MakeImeKbDrawParams(void) {
    ImeKbDrawParams params;
    memset(&params, 0, sizeof(params));
    params.enter_label = ORBIS_IME_ENTER_LABEL_DEFAULT;
    params.show_selection_highlight = true;
    params.allow_nav_input = true;
    params.use_lstick_key_nav = true;
    params.allow_activate_input = true;
    params.requested_selected_row = -1;
    params.requested_selected_col = -1;
    params.key_bg_default = OVL_COL(35, 35, 35, 255);
    params.key_bg_function = OVL_COL(60, 60, 60, 255);
    params.key_bg_symbol = OVL_COL(78, 78, 78, 255);
    params.key_border = OVL_COL(80, 80, 80, 255);
    params.key_done = OVL_COL(30, 90, 170, 255);
    params.key_text = OVL_COL(230, 230, 230, 255);
    params.key_hotkey_text = OVL_COL(220, 220, 220, 255);
    return params;
}

void ApplyImeStyleToKeyboardDrawParams(const ImeStyleConfig* style, ImeKbDrawParams* params) {
    ovl_vec4_t hotkey;
    params->key_bg_default = ImeColorToCol(&style->color_button_default);
    params->key_bg_function = ImeColorToCol(&style->color_button_function);
    params->key_bg_symbol = ImeColorToCol(&style->color_button_symbol);
    params->key_border = ImeColorToCol(&style->color_line);
    params->key_done = ImeColorToCol(&style->color_special);
    params->key_text = ImeColorToCol(&style->color_text);

    hotkey = ImeColorToVec4(&style->color_text);
    hotkey.x = clamp_float(hotkey.x * 0.92f, 0.0f, 1.0f);
    hotkey.y = clamp_float(hotkey.y * 0.92f, 0.0f, 1.0f);
    hotkey.z = clamp_float(hotkey.z * 0.92f, 0.0f, 1.0f);
    params->key_hotkey_text = ovl_col_from_vec4(hotkey);
}

/* One key as it is drawn this frame */
typedef struct RenderedKey {
    const ImeKbKeySpec* key;
    int row;
    int col;
    ovl_vec2_t pos;
    ovl_vec2_t size;
    ovl_vec2_t center;
    ovl_col_t bg;
    const char* label;
    const char* hotkey_label;
    ImeKbKeyGlyph glyph;
    bool is_done;
    bool selectable;
} RenderedKey;

/* Where a grid's cursor is and what its highlight is doing, kept from frame to frame */
typedef struct ImeKbNavState {
    u32 id;
    bool used;
    int cursor_row;
    int cursor_col;
    int fallback_prefer_col_dir;
    ImeEdgeWrapNavState edge_wrap_nav;
    int last_grid_rows;
    int last_grid_cols;
    int highlighted_row;
    int highlighted_col;
    int fade_row;
    int fade_col;
    double fade_started_at;
    int press_pulse_row;
    int press_pulse_col;
    double press_pulse_started_at;
} ImeKbNavState;

/* One state a window that draws a grid: libSceIme's keyboard and libSceImeDialog's are the two
 * there are */
#define IME_KB_NAV_STATES 8
static ImeKbNavState s_nav_states[IME_KB_NAV_STATES];

#define IME_KB_GRID_CELLS (IME_KB_GRID_ROWS_MAX * IME_KB_GRID_COLS_MAX)

/* A frame's grid */
typedef struct KbGrid {
    const ImeKbGridLayout* layout;
    const ImeKbDrawParams* params;
    ovl_draw_list_t* draw;
    ImeKbNavState* nav;
    int rows;
    int cols;
    int fixed_bottom_rows;
    float row_heights[IME_KB_GRID_ROWS_MAX];
    float row_offsets[IME_KB_GRID_ROWS_MAX];
    const ImeKbKeySpec* occupied[IME_KB_GRID_CELLS];
    bool is_anchor[IME_KB_GRID_CELLS];
    RenderedKey rendered[IME_KB_GRID_CELLS];
    int rendered_count;
} KbGrid;

#define kEdgeWrapHoldDelaySec 0.5
#define kRepeatIntentWindowSec 0.45
#define kSelectionFadeOutDurationSec 0.2
#define kSelectedBorderThickness 2.0f

static ImeKbNavState* NavStateFor(u32 id) {
    int i;
    ImeKbNavState* state = NULL;
    for (i = 0; i < IME_KB_NAV_STATES; ++i) {
        if (s_nav_states[i].used && s_nav_states[i].id == id) {
            return &s_nav_states[i];
        }
        if (!state && !s_nav_states[i].used) {
            state = &s_nav_states[i];
        }
    }
    if (!state) {
        /* More windows than there are states: the first one is taken over */
        state = &s_nav_states[0];
    }
    memset(state, 0, sizeof(*state));
    state->id = id;
    state->used = true;
    state->cursor_row = -1;
    state->cursor_col = -1;
    state->edge_wrap_nav = MakeImeEdgeWrapNavState();
    state->last_grid_rows = -1;
    state->last_grid_cols = -1;
    state->highlighted_row = -1;
    state->highlighted_col = -1;
    state->fade_row = -1;
    state->fade_col = -1;
    state->press_pulse_row = -1;
    state->press_pulse_col = -1;
    state->press_pulse_started_at = -1.0;
    return state;
}

static void ClearSelectionFade(ImeKbNavState* nav) {
    nav->highlighted_row = -1;
    nav->highlighted_col = -1;
    nav->fade_row = -1;
    nav->fade_col = -1;
    nav->fade_started_at = 0.0;
    nav->press_pulse_row = -1;
    nav->press_pulse_col = -1;
    nav->press_pulse_started_at = -1.0;
}

static float SpanRowHeight(const KbGrid* g, int row, int span) {
    float total = 0.0f;
    int i;
    for (i = 0; i < span; ++i) {
        total += g->row_heights[row + i];
    }
    if (span > 1) {
        total += g->layout->key_gap_y * (float)(span - 1);
    }
    return total;
}

static ovl_col_t SlotBaseBg(const KbGrid* g, int slot_row) {
    const bool function_row =
        g->fixed_bottom_rows > 0 && slot_row >= KB_MAX(0, g->rows - g->fixed_bottom_rows);
    if (function_row) {
        return g->params->key_bg_function;
    }
    if (g->params->selection.family == IME_KB_LAYOUT_FAMILY_SYMBOLS) {
        return g->params->key_bg_symbol;
    }
    return g->params->key_bg_default;
}

static bool IsSelectableCell(const KbGrid* g, int row, int col) {
    const ImeKbKeySpec* key;
    if (row < 0 || row >= g->rows || col < 0 || col >= g->cols) {
        return false;
    }
    key = g->occupied[row * g->cols + col];
    return key && key->action != IME_KB_KEY_ACTION_NONE;
}

static void FirstSelectableCell(const KbGrid* g, int* out_row, int* out_col) {
    int row, col;
    for (row = 0; row < g->rows; ++row) {
        for (col = 0; col < g->cols; ++col) {
            if (IsSelectableCell(g, row, col)) {
                *out_row = row;
                *out_col = col;
                return;
            }
        }
    }
    *out_row = -1;
    *out_col = -1;
}

static void NearestSelectableCell(const KbGrid* g, int from_row, int from_col, int* out_row,
                                  int* out_col) {
    int best_row = -1;
    int best_col = -1;
    int best_distance = INT_MAX;
    int row, col;
    for (row = 0; row < g->rows; ++row) {
        for (col = 0; col < g->cols; ++col) {
            int distance;
            if (!IsSelectableCell(g, row, col)) {
                continue;
            }
            distance = abs(row - from_row) + abs(col - from_col);
            if (distance < best_distance) {
                best_distance = distance;
                best_row = row;
                best_col = col;
            }
        }
    }
    *out_row = best_row;
    *out_col = best_col;
}

static void NearestSelectableCellOnRow(const KbGrid* g, int from_row, int from_col,
                                       int prefer_col_dir, int* out_row, int* out_col) {
    int best_col = -1;
    int best_distance = INT_MAX;
    bool best_in_direction = false;
    int col;
    *out_row = -1;
    *out_col = -1;
    if (from_row < 0 || from_row >= g->rows) {
        return;
    }
    for (col = 0; col < g->cols; ++col) {
        int distance;
        bool in_direction;
        bool better_tie;
        if (!IsSelectableCell(g, from_row, col)) {
            continue;
        }
        distance = abs(col - from_col);
        in_direction =
            (prefer_col_dir > 0 && col > from_col) || (prefer_col_dir < 0 && col < from_col);
        better_tie = (prefer_col_dir != 0 && in_direction != best_in_direction) ? in_direction
                                                                                : col < best_col;
        if (distance < best_distance ||
            (distance == best_distance && (best_col < 0 || better_tie))) {
            best_distance = distance;
            best_col = col;
            best_in_direction = in_direction;
        }
    }
    if (best_col >= 0) {
        *out_row = from_row;
        *out_col = best_col;
    }
}

/* The cell that shows as selected: the cursor's, or the nearest key when the cursor's cell has
 * none */
static void VisibleCellForCursor(const KbGrid* g, int* out_row, int* out_col) {
    const ImeKbNavState* nav = g->nav;
    if (IsSelectableCell(g, nav->cursor_row, nav->cursor_col)) {
        *out_row = nav->cursor_row;
        *out_col = nav->cursor_col;
        return;
    }
    NearestSelectableCellOnRow(g, nav->cursor_row, nav->cursor_col, nav->fallback_prefer_col_dir,
                               out_row, out_col);
    if (*out_row >= 0 && *out_col >= 0) {
        return;
    }
    NearestSelectableCell(g, nav->cursor_row, nav->cursor_col, out_row, out_col);
}

static void MoveCursor(KbGrid* g, int step_row, int step_col, bool repeat_hint) {
    ImeKbNavState* nav = g->nav;
    int row = nav->cursor_row;
    int col = nav->cursor_col;
    const ImeKbKeySpec* origin_key;
    int max_steps;
    bool crossed_wrap = false;
    double now;
    int i;
    if (step_row == 0 && step_col == 0) {
        return;
    }
    now = ovl_time();
    if (row < 0 || col < 0) {
        FirstSelectableCell(g, &nav->cursor_row, &nav->cursor_col);
        nav->fallback_prefer_col_dir = 0;
        ResetImeEdgeWrapNav(&nav->edge_wrap_nav);
        return;
    }

    origin_key = g->occupied[nav->cursor_row * g->cols + nav->cursor_col];
    if (origin_key && origin_key->action == IME_KB_KEY_ACTION_NONE) {
        origin_key = NULL;
    }
    max_steps = KB_MAX(1, g->rows * g->cols);
    for (i = 0; i < max_steps; ++i) {
        const ImeKbKeySpec* candidate_key;
        crossed_wrap = crossed_wrap || DoesImeKeyboardStepCrossGridEdge(row, col, step_row,
                                                                        step_col, g->rows, g->cols);
        row = (row + step_row + g->rows) % g->rows;
        col = (col + step_col + g->cols) % g->cols;

        candidate_key = g->occupied[row * g->cols + col];
        if (!candidate_key || candidate_key->action == IME_KB_KEY_ACTION_NONE) {
            continue;
        }
        /* Treat each spanned key as one navigation node in every direction. */
        if (origin_key && candidate_key == origin_key) {
            continue;
        }

        if (ShouldDelayImeEdgeWrap(&nav->edge_wrap_nav, step_row, step_col, repeat_hint,
                                   crossed_wrap, now, kEdgeWrapHoldDelaySec,
                                   kRepeatIntentWindowSec)) {
            return;
        }

        nav->cursor_row = row;
        nav->cursor_col = col;
        nav->fallback_prefer_col_dir = step_col;
        CommitImeEdgeWrapStep(&nav->edge_wrap_nav, step_row, step_col, now);
        return;
    }
}

static bool ActionAllowsRepeat(ImeKbKeyAction action) {
    switch (action) {
    case IME_KB_KEY_ACTION_CHARACTER:
    case IME_KB_KEY_ACTION_SPACE:
    case IME_KB_KEY_ACTION_BACKSPACE:
    case IME_KB_KEY_ACTION_ARROW_LEFT:
    case IME_KB_KEY_ACTION_ARROW_RIGHT:
    case IME_KB_KEY_ACTION_ARROW_UP:
    case IME_KB_KEY_ACTION_ARROW_DOWN:
    case IME_KB_KEY_ACTION_NEW_LINE:
        return true;
    default:
        return false;
    }
}

static void DrawSymbolGlyph(const KbGrid* g, float cx, float cy, float glyph_extent,
                            const char* symbol) {
    ovl_font_t* font = ovl_font_current();
    const float symbol_font_size = KB_MAX(8.0f, glyph_extent * 0.62f);
    const ovl_vec2_t symbol_size =
        ovl_font_calc_text_size(font, symbol_font_size, 0.0f, symbol, NULL);
    ovl_add_text(g->draw, font, symbol_font_size,
                 vec2(cx - symbol_size.x * 0.5f, cy - symbol_size.y * 0.5f), g->params->key_text,
                 symbol, NULL, 0.0f);
}

static void DrawShiftGlyph(const KbGrid* g, ovl_vec2_t pos, ovl_vec2_t size, float thickness,
                           bool filled, bool caps_locked) {
    const ovl_col_t color = g->params->key_text;
    const float cx = pos.x + size.x * 0.5f;
    const float cy = pos.y + size.y * 0.5f;
    const float extent = KB_MIN(size.x, size.y);
    const float scale = 0.88f;
    const float y_offset = extent * 0.03f;
    const float tip_y = cy + y_offset - extent * (0.30f * scale);
    const float shoulder_y = cy + y_offset - extent * (0.03f * scale);
    const float base_y = cy + y_offset + extent * (0.21f * scale);
    const float head_half_w = extent * (0.30f * scale);
    const float stem_half_w = extent * (0.11f * scale);
    ovl_vec2_t shape[7];
    shape[0] = vec2(cx, tip_y);
    shape[1] = vec2(cx - head_half_w, shoulder_y);
    shape[2] = vec2(cx - stem_half_w, shoulder_y);
    shape[3] = vec2(cx - stem_half_w, base_y);
    shape[4] = vec2(cx + stem_half_w, base_y);
    shape[5] = vec2(cx + stem_half_w, shoulder_y);
    shape[6] = vec2(cx + head_half_w, shoulder_y);

    if (filled) {
        ovl_add_convex_poly_filled(g->draw, shape, 7, color);
    } else {
        ovl_add_polyline(g->draw, shape, 7, color, OVL_CLOSED, thickness);
    }

    if (caps_locked) {
        const float marker_w = extent * (0.24f * scale);
        const float marker_h = KB_MAX(thickness * 1.25f, extent * 0.07f);
        const float marker_y =
            KB_MIN(pos.y + size.y - marker_h * 0.9f, base_y + extent * (0.10f * scale));
        ovl_add_rect_filled(g->draw, vec2(cx - marker_w * 0.5f, marker_y - marker_h * 0.5f),
                            vec2(cx + marker_w * 0.5f, marker_y + marker_h * 0.5f), color,
                            marker_h * 0.45f, 0);
    }
}

static void DrawKeyGlyph(const KbGrid* g, ovl_vec2_t pos, ovl_vec2_t size, ImeKbKeyGlyph glyph) {
    const float glyph_extent = KB_MIN(size.x, size.y);
    const float thickness = KB_MAX(1.2f, glyph_extent * 0.05f);
    const ovl_col_t color = g->params->key_text;
    const float cx = pos.x + size.x * 0.5f;
    const float cy = pos.y + size.y * 0.5f;

    switch (glyph) {
    case IME_KB_KEY_GLYPH_BACKSPACE: {
        const float body_w = glyph_extent * 0.56f;
        const float body_h = glyph_extent * 0.42f;
        const float tip_w = glyph_extent * 0.28f;
        const float left = cx - (body_w - tip_w) * 0.5f;
        const float right = left + body_w;
        const float top = cy - body_h * 0.5f;
        const float bottom = cy + body_h * 0.5f;
        const float cross_half = glyph_extent * 0.10f;
        const float cross_cx = left + body_w * 0.57f;
        ovl_vec2_t frame[5];
        frame[0] = vec2(left, top);
        frame[1] = vec2(right, top);
        frame[2] = vec2(right, bottom);
        frame[3] = vec2(left, bottom);
        frame[4] = vec2(left - tip_w, cy);
        ovl_add_polyline(g->draw, frame, 5, color, OVL_CLOSED, thickness);
        ovl_add_line(g->draw, vec2(cross_cx - cross_half, cy - cross_half),
                     vec2(cross_cx + cross_half, cy + cross_half), color, thickness);
        ovl_add_line(g->draw, vec2(cross_cx + cross_half, cy - cross_half),
                     vec2(cross_cx - cross_half, cy + cross_half), color, thickness);
        break;
    }
    case IME_KB_KEY_GLYPH_ARROW_LEFT:
        DrawSymbolGlyph(g, cx, cy, glyph_extent, kArrowLabelLeft);
        break;
    case IME_KB_KEY_GLYPH_ARROW_RIGHT:
        DrawSymbolGlyph(g, cx, cy, glyph_extent, kArrowLabelRight);
        break;
    case IME_KB_KEY_GLYPH_ARROW_UP:
        DrawSymbolGlyph(g, cx, cy, glyph_extent, kArrowLabelUp);
        break;
    case IME_KB_KEY_GLYPH_ARROW_DOWN:
        DrawSymbolGlyph(g, cx, cy, glyph_extent, kArrowLabelDown);
        break;
    case IME_KB_KEY_GLYPH_SHIFT_OUTLINE:
        DrawShiftGlyph(g, pos, size, thickness, false, false);
        break;
    case IME_KB_KEY_GLYPH_SHIFT_FILLED:
        DrawShiftGlyph(g, pos, size, thickness, true, false);
        break;
    case IME_KB_KEY_GLYPH_CAPS_LOCK_FILLED:
        DrawShiftGlyph(g, pos, size, thickness, true, true);
        break;
    case IME_KB_KEY_GLYPH_NONE:
    default:
        break;
    }
}

static void DrawHotkeyBadge(const KbGrid* g, ovl_vec2_t pos, ovl_vec2_t size, ovl_font_t* font,
                            float base_font_size, const char* hotkey_label) {
    const float hotkey_padding_x = KB_MAX(3.0f, size.y * 0.08f);
    const float hotkey_max_w = KB_MAX(0.0f, size.x - hotkey_padding_x * 2.0f);
    float hotkey_font_size = KB_MAX(6.0f, base_font_size * kHotkeyLabelScale);
    ovl_vec2_t hotkey_size;
    ovl_vec2_t hotkey_pos;
    ovl_vec2_t bg_min;
    ovl_vec2_t bg_max;
    float bg_pad_x;
    float bg_pad_bottom;
    float bg_rounding;
    if (!hotkey_label || hotkey_label[0] == '\0') {
        return;
    }
    hotkey_size = ovl_font_calc_text_size(font, hotkey_font_size, 0.0f, hotkey_label, NULL);
    if (hotkey_max_w > 0.0f && hotkey_size.x > hotkey_max_w && hotkey_size.x > 0.0f) {
        hotkey_font_size *= hotkey_max_w / hotkey_size.x;
        hotkey_size = ovl_font_calc_text_size(font, hotkey_font_size, 0.0f, hotkey_label, NULL);
    }
    hotkey_pos = vec2(pos.x + hotkey_padding_x, pos.y);
    bg_pad_x = KB_MAX(2.0f, hotkey_font_size * 0.28f);
    bg_pad_bottom = KB_MAX(1.0f, hotkey_font_size * 0.16f);
    bg_min = vec2(hotkey_pos.x - bg_pad_x, pos.y);
    bg_max =
        vec2(hotkey_pos.x + hotkey_size.x + bg_pad_x, hotkey_pos.y + hotkey_size.y + bg_pad_bottom);
    bg_rounding = KB_MAX(2.0f, hotkey_font_size * 0.24f);
    ovl_add_rect_filled(g->draw, bg_min, bg_max, OVL_COL(10, 10, 10, 132), bg_rounding, 0);
    ovl_add_rect(g->draw, bg_min, bg_max, OVL_COL(92, 92, 92, 105), bg_rounding, 0, 1.0f);
    ovl_add_text(g->draw, font, hotkey_font_size, hotkey_pos, g->params->key_hotkey_text,
                 hotkey_label, NULL, 0.0f);
}

static void DrawKey(const KbGrid* g, const RenderedKey* key, float selection_alpha,
                    float pulse_expand_px) {
    const ovl_vec2_t pos = key->pos;
    const ovl_vec2_t size = key->size;
    const char* label = key->label;
    const float clamped_selection_alpha = clamp_float(selection_alpha, 0.0f, 1.0f);
    const bool selected = clamped_selection_alpha > 0.0f;
    const float expand = KB_MAX(0.0f, pulse_expand_px);
    const float draw_corner_radius = g->layout->corner_radius + expand;
    const bool emphasize_main_label = key->key && key->key->action == IME_KB_KEY_ACTION_CHARACTER;
    ovl_col_t bg = key->bg;
    ovl_col_t border_color = g->params->key_border;
    ovl_vec2_t draw_min;
    ovl_vec2_t draw_max;
    ovl_font_t* font;
    float base_font_size;

    if (selected) {
        const float selection_boost = 0.11f * clamped_selection_alpha;
        ovl_vec4_t selected_bg = ovl_vec4_from_col(bg);
        selected_bg.x = KB_MIN(1.0f, selected_bg.x + selection_boost);
        selected_bg.y = KB_MIN(1.0f, selected_bg.y + selection_boost);
        selected_bg.z = KB_MIN(1.0f, selected_bg.z + selection_boost);
        bg = ovl_col_from_vec4(selected_bg);
    }
    draw_min = vec2(pos.x - expand, pos.y - expand);
    draw_max = vec2(pos.x + size.x + expand, pos.y + size.y + expand);
    ovl_add_rect_filled(g->draw, draw_min, draw_max, bg, draw_corner_radius, 0);
    if (key->is_done) {
        ovl_vec4_t done_border = ovl_vec4_from_col(bg);
        done_border.x = clamp_float(done_border.x * 0.72f, 0.0f, 1.0f);
        done_border.y = clamp_float(done_border.y * 0.72f, 0.0f, 1.0f);
        done_border.z = clamp_float(done_border.z * 0.72f, 0.0f, 1.0f);
        done_border.w = 1.0f;
        border_color = ovl_col_from_vec4(done_border);
    }
    if (selected) {
        border_color = OVL_COL(248, 248, 248, 255);
        if (clamped_selection_alpha < 1.0f) {
            ovl_vec4_t border = ovl_vec4_from_col(border_color);
            border.w *= clamped_selection_alpha;
            border_color = ovl_col_from_vec4(border);
        }
    }
    ovl_add_rect(g->draw, draw_min, draw_max, border_color, draw_corner_radius, 0,
                 selected ? 2.0f : 1.0f);
    font = ovl_font_current();
    base_font_size = ovl_font_current_size();
    if ((!label || label[0] == '\0') && key->glyph != IME_KB_KEY_GLYPH_NONE) {
        DrawKeyGlyph(g, pos, size, key->glyph);
    }
    if (label && label[0] != '\0') {
        float label_font_size =
            base_font_size * (emphasize_main_label ? kTypingKeyLabelScale : kFunctionKeyLabelScale);
        const float label_padding_x = KB_MAX(4.0f, size.x * 0.08f);
        const float label_max_w = KB_MAX(0.0f, size.x - label_padding_x * 2.0f);
        ovl_vec2_t text_size = ovl_font_calc_text_size(font, label_font_size, 0.0f, label, NULL);
        if (label_max_w > 0.0f && text_size.x > label_max_w && text_size.x > 0.0f) {
            label_font_size *= label_max_w / text_size.x;
            text_size = ovl_font_calc_text_size(font, label_font_size, 0.0f, label, NULL);
        }
        ovl_add_text(
            g->draw, font, label_font_size,
            vec2(pos.x + (size.x - text_size.x) * 0.5f, pos.y + (size.y - text_size.y) * 0.5f),
            g->params->key_text, label, NULL, 0.0f);
    }
    /* Draw hotkey tag last so it always stays over main key label/glyph. */
    DrawHotkeyBadge(g, pos, size, font, base_font_size, key->hotkey_label);
}

/* Lays the layout's keys out on the grid: which key each cell belongs to, and the keys as they
 * are drawn */
static void BuildGrid(KbGrid* g, const ImeKbLayoutModel* model) {
    const ImeKbGridLayout* layout = g->layout;
    const ImeKbDrawParams* params = g->params;
    const float key_w = (layout->size.x - layout->key_gap_x * (g->cols - 1)) / g->cols;
    float y_cursor = layout->pos.y;
    size_t i;
    int row, col;

    for (row = 0; row < g->rows; ++row) {
        g->row_heights[row] = layout->key_h;
    }
    if (g->fixed_bottom_rows > 0 && layout->bottom_row_h > 0.0f) {
        for (row = g->rows - g->fixed_bottom_rows; row < g->rows; ++row) {
            g->row_heights[row] = layout->bottom_row_h;
        }
    }
    for (row = 0; row < g->rows; ++row) {
        g->row_offsets[row] = y_cursor;
        y_cursor += g->row_heights[row];
        if (row + 1 < g->rows) {
            y_cursor += layout->key_gap_y;
        }
    }

    for (row = 0; row < g->rows * g->cols; ++row) {
        g->occupied[row] = NULL;
        g->is_anchor[row] = false;
    }
    for (i = 0; i < model->key_count; ++i) {
        const ImeKbKeySpec* key = &model->keys[i];
        int row_end, col_end;
        if (key->col_span == 0 || key->row_span == 0) {
            continue;
        }
        if (key->row >= g->rows || key->col >= g->cols) {
            continue;
        }
        row_end = KB_MIN(g->rows, (int)(key->row + key->row_span));
        col_end = KB_MIN(g->cols, (int)(key->col + key->col_span));
        g->is_anchor[key->row * g->cols + key->col] = true;
        for (row = key->row; row < row_end; ++row) {
            for (col = key->col; col < col_end; ++col) {
                g->occupied[row * g->cols + col] = key;
            }
        }
    }

    g->rendered_count = 0;
    for (row = 0; row < g->rows; ++row) {
        for (col = 0; col < g->cols; ++col) {
            const int cell_index = row * g->cols + col;
            const ImeKbKeySpec* key = g->occupied[cell_index];
            RenderedKey* out;
            int col_span = 1;
            int row_span = 1;
            if (key && !g->is_anchor[cell_index]) {
                continue;
            }
            if (key) {
                col_span = KB_MIN(g->cols - col, (int)key->col_span);
                row_span = KB_MIN(g->rows - row, (int)key->row_span);
            }

            out = &g->rendered[g->rendered_count++];
            out->key = key;
            out->row = row;
            out->col = col;
            out->pos = vec2(layout->pos.x + col * (key_w + layout->key_gap_x), g->row_offsets[row]);
            out->size = vec2(key_w * col_span + layout->key_gap_x * (col_span - 1),
                             SpanRowHeight(g, row, row_span));
            out->center = vec2(out->pos.x + out->size.x * 0.5f, out->pos.y + out->size.y * 0.5f);
            out->bg = SlotBaseBg(g, row);
            out->label = NULL;
            out->hotkey_label = NULL;
            out->glyph = IME_KB_KEY_GLYPH_NONE;
            out->is_done = false;
            out->selectable = key && key->action != IME_KB_KEY_ACTION_NONE;
            if (key) {
                const bool symbols_layout =
                    params->selection.family == IME_KB_LAYOUT_FAMILY_SYMBOLS;
                const bool typing_key = key->action == IME_KB_KEY_ACTION_CHARACTER;
                if (symbols_layout && typing_key) {
                    out->bg = params->key_bg_symbol;
                } else if (!typing_key && key->action != IME_KB_KEY_ACTION_NONE) {
                    out->bg = params->key_bg_function;
                } else if (typing_key) {
                    out->bg = params->key_bg_default;
                }
                out->is_done = key->action == IME_KB_KEY_ACTION_DONE;
                out->hotkey_label = key->hotkey_label;
                if (out->is_done) {
                    out->bg = params->key_done;
                    out->label = GetEnterLabel(params->enter_label);
                    out->glyph = key->glyph;
                } else {
                    out->label = ResolveSymbolOverrideLabel(
                        &params->selection, params->supported_languages, key, key->label);
                    out->label = ResolveShiftOverrideLabel(&params->selection, key, out->label);
                    out->glyph = ResolveShiftOverrideGlyph(&params->selection, key, key->glyph);
                }
            }
        }
    }
}

/* Moves the cursor by the pad's direction keys and the moves the caller asks for */
static void HandleNavInput(KbGrid* g) {
    const ImeKbDrawParams* params = g->params;
    const bool stick = params->use_lstick_key_nav;
    const bool left_once = ovl_is_key_pressed(OVL_KEY_PAD_DPAD_LEFT, false) ||
                           (stick && ovl_is_key_pressed(OVL_KEY_PAD_LSTICK_LEFT, false));
    const bool right_once = ovl_is_key_pressed(OVL_KEY_PAD_DPAD_RIGHT, false) ||
                            (stick && ovl_is_key_pressed(OVL_KEY_PAD_LSTICK_RIGHT, false));
    const bool up_once = ovl_is_key_pressed(OVL_KEY_PAD_DPAD_UP, false) ||
                         (stick && ovl_is_key_pressed(OVL_KEY_PAD_LSTICK_UP, false));
    const bool down_once = ovl_is_key_pressed(OVL_KEY_PAD_DPAD_DOWN, false) ||
                           (stick && ovl_is_key_pressed(OVL_KEY_PAD_LSTICK_DOWN, false));
    const bool left_repeat =
        !left_once && (ovl_is_key_pressed(OVL_KEY_PAD_DPAD_LEFT, true) ||
                       (stick && ovl_is_key_pressed(OVL_KEY_PAD_LSTICK_LEFT, true)));
    const bool right_repeat =
        !right_once && (ovl_is_key_pressed(OVL_KEY_PAD_DPAD_RIGHT, true) ||
                        (stick && ovl_is_key_pressed(OVL_KEY_PAD_LSTICK_RIGHT, true)));
    const bool up_repeat = !up_once && (ovl_is_key_pressed(OVL_KEY_PAD_DPAD_UP, true) ||
                                        (stick && ovl_is_key_pressed(OVL_KEY_PAD_LSTICK_UP, true)));
    const bool down_repeat =
        !down_once && (ovl_is_key_pressed(OVL_KEY_PAD_DPAD_DOWN, true) ||
                       (stick && ovl_is_key_pressed(OVL_KEY_PAD_LSTICK_DOWN, true)));

    if (left_once || left_repeat || params->external_nav_left) {
        MoveCursor(g, 0, -1,
                   left_repeat || (params->external_nav_left && params->external_nav_left_repeat));
    } else if (right_once || right_repeat || params->external_nav_right) {
        MoveCursor(g, 0, 1,
                   right_repeat ||
                       (params->external_nav_right && params->external_nav_right_repeat));
    } else if (up_once || up_repeat || params->external_nav_up) {
        MoveCursor(g, -1, 0,
                   up_repeat || (params->external_nav_up && params->external_nav_up_repeat));
    } else if (down_once || down_repeat || params->external_nav_down) {
        MoveCursor(g, 1, 0,
                   down_repeat || (params->external_nav_down && params->external_nav_down_repeat));
    }
}

void DrawImeKeyboardGrid(const ImeKbGridLayout* layout, const ImeKbDrawParams* params,
                         ImeKbDrawState* state) {
    KbGrid grid;
    KbGrid* g = &grid;
    ImeKbNavState* nav;
    const ImeKbLayoutModel* model;
    bool key_activate_once;
    bool key_activate_repeat;
    int selected_render_index = -1;
    int visible_row, visible_col;
    int highlighted_row = -1;
    int highlighted_col = -1;
    float faded_selection_alpha = 0.0f;
    bool activate_selected = false;
    bool activate_selected_pulse = false;
    int i;

    state->done_pressed = false;
    state->pressed_action = IME_KB_KEY_ACTION_NONE;
    state->pressed_label = NULL;
    state->pressed_keycode = 0;
    state->pressed_character = 0;
    state->selected_row = -1;
    state->selected_col = -1;
    state->selected_center = vec2(0.0f, 0.0f);

    g->draw = ovl_window_draw_list();
    if (!g->draw || layout->cols <= 0 || layout->rows <= 0 || layout->size.x <= 0.0f ||
        layout->size.y <= 0.0f || layout->rows > IME_KB_GRID_ROWS_MAX ||
        layout->cols > IME_KB_GRID_COLS_MAX) {
        return;
    }

    model = params->layout_model;
    if (!model) {
        model = GetImeKeyboardLayout(&params->selection);
    }
    if (!model || !model->keys || model->key_count == 0) {
        model = &kLatinLowerModel;
    }

    g->layout = layout;
    g->params = params;
    g->rows = layout->rows;
    g->cols = layout->cols;
    g->fixed_bottom_rows = clamp_int(layout->fixed_bottom_rows, 0, g->rows);
    BuildGrid(g, model);

    nav = NavStateFor(ovl_get_id("##ImeKbGridNav"));
    g->nav = nav;
    if (nav->last_grid_rows != g->rows || nav->last_grid_cols != g->cols) {
        nav->last_grid_rows = g->rows;
        nav->last_grid_cols = g->cols;
        ResetImeEdgeWrapNav(&nav->edge_wrap_nav);
        ClearSelectionFade(nav);
    }
    if (params->reset_nav_state) {
        ResetImeEdgeWrapNav(&nav->edge_wrap_nav);
        ClearSelectionFade(nav);
    }

    if (params->requested_selected_row >= 0 && params->requested_selected_col >= 0) {
        const int row = clamp_int(params->requested_selected_row, 0, KB_MAX(0, g->rows - 1));
        const int col = clamp_int(params->requested_selected_col, 0, KB_MAX(0, g->cols - 1));
        int req_row, req_col;
        NearestSelectableCellOnRow(g, row, col, 0, &req_row, &req_col);
        if (req_row < 0 || req_col < 0) {
            NearestSelectableCell(g, row, col, &req_row, &req_col);
        }
        if (req_row >= 0 && req_col >= 0) {
            nav->cursor_row = req_row;
            nav->cursor_col = req_col;
            nav->fallback_prefer_col_dir = 0;
            ResetImeEdgeWrapNav(&nav->edge_wrap_nav);
        }
    }

    if (nav->cursor_row < 0 || nav->cursor_row >= g->rows || nav->cursor_col < 0 ||
        nav->cursor_col >= g->cols) {
        const int anchor_row = clamp_int(nav->cursor_row, 0, KB_MAX(0, g->rows - 1));
        const int anchor_col = clamp_int(nav->cursor_col, 0, KB_MAX(0, g->cols - 1));
        int row, col;
        NearestSelectableCell(g, anchor_row, anchor_col, &row, &col);
        if (row < 0 || col < 0) {
            FirstSelectableCell(g, &row, &col);
        }
        nav->cursor_row = row;
        nav->cursor_col = col;
        nav->fallback_prefer_col_dir = 0;
        ResetImeEdgeWrapNav(&nav->edge_wrap_nav);
    }

    if (params->allow_nav_input) {
        HandleNavInput(g);
    }

    key_activate_once = ovl_is_key_pressed(OVL_KEY_PAD_FACE_DOWN, false);
    key_activate_repeat = ovl_is_key_pressed(OVL_KEY_PAD_FACE_DOWN, true);

    VisibleCellForCursor(g, &visible_row, &visible_col);
    if (IsSelectableCell(g, visible_row, visible_col)) {
        const ImeKbKeySpec* selected_spec = g->occupied[visible_row * g->cols + visible_col];
        for (i = 0; i < g->rendered_count; ++i) {
            if (g->rendered[i].key == selected_spec && g->rendered[i].selectable) {
                selected_render_index = i;
                break;
            }
        }
    }
    state->selected_row = nav->cursor_row;
    state->selected_col = nav->cursor_col;
    if (selected_render_index >= 0) {
        state->selected_center = g->rendered[selected_render_index].center;
    }

    if (params->show_selection_highlight && selected_render_index >= 0) {
        highlighted_row = g->rendered[selected_render_index].row;
        highlighted_col = g->rendered[selected_render_index].col;
    }
    if (highlighted_row != nav->highlighted_row || highlighted_col != nav->highlighted_col) {
        if (nav->highlighted_row >= 0 && nav->highlighted_col >= 0) {
            nav->fade_row = nav->highlighted_row;
            nav->fade_col = nav->highlighted_col;
            nav->fade_started_at = ovl_time();
        } else {
            nav->fade_row = -1;
            nav->fade_col = -1;
            nav->fade_started_at = 0.0;
        }
        nav->highlighted_row = highlighted_row;
        nav->highlighted_col = highlighted_col;
    }

    if (nav->fade_row >= 0 && nav->fade_col >= 0) {
        const double elapsed = ovl_time() - nav->fade_started_at;
        if (elapsed >= 0.0 && elapsed < kSelectionFadeOutDurationSec) {
            faded_selection_alpha = 1.0f - (float)(elapsed / kSelectionFadeOutDurationSec);
        } else {
            nav->fade_row = -1;
            nav->fade_col = -1;
            nav->fade_started_at = 0.0;
        }
    }

    if (params->allow_activate_input) {
        bool activate_once = key_activate_once;
        bool activate_repeat = key_activate_repeat && !key_activate_once;
        if (!activate_once && params->external_activate_pressed) {
            if (params->external_activate_repeat) {
                activate_repeat = true;
            } else {
                activate_once = true;
            }
        }

        if (selected_render_index >= 0) {
            const RenderedKey* selected_key = &g->rendered[selected_render_index];
            const ImeKbKeyAction action = (selected_key->key && selected_key->selectable)
                                              ? selected_key->key->action
                                              : IME_KB_KEY_ACTION_NONE;
            if (!ActionAllowsRepeat(action)) {
                activate_repeat = false;
            }
        }

        activate_selected = activate_once || activate_repeat;
        activate_selected_pulse = activate_once;
    }

    for (i = 0; i < g->rendered_count; ++i) {
        const RenderedKey* key = &g->rendered[i];
        const bool selected = params->show_selection_highlight && (i == selected_render_index);
        float selection_alpha = selected ? 1.0f : 0.0f;
        float press_pulse_expand = 0.0f;
        if (!selected && faded_selection_alpha > 0.0f && key->row == nav->fade_row &&
            key->col == nav->fade_col) {
            selection_alpha = faded_selection_alpha;
        }
        if (selected && key->row == nav->press_pulse_row && key->col == nav->press_pulse_col) {
            press_pulse_expand = ComputePressPulseExpand(
                nav->press_pulse_started_at, ovl_time(), kSelectorPressPulseDurationSec,
                kSelectedBorderThickness * kSelectorPressPulseExpandBorderFactor);
        }
        DrawKey(g, key, selection_alpha, press_pulse_expand);
    }

    if (activate_selected && selected_render_index >= 0) {
        const RenderedKey* key = &g->rendered[selected_render_index];
        if (key->key && key->selectable) {
            if (activate_selected_pulse) {
                nav->press_pulse_row = key->row;
                nav->press_pulse_col = key->col;
                nav->press_pulse_started_at = ovl_time();
            }
            state->pressed_action = key->key->action;
            state->pressed_label = key->label;
            state->pressed_keycode = ResolveImeKeycode(key->key, key->label);
            state->pressed_character = ResolveImeCharacter(key->key, key->label);
            if (key->is_done) {
                state->done_pressed = true;
            }
        }
    }
}
