// SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

// libSceUlt's objects without the game (src/core/libraries/ult/ult_sync.h): the queue keeps
// every item, in order, across threads that fill and empty it and through a pool with too few
// items for them; two queues share a pool; the mutex excludes, counts its recursive locks and
// refuses a second lock and a stranger's unlock; the semaphore lets no more through than it
// has; no signal of a condition variable is lost and SignalAll wakes every waiter; and with a
// runtime of one worker no two user-level threads ever run at once, changing only where one
// waits.

#include <atomic>
#include <cstdio>
#include <thread>
#include <vector>

#include "core/libraries/ult/ult_sync.h"

using namespace Libraries::Ult::Sync;

static int failures = 0;

#define CHECK(cond, what)                                                                      \
    do {                                                                                       \
        if (!(cond)) {                                                                         \
            std::printf("FAIL %s (line %d)\n", what, __LINE__);                                \
            ++failures;                                                                        \
        }                                                                                      \
    } while (0)

static void TestQueue() {
    // 5 items of 12 bytes for 3 producers of 20000 each: pushes wait for the poppers
    constexpr u32 Capacity = 5;
    struct Item {
        u32 producer;
        u32 sequence;
        u32 check;
    };
    std::vector<u8> work(DataPool::WorkAreaSize(Capacity, sizeof(Item)) + 1);
    DataPool pool{work.data() + 1, Capacity, sizeof(Item)}; // unaligned, as a game's may be
    Queue queue{pool};
    std::atomic<long> popped{0};
    std::atomic<int> out_of_order{0}, corrupt{0};
    std::vector<std::thread> threads;
    for (u32 p = 0; p < 3; ++p) {
        threads.emplace_back([&, p] {
            for (u32 n = 0; n < 20000; ++n) {
                const Item item{p, n, p * 31 + n};
                queue.Push(&item);
            }
        });
    }
    for (int c = 0; c < 2; ++c) {
        threads.emplace_back([&] {
            u32 last[3] = {0, 0, 0};
            bool seen[3] = {false, false, false};
            while (popped.fetch_add(1) < 60000) {
                Item item;
                queue.Pop(&item);
                if (item.producer > 2 || item.check != item.producer * 31 + item.sequence) {
                    ++corrupt;
                    continue;
                }
                // Each consumer sees a producer's items in the order they were pushed
                if (seen[item.producer] && item.sequence <= last[item.producer]) {
                    ++out_of_order;
                }
                seen[item.producer] = true;
                last[item.producer] = item.sequence;
            }
        });
    }
    for (auto& thread : threads) {
        thread.join();
    }
    Item item;
    CHECK(corrupt == 0, "every item whole");
    CHECK(out_of_order == 0, "first in, first out");
    CHECK(!queue.TryPop(&item), "all 60000 popped, none left");

    // Full and empty, without waiting
    for (u32 n = 0; n < Capacity; ++n) {
        const Item full{9, n, 0};
        CHECK(queue.TryPush(&full), "room for the pool's items");
    }
    CHECK(!queue.TryPush(&item), "no push into a full pool");
    // A second queue of the same pool gets nothing until the first gives an item back
    Queue other{pool};
    CHECK(!other.TryPush(&item), "the pool's items are shared by its queues");
    CHECK(queue.TryPop(&item) && item.sequence == 0, "oldest first");
    CHECK(other.TryPush(&item) && other.TryPop(&item) && item.producer == 9,
          "the second queue takes the freed item");
    for (u32 n = 1; n < Capacity; ++n) {
        CHECK(queue.TryPop(&item) && item.sequence == n, "in order");
    }
    CHECK(!queue.TryPop(&item) && !other.TryPop(&item), "both empty");
}

static void TestMutex() {
    Mutex plain{false}, recursive{true};
    long counter = 0;
    std::vector<std::thread> threads;
    for (int t = 0; t < 4; ++t) {
        threads.emplace_back([&] {
            for (int n = 0; n < 50000; ++n) {
                plain.Lock();
                ++counter;
                plain.Unlock();
            }
        });
    }
    for (auto& thread : threads) {
        thread.join();
    }
    CHECK(counter == 200000, "mutex excludes");
    CHECK(plain.Lock() == Mutex::Result::Locked, "locked");
    CHECK(plain.TryLock() == Mutex::Result::Relocked, "a plain mutex is not locked twice");
    std::thread([&] {
        CHECK(plain.TryLock() == Mutex::Result::Busy, "busy for another thread");
        CHECK(!plain.Unlock(), "not unlocked by a stranger");
    }).join();
    CHECK(plain.Unlock() && !plain.Unlock(), "unlocked once");
    CHECK(recursive.Lock() == Mutex::Result::Locked &&
              recursive.TryLock() == Mutex::Result::Locked,
          "a recursive mutex locked twice");
    CHECK(recursive.Unlock(), "first unlock");
    std::thread([&] { CHECK(recursive.TryLock() == Mutex::Result::Busy, "still held"); }).join();
    CHECK(recursive.Unlock(), "second unlock");
    std::thread([&] {
        CHECK(recursive.TryLock() == Mutex::Result::Locked, "free after both");
        recursive.Unlock();
    }).join();
}

static void TestSemaphore() {
    Semaphore semaphore{3};
    std::atomic<int> inside{0}, most{0};
    std::vector<std::thread> threads;
    for (int t = 0; t < 8; ++t) {
        threads.emplace_back([&] {
            for (int n = 0; n < 5000; ++n) {
                semaphore.Acquire(1);
                const int now = ++inside;
                int seen = most.load();
                while (now > seen && !most.compare_exchange_weak(seen, now)) {
                }
                --inside;
                semaphore.Release(1);
            }
        });
    }
    for (auto& thread : threads) {
        thread.join();
    }
    CHECK(most <= 3, "no more through than it has");
    CHECK(semaphore.TryAcquire(3) && !semaphore.TryAcquire(1), "all three, then none");
    semaphore.Release(2);
    CHECK(!semaphore.TryAcquire(3) && semaphore.TryAcquire(2), "two given back");
}

static void TestCondition() {
    Mutex mutex{false};
    Condition condition{mutex};
    // A handshake of 20000 turns: a lost signal would hang it
    int turn = 0;
    std::thread other([&] {
        for (int n = 0; n < 20000; ++n) {
            mutex.Lock();
            while (turn != 1) {
                condition.Wait();
            }
            turn = 0;
            condition.SignalAll();
            mutex.Unlock();
        }
    });
    for (int n = 0; n < 20000; ++n) {
        mutex.Lock();
        while (turn != 0) {
            condition.Wait();
        }
        turn = 1;
        condition.SignalAll();
        mutex.Unlock();
    }
    other.join();
    CHECK(turn == 0, "20000 turns handed over");
    CHECK(!condition.Wait(), "no wait without the mutex");

    // Six waiters: one signal wakes one, SignalAll the rest
    int go = 0, woken = 0;
    std::atomic<int> waiting{0};
    std::vector<std::thread> threads;
    for (int t = 0; t < 6; ++t) {
        threads.emplace_back([&] {
            mutex.Lock();
            ++waiting;
            while (go == 0) {
                condition.Wait();
            }
            --go;
            ++woken;
            mutex.Unlock();
        });
    }
    while (waiting < 6) {
        std::this_thread::yield();
    }
    mutex.Lock(); // all six are in Wait once this is had
    go = 1;
    condition.Signal();
    mutex.Unlock();
    for (;;) {
        mutex.Lock();
        const bool one = woken == 1;
        mutex.Unlock();
        if (one) {
            break;
        }
        std::this_thread::yield();
    }
    mutex.Lock();
    CHECK(woken == 1, "one signal, one waiter");
    go = 5;
    condition.SignalAll();
    mutex.Unlock();
    for (auto& thread : threads) {
        thread.join();
    }
    CHECK(woken == 6, "SignalAll wakes the rest");
}

// ---- a runtime of one worker ----

static thread_local Tokens* t_tokens = nullptr;
static std::atomic<int> running{0}, overlaps{0};

static void Stop() {
    if (t_tokens) {
        --running;
        t_tokens->Give();
    }
}

static void Go() {
    if (t_tokens) {
        t_tokens->Take();
        if (++running != 1) {
            ++overlaps;
        }
    }
}

static void TestTokens() {
    Tokens tokens{1};
    Mutex mutex{false};
    Condition condition{mutex};
    constexpr u32 Capacity = 2;
    std::vector<u8> work(DataPool::WorkAreaSize(Capacity, sizeof(int)));
    DataPool pool{work.data(), Capacity, sizeof(int)};
    Queue queue{pool};
    long shared = 0; // changed with no lock: only one of them runs at a time
    before_park = Stop;
    after_park = Go;
    std::vector<std::thread> threads;
    for (int t = 0; t < 6; ++t) {
        threads.emplace_back([&, t] {
            t_tokens = &tokens;
            Go();
            for (int n = 0; n < 3000; ++n) {
                ++shared;
                if (t % 2 == 0) {
                    queue.Push(&n); // waits when the two items are taken
                } else {
                    int value;
                    queue.Pop(&value); // waits when there is none
                }
                ++shared;
                mutex.Lock();
                ++shared;
                mutex.Unlock();
            }
            Stop();
        });
    }
    for (auto& thread : threads) {
        thread.join();
    }
    before_park = nullptr;
    after_park = nullptr;
    CHECK(overlaps == 0, "one worker: never two running at once");
    CHECK(shared == 6 * 3000 * 3, "what they share kept whole with no lock of its own");
}

int main() {
    TestQueue();
    TestMutex();
    TestSemaphore();
    TestCondition();
    TestTokens();
    if (failures) {
        std::printf("%d failures\n", failures);
        return 1;
    }
    std::printf("ult_sync: all passed\n");
    return 0;
}
