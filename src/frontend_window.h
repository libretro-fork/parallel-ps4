// SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <atomic>

#include "common/types.h"

namespace Frontend {

// What the emulator keeps of a window in the core, where the frontend shows the picture and
// there is no window of the system's: the size of the picture - the Resolution option's, or with
// Resolution: Default the game's own (the presenter sets it) - read by the presenter and ImGui on
// the renderer's thread.
class Window {
public:
    Window(s32 width_, s32 height_) : width{width_}, height{height_} {}

    s32 GetWidth() const {
        return width;
    }

    s32 GetHeight() const {
        return height;
    }

    void SetSize(s32 width_, s32 height_) {
        width = width_;
        height = height_;
    }

private:
    s32 width;
    s32 height;
};

// The emulator is asked to end (IPC's STOP, the quit dialog). Its main thread, waiting in
// WaitForQuit once the game runs, ends the process the way it ends on the core's word.
inline std::atomic<bool> quit_requested{false};

inline void RequestQuit() {
    quit_requested.store(true, std::memory_order_release);
    quit_requested.notify_all();
}

inline void WaitForQuit() {
    quit_requested.wait(false, std::memory_order_acquire);
}

} // namespace Frontend
