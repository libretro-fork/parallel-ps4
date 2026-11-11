// SPDX-FileCopyrightText: Copyright 2025-2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <mutex>
#include <optional>
#include "common/combiner.h"
#include "common/event_count.h"
#include "common/types.h"

namespace Libraries::Np::NpScore {

// Per-request state shared between np_score.cpp (creator/waiter) and the
// handler that fulfils the request (np_handler.cpp)
struct ScoreRequestCtx {
    Common::Combiner mutex;
    Common::EventCount cv;     // notified once the result is set
    std::optional<s32> result; // nullopt = still pending

    s32 titleCtxId = 0;
    s32 userId = -1;
    s32 pcId = 0;
    u32* tmpRankOut = nullptr;

    void SetResult(s32 r) {
        if (mutex.Run([&] {
                if (result.has_value()) {
                    return true;
                }
                result = r;
                return false;
            })) {
            return;
        }
        cv.Notify();
    }

    // Block until result is set. Used by the synced functions
    s32 Wait() {
        cv.Wait([this] { return mutex.Run([this] { return result.has_value(); }); });
        return mutex.Run([this] { return *result; });
    }
};

} // namespace Libraries::Np::NpScore
