// SPDX-FileCopyrightText: Copyright 2024-2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "common/combiner.h"
#include "common/types.h"
#include "core/emulator_settings.h"
#include "video_core/buffer_cache/region_definitions.h"
#include "video_core/page_manager.h"

namespace VideoCore {

/**
 * Allows tracking CPU and GPU modification of pages in a contigious virtual address region.
 * Information is stored in bitsets for spacial locality and fast update of single pages.
 */
class RegionManager {
public:
    explicit RegionManager(PageManager* tracker_, VAddr cpu_addr_)
        : tracker{tracker_}, cpu_addr{cpu_addr_},
          readbacks_mode{emu_settings_get_readbacks_mode()} {
        cpu.Fill(true);
        gpu.Fill(false);
    }

    void SetCpuAddress(VAddr new_cpu_addr) {
        cpu_addr = new_cpu_addr;
    }

    static constexpr Bounds GetBounds(u64 offset, u64 size) {
        const u64 end_address = offset + size - 1;
        return Bounds{
            .start_word = offset / BYTES_PER_WORD,
            .start_page = (offset) / BYTES_PER_PAGE,
            .end_word = end_address / BYTES_PER_WORD,
            .end_page = (end_address) / BYTES_PER_PAGE,
        };
    }

    /// The pages from @start_page on in its word
    static constexpr u64 StartMask(u64 start_page) {
        return ~u64{0} << (start_page & (PAGES_PER_WORD - 1));
    }

    /// The pages up to @end_page in its word
    static constexpr u64 EndMask(u64 end_page) {
        return ~u64{0} >> (63 - (end_page & (PAGES_PER_WORD - 1)));
    }

    template <typename Func>
    static constexpr void IterateWords(Bounds bounds, Func&& func) {
        const u64 start_word = bounds.start_word;
        const u64 end_word = bounds.end_word;
        const u64 start_mask = StartMask(bounds.start_page);
        const u64 end_mask = EndMask(bounds.end_page);
        if (start_word == end_word) [[likely]] {
            func(start_word, start_mask & end_mask);
        } else {
            func(start_word, start_mask);
            for (s64 i = start_word + 1; i < end_word; ++i) {
                func(i, ~0ULL);
            }
            func(end_word, end_mask);
        }
    }

    template <typename Func>
    static constexpr void IteratePages(u64 word, Func&& func) {
        u64 offset{};
        while (word != 0) {
            const u64 empty_bits = static_cast<u64>(__builtin_ctzll(word));
            offset += empty_bits;
            word >>= empty_bits;
            // The run of set bits from the bottom; all 64 when the word is all ones
            const u64 set_bits = ~word == 0 ? 64 : static_cast<u64>(__builtin_ctzll(~word));
            func(offset, set_bits);
            word = set_bits < PAGES_PER_WORD ? (word >> set_bits) : 0;
            offset += set_bits;
        }
    }

    template <StateOp cpu_op, StateOp gpu_op, bool locked = true>
    void ChangeRegionState(u64 offset, u64 size) {
        const Bounds bounds = GetBounds(offset, size);
        if (!HasEffect<cpu_op, gpu_op>(bounds)) {
            return;
        }
        if constexpr (locked) {
            combiner.Run([this, &bounds] { ChangeState<cpu_op, gpu_op>(bounds); });
        } else {
            ChangeState<cpu_op, gpu_op>(bounds);
        }
    }

    template <Type type, StateOp cpu_op, StateOp gpu_op, bool locked = true, typename Func>
    void ForEachModifiedRange(u64 offset, s64 size, Func&& func) {
        const Bounds bounds = GetBounds(offset, size);
        if (!HasEffect<cpu_op, gpu_op, type>(bounds)) {
            return;
        }
        if constexpr (locked) {
            combiner.Run([this, &bounds, &func] {
                ChangeStateForEachModified<type, cpu_op, gpu_op>(bounds, func);
            });
        } else {
            ChangeStateForEachModified<type, cpu_op, gpu_op>(bounds, func);
        }
    }

    /// Calls func(address, size) for each run of pages in the range that hold bytes the GPU
    /// wrote and that the CPU has not written to since they were last uploaded.
    template <typename Func>
    void ForEachGpuKeptRange(u64 offset, u64 size, Func&& func) {
        IterateWords(GetBounds(offset, size), [&](u64 index, u64 mask) {
            const u64 word = gpu[index] & ~cpu[index] & mask;
            IteratePages(word, [&](u64 pages_offset, u64 pages_size) {
                func(cpu_addr + (index * PAGES_PER_WORD + pages_offset) * BYTES_PER_PAGE,
                     pages_size * BYTES_PER_PAGE);
            });
        });
    }

    template <Type type>
    bool IsRegionModified(u64 offset, u64 size) noexcept {
        const RegionBits& state = GetRegionBits<type>();
        const Bounds bounds = GetBounds(offset, size);
        const u64 start_word = bounds.start_word;
        const u64 end_word = bounds.end_word;
        const u64 start_mask = StartMask(bounds.start_page);
        const u64 end_mask = EndMask(bounds.end_page);
        if (start_word == end_word) [[likely]] {
            return state[start_word] & (start_mask & end_mask);
        } else {
            if (state[start_word] & start_mask) {
                return true;
            }
            for (s64 i = start_word + 1; i < static_cast<s64>(end_word); ++i) {
                if (state[i]) {
                    return true;
                }
            }
            return state[end_word] & end_mask;
        }
    }

    /// Runs @func with no other change to this region at the same time; it may make the
    /// unlocked (locked = false) calls.
    template <typename F>
    decltype((*static_cast<F*>(nullptr))()) Exclusive(F&& func) {
        return combiner.Run(static_cast<F&&>(func));
    }

private:
    /// ChangeRegionState's change, with the region held
    template <StateOp cpu_op, StateOp gpu_op>
    void ChangeState(const Bounds& bounds) {
        bool update_watchers{};
        RegionBits write_prot;
        RegionBits read_prot;
        Bounds watcher_bounds = MIN_BOUNDS;
        IterateWords(bounds, [&](u64 index, u64 mask) {
            update_watchers |= UpdateProtection<cpu_op, gpu_op>(write_prot, read_prot,
                                                                watcher_bounds, index, mask);
        });
        const PageOp write_op = GetPageOp<Type::CPU>(cpu_op);
        const PageOp read_op = GetPageOp<Type::GPU>(gpu_op);
        update_watchers &= write_op != PageOp::None || read_op != PageOp::None;
        if (update_watchers) {
            tracker->UpdatePageWatchersForRegion(cpu_addr, watcher_bounds, write_prot, read_prot,
                                                 write_op, read_op);
        }
        IterateWords(bounds,
                     [&](u64 index, u64 mask) { UpdateState<cpu_op, gpu_op>(index, mask); });
    }

    /// ForEachModifiedRange's change, with the region held
    template <Type type, StateOp cpu_op, StateOp gpu_op, typename Func>
    void ChangeStateForEachModified(const Bounds& bounds, Func& func) {
        RegionBits& state = GetRegionBits<type>();
        bool update_watchers{};
        RegionBits write_prot;
        RegionBits read_prot;
        u64 start_page{};
        u64 end_page{};
        Bounds watcher_bounds = MIN_BOUNDS;
        {
            IterateWords(bounds, [&](u64 index, u64 mask) {
                const u64 base_page = index * PAGES_PER_WORD;
                const u64 word = state[index] & mask;
                update_watchers |= UpdateProtection<cpu_op, gpu_op>(write_prot, read_prot,
                                                                    watcher_bounds, index, mask);
                IteratePages(word, [&](u64 pages_offset, u64 pages_size) {
                    if (end_page == base_page + pages_offset) {
                        end_page += pages_size;
                        return;
                    }
                    if (end_page) {
                        func(cpu_addr + start_page * BYTES_PER_PAGE,
                             (end_page - start_page) * BYTES_PER_PAGE);
                    }
                    start_page = base_page + pages_offset;
                    end_page = start_page + pages_size;
                });
            });
            if (end_page) {
                func(cpu_addr + start_page * BYTES_PER_PAGE,
                     (end_page - start_page) * BYTES_PER_PAGE);
            }
        }
        const PageOp write_op = GetPageOp<Type::CPU>(cpu_op);
        const PageOp read_op = GetPageOp<Type::GPU>(gpu_op);
        update_watchers &= write_op != PageOp::None || read_op != PageOp::None;
        if (update_watchers) {
            tracker->UpdatePageWatchersForRegion(cpu_addr, watcher_bounds, write_prot, read_prot,
                                                 write_op, read_op);
        }
        IterateWords(bounds,
                     [&](u64 index, u64 mask) { UpdateState<cpu_op, gpu_op>(index, mask); });
    }

    template <StateOp op>
    constexpr static u64 Apply(u64 word, u64 mask) {
        if constexpr (op == StateOp::Clear) {
            return word &= ~mask;
        } else if constexpr (op == StateOp::Set) {
            return word |= mask;
        } else {
            static_assert(false);
        }
    }

    template <StateOp cpu_op, StateOp gpu_op>
    constexpr bool UpdateProtection(RegionBits& write_prot, RegionBits& read_prot, Bounds& bounds,
                                    u64 index, u64 mask) {
        u64 prot_word = 0;
        if constexpr (cpu_op != StateOp::None) {
            const u64 prev = cpu[index];
            const u64 next = Apply<cpu_op>(prev, mask);
            write_prot[index] = (next ^ prev) & mask;
            prot_word |= write_prot[index];
        }
        if constexpr (gpu_op != StateOp::None) {
            const u64 prev = gpu[index];
            const u64 next = Apply<gpu_op>(prev, mask);
            read_prot[index] = (next ^ prev) & mask;
            prot_word |= read_prot[index];
        }
        if (prot_word) {
            if (index <= bounds.start_word) {
                bounds.start_word = index;
                bounds.start_page = static_cast<u64>(__builtin_ctzll(prot_word));
            }
            if (index >= bounds.end_word) {
                bounds.end_word = index;
                bounds.end_page = PAGES_PER_WORD - static_cast<u64>(__builtin_clzll(prot_word)) - 1;
            }
            return true;
        }
        return false;
    }

    template <StateOp cpu_op, StateOp gpu_op>
    constexpr void UpdateState(u64 index, u64 mask) {
        const u64 summary_bit = u64{1} << index;
        if constexpr (cpu_op != StateOp::None) {
            cpu[index] = Apply<cpu_op>(cpu[index], mask);
            cpu.set_summary =
                cpu[index] ? (cpu.set_summary | summary_bit) : (cpu.set_summary & ~summary_bit);
            cpu.clear_summary = cpu[index] != ~u64{0} ? (cpu.clear_summary | summary_bit)
                                                      : (cpu.clear_summary & ~summary_bit);
        }
        if constexpr (gpu_op != StateOp::None) {
            gpu[index] = Apply<gpu_op>(gpu[index], mask);
            gpu.set_summary =
                gpu[index] ? (gpu.set_summary | summary_bit) : (gpu.set_summary & ~summary_bit);
            gpu.clear_summary = gpu[index] != ~u64{0} ? (gpu.clear_summary | summary_bit)
                                                      : (gpu.clear_summary & ~summary_bit);
        }
    }

    /// Whether any page of @bounds is set in @state
    static bool AnySet(const RegionBits& state, const Bounds& bounds) {
        const u64 start_mask = StartMask(bounds.start_page);
        const u64 end_mask = EndMask(bounds.end_page);
        if (bounds.start_word == bounds.end_word) {
            const u64 mask = start_mask & end_mask;
            return state[bounds.start_word] & mask;
        }
        const u64 interior_mask = (u64{1} << bounds.end_word) - (u64{2} << bounds.start_word);
        return (state[bounds.start_word] & start_mask) || (state[bounds.end_word] & end_mask) ||
               (state.set_summary & interior_mask);
    }

    /// Whether any page of @bounds is clear in @state
    static bool AnyClear(const RegionBits& state, const Bounds& bounds) {
        const u64 start_mask = StartMask(bounds.start_page);
        const u64 end_mask = EndMask(bounds.end_page);
        if (bounds.start_word == bounds.end_word) {
            const u64 mask = start_mask & end_mask;
            return (state[bounds.start_word] & mask) != mask;
        }
        const u64 interior_mask = (u64{1} << bounds.end_word) - (u64{2} << bounds.start_word);
        return (state[bounds.start_word] & start_mask) != start_mask ||
               (state[bounds.end_word] & end_mask) != end_mask ||
               (state.clear_summary & interior_mask);
    }

    template <StateOp cpu_op, StateOp gpu_op, Type iter_type = Type::None>
    constexpr bool HasEffect(const Bounds& bounds) const noexcept {
        if constexpr (iter_type == Type::CPU) {
            if (AnySet(cpu, bounds)) {
                return true;
            }
        } else if constexpr (cpu_op == StateOp::Set) {
            if (AnyClear(cpu, bounds)) {
                return true;
            }
        } else if constexpr (cpu_op == StateOp::Clear) {
            if (AnySet(cpu, bounds)) {
                return true;
            }
        }

        if constexpr (iter_type == Type::GPU) {
            return AnySet(gpu, bounds);
        } else if constexpr (gpu_op == StateOp::Set) {
            return AnyClear(gpu, bounds);
        } else if constexpr (gpu_op == StateOp::Clear) {
            return AnySet(gpu, bounds);
        } else {
            return false;
        }
    }

    template <Type type>
        requires(type == Type::CPU || type == Type::GPU)
    constexpr PageOp GetPageOp(StateOp state_op) {
        if constexpr (type == Type::CPU) {
            if (state_op == StateOp::Set) {
                return PageOp::Untrack;
            } else if (state_op == StateOp::Clear) {
                return PageOp::Track;
            }
        } else if (type == Type::GPU && readbacks_mode == EMU_READBACKS_PRECISE) {
            if (state_op == StateOp::Set) {
                return PageOp::Track;
            } else if (state_op == StateOp::Clear) {
                return PageOp::Untrack;
            }
        }
        return PageOp::None;
    }

    template <Type type>
        requires(type == Type::CPU || type == Type::GPU)
    RegionBits& GetRegionBits() noexcept {
        if constexpr (type == Type::CPU) {
            return cpu;
        } else if constexpr (type == Type::GPU) {
            return gpu;
        }
    }

    PageManager* tracker;
    VAddr cpu_addr{};
    u32 readbacks_mode;
    RegionBits cpu;
    RegionBits gpu;
    // The region's changes run one at a time here
    Common::Combiner combiner;
};

} // namespace VideoCore
