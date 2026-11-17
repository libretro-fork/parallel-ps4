// SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <list>

#include "common/intrusive_list_cpp.h"

namespace Libraries::Kernel {

struct Pthread;
struct SleepQueue;

// A queue takes itself out of whatever list holds it when destroyed
using ListBaseHook = Common::ListHook;

using SleepqList = Common::IntrusiveList<SleepQueue>;

struct SleepQueue : public ListBaseHook {
    // The threads blocked on it, oldest first (FreeBSD's TAILQ), linked through the threads
    // themselves (Pthread::sq_prev, sq_next): blocking adds no list node
    Pthread* sq_blocked{};
    Pthread* sq_blocked_tail{};
    SleepqList sq_freeq;
    void* sq_wchan;
    int sq_type;
};

void SleepqLock(void* wchan);

void SleepqUnlock(void* wchan);

SleepQueue* SleepqLookup(void* wchan);

void SleepqAdd(void* wchan, Pthread* td);

bool SleepqRemove(SleepQueue* sq, Pthread* td);

void SleepqDrop(SleepQueue* sq, void (*callback)(Pthread*, void*), void* arg);

} // namespace Libraries::Kernel
