// SPDX-FileCopyrightText: 2024 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <algorithm>

#include "common/interval_set.h"
#include "common/types.h"

namespace VideoCore {

/// A set of address ranges: sorted, merged intervals in one array (IntervalList), where
/// Boost.ICL kept a tree of pool-allocated nodes.
struct RangeSet {
    explicit RangeSet() {
        m_ranges.Reserve(1024);
    }
    ~RangeSet() = default;

    void Add(VAddr base_address, size_t size) {
        m_ranges.Add(Interval{base_address, base_address + size});
    }

    void Subtract(VAddr base_address, size_t size) {
        m_ranges.Subtract(base_address, base_address + size);
    }

    void Clear() {
        m_ranges.Clear();
    }

    bool Contains(VAddr base_address, size_t size) const {
        // An empty range is contained in anything, as Boost.ICL had it
        return size == 0 || m_ranges.Contains(base_address, base_address + size);
    }

    bool Intersects(VAddr base_address, size_t size) const {
        return m_ranges.Overlaps(base_address, base_address + size);
    }

    template <typename Func>
    void ForEach(Func&& func) const {
        for (const Interval& range : m_ranges) {
            func(range.start, range.end);
        }
    }

    template <typename Func>
    void ForEachInRange(VAddr base_addr, size_t size, Func&& func) const {
        if (size == 0) {
            return;
        }
        const VAddr start_address = base_addr;
        const VAddr end_address = start_address + size;
        m_ranges.ForEachInRange(start_address, end_address, [&](const Interval& range) {
            func(std::max<VAddr>(range.start, start_address),
                 std::min<VAddr>(range.end, end_address));
        });
    }

    template <typename Func>
    void ForEachNotInRange(VAddr base_addr, size_t size, Func&& func) const {
        const VAddr end_addr = base_addr + size;
        ForEachInRange(base_addr, size, [&](VAddr range_addr, VAddr range_end) {
            if (size_t gap_size = range_addr - base_addr; gap_size != 0) {
                func(base_addr, gap_size);
            }
            base_addr = range_end;
        });
        if (base_addr != end_addr) {
            func(base_addr, end_addr - base_addr);
        }
    }

    IntervalList<> m_ranges;
};

} // namespace VideoCore
