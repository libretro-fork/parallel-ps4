/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

/* A game thread's life, as one atomic word moved by compare-and-swap: whether it has ended,
 * whether it is detached, whether a thread is joining it, and how many hold its record. Its
 * record may be collected once it has ended, is detached and no one holds it: exactly one of
 * the threads whose move makes that so is told to collect it. No lock: libthr's thread lock
 * guarded these. */

#ifndef SHADPS4_THREAD_LIFE_H
#define SHADPS4_THREAD_LIFE_H

#include <retro_atomic.h>
#include <retro_common_api.h>

RETRO_BEGIN_DECLS

enum {
    THREAD_LIFE_DEAD = 1,
    THREAD_LIFE_DETACHED = 2,
    THREAD_LIFE_JOINED = 4,    /* a thread is joining it */
    THREAD_LIFE_COLLECTED = 8, /* handed to be collected: no one may hold it again */
    THREAD_LIFE_REF = 16       /* one hold */
};

enum {
    THREAD_LIFE_OK = 0,
    THREAD_LIFE_GONE,
    THREAD_LIFE_DETACHED_ALREADY,
    THREAD_LIFE_JOINED_ALREADY
};

/* Starts held once, by the thread itself; @detached: made detached. */
void thread_life_init(retro_atomic_int_t* life, int detached);

/* A hold on its record: OK; GONE if it is being collected, or (unless @include_dead) has
 * ended. */
int thread_life_hold(retro_atomic_int_t* life, int include_dead);
/* A hold given back: 1 if the caller must now collect it. */
int thread_life_release(retro_atomic_int_t* life);

/* The thread ends, and gives back its own hold: 1 if it must be collected now. */
int thread_life_end(retro_atomic_int_t* life);

/* Detached: OK, and *@collect whether it must now be collected; DETACHED_ALREADY, or
 * JOINED_ALREADY if a thread is joining it. */
int thread_life_detach(retro_atomic_int_t* life, int* collect);

/* A thread starts joining it: OK, DETACHED_ALREADY, or JOINED_ALREADY. */
int thread_life_join(retro_atomic_int_t* life);
/* The joiner gives up (timed out, cancelled). */
void thread_life_unjoin(retro_atomic_int_t* life);
/* The joiner has its result: detached, no longer joined; 1 if it must be collected now. */
int thread_life_joined(retro_atomic_int_t* life);

int thread_life_is(retro_atomic_int_t* life, int flag);

RETRO_END_DECLS

#endif
