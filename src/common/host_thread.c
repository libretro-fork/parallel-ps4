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
#elif defined(_WIN32) && defined(__clang__)
/* clang for MinGW: its __builtin_setjmp keeps the frame pointer the Win64 frame layout moved
 * (rbp sits 0x80 above rsp), so after the jump the function reads its locals from the wrong
 * place and crashes as a thread ends (writing address 0x38). The CRT's own, given no frame,
 * restores every register as it was, without unwinding through the guest's frames. */
#include <setjmp.h>
#define HOST_THREAD_LOCAL __thread
typedef jmp_buf host_thread_jump_t;
#define HOST_THREAD_SETJMP(buf) _setjmp(buf, NULL)
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
static HOST_THREAD_LOCAL host_thread_func_t host_thread_exit_func;
static HOST_THREAD_LOCAL void* host_thread_exit_arg;

static void host_thread_entry(void* data) {
    const host_thread_t* thread = (const host_thread_t*)data;
    const host_thread_func_t func = thread->func;
    void* const arg = thread->arg;
    host_thread_func_t at_exit;
    host_thread_started = 1;
    host_thread_exit_func = NULL;
    if (HOST_THREAD_SETJMP(host_thread_jump) == 0)
        func(arg);
    host_thread_started = 0;
    /* Back on this thread's own stack, every frame of the thread's gone */
    at_exit = host_thread_exit_func;
    host_thread_exit_func = NULL;
    if (at_exit)
        at_exit(host_thread_exit_arg);
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

void host_thread_at_exit(host_thread_func_t func, void* arg) {
    host_thread_exit_arg = arg;
    host_thread_exit_func = func;
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
