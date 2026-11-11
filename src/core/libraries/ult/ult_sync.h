/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef SHADPS4_LIBRARIES_ULT_SYNC_H
#define SHADPS4_LIBRARIES_ULT_SYNC_H

/* What libSceUlt's objects do, without the game's side of them (ult.c): run
 * tokens, a mutex, a semaphore, a condition variable, and the queue with its
 * pool of items. No lock: retro_atomic words, a combiner where a list is
 * changed, and a retro_eventcount to park on. Tested on its own
 * (libretro/tests/ult_sync_test.c). */

#include <stddef.h>
#include <stdint.h>

#include <queues/mpsc_stack.h>
#include <retro_atomic.h>
#include <retro_common_api.h>
#include <rthreads/retro_eventcount.h>

RETRO_BEGIN_DECLS

/* Called around a park, by the thread that parks. A user-level thread gives
 * up its runtime's run token while it waits in one of these objects and takes
 * one again after, which is when the real library switches to another
 * user-level thread. */
void ult_sync_set_park_hooks(void (*before)(void), void (*after)(void));

/* As many threads run at once as a runtime has worker threads: each holds a
 * token while it runs. */
typedef struct ult_tokens {
    retro_atomic_int_t tokens;
    retro_eventcount_t ec;
} ult_tokens;

void ult_tokens_init(ult_tokens* t, int count);
void ult_tokens_free(ult_tokens* t);
int ult_tokens_try_take(ult_tokens* t);
void ult_tokens_take(ult_tokens* t);
void ult_tokens_give(ult_tokens* t);

typedef struct ult_semaphore {
    retro_atomic_int_t resources;
    retro_eventcount_t ec;
} ult_semaphore;

void ult_semaphore_init(ult_semaphore* s, int resources);
void ult_semaphore_free(ult_semaphore* s);
int ult_semaphore_try_acquire(ult_semaphore* s, int count);
void ult_semaphore_acquire(ult_semaphore* s, int count);
void ult_semaphore_release(ult_semaphore* s, int count);

typedef struct ult_mutex {
    retro_atomic_ptr_t owner; /* the thread's number, or none */
    uint32_t count;           /* the owner's */
    int recursive;
    retro_eventcount_t ec;
} ult_mutex;

#define ULT_MUTEX_LOCKED 0
#define ULT_MUTEX_BUSY 1
#define ULT_MUTEX_RELOCKED 2 /* the caller holds it already and it is not recursive */

void ult_mutex_init(ult_mutex* m, int recursive);
void ult_mutex_free(ult_mutex* m);
int ult_mutex_try_lock(ult_mutex* m);
int ult_mutex_lock(ult_mutex* m);
int ult_mutex_held_by_caller(ult_mutex* m);
/* 0 if the caller does not hold it */
int ult_mutex_unlock(ult_mutex* m);

/* A condition variable of one mutex. Each waiter takes a number as it starts
 * to wait, under the mutex, and a signal lets the waiters through in that
 * order, up to a number: so a signal reaches a thread that was waiting when it
 * was sent, never one that came to wait after (the signaller itself, waiting
 * next, would otherwise take its own signal and leave the thread it meant
 * asleep). */
typedef struct ult_condition {
    ult_mutex* mutex;
    retro_atomic_int_t arrived;  /* waiters there have been */
    retro_atomic_int_t released; /* how many of them signals have let through */
    retro_eventcount_t ec;
} ult_condition;

void ult_condition_init(ult_condition* c, ult_mutex* mutex);
void ult_condition_free(ult_condition* c);
/* 0 if the caller does not hold the mutex */
int ult_condition_wait(ult_condition* c);
void ult_condition_signal(ult_condition* c);
void ult_condition_signal_all(ult_condition* c);

/* Runs functions one at a time, in the order they were handed over, without a
 * lock: a caller pushes its request (on its own stack) and tries to become the
 * one that runs every request queued; the others park until theirs has run. */
typedef struct ult_combiner {
    mpsc_stack_t pending;
    retro_atomic_int_t busy;
    retro_eventcount_t done_ec;
} ult_combiner;

#define ULT_NONE 0xFFFFFFFFu

/* The items the queues of a pool hold, in the work area the game gave for
 * them: the items' bytes, then a link per item. Nothing is allocated for an
 * item. */
typedef struct ult_data_pool {
    ult_combiner combiner;    /* the free list, and the lists of the pool's queues */
    retro_eventcount_t space; /* an item was given back */
    uint8_t* data;
    uint32_t capacity;
    uint64_t item_size;
    uint32_t free_head;
} ult_data_pool;

/* What ult_data_pool_init needs at the work area */
#define ULT_DATA_POOL_WORK_AREA(capacity, item_size)                                               \
    ((uint64_t)(capacity) * (item_size) + (uint64_t)(capacity) * sizeof(uint32_t))

void ult_data_pool_init(ult_data_pool* p, void* work_area, uint32_t capacity, uint64_t item_size);
void ult_data_pool_free(ult_data_pool* p);

/* A queue of items of a pool, first in first out. A push waits for a free item
 * of the pool, a pop for an item in the queue. */
typedef struct ult_queue {
    ult_data_pool* pool;
    uint32_t head; /* in the pool's combiner */
    uint32_t tail;
    retro_eventcount_t filled; /* an item was appended */
} ult_queue;

void ult_queue_init(ult_queue* q, ult_data_pool* pool);
void ult_queue_free(ult_queue* q);
int ult_queue_try_push(ult_queue* q, const void* item);
void ult_queue_push(ult_queue* q, const void* item);
/* @item may be null, to drop the item */
int ult_queue_try_pop(ult_queue* q, void* item);
void ult_queue_pop(ult_queue* q, void* item);

RETRO_END_DECLS

#endif
