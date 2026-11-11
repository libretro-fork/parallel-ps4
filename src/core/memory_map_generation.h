// SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <cstdint>
#include <retro_atomic.h>

namespace Core {

class MemoryMapGeneration final {
public:
    class Mutation final {
    public:
        explicit Mutation(MemoryMapGeneration& owner_) : owner{owner_} {
            retro_atomic_fetch_add_64(&owner.value, 1);
        }

        ~Mutation() {
            Finish();
        }

        void Finish() {
            if (!active) {
                return;
            }
            retro_atomic_fetch_add_64(&owner.value, 1);
            active = false;
        }

        Mutation(const Mutation&) = delete;
        Mutation& operator=(const Mutation&) = delete;

    private:
        MemoryMapGeneration& owner;
        bool active{true};
    };

    [[nodiscard]] Mutation BeginMutation() {
        return Mutation{*this};
    }

    [[nodiscard]] std::uint64_t Load() const {
        return static_cast<u64>(retro_atomic_load_acquire_64(&value));
    }

private:
    retro_atomic_64_t value{};
};

} // namespace Core
