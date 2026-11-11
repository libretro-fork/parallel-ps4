// SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <retro_atomic.h>

#include "common/event_count.h"
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
inline retro_atomic_int_t quit_requested{0};
inline Common::EventCount quit_event;

inline void RequestQuit() {
    retro_atomic_store_release_int(&quit_requested, 1);
    quit_event.Notify();
}

inline void WaitForQuit() {
    quit_event.Wait([] { return retro_atomic_load_acquire_int(&quit_requested) != 0; });
}

} // namespace Frontend
