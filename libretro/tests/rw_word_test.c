/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

/* The guest rwlocks' word (rw_word.c): four threads read and write a block of plain memory
 * through it, a writer rewriting every word of it, readers checking it whole; never a reader
 * beside a writer nor two writers, which the thread sanitizer would see. A thread turned away
 * tries again after a yield, as its caller's park would have it. */

#include <stdio.h>

#include <retro_atomic.h>
#include <rthreads/rthreads.h>

#include "common/rw_word.h"

#define THREADS 4
#define TURNS 50000
#define WORDS 32

static retro_atomic_int_t word;
static int block[WORDS];
static retro_atomic_int_t readers_in, writers_in;
static int failures;

typedef struct user {
    unsigned seed;
    long reads, writes, torn, beside;
} user_t;

static void user_run(void* arg) {
    user_t* u = (user_t*)arg;
    int turn, i;
    for (turn = 0; turn < TURNS; turn++) {
        u->seed = u->seed * 1103515245u + 12345u;
        if ((u->seed >> 16) % 4 == 0) {
            while (!rw_word_try_write(&word)) {
                sthread_yield();
            }
            if (retro_atomic_fetch_add_int(&writers_in, 1) != 0 ||
                retro_atomic_load_acquire_int(&readers_in) != 0) {
                u->beside++;
            }
            for (i = 0; i < WORDS; i++) {
                block[i] = turn;
            }
            retro_atomic_fetch_add_int(&writers_in, -1);
            u->writes++;
            if (rw_word_unlock_write(&word) != 1) {
                u->beside++;
            }
        } else {
            while (!rw_word_try_read(&word)) {
                sthread_yield();
            }
            retro_atomic_fetch_add_int(&readers_in, 1);
            if (retro_atomic_load_acquire_int(&writers_in) != 0) {
                u->beside++;
            }
            for (i = 1; i < WORDS; i++) {
                if (block[i] != block[0]) {
                    u->torn++;
                    break;
                }
            }
            retro_atomic_fetch_add_int(&readers_in, -1);
            u->reads++;
            (void)rw_word_unlock_read(&word);
        }
    }
}

int main(void) {
    user_t users[THREADS] = {{0}};
    sthread_t* threads[THREADS];
    long reads = 0, writes = 0, torn = 0, beside = 0;
    int i;
    rw_word_init(&word);
    retro_atomic_int_init(&readers_in, 0);
    retro_atomic_int_init(&writers_in, 0);

    /* Alone: readers share it, a writer has it alone, the last reader out frees it */
    if (!rw_word_try_read(&word) || !rw_word_try_read(&word) || rw_word_try_write(&word) ||
        rw_word_unlock_read(&word) || !rw_word_unlock_read(&word) || !rw_word_try_write(&word) ||
        rw_word_try_read(&word) || rw_word_try_write(&word) || !rw_word_unlock_write(&word) ||
        retro_atomic_load_acquire_int(&word) != 0 || rw_word_unlock_read(&word) != -1 ||
        rw_word_unlock_write(&word) != -1 || !rw_word_try_read(&word) ||
        rw_word_unlock_write(&word) != -1 || rw_word_unlock_read(&word) != 1 ||
        retro_atomic_load_acquire_int(&word) != 0) {
        printf("rw_word: taken or given back wrong, alone\n");
        failures++;
    }

    for (i = 0; i < THREADS; i++) {
        users[i].seed = 2654435761u * (unsigned)(i + 1);
        threads[i] = sthread_create(user_run, &users[i]);
    }
    for (i = 0; i < THREADS; i++) {
        sthread_join(threads[i]);
        reads += users[i].reads;
        writes += users[i].writes;
        torn += users[i].torn;
        beside += users[i].beside;
    }
    if (torn || beside) {
        printf("rw_word: %ld reads found the block half written, %ld times in beside a writer\n",
               torn, beside);
        failures++;
    }
    if (retro_atomic_load_acquire_int(&word) != 0) {
        printf("rw_word: not free at the end\n");
        failures++;
    }
    if (failures) {
        printf("rw_word: %d FAILED\n", failures);
        return 1;
    }
    printf("rw_word: all passed (%ld reads, %ld writes from %d threads)\n", reads, writes,
           THREADS);
    return 0;
}
