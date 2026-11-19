// SPDX-FileCopyrightText: Copyright 2024-2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <limits>
#include <optional>

#include "core/libraries/kernel/sync/semaphore.h"

#include <retro_atomic.h>
#include "common/combiner.h"
#include "common/logging/log.h"
#include "common/slot_vector.h"
#include "core/libraries/kernel/kernel.h"
#include "core/libraries/kernel/orbis_error.h"
#include "core/libraries/kernel/posix_error.h"
#include "core/libraries/kernel/threads/pthread.h"
#include "core/libraries/kernel/time.h"
#include "core/libraries/libs.h"

namespace Libraries::Kernel {

constexpr s32 ORBIS_KERNEL_SEM_VALUE_MAX = 0x7FFFFFFF;

struct PthreadSem {
    explicit PthreadSem(s32 value_) : value{value_} {}

    retro_atomic_int_t value;
    // waiters changes one call at a time here
    Common::Combiner waiters_combiner;
    // Linked through the threads (Pthread::sem_prev, sem_next): a wait adds no list node
    Pthread* waiters{};

    void AddWaiter(Pthread* thread) {
        thread->sem_prev = nullptr;
        thread->sem_next = waiters;
        if (waiters != nullptr) {
            waiters->sem_prev = thread;
        }
        waiters = thread;
        thread->sem_listed = true;
    }

    void RemoveWaiter(Pthread* thread) {
        if (!thread->sem_listed) {
            return;
        }
        if (thread->sem_prev != nullptr) {
            thread->sem_prev->sem_next = thread->sem_next;
        } else {
            waiters = thread->sem_next;
        }
        if (thread->sem_next != nullptr) {
            thread->sem_next->sem_prev = thread->sem_prev;
        }
        thread->sem_prev = nullptr;
        thread->sem_next = nullptr;
        thread->sem_listed = false;
    }
};

class ScopedPthreadCritical {
public:
    explicit ScopedPthreadCritical(Pthread* thread_) : thread{thread_} {
        if (thread != nullptr) {
            retro_atomic_fetch_add_int(&thread->critical_count, 1);
        }
    }

    ~ScopedPthreadCritical() {
        if (thread == nullptr) {
            return;
        }
        const int previous = retro_atomic_fetch_sub_int(&thread->critical_count, 1);
        ASSERT(previous > 0);
        if (previous == 1) {
            PthreadCancelInterrupt();
        }
    }

private:
    Pthread* thread;
};

static bool PthreadSemTryAcquire(PthreadSem* sem) {
    s32 value = static_cast<s32>(retro_atomic_load_acquire_int(&sem->value));
    while (value > 0) {
        if ((retro_atomic_cas_int(&sem->value, value, value - 1) ||
             ((value = static_cast<s32>(retro_atomic_load_acquire_int(&sem->value))), false))) {
            return true;
        }
    }
    return false;
}

static s32 PthreadSemWait(PthreadSem* sem, const OrbisKernelTimespec* abstime, u64 usec = 0) {
    const bool is_reltime = abstime == THR_RELTIME;
    // A relative wait's deadline on the game's time, taken once
    const u64 reltime_deadline = is_reltime ? GameDeadlineIn(usec * 1000) : 0;

    for (;;) {
        // libkernel checks and decrements the value before validating a timeout.
        if (PthreadSemTryAcquire(sem)) {
            return 0;
        }
        if (!is_reltime && abstime != nullptr &&
            (abstime->tv_nsec < 0 || abstime->tv_nsec >= 1'000'000'000)) {
            *__Error() = POSIX_EINVAL;
            return -1;
        }

        ASSERT_MSG(g_curthread != nullptr, "POSIX semaphore wait requires a guest pthread");
        PthreadTestCancel();
        Pthread* curthread = g_curthread;

        bool cancel_before_wait = false;
        {
            ScopedPthreadCritical critical{curthread};
            const bool acquired = sem->waiters_combiner.Run([&] {
                if (PthreadSemTryAcquire(sem)) {
                    return true;
                }

                // This is the host equivalent of entering a cancellation-point umtx wait.
                // Resetting the predicate and publishing the waiter are serialized with
                // sem_post.
                (void)retro_atomic_exchange_int(&curthread->cancel_point, 1);
                curthread->ClearWake();
                if (curthread->ShouldCancel()) {
                    (void)retro_atomic_exchange_int(&curthread->cancel_point, 0);
                    cancel_before_wait = true;
                } else {
                    sem->AddWaiter(curthread);
                }
                return false;
            });
            if (acquired) {
                return 0;
            }
        }
        if (cancel_before_wait) {
            PthreadTestCancel();
        }

        bool woke = false;
        if (is_reltime) {
            if (reltime_deadline > GameNow()) {
                woke = curthread->SleepUntil(reltime_deadline);
            }
        } else {
            woke = curthread->Sleep(abstime, 0, ClockId::Realtime);
        }

        {
            ScopedPthreadCritical critical{curthread};
            sem->waiters_combiner.Run([&] {
                sem->RemoveWaiter(curthread);
                (void)retro_atomic_exchange_int(&curthread->cancel_point, 0);
            });
        }

        // A post racing the timeout wins if its token is still available, as in libkernel's
        // value recheck after _umtx_op returns.
        if (PthreadSemTryAcquire(sem)) {
            return 0;
        }
        PthreadTestCancel();
        if (!woke) {
            *__Error() = POSIX_ETIMEDOUT;
            return -1;
        }
    }
}

class OrbisSem {
public:
    OrbisSem(s32 init_count, s32 max_count, std::string_view name, bool is_fifo)
        : name{name}, token_count{init_count}, max_count{max_count}, init_count{init_count},
          is_fifo{is_fifo} {}
    ~OrbisSem() = default;

    s32 Wait(bool can_block, s32 need_count, u32* timeout) {
        // The combiner may run this on another thread: the caller is named here
        Pthread* const curthread = g_curthread;
        std::optional<WaitingThread> waiter;
        const s32 early = combiner.Run([&]() -> s32 {
            if (static_cast<s32>(retro_atomic_load_seq_cst_int(&token_count)) >= need_count) {
                retro_atomic_fetch_sub_int(&token_count, need_count);
                return ORBIS_OK;
            }
            if (!can_block) {
                return ORBIS_KERNEL_ERROR_EBUSY;
            }

            if (timeout && *timeout == 0) {
                return ORBIS_KERNEL_ERROR_ETIMEDOUT;
            }

            // Create waiting thread object and add it into the list of waiters.
            waiter.emplace(need_count, is_fifo, curthread);
            AddWaiter(&*waiter);
            return 1; // waits
        });
        if (early != 1) {
            return early;
        }

        // Perform the wait.
        const s64 waited = waiter->Wait(timeout);
        return combiner.Run([&] {
            if (timeout) {
                if (waiter->was_signaled) {
                    *timeout -= waited;
                } else {
                    *timeout = 0;
                }
            }
            const s32 result = waiter->GetResult();
            if (result == ORBIS_KERNEL_ERROR_ETIMEDOUT) {
                RemoveWaiter(&*waiter);
            }
            return result;
        });
    }

    bool Signal(s32 signal_count) {
        return combiner.Run([&] { return SignalOwned(signal_count); });
    }

    bool SignalOwned(s32 signal_count) {
        if (static_cast<s32>(retro_atomic_load_seq_cst_int(&token_count)) + signal_count >
            max_count) {
            return false;
        }
        retro_atomic_fetch_add_int(&token_count, signal_count);

        // Wake up threads in order of priority.
        for (WaitingThread* waiter = wait_head; waiter != nullptr;) {
            WaitingThread* const next = waiter->next;
            if (waiter->need_count >
                static_cast<s32>(retro_atomic_load_seq_cst_int(&token_count))) {
                waiter = next;
                continue;
            }
            RemoveWaiter(waiter);
            retro_atomic_fetch_sub_int(&token_count, waiter->need_count);
            waiter->was_signaled = true;
            waiter->sem.release();
            waiter = next;
        }

        return true;
    }

    s32 Cancel(s32 set_count, s32* num_waiters) {
        return combiner.Run([&] {
            if (num_waiters) {
                *num_waiters = wait_count;
            }
            while (WaitingThread* waiter = wait_head) {
                RemoveWaiter(waiter);
                waiter->was_canceled = true;
                waiter->sem.release();
            }
            (void)retro_atomic_exchange_int(&token_count, set_count < 0 ? init_count : set_count);
            return ORBIS_OK;
        });
    }

    void Delete() {
        combiner.Run([&] {
            while (WaitingThread* waiter = wait_head) {
                RemoveWaiter(waiter);
                waiter->was_deleted = true;
                waiter->sem.release();
            }
        });
    }

public:
    struct WaitingThread {
        // One wake, and its timer on the game's time
        Semaphore<2> sem;
        u32 priority;
        s32 need_count;
        // Its place in the semaphore's list of waiters: the waiter is on the waiting thread's
        // stack, and is its own list node
        WaitingThread* prev{};
        WaitingThread* next{};
        bool was_signaled{};
        bool was_deleted{};
        bool was_canceled{};

        explicit WaitingThread(s32 need_count, bool is_fifo, Pthread* thread)
            : sem{0}, priority{0}, need_count{need_count} {
            // Retrieve calling thread priority for sorting into waiting threads list.
            if (!is_fifo) {
                priority = thread->attr.prio;
            }
        }

        [[nodiscard]] s32 GetResult() const {
            if (was_signaled) {
                return ORBIS_OK;
            }
            if (was_deleted) {
                return ORBIS_KERNEL_ERROR_EACCES;
            }
            if (was_canceled) {
                return ORBIS_KERNEL_ERROR_ECANCELED;
            }
            return ORBIS_KERNEL_ERROR_ETIMEDOUT;
        }

        /// Blocks until woken or out of time, on the game's time. @return the microseconds
        /// of it that passed.
        s64 Wait(const u32* timeout) {
            if (!timeout) {
                GameSemaWait(sem, 0);
                return 0;
            }
            const u64 start = GameNow();
            GameSemaWait(sem, GameDeadlineIn(static_cast<u64>(*timeout) * 1000));
            return static_cast<s64>((GameNow() - start) / 1000);
        }
    };

    void AddWaiter(WaitingThread* waiter) {
        // At the end for FIFO order; else right before the first with lower priority (greater
        // number) than it
        WaitingThread* before = nullptr;
        if (!is_fifo) {
            before = wait_head;
            while (before != nullptr && before->priority <= waiter->priority) {
                before = before->next;
            }
        }
        waiter->next = before;
        waiter->prev = before != nullptr ? before->prev : wait_tail;
        if (waiter->prev != nullptr) {
            waiter->prev->next = waiter;
        } else {
            wait_head = waiter;
        }
        if (before != nullptr) {
            before->prev = waiter;
        } else {
            wait_tail = waiter;
        }
        ++wait_count;
    }

    void RemoveWaiter(WaitingThread* waiter) {
        if (waiter->prev != nullptr) {
            waiter->prev->next = waiter->next;
        } else {
            wait_head = waiter->next;
        }
        if (waiter->next != nullptr) {
            waiter->next->prev = waiter->prev;
        } else {
            wait_tail = waiter->prev;
        }
        waiter->prev = nullptr;
        waiter->next = nullptr;
        --wait_count;
    }

    // The waiters, in the order they are woken: linked through themselves (WaitingThread::prev,
    // next), so that a wait adds no list node
    WaitingThread* wait_head{};
    WaitingThread* wait_tail{};
    s32 wait_count{};
    std::string name;
    retro_atomic_int_t token_count;
    // The count and the waiters change one call at a time here
    Common::Combiner combiner;
    s32 max_count;
    s32 init_count;
    bool is_fifo;
};

static constexpr u32 MAX_ORBIS_SEMAPHORES = std::numeric_limits<u16>::max();

using OrbisKernelSema = Common::SlotId;

static Common::SlotVector<OrbisSem> orbis_sems;

s32 PS4_SYSV_ABI sceKernelCreateSema(OrbisKernelSema* sem, const char* pName, u32 attr,
                                     s32 initCount, s32 maxCount, const void* pOptParam) {
    if (!pName || attr > 2 || initCount < 0 || maxCount <= 0 || initCount > maxCount) {
        LOG_ERROR(Lib_Kernel, "Semaphore creation parameters are invalid!");
        return ORBIS_KERNEL_ERROR_EINVAL;
    }
    *sem = orbis_sems.Insert(initCount, maxCount, pName, attr == 1);
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceKernelWaitSema(OrbisKernelSema sem, s32 needCount, u32* pTimeout) {
    if (!orbis_sems.IsAllocated(sem)) {
        return ORBIS_KERNEL_ERROR_ESRCH;
    }
    return orbis_sems[sem].Wait(true, needCount, pTimeout);
}

s32 PS4_SYSV_ABI sceKernelSignalSema(OrbisKernelSema sem, s32 signalCount) {
    if (!orbis_sems.IsAllocated(sem)) {
        return ORBIS_KERNEL_ERROR_ESRCH;
    }
    if (!orbis_sems[sem].Signal(signalCount)) {
        return ORBIS_KERNEL_ERROR_EINVAL;
    }
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceKernelPollSema(OrbisKernelSema sem, s32 needCount) {
    if (!orbis_sems.IsAllocated(sem)) {
        return ORBIS_KERNEL_ERROR_ESRCH;
    }
    return orbis_sems[sem].Wait(false, needCount, nullptr);
}

s32 PS4_SYSV_ABI sceKernelCancelSema(OrbisKernelSema sem, s32 setCount, s32* pNumWaitThreads) {
    if (!orbis_sems.IsAllocated(sem)) {
        return ORBIS_KERNEL_ERROR_ESRCH;
    }
    return orbis_sems[sem].Cancel(setCount, pNumWaitThreads);
}

s32 PS4_SYSV_ABI sceKernelDeleteSema(OrbisKernelSema sem) {
    if (!orbis_sems.IsAllocated(sem)) {
        return ORBIS_KERNEL_ERROR_ESRCH;
    }
    orbis_sems[sem].Delete();
    orbis_sems.Erase(sem);
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI posix_sem_init(PthreadSem** sem, s32 /*pshared*/, u32 value) {
    if (value > ORBIS_KERNEL_SEM_VALUE_MAX) {
        *__Error() = POSIX_EINVAL;
        return -1;
    }
    if (sem != nullptr) {
        *sem = new PthreadSem(static_cast<s32>(value));
    }
    return 0;
}

s32 PS4_SYSV_ABI posix_sem_destroy(PthreadSem** sem) {
    if (sem == nullptr || *sem == nullptr) {
        *__Error() = POSIX_EINVAL;
        return -1;
    }
    delete *sem;
    *sem = nullptr;
    return 0;
}

s32 PS4_SYSV_ABI posix_sem_wait(PthreadSem** sem) {
    if (sem == nullptr || *sem == nullptr) {
        *__Error() = POSIX_EINVAL;
        return -1;
    }
    return PthreadSemWait(*sem, nullptr);
}

s32 PS4_SYSV_ABI posix_sem_trywait(PthreadSem** sem) {
    if (sem == nullptr || *sem == nullptr) {
        *__Error() = POSIX_EINVAL;
        return -1;
    }
    if (!PthreadSemTryAcquire(*sem)) {
        *__Error() = POSIX_EAGAIN;
        return -1;
    }
    return 0;
}

s32 PS4_SYSV_ABI posix_sem_timedwait(PthreadSem** sem, const OrbisKernelTimespec* t) {
    if (sem == nullptr || *sem == nullptr) {
        *__Error() = POSIX_EINVAL;
        return -1;
    }
    return PthreadSemWait(*sem, t);
}

s32 PS4_SYSV_ABI posix_sem_reltimedwait_np(PthreadSem** sem, u32 usec) {
    if (sem == nullptr || *sem == nullptr) {
        *__Error() = POSIX_EINVAL;
        return -1;
    }
    return PthreadSemWait(*sem, THR_RELTIME, usec);
}

s32 PS4_SYSV_ABI posix_sem_post(PthreadSem** sem) {
    if (sem == nullptr || *sem == nullptr) {
        *__Error() = POSIX_EINVAL;
        return -1;
    }
    ScopedPthreadCritical critical{g_curthread};
    // Atomically check for overflow and increment in one step.
    s32 current;
    do {
        current = retro_atomic_load_seq_cst_int(&(*sem)->value);
        if (current == ORBIS_KERNEL_SEM_VALUE_MAX) {
            *__Error() = POSIX_EOVERFLOW;
            return -1;
        }
    } while (!retro_atomic_cas_int(&(*sem)->value, current, current + 1));

    (*sem)->waiters_combiner.Run([&] {
        // libkernel wakes every waiter after incrementing the value. Only waiters that win the
        // value CAS return; the rest publish themselves again and continue waiting.
        while (Pthread* waiter = (*sem)->waiters) {
            (*sem)->RemoveWaiter(waiter);
            waiter->wake_sema.release();
        }
    });
    return 0;
}

s32 PS4_SYSV_ABI posix_sem_getvalue(PthreadSem** sem, s32* sval) {
    if (sem == nullptr || *sem == nullptr) {
        *__Error() = POSIX_EINVAL;
        return -1;
    }
    if (sval) {
        *sval = retro_atomic_load_seq_cst_int(&(*sem)->value);
    }
    return 0;
}

s32 PS4_SYSV_ABI posix_sem_setname(PthreadSem** sem, const char* name) {
    if (!name) {
        *__Error() = POSIX_EINVAL;
        return -1;
    }
    return 0;
}

s32 PS4_SYSV_ABI scePthreadSemInit(PthreadSem** sem, s32 flag, u32 value, const char* name) {
    if (flag != 0) {
        return ORBIS_KERNEL_ERROR_EINVAL;
    }

    s32 ret = posix_sem_init(sem, 0, value);
    if (ret != 0) {
        return ErrnoToSceKernelError(*__Error());
    }

    return ORBIS_OK;
}

s32 PS4_SYSV_ABI scePthreadSemDestroy(PthreadSem** sem) {
    s32 ret = posix_sem_destroy(sem);
    if (ret != 0) {
        return ErrnoToSceKernelError(*__Error());
    }

    return ORBIS_OK;
}

s32 PS4_SYSV_ABI scePthreadSemWait(PthreadSem** sem) {
    s32 ret = posix_sem_wait(sem);
    if (ret != 0) {
        return ErrnoToSceKernelError(*__Error());
    }

    return ORBIS_OK;
}

s32 PS4_SYSV_ABI scePthreadSemTrywait(PthreadSem** sem) {
    s32 ret = posix_sem_trywait(sem);
    if (ret != 0) {
        return ErrnoToSceKernelError(*__Error());
    }

    return ORBIS_OK;
}

s32 PS4_SYSV_ABI scePthreadSemTimedwait(PthreadSem** sem, u32 usec) {
    s32 ret = posix_sem_reltimedwait_np(sem, usec);
    if (ret != 0) {
        return ErrnoToSceKernelError(*__Error());
    }

    return ORBIS_OK;
}

s32 PS4_SYSV_ABI scePthreadSemPost(PthreadSem** sem) {
    s32 ret = posix_sem_post(sem);
    if (ret != 0) {
        return ErrnoToSceKernelError(*__Error());
    }

    return ORBIS_OK;
}

s32 PS4_SYSV_ABI scePthreadSemGetvalue(PthreadSem** sem, s32* sval) {
    s32 ret = posix_sem_getvalue(sem, sval);
    if (ret != 0) {
        return ErrnoToSceKernelError(*__Error());
    }

    return ORBIS_OK;
}

void RegisterSemaphore(Core::Loader::SymbolsResolver* sym) {
    orbis_sems.ResizeDestructive(MAX_ORBIS_SEMAPHORES);

    // Orbis
    LIB_FUNCTION("188x57JYp0g", "libkernel", 1, "libkernel", sceKernelCreateSema);
    LIB_FUNCTION("Zxa0VhQVTsk", "libkernel", 1, "libkernel", sceKernelWaitSema);
    LIB_FUNCTION("4czppHBiriw", "libkernel", 1, "libkernel", sceKernelSignalSema);
    LIB_FUNCTION("12wOHk8ywb0", "libkernel", 1, "libkernel", sceKernelPollSema);
    LIB_FUNCTION("4DM06U2BNEY", "libkernel", 1, "libkernel", sceKernelCancelSema);
    LIB_FUNCTION("R1Jvn8bSCW8", "libkernel", 1, "libkernel", sceKernelDeleteSema);

    // Posix
    LIB_FUNCTION("pDuPEf3m4fI", "libScePosix", 1, "libkernel", posix_sem_init);
    LIB_FUNCTION("cDW233RAwWo", "libScePosix", 1, "libkernel", posix_sem_destroy);
    LIB_FUNCTION("YCV5dGGBcCo", "libScePosix", 1, "libkernel", posix_sem_wait);
    LIB_FUNCTION("WBWzsRifCEA", "libScePosix", 1, "libkernel", posix_sem_trywait);
    LIB_FUNCTION("w5IHyvahg-o", "libScePosix", 1, "libkernel", posix_sem_timedwait);
    LIB_FUNCTION("4SbrhCozqQU", "libScePosix", 1, "libkernel", posix_sem_reltimedwait_np);
    LIB_FUNCTION("IKP8typ0QUk", "libScePosix", 1, "libkernel", posix_sem_post);
    LIB_FUNCTION("Bq+LRV-N6Hk", "libScePosix", 1, "libkernel", posix_sem_getvalue);

    LIB_FUNCTION("pDuPEf3m4fI", "libkernel", 1, "libkernel", posix_sem_init);
    LIB_FUNCTION("cDW233RAwWo", "libkernel", 1, "libkernel", posix_sem_destroy);
    LIB_FUNCTION("YCV5dGGBcCo", "libkernel", 1, "libkernel", posix_sem_wait);
    LIB_FUNCTION("WBWzsRifCEA", "libkernel", 1, "libkernel", posix_sem_trywait);
    LIB_FUNCTION("w5IHyvahg-o", "libkernel", 1, "libkernel", posix_sem_timedwait);
    LIB_FUNCTION("4SbrhCozqQU", "libkernel", 1, "libkernel", posix_sem_reltimedwait_np);
    LIB_FUNCTION("IKP8typ0QUk", "libkernel", 1, "libkernel", posix_sem_post);
    LIB_FUNCTION("Bq+LRV-N6Hk", "libkernel", 1, "libkernel", posix_sem_getvalue);
    LIB_FUNCTION("-wUggz2S5yk", "libkernel", 1, "libkernel", posix_sem_setname);

    LIB_FUNCTION("GEnUkDZoUwY", "libkernel", 1, "libkernel", scePthreadSemInit);
    LIB_FUNCTION("Vwc+L05e6oE", "libkernel", 1, "libkernel", scePthreadSemDestroy);
    LIB_FUNCTION("C36iRE0F5sE", "libkernel", 1, "libkernel", scePthreadSemWait);
    LIB_FUNCTION("H2a+IN9TP0E", "libkernel", 1, "libkernel", scePthreadSemTrywait);
    LIB_FUNCTION("fjN6NQHhK8k", "libkernel", 1, "libkernel", scePthreadSemTimedwait);
    LIB_FUNCTION("aishVAiFaYM", "libkernel", 1, "libkernel", scePthreadSemPost);
    LIB_FUNCTION("DjpBvGlaWbQ", "libkernel", 1, "libkernel", scePthreadSemGetvalue);
}

} // namespace Libraries::Kernel
