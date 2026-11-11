// SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <atomic>

#include <rthreads/retro_eventcount.h>

namespace Common {

/// Mutual exclusion for what is held across calls (taken in one function, given back in
/// another), where a Common::Combiner cannot stand in. No host mutex: one atomic flag, taken
/// with an exchange; a thread that finds it taken parks on a retro_eventcount, which unlock
/// notifies only when someone is parked. Meets Lockable, for std::scoped_lock and the like.
class EventMutex {
public:
    EventMutex() {
        retro_eventcount_init(&released);
    }
    ~EventMutex() {
        retro_eventcount_free(&released);
    }
    EventMutex(const EventMutex&) = delete;
    EventMutex& operator=(const EventMutex&) = delete;

    bool try_lock() {
        return !held.load(std::memory_order_relaxed) &&
               !held.exchange(true, std::memory_order_acquire);
    }

    void lock() {
        while (!try_lock()) {
            const int key = retro_eventcount_prepare_wait(&released);
            if (try_lock()) {
                retro_eventcount_cancel_wait(&released);
                return;
            }
            retro_eventcount_commit_wait(&released, key);
        }
    }

    void unlock() {
        held.store(false, std::memory_order_release);
        retro_eventcount_notify(&released);
    }

    /// @func with the mutex held, for state a Common::EventCondition waits on.
    template <typename F>
    auto Run(F&& func) -> decltype(func()) {
        lock();
        struct Unlock {
            EventMutex* mutex;
            ~Unlock() {
                mutex->unlock();
            }
        } unlock_on_exit{this};
        return func();
    }

private:
    std::atomic<bool> held{false};
    retro_eventcount_t released{};
};

} // namespace Common

#include <chrono>
#include <condition_variable>
#include <stop_token>

namespace Common {

/// What std::condition_variable_any does, on a retro_eventcount: no host mutex of its own. A
/// waiter opens its wait before it lets go of the caller's lock, so a notify made after the
/// state changed under that lock always reaches it. Paired with Common::EventMutex where code
/// waits on a condition under a lock.
class EventCondition {
public:
    EventCondition() {
        retro_eventcount_init(&ec);
    }
    ~EventCondition() {
        retro_eventcount_free(&ec);
    }
    EventCondition(const EventCondition&) = delete;
    EventCondition& operator=(const EventCondition&) = delete;

    void notify_one() {
        retro_eventcount_notify(&ec);
    }
    void notify_all() {
        retro_eventcount_notify(&ec);
    }

    template <typename Lock>
    void wait(Lock& lock) {
        const int key = retro_eventcount_prepare_wait(&ec);
        lock.unlock();
        retro_eventcount_commit_wait(&ec, key);
        lock.lock();
    }

    template <typename Lock, typename Pred>
    void wait(Lock& lock, Pred pred) {
        while (!pred()) {
            wait(lock);
        }
    }

    template <typename Lock, typename Pred>
    bool wait(Lock& lock, std::stop_token stop, Pred pred) {
        const std::stop_callback wake{stop, [this] { notify_all(); }};
        while (!pred()) {
            if (stop.stop_requested()) {
                return false;
            }
            const int key = retro_eventcount_prepare_wait(&ec);
            if (stop.stop_requested()) {
                retro_eventcount_cancel_wait(&ec);
                return pred();
            }
            lock.unlock();
            retro_eventcount_commit_wait(&ec, key);
            lock.lock();
        }
        return true;
    }

    template <typename Lock, typename Clock, typename Duration>
    std::cv_status wait_until(Lock& lock, const std::chrono::time_point<Clock, Duration>& until) {
        const auto left =
            std::chrono::ceil<std::chrono::microseconds>(until - Clock::now()).count();
        if (left <= 0) {
            return std::cv_status::timeout;
        }
        const int key = retro_eventcount_prepare_wait(&ec);
        lock.unlock();
        const bool notified = retro_eventcount_commit_wait_timeout(&ec, key, left);
        lock.lock();
        return notified ? std::cv_status::no_timeout : std::cv_status::timeout;
    }

    template <typename Lock, typename Clock, typename Duration, typename Pred>
    bool wait_until(Lock& lock, const std::chrono::time_point<Clock, Duration>& until, Pred pred) {
        while (!pred()) {
            if (wait_until(lock, until) == std::cv_status::timeout) {
                return pred();
            }
        }
        return true;
    }

    template <typename Lock, typename Rep, typename Period>
    std::cv_status wait_for(Lock& lock, const std::chrono::duration<Rep, Period>& rel) {
        return wait_until(lock, std::chrono::steady_clock::now() + rel);
    }

    template <typename Lock, typename Rep, typename Period, typename Pred>
    bool wait_for(Lock& lock, const std::chrono::duration<Rep, Period>& rel, Pred pred) {
        return wait_until(lock, std::chrono::steady_clock::now() + rel, std::move(pred));
    }

private:
    retro_eventcount_t ec{};
};

} // namespace Common

#include <thread>

namespace Common {

/// Common::EventMutex that the thread holding it may take again, as std::recursive_mutex.
class RecursiveEventMutex {
public:
    bool try_lock() {
        const auto self = std::this_thread::get_id();
        if (owner.load(std::memory_order_relaxed) == self) {
            ++depth;
            return true;
        }
        if (!mutex.try_lock()) {
            return false;
        }
        owner.store(self, std::memory_order_relaxed);
        depth = 1;
        return true;
    }

    void lock() {
        const auto self = std::this_thread::get_id();
        if (owner.load(std::memory_order_relaxed) == self) {
            ++depth;
            return;
        }
        mutex.lock();
        owner.store(self, std::memory_order_relaxed);
        depth = 1;
    }

    void unlock() {
        if (--depth == 0) {
            owner.store(std::thread::id{}, std::memory_order_relaxed);
            mutex.unlock();
        }
    }

    template <typename F>
    auto Run(F&& func) -> decltype(func()) {
        lock();
        struct Unlock {
            RecursiveEventMutex* mutex;
            ~Unlock() {
                mutex->unlock();
            }
        } unlock_on_exit{this};
        return func();
    }

private:
    EventMutex mutex;
    std::atomic<std::thread::id> owner{};
    unsigned depth = 0; // the owner's alone
};

} // namespace Common
