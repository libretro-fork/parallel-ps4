/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <stddef.h>

#include "common/host_thread.h"

#if defined(_MSC_VER) && !defined(__clang__)
#include <setjmp.h>
#define HOST_THREAD_LOCAL __declspec(thread)
typedef jmp_buf host_thread_jump_t;
#define HOST_THREAD_SETJMP(buf) setjmp(buf)
#define HOST_THREAD_LONGJMP(buf) longjmp(buf, 1)
#else
#define HOST_THREAD_LOCAL __thread
/* Frame, stack and resume addresses: what __builtin_setjmp keeps */
typedef void* host_thread_jump_t[5];
#define HOST_THREAD_SETJMP(buf) __builtin_setjmp(buf)
#define HOST_THREAD_LONGJMP(buf) __builtin_longjmp(buf, 1)
#endif

/* The thread's own: the host_thread_t may be freed by another thread once this one is
 * marked ended, before it is gone */
static HOST_THREAD_LOCAL host_thread_jump_t host_thread_jump;
static HOST_THREAD_LOCAL int host_thread_started;

static void host_thread_entry(void* data) {
    const host_thread_t* thread = (const host_thread_t*)data;
    const host_thread_func_t func = thread->func;
    void* const arg = thread->arg;
    host_thread_started = 1;
    if (HOST_THREAD_SETJMP(host_thread_jump) == 0)
        func(arg);
    host_thread_started = 0;
}

int host_thread_start(host_thread_t* thread, host_thread_func_t func, void* arg) {
    thread->func = func;
    thread->arg = arg;
    thread->id = 0;
    thread->thread = sthread_create(host_thread_entry, thread);
    if (!thread->thread)
        return 0;
    thread->id = sthread_get_thread_id(thread->thread);
    return 1;
}

void host_thread_exit(void) {
    HOST_THREAD_LONGJMP(host_thread_jump);
}

int host_thread_is_current(void) {
    return host_thread_started;
}

void host_thread_release(host_thread_t* thread) {
    if (thread->thread)
        sthread_detach(thread->thread);
    thread->thread = NULL;
}

void host_thread_join(host_thread_t* thread) {
    if (thread->thread)
        sthread_join(thread->thread);
    thread->thread = NULL;
}
