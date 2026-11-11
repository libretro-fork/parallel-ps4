//  SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
//  SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <chrono>
#include <map>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

#include <retro_atomic.h>
#include "common/combiner.h"
#include "common/types.h"

namespace Core::Devtools::Widget {

/// Memory used by one memory type within a group.
struct GpuMemoryRow {
    std::string name;
    u64 bytes{};
};

/// Stats published by one subsystem.
struct GpuMemoryGroup {
    std::string name;
    std::vector<GpuMemoryRow> rows;
};

class GpuMemoryViewer {
public:
    bool open = false;

    void Draw();

    static bool IsEnabled() {
        const s64 last = static_cast<s64>(retro_atomic_load_relaxed_64(&last_drawn_ms));
        return last != 0 && NowMs() - last < ENABLED_TIMEOUT_MS;
    }

    static void Publish(GpuMemoryGroup group) {
        std::string name = group.name;
        mutex.Run([&] { groups.insert_or_assign(std::move(name), std::move(group)); });
    }

private:
    static constexpr s64 ENABLED_TIMEOUT_MS = 500;

    static s64 NowMs() {
        using namespace std::chrono;
        return duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
    }

    static inline retro_atomic_64_t last_drawn_ms{0};
    static inline Common::Combiner mutex;
    static inline std::map<std::string, GpuMemoryGroup> groups;
};

} // namespace Core::Devtools::Widget
