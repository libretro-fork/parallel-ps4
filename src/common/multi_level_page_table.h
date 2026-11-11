// SPDX-FileCopyrightText: 2024 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <memory>
#include <retro_atomic.h>

namespace Common {

template <class Traits>
class MultiLevelPageTable final {
    using Entry = typename Traits::Entry;

    static constexpr size_t L1_BITS = Traits::L1_BITS;
    static constexpr size_t L2_BITS = Traits::ADDRESS_SPACE_BITS - L1_BITS - Traits::PAGE_BITS;
    static constexpr size_t NUM_L1_ENTRIES = 1ULL << L2_BITS;
    static constexpr bool NULL_CHECK = Traits::NULL_CHECK;

    using L1Page = std::array<Entry, NUM_L1_ENTRIES>;

public:
    /// The second-level pages are made when first wanted, by whichever thread wants one first:
    /// it publishes its page with a compare-exchange, and a thread that loses the race frees
    /// its own and takes the winner's. No lock, and a reader never waits.
    explicit MultiLevelPageTable(size_t /*chunk_size*/ = 64)
        : top_level{std::make_unique<retro_atomic_ptr_t[]>(TopSize)} {}

    ~MultiLevelPageTable() noexcept {
        for (size_t i = 0; i < TopSize; ++i) {
            delete static_cast<L1Page*>(retro_atomic_load_relaxed_ptr(&top_level[i]));
        }
    }

    MultiLevelPageTable(const MultiLevelPageTable&) = delete;
    MultiLevelPageTable& operator=(const MultiLevelPageTable&) = delete;

    [[nodiscard]] Entry* find(size_t page) {
        L1Page* l1 =
            static_cast<L1Page*>(retro_atomic_load_acquire_ptr(&top_level[page >> L2_BITS]));
        if (!l1) [[unlikely]] {
            return nullptr;
        }
        return &(*l1)[page & (NUM_L1_ENTRIES - 1)];
    }

    [[nodiscard]] const Entry* find(size_t page) const {
        const L1Page* l1 =
            static_cast<L1Page*>(retro_atomic_load_acquire_ptr(&top_level[page >> L2_BITS]));
        if (!l1) [[unlikely]] {
            return nullptr;
        }
        return &(*l1)[page & (NUM_L1_ENTRIES - 1)];
    }

    void reserve(size_t start_page, size_t end_page) {
        const size_t start_l1_page = start_page >> L2_BITS;
        const size_t end_l1_page = end_page >> L2_BITS;
        for (size_t l1_page = start_l1_page; l1_page <= end_l1_page; ++l1_page) {
            Make(l1_page);
        }
    }

    [[nodiscard]] const Entry& operator[](size_t page) const {
        return (*Get(page >> L2_BITS))[page & (NUM_L1_ENTRIES - 1)];
    }

    [[nodiscard]] Entry& operator[](size_t page) {
        return (*Get(page >> L2_BITS))[page & (NUM_L1_ENTRIES - 1)];
    }

private:
    static constexpr size_t TopSize = 1ULL << L1_BITS;

    L1Page* Get(size_t l1_page) const {
        L1Page* l1 = static_cast<L1Page*>(retro_atomic_load_acquire_ptr(&top_level[l1_page]));
        if (NULL_CHECK && !l1) [[unlikely]] {
            l1 = Make(l1_page);
        }
        return l1;
    }

    L1Page* Make(size_t l1_page) const {
        L1Page* l1 = static_cast<L1Page*>(retro_atomic_load_acquire_ptr(&top_level[l1_page]));
        if (l1) {
            return l1;
        }
        auto* made = new L1Page{};
        if ((retro_atomic_cas_ptr(&top_level[l1_page], l1, made) ||
             ((l1 = static_cast<L1Page*>(retro_atomic_load_acquire_ptr(&top_level[l1_page]))),
              false))) {
            return made;
        }
        delete made; // another thread's page went in first: l1 holds it
        return l1;
    }

    std::unique_ptr<retro_atomic_ptr_t[]> top_level;
};

} // namespace Common
