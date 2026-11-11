// SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <type_traits>

#include <retro_atomic.h>

namespace Common {
template <typename CounterType>
class IncrementalIdProvider {
    static_assert(std::is_integral_v<CounterType> && sizeof(CounterType) <= 8);

public:
    IncrementalIdProvider() {
        retro_atomic_64_init(&counter, 0);
    }
    ~IncrementalIdProvider() = default;

    CounterType Next() {
        return static_cast<CounterType>(retro_atomic_fetch_add_64(&counter, 1) + 1);
    }

private:
    retro_atomic_64_t counter;
};

} // namespace Common
