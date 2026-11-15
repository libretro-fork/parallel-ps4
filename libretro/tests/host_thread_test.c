/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

/* host_thread: a thread ended from anywhere in it, as a guest thread by pthread_exit.
 *   - ended from deep in its calls: nothing after the exit runs, the frames between are
 *     left, the thread ends (a join returns);
 *   - one that returns on its own ends the same way;
 *   - is_current: set inside, not outside;
 *   - four at a time, 2000 in all, each released (detached) while it may still be running:
 *     every one ran, none ran past its exit, nothing is left (AddressSanitizer's leak check
 *     sees what rthreads holds). Each has its own host_thread_t, which a thread reads as it
 *     starts (the core's is the guest thread's, which outlives it). */

#include <stdio.h>
#include <string.h>

#include <retro_atomic.h>
#include <rthreads/rthreads.h>

#include "common/host_thread.h"

static int failures = 0;

#define CHECK(cond, what)                                                                      \
    do {                                                                                       \
        if (!(cond)) {                                                                         \
            printf("FAIL %s (line %d)\n", what, __LINE__);                                     \
            ++failures;                                                                        \
        }                                                                                      \
    } while (0)

static retro_atomic_int_t reached;  /* the deepest call reached */
static retro_atomic_int_t past;     /* code after an exit: must never run */
static retro_atomic_int_t current;  /* is_current inside */
static retro_atomic_int_t returned; /* threads that returned on their own */

static int dive(int depth) {
    char frame[64]; /* a frame of its own at each level */
    memset(frame, depth, sizeof(frame));
    if (depth == 0) {
        retro_atomic_fetch_add_int(&reached, 1);
        host_thread_exit();
        retro_atomic_fetch_add_int(&past, 1);
        return 0;
    }
    if (dive(depth - 1) != 0 || frame[0] != (char)depth)
        retro_atomic_fetch_add_int(&past, 1);
    retro_atomic_fetch_add_int(&past, 1); /* reached only if the exit returned */
    return 1;
}

static void diver(void* arg) {
    if (host_thread_is_current())
        retro_atomic_fetch_add_int(&current, 1);
    dive((int)(size_t)arg);
    retro_atomic_fetch_add_int(&past, 1);
}

static void returner(void* arg) {
    (void)arg;
    retro_atomic_fetch_add_int(&returned, 1);
}

#define BATCH 4
#define BATCHES 500

static host_thread_t many[BATCH * BATCHES];

int main(void) {
    host_thread_t t[1];
    int i, b;

    retro_atomic_int_init(&reached, 0);
    retro_atomic_int_init(&past, 0);
    retro_atomic_int_init(&current, 0);
    retro_atomic_int_init(&returned, 0);

    CHECK(!host_thread_is_current(), "not on the main thread");
    CHECK(host_thread_start(&t[0], diver, (void*)(size_t)50), "started");
    CHECK(t[0].id != 0, "an id to signal it by");
    host_thread_join(&t[0]);
    CHECK(retro_atomic_load_acquire_int(&reached) == 1, "it reached the bottom");
    CHECK(retro_atomic_load_acquire_int(&past) == 0, "nothing ran after the exit");
    CHECK(retro_atomic_load_acquire_int(&current) == 1, "is_current inside");

    CHECK(host_thread_start(&t[0], returner, NULL), "started");
    host_thread_join(&t[0]);
    CHECK(retro_atomic_load_acquire_int(&returned) == 1, "one that returns ends too");

    for (b = 0; b < BATCHES; b++) {
        host_thread_t* batch = &many[b * BATCH];
        for (i = 0; i < BATCH; i++)
            CHECK(host_thread_start(&batch[i], diver, (void*)(size_t)(i * 7)), "started");
        /* Released at once, running or not: rthreads frees what it holds when each ends */
        for (i = 0; i < BATCH; i++)
            host_thread_release(&batch[i]);
    }
    /* Released threads cannot be joined: their count says when they are all through */
    while (retro_atomic_load_acquire_int(&reached) < 1 + BATCH * BATCHES)
        sthread_yield();
    CHECK(retro_atomic_load_acquire_int(&past) == 0, "none ran after its exit");
    printf("host_thread: %d threads ended from inside\n", retro_atomic_load_acquire_int(&reached));

    if (failures) {
        printf("host_thread: %d failed\n", failures);
        return 1;
    }
    printf("host_thread: all passed\n");
    return 0;
}
