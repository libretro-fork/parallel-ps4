// SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <cstring>

#include <retro_atomic.h>

#include "common/combiner.h"
#include "common/event_count.h"
#include "common/types.h"

/// What libSceUlt's objects do, without the game's side of them (ult.cpp): run tokens, a
/// mutex, a semaphore, a condition variable, and the queue with its pool of items. No lock:
/// retro_atomic words, a Common::Combiner where a list is changed, and a Common::EventCount to
/// park on. Tested on its own (libretro/tests/ult_sync_test.cpp).
namespace Libraries::Ult::Sync {

/// Called around a park, by the thread that parks. A user-level thread gives up its runtime's
/// run token while it waits in one of these objects and takes one again after, which is when
/// the real library switches to another user-level thread.
inline void (*before_park)() = nullptr;
inline void (*after_park)() = nullptr;

/// Runs @attempt until it succeeds, parked on @ec between tries.
template <typename F>
void ParkUntil(Common::EventCount& ec, F&& attempt) {
    if (attempt()) {
        return;
    }
    if (before_park) {
        before_park();
    }
    ec.Wait(attempt);
    if (after_park) {
        after_park();
    }
}

/// One address per thread, which no other thread has while it lives
inline void* ThisThread() {
    static thread_local char tag;
    return &tag;
}

/// As many threads run at once as a runtime has worker threads: each holds a token while it
/// runs.
class Tokens {
public:
    explicit Tokens(int count) {
        retro_atomic_int_init(&tokens, count);
    }

    bool TryTake() {
        int now = retro_atomic_load_acquire_int(&tokens);
        while (now > 0) {
            if (retro_atomic_cas_int(&tokens, now, now - 1)) {
                return true;
            }
            now = retro_atomic_load_acquire_int(&tokens);
        }
        return false;
    }

    void Take() {
        ec.Wait([this] { return TryTake(); });
    }

    void Give() {
        retro_atomic_fetch_add_int(&tokens, 1);
        ec.Notify();
    }

private:
    retro_atomic_int_t tokens;
    Common::EventCount ec;
};

class Semaphore {
public:
    explicit Semaphore(int resources_) {
        retro_atomic_int_init(&resources, resources_);
    }

    bool TryAcquire(int count) {
        int now = retro_atomic_load_acquire_int(&resources);
        while (now >= count) {
            if (retro_atomic_cas_int(&resources, now, now - count)) {
                return true;
            }
            now = retro_atomic_load_acquire_int(&resources);
        }
        return false;
    }

    void Acquire(int count) {
        ParkUntil(ec, [&] { return TryAcquire(count); });
    }

    void Release(int count) {
        retro_atomic_fetch_add_int(&resources, count);
        ec.Notify();
    }

private:
    retro_atomic_int_t resources;
    Common::EventCount ec;
};

class Mutex {
public:
    enum class Result { Locked, Busy, Relocked };

    explicit Mutex(bool recursive_) : recursive{recursive_} {
        retro_atomic_ptr_init(&owner, nullptr);
    }

    /// Relocked: the caller holds it already and it is not recursive.
    Result TryLock() {
        void* const self = ThisThread();
        if (retro_atomic_load_relaxed_ptr(&owner) == self) {
            if (!recursive) {
                return Result::Relocked;
            }
            ++count;
            return Result::Locked;
        }
        if (!retro_atomic_cas_ptr(&owner, nullptr, self)) {
            return Result::Busy;
        }
        count = 1;
        return Result::Locked;
    }

    Result Lock() {
        Result result = Result::Busy;
        ParkUntil(ec, [&] {
            result = TryLock();
            return result != Result::Busy;
        });
        return result;
    }

    bool HeldByCaller() const {
        return retro_atomic_load_relaxed_ptr(const_cast<retro_atomic_ptr_t*>(&owner)) ==
               ThisThread();
    }

    /// False if the caller does not hold it.
    bool Unlock() {
        if (!HeldByCaller()) {
            return false;
        }
        if (--count == 0) {
            retro_atomic_store_release_ptr(&owner, nullptr);
            ec.Notify();
        }
        return true;
    }

    /// Lets go of every lock the caller has on it; how many there were, for Relock.
    u32 UnlockAll() {
        const u32 held = count;
        count = 0;
        retro_atomic_store_release_ptr(&owner, nullptr);
        ec.Notify();
        return held;
    }

    void Relock(u32 held) {
        void* const self = ThisThread();
        ParkUntil(ec, [&] { return retro_atomic_cas_ptr(&owner, nullptr, self) != 0; });
        count = held;
    }

private:
    retro_atomic_ptr_t owner;
    u32 count{}; // the owner's
    bool recursive;
    Common::EventCount ec;
};

/// A condition variable of one mutex. Each waiter takes a number as it starts to wait, under
/// the mutex, and a signal lets the waiters through in that order, up to a number: so a signal
/// reaches a thread that was waiting when it was sent, never one that came to wait after (the
/// signaller itself, waiting next, would otherwise take its own signal and leave the thread
/// it meant asleep).
class Condition {
public:
    explicit Condition(Mutex& mutex_) : mutex{mutex_} {
        retro_atomic_int_init(&arrived, 0);
        retro_atomic_int_init(&released, 0);
    }

    /// False if the caller does not hold the mutex.
    bool Wait() {
        if (!mutex.HeldByCaller()) {
            return false;
        }
        // Numbered before the mutex is let go: a signal sent under the mutex counts this one
        const u32 number = static_cast<u32>(retro_atomic_fetch_add_int(&arrived, 1));
        const u32 held = mutex.UnlockAll();
        ParkUntil(ec, [&] {
            // Through once the signals have reached its number (the counters may wrap)
            const u32 upto = static_cast<u32>(retro_atomic_load_acquire_int(&released));
            return static_cast<s32>(upto - number) > 0;
        });
        mutex.Relock(held);
        return true;
    }

    void Signal() {
        Release(false);
    }

    void SignalAll() {
        Release(true);
    }

private:
    void Release(bool all) {
        for (;;) {
            const u32 upto = static_cast<u32>(retro_atomic_load_acquire_int(&released));
            const u32 last = static_cast<u32>(retro_atomic_load_acquire_int(&arrived));
            if (upto == last) {
                return; // nobody waiting
            }
            if (retro_atomic_cas_int(&released, static_cast<int>(upto),
                                     static_cast<int>(all ? last : upto + 1))) {
                ec.Notify();
                return;
            }
        }
    }

    Mutex& mutex;
    retro_atomic_int_t arrived;  // waiters there have been
    retro_atomic_int_t released; // how many of them signals have let through
    Common::EventCount ec;
};

/// The items the queues of a pool hold, in the work area the game gave for them: the items'
/// bytes, then a link per item. Nothing is allocated for an item.
class DataPool {
public:
    static constexpr u32 None = 0xFFFFFFFFu;

    /// What Init needs at @work_area for @capacity items of @item_size bytes.
    static constexpr u64 WorkAreaSize(u32 capacity, u64 item_size) {
        return u64{capacity} * item_size + u64{capacity} * sizeof(u32);
    }

    DataPool(void* work_area, u32 capacity_, u64 item_size_)
        : data{static_cast<u8*>(work_area)}, capacity{capacity_}, item_size{item_size_} {
        for (u32 i = 0; i < capacity; ++i) {
            SetNext(i, i + 1 < capacity ? i + 1 : None);
        }
        free_head = capacity ? 0 : None;
    }

    u64 ItemSize() const {
        return item_size;
    }

    u8* Item(u32 slot) {
        return data + u64{slot} * item_size;
    }

    /// A free item, or None.
    u32 Take() {
        return combiner.Run([this] {
            const u32 slot = free_head;
            if (slot != None) {
                free_head = Next(slot);
            }
            return slot;
        });
    }

    void Give(u32 slot) {
        combiner.Run([&] {
            SetNext(slot, free_head);
            free_head = slot;
        });
        space.Notify();
    }

    // The links are unaligned in the work area: read and written bytewise
    u32 Next(u32 slot) const {
        u32 next;
        std::memcpy(&next, Links() + u64{slot} * sizeof(u32), sizeof(next));
        return next;
    }

    void SetNext(u32 slot, u32 next) {
        std::memcpy(Links() + u64{slot} * sizeof(u32), &next, sizeof(next));
    }

    Common::Combiner combiner; // the free list, and the lists of the pool's queues
    Common::EventCount space;  // an item was given back

private:
    u8* Links() const {
        return data + u64{capacity} * item_size;
    }

    u8* data;
    u32 capacity;
    u64 item_size;
    u32 free_head{None};
};

/// A queue of items of a pool, first in first out. A push waits for a free item of the pool,
/// a pop for an item in the queue.
class Queue {
public:
    explicit Queue(DataPool& pool_) : pool{pool_} {}

    bool TryPush(const void* item) {
        const u32 slot = pool.Take();
        if (slot == DataPool::None) {
            return false;
        }
        Append(slot, item);
        return true;
    }

    void Push(const void* item) {
        u32 slot = DataPool::None;
        ParkUntil(pool.space, [&] {
            slot = pool.Take();
            return slot != DataPool::None;
        });
        Append(slot, item);
    }

    bool TryPop(void* item) {
        const u32 slot = TakeHead();
        if (slot == DataPool::None) {
            return false;
        }
        Remove(slot, item);
        return true;
    }

    void Pop(void* item) {
        u32 slot = DataPool::None;
        ParkUntil(filled, [&] {
            slot = TakeHead();
            return slot != DataPool::None;
        });
        Remove(slot, item);
    }

private:
    void Append(u32 slot, const void* item) {
        // The item is this thread's alone until it is linked in
        std::memcpy(pool.Item(slot), item, pool.ItemSize());
        pool.combiner.Run([&] {
            pool.SetNext(slot, DataPool::None);
            if (tail == DataPool::None) {
                head = slot;
            } else {
                pool.SetNext(tail, slot);
            }
            tail = slot;
        });
        filled.Notify();
    }

    u32 TakeHead() {
        return pool.combiner.Run([this] {
            const u32 slot = head;
            if (slot != DataPool::None) {
                head = pool.Next(slot);
                if (head == DataPool::None) {
                    tail = DataPool::None;
                }
            }
            return slot;
        });
    }

    void Remove(u32 slot, void* item) {
        if (item) { // none when a queue being destroyed drops what it holds
            std::memcpy(item, pool.Item(slot), pool.ItemSize());
        }
        pool.Give(slot);
    }

    DataPool& pool;
    u32 head{DataPool::None}; // in the pool's combiner
    u32 tail{DataPool::None};
    Common::EventCount filled; // an item was appended
};

} // namespace Libraries::Ult::Sync
