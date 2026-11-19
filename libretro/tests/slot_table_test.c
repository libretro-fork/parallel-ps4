/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

/* The slot table (slot_table.c): four threads take and give back slots of a table smaller
 * than they would all like, each writing its mark in plain memory for the slot it has; never
 * two at once in a slot (the thread sanitizer would see it), and the lowest free one first. */

#include <stdio.h>

#include <retro_atomic.h>
#include <rthreads/rthreads.h>

#include "common/slot_table.h"

#define THREADS 4
#define SLOTS 6
#define FIRST 2
#define TURNS 100000

static retro_atomic_int_t slots[SLOTS];
static int owner[SLOTS];
static retro_atomic_int_t clashes;

static void user_run(void* arg) {
    const int me = (int)(size_t)arg;
    int turn, my_clashes = 0;
    for (turn = 0; turn < TURNS; turn++) {
        const int a = slot_table_take(slots, FIRST, SLOTS);
        const int b = slot_table_take(slots, FIRST, SLOTS);
        if (a >= 0) {
            if (owner[a] != 0) {
                my_clashes++;
            }
            owner[a] = me;
        }
        if (b >= 0) {
            if (owner[b] != 0) {
                my_clashes++;
            }
            owner[b] = me;
            owner[b] = 0;
            slot_table_give(slots, b);
        }
        if (a >= 0) {
            if (owner[a] != me) {
                my_clashes++;
            }
            owner[a] = 0;
            slot_table_give(slots, a);
        }
    }
    retro_atomic_fetch_add_int(&clashes, my_clashes);
}

int main(void) {
    sthread_t* threads[THREADS];
    int i, failures = 0;
    retro_atomic_int_init(&clashes, 0);
    for (i = 0; i < SLOTS; i++) {
        retro_atomic_int_init(&slots[i], 0);
    }
    /* Alone: lowest first, none below the range, -1 when full */
    if (slot_table_take(slots, FIRST, SLOTS) != FIRST ||
        slot_table_take(slots, FIRST, SLOTS) != FIRST + 1 || !slot_table_taken(slots, FIRST) ||
        slot_table_taken(slots, 0)) {
        printf("slot_table: not the lowest free slot\n");
        failures++;
    }
    slot_table_give(slots, FIRST);
    if (slot_table_take(slots, FIRST, SLOTS) != FIRST) {
        printf("slot_table: a slot given back not taken again\n");
        failures++;
    }
    while (slot_table_take(slots, FIRST, SLOTS) >= 0) {
    }
    if (slot_table_take(slots, FIRST, SLOTS) != -1) {
        printf("slot_table: a full table gave a slot\n");
        failures++;
    }
    for (i = FIRST; i < SLOTS; i++) {
        slot_table_give(slots, i);
    }

    for (i = 0; i < THREADS; i++) {
        threads[i] = sthread_create(user_run, (void*)(size_t)(i + 1));
    }
    for (i = 0; i < THREADS; i++) {
        sthread_join(threads[i]);
    }
    if (retro_atomic_load_acquire_int(&clashes)) {
        printf("slot_table: %d times a slot had by two threads\n",
               retro_atomic_load_acquire_int(&clashes));
        failures++;
    }
    for (i = 0; i < SLOTS; i++) {
        if (slot_table_taken(slots, i)) {
            printf("slot_table: slot %d still taken\n", i);
            failures++;
        }
    }
    if (failures) {
        printf("slot_table: %d FAILED\n", failures);
        return 1;
    }
    printf("slot_table: all passed (%d threads, %d turns each, %d slots)\n", THREADS, TURNS,
           SLOTS - FIRST);
    return 0;
}
