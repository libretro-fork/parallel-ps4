// SPDX-FileCopyrightText: 2024 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <atomic>
#include <memory>

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
        : top_level{std::make_unique<std::atomic<L1Page*>[]>(TopSize)} {}

    ~MultiLevelPageTable() noexcept {
        for (size_t i = 0; i < TopSize; ++i) {
            delete top_level[i].load(std::memory_order_relaxed);
        }
    }

    MultiLevelPageTable(const MultiLevelPageTable&) = delete;
    MultiLevelPageTable& operator=(const MultiLevelPageTable&) = delete;

    [[nodiscard]] Entry* find(size_t page) {
        L1Page* l1 = top_level[page >> L2_BITS].load(std::memory_order_acquire);
        if (!l1) [[unlikely]] {
            return nullptr;
        }
        return &(*l1)[page & (NUM_L1_ENTRIES - 1)];
    }

    [[nodiscard]] const Entry* find(size_t page) const {
        const L1Page* l1 = top_level[page >> L2_BITS].load(std::memory_order_acquire);
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
        L1Page* l1 = top_level[l1_page].load(std::memory_order_acquire);
        if (NULL_CHECK && !l1) [[unlikely]] {
            l1 = Make(l1_page);
        }
        return l1;
    }

    L1Page* Make(size_t l1_page) const {
        L1Page* l1 = top_level[l1_page].load(std::memory_order_acquire);
        if (l1) {
            return l1;
        }
        auto* made = new L1Page{};
        if (top_level[l1_page].compare_exchange_strong(l1, made, std::memory_order_acq_rel,
                                                       std::memory_order_acquire)) {
            return made;
        }
        delete made; // another thread's page went in first: l1 holds it
        return l1;
    }

    std::unique_ptr<std::atomic<L1Page*>[]> top_level;
};

} // namespace Common
