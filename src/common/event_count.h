// SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <rthreads/retro_eventcount.h>

namespace Common {

/// A libretro-common retro_eventcount that sets itself up and frees itself, for a member.
class EventCount {
public:
    EventCount() {
        retro_eventcount_init(&ec);
    }
    ~EventCount() {
        retro_eventcount_free(&ec);
    }
    EventCount(const EventCount&) = delete;
    EventCount& operator=(const EventCount&) = delete;

    retro_eventcount_t* get() {
        return &ec;
    }

    void Notify() {
        retro_eventcount_notify(&ec);
    }

    /// Parks until @ready() is true, rechecking after every notify.
    template <typename F>
    void Wait(F&& ready) {
        while (!ready()) {
            const int key = retro_eventcount_prepare_wait(&ec);
            if (ready()) {
                retro_eventcount_cancel_wait(&ec);
                return;
            }
            retro_eventcount_commit_wait(&ec, key);
        }
    }

private:
    retro_eventcount_t ec{};
};

} // namespace Common
