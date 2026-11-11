// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <array>
#include <cstddef>
#include <stop_token>
#include <utility>

#include <retro_atomic.h>
#include <rthreads/retro_eventcount.h>

namespace Common {

namespace detail {
constexpr std::size_t DefaultCapacity = 0x1000;

/// A bounded queue any number of threads push to and pop from, with no lock (Vyukov's: each
/// cell carries a sequence number that says whose turn it is). The waits park on a
/// retro_eventcount: one for something to pop, one for room to push.
template <typename T, std::size_t Capacity>
class BoundedQueue {
    static_assert((Capacity & (Capacity - 1)) == 0, "Capacity must be a power of two.");

public:
    BoundedQueue() {
        for (std::size_t i = 0; i < Capacity; ++i) {
            retro_atomic_store_relaxed_64(&m_cells[i].seq, i);
        }
        retro_eventcount_init(&m_readable);
        retro_eventcount_init(&m_writable);
    }
    ~BoundedQueue() {
        retro_eventcount_free(&m_writable);
        retro_eventcount_free(&m_readable);
    }
    BoundedQueue(const BoundedQueue&) = delete;
    BoundedQueue& operator=(const BoundedQueue&) = delete;

    template <typename... Args>
    bool TryEmplace(Args&&... args) {
        if (!TryPush(std::forward<Args>(args)...)) {
            return false;
        }
        retro_eventcount_notify(&m_readable);
        return true;
    }

    template <typename... Args>
    void EmplaceWait(Args&&... args) {
        while (!TryPush(std::forward<Args>(args)...)) {
            const int key = retro_eventcount_prepare_wait(&m_writable);
            if (!Full()) {
                retro_eventcount_cancel_wait(&m_writable);
                continue;
            }
            retro_eventcount_commit_wait(&m_writable, key);
        }
        retro_eventcount_notify(&m_readable);
    }

    bool TryPop(T& t) {
        if (!TryTake(t)) {
            return false;
        }
        retro_eventcount_notify(&m_writable);
        return true;
    }

    void PopWait(T& t) {
        PopWait(t, std::stop_token{});
    }

    /// False if @stop_token was stopped first.
    bool PopWait(T& t, std::stop_token stop_token) {
        // A stop wakes the wait below
        const std::stop_callback wake{stop_token, [this] { retro_eventcount_notify(&m_readable); }};
        while (!TryTake(t)) {
            if (stop_token.stop_requested()) {
                return false;
            }
            const int key = retro_eventcount_prepare_wait(&m_readable);
            if (!Empty() || stop_token.stop_requested()) {
                retro_eventcount_cancel_wait(&m_readable);
                continue;
            }
            retro_eventcount_commit_wait(&m_readable, key);
        }
        retro_eventcount_notify(&m_writable);
        return true;
    }

    T PopWait() {
        T t;
        PopWait(t);
        return t;
    }

    T PopWait(std::stop_token stop_token) {
        T t;
        PopWait(t, stop_token);
        return t;
    }

private:
    struct Cell {
        retro_atomic_64_t seq;
        T data;
    };

    template <typename... Args>
    bool TryPush(Args&&... args) {
        std::size_t pos = static_cast<std::size_t>(retro_atomic_load_relaxed_64(&m_write_index));
        Cell* cell;
        for (;;) {
            cell = &m_cells[pos & (Capacity - 1)];
            const std::size_t seq =
                static_cast<std::size_t>(retro_atomic_load_acquire_64(&cell->seq));
            const auto diff = static_cast<std::ptrdiff_t>(seq - pos);
            if (diff == 0) {
                if ((retro_atomic_cas_64(&m_write_index, pos, pos + 1) ||
                     ((pos =
                           static_cast<std::size_t>(retro_atomic_load_acquire_64(&m_write_index))),
                      false))) {
                    break;
                }
            } else if (diff < 0) {
                return false; // full
            } else {
                pos = static_cast<std::size_t>(retro_atomic_load_relaxed_64(&m_write_index));
            }
        }
        cell->data = T(std::forward<Args>(args)...);
        retro_atomic_store_release_64(&cell->seq, pos + 1);
        return true;
    }

    bool TryTake(T& t) {
        std::size_t pos = static_cast<std::size_t>(retro_atomic_load_relaxed_64(&m_read_index));
        Cell* cell;
        for (;;) {
            cell = &m_cells[pos & (Capacity - 1)];
            const std::size_t seq =
                static_cast<std::size_t>(retro_atomic_load_acquire_64(&cell->seq));
            const auto diff = static_cast<std::ptrdiff_t>(seq - (pos + 1));
            if (diff == 0) {
                if ((retro_atomic_cas_64(&m_read_index, pos, pos + 1) ||
                     ((pos = static_cast<std::size_t>(retro_atomic_load_acquire_64(&m_read_index))),
                      false))) {
                    break;
                }
            } else if (diff < 0) {
                return false; // empty
            } else {
                pos = static_cast<std::size_t>(retro_atomic_load_relaxed_64(&m_read_index));
            }
        }
        t = std::move(cell->data);
        cell->data = T{}; // what it held is released now, not when the cell is next written
        retro_atomic_store_release_64(&cell->seq, pos + Capacity);
        return true;
    }

    bool Empty() const {
        const std::size_t pos =
            static_cast<std::size_t>(retro_atomic_load_relaxed_64(&m_read_index));
        return static_cast<std::size_t>(
                   retro_atomic_load_acquire_64(&m_cells[pos & (Capacity - 1)].seq)) != pos + 1;
    }

    bool Full() const {
        const std::size_t pos =
            static_cast<std::size_t>(retro_atomic_load_relaxed_64(&m_write_index));
        return static_cast<std::size_t>(
                   retro_atomic_load_acquire_64(&m_cells[pos & (Capacity - 1)].seq)) != pos;
    }

    alignas(128) retro_atomic_64_t m_read_index{0};
    alignas(128) retro_atomic_64_t m_write_index{0};
    alignas(128) std::array<Cell, Capacity> m_cells;
    retro_eventcount_t m_readable{};
    retro_eventcount_t m_writable{};
};
} // namespace detail

// The three are the same queue now: it takes any number of threads on either side.
template <typename T, std::size_t Capacity = detail::DefaultCapacity>
using SPSCQueue = detail::BoundedQueue<T, Capacity>;
template <typename T, std::size_t Capacity = detail::DefaultCapacity>
using MPSCQueue = detail::BoundedQueue<T, Capacity>;
template <typename T, std::size_t Capacity = detail::DefaultCapacity>
using MPMCQueue = detail::BoundedQueue<T, Capacity>;

} // namespace Common
