/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

/* A thread's name read while another renames it (word_name.c): a name read is always one of
 * the names given or a mix of their bytes, terminated, never past its end; set and got back
 * whole when alone, and cut to its length. */

#include <stdio.h>
#include <string.h>

#include <retro_atomic.h>
#include <rthreads/rthreads.h>

#include "common/word_name.h"

static word_name_t name;
static retro_atomic_int_t done;
static int failures;

static void renamer(void* arg) {
    int i;
    (void)arg;
    for (i = 0; i < 200000; i++) {
        word_name_set(&name, (i & 1) ? "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA" : "bbbb");
    }
    retro_atomic_store_release_int(&done, 1);
}

int main(void) {
    char out[WORD_NAME_MAX + 1];
    sthread_t* thread;
    long reads = 0, bad = 0;
    word_name_init(&name);
    if (word_name_get(&name, out) != 0) {
        printf("word_name: not empty at first\n");
        failures++;
    }
    word_name_set(&name, "shadPS4:GameThread");
    if (word_name_get(&name, out) != 18 || strcmp(out, "shadPS4:GameThread") != 0) {
        printf("word_name: not the name given\n");
        failures++;
    }
    word_name_set(&name, "0123456789012345678901234567890123456789");
    if (word_name_get(&name, out) != WORD_NAME_MAX || memcmp(out, "0123456789", 10) != 0) {
        printf("word_name: a long name not cut to %d\n", WORD_NAME_MAX);
        failures++;
    }
    word_name_set(&name, NULL);
    if (word_name_get(&name, out) != 0) {
        printf("word_name: no name not empty\n");
        failures++;
    }

    retro_atomic_int_init(&done, 0);
    thread = sthread_create(renamer, NULL);
    while (!retro_atomic_load_acquire_int(&done)) {
        size_t len = word_name_get(&name, out), i;
        for (i = 0; i < len; i++) {
            if (out[i] != 'A' && out[i] != 'b') {
                bad++;
                break;
            }
        }
        reads++;
    }
    sthread_join(thread);
    if (bad) {
        printf("word_name: %ld of %ld reads had a byte of no name\n", bad, reads);
        failures++;
    }
    if (failures) {
        printf("word_name: %d FAILED\n", failures);
        return 1;
    }
    printf("word_name: all passed (%ld reads during renames)\n", reads);
    return 0;
}
