/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef SHADPS4_LIBRARIES_ULT_H
#define SHADPS4_LIBRARIES_ULT_H

/* libSceUlt: user-level threads and the objects they wait on (queue, mutex,
 * semaphore, condition variable). Used when the system folder has no
 * libSceUlt.sprx; with the module present it is loaded and this is not
 * registered.
 *
 * A user-level thread is a thread of the game's here, not a fiber switched on
 * the runtime's worker threads. What the game can tell of the difference is
 * kept: as many user-level threads of a runtime run at once as it has worker
 * threads. Each holds one of the runtime's run tokens while it runs, gives it
 * up when it waits in one of the library's objects (where the real library
 * switches to another user-level thread) and takes one again to go on. With
 * one worker thread, then, they run one at a time and change only at those
 * waits, as on a PS4.
 *
 * The game's objects hold a tag and a pointer to their state here; the
 * functions find the state through it, with no table and no lock. The names,
 * sizes and error values follow the PS4's library.
 *
 * These are the functions a game calls, with its calling convention; they are
 * registered for the game in sysmodule_internal.cpp, which also gives the
 * library the game's threads (ult_set_host). */

#include <stdint.h>

#include <retro_common_api.h>

#if defined(__GNUC__) && (defined(__x86_64__) || defined(_M_X64))
#define ULT_GUEST_ABI __attribute__((sysv_abi))
#else
#define ULT_GUEST_ABI
#endif

RETRO_BEGIN_DECLS

#define ULT_OK 0
#define ULT_ERROR_NULL ((int32_t)0x80810001)
#define ULT_ERROR_ALIGNMENT ((int32_t)0x80810002)
#define ULT_ERROR_RANGE ((int32_t)0x80810003)
#define ULT_ERROR_INVALID ((int32_t)0x80810004)
#define ULT_ERROR_PERMISSION ((int32_t)0x80810005)
#define ULT_ERROR_STATE ((int32_t)0x80810006)
#define ULT_ERROR_BUSY ((int32_t)0x80810007)
#define ULT_ERROR_AGAIN ((int32_t)0x80810008)

/* What a user-level thread starts in: a function of the game's */
typedef void* ult_entry;

/* A thread of the game's as the library starts it: runs a user-level thread */
void* ULT_GUEST_ABI ult_run_ulthread(void* ulthread_state);

/* The game's threads, which only the emulator's C++ side can make */
typedef struct ult_host {
    /* Starts a thread of the game's in ult_run_ulthread(@arg). 0 when it could not. */
    int (*thread_create)(void** thread, void* arg, const char* name);
    void (*thread_join)(void* thread);
    /* Ends the calling thread; does not return */
    void (*thread_exit)(void);
    void (*yield)(void);
    /* A word each thread has of its own, null until it sets it */
    void* (*self_get)(void);
    void (*self_set)(void* value);
    /* Calls the game's @entry(@arg) */
    int32_t (*call_entry)(ult_entry entry, uint64_t arg);
} ult_host;

/* Before a game calls anything */
void ult_set_host(const ult_host* host);

int32_t ULT_GUEST_ABI sceUltInitialize(void);
int32_t ULT_GUEST_ABI sceUltFinalize(void);
int32_t ULT_GUEST_ABI _sceUltOptParamInitialize(void* opt_param, uint32_t build_version);
int32_t ULT_GUEST_ABI _sceUltUlthreadRuntimeOptParamInitialize(void* opt_param,
                                                               uint32_t build_version);

uint64_t ULT_GUEST_ABI sceUltWaitingQueueResourcePoolGetWorkAreaSize(uint32_t num_threads,
                                                                     uint32_t num_sync_objects);
int32_t ULT_GUEST_ABI _sceUltWaitingQueueResourcePoolCreate(void* pool, const char* name,
                                                            uint32_t num_threads,
                                                            uint32_t num_sync_objects,
                                                            void* work_area, const void* opt_param,
                                                            uint32_t build_version);
int32_t ULT_GUEST_ABI sceUltWaitingQueueResourcePoolDestroy(void* pool);

uint64_t ULT_GUEST_ABI sceUltQueueDataResourcePoolGetWorkAreaSize(uint32_t num_data,
                                                                  uint64_t data_size,
                                                                  uint32_t num_queue_objects);
int32_t ULT_GUEST_ABI _sceUltQueueDataResourcePoolCreate(
    void* pool, const char* name, uint32_t num_data, uint64_t data_size, uint32_t num_queue_objects,
    void* waiting_pool, void* work_area, const void* opt_param, uint32_t build_version);
int32_t ULT_GUEST_ABI sceUltQueueDataResourcePoolDestroy(void* pool);

int32_t ULT_GUEST_ABI _sceUltQueueCreate(void* queue, const char* name, uint64_t data_size,
                                         void* waiting_pool, void* data_pool, const void* opt_param,
                                         uint32_t build_version);
int32_t ULT_GUEST_ABI sceUltQueueDestroy(void* queue);
int32_t ULT_GUEST_ABI sceUltQueuePush(void* queue, const void* data);
int32_t ULT_GUEST_ABI sceUltQueueTryPush(void* queue, const void* data);
int32_t ULT_GUEST_ABI sceUltQueuePop(void* queue, void* data);
int32_t ULT_GUEST_ABI sceUltQueueTryPop(void* queue, void* data);

uint64_t ULT_GUEST_ABI sceUltUlthreadRuntimeGetWorkAreaSize(uint32_t max_ulthreads,
                                                            uint32_t workers);
int32_t ULT_GUEST_ABI _sceUltUlthreadRuntimeCreate(void* runtime, const char* name,
                                                   uint32_t max_ulthreads, uint32_t workers,
                                                   void* work_area, const void* opt_param,
                                                   uint32_t build_version);
int32_t ULT_GUEST_ABI sceUltUlthreadRuntimeDestroy(void* runtime);

int32_t ULT_GUEST_ABI _sceUltUlthreadCreate(void* ulthread, const char* name, ult_entry entry,
                                            uint64_t arg, void* context, uint64_t context_size,
                                            void* runtime, const void* opt_param,
                                            uint32_t build_version);
int32_t ULT_GUEST_ABI sceUltUlthreadJoin(void* ulthread, int32_t* status);
int32_t ULT_GUEST_ABI sceUltUlthreadTryJoin(void* ulthread, int32_t* status);
int32_t ULT_GUEST_ABI sceUltUlthreadExit(int32_t status);
int32_t ULT_GUEST_ABI sceUltUlthreadGetSelf(void** ulthread);
int32_t ULT_GUEST_ABI sceUltUlthreadYield(void);

int32_t ULT_GUEST_ABI _sceUltMutexCreate(void* mutex, const char* name, void* waiting_pool,
                                         const void* opt_param, uint32_t build_version);
int32_t ULT_GUEST_ABI sceUltMutexDestroy(void* mutex);
int32_t ULT_GUEST_ABI sceUltMutexLock(void* mutex);
int32_t ULT_GUEST_ABI sceUltMutexTryLock(void* mutex);
int32_t ULT_GUEST_ABI sceUltMutexUnlock(void* mutex);

int32_t ULT_GUEST_ABI _sceUltSemaphoreCreate(void* semaphore, const char* name, uint32_t resources,
                                             void* waiting_pool, const void* opt_param,
                                             uint32_t build_version);
int32_t ULT_GUEST_ABI sceUltSemaphoreDestroy(void* semaphore);
int32_t ULT_GUEST_ABI sceUltSemaphoreAcquire(void* semaphore, int32_t count);
int32_t ULT_GUEST_ABI sceUltSemaphoreTryAcquire(void* semaphore, int32_t count);
int32_t ULT_GUEST_ABI sceUltSemaphoreRelease(void* semaphore, int32_t count);

int32_t ULT_GUEST_ABI _sceUltConditionVariableCreate(void* condition, const char* name, void* mutex,
                                                     const void* opt_param, uint32_t build_version);
int32_t ULT_GUEST_ABI sceUltConditionVariableDestroy(void* condition);
int32_t ULT_GUEST_ABI sceUltConditionVariableWait(void* condition);
int32_t ULT_GUEST_ABI sceUltConditionVariableSignal(void* condition);
int32_t ULT_GUEST_ABI sceUltConditionVariableSignalAll(void* condition);

RETRO_END_DECLS

#endif
