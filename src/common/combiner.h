// SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <new>

#include <queues/mpsc_stack.h>
#include <retro_atomic.h>
#include <rthreads/retro_eventcount.h>

#include "common/concepts.h"

namespace Common {

/// Runs functions one at a time, in the order they were handed over, without a lock: what a
/// mutex around them would give, with no thread ever parked on a lock another holds.
///
/// A caller pushes its request (on its own stack: nothing is allocated) onto a lock-free
/// mpsc_stack and then tries to become the combiner. The one that does runs every request
/// queued, its own and the others', oldest first, until none is left; the others park on a
/// retro_eventcount until theirs has run. A function that hands work to the same Combiner from
/// inside it has it run there and then, as a recursive mutex would.
class Combiner {
public:
    Combiner() {
        mpsc_stack_init(&pending);
        retro_eventcount_init(&done_ec);
    }
    ~Combiner() {
        retro_eventcount_free(&done_ec);
    }
    Combiner(const Combiner&) = delete;
    Combiner& operator=(const Combiner&) = delete;

    /// Runs @func with no other function of this Combiner running, and returns what it returns.
    template <typename F>
    auto Run(F&& func) -> decltype(func()) {
        using R = decltype(func());
        using Func = typename RemoveReference<F>::Type;
        if constexpr (IsSame<R, void>) {
            RunErased(const_cast<void*>(static_cast<const void*>(__builtin_addressof(func))),
                      [](void* f) { (*static_cast<Func*>(f))(); });
        } else if constexpr (IsReference<R>) {
            typename RemoveReference<R>::Type* result = nullptr;
            auto call = [&] { result = __builtin_addressof(func()); };
            RunErased(&call, [](void* f) { (*static_cast<decltype(call)*>(f))(); });
            return static_cast<R>(*result);
        } else {
            // Made in place by the function's run, moved out and ended here
            alignas(R) unsigned char storage[sizeof(R)];
            auto call = [&] { new (storage) R(func()); };
            RunErased(&call, [](void* f) { (*static_cast<decltype(call)*>(f))(); });
            R* made = __builtin_launder(reinterpret_cast<R*>(storage));
            R result(static_cast<R&&>(*made));
            made->~R();
            return result;
        }
    }

private:
    struct Request {
        mpsc_stack_node_t node; // first: the stack links requests through it
        void (*call)(void*);
        void* ctx;
        retro_atomic_int_t done;
    };

    /// One address per thread, which no other thread has while it lives
    static void* ThisThread() {
        static thread_local char tag;
        return &tag;
    }

    void RunErased(void* ctx, void (*call)(void*)) {
        // From inside a function this Combiner runs: it holds the Combiner already
        if (retro_atomic_load_relaxed_ptr(&owner) == ThisThread()) {
            call(ctx);
            return;
        }
        Request request{{}, call, ctx, false};
        mpsc_stack_push(&pending, &request.node);
        // Pairs with Combine's fence: a push that loses the race for busy is seen there
        retro_atomic_thread_fence_seq_cst();
        for (;;) {
            if (retro_atomic_exchange_int(&busy, 1) == 0) {
                Combine();
            }
            if (retro_atomic_load_acquire_int(&request.done) != 0) {
                return;
            }
            const int key = retro_eventcount_prepare_wait(&done_ec);
            // Not run yet, and nobody combining: whoever released found it pushed and is
            // about to take over, or this thread can
            if ((retro_atomic_load_acquire_int(&request.done) != 0) ||
                retro_atomic_load_seq_cst_int(&busy) == 0) {
                retro_eventcount_cancel_wait(&done_ec);
                continue;
            }
            retro_eventcount_commit_wait(&done_ec, key);
        }
    }

    /// With busy held: runs what is queued, then lets go of busy, unless more came meanwhile
    /// and this thread can take busy again.
    void Combine() {
        for (;;) {
            mpsc_stack_node_t* node = mpsc_stack_reverse(mpsc_stack_drain(&pending));
            const bool ran = node != nullptr;
            retro_atomic_store_relaxed_ptr(&owner, ThisThread());
            while (node) {
                auto* request = reinterpret_cast<Request*>(node);
                node = node->next;
                request->call(request->ctx);
                // The caller's: it may be gone as soon as done is set
                retro_atomic_store_release_int(&request->done, 1);
            }
            retro_atomic_store_relaxed_ptr(&owner, nullptr);
            if (ran) {
                retro_eventcount_notify(&done_ec);
            }
            (void)retro_atomic_exchange_int(&busy, 0);
            // A push whose taking of busy failed is seen here (RunErased's fence)
            retro_atomic_thread_fence_seq_cst();
            if (mpsc_stack_empty(&pending) || (retro_atomic_exchange_int(&busy, 1) != 0)) {
                return;
            }
        }
    }

    mpsc_stack_t pending{};
    retro_atomic_int_t busy{0};
    /// The thread running the functions, while it does
    retro_atomic_ptr_t owner{};
    retro_eventcount_t done_ec{};
};

} // namespace Common
