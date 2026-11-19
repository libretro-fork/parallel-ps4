/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

/* The reader gate (pin_gate.c): an owner rewrites a block of plain memory behind it, word by
 * word, while readers come and go; a reader let in must always find the block whole (every
 * word the same), and the thread sanitizer must see no race on it. Then: closing an empty
 * gate returns at once, and a closed gate turns a reader away without letting it in. */

#include <stdio.h>

#include <retro_atomic.h>
#include <rthreads/rthreads.h>

#include "common/pin_gate.h"

#define READERS 3
#define WORDS 64
#define ROUNDS 20000

static pin_gate_t gate;
static int block[WORDS];
static retro_atomic_int_t done;
static int failures;

typedef struct reader {
    long in, away, torn;
} reader_t;

static void reader_run(void* arg) {
    reader_t* r = (reader_t*)arg;
    while (!retro_atomic_load_acquire_int(&done)) {
        int i, first;
        if (!pin_gate_enter(&gate)) {
            r->away++;
            continue;
        }
        first = block[0];
        for (i = 1; i < WORDS; i++) {
            if (block[i] != first) {
                r->torn++;
                break;
            }
        }
        r->in++;
        pin_gate_leave(&gate);
    }
}

int main(void) {
    reader_t readers[READERS] = {{0}};
    sthread_t* threads[READERS];
    long in = 0, away = 0, torn = 0;
    int i, round;

    pin_gate_init(&gate);
    retro_atomic_int_init(&done, 0);
    for (i = 0; i < READERS; i++) {
        threads[i] = sthread_create(reader_run, &readers[i]);
    }
    for (round = 1; round <= ROUNDS; round++) {
        pin_gate_close(&gate);
        for (i = 0; i < WORDS; i++) {
            block[i] = round;
        }
        pin_gate_open(&gate);
    }
    retro_atomic_store_release_int(&done, 1);
    for (i = 0; i < READERS; i++) {
        sthread_join(threads[i]);
        in += readers[i].in;
        away += readers[i].away;
        torn += readers[i].torn;
    }
    if (torn != 0) {
        printf("pin_gate: %ld of %ld reads found the block half rewritten\n", torn, in);
        failures++;
    }
    if (in == 0) {
        printf("pin_gate: no reader got in\n");
        failures++;
    }

    /* Empty: closes at once. Closed: a reader is turned away and is not counted in, so the
     * owner's next close does not wait for it */
    pin_gate_close(&gate);
    if (pin_gate_enter(&gate)) {
        printf("pin_gate: a reader got into a closed gate\n");
        failures++;
    }
    pin_gate_close(&gate);
    pin_gate_open(&gate);
    /* Shut with a reader in: not drained until it leaves */
    if (!pin_gate_enter(&gate)) {
        printf("pin_gate: a reader was turned away from an open gate\n");
        failures++;
    } else {
        pin_gate_shut(&gate);
        if (pin_gate_drained(&gate)) {
            printf("pin_gate: drained with a reader in\n");
            failures++;
        }
        pin_gate_leave(&gate);
        if (!pin_gate_drained(&gate)) {
            printf("pin_gate: not drained once the reader left\n");
            failures++;
        }
        pin_gate_open(&gate);
    }
    if (!pin_gate_enter(&gate)) {
        printf("pin_gate: a reader was turned away from an open gate\n");
        failures++;
    } else {
        pin_gate_leave(&gate);
    }
    pin_gate_free(&gate);

    if (failures) {
        printf("pin_gate: %d FAILED\n", failures);
        return 1;
    }
    printf("pin_gate: all passed (%d rewrites, %ld reads let in, %ld turned away)\n", ROUNDS,
           in, away);
    return 0;
}
