// SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <mutex>
#include <vector>
#include <retro_atomic.h>
#include "common/combiner.h"
#include "common/singleton.h"
#include "common/slab_heap.h"
#include "common/types.h"

namespace Libraries::Kernel {

struct Pthread;
struct PthreadAttr;

struct Stack {
    size_t stacksize; /* Stack size (rounded up). */
    size_t guardsize; /* Guard size. */
    void* stackaddr;  /* Stack address. */
    Stack* next;      /* The next spare stack of its list */
};

struct ThreadState {
    static constexpr size_t GcThreshold = 5;
    static constexpr size_t MaxThreads = 100000;
    static constexpr size_t MaxCachedThreads = 100;

    explicit ThreadState();

    bool GcNeeded() const noexcept {
        return retro_atomic_load_relaxed_size(&gc_count) >= GcThreshold;
    }

    void Collect(Pthread* curthread);

    /// @thread's life said it is to be collected (ended, detached, held by no one): off the
    /// live list and onto the list to collect, once
    void Retire(Pthread* thread);

    Pthread* Alloc(Pthread* curthread);

    void Free(Pthread* curthread, Pthread* thread);

    /// A hold on @thread's record if it is a live thread's (and not ended, unless
    /// @include_dead): 0, and the caller gives it back with RefDelete; else an error, no hold
    int FindThread(Pthread* thread, bool include_dead);

    int RefAdd(Pthread* thread, bool include_dead);

    void RefDelete(Pthread* thread);

    int CreateStack(PthreadAttr* attr);

    void FreeStack(PthreadAttr* attr);

    void Link(Pthread* curthread, Pthread* thread) {
        thread_list_combiner.Run([&] { LinkLocked(thread); });
        retro_atomic_fetch_add_int(&active_threads, 1);
    }

    void Unlink(Pthread* curthread, Pthread* thread) {
        thread_list_combiner.Run([&] { UnlinkLocked(thread); });
        retro_atomic_fetch_sub_int(&active_threads, 1);
    }

    /// Whether @thread is a record of the thread heap, at the start of one: what a pointer the
    /// game hands in must be before its record is looked at
    bool IsThreadRecord(const Pthread* thread) const;

    Common::SlabHeap<Pthread> thread_heap;
    // The live threads, linked through the records (Pthread::thread_prev, thread_next), and
    // whether each record of the heap is on the list, by its index: a lookup of a pointer the
    // game hands in, and a thread made or ended, take no node and no tree. In the thread list's
    // combiner.
    Pthread* threads{};
    std::vector<u8> thread_live;
    void LinkLocked(Pthread* thread);
    void UnlinkLocked(Pthread* thread);
    // Lists linked through the records (Pthread::list_next) and the spare stacks themselves
    // (Stack::next), so that making and ending a thread adds and takes no list node: the
    // records of ended threads to collect, the free records, the spare stacks
    Pthread* free_threads{};
    std::size_t free_count{};
    Pthread* gc_list{};
    retro_atomic_size_t gc_count{};
    // Each list, and the TLS blocks, changes one call at a time in its own combiner
    Common::Combiner free_thread_combiner;
    Common::Combiner tcb_combiner;
    Common::Combiner thread_list_combiner;
    retro_atomic_int_t total_threads{};
    retro_atomic_int_t active_threads{};
    Stack* dstackq{};
    Stack* mstackq{};
    VAddr last_stack = 0;
};

using ThrState = Common::Singleton<ThreadState>;

} // namespace Libraries::Kernel
