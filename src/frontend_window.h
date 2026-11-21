// SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <retro_atomic.h>

#include "common/event_count.h"
#include "common/types.h"

namespace Frontend {

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
