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
    thread_live.assign(thread_heap.GetSlabHeapSize(), 0);
}

bool ThreadState::IsThreadRecord(const Pthread* thread) const {
    const auto address = reinterpret_cast<uintptr_t>(thread);
    return thread_heap.Contains(address) &&
           (address - thread_heap.GetSlabHeapAddress()) % sizeof(Pthread) == 0;
}

void ThreadState::LinkLocked(Pthread* thread) {
    u8& live = thread_live[thread_heap.GetObjectIndex(thread)];
    if (live) {
        return;
    }
    live = 1;
    thread->thread_prev = nullptr;
    thread->thread_next = threads;
    if (threads != nullptr) {
        threads->thread_prev = thread;
    }
    threads = thread;
}

void ThreadState::UnlinkLocked(Pthread* thread) {
    u8& live = thread_live[thread_heap.GetObjectIndex(thread)];
    if (!live) {
        return;
    }
    live = 0;
    if (thread->thread_prev != nullptr) {
        thread->thread_prev->thread_next = thread->thread_next;
    } else {
        threads = thread->thread_next;
    }
    if (thread->thread_next != nullptr) {
        thread->thread_next->thread_prev = thread->thread_prev;
    }
    thread->thread_prev = nullptr;
    thread->thread_next = nullptr;
}

void ThreadState::Collect(Pthread* curthread) {
    // The ended ones, taken off the list and linked through the same field
    Pthread* work_list = nullptr;
    thread_list_combiner.Run([&] {
        for (Pthread** link = &gc_list; *link != nullptr;) {
            Pthread* td = *link;
            if (static_cast<s32>(retro_atomic_load_seq_cst_int(&td->tid)) != TidTerminated) {
                link = &td->list_next;
                continue;
            }
            FreeStack(&td->attr);
            *link = td->list_next;
            td->list_next = work_list;
            work_list = td;
            retro_atomic_fetch_sub_size(&gc_count, 1);
        }
    });
    while (Pthread* td = work_list) {
        work_list = td->list_next;
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
            UnlinkLocked(thread);
            thread->list_next = gc_list;
            gc_list = thread;
            retro_atomic_fetch_add_size(&gc_count, 1);
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
            if (free_threads != nullptr) {
                thread = free_threads;
                free_threads = thread->list_next;
                --free_count;
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
        // What the record holds that allocates is kept for its next thread: its lock, its
        // sleep queue, its host thread's object and its name's storage. Only a record never
        // used before makes them. A recycled record's previous host thread, ended, is released
        // here (it was left to leak, with a Windows handle and what rthreads holds for it, as
        // the record was cleared over it).
        std::unique_ptr<Common::EventMutex> lock = std::move(thread->lock);
        std::unique_ptr<Core::NativeThread> native = std::move(thread->native_thr);
        SleepQueue* sleepqueue = thread->sleepqueue;
        std::string name = std::move(thread->name);
        std::destroy_at(thread);
        // Initialize thread struct memory to 0. This is safe since it will be constructed
        // immediately after.
        std::memset(static_cast<void*>(thread), 0, sizeof(Pthread));
        std::construct_at(thread);
        thread->tcb = tcb;
        if (lock) {
            std::destroy_at(lock.get());
            std::construct_at(lock.get());
        } else {
            lock = std::make_unique<Common::EventMutex>();
        }
        thread->lock = std::move(lock);
        if (native) {
            std::destroy_at(native.get());
            std::construct_at(native.get());
        }
        thread->native_thr = std::move(native);
        name.clear();
        thread->name = std::move(name);
        thread->sleepqueue = sleepqueue != nullptr ? sleepqueue : new SleepQueue{};
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
        if (free_count >= MaxCachedThreads) {
            should_free = true;
        } else {
            should_free = false;
            thread->list_next = free_threads;
            free_threads = thread;
            ++free_count;
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
        // A pointer of the game's: a record of the heap, and one of a live thread
        if (!IsThreadRecord(thread) || !thread_live[thread_heap.GetObjectIndex(thread)]) {
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
