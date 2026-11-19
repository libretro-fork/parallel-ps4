/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

/* The video player's state machine (avplayer_fsm.c): its moves are the ones the player had
 * under a lock, and threads moving it at once lose none. Every move a thread made is counted
 * by the state it left and the one it entered; with nothing lost, each state was left as many
 * times as it was entered, but for the first and the last. And the transport (who starts and
 * stops it): one thread at a time, and no wish lost. */

#include <stdio.h>
#include <stdlib.h>

#include <rthreads/rthreads.h>

#include "core/libraries/avplayer/avplayer_fsm.h"

#define THREADS 4
#define MOVES 200000

static int failures;

#define CHECK(cond, ...)                                                                         \
    do {                                                                                         \
        if (!(cond)) {                                                                           \
            printf("avplayer_fsm: " __VA_ARGS__);                                                \
            printf("\n");                                                                        \
            failures++;                                                                          \
        }                                                                                        \
    } while (0)

/* The checks as the player made them, before this module (AvPlayerState::IsStateTransitionValid) */
static int reference_allowed(int from, int to) {
    switch (to) {
    case AVPLAYER_FSM_PLAY:
        return !(from == AVPLAYER_FSM_STOP || from == AVPLAYER_FSM_END_OF_FILE ||
                 from == AVPLAYER_FSM_ERROR);
    case AVPLAYER_FSM_PAUSE:
        return !(from == AVPLAYER_FSM_STOP || from == AVPLAYER_FSM_END_OF_FILE ||
                 from == AVPLAYER_FSM_STARTING || from == AVPLAYER_FSM_ERROR);
    case AVPLAYER_FSM_JUMP:
        return !(from == AVPLAYER_FSM_STOP || from == AVPLAYER_FSM_END_OF_FILE ||
                 from == AVPLAYER_FSM_TRICK_MODE || from == AVPLAYER_FSM_STARTING ||
                 from == AVPLAYER_FSM_ERROR);
    case AVPLAYER_FSM_TRICK_MODE:
        return !(from == AVPLAYER_FSM_STOP || from == AVPLAYER_FSM_END_OF_FILE ||
                 from == AVPLAYER_FSM_JUMP || from == AVPLAYER_FSM_STARTING ||
                 from == AVPLAYER_FSM_ERROR);
    case AVPLAYER_FSM_BUFFERING:
        return !(from == AVPLAYER_FSM_STOP || from == AVPLAYER_FSM_END_OF_FILE ||
                 from == AVPLAYER_FSM_PAUSE || from == AVPLAYER_FSM_STARTING ||
                 from == AVPLAYER_FSM_ERROR);
    default:
        return 1;
    }
}

static void test_table(void) {
    int from, to;
    for (from = 0; from < AVPLAYER_FSM_COUNT; from++) {
        for (to = 0; to < AVPLAYER_FSM_COUNT; to++) {
            CHECK(avplayer_fsm_allowed(from, to) == reference_allowed(from, to),
                  "%d -> %d: %d, the player had %d", from, to, avplayer_fsm_allowed(from, to),
                  reference_allowed(from, to));
        }
    }
}

static void test_single(void) {
    avplayer_fsm_t fsm;
    int from = -1, to = -1;
    avplayer_fsm_init(&fsm, AVPLAYER_FSM_INITIAL);
    CHECK(avplayer_fsm_current(&fsm) == AVPLAYER_FSM_INITIAL, "init: not initial");
    CHECK(avplayer_fsm_previous(&fsm) == AVPLAYER_FSM_UNKNOWN, "init: a state before");
    CHECK(avplayer_fsm_set(&fsm, AVPLAYER_FSM_READY, &from) && from == AVPLAYER_FSM_INITIAL,
          "initial -> ready");
    CHECK(avplayer_fsm_set(&fsm, AVPLAYER_FSM_PLAY, &from) && from == AVPLAYER_FSM_READY,
          "ready -> play");
    CHECK(avplayer_fsm_set(&fsm, AVPLAYER_FSM_PAUSE, NULL), "play -> pause");
    CHECK(avplayer_fsm_previous(&fsm) == AVPLAYER_FSM_PLAY, "pause: play not before it");
    /* Resume: back to playing, and pause is then the state before */
    CHECK(avplayer_fsm_revert(&fsm, &from, &to) && from == AVPLAYER_FSM_PAUSE &&
              to == AVPLAYER_FSM_PLAY,
          "revert from pause: %d -> %d", from, to);
    CHECK(avplayer_fsm_previous(&fsm) == AVPLAYER_FSM_PAUSE, "revert: pause not before");
    CHECK(avplayer_fsm_set(&fsm, AVPLAYER_FSM_STOP, NULL), "play -> stop");
    from = -1;
    CHECK(!avplayer_fsm_set(&fsm, AVPLAYER_FSM_PLAY, &from) && from == AVPLAYER_FSM_STOP,
          "stop -> play was made");
    CHECK(avplayer_fsm_current(&fsm) == AVPLAYER_FSM_STOP &&
              avplayer_fsm_previous(&fsm) == AVPLAYER_FSM_PLAY,
          "a refused move changed the state");
    /* Back to play from stop is refused too */
    CHECK(!avplayer_fsm_revert(&fsm, NULL, &to) && to == AVPLAYER_FSM_STOP,
          "stop -> play by revert was made");
    CHECK(avplayer_fsm_set(&fsm, AVPLAYER_FSM_STARTING, NULL), "stop -> starting");
    CHECK(avplayer_fsm_set(&fsm, AVPLAYER_FSM_STARTING, &from) && from == AVPLAYER_FSM_STARTING,
          "starting -> starting");
}

static avplayer_fsm_t shared;

typedef struct mover {
    unsigned seed;
    long entered[AVPLAYER_FSM_COUNT];
    long left[AVPLAYER_FSM_COUNT];
    long moves;
} mover_t;

static void mover_run(void* arg) {
    mover_t* m = (mover_t*)arg;
    int i;
    for (i = 0; i < MOVES; i++) {
        int from, to, done;
        m->seed = m->seed * 1103515245u + 12345u;
        if (((m->seed >> 16) & 7) == 0) {
            done = avplayer_fsm_revert(&shared, &from, &to);
        } else {
            to = (int)((m->seed >> 20) % AVPLAYER_FSM_COUNT);
            done = avplayer_fsm_set(&shared, to, &from);
        }
        if (done) {
            CHECK(avplayer_fsm_allowed(from, to), "a move %d -> %d was made", from, to);
            m->left[from]++;
            m->entered[to]++;
            m->moves++;
        }
    }
}

static void test_threads(void) {
    mover_t movers[THREADS] = {{0}};
    sthread_t* threads[THREADS];
    long balance[AVPLAYER_FSM_COUNT] = {0};
    long moves = 0;
    int i, s, last;
    avplayer_fsm_init(&shared, AVPLAYER_FSM_INITIAL);
    for (i = 0; i < THREADS; i++) {
        movers[i].seed = 7919u * (unsigned)(i + 1);
        threads[i] = sthread_create(mover_run, &movers[i]);
    }
    for (i = 0; i < THREADS; i++) {
        sthread_join(threads[i]);
    }
    for (i = 0; i < THREADS; i++) {
        moves += movers[i].moves;
        for (s = 0; s < AVPLAYER_FSM_COUNT; s++) {
            balance[s] += movers[i].entered[s] - movers[i].left[s];
        }
    }
    last = avplayer_fsm_current(&shared);
    for (s = 0; s < AVPLAYER_FSM_COUNT; s++) {
        long want = (s == last) - (s == AVPLAYER_FSM_INITIAL);
        CHECK(balance[s] == want, "state %d entered %ld times more than left, not %ld", s,
              balance[s], want);
    }
    CHECK(moves > THREADS * (long)MOVES / 4, "only %ld moves were made", moves);
    printf("avplayer_fsm: %ld moves from %d threads\n", moves, THREADS);
}

/* Threads start and stop at random; whoever has the transport does the work, counted in plain
 * memory (the thread sanitizer sees two at once). Every wish is done or replaced, once. */
static avplayer_transport_t transport;
static long inside, done_ops[3];

typedef struct asker {
    unsigned seed;
    long asked, replaced;
} asker_t;

static void transport_do(int op) {
    inside++;
    if (inside != 1) {
        failures++;
    }
    done_ops[op]++;
    inside--;
}

static void asker_run(void* arg) {
    asker_t* a = (asker_t*)arg;
    int i;
    for (i = 0; i < MOVES / 4; i++) {
        int op, replaced = AVPLAYER_TRANSPORT_NONE;
        a->seed = a->seed * 1103515245u + 12345u;
        op = (a->seed >> 16) & 1 ? AVPLAYER_TRANSPORT_START : AVPLAYER_TRANSPORT_STOP;
        a->asked++;
        if (avplayer_transport_claim(&transport, op, &replaced)) {
            do {
                transport_do(op);
            } while ((op = avplayer_transport_next(&transport)) != AVPLAYER_TRANSPORT_NONE);
        } else if (replaced != AVPLAYER_TRANSPORT_NONE) {
            a->replaced++;
        }
    }
}

static void test_transport(void) {
    asker_t askers[THREADS] = {{0}};
    sthread_t* threads[THREADS];
    long asked = 0, replaced = 0, done;
    int i, op = -1;
    avplayer_transport_init(&transport);
    /* Alone: has it at once, nothing left after */
    CHECK(avplayer_transport_claim(&transport, AVPLAYER_TRANSPORT_START, NULL), "not had alone");
    CHECK(!avplayer_transport_claim(&transport, AVPLAYER_TRANSPORT_STOP, &op) &&
              op == AVPLAYER_TRANSPORT_NONE,
          "had twice");
    CHECK(!avplayer_transport_claim(&transport, AVPLAYER_TRANSPORT_START, &op) &&
              op == AVPLAYER_TRANSPORT_STOP,
          "a wish not replaced");
    CHECK(avplayer_transport_next(&transport) == AVPLAYER_TRANSPORT_START, "the last wish lost");
    CHECK(avplayer_transport_next(&transport) == AVPLAYER_TRANSPORT_NONE, "a wish done twice");
    CHECK(avplayer_transport_claim(&transport, AVPLAYER_TRANSPORT_STOP, NULL), "not let go");
    CHECK(avplayer_transport_next(&transport) == AVPLAYER_TRANSPORT_NONE, "a wish from nowhere");

    for (i = 0; i < THREADS; i++) {
        askers[i].seed = 104729u * (unsigned)(i + 1);
        threads[i] = sthread_create(asker_run, &askers[i]);
    }
    for (i = 0; i < THREADS; i++) {
        sthread_join(threads[i]);
        asked += askers[i].asked;
        replaced += askers[i].replaced;
    }
    done = done_ops[AVPLAYER_TRANSPORT_START] + done_ops[AVPLAYER_TRANSPORT_STOP];
    CHECK(done + replaced == asked, "%ld asked, %ld done and %ld replaced", asked, done, replaced);
    CHECK(done_ops[AVPLAYER_TRANSPORT_NONE] == 0, "nothing was done");
    printf("avplayer_fsm: transport: %ld asked from %d threads, %ld done, %ld replaced\n", asked,
           THREADS, done, replaced);
}

int main(void) {
    test_table();
    test_single();
    test_threads();
    test_transport();
    if (failures) {
        printf("avplayer_fsm: %d FAILED\n", failures);
        return 1;
    }
    printf("avplayer_fsm: all passed\n");
    return 0;
}
