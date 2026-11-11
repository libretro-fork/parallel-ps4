// SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

// libSceUlt: user-level threads and the objects they wait on (queue, mutex, semaphore,
// condition variable). Used when the system folder has no libSceUlt.sprx; with the module
// present it is loaded and this is not registered.
//
// A user-level thread is a thread of the game's here, not a fiber switched on the runtime's
// worker threads. What the game can tell of the difference is kept: as many user-level
// threads of a runtime run at once as it has worker threads. Each holds one of the runtime's
// run tokens while it runs, gives it up when it waits in one of the library's objects (where
// the real library switches to another user-level thread) and takes one again to go on. With
// one worker thread, then, they run one at a time and change only at those waits, as on a PS4.
//
// The game's objects hold a tag and a pointer to their state here; the functions find the
// state through it, with no table and no lock. The names, sizes and error values follow the
// PS4's library (the option parameters' header and defaults and the work area sizes as
// AnyPS5 has them).

#include <cstring>

#include <retro_atomic.h>

#include "common/logging/log.h"
#include "core/guest_call.h"
#include "core/libraries/kernel/threads.h"
#include "core/libraries/kernel/threads/pthread.h"
#include "core/libraries/libs.h"
#include "core/libraries/ult/ult.h"
#include "core/libraries/ult/ult_sync.h"

namespace Libraries::Kernel {
void PS4_SYSV_ABI sched_yield();
}

namespace Libraries::Ult {

namespace {

constexpr s32 ORBIS_ULT_OK = 0;
constexpr s32 ORBIS_ULT_ERROR_NULL = static_cast<s32>(0x80810001);
constexpr s32 ORBIS_ULT_ERROR_ALIGNMENT = static_cast<s32>(0x80810002);
constexpr s32 ORBIS_ULT_ERROR_RANGE = static_cast<s32>(0x80810003);
constexpr s32 ORBIS_ULT_ERROR_INVALID = static_cast<s32>(0x80810004);
constexpr s32 ORBIS_ULT_ERROR_PERMISSION = static_cast<s32>(0x80810005);
constexpr s32 ORBIS_ULT_ERROR_STATE = static_cast<s32>(0x80810006);
constexpr s32 ORBIS_ULT_ERROR_BUSY = static_cast<s32>(0x80810007);
constexpr s32 ORBIS_ULT_ERROR_AGAIN = static_cast<s32>(0x80810008);

/// Every option parameter block: 128 bytes, a header, the SDK version, then its fields
constexpr u32 OptParamSize = 128;
constexpr u32 OptParamHeader[3] = {0x6C696275, 0x6C746D61, 0x67696300};

using UlthreadEntry = s32 PS4_SYSV_ABI (*)(u64 arg);

enum class Kind : u32 {
    WaitingPool = 1,
    DataPool,
    Queue,
    Runtime,
    Ulthread,
    Mutex,
    Semaphore,
    Condition,
};

/// The start of each of the game's objects, once created
struct Handle {
    u32 magic;
    Kind kind;
    void* state;
};
constexpr u32 HandleMagic = 0x544C5553; // "SULT"

struct WaitingPoolState {
    u32 num_threads;
    u32 num_sync_objects;
};

struct DataPoolState {
    DataPoolState(void* work_area, u32 capacity, u64 item_size, u32 max_queues_)
        : pool{work_area, capacity, item_size}, max_queues{max_queues_} {
        retro_atomic_int_init(&queues, 0);
    }
    Sync::DataPool pool;
    u32 max_queues;
    retro_atomic_int_t queues;
};

struct QueueState {
    explicit QueueState(DataPoolState& data_pool_)
        : queue{data_pool_.pool}, data_pool{data_pool_} {}
    Sync::Queue queue;
    DataPoolState& data_pool;
};

struct RuntimeState {
    RuntimeState(u32 max_ulthreads_, u32 workers_)
        : tokens{static_cast<int>(workers_)}, max_ulthreads{max_ulthreads_}, workers{workers_} {
        retro_atomic_int_init(&ulthreads, 0);
    }
    Sync::Tokens tokens;
    u32 max_ulthreads;
    u32 workers;
    retro_atomic_int_t ulthreads;
};

struct UlthreadState {
    void* object; // the game's
    RuntimeState* runtime;
    UlthreadEntry entry;
    u64 arg;
    Kernel::PthreadT thread;
    s32 status; // written before exited is set
    retro_atomic_int_t exited;
    retro_atomic_int_t joined;
    Common::EventCount done;
};

struct MutexState {
    explicit MutexState(bool recursive) : mutex{recursive} {}
    Sync::Mutex mutex;
};

struct SemaphoreState {
    explicit SemaphoreState(int resources) : semaphore{resources} {}
    Sync::Semaphore semaphore;
};

struct ConditionState {
    explicit ConditionState(Sync::Mutex& mutex) : condition{mutex} {}
    Sync::Condition condition;
};

/// The user-level thread this thread is, if it is one
thread_local UlthreadState* t_self = nullptr;

s32 CheckObject(const void* object) {
    if (object == nullptr) {
        return ORBIS_ULT_ERROR_NULL;
    }
    if ((reinterpret_cast<uintptr_t>(object) & 7u) != 0) {
        return ORBIS_ULT_ERROR_ALIGNMENT;
    }
    return ORBIS_ULT_OK;
}

void Bind(void* object, Kind kind, void* state) {
    const Handle handle{HandleMagic, kind, state};
    std::memcpy(object, &handle, sizeof(handle));
}

void Unbind(void* object) {
    std::memset(object, 0, sizeof(Handle));
}

/// The state of the game's object, or null with *error set.
template <typename T>
T* Find(const void* object, Kind kind, s32* error) {
    *error = CheckObject(object);
    if (*error != ORBIS_ULT_OK) {
        return nullptr;
    }
    Handle handle;
    std::memcpy(&handle, object, sizeof(handle));
    if (handle.magic != HandleMagic || handle.kind != kind || handle.state == nullptr) {
        *error = ORBIS_ULT_ERROR_STATE;
        return nullptr;
    }
    return static_cast<T*>(handle.state);
}

/// An optional block of option parameters: null, or one with the header.
s32 CheckOptParam(const void* opt_param) {
    if (opt_param == nullptr) {
        return ORBIS_ULT_OK;
    }
    if ((reinterpret_cast<uintptr_t>(opt_param) & 7u) != 0) {
        return ORBIS_ULT_ERROR_ALIGNMENT;
    }
    return std::memcmp(opt_param, OptParamHeader, sizeof(OptParamHeader)) == 0
               ? ORBIS_ULT_OK
               : ORBIS_ULT_ERROR_INVALID;
}

s32 InitOptParam(void* opt_param, u32 build_version) {
    if (opt_param == nullptr) {
        return ORBIS_ULT_ERROR_NULL;
    }
    std::memset(opt_param, 0, OptParamSize);
    std::memcpy(opt_param, OptParamHeader, sizeof(OptParamHeader));
    std::memcpy(static_cast<u8*>(opt_param) + sizeof(OptParamHeader), &build_version,
                sizeof(build_version));
    return ORBIS_ULT_OK;
}

void GiveToken() {
    if (t_self) {
        t_self->runtime->tokens.Give();
    }
}

void TakeToken() {
    if (t_self) {
        t_self->runtime->tokens.Take();
    }
}

/// The end of a user-level thread, by returning or by sceUltUlthreadExit.
void FinishUlthread(UlthreadState* self, s32 status) {
    self->status = status;
    t_self = nullptr;
    self->runtime->tokens.Give();
    retro_atomic_store_release_int(&self->exited, 1);
    self->done.Notify();
}

void* PS4_SYSV_ABI RunUlthread(void* arg) {
    auto* self = static_cast<UlthreadState*>(arg);
    t_self = self;
    self->runtime->tokens.Take();
    const s32 status = Core::CallGuest(self->entry, self->arg);
    // Not if it left through sceUltUlthreadExit, which has done this
    if (t_self == self) {
        FinishUlthread(self, status);
    }
    return nullptr;
}

} // Anonymous namespace

s32 PS4_SYSV_ABI sceUltInitialize() {
    LOG_INFO(Lib_Ult, "called");
    return ORBIS_ULT_OK;
}

s32 PS4_SYSV_ABI sceUltFinalize() {
    LOG_INFO(Lib_Ult, "called");
    return ORBIS_ULT_OK;
}

// ---- option parameters ----

s32 PS4_SYSV_ABI _sceUltOptParamInitialize(void* opt_param, u32 build_version) {
    return InitOptParam(opt_param, build_version);
}

s32 PS4_SYSV_ABI _sceUltUlthreadRuntimeOptParamInitialize(void* opt_param, u32 build_version) {
    // The console's defaults, after the header and version
    static constexpr u32 defaults[] = {0x4000, 0, 0x1FFF, 0, 0x2BC, 4};
    const s32 result = InitOptParam(opt_param, build_version);
    if (result == ORBIS_ULT_OK) {
        std::memcpy(static_cast<u8*>(opt_param) + 16, defaults, sizeof(defaults));
    }
    return result;
}

// ---- waiting queue resource pool ----

u64 PS4_SYSV_ABI sceUltWaitingQueueResourcePoolGetWorkAreaSize(u32 num_threads,
                                                               u32 num_sync_objects) {
    return 32ull * (u64{num_sync_objects} + 2ull * num_threads + 2);
}

s32 PS4_SYSV_ABI _sceUltWaitingQueueResourcePoolCreate(void* pool, const char* name,
                                                       u32 num_threads, u32 num_sync_objects,
                                                       void* work_area, const void* opt_param,
                                                       u32 build_version) {
    LOG_INFO(Lib_Ult, "'{}': {} threads, {} objects", name ? name : "", num_threads,
             num_sync_objects);
    if (const s32 error = CheckObject(pool); error != ORBIS_ULT_OK) {
        return error;
    }
    if (work_area == nullptr) {
        return ORBIS_ULT_ERROR_NULL;
    }
    if (const s32 error = CheckOptParam(opt_param); error != ORBIS_ULT_OK) {
        return error;
    }
    // Nothing waits in it here: the objects park their own waiters
    Bind(pool, Kind::WaitingPool, new WaitingPoolState{num_threads, num_sync_objects});
    return ORBIS_ULT_OK;
}

s32 PS4_SYSV_ABI sceUltWaitingQueueResourcePoolDestroy(void* pool) {
    s32 error;
    auto* state = Find<WaitingPoolState>(pool, Kind::WaitingPool, &error);
    if (!state) {
        return error;
    }
    Unbind(pool);
    delete state;
    return ORBIS_ULT_OK;
}

// ---- queue data resource pool ----

u64 PS4_SYSV_ABI sceUltQueueDataResourcePoolGetWorkAreaSize(u32 num_data, u64 data_size,
                                                            u32 num_queue_objects) {
    return u64{num_data} * data_size + 32ull * (u64{num_queue_objects} + 2ull * num_data + 3);
}

s32 PS4_SYSV_ABI _sceUltQueueDataResourcePoolCreate(void* pool, const char* name, u32 num_data,
                                                    u64 data_size, u32 num_queue_objects,
                                                    void* waiting_pool, void* work_area,
                                                    const void* opt_param, u32 build_version) {
    LOG_INFO(Lib_Ult, "'{}': {} items of {} bytes, {} queues", name ? name : "", num_data,
             data_size, num_queue_objects);
    if (const s32 error = CheckObject(pool); error != ORBIS_ULT_OK) {
        return error;
    }
    if (work_area == nullptr) {
        return ORBIS_ULT_ERROR_NULL;
    }
    if (num_data == 0 || data_size == 0 || num_queue_objects == 0) {
        return ORBIS_ULT_ERROR_RANGE;
    }
    if (const s32 error = CheckOptParam(opt_param); error != ORBIS_ULT_OK) {
        return error;
    }
    if (waiting_pool != nullptr) {
        s32 error;
        if (!Find<WaitingPoolState>(waiting_pool, Kind::WaitingPool, &error)) {
            return ORBIS_ULT_ERROR_INVALID;
        }
    }
    // The items live in the game's work area, which is larger than they need
    // (sceUltQueueDataResourcePoolGetWorkAreaSize); only the pool's few words are here
    static_assert(Sync::DataPool::WorkAreaSize(1, 1) <= 1 + 32 * (1 + 2 + 3));
    Bind(pool, Kind::DataPool,
         new DataPoolState{work_area, num_data, data_size, num_queue_objects});
    return ORBIS_ULT_OK;
}

s32 PS4_SYSV_ABI sceUltQueueDataResourcePoolDestroy(void* pool) {
    s32 error;
    auto* state = Find<DataPoolState>(pool, Kind::DataPool, &error);
    if (!state) {
        return error;
    }
    if (retro_atomic_load_acquire_int(&state->queues) != 0) {
        return ORBIS_ULT_ERROR_STATE;
    }
    Unbind(pool);
    delete state;
    return ORBIS_ULT_OK;
}

// ---- queue ----

s32 PS4_SYSV_ABI _sceUltQueueCreate(void* queue, const char* name, u64 data_size,
                                    void* waiting_pool, void* data_pool, const void* opt_param,
                                    u32 build_version) {
    LOG_INFO(Lib_Ult, "'{}': items of {} bytes", name ? name : "", data_size);
    if (const s32 error = CheckObject(queue); error != ORBIS_ULT_OK) {
        return error;
    }
    if (const s32 error = CheckOptParam(opt_param); error != ORBIS_ULT_OK) {
        return error;
    }
    s32 error;
    auto* pool = Find<DataPoolState>(data_pool, Kind::DataPool, &error);
    if (!pool) {
        return data_pool == nullptr ? ORBIS_ULT_ERROR_NULL : ORBIS_ULT_ERROR_INVALID;
    }
    if (data_size != pool->pool.ItemSize()) {
        return ORBIS_ULT_ERROR_INVALID;
    }
    if (static_cast<u32>(retro_atomic_fetch_add_int(&pool->queues, 1)) >= pool->max_queues) {
        retro_atomic_fetch_sub_int(&pool->queues, 1);
        return ORBIS_ULT_ERROR_AGAIN;
    }
    Bind(queue, Kind::Queue, new QueueState{*pool});
    return ORBIS_ULT_OK;
}

s32 PS4_SYSV_ABI sceUltQueueDestroy(void* queue) {
    s32 error;
    auto* state = Find<QueueState>(queue, Kind::Queue, &error);
    if (!state) {
        return error;
    }
    Unbind(queue);
    // What it still holds goes back to the pool
    while (state->queue.TryPop(nullptr)) {
    }
    retro_atomic_fetch_sub_int(&state->data_pool.queues, 1);
    delete state;
    return ORBIS_ULT_OK;
}

s32 PS4_SYSV_ABI sceUltQueuePush(void* queue, const void* data) {
    s32 error;
    auto* state = Find<QueueState>(queue, Kind::Queue, &error);
    if (!state) {
        return error;
    }
    if (data == nullptr) {
        return ORBIS_ULT_ERROR_NULL;
    }
    state->queue.Push(data);
    return ORBIS_ULT_OK;
}

s32 PS4_SYSV_ABI sceUltQueueTryPush(void* queue, const void* data) {
    s32 error;
    auto* state = Find<QueueState>(queue, Kind::Queue, &error);
    if (!state) {
        return error;
    }
    if (data == nullptr) {
        return ORBIS_ULT_ERROR_NULL;
    }
    return state->queue.TryPush(data) ? ORBIS_ULT_OK : ORBIS_ULT_ERROR_AGAIN;
}

s32 PS4_SYSV_ABI sceUltQueuePop(void* queue, void* data) {
    s32 error;
    auto* state = Find<QueueState>(queue, Kind::Queue, &error);
    if (!state) {
        return error;
    }
    if (data == nullptr) {
        return ORBIS_ULT_ERROR_NULL;
    }
    state->queue.Pop(data);
    return ORBIS_ULT_OK;
}

s32 PS4_SYSV_ABI sceUltQueueTryPop(void* queue, void* data) {
    s32 error;
    auto* state = Find<QueueState>(queue, Kind::Queue, &error);
    if (!state) {
        return error;
    }
    if (data == nullptr) {
        return ORBIS_ULT_ERROR_NULL;
    }
    return state->queue.TryPop(data) ? ORBIS_ULT_OK : ORBIS_ULT_ERROR_AGAIN;
}

// ---- user-level thread runtime ----

u64 PS4_SYSV_ABI sceUltUlthreadRuntimeGetWorkAreaSize(u32 max_ulthreads, u32 workers) {
    const u64 threads = u64{max_ulthreads} + workers;
    const u64 thread_area = (32ull * (2ull * max_ulthreads + 4) + 4ull * max_ulthreads + 7) & ~7ull;
    const u64 scheduler_area = 32ull * (2ull * workers + 4) + 32ull * (2ull * threads + 4);
    return thread_area + scheduler_area + 512ull * workers + 8ull * (max_ulthreads + threads);
}

s32 PS4_SYSV_ABI _sceUltUlthreadRuntimeCreate(void* runtime, const char* name, u32 max_ulthreads,
                                              u32 workers, void* work_area, const void* opt_param,
                                              u32 build_version) {
    LOG_INFO(Lib_Ult, "'{}': {} user-level threads on {} workers", name ? name : "", max_ulthreads,
             workers);
    if (const s32 error = CheckObject(runtime); error != ORBIS_ULT_OK) {
        return error;
    }
    if (work_area == nullptr) {
        return ORBIS_ULT_ERROR_NULL;
    }
    if (max_ulthreads == 0 || workers == 0) {
        return ORBIS_ULT_ERROR_RANGE;
    }
    if (const s32 error = CheckOptParam(opt_param); error != ORBIS_ULT_OK) {
        return error;
    }
    Bind(runtime, Kind::Runtime, new RuntimeState{max_ulthreads, workers});
    return ORBIS_ULT_OK;
}

s32 PS4_SYSV_ABI sceUltUlthreadRuntimeDestroy(void* runtime) {
    s32 error;
    auto* state = Find<RuntimeState>(runtime, Kind::Runtime, &error);
    if (!state) {
        return error;
    }
    if (retro_atomic_load_acquire_int(&state->ulthreads) != 0) {
        return ORBIS_ULT_ERROR_STATE;
    }
    Unbind(runtime);
    delete state;
    return ORBIS_ULT_OK;
}

// ---- user-level threads ----

s32 PS4_SYSV_ABI _sceUltUlthreadCreate(void* ulthread, const char* name, UlthreadEntry entry,
                                       u64 arg, void* context, u64 context_size, void* runtime,
                                       const void* opt_param, u32 build_version) {
    if (const s32 error = CheckObject(ulthread); error != ORBIS_ULT_OK) {
        return error;
    }
    if (entry == nullptr) {
        return ORBIS_ULT_ERROR_NULL;
    }
    if (const s32 error = CheckOptParam(opt_param); error != ORBIS_ULT_OK) {
        return error;
    }
    s32 error;
    auto* owner = Find<RuntimeState>(runtime, Kind::Runtime, &error);
    if (!owner) {
        return runtime == nullptr ? ORBIS_ULT_ERROR_NULL : ORBIS_ULT_ERROR_INVALID;
    }
    if (static_cast<u32>(retro_atomic_fetch_add_int(&owner->ulthreads, 1)) >=
        owner->max_ulthreads) {
        retro_atomic_fetch_sub_int(&owner->ulthreads, 1);
        return ORBIS_ULT_ERROR_AGAIN;
    }
    auto* state = new UlthreadState{};
    state->object = ulthread;
    state->runtime = owner;
    state->entry = entry;
    state->arg = arg;
    retro_atomic_int_init(&state->exited, 0);
    retro_atomic_int_init(&state->joined, 0);
    Bind(ulthread, Kind::Ulthread, state);
    // It runs on a stack of its own, as the game's threads do: the context the game gives
    // is sized for its code alone, not for the emulator's under it
    if (Kernel::posix_pthread_create_name_np(&state->thread, nullptr, RunUlthread, state,
                                             name ? name : "Ulthread") != 0) {
        LOG_ERROR(Lib_Ult, "user-level thread '{}' not created", name ? name : "");
        Unbind(ulthread);
        retro_atomic_fetch_sub_int(&owner->ulthreads, 1);
        delete state;
        return ORBIS_ULT_ERROR_AGAIN;
    }
    return ORBIS_ULT_OK;
}

static s32 Join(void* ulthread, s32* status, bool wait) {
    s32 error;
    auto* state = Find<UlthreadState>(ulthread, Kind::Ulthread, &error);
    if (!state) {
        return error;
    }
    if (state == t_self) {
        return ORBIS_ULT_ERROR_PERMISSION;
    }
    const auto exited = [state] { return retro_atomic_load_acquire_int(&state->exited) != 0; };
    if (!wait && !exited()) {
        return ORBIS_ULT_ERROR_BUSY;
    }
    // One joiner has it
    if (!retro_atomic_cas_int(&state->joined, 0, 1)) {
        return ORBIS_ULT_ERROR_STATE;
    }
    Sync::ParkUntil(state->done, exited);
    Kernel::posix_pthread_join(state->thread, nullptr);
    if (status) {
        *status = state->status;
    }
    Unbind(ulthread);
    retro_atomic_fetch_sub_int(&state->runtime->ulthreads, 1);
    delete state;
    return ORBIS_ULT_OK;
}

s32 PS4_SYSV_ABI sceUltUlthreadJoin(void* ulthread, s32* status) {
    return Join(ulthread, status, true);
}

s32 PS4_SYSV_ABI sceUltUlthreadTryJoin(void* ulthread, s32* status) {
    return Join(ulthread, status, false);
}

s32 PS4_SYSV_ABI sceUltUlthreadExit(s32 status) {
    UlthreadState* self = t_self;
    if (!self) {
        return ORBIS_ULT_ERROR_PERMISSION;
    }
    FinishUlthread(self, status);
    Kernel::posix_pthread_exit(nullptr);
    return ORBIS_ULT_OK;
}

s32 PS4_SYSV_ABI sceUltUlthreadGetSelf(void** ulthread) {
    if (ulthread == nullptr) {
        return ORBIS_ULT_ERROR_NULL;
    }
    if (!t_self) {
        return ORBIS_ULT_ERROR_PERMISSION;
    }
    *ulthread = t_self->object;
    return ORBIS_ULT_OK;
}

s32 PS4_SYSV_ABI sceUltUlthreadYield() {
    if (!t_self) {
        return ORBIS_ULT_ERROR_PERMISSION;
    }
    // To another user-level thread waiting for a worker, if there is one
    GiveToken();
    Kernel::sched_yield();
    TakeToken();
    return ORBIS_ULT_OK;
}

// ---- mutex ----

s32 PS4_SYSV_ABI _sceUltMutexCreate(void* mutex, const char* name, void* waiting_pool,
                                    const void* opt_param, u32 build_version) {
    if (const s32 error = CheckObject(mutex); error != ORBIS_ULT_OK) {
        return error;
    }
    if (const s32 error = CheckOptParam(opt_param); error != ORBIS_ULT_OK) {
        return error;
    }
    u32 attribute = 0; // after the header and version: not 0 for a recursive mutex
    if (opt_param) {
        std::memcpy(&attribute, static_cast<const u8*>(opt_param) + 16, sizeof(attribute));
    }
    Bind(mutex, Kind::Mutex, new MutexState{attribute != 0});
    return ORBIS_ULT_OK;
}

s32 PS4_SYSV_ABI sceUltMutexDestroy(void* mutex) {
    s32 error;
    auto* state = Find<MutexState>(mutex, Kind::Mutex, &error);
    if (!state) {
        return error;
    }
    Unbind(mutex);
    delete state;
    return ORBIS_ULT_OK;
}

s32 PS4_SYSV_ABI sceUltMutexLock(void* mutex) {
    s32 error;
    auto* state = Find<MutexState>(mutex, Kind::Mutex, &error);
    if (!state) {
        return error;
    }
    return state->mutex.Lock() == Sync::Mutex::Result::Locked ? ORBIS_ULT_OK
                                                              : ORBIS_ULT_ERROR_STATE;
}

s32 PS4_SYSV_ABI sceUltMutexTryLock(void* mutex) {
    s32 error;
    auto* state = Find<MutexState>(mutex, Kind::Mutex, &error);
    if (!state) {
        return error;
    }
    switch (state->mutex.TryLock()) {
    case Sync::Mutex::Result::Locked:
        return ORBIS_ULT_OK;
    case Sync::Mutex::Result::Busy:
        return ORBIS_ULT_ERROR_BUSY;
    default:
        return ORBIS_ULT_ERROR_STATE;
    }
}

s32 PS4_SYSV_ABI sceUltMutexUnlock(void* mutex) {
    s32 error;
    auto* state = Find<MutexState>(mutex, Kind::Mutex, &error);
    if (!state) {
        return error;
    }
    return state->mutex.Unlock() ? ORBIS_ULT_OK : ORBIS_ULT_ERROR_PERMISSION;
}

// ---- semaphore ----

s32 PS4_SYSV_ABI _sceUltSemaphoreCreate(void* semaphore, const char* name, u32 resources,
                                        void* waiting_pool, const void* opt_param,
                                        u32 build_version) {
    if (const s32 error = CheckObject(semaphore); error != ORBIS_ULT_OK) {
        return error;
    }
    if (const s32 error = CheckOptParam(opt_param); error != ORBIS_ULT_OK) {
        return error;
    }
    if (resources > 0x7FFFFFFFu) {
        return ORBIS_ULT_ERROR_RANGE;
    }
    Bind(semaphore, Kind::Semaphore, new SemaphoreState{static_cast<int>(resources)});
    return ORBIS_ULT_OK;
}

s32 PS4_SYSV_ABI sceUltSemaphoreDestroy(void* semaphore) {
    s32 error;
    auto* state = Find<SemaphoreState>(semaphore, Kind::Semaphore, &error);
    if (!state) {
        return error;
    }
    Unbind(semaphore);
    delete state;
    return ORBIS_ULT_OK;
}

s32 PS4_SYSV_ABI sceUltSemaphoreAcquire(void* semaphore, s32 count) {
    s32 error;
    auto* state = Find<SemaphoreState>(semaphore, Kind::Semaphore, &error);
    if (!state) {
        return error;
    }
    if (count <= 0) {
        return ORBIS_ULT_ERROR_RANGE;
    }
    state->semaphore.Acquire(count);
    return ORBIS_ULT_OK;
}

s32 PS4_SYSV_ABI sceUltSemaphoreTryAcquire(void* semaphore, s32 count) {
    s32 error;
    auto* state = Find<SemaphoreState>(semaphore, Kind::Semaphore, &error);
    if (!state) {
        return error;
    }
    if (count <= 0) {
        return ORBIS_ULT_ERROR_RANGE;
    }
    return state->semaphore.TryAcquire(count) ? ORBIS_ULT_OK : ORBIS_ULT_ERROR_AGAIN;
}

s32 PS4_SYSV_ABI sceUltSemaphoreRelease(void* semaphore, s32 count) {
    s32 error;
    auto* state = Find<SemaphoreState>(semaphore, Kind::Semaphore, &error);
    if (!state) {
        return error;
    }
    if (count <= 0) {
        return ORBIS_ULT_ERROR_RANGE;
    }
    state->semaphore.Release(count);
    return ORBIS_ULT_OK;
}

// ---- condition variable ----

s32 PS4_SYSV_ABI _sceUltConditionVariableCreate(void* condition, const char* name, void* mutex,
                                                const void* opt_param, u32 build_version) {
    if (const s32 error = CheckObject(condition); error != ORBIS_ULT_OK) {
        return error;
    }
    if (const s32 error = CheckOptParam(opt_param); error != ORBIS_ULT_OK) {
        return error;
    }
    s32 error;
    auto* owner = Find<MutexState>(mutex, Kind::Mutex, &error);
    if (!owner) {
        return mutex == nullptr ? ORBIS_ULT_ERROR_NULL : ORBIS_ULT_ERROR_INVALID;
    }
    Bind(condition, Kind::Condition, new ConditionState{owner->mutex});
    return ORBIS_ULT_OK;
}

s32 PS4_SYSV_ABI sceUltConditionVariableDestroy(void* condition) {
    s32 error;
    auto* state = Find<ConditionState>(condition, Kind::Condition, &error);
    if (!state) {
        return error;
    }
    Unbind(condition);
    delete state;
    return ORBIS_ULT_OK;
}

s32 PS4_SYSV_ABI sceUltConditionVariableWait(void* condition) {
    s32 error;
    auto* state = Find<ConditionState>(condition, Kind::Condition, &error);
    if (!state) {
        return error;
    }
    return state->condition.Wait() ? ORBIS_ULT_OK : ORBIS_ULT_ERROR_PERMISSION;
}

s32 PS4_SYSV_ABI sceUltConditionVariableSignal(void* condition) {
    s32 error;
    auto* state = Find<ConditionState>(condition, Kind::Condition, &error);
    if (!state) {
        return error;
    }
    state->condition.Signal();
    return ORBIS_ULT_OK;
}

s32 PS4_SYSV_ABI sceUltConditionVariableSignalAll(void* condition) {
    s32 error;
    auto* state = Find<ConditionState>(condition, Kind::Condition, &error);
    if (!state) {
        return error;
    }
    state->condition.SignalAll();
    return ORBIS_ULT_OK;
}

void RegisterLib(Core::Loader::SymbolsResolver* sym) {
    Sync::before_park = GiveToken;
    Sync::after_park = TakeToken;

    LIB_FUNCTION("hZIg1EWGsHM", "libSceUlt", 1, "libSceUlt", sceUltInitialize);
    LIB_FUNCTION("d-kSG2fLrvI", "libSceUlt", 1, "libSceUlt", sceUltFinalize);
    LIB_FUNCTION("LuLTRt0rfTw", "libSceUlt", 1, "libSceUlt",
                 _sceUltOptParamInitialize); // _sceUltWaitingQueueResourcePoolOptParamInitialize
    LIB_FUNCTION("WIWV1Qd7PFU", "libSceUlt", 1, "libSceUlt",
                 sceUltWaitingQueueResourcePoolGetWorkAreaSize);
    LIB_FUNCTION("YiHujOG9vXY", "libSceUlt", 1, "libSceUlt", _sceUltWaitingQueueResourcePoolCreate);
    LIB_FUNCTION("or55417wcDk", "libSceUlt", 1, "libSceUlt", sceUltWaitingQueueResourcePoolDestroy);
    LIB_FUNCTION("6gYjd50q0CE", "libSceUlt", 1, "libSceUlt",
                 _sceUltOptParamInitialize); // _sceUltQueueDataResourcePoolOptParamInitialize
    LIB_FUNCTION("evj9YPkS8s4", "libSceUlt", 1, "libSceUlt",
                 sceUltQueueDataResourcePoolGetWorkAreaSize);
    LIB_FUNCTION("TFHm6-N6vks", "libSceUlt", 1, "libSceUlt", _sceUltQueueDataResourcePoolCreate);
    LIB_FUNCTION("dh11uAUWNyM", "libSceUlt", 1, "libSceUlt", sceUltQueueDataResourcePoolDestroy);
    LIB_FUNCTION("TkASc9I-xX0", "libSceUlt", 1, "libSceUlt",
                 _sceUltOptParamInitialize); // _sceUltQueueOptParamInitialize
    LIB_FUNCTION("9Y5keOvb6ok", "libSceUlt", 1, "libSceUlt", _sceUltQueueCreate);
    LIB_FUNCTION("PP9nZxpSKLY", "libSceUlt", 1, "libSceUlt", sceUltQueueDestroy);
    LIB_FUNCTION("dUwpX3e5NDE", "libSceUlt", 1, "libSceUlt", sceUltQueuePush);
    LIB_FUNCTION("6Mc2Xs7pI1I", "libSceUlt", 1, "libSceUlt", sceUltQueueTryPush);
    LIB_FUNCTION("RVSq2tsm2yw", "libSceUlt", 1, "libSceUlt", sceUltQueuePop);
    LIB_FUNCTION("uZz3ci7XYqc", "libSceUlt", 1, "libSceUlt", sceUltQueueTryPop);
    LIB_FUNCTION("V2u3WLrwh64", "libSceUlt", 1, "libSceUlt",
                 _sceUltUlthreadRuntimeOptParamInitialize);
    LIB_FUNCTION("grs2pbc2awM", "libSceUlt", 1, "libSceUlt", sceUltUlthreadRuntimeGetWorkAreaSize);
    LIB_FUNCTION("jw9FkZBXo-g", "libSceUlt", 1, "libSceUlt", _sceUltUlthreadRuntimeCreate);
    LIB_FUNCTION("-gxcs521SvA", "libSceUlt", 1, "libSceUlt", sceUltUlthreadRuntimeDestroy);
    LIB_FUNCTION("vIuen2qlRmo", "libSceUlt", 1, "libSceUlt",
                 _sceUltOptParamInitialize); // _sceUltUlthreadOptParamInitialize
    LIB_FUNCTION("znI3q8S7KQ4", "libSceUlt", 1, "libSceUlt", _sceUltUlthreadCreate);
    LIB_FUNCTION("gCeAI57LGgI", "libSceUlt", 1, "libSceUlt", sceUltUlthreadJoin);
    LIB_FUNCTION("DsW+3FTXL0Q", "libSceUlt", 1, "libSceUlt", sceUltUlthreadTryJoin);
    LIB_FUNCTION("kF--s3xZjco", "libSceUlt", 1, "libSceUlt", sceUltUlthreadExit);
    LIB_FUNCTION("HFd-lpjGxJA", "libSceUlt", 1, "libSceUlt", sceUltUlthreadYield);
    LIB_FUNCTION("xOQ9oACobA4", "libSceUlt", 1, "libSceUlt", sceUltUlthreadGetSelf);
    LIB_FUNCTION("1+8t9aHLiz8", "libSceUlt", 1, "libSceUlt",
                 _sceUltOptParamInitialize); // _sceUltMutexOptParamInitialize
    LIB_FUNCTION("mmt8Sa6tL6c", "libSceUlt", 1, "libSceUlt", _sceUltMutexCreate);
    LIB_FUNCTION("jW+HnafeS3Y", "libSceUlt", 1, "libSceUlt", sceUltMutexDestroy);
    LIB_FUNCTION("8hEGkR1pfr8", "libSceUlt", 1, "libSceUlt", sceUltMutexLock);
    LIB_FUNCTION("jOsUG0BJI-Y", "libSceUlt", 1, "libSceUlt", sceUltMutexTryLock);
    LIB_FUNCTION("h0XebKiMBtk", "libSceUlt", 1, "libSceUlt", sceUltMutexUnlock);
    LIB_FUNCTION("NPRRPNKDBN0", "libSceUlt", 1, "libSceUlt",
                 _sceUltOptParamInitialize); // _sceUltSemaphoreOptParamInitialize
    LIB_FUNCTION("h5QlIYj+Ro8", "libSceUlt", 1, "libSceUlt", _sceUltSemaphoreCreate);
    LIB_FUNCTION("izXyehpoZGo", "libSceUlt", 1, "libSceUlt", sceUltSemaphoreDestroy);
    LIB_FUNCTION("QAH1ofI97vU", "libSceUlt", 1, "libSceUlt", sceUltSemaphoreAcquire);
    LIB_FUNCTION("HA1Ldbi3lPY", "libSceUlt", 1, "libSceUlt", sceUltSemaphoreTryAcquire);
    LIB_FUNCTION("lbtk5X1mecw", "libSceUlt", 1, "libSceUlt", sceUltSemaphoreRelease);
    LIB_FUNCTION("RVmEia0vXMI", "libSceUlt", 1, "libSceUlt",
                 _sceUltOptParamInitialize); // _sceUltConditionVariableOptParamInitialize
    LIB_FUNCTION("jnKaHGkrxZ4", "libSceUlt", 1, "libSceUlt", _sceUltConditionVariableCreate);
    LIB_FUNCTION("xrmmI832R4U", "libSceUlt", 1, "libSceUlt", sceUltConditionVariableDestroy);
    LIB_FUNCTION("5xGAHCxA8M0", "libSceUlt", 1, "libSceUlt", sceUltConditionVariableWait);
    LIB_FUNCTION("JTw1cAVkuc0", "libSceUlt", 1, "libSceUlt", sceUltConditionVariableSignal);
    LIB_FUNCTION("byiceqcMvV0", "libSceUlt", 1, "libSceUlt", sceUltConditionVariableSignalAll);
}

} // namespace Libraries::Ult
