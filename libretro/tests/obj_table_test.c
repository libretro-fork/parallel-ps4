/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

/* The object table (obj_table.c): three threads hold and give back objects while a fourth
 * removes them and puts new ones in; an object held is never one already freed, and each is
 * freed exactly once, by whichever thread let go of it last. Alone: holds of a free, doomed
 * or out-of-range slot find nothing, and a doomed object with users is freed by the last. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <retro_atomic.h>
#include <rthreads/rthreads.h>

#include "common/obj_table.h"
#include "common/run_once.h"

#define SLOTS 4
#define ROUNDS 20000
#define HOLDERS 3

typedef struct object {
    retro_atomic_int_t freed;
    int index;
} object_t;

static obj_table_t table;
static retro_atomic_int_t done, frees, made, bad;
static int failures;

static void free_object(int index) {
    object_t* object = (object_t*)obj_table_peek(&table, index);
    if (retro_atomic_exchange_int(&object->freed, 1) != 0) {
        (void)retro_atomic_fetch_add_int(&bad, 1);
    }
    (void)retro_atomic_fetch_add_int(&frees, 1);
    obj_table_reuse(&table, index);
}

/* Every object made, given back to the heap at the end: one a holder wrongly kept could
 * still be looked at until then */
static object_t* all[ROUNDS + SLOTS + 8];

static object_t* new_object(void) {
    object_t* object = (object_t*)calloc(1, sizeof(*object));
    retro_atomic_int_init(&object->freed, 0);
    all[retro_atomic_fetch_add_int(&made, 1)] = object;
    return object;
}

static void holder(void* arg) {
    unsigned seed = 2654435761u * (unsigned)(size_t)arg;
    while (!retro_atomic_load_acquire_int(&done)) {
        int index;
        object_t* object;
        seed = seed * 1103515245u + 12345u;
        index = (int)((seed >> 16) % SLOTS);
        object = (object_t*)obj_table_hold(&table, index);
        if (!object) {
            continue;
        }
        if (retro_atomic_load_acquire_int(&object->freed)) {
            (void)retro_atomic_fetch_add_int(&bad, 1);
        }
        if (obj_table_release(&table, index)) {
            free_object(index);
        }
    }
}

/* Objects found by id: a stale id finds nothing once its slot holds another, and an object
 * removed while held is freed by the last put back */
typedef struct keyed {
    obj_head_t head;
    int freed;
} keyed_t;

static void keyed_destroy(obj_head_t* object) {
    ((keyed_t*)object)->freed++;
}

static void test_keyed(void) {
    obj_table_t keyed_table;
    keyed_t a, b;
    int64_t a_id;
    memset(&a, 0, sizeof(a));
    memset(&b, 0, sizeof(b));
    a.head.destroy = keyed_destroy;
    b.head.destroy = keyed_destroy;
    obj_table_init(&keyed_table, 2);
    if (!obj_table_put(&keyed_table, &a.head, 4, 16, (int64_t)7 << 16) ||
        (a.head.id >> 16) != 7 || (a.head.id & 0xf) != 1 ||
        obj_table_find(&keyed_table, a.head.id, 4) != &a.head) {
        printf("obj_table: an object not found by its id\n");
        failures++;
    }
    a_id = a.head.id;
    /* Removed while held: freed by the put back, not before */
    obj_table_remove(&keyed_table, &a.head);
    if (a.freed != 0 || obj_table_find(&keyed_table, a_id, 4) != NULL) {
        printf("obj_table: a removed object freed while held, or still found\n");
        failures++;
    }
    obj_table_put_back(&keyed_table, &a.head);
    if (a.freed != 1) {
        printf("obj_table: a removed object not freed by its last put back\n");
        failures++;
    }
    /* Its slot taken again: the old id finds nothing */
    if (!obj_table_put(&keyed_table, &b.head, 4, 16, (int64_t)7 << 16) || b.head.index != 0 ||
        b.head.id == a_id || obj_table_find(&keyed_table, a_id, 4) != NULL) {
        printf("obj_table: a stale id found the slot's new object\n");
        failures++;
    }
    /* Two holders delete it: one takes it off and frees it with its put back */
    if (obj_table_find(&keyed_table, b.head.id, 4) != &b.head ||
        obj_table_find(&keyed_table, b.head.id, 4) != &b.head ||
        obj_table_retire(&keyed_table, &b.head) != 1 ||
        obj_table_retire(&keyed_table, &b.head) != 0 || obj_table_users(&keyed_table, 0) != 2) {
        printf("obj_table: a held object retired by both of its deleters\n");
        failures++;
    }
    obj_table_put_back(&keyed_table, &b.head);
    obj_table_put_back(&keyed_table, &b.head);
    if (b.freed != 1 || obj_table_put(&keyed_table, &b.head, 4, 16, 0) != 1) {
        printf("obj_table: a retired object not freed by its last put back\n");
        failures++;
    }
    b.freed = 0;
    obj_table_destroy(&keyed_table);
    if (b.freed != 1) {
        printf("obj_table: an object left in the table not freed with it\n");
        failures++;
    }
}

int main(void) {
    sthread_t* threads[HOLDERS];
    object_t* a;
    int i, r, index;
    run_once_setup();
    test_keyed();
    obj_table_init(&table, SLOTS);
    retro_atomic_int_init(&done, 0);
    retro_atomic_int_init(&frees, 0);
    retro_atomic_int_init(&made, 0);
    retro_atomic_int_init(&bad, 0);

    /* Alone */
    a = new_object();
    index = obj_table_add(&table, a, 0, SLOTS);
    if (index != 0 || obj_table_hold(&table, 1) || obj_table_hold(&table, -1) ||
        obj_table_hold(&table, SLOTS) || obj_table_hold(&table, 0) != a ||
        obj_table_users(&table, 0) != 1 || obj_table_doom(&table, 0) != 0 ||
        obj_table_hold(&table, 0) || obj_table_live(&table, 0) || obj_table_doom(&table, 0) != -1 ||
        obj_table_add(&table, a, 0, 1) != -1 || !obj_table_release(&table, 0)) {
        printf("obj_table: taken, held or doomed wrong, alone\n");
        failures++;
    }
    free_object(0);
    if (obj_table_add(&table, new_object(), 0, SLOTS) != 0 || obj_table_doom(&table, 0) != 1) {
        printf("obj_table: a slot not taken again, or an unheld object not freed at once\n");
        failures++;
    }
    free_object(0);
    retro_atomic_store_release_int(&frees, 0);

    for (i = 0; i < SLOTS; i++) {
        obj_table_add(&table, new_object(), 0, SLOTS);
    }
    for (i = 0; i < HOLDERS; i++) {
        threads[i] = sthread_create(holder, (void*)(size_t)(i + 1));
    }
    /* Remove each in turn, then put a new one in its place once it is freed */
    for (r = 0; r < ROUNDS; r++) {
        const int victim = r % SLOTS;
        const int doomed = obj_table_doom(&table, victim);
        object_t* fresh;
        if (doomed == 1) {
            free_object(victim);
        }
        fresh = new_object();
        while (obj_table_add(&table, fresh, victim, victim + 1) < 0) {
            /* Not freed yet: its last holder is about to */
            sthread_yield();
        }
    }
    retro_atomic_store_release_int(&done, 1);
    for (i = 0; i < HOLDERS; i++) {
        sthread_join(threads[i]);
    }
    for (i = 0; i < SLOTS; i++) {
        if (obj_table_doom(&table, i) == 1) {
            free_object(i);
        }
    }
    if (retro_atomic_load_acquire_int(&bad) != 0) {
        printf("obj_table: %d times an object freed twice or seen after it was freed\n",
               retro_atomic_load_acquire_int(&bad));
        failures++;
    }
    /* The two made alone were freed before the count was cleared */
    if (retro_atomic_load_acquire_int(&frees) + 2 != retro_atomic_load_acquire_int(&made)) {
        printf("obj_table: %d objects made, %d freed\n", retro_atomic_load_acquire_int(&made),
               retro_atomic_load_acquire_int(&frees));
        failures++;
    }
    for (i = 0; i < retro_atomic_load_acquire_int(&made); i++) {
        free(all[i]);
    }
    obj_table_free(&table);
    if (failures) {
        printf("obj_table: %d FAILED\n", failures);
        return 1;
    }
    printf("obj_table: all passed (%d objects through %d slots, %d threads holding them)\n",
           r + SLOTS, SLOTS, HOLDERS);
    return 0;
}
