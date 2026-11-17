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
 *     starts (the core's is the guest thread's, which outlives it);
 *   - at_exit: called once as each of them ends, with its argument, after its exit and
 *     with its frames gone (its frame lies above the deepest one's). */

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
static retro_atomic_int_t exited;   /* at_exit calls */
static retro_atomic_int_t exit_bad; /* at_exit calls with a wrong argument, or on a frame */

/* Each thread's: how deep it goes, its deepest frame, and its at_exit argument */
struct ender {
    int depth;
    const char* deepest;
    int tag;
};

/* A frame's place on the thread's real stack (a local may lie elsewhere: AddressSanitizer
 * keeps locals on a stack of its own) */
#if defined(__GNUC__) || defined(__clang__)
#define FRAME_HERE() ((const char*)__builtin_frame_address(0))
#else
#define FRAME_HERE() ((const char*)0)
#endif

static void ended(void* arg) {
    struct ender* e = (struct ender*)arg;
    const char* here = FRAME_HERE();
    if (e->tag != 0x5eed || (e->deepest && here && (size_t)here <= (size_t)e->deepest))
        retro_atomic_fetch_add_int(&exit_bad, 1);
    retro_atomic_fetch_add_int(&exited, 1);
}

static int dive(struct ender* e, int depth) {
    char frame[64]; /* a frame of its own at each level */
    memset(frame, depth, sizeof(frame));
    if (depth == 0) {
        e->deepest = FRAME_HERE();
        retro_atomic_fetch_add_int(&reached, 1);
        host_thread_exit();
        retro_atomic_fetch_add_int(&past, 1);
        return 0;
    }
    if (dive(e, depth - 1) != 0 || frame[0] != (char)depth)
        retro_atomic_fetch_add_int(&past, 1);
    retro_atomic_fetch_add_int(&past, 1); /* reached only if the exit returned */
    return 1;
}

static void diver(void* arg) {
    struct ender* e = (struct ender*)arg;
    if (host_thread_is_current())
        retro_atomic_fetch_add_int(&current, 1);
    e->tag = 0x5eed;
    host_thread_at_exit(ended, e);
    dive(e, e->depth);
    retro_atomic_fetch_add_int(&past, 1);
}

static void returner(void* arg) {
    struct ender* e = (struct ender*)arg;
    e->tag = 0x5eed;
    host_thread_at_exit(ended, e);
    retro_atomic_fetch_add_int(&returned, 1);
}

#define BATCH 4
#define BATCHES 500

static host_thread_t many[BATCH * BATCHES];
static struct ender enders[2 + BATCH * BATCHES];

int main(void) {
    host_thread_t t[1];
    int i, b;

    retro_atomic_int_init(&reached, 0);
    retro_atomic_int_init(&past, 0);
    retro_atomic_int_init(&current, 0);
    retro_atomic_int_init(&returned, 0);
    retro_atomic_int_init(&exited, 0);
    retro_atomic_int_init(&exit_bad, 0);

    CHECK(!host_thread_is_current(), "not on the main thread");
    enders[0].depth = 50;
    CHECK(host_thread_start(&t[0], diver, &enders[0]), "started");
    CHECK(t[0].id != 0, "an id to signal it by");
    host_thread_join(&t[0]);
    CHECK(retro_atomic_load_acquire_int(&reached) == 1, "it reached the bottom");
    CHECK(retro_atomic_load_acquire_int(&past) == 0, "nothing ran after the exit");
    CHECK(retro_atomic_load_acquire_int(&current) == 1, "is_current inside");
    CHECK(retro_atomic_load_acquire_int(&exited) == 1, "at_exit called as it ended");
    CHECK(retro_atomic_load_acquire_int(&exit_bad) == 0, "at_exit off its frames");

    CHECK(host_thread_start(&t[0], returner, &enders[1]), "started");
    host_thread_join(&t[0]);
    CHECK(retro_atomic_load_acquire_int(&returned) == 1, "one that returns ends too");
    CHECK(retro_atomic_load_acquire_int(&exited) == 2, "at_exit called as it returned");

    for (b = 0; b < BATCHES; b++) {
        host_thread_t* batch = &many[b * BATCH];
        for (i = 0; i < BATCH; i++) {
            struct ender* e = &enders[2 + b * BATCH + i];
            e->depth = i * 7;
            CHECK(host_thread_start(&batch[i], diver, e), "started");
        }
        /* Released at once, running or not: rthreads frees what it holds when each ends */
        for (i = 0; i < BATCH; i++)
            host_thread_release(&batch[i]);
    }
    /* Released threads cannot be joined: their count says when they are all through */
    while (retro_atomic_load_acquire_int(&exited) < 2 + BATCH * BATCHES)
        sthread_yield();
    CHECK(retro_atomic_load_acquire_int(&reached) == 1 + BATCH * BATCHES, "every one ran");
    CHECK(retro_atomic_load_acquire_int(&past) == 0, "none ran after its exit");
    CHECK(retro_atomic_load_acquire_int(&exit_bad) == 0, "each at_exit off its frames");
    printf("host_thread: %d threads ended from inside\n", retro_atomic_load_acquire_int(&reached));

    if (failures) {
        printf("host_thread: %d failed\n", failures);
        return 1;
    }
    printf("host_thread: all passed\n");
    return 0;
}
