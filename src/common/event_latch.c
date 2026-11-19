/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "common/event_latch.h"

void event_latch_init(event_latch_t* latch) {
    retro_atomic_int_init(&latch->set, 0);
    retro_eventcount_init(&latch->waiters);
}

void event_latch_free(event_latch_t* latch) {
    retro_eventcount_free(&latch->waiters);
}

void event_latch_set(event_latch_t* latch) {
    retro_atomic_store_release_int(&latch->set, 1);
    retro_eventcount_notify(&latch->waiters);
}

void event_latch_clear(event_latch_t* latch) {
    retro_atomic_store_release_int(&latch->set, 0);
}

int event_latch_is_set(event_latch_t* latch) {
    return retro_atomic_load_acquire_int(&latch->set) != 0;
}

void event_latch_wait(event_latch_t* latch) {
    /* The wait is opened before the flag is read again: a set between the two is seen by the
     * read, one after it by the eventcount */
    while (!event_latch_is_set(latch)) {
        const int key = retro_eventcount_prepare_wait(&latch->waiters);
        if (event_latch_is_set(latch)) {
            retro_eventcount_cancel_wait(&latch->waiters);
            return;
        }
        retro_eventcount_commit_wait(&latch->waiters, key);
    }
}
