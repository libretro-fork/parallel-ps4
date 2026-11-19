/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

/* A latch: something that has happened or has not, which threads can wait for. One atomic
 * flag and a retro_eventcount: no semaphore, no lock, no timeout. Unlike a semaphore it is
 * not used up by a waiter, so every wait after it is set returns at once, and setting it
 * twice is setting it once. */

#ifndef SHADPS4_EVENT_LATCH_H
#define SHADPS4_EVENT_LATCH_H

#include <retro_atomic.h>
#include <retro_common_api.h>
#include <rthreads/retro_eventcount.h>

RETRO_BEGIN_DECLS

typedef struct event_latch {
    retro_atomic_int_t set;
    retro_eventcount_t waiters;
} event_latch_t;

/* Not set. Before any other call; event_latch_free after the last. */
void event_latch_init(event_latch_t* latch);
void event_latch_free(event_latch_t* latch);

/* From any thread: it has happened. Every thread waiting returns, and every later wait. */
void event_latch_set(event_latch_t* latch);

/* From any thread: not set again, for the next time round. A thread already waiting goes on
 * waiting. */
void event_latch_clear(event_latch_t* latch);

/* Whether it is set. */
int event_latch_is_set(event_latch_t* latch);

/* Returns once it is set: at once if it is already. */
void event_latch_wait(event_latch_t* latch);

RETRO_END_DECLS

#endif
