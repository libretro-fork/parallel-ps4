// SPDX-FileCopyrightText: Copyright 2024-2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <array>
#include <atomic>
#include <chrono>
#include <cstring>
#include <mutex>
#include <thread>

#include <queues/mpsc_stack.h>
#include <rthreads/retro_eventcount.h>
#include <rthreads/rthreads.h>

#include "aio.h"
#include "common/assert.h"
#include "common/logging/log.h"
#include "core/libraries/kernel/equeue.h"
#include "core/libraries/kernel/orbis_error.h"
#include "core/libraries/libs.h"
#include "file_system.h"

namespace Libraries::Kernel {

// Asynchronous I/O, asynchronous: a submit hands its requests to a worker thread and returns,
// as on the console, where it read or wrote them first. Each submit id is a batch slot made
// once; game threads push batches onto a lock-free stack, the worker takes them all at once,
// does them in order and wakes whoever waits. No lock and no allocation once running.
static constexpr s32 ORBIS_KERNEL_AIO_MAX_QUEUES = 512;
// The most requests one submit takes on the console
static constexpr u32 AioMaxBatch = 128;

struct AioBatch {
    mpsc_stack_node_t node; // first: the stack links batches through it
    std::atomic<s32> state{0};
    bool write = false;
    u32 count = 0;
    std::array<OrbisKernelAioRWRequest, AioMaxBatch> requests{};
};

static std::array<AioBatch, ORBIS_KERNEL_AIO_MAX_QUEUES> g_batches;
static std::atomic<u32> g_next_id{0};
static mpsc_stack_t g_pending;
static retro_eventcount_t g_work;
static retro_eventcount_t g_done;
static std::once_flag g_worker_once;

static void AioDo(const OrbisKernelAioRWRequest& req, bool write, std::atomic<s32>& state) {
    if (state.load(std::memory_order_acquire) == ORBIS_KERNEL_AIO_STATE_ABORTED) {
        req.result->state = ORBIS_KERNEL_AIO_STATE_ABORTED;
        return;
    }
    const s64 ret = write ? sceKernelPwrite(req.fd, req.buf, req.nbyte, req.offset)
                          : sceKernelPread(req.fd, req.buf, req.nbyte, req.offset);
    req.result->returnValue = ret;
    req.result->state = ret < 0 ? ORBIS_KERNEL_AIO_STATE_ABORTED : ORBIS_KERNEL_AIO_STATE_COMPLETED;
}

static void AioWorker(void*) {
    for (;;) {
        const int key = retro_eventcount_prepare_wait(&g_work);
        mpsc_stack_node_t* node = mpsc_stack_drain(&g_pending);
        if (!node) {
            retro_eventcount_commit_wait(&g_work, key);
            continue;
        }
        retro_eventcount_cancel_wait(&g_work);
        // Pushed newest first: in submit order again
        for (node = mpsc_stack_reverse(node); node;) {
            auto* batch = reinterpret_cast<AioBatch*>(node);
            node = node->next;
            bool failed = false;
            for (u32 i = 0; i < batch->count; i++) {
                AioDo(batch->requests[i], batch->write, batch->state);
                failed |= batch->requests[i].result->state == ORBIS_KERNEL_AIO_STATE_ABORTED;
            }
            // Done, unless it was cancelled meanwhile
            s32 expected = ORBIS_KERNEL_AIO_STATE_PROCESSING;
            batch->state.compare_exchange_strong(
                expected,
                failed ? ORBIS_KERNEL_AIO_STATE_ABORTED : ORBIS_KERNEL_AIO_STATE_COMPLETED,
                std::memory_order_acq_rel);
        }
        retro_eventcount_notify(&g_done);
    }
}

static AioBatch* Batch(OrbisKernelAioSubmitId id) {
    return id > 0 && id < ORBIS_KERNEL_AIO_MAX_QUEUES ? &g_batches[id] : nullptr;
}

static s32 State(OrbisKernelAioSubmitId id) {
    const AioBatch* batch = Batch(id);
    return batch ? batch->state.load(std::memory_order_acquire) : 0;
}

static void SetState(OrbisKernelAioSubmitId id, s32 state) {
    if (AioBatch* batch = Batch(id)) {
        batch->state.store(state, std::memory_order_release);
    }
}

// Hands @count requests to the worker under one id. A batch larger than one slot takes, which
// the console refuses, is done here as before.
static OrbisKernelAioSubmitId Submit(const OrbisKernelAioRWRequest* req, u32 count, bool write) {
    std::call_once(g_worker_once, [] {
        mpsc_stack_init(&g_pending);
        retro_eventcount_init(&g_work);
        retro_eventcount_init(&g_done);
        sthread_t* thread = sthread_create(AioWorker, nullptr);
        ASSERT_MSG(thread, "No thread for asynchronous I/O");
        sthread_detach(thread);
    });
    // Ids 1 to 511 in turn; 0 is the cancel functions' own
    const OrbisKernelAioSubmitId id = static_cast<OrbisKernelAioSubmitId>(
        1 + g_next_id.fetch_add(1, std::memory_order_relaxed) % (ORBIS_KERNEL_AIO_MAX_QUEUES - 1));
    AioBatch& batch = g_batches[id];
    // An id comes round again after 511 submits; one still in flight then is waited out
    while (batch.state.load(std::memory_order_acquire) == ORBIS_KERNEL_AIO_STATE_PROCESSING) {
        std::this_thread::yield();
    }
    for (u32 i = 0; i < count; i++) {
        req[i].result->state = ORBIS_KERNEL_AIO_STATE_PROCESSING;
    }
    if (count > AioMaxBatch) {
        batch.state.store(ORBIS_KERNEL_AIO_STATE_PROCESSING, std::memory_order_relaxed);
        bool failed = false;
        for (u32 i = 0; i < count; i++) {
            AioDo(req[i], write, batch.state);
            failed |= req[i].result->state == ORBIS_KERNEL_AIO_STATE_ABORTED;
        }
        batch.state.store(failed ? ORBIS_KERNEL_AIO_STATE_ABORTED
                                 : ORBIS_KERNEL_AIO_STATE_COMPLETED,
                          std::memory_order_release);
        return id;
    }
    batch.write = write;
    batch.count = count;
    std::memcpy(batch.requests.data(), req, count * sizeof(*req));
    batch.state.store(ORBIS_KERNEL_AIO_STATE_PROCESSING, std::memory_order_relaxed);
    mpsc_stack_push(&g_pending, &batch.node);
    retro_eventcount_notify(&g_work);
    return id;
}

// Waits until @id is no longer in flight, at most @usec microseconds when it is not 0.
// False when that ran out.
static bool WaitDone(OrbisKernelAioSubmitId id, u32 usec) {
    if (!Batch(id)) {
        return true;
    }
    const auto start = std::chrono::steady_clock::now();
    for (;;) {
        const int key = retro_eventcount_prepare_wait(&g_done);
        if (State(id) != ORBIS_KERNEL_AIO_STATE_PROCESSING) {
            retro_eventcount_cancel_wait(&g_done);
            return true;
        }
        if (usec == 0) {
            retro_eventcount_commit_wait(&g_done, key);
            continue;
        }
        const s64 waited = std::chrono::duration_cast<std::chrono::microseconds>(
                               std::chrono::steady_clock::now() - start)
                               .count();
        if (waited >= usec) {
            retro_eventcount_cancel_wait(&g_done);
            return State(id) != ORBIS_KERNEL_AIO_STATE_PROCESSING;
        }
        retro_eventcount_commit_wait_timeout(&g_done, key, usec - waited);
    }
}

s32 PS4_SYSV_ABI sceKernelAioInitializeImpl(void* p, s32 size) {
    return 0;
}

s32 PS4_SYSV_ABI sceKernelAioDeleteRequest(OrbisKernelAioSubmitId id, s32* ret) {
    if (ret == nullptr) {
        return ORBIS_KERNEL_ERROR_EFAULT;
    }
    SetState(id, ORBIS_KERNEL_AIO_STATE_ABORTED);
    *ret = 0;
    return 0;
}

s32 PS4_SYSV_ABI sceKernelAioDeleteRequests(OrbisKernelAioSubmitId id[], s32 num, s32 ret[]) {
    if (ret == nullptr) {
        return ORBIS_KERNEL_ERROR_EFAULT;
    }
    for (s32 i = 0; i < num; i++) {
        SetState(id[i], ORBIS_KERNEL_AIO_STATE_ABORTED);
        ret[i] = 0;
    }

    return 0;
}
s32 PS4_SYSV_ABI sceKernelAioPollRequest(OrbisKernelAioSubmitId id, s32* state) {
    if (state == nullptr) {
        return ORBIS_KERNEL_ERROR_EFAULT;
    }
    *state = State(id);
    return 0;
}

s32 PS4_SYSV_ABI sceKernelAioPollRequests(OrbisKernelAioSubmitId id[], s32 num, s32 state[]) {
    if (state == nullptr) {
        return ORBIS_KERNEL_ERROR_EFAULT;
    }
    for (s32 i = 0; i < num; i++) {
        state[i] = State(id[i]);
    }

    return 0;
}

s32 PS4_SYSV_ABI sceKernelAioCancelRequest(OrbisKernelAioSubmitId id, s32* state) {
    if (state == nullptr) {
        return ORBIS_KERNEL_ERROR_EFAULT;
    }
    if (id) {
        SetState(id, ORBIS_KERNEL_AIO_STATE_ABORTED);
        *state = ORBIS_KERNEL_AIO_STATE_ABORTED;
    } else {
        *state = ORBIS_KERNEL_AIO_STATE_PROCESSING;
    }
    return 0;
}

s32 PS4_SYSV_ABI sceKernelAioCancelRequests(OrbisKernelAioSubmitId id[], s32 num, s32 state[]) {
    if (state == nullptr) {
        return ORBIS_KERNEL_ERROR_EFAULT;
    }
    for (s32 i = 0; i < num; i++) {
        if (id[i]) {
            SetState(id[i], ORBIS_KERNEL_AIO_STATE_ABORTED);
            state[i] = ORBIS_KERNEL_AIO_STATE_ABORTED;
        } else {
            state[i] = ORBIS_KERNEL_AIO_STATE_PROCESSING;
        }
    }

    return 0;
}

s32 PS4_SYSV_ABI sceKernelAioWaitRequest(OrbisKernelAioSubmitId id, s32* state, u32* usec) {
    if (state == nullptr) {
        return ORBIS_KERNEL_ERROR_EFAULT;
    }
    const bool done = WaitDone(id, usec ? *usec : 0);
    *state = State(id);
    return done ? 0 : ORBIS_KERNEL_ERROR_ETIMEDOUT;
}

s32 PS4_SYSV_ABI sceKernelAioWaitRequests(OrbisKernelAioSubmitId id[], s32 num, s32 state[],
                                          u32 mode, u32* usec) {
    if (state == nullptr) {
        return ORBIS_KERNEL_ERROR_EFAULT;
    }
    // Mode 0x01 waits for all of them, 0x02 for any one
    const u32 limit = usec ? *usec : 0;
    const auto start = std::chrono::steady_clock::now();
    bool timeout = false;
    for (;;) {
        const int key = retro_eventcount_prepare_wait(&g_done);
        s32 done = 0;
        for (s32 i = 0; i < num; i++) {
            state[i] = State(id[i]);
            done += state[i] != ORBIS_KERNEL_AIO_STATE_PROCESSING;
        }
        if (num == 0 || (mode == 0x02 ? done > 0 : done == num)) {
            retro_eventcount_cancel_wait(&g_done);
            break;
        }
        if (limit == 0) {
            retro_eventcount_commit_wait(&g_done, key);
            continue;
        }
        const s64 waited = std::chrono::duration_cast<std::chrono::microseconds>(
                               std::chrono::steady_clock::now() - start)
                               .count();
        if (waited >= limit) {
            retro_eventcount_cancel_wait(&g_done);
            timeout = true;
            break;
        }
        retro_eventcount_commit_wait_timeout(&g_done, key, limit - waited);
    }
    return timeout ? ORBIS_KERNEL_ERROR_ETIMEDOUT : 0;
}

s32 PS4_SYSV_ABI sceKernelAioSubmitReadCommands(OrbisKernelAioRWRequest req[], s32 size, s32 prio,
                                                OrbisKernelAioSubmitId* id) {
    if (req == nullptr || id == nullptr || size < 0) {
        return ORBIS_KERNEL_ERROR_EFAULT;
    }
    *id = Submit(req, static_cast<u32>(size), false);
    return 0;
}

s32 PS4_SYSV_ABI sceKernelAioSubmitReadCommandsMultiple(OrbisKernelAioRWRequest req[], s32 size,
                                                        s32 prio, OrbisKernelAioSubmitId id[]) {
    if (req == nullptr || id == nullptr || size < 0) {
        return ORBIS_KERNEL_ERROR_EFAULT;
    }
    for (s32 i = 0; i < size; i++) {
        id[i] = Submit(&req[i], 1, false);
    }
    return 0;
}

s32 PS4_SYSV_ABI sceKernelAioSubmitWriteCommands(OrbisKernelAioRWRequest req[], s32 size, s32 prio,
                                                 OrbisKernelAioSubmitId* id) {
    if (req == nullptr || id == nullptr || size < 0) {
        return ORBIS_KERNEL_ERROR_EFAULT;
    }
    *id = Submit(req, static_cast<u32>(size), true);
    return 0;
}

s32 PS4_SYSV_ABI sceKernelAioSubmitWriteCommandsMultiple(OrbisKernelAioRWRequest req[], s32 size,
                                                         s32 prio, OrbisKernelAioSubmitId id[]) {
    if (req == nullptr || id == nullptr || size < 0) {
        return ORBIS_KERNEL_ERROR_EFAULT;
    }
    for (s32 i = 0; i < size; i++) {
        id[i] = Submit(&req[i], 1, true);
    }
    return 0;
}

s32 PS4_SYSV_ABI sceKernelAioSetParam() {
    LOG_ERROR(Kernel, "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceKernelAioInitializeParam() {
    LOG_ERROR(Kernel, "(STUBBED) called");
    return ORBIS_OK;
}

void RegisterAio(Core::Loader::SymbolsResolver* sym) {
    LIB_FUNCTION("fR521KIGgb8", "libkernel", 1, "libkernel", sceKernelAioCancelRequest);
    LIB_FUNCTION("3Lca1XBrQdY", "libkernel", 1, "libkernel", sceKernelAioCancelRequests);
    LIB_FUNCTION("5TgME6AYty4", "libkernel", 1, "libkernel", sceKernelAioDeleteRequest);
    LIB_FUNCTION("Ft3EtsZzAoY", "libkernel", 1, "libkernel", sceKernelAioDeleteRequests);
    LIB_FUNCTION("vYU8P9Td2Zo", "libkernel", 1, "libkernel", sceKernelAioInitializeImpl);
    LIB_FUNCTION("nu4a0-arQis", "libkernel", 1, "libkernel", sceKernelAioInitializeParam);
    LIB_FUNCTION("2pOuoWoCxdk", "libkernel", 1, "libkernel", sceKernelAioPollRequest);
    LIB_FUNCTION("o7O4z3jwKzo", "libkernel", 1, "libkernel", sceKernelAioPollRequests);
    LIB_FUNCTION("9WK-vhNXimw", "libkernel", 1, "libkernel", sceKernelAioSetParam);
    LIB_FUNCTION("HgX7+AORI58", "libkernel", 1, "libkernel", sceKernelAioSubmitReadCommands);
    LIB_FUNCTION("lXT0m3P-vs4", "libkernel", 1, "libkernel",
                 sceKernelAioSubmitReadCommandsMultiple);
    LIB_FUNCTION("XQ8C8y+de+E", "libkernel", 1, "libkernel", sceKernelAioSubmitWriteCommands);
    LIB_FUNCTION("xT3Cpz0yh6Y", "libkernel", 1, "libkernel",
                 sceKernelAioSubmitWriteCommandsMultiple);
    LIB_FUNCTION("KOF-oJbQVvc", "libkernel", 1, "libkernel", sceKernelAioWaitRequest);
    LIB_FUNCTION("lgK+oIWkJyA", "libkernel", 1, "libkernel", sceKernelAioWaitRequests);
}

} // namespace Libraries::Kernel