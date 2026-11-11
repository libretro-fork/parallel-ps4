// SPDX-FileCopyrightText: Copyright 2024-2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <limits>
#include <list>
#include <optional>
#include <semaphore>

#include "core/libraries/kernel/sync/semaphore.h"

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

    std::atomic<s32> value;
    // waiters changes one call at a time here
    Common::Combiner waiters_combiner;
    std::list<Pthread*> waiters;
};

class ScopedPthreadCritical {
public:
    explicit ScopedPthreadCritical(Pthread* thread_) : thread{thread_} {
        if (thread != nullptr) {
            thread->critical_count.fetch_add(1, std::memory_order_acq_rel);
        }
    }

    ~ScopedPthreadCritical() {
        if (thread == nullptr) {
            return;
        }
        const int previous = thread->critical_count.fetch_sub(1, std::memory_order_acq_rel);
        ASSERT(previous > 0);
        if (previous == 1) {
            PthreadCancelInterrupt();
        }
    }

private:
    Pthread* thread;
};

static bool PthreadSemTryAcquire(PthreadSem* sem) {
    s32 value = sem->value.load(std::memory_order_acquire);
    while (value > 0) {
        if (sem->value.compare_exchange_weak(value, value - 1, std::memory_order_acq_rel,
                                             std::memory_order_acquire)) {
            return true;
        }
    }
    return false;
}

static s32 PthreadSemWait(PthreadSem* sem, const OrbisKernelTimespec* abstime, u64 usec = 0) {
    const bool is_reltime = abstime == THR_RELTIME;
    auto reltime_deadline = std::chrono::steady_clock::time_point{};
    bool first_sleep = true;

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
                curthread->cancel_point = true;
                curthread->ClearWake();
                if (curthread->ShouldCancel()) {
                    curthread->cancel_point = false;
                    cancel_before_wait = true;
                } else {
                    sem->waiters.push_back(curthread);
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
            if (first_sleep) {
                const auto now = std::chrono::steady_clock::now();
                const auto max_relative = std::chrono::duration_cast<std::chrono::microseconds>(
                                              std::chrono::steady_clock::time_point::max() - now)
                                              .count();
                reltime_deadline = usec > static_cast<u64>(max_relative)
                                       ? std::chrono::steady_clock::time_point::max()
                                       : now + std::chrono::microseconds(usec);
                first_sleep = false;
                woke = curthread->Sleep(THR_RELTIME, usec);
            } else if (const auto current = std::chrono::steady_clock::now();
                       current < reltime_deadline) {
                const auto remaining =
                    std::chrono::ceil<std::chrono::microseconds>(reltime_deadline - current);
                woke = curthread->Sleep(THR_RELTIME, static_cast<u64>(remaining.count()));
            }
        } else {
            woke = curthread->Sleep(abstime, 0, ClockId::Realtime);
        }

        {
            ScopedPthreadCritical critical{curthread};
            sem->waiters_combiner.Run([&] {
                sem->waiters.remove(curthread);
                curthread->cancel_point = false;
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
        WaitList::iterator it;
        const s32 early = combiner.Run([&]() -> s32 {
            if (token_count >= need_count) {
                token_count -= need_count;
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
            it = AddWaiter(&*waiter);
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
                wait_list.erase(it);
            }
            return result;
        });
    }

    bool Signal(s32 signal_count) {
        return combiner.Run([&] { return SignalOwned(signal_count); });
    }

    bool SignalOwned(s32 signal_count) {
        if (token_count + signal_count > max_count) {
            return false;
        }
        token_count += signal_count;

        // Wake up threads in order of priority.
        for (auto it = wait_list.begin(); it != wait_list.end();) {
            auto* waiter = *it;
            if (waiter->need_count > token_count) {
                ++it;
                continue;
            }
            it = wait_list.erase(it);
            token_count -= waiter->need_count;
            waiter->was_signaled = true;
            waiter->sem.release();
        }

        return true;
    }

    s32 Cancel(s32 set_count, s32* num_waiters) {
        return combiner.Run([&] {
            if (num_waiters) {
                *num_waiters = static_cast<s32>(wait_list.size());
            }
            for (auto* waiter : wait_list) {
                waiter->was_canceled = true;
                waiter->sem.release();
            }
            wait_list.clear();
            token_count = set_count < 0 ? init_count : set_count;
            return ORBIS_OK;
        });
    }

    void Delete() {
        combiner.Run([&] {
            for (auto* waiter : wait_list) {
                waiter->was_deleted = true;
                waiter->sem.release();
            }
            wait_list.clear();
        });
    }

public:
    struct WaitingThread {
        BinarySemaphore sem;
        u32 priority;
        s32 need_count;
        std::string thr_name;
        bool was_signaled{};
        bool was_deleted{};
        bool was_canceled{};

        explicit WaitingThread(s32 need_count, bool is_fifo, Pthread* thread)
            : sem{0}, priority{0}, need_count{need_count} {
            // Retrieve calling thread priority for sorting into waiting threads list.
            if (!is_fifo) {
                priority = thread->attr.prio;
            }

            thr_name = thread->name;
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

        /// Blocks until woken or out of time. @return the microseconds it waited.
        s64 Wait(const u32* timeout) {
            if (!timeout) {
                // Wait indefinitely until we are woken up.
                sem.acquire();
                return 0;
            }
            // Wait until timeout runs out, recording how much time it took.
            const auto start = std::chrono::high_resolution_clock::now();
            sem.try_acquire_for(std::chrono::microseconds(*timeout));
            const auto end = std::chrono::high_resolution_clock::now();
            return std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
        }
    };

    using WaitList = std::list<WaitingThread*>;

    WaitList::iterator AddWaiter(WaitingThread* waiter) {
        // Insert at the end of the list for FIFO order.
        if (is_fifo) {
            wait_list.push_back(waiter);
            return --wait_list.end();
        }
        // Find the first with lower priority (greater number) than us and insert right before it.
        auto it = wait_list.begin();
        while (it != wait_list.end() && (*it)->priority <= waiter->priority) {
            ++it;
        }
        return wait_list.insert(it, waiter);
    }

    WaitList wait_list;
    std::string name;
    std::atomic<s32> token_count;
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
    s32 current = (*sem)->value.load();
    do {
        if (current == ORBIS_KERNEL_SEM_VALUE_MAX) {
            *__Error() = POSIX_EOVERFLOW;
            return -1;
        }
    } while (!(*sem)->value.compare_exchange_weak(current, current + 1));

    (*sem)->waiters_combiner.Run([&] {
        // libkernel wakes every waiter after incrementing the value. Only waiters that win the
        // value CAS return; the rest publish themselves again and continue waiting.
        for (Pthread* waiter : (*sem)->waiters) {
            waiter->wake_sema.release();
        }
        (*sem)->waiters.clear();
    });
    return 0;
}

s32 PS4_SYSV_ABI posix_sem_getvalue(PthreadSem** sem, s32* sval) {
    if (sem == nullptr || *sem == nullptr) {
        *__Error() = POSIX_EINVAL;
        return -1;
    }
    if (sval) {
        *sval = (*sem)->value;
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
