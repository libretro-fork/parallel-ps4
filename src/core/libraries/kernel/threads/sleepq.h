// SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <list>

#include "common/intrusive_list.h"

namespace Libraries::Kernel {

struct Pthread;
struct SleepQueue;

// A queue takes itself out of whatever list holds it when destroyed
using ListBaseHook = Common::ListHook;

using SleepqList = Common::IntrusiveList<SleepQueue>;

struct SleepQueue : public ListBaseHook {
    std::list<Pthread*> sq_blocked;
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
