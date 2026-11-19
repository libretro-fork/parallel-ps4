/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

/* The bounded many-to-one queue (mpsc_ring.c): four threads put numbered items into a queue
 * too small for them, one takes; every item taken once, whole, each thread's in its order, and
 * a full queue refuses rather than overwrites. */

#include <stdio.h>
#include <string.h>

#include <retro_atomic.h>
#include <rthreads/rthreads.h>

#include "common/mpsc_ring.h"

#define PUTTERS 4
#define ITEMS 50000

typedef struct item {
    int putter;
    int n;
    int check[6]; /* each n: a torn item shows */
} item_t;

static mpsc_ring_t ring;
static retro_atomic_int_t done_putters;
static int failures;

static void putter(void* arg) {
    const int me = (int)(size_t)arg;
    int n;
    for (n = 0; n < ITEMS; n++) {
        item_t it;
        int i;
        it.putter = me;
        it.n = n;
        for (i = 0; i < 6; i++) {
            it.check[i] = n ^ (me << 20);
        }
        while (!mpsc_ring_put(&ring, &it)) {
            sthread_yield();
        }
    }
    (void)retro_atomic_fetch_add_int(&done_putters, 1);
}

int main(void) {
    sthread_t* threads[PUTTERS];
    int next[PUTTERS];
    long taken = 0, torn = 0, order = 0;
    item_t it;
    int i;

    if (!mpsc_ring_init(&ring, 4, sizeof(item_t)) || !mpsc_ring_empty(&ring)) {
        printf("mpsc_ring: not made empty\n");
        return 1;
    }
    for (i = 0; i < 4; i++) {
        it.n = i;
        if (!mpsc_ring_put(&ring, &it)) {
            printf("mpsc_ring: refused below its size\n");
            failures++;
        }
    }
    if (mpsc_ring_put(&ring, &it)) {
        printf("mpsc_ring: a full queue took one more\n");
        failures++;
    }
    for (i = 0; i < 4; i++) {
        if (!mpsc_ring_take(&ring, &it) || it.n != i) {
            printf("mpsc_ring: not in order alone\n");
            failures++;
        }
    }
    if (mpsc_ring_take(&ring, &it) || !mpsc_ring_empty(&ring)) {
        printf("mpsc_ring: an empty queue gave one\n");
        failures++;
    }
    mpsc_ring_free(&ring);

    mpsc_ring_init(&ring, 8, sizeof(item_t));
    retro_atomic_int_init(&done_putters, 0);
    for (i = 0; i < PUTTERS; i++) {
        next[i] = 0;
        threads[i] = sthread_create(putter, (void*)(size_t)i);
    }
    for (;;) {
        if (mpsc_ring_take(&ring, &it)) {
            for (i = 0; i < 6; i++) {
                if (it.check[i] != (it.n ^ (it.putter << 20))) {
                    torn++;
                    break;
                }
            }
            if (it.putter < 0 || it.putter >= PUTTERS || it.n != next[it.putter]) {
                order++;
            } else {
                next[it.putter]++;
            }
            taken++;
            continue;
        }
        if (retro_atomic_load_acquire_int(&done_putters) == PUTTERS && mpsc_ring_empty(&ring)) {
            break;
        }
        sthread_yield();
    }
    for (i = 0; i < PUTTERS; i++) {
        sthread_join(threads[i]);
    }
    if (taken != (long)PUTTERS * ITEMS || torn || order) {
        printf("mpsc_ring: %ld taken of %d, %ld torn, %ld out of order\n", taken,
               PUTTERS * ITEMS, torn, order);
        failures++;
    }
    mpsc_ring_free(&ring);
    if (failures) {
        printf("mpsc_ring: %d FAILED\n", failures);
        return 1;
    }
    printf("mpsc_ring: all passed (%ld items from %d threads, through 8 slots)\n", taken,
           PUTTERS);
    return 0;
}
