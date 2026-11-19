/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "common/thread_life.h"

#define LIFE_FLAGS (THREAD_LIFE_REF - 1)

/* Ended, detached, held by no one, and not yet handed over: the move that makes it so hands
 * it over */
static int ready(int word) {
    return (word & (THREAD_LIFE_DEAD | THREAD_LIFE_DETACHED | THREAD_LIFE_COLLECTED)) ==
               (THREAD_LIFE_DEAD | THREAD_LIFE_DETACHED) &&
           word < THREAD_LIFE_REF;
}

/* Moves the word by @f; if the move made it ready, marks it handed over in the same step */
static int move(retro_atomic_int_t* life, int set, int clear, int refs) {
    for (;;) {
        const int word = retro_atomic_load_acquire_int(life);
        int next = ((word | set) & ~clear) + refs * THREAD_LIFE_REF;
        int collect = ready(next);
        if (collect) {
            next |= THREAD_LIFE_COLLECTED;
        }
        if (retro_atomic_cas_int(life, word, next)) {
            return collect;
        }
    }
}

void thread_life_init(retro_atomic_int_t* life, int detached) {
    retro_atomic_int_init(life, THREAD_LIFE_REF | (detached ? THREAD_LIFE_DETACHED : 0));
}

int thread_life_hold(retro_atomic_int_t* life, int include_dead) {
    for (;;) {
        const int word = retro_atomic_load_acquire_int(life);
        if ((word & THREAD_LIFE_COLLECTED) || (!include_dead && (word & THREAD_LIFE_DEAD))) {
            return THREAD_LIFE_GONE;
        }
        if (retro_atomic_cas_int(life, word, word + THREAD_LIFE_REF)) {
            return THREAD_LIFE_OK;
        }
    }
}

int thread_life_release(retro_atomic_int_t* life) {
    return move(life, 0, 0, -1);
}

int thread_life_end(retro_atomic_int_t* life) {
    return move(life, THREAD_LIFE_DEAD, 0, -1);
}

int thread_life_detach(retro_atomic_int_t* life, int* collect) {
    for (;;) {
        const int word = retro_atomic_load_acquire_int(life);
        int next;
        if (word & THREAD_LIFE_DETACHED) {
            return THREAD_LIFE_DETACHED_ALREADY;
        }
        if (word & THREAD_LIFE_JOINED) {
            return THREAD_LIFE_JOINED_ALREADY;
        }
        next = word | THREAD_LIFE_DETACHED;
        *collect = ready(next);
        if (*collect) {
            next |= THREAD_LIFE_COLLECTED;
        }
        if (retro_atomic_cas_int(life, word, next)) {
            return THREAD_LIFE_OK;
        }
    }
}

int thread_life_join(retro_atomic_int_t* life) {
    for (;;) {
        const int word = retro_atomic_load_acquire_int(life);
        if (word & THREAD_LIFE_DETACHED) {
            return THREAD_LIFE_DETACHED_ALREADY;
        }
        if (word & THREAD_LIFE_JOINED) {
            return THREAD_LIFE_JOINED_ALREADY;
        }
        if (retro_atomic_cas_int(life, word, word | THREAD_LIFE_JOINED)) {
            return THREAD_LIFE_OK;
        }
    }
}

void thread_life_unjoin(retro_atomic_int_t* life) {
    (void)move(life, 0, THREAD_LIFE_JOINED, 0);
}

int thread_life_joined(retro_atomic_int_t* life) {
    return move(life, THREAD_LIFE_DETACHED, THREAD_LIFE_JOINED, 0);
}

int thread_life_is(retro_atomic_int_t* life, int flag) {
    return (retro_atomic_load_acquire_int(life) & flag) != 0;
}
