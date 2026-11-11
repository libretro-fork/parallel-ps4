/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <stdlib.h>
#include <string.h>

#include <retro_atomic.h>
#include <rthreads/retro_eventcount.h>

#include "common/c_bridge.h"
#include "core/libraries/ult/ult.h"
#include "core/libraries/ult/ult_sync.h"

/* Every option parameter block: 128 bytes, a header, the SDK version, then its fields */
#define OPT_PARAM_SIZE 128
static const uint32_t opt_param_header[3] = {0x6C696275, 0x6C746D61, 0x67696300};

enum {
    KIND_WAITING_POOL = 1,
    KIND_DATA_POOL,
    KIND_QUEUE,
    KIND_RUNTIME,
    KIND_ULTHREAD,
    KIND_MUTEX,
    KIND_SEMAPHORE,
    KIND_CONDITION
};

/* The start of each of the game's objects, once created */
typedef struct handle {
    uint32_t magic;
    uint32_t kind;
    void* state;
} handle_t;
#define HANDLE_MAGIC 0x544C5553u /* "SULT" */

typedef struct waiting_pool_state {
    uint32_t num_threads;
    uint32_t num_sync_objects;
} waiting_pool_state;

typedef struct data_pool_state {
    ult_data_pool pool;
    uint32_t max_queues;
    retro_atomic_int_t queues;
} data_pool_state;

typedef struct queue_state {
    ult_queue queue;
    data_pool_state* data_pool;
} queue_state;

typedef struct runtime_state {
    ult_tokens tokens;
    uint32_t max_ulthreads;
    uint32_t workers;
    retro_atomic_int_t ulthreads;
} runtime_state;

typedef struct ulthread_state {
    void* object; /* the game's */
    runtime_state* runtime;
    ult_entry entry;
    uint64_t arg;
    void* thread;
    int32_t status; /* written before exited is set */
    retro_atomic_int_t exited;
    retro_atomic_int_t joined;
    retro_eventcount_t done;
} ulthread_state;

static ult_host host;

/* The user-level thread the calling thread is, if it is one */
static ulthread_state* self(void) {
    return (ulthread_state*)host.self_get();
}

static void give_token(void) {
    ulthread_state* s = self();
    if (s)
        ult_tokens_give(&s->runtime->tokens);
}

static void take_token(void) {
    ulthread_state* s = self();
    if (s)
        ult_tokens_take(&s->runtime->tokens);
}

void ult_set_host(const ult_host* h) {
    host = *h;
    ult_sync_set_park_hooks(give_token, take_token);
}

static int32_t check_object(const void* object) {
    if (!object)
        return ULT_ERROR_NULL;
    if ((uintptr_t)object & 7u)
        return ULT_ERROR_ALIGNMENT;
    return ULT_OK;
}

static void bind(void* object, uint32_t kind, void* state) {
    handle_t h;
    h.magic = HANDLE_MAGIC;
    h.kind = kind;
    h.state = state;
    memcpy(object, &h, sizeof(h));
}

static void unbind(void* object) {
    memset(object, 0, sizeof(handle_t));
}

/* The state of the game's object, or null with *error set */
static void* find(const void* object, uint32_t kind, int32_t* error) {
    handle_t h;
    *error = check_object(object);
    if (*error != ULT_OK)
        return NULL;
    memcpy(&h, object, sizeof(h));
    if (h.magic != HANDLE_MAGIC || h.kind != kind || !h.state) {
        *error = ULT_ERROR_STATE;
        return NULL;
    }
    return h.state;
}

/* An optional block of option parameters: null, or one with the header */
static int32_t check_opt_param(const void* opt_param) {
    if (!opt_param)
        return ULT_OK;
    if ((uintptr_t)opt_param & 7u)
        return ULT_ERROR_ALIGNMENT;
    return memcmp(opt_param, opt_param_header, sizeof(opt_param_header)) == 0 ? ULT_OK
                                                                              : ULT_ERROR_INVALID;
}

/* The end of a user-level thread, by returning or by sceUltUlthreadExit */
static void finish_ulthread(ulthread_state* s, int32_t status) {
    s->status = status;
    host.self_set(NULL);
    ult_tokens_give(&s->runtime->tokens);
    retro_atomic_store_release_int(&s->exited, 1);
    retro_eventcount_notify(&s->done);
}

void* ULT_GUEST_ABI ult_run_ulthread(void* ulthread) {
    ulthread_state* s = (ulthread_state*)ulthread;
    int32_t status;
    host.self_set(s);
    ult_tokens_take(&s->runtime->tokens);
    status = host.call_entry(s->entry, s->arg);
    /* Not if it left through sceUltUlthreadExit, which has done this */
    if (self() == s)
        finish_ulthread(s, status);
    return NULL;
}

int32_t ULT_GUEST_ABI sceUltInitialize(void) {
    c_log(C_LOG_LIB_ULT, C_LOG_LEVEL_INFO, "sceUltInitialize", "called");
    return ULT_OK;
}

int32_t ULT_GUEST_ABI sceUltFinalize(void) {
    c_log(C_LOG_LIB_ULT, C_LOG_LEVEL_INFO, "sceUltFinalize", "called");
    return ULT_OK;
}

/* ---- option parameters ---- */

int32_t ULT_GUEST_ABI _sceUltOptParamInitialize(void* opt_param, uint32_t build_version) {
    if (!opt_param)
        return ULT_ERROR_NULL;
    memset(opt_param, 0, OPT_PARAM_SIZE);
    memcpy(opt_param, opt_param_header, sizeof(opt_param_header));
    memcpy((uint8_t*)opt_param + sizeof(opt_param_header), &build_version, sizeof(build_version));
    return ULT_OK;
}

int32_t ULT_GUEST_ABI _sceUltUlthreadRuntimeOptParamInitialize(void* opt_param,
                                                               uint32_t build_version) {
    /* The console's defaults, after the header and version */
    static const uint32_t defaults[6] = {0x4000, 0, 0x1FFF, 0, 0x2BC, 4};
    const int32_t result = _sceUltOptParamInitialize(opt_param, build_version);
    if (result == ULT_OK)
        memcpy((uint8_t*)opt_param + 16, defaults, sizeof(defaults));
    return result;
}

/* ---- waiting queue resource pool ---- */

uint64_t ULT_GUEST_ABI sceUltWaitingQueueResourcePoolGetWorkAreaSize(uint32_t num_threads,
                                                                     uint32_t num_sync_objects) {
    return 32u * ((uint64_t)num_sync_objects + 2u * (uint64_t)num_threads + 2u);
}

int32_t ULT_GUEST_ABI _sceUltWaitingQueueResourcePoolCreate(void* pool, const char* name,
                                                            uint32_t num_threads,
                                                            uint32_t num_sync_objects,
                                                            void* work_area, const void* opt_param,
                                                            uint32_t build_version) {
    waiting_pool_state* state;
    int32_t error;
    (void)build_version;
    c_log(C_LOG_LIB_ULT, C_LOG_LEVEL_INFO, "_sceUltWaitingQueueResourcePoolCreate",
          "'%s': %u threads, %u objects", name ? name : "", (unsigned)num_threads,
          (unsigned)num_sync_objects);
    error = check_object(pool);
    if (error != ULT_OK)
        return error;
    if (!work_area)
        return ULT_ERROR_NULL;
    error = check_opt_param(opt_param);
    if (error != ULT_OK)
        return error;
    /* Nothing waits in it here: the objects park their own waiters */
    state = (waiting_pool_state*)malloc(sizeof(*state));
    if (!state)
        return ULT_ERROR_AGAIN;
    state->num_threads = num_threads;
    state->num_sync_objects = num_sync_objects;
    bind(pool, KIND_WAITING_POOL, state);
    return ULT_OK;
}

int32_t ULT_GUEST_ABI sceUltWaitingQueueResourcePoolDestroy(void* pool) {
    int32_t error;
    void* state = find(pool, KIND_WAITING_POOL, &error);
    if (!state)
        return error;
    unbind(pool);
    free(state);
    return ULT_OK;
}

/* ---- queue data resource pool ---- */

uint64_t ULT_GUEST_ABI sceUltQueueDataResourcePoolGetWorkAreaSize(uint32_t num_data,
                                                                  uint64_t data_size,
                                                                  uint32_t num_queue_objects) {
    return (uint64_t)num_data * data_size +
           32u * ((uint64_t)num_queue_objects + 2u * (uint64_t)num_data + 3u);
}

int32_t ULT_GUEST_ABI _sceUltQueueDataResourcePoolCreate(
    void* pool, const char* name, uint32_t num_data, uint64_t data_size, uint32_t num_queue_objects,
    void* waiting_pool, void* work_area, const void* opt_param, uint32_t build_version) {
    data_pool_state* state;
    int32_t error;
    (void)build_version;
    c_log(C_LOG_LIB_ULT, C_LOG_LEVEL_INFO, "_sceUltQueueDataResourcePoolCreate",
          "'%s': %u items of %lu bytes, %u queues", name ? name : "", (unsigned)num_data,
          (unsigned long)data_size, (unsigned)num_queue_objects);
    error = check_object(pool);
    if (error != ULT_OK)
        return error;
    if (!work_area)
        return ULT_ERROR_NULL;
    if (num_data == 0 || data_size == 0 || num_queue_objects == 0)
        return ULT_ERROR_RANGE;
    error = check_opt_param(opt_param);
    if (error != ULT_OK)
        return error;
    if (waiting_pool && !find(waiting_pool, KIND_WAITING_POOL, &error))
        return ULT_ERROR_INVALID;
    /* The items live in the game's work area, which is larger than they need
     * (sceUltQueueDataResourcePoolGetWorkAreaSize: 96 bytes and more an item over its
     * size, against the 4 of a link); only the pool's few words are here */
    state = (data_pool_state*)malloc(sizeof(*state));
    if (!state)
        return ULT_ERROR_AGAIN;
    ult_data_pool_init(&state->pool, work_area, num_data, data_size);
    state->max_queues = num_queue_objects;
    retro_atomic_int_init(&state->queues, 0);
    bind(pool, KIND_DATA_POOL, state);
    return ULT_OK;
}

int32_t ULT_GUEST_ABI sceUltQueueDataResourcePoolDestroy(void* pool) {
    int32_t error;
    data_pool_state* state = (data_pool_state*)find(pool, KIND_DATA_POOL, &error);
    if (!state)
        return error;
    if (retro_atomic_load_acquire_int(&state->queues) != 0)
        return ULT_ERROR_STATE;
    unbind(pool);
    ult_data_pool_free(&state->pool);
    free(state);
    return ULT_OK;
}

/* ---- queue ---- */

int32_t ULT_GUEST_ABI _sceUltQueueCreate(void* queue, const char* name, uint64_t data_size,
                                         void* waiting_pool, void* data_pool, const void* opt_param,
                                         uint32_t build_version) {
    data_pool_state* pool;
    queue_state* state;
    int32_t error;
    (void)waiting_pool;
    (void)build_version;
    c_log(C_LOG_LIB_ULT, C_LOG_LEVEL_INFO, "_sceUltQueueCreate", "'%s': items of %lu bytes",
          name ? name : "", (unsigned long)data_size);
    error = check_object(queue);
    if (error != ULT_OK)
        return error;
    error = check_opt_param(opt_param);
    if (error != ULT_OK)
        return error;
    pool = (data_pool_state*)find(data_pool, KIND_DATA_POOL, &error);
    if (!pool)
        return data_pool ? ULT_ERROR_INVALID : ULT_ERROR_NULL;
    if (data_size != pool->pool.item_size)
        return ULT_ERROR_INVALID;
    state = (queue_state*)malloc(sizeof(*state));
    if (!state)
        return ULT_ERROR_AGAIN;
    if ((uint32_t)retro_atomic_fetch_add_int(&pool->queues, 1) >= pool->max_queues) {
        retro_atomic_fetch_sub_int(&pool->queues, 1);
        free(state);
        return ULT_ERROR_AGAIN;
    }
    ult_queue_init(&state->queue, &pool->pool);
    state->data_pool = pool;
    bind(queue, KIND_QUEUE, state);
    return ULT_OK;
}

int32_t ULT_GUEST_ABI sceUltQueueDestroy(void* queue) {
    int32_t error;
    queue_state* state = (queue_state*)find(queue, KIND_QUEUE, &error);
    if (!state)
        return error;
    unbind(queue);
    /* What it still holds goes back to the pool */
    while (ult_queue_try_pop(&state->queue, NULL)) {
    }
    retro_atomic_fetch_sub_int(&state->data_pool->queues, 1);
    ult_queue_free(&state->queue);
    free(state);
    return ULT_OK;
}

int32_t ULT_GUEST_ABI sceUltQueuePush(void* queue, const void* data) {
    int32_t error;
    queue_state* state = (queue_state*)find(queue, KIND_QUEUE, &error);
    if (!state)
        return error;
    if (!data)
        return ULT_ERROR_NULL;
    ult_queue_push(&state->queue, data);
    return ULT_OK;
}

int32_t ULT_GUEST_ABI sceUltQueueTryPush(void* queue, const void* data) {
    int32_t error;
    queue_state* state = (queue_state*)find(queue, KIND_QUEUE, &error);
    if (!state)
        return error;
    if (!data)
        return ULT_ERROR_NULL;
    return ult_queue_try_push(&state->queue, data) ? ULT_OK : ULT_ERROR_AGAIN;
}

int32_t ULT_GUEST_ABI sceUltQueuePop(void* queue, void* data) {
    int32_t error;
    queue_state* state = (queue_state*)find(queue, KIND_QUEUE, &error);
    if (!state)
        return error;
    if (!data)
        return ULT_ERROR_NULL;
    ult_queue_pop(&state->queue, data);
    return ULT_OK;
}

int32_t ULT_GUEST_ABI sceUltQueueTryPop(void* queue, void* data) {
    int32_t error;
    queue_state* state = (queue_state*)find(queue, KIND_QUEUE, &error);
    if (!state)
        return error;
    if (!data)
        return ULT_ERROR_NULL;
    return ult_queue_try_pop(&state->queue, data) ? ULT_OK : ULT_ERROR_AGAIN;
}

/* ---- user-level thread runtime ---- */

uint64_t ULT_GUEST_ABI sceUltUlthreadRuntimeGetWorkAreaSize(uint32_t max_ulthreads,
                                                            uint32_t workers) {
    const uint64_t n = max_ulthreads, w = workers;
    const uint64_t threads = n + w;
    const uint64_t thread_area = (32u * (2u * n + 4u) + 4u * n + 7u) & ~(uint64_t)7u;
    const uint64_t scheduler_area = 32u * (2u * w + 4u) + 32u * (2u * threads + 4u);
    return thread_area + scheduler_area + 512u * w + 8u * (n + threads);
}

int32_t ULT_GUEST_ABI _sceUltUlthreadRuntimeCreate(void* runtime, const char* name,
                                                   uint32_t max_ulthreads, uint32_t workers,
                                                   void* work_area, const void* opt_param,
                                                   uint32_t build_version) {
    runtime_state* state;
    int32_t error;
    (void)build_version;
    c_log(C_LOG_LIB_ULT, C_LOG_LEVEL_INFO, "_sceUltUlthreadRuntimeCreate",
          "'%s': %u user-level threads on %u workers", name ? name : "", (unsigned)max_ulthreads,
          (unsigned)workers);
    error = check_object(runtime);
    if (error != ULT_OK)
        return error;
    if (!work_area)
        return ULT_ERROR_NULL;
    if (max_ulthreads == 0 || workers == 0 || workers > 0x7FFFFFFFu)
        return ULT_ERROR_RANGE;
    error = check_opt_param(opt_param);
    if (error != ULT_OK)
        return error;
    state = (runtime_state*)malloc(sizeof(*state));
    if (!state)
        return ULT_ERROR_AGAIN;
    ult_tokens_init(&state->tokens, (int)workers);
    state->max_ulthreads = max_ulthreads;
    state->workers = workers;
    retro_atomic_int_init(&state->ulthreads, 0);
    bind(runtime, KIND_RUNTIME, state);
    return ULT_OK;
}

int32_t ULT_GUEST_ABI sceUltUlthreadRuntimeDestroy(void* runtime) {
    int32_t error;
    runtime_state* state = (runtime_state*)find(runtime, KIND_RUNTIME, &error);
    if (!state)
        return error;
    if (retro_atomic_load_acquire_int(&state->ulthreads) != 0)
        return ULT_ERROR_STATE;
    unbind(runtime);
    ult_tokens_free(&state->tokens);
    free(state);
    return ULT_OK;
}

/* ---- user-level threads ---- */

int32_t ULT_GUEST_ABI _sceUltUlthreadCreate(void* ulthread, const char* name, ult_entry entry,
                                            uint64_t arg, void* context, uint64_t context_size,
                                            void* runtime, const void* opt_param,
                                            uint32_t build_version) {
    runtime_state* owner;
    ulthread_state* state;
    int32_t error;
    (void)context;
    (void)context_size;
    (void)build_version;
    error = check_object(ulthread);
    if (error != ULT_OK)
        return error;
    if (!entry)
        return ULT_ERROR_NULL;
    error = check_opt_param(opt_param);
    if (error != ULT_OK)
        return error;
    owner = (runtime_state*)find(runtime, KIND_RUNTIME, &error);
    if (!owner)
        return runtime ? ULT_ERROR_INVALID : ULT_ERROR_NULL;
    state = (ulthread_state*)calloc(1, sizeof(*state));
    if (!state)
        return ULT_ERROR_AGAIN;
    if ((uint32_t)retro_atomic_fetch_add_int(&owner->ulthreads, 1) >= owner->max_ulthreads) {
        retro_atomic_fetch_sub_int(&owner->ulthreads, 1);
        free(state);
        return ULT_ERROR_AGAIN;
    }
    state->object = ulthread;
    state->runtime = owner;
    state->entry = entry;
    state->arg = arg;
    retro_atomic_int_init(&state->exited, 0);
    retro_atomic_int_init(&state->joined, 0);
    retro_eventcount_init(&state->done);
    bind(ulthread, KIND_ULTHREAD, state);
    /* It runs on a stack of its own, as the game's threads do: the context the game
     * gives is sized for its code alone, not for the emulator's under it */
    if (!host.thread_create(&state->thread, state, name ? name : "Ulthread")) {
        c_log(C_LOG_LIB_ULT, C_LOG_LEVEL_ERROR, "_sceUltUlthreadCreate",
              "user-level thread '%s' not created", name ? name : "");
        unbind(ulthread);
        retro_atomic_fetch_sub_int(&owner->ulthreads, 1);
        retro_eventcount_free(&state->done);
        free(state);
        return ULT_ERROR_AGAIN;
    }
    return ULT_OK;
}

static int exited_attempt(void* ctx) {
    return retro_atomic_load_acquire_int(&((ulthread_state*)ctx)->exited) != 0;
}

static int32_t join(void* ulthread, int32_t* status, int wait) {
    int32_t error;
    ulthread_state* state = (ulthread_state*)find(ulthread, KIND_ULTHREAD, &error);
    if (!state)
        return error;
    if (state == self())
        return ULT_ERROR_PERMISSION;
    if (!wait && !exited_attempt(state))
        return ULT_ERROR_BUSY;
    /* One joiner has it */
    if (!retro_atomic_cas_int(&state->joined, 0, 1))
        return ULT_ERROR_STATE;
    /* A user-level thread that waits for another lets a third run meanwhile */
    if (!exited_attempt(state)) {
        give_token();
        while (!exited_attempt(state)) {
            const int key = retro_eventcount_prepare_wait(&state->done);
            if (exited_attempt(state)) {
                retro_eventcount_cancel_wait(&state->done);
                break;
            }
            retro_eventcount_commit_wait(&state->done, key);
        }
        take_token();
    }
    host.thread_join(state->thread);
    if (status)
        *status = state->status;
    unbind(ulthread);
    retro_atomic_fetch_sub_int(&state->runtime->ulthreads, 1);
    retro_eventcount_free(&state->done);
    free(state);
    return ULT_OK;
}

int32_t ULT_GUEST_ABI sceUltUlthreadJoin(void* ulthread, int32_t* status) {
    return join(ulthread, status, 1);
}

int32_t ULT_GUEST_ABI sceUltUlthreadTryJoin(void* ulthread, int32_t* status) {
    return join(ulthread, status, 0);
}

int32_t ULT_GUEST_ABI sceUltUlthreadExit(int32_t status) {
    ulthread_state* s = self();
    if (!s)
        return ULT_ERROR_PERMISSION;
    finish_ulthread(s, status);
    host.thread_exit();
    return ULT_OK;
}

int32_t ULT_GUEST_ABI sceUltUlthreadGetSelf(void** ulthread) {
    ulthread_state* s = self();
    if (!ulthread)
        return ULT_ERROR_NULL;
    if (!s)
        return ULT_ERROR_PERMISSION;
    *ulthread = s->object;
    return ULT_OK;
}

int32_t ULT_GUEST_ABI sceUltUlthreadYield(void) {
    if (!self())
        return ULT_ERROR_PERMISSION;
    /* To another user-level thread waiting for a worker, if there is one */
    give_token();
    host.yield();
    take_token();
    return ULT_OK;
}

/* ---- mutex ---- */

int32_t ULT_GUEST_ABI _sceUltMutexCreate(void* mutex, const char* name, void* waiting_pool,
                                         const void* opt_param, uint32_t build_version) {
    ult_mutex* state;
    uint32_t attribute = 0; /* after the header and version: not 0 for a recursive mutex */
    int32_t error;
    (void)name;
    (void)waiting_pool;
    (void)build_version;
    error = check_object(mutex);
    if (error != ULT_OK)
        return error;
    error = check_opt_param(opt_param);
    if (error != ULT_OK)
        return error;
    if (opt_param)
        memcpy(&attribute, (const uint8_t*)opt_param + 16, sizeof(attribute));
    state = (ult_mutex*)malloc(sizeof(*state));
    if (!state)
        return ULT_ERROR_AGAIN;
    ult_mutex_init(state, attribute != 0);
    bind(mutex, KIND_MUTEX, state);
    return ULT_OK;
}

int32_t ULT_GUEST_ABI sceUltMutexDestroy(void* mutex) {
    int32_t error;
    ult_mutex* state = (ult_mutex*)find(mutex, KIND_MUTEX, &error);
    if (!state)
        return error;
    unbind(mutex);
    ult_mutex_free(state);
    free(state);
    return ULT_OK;
}

int32_t ULT_GUEST_ABI sceUltMutexLock(void* mutex) {
    int32_t error;
    ult_mutex* state = (ult_mutex*)find(mutex, KIND_MUTEX, &error);
    if (!state)
        return error;
    return ult_mutex_lock(state) == ULT_MUTEX_LOCKED ? ULT_OK : ULT_ERROR_STATE;
}

int32_t ULT_GUEST_ABI sceUltMutexTryLock(void* mutex) {
    int32_t error;
    ult_mutex* state = (ult_mutex*)find(mutex, KIND_MUTEX, &error);
    if (!state)
        return error;
    switch (ult_mutex_try_lock(state)) {
    case ULT_MUTEX_LOCKED:
        return ULT_OK;
    case ULT_MUTEX_BUSY:
        return ULT_ERROR_BUSY;
    default:
        return ULT_ERROR_STATE;
    }
}

int32_t ULT_GUEST_ABI sceUltMutexUnlock(void* mutex) {
    int32_t error;
    ult_mutex* state = (ult_mutex*)find(mutex, KIND_MUTEX, &error);
    if (!state)
        return error;
    return ult_mutex_unlock(state) ? ULT_OK : ULT_ERROR_PERMISSION;
}

/* ---- semaphore ---- */

int32_t ULT_GUEST_ABI _sceUltSemaphoreCreate(void* semaphore, const char* name, uint32_t resources,
                                             void* waiting_pool, const void* opt_param,
                                             uint32_t build_version) {
    ult_semaphore* state;
    int32_t error;
    (void)name;
    (void)waiting_pool;
    (void)build_version;
    error = check_object(semaphore);
    if (error != ULT_OK)
        return error;
    error = check_opt_param(opt_param);
    if (error != ULT_OK)
        return error;
    if (resources > 0x7FFFFFFFu)
        return ULT_ERROR_RANGE;
    state = (ult_semaphore*)malloc(sizeof(*state));
    if (!state)
        return ULT_ERROR_AGAIN;
    ult_semaphore_init(state, (int)resources);
    bind(semaphore, KIND_SEMAPHORE, state);
    return ULT_OK;
}

int32_t ULT_GUEST_ABI sceUltSemaphoreDestroy(void* semaphore) {
    int32_t error;
    ult_semaphore* state = (ult_semaphore*)find(semaphore, KIND_SEMAPHORE, &error);
    if (!state)
        return error;
    unbind(semaphore);
    ult_semaphore_free(state);
    free(state);
    return ULT_OK;
}

int32_t ULT_GUEST_ABI sceUltSemaphoreAcquire(void* semaphore, int32_t count) {
    int32_t error;
    ult_semaphore* state = (ult_semaphore*)find(semaphore, KIND_SEMAPHORE, &error);
    if (!state)
        return error;
    if (count <= 0)
        return ULT_ERROR_RANGE;
    ult_semaphore_acquire(state, count);
    return ULT_OK;
}

int32_t ULT_GUEST_ABI sceUltSemaphoreTryAcquire(void* semaphore, int32_t count) {
    int32_t error;
    ult_semaphore* state = (ult_semaphore*)find(semaphore, KIND_SEMAPHORE, &error);
    if (!state)
        return error;
    if (count <= 0)
        return ULT_ERROR_RANGE;
    return ult_semaphore_try_acquire(state, count) ? ULT_OK : ULT_ERROR_AGAIN;
}

int32_t ULT_GUEST_ABI sceUltSemaphoreRelease(void* semaphore, int32_t count) {
    int32_t error;
    ult_semaphore* state = (ult_semaphore*)find(semaphore, KIND_SEMAPHORE, &error);
    if (!state)
        return error;
    if (count <= 0)
        return ULT_ERROR_RANGE;
    ult_semaphore_release(state, count);
    return ULT_OK;
}

/* ---- condition variable ---- */

int32_t ULT_GUEST_ABI _sceUltConditionVariableCreate(void* condition, const char* name, void* mutex,
                                                     const void* opt_param,
                                                     uint32_t build_version) {
    ult_mutex* owner;
    ult_condition* state;
    int32_t error;
    (void)name;
    (void)build_version;
    error = check_object(condition);
    if (error != ULT_OK)
        return error;
    error = check_opt_param(opt_param);
    if (error != ULT_OK)
        return error;
    owner = (ult_mutex*)find(mutex, KIND_MUTEX, &error);
    if (!owner)
        return mutex ? ULT_ERROR_INVALID : ULT_ERROR_NULL;
    state = (ult_condition*)malloc(sizeof(*state));
    if (!state)
        return ULT_ERROR_AGAIN;
    ult_condition_init(state, owner);
    bind(condition, KIND_CONDITION, state);
    return ULT_OK;
}

int32_t ULT_GUEST_ABI sceUltConditionVariableDestroy(void* condition) {
    int32_t error;
    ult_condition* state = (ult_condition*)find(condition, KIND_CONDITION, &error);
    if (!state)
        return error;
    unbind(condition);
    ult_condition_free(state);
    free(state);
    return ULT_OK;
}

int32_t ULT_GUEST_ABI sceUltConditionVariableWait(void* condition) {
    int32_t error;
    ult_condition* state = (ult_condition*)find(condition, KIND_CONDITION, &error);
    if (!state)
        return error;
    return ult_condition_wait(state) ? ULT_OK : ULT_ERROR_PERMISSION;
}

int32_t ULT_GUEST_ABI sceUltConditionVariableSignal(void* condition) {
    int32_t error;
    ult_condition* state = (ult_condition*)find(condition, KIND_CONDITION, &error);
    if (!state)
        return error;
    ult_condition_signal(state);
    return ULT_OK;
}

int32_t ULT_GUEST_ABI sceUltConditionVariableSignalAll(void* condition) {
    int32_t error;
    ult_condition* state = (ult_condition*)find(condition, KIND_CONDITION, &error);
    if (!state)
        return error;
    ult_condition_signal_all(state);
    return ULT_OK;
}
