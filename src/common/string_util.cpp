// SPDX-FileCopyrightText: 2013 Dolphin Emulator Project
// SPDX-FileCopyrightText: 2014 Citra Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include "common/string_util.h"

namespace Common {

namespace {

char LowerAscii(char c) {
    return (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c;
}

} // Anonymous namespace

void ToLowerInPlace(char* str, size_t len) {
    for (size_t i = 0; i < len; i++) {
        str[i] = LowerAscii(str[i]);
    }
}

bool EqualsIgnoreCase(const char* a, size_t a_len, const char* b, size_t b_len) {
    if (a_len != b_len) {
        return false;
    }
    for (size_t i = 0; i < a_len; i++) {
        if (LowerAscii(a[i]) != LowerAscii(b[i])) {
            return false;
        }
    }
    return true;
}

u32 SplitString(const char* str, size_t len, char delimiter, StringPiece* out, u32 max) {
    u32 count = 0;
    size_t start = 0;
    while (start < len) {
        const void* hit = memchr(str + start, delimiter, len - start);
        const size_t end = hit ? static_cast<size_t>(static_cast<const char*>(hit) - str) : len;
        if (count < max) {
            out[count].data = str + start;
            out[count].size = end - start;
        }
        count++;
        start = end + 1;
    }
    return count;
}

std::string StringFromFormat(const char* format, ...) {
    va_list args;
    va_start(args, format);
    va_list again;
    va_copy(again, args);
    const int n = vsnprintf(nullptr, 0, format, args);
    va_end(args);
    std::string text;
    if (n > 0) {
        text.resize(static_cast<size_t>(n));
        vsnprintf(text.data(), text.size() + 1, format, again);
    }
    va_end(again);
    return text;
}

BinaryTextBuf BinaryText(unsigned long long value, int digits) {
    char bits[64];
    int n = 0;
    do {
        bits[n++] = static_cast<char>('0' + (value & 1));
        value >>= 1;
    } while (value != 0 && n < 64);
    while (n < digits && n < 64) {
        bits[n++] = '0';
    }
    BinaryTextBuf text;
    for (int i = 0; i < n; i++) {
        text.s[i] = bits[n - 1 - i];
    }
    text.s[n] = '\0';
    return text;
}

} // namespace Common
