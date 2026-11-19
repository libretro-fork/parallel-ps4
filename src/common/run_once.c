/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "common/run_once.h"

#include <rthreads/retro_eventcount.h>

/* Notified whenever any once is done: the threads that met another at a first time wait here */
static retro_eventcount_t run_once_done;
static int run_once_ready;

void run_once_setup(void) {
    if (run_once_ready)
        return;
    retro_eventcount_init(&run_once_done);
    run_once_ready = 1;
}

void run_once_slow(run_once_t* once, void (*fn)(void* arg), void* arg) {
    for (;;) {
        int key;
        if (retro_atomic_load_acquire_int(&once->state) == 2)
            return;
        if (retro_atomic_cas_int(&once->state, 0, 1)) {
            fn(arg);
            /* With a full fence, as an eventcount's notifier's state change */
            (void)retro_atomic_exchange_int(&once->state, 2);
            retro_eventcount_notify(&run_once_done);
            return;
        }
        /* Another thread is at it */
        key = retro_eventcount_prepare_wait(&run_once_done);
        if (retro_atomic_load_acquire_int(&once->state) == 2) {
            retro_eventcount_cancel_wait(&run_once_done);
            return;
        }
        retro_eventcount_commit_wait(&run_once_done, key);
    }
}
