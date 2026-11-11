// SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <mutex>
#include <optional>
#include "common/combiner.h"
#include "common/event_count.h"
#include "common/types.h"

namespace Libraries::Np::NpTus {

struct TusRequestCtx {
    Common::Combiner mutex;
    Common::EventCount cv; // notified once the result is set
    std::optional<s32> result;

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

    // True once the operation has completed (or been aborted).
    bool HasResult() {
        return mutex.Run([&]() -> bool { return result.has_value(); });
    }

    // Block until the handler (or an abort/delete) sets the result.
    s32 Wait() {
        cv.Wait([this] { return mutex.Run([this] { return result.has_value(); }); });
        return mutex.Run([this] { return *result; });
    }
};

} // namespace Libraries::Np::NpTus
