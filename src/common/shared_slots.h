// SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <array>
#include <cstddef>
#include <memory>
#include <optional>

#include <queues/retro_retire.h>
#include <retro_atomic.h>

#if !defined(RETRO_RETIRE_LOCK_FREE)
#error "Common::SharedSlots needs lock-free pointer atomics"
#endif

namespace Common {

/// A fixed table of shared objects, each slot empty or holding one, that any thread reads and
/// changes with no lock: what an array of shared_ptr under a shared_mutex gives.
///
/// A slot points at a holder of its shared_ptr. A reader enters libretro-common's
/// retro_retire, copies the shared_ptr out and leaves, so what it took stays alive for as long
/// as it keeps it; a holder taken out of its slot is freed once no reader is in. Holders are
/// allocated when an object is put in, not when one is read.
template <typename T, size_t N>
class SharedSlots {
public:
    SharedSlots() {
        retro_retire_init(&retire);
    }
    ~SharedSlots() {
        for (auto& slot : slots) {
            delete static_cast<Holder*>(retro_atomic_exchange_ptr(&slot, nullptr));
        }
        retro_retire_drain(&retire, Free);
    }
    SharedSlots(const SharedSlots&) = delete;
    SharedSlots& operator=(const SharedSlots&) = delete;

    static constexpr size_t size() {
        return N;
    }

    /// What slot @index holds, or null.
    std::shared_ptr<T> Get(size_t index) {
        retro_retire_enter(&retire);
        const Holder* holder = static_cast<Holder*>(retro_atomic_load_acquire_ptr(&slots[index]));
        std::shared_ptr<T> object = holder ? holder->object : nullptr;
        retro_retire_leave(&retire, Free);
        return object;
    }

    /// True if slot @index holds nothing (it may be filled right after).
    bool Empty(size_t index) const {
        return static_cast<Holder*>(retro_atomic_load_acquire_ptr(&slots[index])) == nullptr;
    }

    /// Puts @object in slot @index if it is empty. @return false if it was not.
    bool Put(size_t index, std::shared_ptr<T> object) {
        auto* holder = new Holder{{}, std::move(object)};
        Holder* expected = nullptr;
        if (!(retro_atomic_cas_ptr(&slots[index], expected, holder) ||
              ((expected = static_cast<Holder*>(retro_atomic_load_acquire_ptr(&slots[index]))),
               false))) {
            delete holder;
            return false;
        }
        retro_retire(&retire, nullptr, Free); // readers from here on see it
        return true;
    }

    /// Puts @object in an empty slot, looking from where the last insert left off.
    /// @return the slot, or nothing if all are full.
    std::optional<size_t> Insert(std::shared_ptr<T> object) {
        auto* holder = new Holder{{}, std::move(object)};
        const size_t start = static_cast<size_t>(retro_atomic_load_relaxed_64(&next));
        for (size_t n = 0; n < N; ++n) {
            const size_t index = (start + n) % N;
            Holder* expected = nullptr;
            if ((retro_atomic_cas_ptr(&slots[index], expected, holder) ||
                 ((expected = static_cast<Holder*>(retro_atomic_load_acquire_ptr(&slots[index]))),
                  false))) {
                retro_atomic_store_relaxed_64(&next, (index + 1) % N);
                retro_retire(&retire, nullptr, Free);
                return index;
            }
        }
        delete holder;
        return std::nullopt;
    }

    /// Empties slot @index. @return what it held, or null.
    std::shared_ptr<T> Take(size_t index) {
        Holder* holder = static_cast<Holder*>(retro_atomic_exchange_ptr(&slots[index], nullptr));
        if (!holder) {
            return nullptr;
        }
        std::shared_ptr<T> object = holder->object;
        retro_retire(&retire, &holder->node, Free);
        return object;
    }

private:
    struct Holder {
        mpsc_stack_node_t node; // first: retro_retire links holders through it
        std::shared_ptr<T> object;
    };

    static void Free(mpsc_stack_node_t* node) {
        delete reinterpret_cast<Holder*>(node);
    }

    std::array<retro_atomic_ptr_t, N> slots{};
    retro_atomic_64_t next{0};
    retro_retire_t retire{};
};

} // namespace Common
