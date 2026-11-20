// SPDX-FileCopyrightText: 2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <stddef.h>

#include "common/small_vector.h"
#include "common/types.h"

struct Interval {
    u64 start;
    u64 end;

    constexpr bool CanMergeWith(const Interval&) const noexcept {
        return true;
    }
    constexpr Interval SubRange(u64 a, u64 b) const noexcept {
        return {a, b};
    }
};

namespace Common::Detail {
template <class A, class B>
struct IsSameType {
    static constexpr bool value = false;
};
template <class A>
struct IsSameType<A, A> {
    static constexpr bool value = true;
};
template <class A, class B>
concept SameAs = IsSameType<A, B>::value;
} // namespace Common::Detail

template <class IV>
concept IsInterval = __is_base_of(Interval, IV) && __is_trivially_copyable(IV) &&
                     requires(const IV& a, const IV& b, u64 x) {
                         { a.CanMergeWith(b) } -> Common::Detail::SameAs<bool>;
                         { a.SubRange(x, x) } -> Common::Detail::SameAs<IV>;
                     };

template <class IV = Interval>
    requires IsInterval<IV>
class IntervalList {
public:
    using List = Common::SmallVector<IV, 4>;
    using const_iterator = typename List::const_iterator;
    using iterator = typename List::iterator;

    const_iterator begin() const {
        return intervals.begin();
    }
    const_iterator end() const {
        return intervals.end();
    }

    void Clear() {
        intervals.clear();
    }
    bool Empty() const {
        return intervals.empty();
    }
    size_t Size() const {
        return intervals.size();
    }
    void Reserve(size_t n) {
        intervals.reserve(n);
    }

    /// Adds a range to the interval set.
    void Add(IV value) {
        if (value.start >= value.end) [[unlikely]] {
            return;
        }

        IV out[3];
        size_t m = 0;

        iterator first;
        iterator last;
        OverlapRun(value.start, value.end, first, last);
        if (first != last) {
            if (first->start < value.start) {
                out[m++] = first->SubRange(first->start, value.start);
            }
            out[m++] = value;
            const iterator prev = last - 1;
            if (prev->end > value.end) {
                out[m++] = prev->SubRange(value.end, prev->end);
            }
        } else {
            out[m++] = value;
        }

        const size_t at = static_cast<size_t>(first - intervals.begin());
        Splice(first, last, out, m);
        Coalesce(at, m);
    }

    /// Removes a range from the interval set, cutting the intervals at its edges.
    void Subtract(u64 start, u64 end) {
        if (start >= end) [[unlikely]] {
            return;
        }
        const iterator first = intervals.begin() + EndAfter(start);
        iterator last = first;
        while (last != intervals.end() && last->start < end) {
            ++last;
        }
        if (first == last) {
            return;
        }
        IV out[2];
        size_t m = 0;
        if (first->start < start) {
            out[m++] = first->SubRange(first->start, start);
        }
        const iterator prev = last - 1;
        if (prev->end > end) {
            out[m++] = prev->SubRange(end, prev->end);
        }
        Splice(first, last, out, m);
    }

    /// Returns immutable iterator to an interval that contains provided address.
    const_iterator Find(u64 addr) const {
        const const_iterator it = intervals.begin() + EndAfter(addr);
        if (it == intervals.end()) {
            return intervals.end();
        }
        return (it->start <= addr && addr < it->end) ? it : intervals.end();
    }

    /// Returns true if address if contained inside an interval.
    bool Contains(u64 addr) const {
        return Find(addr) != intervals.end();
    }

    /// Returns true if range is fully contained in the interval set.
    bool Contains(u64 start, u64 end) const {
        if (start >= end) [[unlikely]] {
            return false;
        }
        const_iterator it = intervals.begin() + EndAfter(start);
        u64 cur = start;
        while (it != intervals.end() && it->start <= cur && cur < end) {
            cur = it->end;
            ++it;
        }
        return cur >= end;
    }

    /// Returns true if range partially overlaps an interval.
    bool Overlaps(u64 start, u64 end) const {
        if (start >= end) [[unlikely]] {
            return false;
        }
        const_iterator it = intervals.begin() + EndAfter(start);
        return it != intervals.end() && it->start < end;
    }

    /// Calls 'func' for each gap in provided range.
    template <typename Func>
    void ForEachGap(u64 start, u64 end, Func&& func) const {
        if (start >= end) [[unlikely]] {
            return;
        }
        const_iterator it = intervals.begin() + EndAfter(start);
        u64 cur = start;
        for (; it != intervals.end() && it->start < end; ++it) {
            if (it->start > cur) {
                func(cur, it->start);
            }
            cur = cur > it->end ? cur : it->end;
        }
        if (cur < end) {
            func(cur, end);
        }
    }

    /// Calls 'func' for each interval that partially overlaps provided range.
    template <typename Func>
    void ForEachInRange(u64 start, u64 end, Func&& func) const {
        if (start >= end) [[unlikely]] {
            return;
        }
        const_iterator it = intervals.begin() + EndAfter(start);
        if (it == intervals.end()) {
            return;
        }
        while (it != intervals.end() && it->start < end) {
            func(*it);
            ++it;
        }
    }

protected:
    /// Returns first interval that overlaps range and first interval after range.
    void OverlapRun(u64 start, u64 end, iterator& first, iterator& last) noexcept {
        first = intervals.begin() + EndAfter(start);
        last = first;
        while (last != intervals.end() && last->start <= end) {
            ++last;
        }
    }

    /// The index of the first interval that ends after @addr
    size_t EndAfter(u64 addr) const noexcept {
        size_t first = 0;
        size_t count = intervals.size();
        while (count > 0) {
            const size_t half = count / 2;
            if (!(addr < intervals[first + half].end)) {
                first += half + 1;
                count -= half + 1;
            } else {
                count = half;
            }
        }
        return first;
    }

    /// Inserts intervals from first until last
    void Splice(iterator first, iterator last, const IV* src, size_t m) noexcept {
        if (first == last) {
            intervals.insert(first, src, src + m);
            return;
        }
        const size_t slots = static_cast<size_t>(last - first);
        if (m <= slots) {
            for (size_t i = 0; i < m; ++i) {
                first[i] = src[i];
            }
            if (m < slots) {
                intervals.erase(first + m, last);
            }
        } else {
            for (size_t i = 0; i < slots; ++i) {
                first[i] = src[i];
            }
            intervals.insert(last, src + slots, src + m);
        }
    }

    /// Merges compatible adjacent intervals.
    void Coalesce(size_t start, size_t count) {
        size_t stop = start + count + 1;
        stop = stop < intervals.size() ? stop : intervals.size();
        for (size_t i = start ? start - 1 : start; i + 1 < stop;) {
            IV& a = intervals[i];
            IV& b = intervals[i + 1];
            if (a.end == b.start && a.CanMergeWith(b)) {
                a.end = b.end;
                intervals.erase(intervals.begin() + i + 1);
                --stop;
            } else {
                ++i;
            }
        }
    }

    List intervals;
};

template <class IV>
concept IsDomInterval = IsInterval<IV> && requires(const IV& a, const IV& b) {
    { a.Dominant(b) } -> Common::Detail::SameAs<bool>;
};

template <class IV = Interval>
    requires IsDomInterval<IV>
struct DomIntervalList : public IntervalList<IV> {
    template <typename... Args>
    void Add(Args&&... args) {
        Add(IV{args...});
    }

    void Add(IV value) {
        typename IntervalList<IV>::iterator first;
        typename IntervalList<IV>::iterator last;
        this->OverlapRun(value.start, value.end, first, last);
        Common::SmallVector<IV, 8> out;
        if (first != last && first->start < value.start) {
            out.emplace_back(first->SubRange(first->start, value.start));
        }
        u64 cur = value.start;
        for (typename IntervalList<IV>::iterator it = first; it != last; ++it) {
            if (!it->Dominant(value)) {
                continue;
            }
            const u64 a = it->start > value.start ? it->start : value.start;
            const u64 b = it->end < value.end ? it->end : value.end;
            if (a >= b) {
                continue;
            }
            if (cur < a) {
                out.emplace_back(value.SubRange(cur, a));
            }
            out.emplace_back(it->SubRange(a, b));
            cur = b;
        }
        if (cur < value.end) {
            out.emplace_back(value.SubRange(cur, value.end));
        }
        if (first != last) {
            const typename IntervalList<IV>::iterator prev = last - 1;
            if (prev->end > value.end) {
                out.emplace_back(prev->SubRange(value.end, prev->end));
            }
        }
        size_t w = 0;
        for (size_t r = 1; r < out.size(); ++r) {
            if (out[w].end == out[r].start && out[w].CanMergeWith(out[r])) {
                out[w].end = out[r].end;
            } else {
                out[++w] = out[r];
            }
        }
        if (!out.empty()) {
            out.resize(w + 1);
        }
        const size_t at = static_cast<size_t>(first - this->intervals.begin());
        this->Splice(first, last, out.data(), out.size());
        this->Coalesce(at, out.size());
    }
};
