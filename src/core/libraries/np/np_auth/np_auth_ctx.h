// SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <cstring>
#include <mutex>
#include <optional>
#include <string>
#include "common/combiner.h"
#include "common/event_count.h"
#include "common/types.h"
#include "core/libraries/error_codes.h"
#include "core/libraries/np/np_types.h"

namespace Libraries::Np::NpAuth {

struct AuthRequestCtx {
    Common::Combiner mutex;
    Common::EventCount cv; // notified once the result is set
    std::optional<s32> result;

    OrbisNpAuthorizationCode* authCode = nullptr;
    s32* issuerId = nullptr;

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
    void SetAuthorizationCode(const std::string& code, s32 issuer) {
        if (mutex.Run([&] {
                if (result.has_value()) {
                    return true;
                }
                std::memset(authCode, 0, sizeof(OrbisNpAuthorizationCode));
                std::memcpy(authCode->code, code.data(), code.size());
                if (issuerId) {
                    *issuerId = issuer;
                }
                result = ORBIS_OK;
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

} // namespace Libraries::Np::NpAuth
