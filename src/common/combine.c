/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <stddef.h>

#include <rthreads/rthreads.h>

#include "common/combine.h"

typedef struct combine_request {
    mpsc_stack_node_t node; /* first: the stack links requests through it */
    void (*call)(void* ctx);
    void* ctx;
    retro_atomic_int_t done;
} combine_request_t;

/* One value per thread, which no other thread has while it lives (never null) */
static void* combine_this_thread(void) {
    return (void*)(sthread_get_current_thread_id() | 1);
}

void combiner_init(combiner_t* c) {
    mpsc_stack_init(&c->pending);
    retro_atomic_int_init(&c->busy, 0);
    retro_atomic_ptr_init(&c->owner, NULL);
    retro_eventcount_init(&c->done_ec);
}

void combiner_free(combiner_t* c) {
    retro_eventcount_free(&c->done_ec);
}

/* With busy held: runs what is queued, then lets go of busy, unless more came meanwhile and
 * this thread can take busy again. */
static void combiner_combine(combiner_t* c) {
    for (;;) {
        mpsc_stack_node_t* node = mpsc_stack_reverse(mpsc_stack_drain(&c->pending));
        const int ran = node != NULL;
        retro_atomic_store_relaxed_ptr(&c->owner, combine_this_thread());
        while (node) {
            combine_request_t* request = (combine_request_t*)node;
            node = node->next;
            request->call(request->ctx);
            /* The caller's: it may be gone as soon as done is set */
            retro_atomic_store_release_int(&request->done, 1);
        }
        retro_atomic_store_relaxed_ptr(&c->owner, NULL);
        if (ran)
            retro_eventcount_notify(&c->done_ec);
        (void)retro_atomic_exchange_int(&c->busy, 0);
        /* A push whose taking of busy failed is seen here (combiner_run's fence) */
        retro_atomic_thread_fence_seq_cst();
        if (mpsc_stack_empty(&c->pending) || retro_atomic_exchange_int(&c->busy, 1) != 0)
            return;
    }
}

void combiner_run(combiner_t* c, void (*call)(void* ctx), void* ctx) {
    combine_request_t request;
    /* From inside a function this combiner runs: it holds the combiner already */
    if (retro_atomic_load_relaxed_ptr(&c->owner) == combine_this_thread()) {
        call(ctx);
        return;
    }
    request.node.next = NULL;
    request.call = call;
    request.ctx = ctx;
    retro_atomic_int_init(&request.done, 0);
    mpsc_stack_push(&c->pending, &request.node);
    /* Pairs with combiner_combine's fence: a push that loses the race for busy is seen there */
    retro_atomic_thread_fence_seq_cst();
    for (;;) {
        int key;
        if (retro_atomic_exchange_int(&c->busy, 1) == 0)
            combiner_combine(c);
        if (retro_atomic_load_acquire_int(&request.done) != 0)
            return;
        key = retro_eventcount_prepare_wait(&c->done_ec);
        /* Not run yet, and nobody combining: whoever released found it pushed and is about to
         * take over, or this thread can */
        if (retro_atomic_load_acquire_int(&request.done) != 0 ||
            retro_atomic_load_seq_cst_int(&c->busy) == 0) {
            retro_eventcount_cancel_wait(&c->done_ec);
            continue;
        }
        retro_eventcount_commit_wait(&c->done_ec, key);
    }
}
