/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

/* A service thread that can be asked to stop: an rthreads thread and one atomic flag. What
 * std::jthread and its stop_token were used for, without the stop state allocated for every
 * thread and shared by count, the lock taken to hang a wake on it and to take it off again
 * around every wait, and the pointer followed to ask whether to stop. The thread asks with
 * one load of a flag that lies next to what it works on.
 *
 * There is no callback to wake the thread: whoever asks it to stop knows what it waits on, and
 * notifies that after asking.
 *
 *   stop_thread_request(&t);
 *   retro_eventcount_notify(&work);     the thread's own wait
 *   stop_thread_join(&t);
 *
 * and the thread, around its wait:
 *
 *   key = retro_eventcount_prepare_wait(&work);
 *   if (have_work() || stop_thread_stopping(&t)) { cancel; ... }
 *   retro_eventcount_commit_wait(&work, key);
 */

#ifndef SHADPS4_STOP_THREAD_H
#define SHADPS4_STOP_THREAD_H

#include <retro_atomic.h>
#include <retro_common_api.h>
#include <retro_inline.h>
#include <rthreads/rthreads.h>

RETRO_BEGIN_DECLS

typedef struct stop_thread {
    sthread_t* thread;
    retro_atomic_int_t stop;
} stop_thread_t;

/* A zeroed stop_thread_t is one not started: STOP_THREAD_INIT for a static or a member. */
#define STOP_THREAD_INIT {0}

/* Starts @run(@arg) on a thread of its own, not asked to stop. 0 if no thread could be made,
 * or one is already started. */
int stop_thread_start(stop_thread_t* t, void (*run)(void* arg), void* arg);

/* Whether a thread was started and has not been joined. */
int stop_thread_started(const stop_thread_t* t);

/* From any thread: the thread is asked to stop. The caller then wakes whatever it waits on. */
void stop_thread_request(stop_thread_t* t);

/* On the thread, and anywhere: whether it has been asked to stop. */
static INLINE int stop_thread_stopping(stop_thread_t* t) {
    return retro_atomic_load_acquire_int(&t->stop) != 0;
}

/* Waits for the thread to end, if one was started; it can then be started again. Not from the
 * thread itself. It does not ask the thread to stop: stop_thread_request first, and the wake. */
void stop_thread_join(stop_thread_t* t);

RETRO_END_DECLS

#endif
