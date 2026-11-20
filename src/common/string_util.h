// SPDX-FileCopyrightText: 2013 Dolphin Emulator Project
// SPDX-FileCopyrightText: 2014 Citra Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <string>
#include <stddef.h>
#include "common/logging/log.h"
#include "common/types.h"

namespace Common {

/// Makes @len bytes of @str lowercase (ASCII; other bytes stay)
void ToLowerInPlace(char* str, size_t len);

/// Whether two strings are the same once lowercased (ASCII)
[[nodiscard]] bool EqualsIgnoreCase(const char* a, size_t a_len, const char* b, size_t b_len);

/// A piece of a string, borrowed from it
struct StringPiece {
    const char* data;
    size_t size;
};

/// Splits @len bytes of @str at @delimiter, like reading it with getline: an empty last piece
/// is dropped. Fills up to @max pieces of @out and gives how many there are in all.
u32 SplitString(const char* str, size_t len, char delimiter, StringPiece* out, u32 max);

/// The text of @value in binary, in BinaryTextBuf::s
struct BinaryTextBuf {
    char s[65];
};

/// @value in binary, at least @digits of them (zeros before)
[[nodiscard]] BinaryTextBuf BinaryText(unsigned long long value, int digits);

/// The text a printf format makes of its arguments.
/// Stays a std::string until its callers convert.
[[nodiscard]] std::string StringFromFormat(const char* format, ...) SHAD_PRINTF(1, 2);

} // namespace Common
