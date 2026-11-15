/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

/* node_pool: a lock-free pool of free nodes.
 *   - empty, then nodes given back come out again, the last first;
 *   - four threads taking and giving back the same few nodes as fast as they can: a node is
 *     never held by two at once (each marks it its own, and finds it as it left it), none is
 *     lost, and what a thread wrote into a node is what the next thread to take it finds. */

#include <stdio.h>
#include <string.h>

#include <retro_atomic.h>
#include <rthreads/rthreads.h>

#include "common/node_pool.h"

static int failures = 0;

#define CHECK(cond, what)                                                                      \
    do {                                                                                       \
        if (!(cond)) {                                                                         \
            printf("FAIL %s (line %d)\n", what, __LINE__);                                     \
            ++failures;                                                                        \
        }                                                                                      \
    } while (0)

typedef struct item {
    node_pool_node_t link; /* first */
    retro_atomic_int_t holder;
    unsigned value;
} item_t;

#define THREADS 4
#define ITEMS 8
#define ROUNDS 200000

static node_pool_t pool;
static item_t items[ITEMS];
static retro_atomic_int_t bad;
static retro_atomic_int_t handed;

static void worker(void* data) {
    const int me = (int)(size_t)data;
    unsigned last = 0;
    int i;
    for (i = 0; i < ROUNDS; i++) {
        item_t* it = (item_t*)node_pool_pop(&pool);
        if (!it)
            continue;
        if (retro_atomic_exchange_int(&it->holder, me) != 0)
            retro_atomic_fetch_add_int(&bad, 1);
        /* The value the last holder left: odd, and the next of what it found */
        if ((it->value & 1u) == 0)
            retro_atomic_fetch_add_int(&bad, 1);
        last = it->value;
        it->value = last + 2;
        if (retro_atomic_load_acquire_int(&it->holder) != me)
            retro_atomic_fetch_add_int(&bad, 1);
        retro_atomic_store_release_int(&it->holder, 0);
        retro_atomic_fetch_add_int(&handed, 1);
        node_pool_push(&pool, &it->link);
    }
    (void)last;
}

int main(void) {
    sthread_t* threads[THREADS];
    item_t a, b;
    unsigned total = 0;
    int i, count = 0;

    node_pool_init(&pool);
    CHECK(node_pool_pop(&pool) == NULL, "empty");
    node_pool_push(&pool, &a.link);
    node_pool_push(&pool, &b.link);
    CHECK((item_t*)node_pool_pop(&pool) == &b, "the last given back first");
    CHECK((item_t*)node_pool_pop(&pool) == &a, "then the one before");
    CHECK(node_pool_pop(&pool) == NULL, "empty again");

    retro_atomic_int_init(&bad, 0);
    retro_atomic_int_init(&handed, 0);
    for (i = 0; i < ITEMS; i++) {
        retro_atomic_int_init(&items[i].holder, 0);
        items[i].value = 1;
        node_pool_push(&pool, &items[i].link);
    }
    for (i = 0; i < THREADS; i++)
        threads[i] = sthread_create(worker, (void*)(size_t)(i + 1));
    for (i = 0; i < THREADS; i++)
        sthread_join(threads[i]);
    CHECK(retro_atomic_load_acquire_int(&bad) == 0, "never held by two, found as it was left");
    while (node_pool_pop(&pool))
        count++;
    CHECK(count == ITEMS, "none lost");
    for (i = 0; i < ITEMS; i++)
        total += (items[i].value - 1) / 2;
    CHECK(total == (unsigned)retro_atomic_load_acquire_int(&handed),
          "every hand-over counted in the nodes");
    printf("node_pool: %d hand-overs\n", retro_atomic_load_acquire_int(&handed));

    if (failures) {
        printf("node_pool: %d failed\n", failures);
        return 1;
    }
    printf("node_pool: all passed\n");
    return 0;
}
