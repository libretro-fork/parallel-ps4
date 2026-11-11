// SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <atomic>
#include <utility>

#include <queues/retro_retire.h>

#include "common/combiner.h"

#if !defined(RETRO_RETIRE_LOCK_FREE)
#error "Common::Published needs lock-free pointer atomics"
#endif

namespace Common {

/// A value read far more often than it changes, with no lock on either side: what a
/// shared_mutex around it gives.
///
/// Readers see one published copy, whole, inside libretro-common's retro_retire. A change
/// copies it, changes the copy and publishes that; the copy it replaced is freed once no reader
/// is in. Changes run one at a time in a Common::Combiner, so each starts from the last.
template <typename T>
class Published {
public:
    template <typename... Args>
    explicit Published(Args&&... args) : current{new Node{{}, T(std::forward<Args>(args)...)}} {
        retro_retire_init(&retire);
    }
    ~Published() {
        delete current.load();
        retro_retire_drain(&retire, Free);
    }
    Published(const Published&) = delete;
    Published& operator=(const Published&) = delete;

    /// Calls @func with the current value, and returns what it returns. @func must not keep a
    /// reference to the value past the call.
    template <typename F>
    auto Read(F&& func) const {
        retro_retire_enter(&retire);
        struct Leave {
            retro_retire_t* retire;
            ~Leave() {
                retro_retire_leave(retire, Free);
            }
        } leave{&retire};
        return func(std::as_const(current.load(std::memory_order_acquire)->value));
    }

    /// Calls @func on a copy of the current value, then publishes the copy.
    template <typename F>
    void Update(F&& func) {
        writers.Run([&] {
            Node* old = current.load(std::memory_order_relaxed);
            auto* node = new Node{{}, old->value};
            func(node->value);
            current.store(node, std::memory_order_release);
            retro_retire(&retire, &old->node, Free);
        });
    }

private:
    struct Node {
        mpsc_stack_node_t node; // first: retro_retire links copies through it
        T value;
    };

    static void Free(mpsc_stack_node_t* node) {
        delete reinterpret_cast<Node*>(node);
    }

    std::atomic<Node*> current;
    mutable retro_retire_t retire{};
    Combiner writers;
};

} // namespace Common
