// SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <cstdlib>

#if defined(__MINGW32__) && !defined(_UCRT)
#include <atomic>
#include <stdlib.h>
#endif

namespace Common {

#if defined(__MINGW32__) && !defined(_UCRT)

// msvcrt, which MSYS2's MINGW64 links against, has no quick_exit: its handlers are kept here,
// as many as the C runtime promises (32), and run the same way, newest first, before the
// process ends without running static destructors or atexit handlers.
namespace Detail {
inline void (*quick_exit_handlers[32])() = {};
inline std::atomic<int> quick_exit_count{0};
} // namespace Detail

inline int AtQuickExit(void (*handler)()) {
    const int slot = Detail::quick_exit_count.fetch_add(1);
    if (slot >= 32) {
        Detail::quick_exit_count.fetch_sub(1);
        return -1;
    }
    Detail::quick_exit_handlers[slot] = handler;
    return 0;
}

[[noreturn]] inline void QuickExit(int status) {
    for (int slot = Detail::quick_exit_count.load(); slot-- > 0;) {
        if (Detail::quick_exit_handlers[slot]) {
            Detail::quick_exit_handlers[slot]();
        }
    }
    _exit(status);
}

#else

inline int AtQuickExit(void (*handler)()) {
    return std::at_quick_exit(handler);
}

[[noreturn]] inline void QuickExit(int status) {
    std::quick_exit(status);
}

#endif

} // namespace Common
