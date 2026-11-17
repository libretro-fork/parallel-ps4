// SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <array>
#include <retro_atomic.h>
#include "common/event_mutex.h"
#include "core/libraries/kernel/threads/pthread.h"
#include "core/libraries/kernel/threads/sleepq.h"

namespace Libraries::Kernel {

static constexpr int HASHSHIFT = 9;
static constexpr int HASHSIZE = (1 << HASHSHIFT);
#define SC_HASH(wchan)                                                                             \
    ((u32)((((uintptr_t)(wchan) >> 3) ^ ((uintptr_t)(wchan) >> (HASHSHIFT + 3))) & (HASHSIZE - 1)))
#define SC_LOOKUP(wc) &sc_table[SC_HASH(wc)]

struct SleepQueueChain {
    // Parks under contention where a spin lock burned the core
    Common::EventMutex sc_lock;
    SleepqList sc_queues;
    int sc_type;
};

static std::array<SleepQueueChain, HASHSIZE> sc_table{};

void SleepqLock(void* wchan) {
    if (g_curthread != nullptr) {
        retro_atomic_fetch_add_int(&g_curthread->locklevel, 1);
    }
    SleepQueueChain* sc = SC_LOOKUP(wchan);
    sc->sc_lock.lock();
}

void SleepqUnlock(void* wchan) {
    SleepQueueChain* sc = SC_LOOKUP(wchan);
    sc->sc_lock.unlock();
    if (g_curthread != nullptr) {
        const int previous = retro_atomic_fetch_sub_int(&g_curthread->locklevel, 1);
        ASSERT(previous > 0);
        if (previous == 1) {
            PthreadCancelInterrupt();
        }
    }
}

SleepQueue* SleepqLookup(void* wchan) {
    SleepQueueChain* sc = SC_LOOKUP(wchan);
    for (auto& sq : sc->sc_queues) {
        if (sq.sq_wchan == wchan) {
            return std::addressof(sq);
        }
    }
    return nullptr;
}

void SleepqAdd(void* wchan, Pthread* td) {
    SleepQueue* sq = SleepqLookup(wchan);
    if (sq != nullptr) {
        sq->sq_freeq.push_front(*td->sleepqueue);
    } else {
        SleepQueueChain* sc = SC_LOOKUP(wchan);
        sq = td->sleepqueue;
        sc->sc_queues.push_front(*sq);
        sq->sq_wchan = wchan;
        /* sq->sq_type = type; */
    }
    td->sleepqueue = nullptr;
    td->wchan = wchan;
    // libkernel uses a TAILQ here. Signal therefore selects the oldest waiter.
    td->sq_next = nullptr;
    td->sq_prev = sq->sq_blocked_tail;
    if (sq->sq_blocked_tail != nullptr) {
        sq->sq_blocked_tail->sq_next = td;
    } else {
        sq->sq_blocked = td;
    }
    sq->sq_blocked_tail = td;
}

// Whether @td is on @sq's blocked threads
static bool SleepqHas(const SleepQueue* sq, const Pthread* td) {
    for (const Pthread* t = sq->sq_blocked; t != nullptr; t = t->sq_next) {
        if (t == td) {
            return true;
        }
    }
    return false;
}

static void SleepqUnblock(SleepQueue* sq, Pthread* td) {
    if (td->sq_prev != nullptr) {
        td->sq_prev->sq_next = td->sq_next;
    } else {
        sq->sq_blocked = td->sq_next;
    }
    if (td->sq_next != nullptr) {
        td->sq_next->sq_prev = td->sq_prev;
    } else {
        sq->sq_blocked_tail = td->sq_prev;
    }
    td->sq_prev = nullptr;
    td->sq_next = nullptr;
}

bool SleepqRemove(SleepQueue* sq, Pthread* td) {
    ASSERT_MSG(sq != nullptr, "Cannot remove a thread from a null sleep queue");
    if (sq == nullptr) [[unlikely]] {
        return false;
    }

    // On it when its wait channel is the queue's (the walk only checks that it is)
    const bool removed = td->wchan == sq->sq_wchan && SleepqHas(sq, td);
    ASSERT_MSG(removed, "Thread is missing from its sleep queue");
    if (!removed) [[unlikely]] {
        return sq->sq_blocked != nullptr;
    }
    SleepqUnblock(sq, td);
    const bool has_waiters = sq->sq_blocked != nullptr;

    td->wchan = nullptr;
    if (!has_waiters) {
        td->sleepqueue = sq;
        sq->unlink();
        return false;
    }

    ASSERT_MSG(!sq->sq_freeq.empty(), "Sleep queue free list is empty while waiters remain");
    td->sleepqueue = std::addressof(sq->sq_freeq.front());
    sq->sq_freeq.pop_front();
    return true;
}

void SleepqDrop(SleepQueue* sq, void (*callback)(Pthread*, void*), void* arg) {
    if (sq->sq_blocked == nullptr) {
        return;
    }

    sq->unlink();
    Pthread* td = sq->sq_blocked;
    SleepqUnblock(sq, td);

    callback(td, arg);

    td->sleepqueue = sq;
    td->wchan = nullptr;

    auto sq2 = sq->sq_freeq.begin();
    while (Pthread* td2 = sq->sq_blocked) {
        SleepqUnblock(sq, td2);
        callback(td2, arg);
        td2->sleepqueue = std::addressof(*sq2);
        td2->wchan = nullptr;
        ++sq2;
    }
    sq->sq_freeq.clear();
}

} // namespace Libraries::Kernel
