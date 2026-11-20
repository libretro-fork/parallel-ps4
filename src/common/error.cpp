// SPDX-FileCopyrightText: 2013 Dolphin Emulator Project
// SPDX-FileCopyrightText: 2014 Citra Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <stddef.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <errno.h>
#include <string.h>
#endif

#include "common/error.h"

namespace Common {

namespace {

/// @e in words, into @out
const char* NativeErrorToString(int e, char* out, size_t size) {
#ifdef _WIN32
    const DWORD res = FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                                     nullptr, e, MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US), out,
                                     static_cast<DWORD>(size), nullptr);
    if (!res) {
        return "(FormatMessageA failed to format error)";
    }
    return out;
#else
#if defined(__GLIBC__) && (_GNU_SOURCE || (_POSIX_C_SOURCE < 200112L && _XOPEN_SOURCE < 600)) ||   \
    defined(ANDROID)
    // Thread safe (GNU-specific)
    return strerror_r(e, out, size);
#else
    // Thread safe (XSI-compliant)
    if (strerror_r(e, out, size) != 0) {
        return "(strerror_r failed to format error)";
    }
    return out;
#endif // GLIBC etc.
#endif // _WIN32
}

} // Anonymous namespace

const char* GetLastErrorMsg() {
    static thread_local char message[256];
#ifdef _WIN32
    return NativeErrorToString(static_cast<int>(GetLastError()), message, sizeof(message));
#else
    return NativeErrorToString(errno, message, sizeof(message));
#endif
}

} // namespace Common
