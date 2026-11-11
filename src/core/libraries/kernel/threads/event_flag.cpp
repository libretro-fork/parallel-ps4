// SPDX-FileCopyrightText: Copyright 2024-2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <chrono>
#include <list>
#include <thread>

#include <rthreads/retro_eventcount.h>

#include "common/assert.h"
#include "common/combiner.h"
#include "common/logging/log.h"
#include "core/libraries/kernel/orbis_error.h"
#include "core/libraries/libs.h"
#ifdef SHADPS4_ENABLE_FEX_GUEST_CPU
#include "core/guest_cpu/hle_call_adapter.h"
#endif
#include <retro_atomic.h>
#include "pthread.h"

namespace Libraries::Kernel {

constexpr int ORBIS_KERNEL_EVF_ATTR_TH_FIFO = 0x01;
constexpr int ORBIS_KERNEL_EVF_ATTR_TH_PRIO = 0x02;
constexpr int ORBIS_KERNEL_EVF_ATTR_SINGLE = 0x10;
constexpr int ORBIS_KERNEL_EVF_ATTR_MULTI = 0x20;

constexpr int ORBIS_KERNEL_EVF_WAITMODE_AND = 0x01;
constexpr int ORBIS_KERNEL_EVF_WAITMODE_OR = 0x02;
constexpr int ORBIS_KERNEL_EVF_WAITMODE_CLEAR_ALL = 0x10;
constexpr int ORBIS_KERNEL_EVF_WAITMODE_CLEAR_PAT = 0x20;

class EventFlagInternal {
public:
    enum class ClearMode { None, All, Bits };
    enum class WaitMode { And, Or };
    enum class ThreadMode { Single, Multi };
    enum class QueueMode { Fifo, ThreadPrio };

    struct Waiter {
        /// Set, after the flags below, once any of them is; the waiter parks on m_wake_ec
        retro_atomic_int_t woken{0};

        u64 bits{};
        WaitMode wait_mode{};
        ClearMode clear_mode{};

        u64 result = 0;

        bool ready = false;
        bool canceled = false;
        bool deleted = false;
        bool timed_out = false;

        int priority = 0;
    };

    EventFlagInternal(const std::string& name, ThreadMode thread_mode, QueueMode queue_mode,
                      u64 bits)
        : m_name(name), m_thread_mode(thread_mode), m_queue_mode(queue_mode), m_bits(bits) {
        retro_eventcount_init(&m_wake_ec);
    }

    ~EventFlagInternal() {
        m_combiner.Run([&] {
            m_deleted = true;

            for (auto* w : m_waiters) {
                w->deleted = true;
                Wake(*w);
            }
        });

        // Until every waiter has left
        for (;;) {
            const int key = retro_eventcount_prepare_wait(&m_wake_ec);
            if (static_cast<u32>(retro_atomic_load_acquire_int(&m_waiter_count)) == 0) {
                retro_eventcount_cancel_wait(&m_wake_ec);
                break;
            }
            retro_eventcount_commit_wait(&m_wake_ec, key);
        }
        retro_eventcount_free(&m_wake_ec);
    }

    bool ConditionMet(const Waiter& waiter) const {
        return (waiter.wait_mode == WaitMode::And) ? (m_bits & waiter.bits) == waiter.bits
                                                   : (m_bits & waiter.bits) != 0;
    }

    void ApplyClear(const Waiter& waiter) {
        switch (waiter.clear_mode) {
        case ClearMode::None:
            break;
        case ClearMode::All:
            m_bits = 0;
            break;
        case ClearMode::Bits:
            m_bits &= ~waiter.bits;
            break;
        default:
            UNREACHABLE();
        }
    }

    void AddWaiter(Waiter& waiter) {
        retro_atomic_fetch_add_int(&m_waiter_count, 1);
        if (m_queue_mode == QueueMode::Fifo) {
            m_waiters.push_back(&waiter);
            return;
        }

        auto it = m_waiters.begin();
        while (it != m_waiters.end() && (*it)->priority >= waiter.priority) {
            ++it;
        }
        m_waiters.insert(it, &waiter);
    }

    void RemoveWaiter(Waiter& waiter) {
        m_waiters.remove(&waiter);
        if (static_cast<u32>(retro_atomic_fetch_sub_int(&m_waiter_count, 1)) == 1 && m_deleted) {
            retro_eventcount_notify(&m_wake_ec);
        }
    }

    void Wake(Waiter& waiter) {
        retro_atomic_store_release_int(&waiter.woken, 1);
        retro_eventcount_notify(&m_wake_ec);
    }

    int Wait(u64 bits, WaitMode wait_mode, ClearMode clear_mode, u64* result, u32* ptr_micros) {
        Waiter waiter{
            .bits = bits,
            .wait_mode = wait_mode,
            .clear_mode = clear_mode,
            .priority = g_curthread->attr.prio,
        };

        constexpr int Waits = 1;
        const int early = m_combiner.Run([&] {
            if (m_deleted) {
                return ORBIS_KERNEL_ERROR_EACCES;
            }

            if (m_thread_mode == ThreadMode::Single && !m_waiters.empty()) {
                return ORBIS_KERNEL_ERROR_EPERM;
            }

            if (ConditionMet(waiter)) {
                if (result)
                    *result = m_bits;

                ApplyClear(waiter);
                return ORBIS_OK;
            }

            AddWaiter(waiter);
            return Waits;
        });
        if (early != Waits) {
            return early;
        }

        const bool infinite = ptr_micros == nullptr;
        const auto deadline = std::chrono::steady_clock::now() +
                              std::chrono::microseconds(infinite ? 0 : *ptr_micros);

        // Until woken, or out of time
        bool out_of_time = false;
        while (retro_atomic_load_acquire_int(&waiter.woken) == 0) {
            const int key = retro_eventcount_prepare_wait(&m_wake_ec);
            if (retro_atomic_load_acquire_int(&waiter.woken) != 0) {
                retro_eventcount_cancel_wait(&m_wake_ec);
                break;
            }
            if (infinite) {
                retro_eventcount_commit_wait(&m_wake_ec, key);
                continue;
            }
            const auto left = std::chrono::ceil<std::chrono::microseconds>(
                deadline - std::chrono::steady_clock::now());
            if (left.count() <= 0) {
                retro_eventcount_cancel_wait(&m_wake_ec);
                out_of_time = true;
                break;
            }
            retro_eventcount_commit_wait_timeout(&m_wake_ec, key, left.count());
        }

        return m_combiner.Run([&] {
            RemoveWaiter(waiter);

            // Woken meanwhile: that stands
            if (out_of_time && retro_atomic_load_relaxed_int(&waiter.woken) == 0) {
                if (result)
                    *result = m_bits;

                *ptr_micros = 0;
                return ORBIS_KERNEL_ERROR_ETIMEDOUT;
            }

            if (result)
                *result = waiter.result;

            if (waiter.canceled)
                return ORBIS_KERNEL_ERROR_ECANCELED;

            if (waiter.deleted)
                return ORBIS_KERNEL_ERROR_EACCES;

            if (waiter.timed_out)
                return ORBIS_KERNEL_ERROR_ETIMEDOUT;

            return ORBIS_OK;
        });
    }

    int Poll(u64 bits, WaitMode wait_mode, ClearMode clear_mode, u64* result) {
        u32 micros = 0;
        auto ret = Wait(bits, wait_mode, clear_mode, result, &micros);
        if (ret == ORBIS_KERNEL_ERROR_ETIMEDOUT) {
            // Poll returns EBUSY instead.
            ret = ORBIS_KERNEL_ERROR_EBUSY;
        }
        return ret;
    }

    void Set(u64 bits) {
        m_combiner.Run([&] {
            m_bits |= bits;

            for (auto it : m_waiters) {
                Waiter* waiter = it;

                if (!ConditionMet(*waiter)) {
                    continue;
                }

                waiter->result = m_bits;
                ApplyClear(*waiter);

                waiter->ready = true;
                Wake(*waiter);
            }
        });
    }

    void Clear(u64 bits) {
        m_combiner.Run([&] { m_bits &= bits; });
    }

    void Cancel(u64 pattern, int* num_waiters) {
        m_combiner.Run([&] {
            if (num_waiters)
                *num_waiters = static_cast<int>(m_waiters.size());

            m_bits = pattern;

            for (Waiter* waiter : m_waiters) {
                waiter->result = pattern;
                waiter->canceled = true;
                Wake(*waiter);
            }
        });
    }

private:
    // The bits and the waiters change one call at a time here; the waiters park on m_wake_ec
    Common::Combiner m_combiner;
    retro_eventcount_t m_wake_ec{};
    retro_atomic_int_t m_waiter_count{0};

    std::list<Waiter*> m_waiters;

    std::string m_name;

    ThreadMode m_thread_mode;
    QueueMode m_queue_mode;
    bool m_deleted = false;

    u64 m_bits = 0;
};

using OrbisKernelUseconds = u32;
using OrbisKernelEventFlag = EventFlagInternal*;

struct OrbisKernelEventFlagOptParam {
    size_t size;
};

int PS4_SYSV_ABI sceKernelCreateEventFlag(OrbisKernelEventFlag* ef, const char* pName, u32 attr,
                                          u64 initPattern,
                                          const OrbisKernelEventFlagOptParam* pOptParam) {
    LOG_TRACE(Kernel_Event, "called name = {} attr = {:#x} initPattern = {:#x}", pName, attr,
              initPattern);
    if (ef == nullptr || pName == nullptr) {
        return ORBIS_KERNEL_ERROR_EINVAL;
    }
    if (pOptParam || attr > (ORBIS_KERNEL_EVF_ATTR_MULTI | ORBIS_KERNEL_EVF_ATTR_TH_PRIO)) {
        return ORBIS_KERNEL_ERROR_EINVAL;
    }

    if (strlen(pName) >= 32) {
        return ORBIS_KERNEL_ERROR_ENAMETOOLONG;
    }

    auto thread_mode = EventFlagInternal::ThreadMode::Single;
    auto queue_mode = EventFlagInternal::QueueMode::Fifo;
    switch (attr & 0xfu) {
    case 0x01:
        queue_mode = EventFlagInternal::QueueMode::Fifo;
        break;
    case 0x02:
        queue_mode = EventFlagInternal::QueueMode::ThreadPrio;
        break;
    case 0x00:
        break;
    default:
        UNREACHABLE();
    }

    switch (attr & 0xf0) {
    case 0x10:
        thread_mode = EventFlagInternal::ThreadMode::Single;
        break;
    case 0x20:
        thread_mode = EventFlagInternal::ThreadMode::Multi;
        break;
    case 0x00:
        break;
    default:
        UNREACHABLE();
    }

    *ef = new EventFlagInternal(std::string(pName), thread_mode, queue_mode, initPattern);
#ifdef SHADPS4_ENABLE_FEX_GUEST_CPU
    // The handle is a host pointer the guest passes back: FEX's HLE bridge checks it
    (void)Core::GuestCpu::PublishHostRange(*ef, sizeof(EventFlagInternal), true);
#endif
    return ORBIS_OK;
}

int PS4_SYSV_ABI sceKernelDeleteEventFlag(OrbisKernelEventFlag ef) {
    if (ef == nullptr) {
        return ORBIS_KERNEL_ERROR_ESRCH;
    }

#ifdef SHADPS4_ENABLE_FEX_GUEST_CPU
    (void)Core::GuestCpu::RevokeHostRange(ef);
#endif
    delete ef;
    return ORBIS_OK;
}

int PS4_SYSV_ABI sceKernelOpenEventFlag() {
    LOG_ERROR(Kernel_Event, "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI sceKernelCloseEventFlag() {
    LOG_ERROR(Kernel_Event, "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI sceKernelClearEventFlag(OrbisKernelEventFlag ef, u64 bitPattern) {
    LOG_DEBUG(Kernel_Event, "called");
    if (ef == nullptr) {
        return ORBIS_KERNEL_ERROR_ESRCH;
    }
    ef->Clear(bitPattern);
    return ORBIS_OK;
}

int PS4_SYSV_ABI sceKernelCancelEventFlag(OrbisKernelEventFlag ef, u64 setPattern,
                                          int* pNumWaitThreads) {
    LOG_DEBUG(Kernel_Event, "called");
    if (ef == nullptr) {
        return ORBIS_KERNEL_ERROR_ESRCH;
    }
    ef->Cancel(setPattern, pNumWaitThreads);
    return ORBIS_OK;
}

int PS4_SYSV_ABI sceKernelSetEventFlag(OrbisKernelEventFlag ef, u64 bitPattern) {
    LOG_TRACE(Kernel_Event, "called");
    if (ef == nullptr) {
        return ORBIS_KERNEL_ERROR_ESRCH;
    }
    ef->Set(bitPattern);
    return ORBIS_OK;
}

int PS4_SYSV_ABI sceKernelPollEventFlag(OrbisKernelEventFlag ef, u64 bitPattern, u32 waitMode,
                                        u64* pResultPat) {
    LOG_DEBUG(Kernel_Event, "called bitPattern = {:#x} waitMode = {:#x}", bitPattern, waitMode);

    if (ef == nullptr) {
        return ORBIS_KERNEL_ERROR_ESRCH;
    }

    if (bitPattern == 0) {
        return ORBIS_KERNEL_ERROR_EINVAL;
    }

    auto wait = EventFlagInternal::WaitMode::And;
    auto clear = EventFlagInternal::ClearMode::None;
    switch (waitMode & 0xf) {
    case 0x01:
        wait = EventFlagInternal::WaitMode::And;
        break;
    case 0x02:
        wait = EventFlagInternal::WaitMode::Or;
        break;
    default:
        UNREACHABLE();
    }

    switch (waitMode & 0xf0) {
    case 0x00:
        clear = EventFlagInternal::ClearMode::None;
        break;
    case 0x10:
        clear = EventFlagInternal::ClearMode::All;
        break;
    case 0x20:
        clear = EventFlagInternal::ClearMode::Bits;
        break;
    default:
        UNREACHABLE();
    }

    auto result = ef->Poll(bitPattern, wait, clear, pResultPat);

    if (result != ORBIS_OK && result != ORBIS_KERNEL_ERROR_EBUSY) {
        LOG_DEBUG(Kernel_Event, "returned {:#x}", result);
    }

    return result;
}
int PS4_SYSV_ABI sceKernelWaitEventFlag(OrbisKernelEventFlag ef, u64 bitPattern, u32 waitMode,
                                        u64* pResultPat, OrbisKernelUseconds* pTimeout) {
    LOG_DEBUG(Kernel_Event, "called bitPattern = {:#x} waitMode = {:#x}", bitPattern, waitMode);
    if (ef == nullptr) {
        return ORBIS_KERNEL_ERROR_ESRCH;
    }

    if (bitPattern == 0) {
        return ORBIS_KERNEL_ERROR_EINVAL;
    }

    auto wait = EventFlagInternal::WaitMode::And;
    auto clear = EventFlagInternal::ClearMode::None;
    switch (waitMode & 0xf) {
    case 0x01:
        wait = EventFlagInternal::WaitMode::And;
        break;
    case 0x02:
        wait = EventFlagInternal::WaitMode::Or;
        break;
    default:
        UNREACHABLE();
    }

    switch (waitMode & 0xf0) {
    case 0x00:
        clear = EventFlagInternal::ClearMode::None;
        break;
    case 0x10:
        clear = EventFlagInternal::ClearMode::All;
        break;
    case 0x20:
        clear = EventFlagInternal::ClearMode::Bits;
        break;
    default:
        UNREACHABLE();
    }

    const int result = ef->Wait(bitPattern, wait, clear, pResultPat, pTimeout);
    if (result != ORBIS_OK && result != ORBIS_KERNEL_ERROR_ETIMEDOUT) {
        LOG_DEBUG(Kernel_Event, "returned {:#x}", result);
    }

    return result;
}

void RegisterKernelEventFlag(Core::Loader::SymbolsResolver* sym) {
    LIB_FUNCTION("PZku4ZrXJqg", "libkernel", 1, "libkernel", sceKernelCancelEventFlag);
    LIB_FUNCTION("7uhBFWRAS60", "libkernel", 1, "libkernel", sceKernelClearEventFlag);
    LIB_FUNCTION("s9-RaxukuzQ", "libkernel", 1, "libkernel", sceKernelCloseEventFlag);
    LIB_FUNCTION("BpFoboUJoZU", "libkernel", 1, "libkernel", sceKernelCreateEventFlag);
    LIB_FUNCTION("8mql9OcQnd4", "libkernel", 1, "libkernel", sceKernelDeleteEventFlag);
    LIB_FUNCTION("1vDaenmJtyA", "libkernel", 1, "libkernel", sceKernelOpenEventFlag);
    LIB_FUNCTION("9lvj5DjHZiA", "libkernel", 1, "libkernel", sceKernelPollEventFlag);
    LIB_FUNCTION("IOnSvHzqu6A", "libkernel", 1, "libkernel", sceKernelSetEventFlag);
    LIB_FUNCTION("JTvBflhYazQ", "libkernel", 1, "libkernel", sceKernelWaitEventFlag);
}

} // namespace Libraries::Kernel
