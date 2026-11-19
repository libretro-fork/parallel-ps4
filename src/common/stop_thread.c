/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "common/stop_thread.h"

#include <stddef.h>

int stop_thread_start(stop_thread_t* t, void (*run)(void* arg), void* arg) {
    if (t->thread)
        return 0;
    retro_atomic_store_release_int(&t->stop, 0);
    t->thread = sthread_create(run, arg);
    return t->thread != NULL;
}

int stop_thread_started(const stop_thread_t* t) {
    return t->thread != NULL;
}

void stop_thread_request(stop_thread_t* t) {
    /* With a full fence, as the eventcount's notifier's state change: the thread's wait opens
     * its window, then reads this */
    (void)retro_atomic_exchange_int(&t->stop, 1);
}

void stop_thread_join(stop_thread_t* t) {
    if (!t->thread)
        return;
    sthread_join(t->thread);
    t->thread = NULL;
}
