// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <stop_token>
#include <utility>

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
            m_cells[i].seq.store(i, std::memory_order::relaxed);
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
        std::atomic_size_t seq;
        T data;
    };

    template <typename... Args>
    bool TryPush(Args&&... args) {
        std::size_t pos = m_write_index.load(std::memory_order::relaxed);
        Cell* cell;
        for (;;) {
            cell = &m_cells[pos & (Capacity - 1)];
            const std::size_t seq = cell->seq.load(std::memory_order::acquire);
            const auto diff = static_cast<std::ptrdiff_t>(seq - pos);
            if (diff == 0) {
                if (m_write_index.compare_exchange_weak(pos, pos + 1, std::memory_order::relaxed)) {
                    break;
                }
            } else if (diff < 0) {
                return false; // full
            } else {
                pos = m_write_index.load(std::memory_order::relaxed);
            }
        }
        cell->data = T(std::forward<Args>(args)...);
        cell->seq.store(pos + 1, std::memory_order::release);
        return true;
    }

    bool TryTake(T& t) {
        std::size_t pos = m_read_index.load(std::memory_order::relaxed);
        Cell* cell;
        for (;;) {
            cell = &m_cells[pos & (Capacity - 1)];
            const std::size_t seq = cell->seq.load(std::memory_order::acquire);
            const auto diff = static_cast<std::ptrdiff_t>(seq - (pos + 1));
            if (diff == 0) {
                if (m_read_index.compare_exchange_weak(pos, pos + 1, std::memory_order::relaxed)) {
                    break;
                }
            } else if (diff < 0) {
                return false; // empty
            } else {
                pos = m_read_index.load(std::memory_order::relaxed);
            }
        }
        t = std::move(cell->data);
        cell->data = T{}; // what it held is released now, not when the cell is next written
        cell->seq.store(pos + Capacity, std::memory_order::release);
        return true;
    }

    bool Empty() const {
        const std::size_t pos = m_read_index.load(std::memory_order::relaxed);
        return m_cells[pos & (Capacity - 1)].seq.load(std::memory_order::acquire) != pos + 1;
    }

    bool Full() const {
        const std::size_t pos = m_write_index.load(std::memory_order::relaxed);
        return m_cells[pos & (Capacity - 1)].seq.load(std::memory_order::acquire) != pos;
    }

    alignas(128) std::atomic_size_t m_read_index{0};
    alignas(128) std::atomic_size_t m_write_index{0};
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
