/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

/* thread_name: a thread's name, its own.
 *   - "" before it is named, NULL naming it "";
 *   - a name too long cut at THREAD_NAME_MAX, the caller's string not kept;
 *   - four threads naming themselves over and over: each finds its own name, never
 *     another's. */

#include <stdio.h>
#include <string.h>

#include <retro_atomic.h>
#include <rthreads/rthreads.h>

#include "common/thread_name.h"

static int failures = 0;

#define CHECK(cond, what)                                                                      \
    do {                                                                                       \
        if (!(cond)) {                                                                         \
            printf("FAIL %s (line %d)\n", what, __LINE__);                                     \
            ++failures;                                                                        \
        }                                                                                      \
    } while (0)

#define THREADS 4
#define ROUNDS 20000

static retro_atomic_int_t bad;
static retro_atomic_int_t unnamed;

static void worker(void* data) {
    char mine[32];
    int i;
    if (thread_name_get()[0] != '\0')
        retro_atomic_fetch_add_int(&unnamed, 1);
    for (i = 0; i < ROUNDS; i++) {
        sprintf(mine, "worker%d:%d", (int)(size_t)data, i & 7);
        thread_name_set(mine);
        mine[0] = 'X'; /* the name is a copy */
        if (strncmp(thread_name_get(), "worker", 6) != 0 ||
            strcmp(thread_name_get() + 6, mine + 6) != 0)
            retro_atomic_fetch_add_int(&bad, 1);
    }
}

int main(void) {
    sthread_t* threads[THREADS];
    char longer[THREAD_NAME_MAX + 20];
    int i;

    CHECK(strcmp(thread_name_get(), "") == 0, "unnamed: empty");
    thread_name_set("shadPS4:Main");
    CHECK(strcmp(thread_name_get(), "shadPS4:Main") == 0, "named");
    thread_name_set(NULL);
    CHECK(strcmp(thread_name_get(), "") == 0, "NULL: empty");
    memset(longer, 'a', sizeof(longer) - 1);
    longer[sizeof(longer) - 1] = '\0';
    thread_name_set(longer);
    CHECK(strlen(thread_name_get()) == THREAD_NAME_MAX, "cut at THREAD_NAME_MAX");

    retro_atomic_int_init(&bad, 0);
    retro_atomic_int_init(&unnamed, 0);
    thread_name_set("shadPS4:Test");
    for (i = 0; i < THREADS; i++)
        threads[i] = sthread_create(worker, (void*)(size_t)i);
    for (i = 0; i < THREADS; i++)
        sthread_join(threads[i]);
    CHECK(retro_atomic_load_acquire_int(&unnamed) == 0, "a new thread starts unnamed");
    CHECK(retro_atomic_load_acquire_int(&bad) == 0, "each its own name");
    CHECK(strcmp(thread_name_get(), "shadPS4:Test") == 0, "the main thread's untouched");

    if (failures) {
        printf("thread_name: %d failed\n", failures);
        return 1;
    }
    printf("thread_name: all passed\n");
    return 0;
}
