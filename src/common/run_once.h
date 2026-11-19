/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

/* Something done once, whichever thread comes to it first: what std::call_once and
 * std::once_flag were used for, on one atomic word. A thread that comes while another is
 * doing it waits for that to end, on an eventcount all of them share (it is not held by
 * anything, and is touched only when two threads do meet at a first time). Once it is done,
 * asking is one load.
 *
 *   static run_once_t tables = RUN_ONCE_INIT;
 *   run_once(&tables, build_tables, NULL);
 */

#ifndef SHADPS4_RUN_ONCE_H
#define SHADPS4_RUN_ONCE_H

#include <retro_atomic.h>
#include <retro_common_api.h>
#include <retro_inline.h>

RETRO_BEGIN_DECLS

typedef struct run_once {
    retro_atomic_int_t state; /* 0: not done; 1: being done; 2: done */
} run_once_t;

/* A zeroed run_once_t is one not done */
#define RUN_ONCE_INIT {0}

/* Once in the process, before its first run_once: makes the eventcount waiters share. Calling
 * it again does nothing. Not to be raced with itself: at the process's start. */
void run_once_setup(void);

/* The part of run_once for when it is not done yet */
void run_once_slow(run_once_t* once, void (*fn)(void* arg), void* arg);

/* @fn(@arg) has been called, once, when this returns: by this call if no other came first,
 * and otherwise by the one that did, which this waits for if it is still at it. @fn must not
 * come back to the same @once. */
static INLINE void run_once(run_once_t* once, void (*fn)(void* arg), void* arg) {
    if (retro_atomic_load_acquire_int(&once->state) != 2)
        run_once_slow(once, fn, arg);
}

/* For what is only to happen the first time and waited for by nobody (a warning logged
 * once): nonzero for the first caller alone. */
static INLINE int run_once_first(run_once_t* once) {
    return retro_atomic_load_acquire_int(&once->state) == 0 &&
           retro_atomic_exchange_int(&once->state, 2) == 0;
}

RETRO_END_DECLS

#endif
