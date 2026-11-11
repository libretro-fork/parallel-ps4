// SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <atomic>
#include <optional>
#include <type_traits>
#include <utility>

#include <queues/mpsc_stack.h>
#include <rthreads/retro_eventcount.h>

namespace Common {

/// Runs functions one at a time, in the order they were handed over, without a lock: what a
/// mutex around them would give, with no thread ever parked on a lock another holds.
///
/// A caller pushes its request (on its own stack: nothing is allocated) onto a lock-free
/// mpsc_stack and then tries to become the combiner. The one that does runs every request
/// queued, its own and the others', oldest first, until none is left; the others park on a
/// retro_eventcount until theirs has run. A function must not hand work to the same Combiner.
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
    auto Run(F&& func) -> std::invoke_result_t<F&> {
        using R = std::invoke_result_t<F&>;
        if constexpr (std::is_void_v<R>) {
            RunErased(&func, [](void* f) { (*static_cast<std::remove_reference_t<F>*>(f))(); });
        } else {
            std::optional<R> result;
            auto call = [&] { result.emplace(func()); };
            RunErased(&call, [](void* f) { (*static_cast<decltype(call)*>(f))(); });
            return std::move(*result);
        }
    }

private:
    struct Request {
        mpsc_stack_node_t node; // first: the stack links requests through it
        void (*call)(void*);
        void* ctx;
        std::atomic<bool> done;
    };

    void RunErased(void* ctx, void (*call)(void*)) {
        Request request{{}, call, ctx, false};
        mpsc_stack_push(&pending, &request.node);
        // Pairs with Combine's fence: a push that loses the race for busy is seen there
        std::atomic_thread_fence(std::memory_order_seq_cst);
        for (;;) {
            if (!busy.exchange(true, std::memory_order_seq_cst)) {
                Combine();
            }
            if (request.done.load(std::memory_order_acquire)) {
                return;
            }
            const int key = retro_eventcount_prepare_wait(&done_ec);
            // Not run yet, and nobody combining: whoever released found it pushed and is
            // about to take over, or this thread can
            if (request.done.load(std::memory_order_acquire) || !busy.load()) {
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
            while (node) {
                auto* request = reinterpret_cast<Request*>(node);
                node = node->next;
                request->call(request->ctx);
                // The caller's: it may be gone as soon as done is set
                request->done.store(true, std::memory_order_release);
            }
            if (ran) {
                retro_eventcount_notify(&done_ec);
            }
            busy.store(false, std::memory_order_seq_cst);
            // A push whose taking of busy failed is seen here (RunErased's fence)
            std::atomic_thread_fence(std::memory_order_seq_cst);
            if (mpsc_stack_empty(&pending) || busy.exchange(true, std::memory_order_acquire)) {
                return;
            }
        }
    }

    mpsc_stack_t pending{};
    std::atomic<bool> busy{false};
    retro_eventcount_t done_ec{};
};

} // namespace Common
