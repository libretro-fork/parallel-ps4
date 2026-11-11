// SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <mutex>
#include <optional>

#include "common/combiner.h"
#include "common/event_count.h"
#include "common/types.h"
#include "core/libraries/np/np_types.h"

namespace Libraries::Np::NpUtility {

// sceNpLookupSetTimeout() values
struct LookupTimeouts {
    s32 resolveRetry = 0;
    u32 resolveTimeout = 0;
    u32 connTimeout = 0;
    u32 sendTimeout = 0;
    u32 recvTimeout = 0;
};

struct LookupTitleCtx {
    s32 userId = -1;
    OrbisNpId selfNpId{};
    std::optional<LookupTimeouts> timeouts;
};

struct LookupRequestCtx {
    Common::Combiner mutex;
    Common::EventCount cv; // notified once the result is set
    std::optional<s32> result;

    s32 titleCtxId = 0;
    bool isAsync = false;
    bool used = false;
    bool aborted = false;
    std::optional<LookupTimeouts> timeouts;

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

    bool HasResult() {
        return mutex.Run([&]() -> bool { return result.has_value(); });
    }

    s32 Wait() {
        cv.Wait([this] { return mutex.Run([this] { return result.has_value(); }); });
        return mutex.Run([this] { return *result; });
    }
};

struct WordFilterTitleCtx {
    s32 userId = -1;
    OrbisNpId selfNpId{};
    std::optional<LookupTimeouts> timeouts;
};

using WordFilterRequestCtx = LookupRequestCtx;

} // namespace Libraries::Np::NpUtility
