// SPDX-FileCopyrightText: 2013 Dolphin Emulator Project
// SPDX-FileCopyrightText: 2014 Citra Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <string>
#include <vector>
#include "common/logging/log.h"

namespace Common {

/// Make a string lowercase
[[nodiscard]] std::string ToLower(std::string_view str);

void ToLowerInPlace(std::string& str);

std::vector<std::string> SplitString(const std::string& str, char delimiter);

std::string_view U8stringToString(std::u8string_view u8str);

#ifdef _WIN32
[[nodiscard]] std::string UTF16ToUTF8(std::wstring_view input);
[[nodiscard]] std::wstring UTF8ToUTF16W(std::string_view str);
#endif

/// @value in binary, at least @digits of them (zeros before)
[[nodiscard]] std::string BinaryText(unsigned long long value, int digits);

/// The text a printf format makes of its arguments
[[nodiscard]] std::string StringFromFormat(const char* format, ...) SHAD_PRINTF(1, 2);

} // namespace Common
