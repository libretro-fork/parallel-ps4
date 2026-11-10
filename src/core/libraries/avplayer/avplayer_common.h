// SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <optional>
#include <string_view>
#include <type_traits>

#include "core/libraries/avplayer/avplayer.h"

#define AVPLAYER_IS_ERROR(x) ((x) < 0)

namespace Libraries::AvPlayer {

enum class AvState {
    Unknown,
    Initial,
    AddingSource,
    Ready,
    Play,
    Stop,
    EndOfFile,
    Pause,
    C0x08,
    Jump,
    TrickMode,
    C0x0B,
    Buffering,
    Starting,
    Error,
};

enum class AvEventType {
    ChangeFlowState = 21,
    WarningId = 22,
    RevertState = 30,
    AddSource = 40,
    Error = 255,
};

union AvPlayerEventData {
    u32 num_frames; // 20
    AvState state;  // AvEventType::ChangeFlowState
    s32 error;      // AvEventType::WarningId
    u32 attempt;    // AvEventType::AddSource
};

struct AvPlayerEvent {
    AvEventType event;
    AvPlayerEventData payload;
};

// A bounded FIFO many threads put into and one takes from, without a lock (Vyukov's queue).
template <class T, std::size_t N>
class MpscRing {
    static_assert(std::is_trivially_copyable_v<T> && (N & (N - 1)) == 0);

public:
    MpscRing() {
        for (std::size_t i = 0; i < N; ++i) {
            m_cells[i].seq.store(i, std::memory_order_relaxed);
        }
    }

    // False when the ring is full.
    bool Push(const T& value) {
        std::size_t pos = m_tail.load(std::memory_order_relaxed);
        for (;;) {
            Cell& cell = m_cells[pos & (N - 1)];
            const std::size_t seq = cell.seq.load(std::memory_order_acquire);
            const auto diff = static_cast<std::ptrdiff_t>(seq - pos);
            if (diff == 0) {
                if (m_tail.compare_exchange_weak(pos, pos + 1, std::memory_order_relaxed)) {
                    cell.value = value;
                    cell.seq.store(pos + 1, std::memory_order_release);
                    return true;
                }
            } else if (diff < 0) {
                return false;
            } else {
                pos = m_tail.load(std::memory_order_relaxed);
            }
        }
    }

    // The consumer's.
    std::optional<T> Pop() {
        Cell& cell = m_cells[m_head & (N - 1)];
        if (cell.seq.load(std::memory_order_acquire) != m_head + 1) {
            return std::nullopt;
        }
        const T value = cell.value;
        cell.seq.store(m_head + N, std::memory_order_release);
        ++m_head;
        return value;
    }

    bool Empty() const {
        return m_cells[m_head & (N - 1)].seq.load(std::memory_order_acquire) != m_head + 1;
    }

    void Clear() {
        while (Pop()) {
        }
    }

private:
    struct Cell {
        std::atomic<std::size_t> seq;
        T value;
    };
    std::array<Cell, N> m_cells{};
    alignas(64) std::atomic<std::size_t> m_tail{0};
    alignas(64) std::size_t m_head = 0;
};

AvPlayerSourceType GetSourceType(std::string_view path);

} // namespace Libraries::AvPlayer
