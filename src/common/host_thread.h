/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef SHADPS4_COMMON_HOST_THREAD_H
#define SHADPS4_COMMON_HOST_THREAD_H

#include <stdint.h>

#include <rthreads/rthreads.h>

#ifdef __cplusplus
extern "C" {
#endif

/* A host thread that can be ended from anywhere in it, as a guest thread's is by
 * pthread_exit: rthreads (sthread_create) runs it, and host_thread_exit jumps back to where it
 * started, without unwinding (the guest's frames have no unwind data, as ExitThread and
 * pthread_exit left them), from where it returns and rthreads ends it as it ends any thread. */
typedef void (*host_thread_func_t)(void*);

typedef struct host_thread {
    sthread_t* thread;
    host_thread_func_t func;
    void* arg;
    uintptr_t id; /* sthread_get_thread_id: the pthread_t, or the Windows thread id */
} host_thread_t;

/* Starts @func(@arg). @thread must stay where it is until the thread has started (its
 * func and arg are read there). @return 0 if it could not be made. */
int host_thread_start(host_thread_t* thread, host_thread_func_t func, void* arg);

/* From the thread itself, anywhere in it: ends it. Does not return. */
void host_thread_exit(void);

/* Whether the calling thread was started by host_thread_start (and is not past its exit) */
int host_thread_is_current(void);

/* What rthreads holds for it, freed once it has ended (now, if it has): the thread itself
 * need not have ended yet. */
void host_thread_release(host_thread_t* thread);

/* Waits for it to end, then frees what rthreads holds (tests; the core releases instead) */
void host_thread_join(host_thread_t* thread);

#ifdef __cplusplus
}
#endif

#endif
