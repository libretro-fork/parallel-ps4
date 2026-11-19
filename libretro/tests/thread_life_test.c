/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

/* A game thread's life (thread_life.c): its record collected exactly once, and only after it
 * has ended, is detached and no one holds it, however the end, the detach or the join and
 * the holds of other threads fall. Alone, the answers libthr gave: no join of a detached
 * thread, one joiner, no detach of one being joined. */

#include <stdio.h>

#include <retro_atomic.h>
#include <rthreads/rthreads.h>

#include "common/thread_life.h"

#define ROUNDS 20000
#define HOLDERS 3

static int failures;

#define CHECK(cond, what)                                                                      \
    do {                                                                                       \
        if (!(cond)) {                                                                         \
            printf("thread_life: FAILED: %s\n", what);                                         \
            failures++;                                                                        \
        }                                                                                      \
    } while (0)

static void test_alone(void) {
    retro_atomic_int_t life;
    int collect = -1;
    thread_life_init(&life, 0);
    CHECK(thread_life_join(&life) == THREAD_LIFE_OK, "joined");
    CHECK(thread_life_join(&life) == THREAD_LIFE_JOINED_ALREADY, "a second joiner");
    CHECK(thread_life_detach(&life, &collect) == THREAD_LIFE_JOINED_ALREADY,
          "detached while joined");
    thread_life_unjoin(&life);
    CHECK(thread_life_hold(&life, 0) == THREAD_LIFE_OK, "held while running");
    CHECK(!thread_life_end(&life), "collected while held");
    CHECK(thread_life_hold(&life, 0) == THREAD_LIFE_GONE, "held as live once ended");
    CHECK(!thread_life_release(&life), "collected while not detached");
    CHECK(thread_life_join(&life) == THREAD_LIFE_OK, "joined once ended");
    CHECK(thread_life_joined(&life), "not collected once joined");
    CHECK(thread_life_hold(&life, 1) == THREAD_LIFE_GONE, "held once handed over");

    thread_life_init(&life, 1);
    CHECK(thread_life_join(&life) == THREAD_LIFE_DETACHED_ALREADY, "a detached thread joined");
    CHECK(thread_life_detach(&life, &collect) == THREAD_LIFE_DETACHED_ALREADY,
          "detached twice");
    CHECK(thread_life_end(&life), "a detached thread not collected as it ended");
}

/* Each round: a thread ends, another detaches or joins it, three hold and release it, all at
 * once; the main thread then checks it was collected once and makes it anew */
#define PARTIES 6
static retro_atomic_int_t shared_life;
static retro_atomic_int_t collected;
static retro_atomic_int_t arrived;

static void note(int collect) {
    if (collect) {
        (void)retro_atomic_fetch_add_int(&collected, 1);
    }
}

/* All PARTIES threads meet here: the count only grows, each meeting is the next multiple */
static void meet(void) {
    const int n = retro_atomic_fetch_add_int(&arrived, 1) + 1;
    const int target = ((n + PARTIES - 1) / PARTIES) * PARTIES;
    while (retro_atomic_load_acquire_int(&arrived) < target) {
        sthread_yield();
    }
}

static void ender(void* arg) {
    int r;
    (void)arg;
    for (r = 1; r <= ROUNDS; r++) {
        meet();
        note(thread_life_end(&shared_life));
        meet();
    }
}

static void detacher(void* arg) {
    int r;
    (void)arg;
    for (r = 1; r <= ROUNDS; r++) {
        int collect = 0;
        meet();
        if (r & 1) {
            if (thread_life_detach(&shared_life, &collect) == THREAD_LIFE_OK) {
                note(collect);
            }
        } else if (thread_life_join(&shared_life) == THREAD_LIFE_OK) {
            /* The joiner waits for the end before it takes the result */
            while (!thread_life_is(&shared_life, THREAD_LIFE_DEAD)) {
                sthread_yield();
            }
            note(thread_life_joined(&shared_life));
        }
        meet();
    }
}

static void holder(void* arg) {
    int r;
    (void)arg;
    for (r = 1; r <= ROUNDS; r++) {
        meet();
        if (thread_life_hold(&shared_life, 1) == THREAD_LIFE_OK) {
            note(thread_life_release(&shared_life));
        }
        meet();
    }
}

static void test_threads(void) {
    sthread_t* threads[PARTIES - 1];
    int i, r, wrong = 0;
    retro_atomic_int_init(&arrived, 0);
    retro_atomic_int_init(&collected, 0);
    thread_life_init(&shared_life, 0);
    threads[0] = sthread_create(ender, NULL);
    threads[1] = sthread_create(detacher, NULL);
    for (i = 0; i < HOLDERS; i++) {
        threads[2 + i] = sthread_create(holder, NULL);
    }
    for (r = 1; r <= ROUNDS; r++) {
        meet();
        meet();
        if (retro_atomic_load_acquire_int(&collected) != 1) {
            wrong++;
        }
        retro_atomic_store_release_int(&collected, 0);
        thread_life_init(&shared_life, 0);
    }
    for (i = 0; i < PARTIES - 1; i++) {
        sthread_join(threads[i]);
    }
    CHECK(wrong == 0, "a record collected other than once in a round");
    printf("thread_life: %d rounds of an end, a detach or join and %d holds at once\n", ROUNDS,
           HOLDERS);
}

int main(void) {
    test_alone();
    test_threads();
    if (failures) {
        printf("thread_life: %d FAILED\n", failures);
        return 1;
    }
    printf("thread_life: all passed\n");
    return 0;
}
