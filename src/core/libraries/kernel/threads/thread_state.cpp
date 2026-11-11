// SPDX-FileCopyrightText: Copyright 2024-2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <retro_atomic.h>
#include "common/alignment.h"
#include "common/scope_exit.h"
#include "common/small_vector.h"
#include "core/libraries/kernel/posix_error.h"
#include "core/libraries/kernel/threads/pthread.h"
#include "core/libraries/kernel/threads/sleepq.h"
#include "core/libraries/kernel/threads/thread_state.h"
#include "core/memory.h"
#include "core/tls.h"

namespace Libraries::Kernel {

thread_local Pthread* g_curthread{};

Core::Tcb* TcbCtor(Pthread* thread, int initial);
void TcbDtor(Core::Tcb* oldtls);

ThreadState::ThreadState() {
    // Reserve memory for maximum amount of threads allowed.
    auto* memory = Core::Memory::Instance();
    auto& impl = memory->GetAddressSpace();
    static constexpr u32 ThrHeapSize = Common::AlignUp(sizeof(Pthread) * MaxThreads, 16_KB);
    void* heap_addr{};
    const int ret = memory->MapMemory(&heap_addr, impl.SystemReservedVirtualBase(), ThrHeapSize,
                                      Core::MemoryProt::CpuReadWrite, Core::MemoryMapFlags::NoFlags,
                                      Core::VMAType::File, "ThrHeap");
    ASSERT_MSG(ret == 0, "Unable to allocate thread heap memory {}", ret);
    thread_heap.Initialize(heap_addr, ThrHeapSize);
}

void ThreadState::Collect(Pthread* curthread) {
    Common::SmallVector<Pthread*, 8> work_list;
    thread_list_combiner.Run([&] {
        for (auto it = gc_list.begin(); it != gc_list.end();) {
            Pthread* td = *it;
            if (static_cast<s32>(retro_atomic_load_seq_cst_int(&td->tid)) != TidTerminated) {
                ++it;
                continue;
            }
            FreeStack(&td->attr);
            work_list.push_back(td);
            it = gc_list.erase(it);
        }
    });
    for (Pthread* td : work_list) {
        Free(curthread, td);
    }
}

void ThreadState::TryCollect(Pthread* thread) {
    SCOPE_EXIT {
        thread->lock->unlock();
    };
    if (!thread->ShouldCollect()) {
        return;
    }

    thread->refcount++;
    thread->lock->unlock();
    // The thread lock is taken again inside, and given back by the SCOPE_EXIT above
    thread_list_combiner.Run([&] {
        thread->lock->lock();
        thread->refcount--;
        if (thread->ShouldCollect()) {
            threads.erase(thread);
            gc_list.push_back(thread);
        }
    });
}

Pthread* ThreadState::Alloc(Pthread* curthread) {
    Pthread* thread = nullptr;
    if (curthread != nullptr) {
        if (GcNeeded()) {
            Collect(curthread);
        }
        free_thread_combiner.Run([&] {
            if (!free_threads.empty()) {
                thread = free_threads.back();
                free_threads.pop_back();
            }
        });
    }
    if (thread == nullptr) {
        if (static_cast<s32>(retro_atomic_load_seq_cst_int(&total_threads)) > MaxThreads) {
            return nullptr;
        }
        retro_atomic_fetch_add_int(&total_threads, 1);
        thread = thread_heap.Allocate();
        if (thread == nullptr) {
            retro_atomic_fetch_sub_int(&total_threads, 1);
            return nullptr;
        }
    }
    Core::Tcb* tcb = nullptr;
    if (curthread != nullptr) {
        tcb = tcb_combiner.Run([&] { return TcbCtor(thread, 0 /* not initial tls */); });
    } else {
        tcb = TcbCtor(thread, 1 /* initial tls */);
    }
    if (tcb != nullptr) {
        // Initialize thread struct memory to 0. This is safe since it will be constructed
        // immediately after.
        std::memset(static_cast<void*>(thread), 0, sizeof(Pthread));
        std::construct_at(thread);
        thread->tcb = tcb;
        thread->sleepqueue = new SleepQueue{};
    } else {
        thread_heap.Free(thread);
        retro_atomic_fetch_sub_int(&total_threads, 1);
        thread = nullptr;
    }
    return thread;
}

void ThreadState::Free(Pthread* curthread, Pthread* thread) {
    if (curthread != nullptr) {
        tcb_combiner.Run([&] { TcbDtor(thread->tcb); });
    } else {
        TcbDtor(thread->tcb);
    }
    thread->tcb = nullptr;
    auto* sleepqueue = thread->sleepqueue;
    bool should_free;
    free_thread_combiner.Run([&] {
        if (free_threads.size() >= MaxCachedThreads) {
            should_free = true;
        } else {
            should_free = false;
            free_threads.push_back(thread);
        }
    });
    if (should_free) {
        delete sleepqueue;
        thread_heap.Free(thread);
        retro_atomic_fetch_sub_int(&total_threads, 1);
    }
}

int ThreadState::FindThread(Pthread* thread, const bool include_dead) {
    if (thread == nullptr) {
        return POSIX_EINVAL;
    }
    // Returns with the thread lock held when it finds the thread
    return thread_list_combiner.Run([&] {
        const auto it = threads.find(thread);
        if (it == threads.end()) {
            return POSIX_ESRCH;
        }
        thread->lock->lock();
        if (!include_dead && thread->state == PthreadState::Dead) {
            thread->lock->unlock();
            return POSIX_ESRCH;
        }
        return 0;
    });
}

int ThreadState::RefAdd(Pthread* thread, bool include_dead) {
    if (thread == nullptr) {
        /* Invalid thread: */
        return POSIX_EINVAL;
    }

    if (int ret = FindThread(thread, include_dead); ret != 0) {
        return ret;
    }

    thread->refcount++;
    thread->lock->unlock();
    return 0;
}

void ThreadState::RefDelete(Pthread* thread) {
    thread->lock->lock();
    thread->refcount--;
    TryCollect(thread);
}

} // namespace Libraries::Kernel
