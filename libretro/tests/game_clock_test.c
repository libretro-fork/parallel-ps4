/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

/* game_clock: the game's time from the frontend's frames.
 *   - 0, standing, until the first frame;
 *   - a frame unused: the next starts where it ended; 60 frames are exactly one second,
 *     3600 exactly a minute (the fraction carried);
 *   - an advance moves within the frame, never past its end, never back;
 *   - 59.94 (60000/1001): a minute of frames is exactly 1001/1000 of a minute;
 *   - four threads advancing to their own deadlines while frames start: the time never
 *     goes back, never passes the frame's end, and ends at the last frame's end;
 *   - timers: one not yet due does not fire, a due one fires at the frame that reaches it,
 *     one disarmed in time never fires, one armed in the past fires at once; oneshots free
 *     their room when they fire;
 *   - a game thread sleeping 1 ms at a time, alone: the time skips to each deadline as soon
 *     as it waits, so it gets every iteration a frame's time holds (16 or 17), and no more;
 *   - the same with another game thread that never waits: no skipping, one iteration a
 *     frame, at the frame's start;
 *   - a thread parked on a word is waiting on it until the word moves (a wake sent) and
 *     waiting again once it parks at the new value; parking and frames tell the sound's
 *     output, which waits for that. */

#include <stdio.h>

#include <retro_atomic.h>
#include <rthreads/retro_eventcount.h>
#include <rthreads/rthreads.h>

#include "common/game_clock.h"

static int failures = 0;

#define CHECK(cond, what)                                                                      \
    do {                                                                                       \
        if (!(cond)) {                                                                         \
            printf("FAIL %s (line %d)\n", what, __LINE__);                                     \
            ++failures;                                                                        \
        }                                                                                      \
    } while (0)

static game_clock_t clock_;
static retro_atomic_int_t stop;
static retro_atomic_int_t bad;

static void advancer(void* data) {
    const uint64_t step = 100000u * (1 + (uint64_t)(size_t)data); /* 0.1..0.4 ms */
    uint64_t last = 0;
    while (!retro_atomic_load_acquire_int(&stop)) {
        const uint64_t now = game_clock_now(&clock_);
        const uint64_t got = game_clock_advance(&clock_, now + step);
        const uint64_t end = game_clock_frame_end(&clock_);
        if (got < last || got > end)
            retro_atomic_fetch_add_int(&bad, 1);
        last = got;
    }
}

#define THREADS 4

static retro_atomic_int_t fired_count;
static void count_fire(void* ctx) {
    (void)ctx;
    retro_atomic_fetch_add_int(&fired_count, 1);
}

/* A game thread that sleeps 1 ms of the game's time at a time */
static retro_atomic_int_t iterations;
static retro_atomic_int_t woken;
static retro_atomic_int_t sleeper_stop;
static retro_eventcount_t wake_ec;
static retro_atomic_int_t sleeper_parked; /* it has waited once */

static void wake_sleeper(void* ctx) {
    (void)ctx;
    game_clock_busy(&clock_); /* handed to the woken thread */
    retro_atomic_store_release_int(&woken, 1);
    retro_eventcount_notify(&wake_ec);
}

static void sleeper(void* data) {
    (void)data;
    game_clock_busy(&clock_);
    while (!retro_atomic_load_acquire_int(&sleeper_stop)) {
        const uint64_t deadline = game_clock_now(&clock_) + 1000000;
        int timer;
        retro_atomic_store_release_int(&woken, 0);
        timer = game_clock_arm(&clock_, deadline, wake_sleeper, NULL);
        game_clock_idle(&clock_);
        retro_atomic_store_release_int(&sleeper_parked, 1);
        for (;;) {
            const int key = retro_eventcount_prepare_wait(&wake_ec);
            if (retro_atomic_load_acquire_int(&woken) || retro_atomic_load_acquire_int(&sleeper_stop)) {
                retro_eventcount_cancel_wait(&wake_ec);
                break;
            }
            retro_eventcount_commit_wait(&wake_ec, key);
        }
        if (!game_clock_disarm(&clock_, timer))
            game_clock_busy(&clock_); /* stopped, not woken by the timer */
        if (retro_atomic_load_acquire_int(&woken))
            retro_atomic_fetch_add_int(&iterations, 1);
    }
    game_clock_idle(&clock_);
}

static retro_atomic_int_t spinner_stop;
static void spinner(void* data) {
    (void)data;
    while (!retro_atomic_load_acquire_int(&spinner_stop))
        sthread_yield();
    game_clock_idle(&clock_);
}

/* Until the sleeper has used the frame up: the time at the frame's end, nobody running */
static void wait_frame_used(void) {
    while (!(game_clock_now(&clock_) == game_clock_frame_end(&clock_) &&
             game_clock_running(&clock_) == 0))
        sthread_yield();
}

/* Until the sleeper is parked: it has waited, and nobody runs but the spinner */
static void wait_parked(int others) {
    while (!retro_atomic_load_acquire_int(&sleeper_parked) ||
           game_clock_running(&clock_) != others)
        sthread_yield();
}

static void timers_test(void) {
    sthread_t* t[2];
    int i, timer;

    game_clock_init(&clock_, 60, 1);
    retro_atomic_int_init(&fired_count, 0);
    game_clock_busy(&clock_); /* this thread counts as a game thread here */
    game_clock_frame(&clock_);
    timer = game_clock_arm(&clock_, 10000000, count_fire, NULL); /* due at the next frame's start */
    CHECK(timer >= 0, "armed");
    CHECK(retro_atomic_load_acquire_int(&fired_count) == 0, "not due: not fired");
    game_clock_frame(&clock_); /* starts at 16.7 ms */
    CHECK(retro_atomic_load_acquire_int(&fired_count) == 1, "fired at the frame reaching it");
    CHECK(game_clock_disarm(&clock_, timer) == 1, "disarm: it fired");
    timer = game_clock_arm(&clock_, 50000000, count_fire, NULL);
    CHECK(game_clock_disarm(&clock_, timer) == 0, "disarmed in time");
    game_clock_frame(&clock_);
    game_clock_frame(&clock_);
    CHECK(retro_atomic_load_acquire_int(&fired_count) == 1, "a disarmed timer never fires");
    timer = game_clock_arm(&clock_, 1000, count_fire, NULL);
    CHECK(retro_atomic_load_acquire_int(&fired_count) == 2, "armed in the past: fires at once");
    game_clock_disarm(&clock_, timer);
    /* A oneshot frees itself: armed a thousand times past the room, all fire */
    for (i = 0; i < 4 * GAME_CLOCK_TIMERS; i++) {
        CHECK(game_clock_arm_oneshot(&clock_, game_clock_now(&clock_) + 1, count_fire, NULL) >= 0,
              "oneshot armed");
        game_clock_frame(&clock_);
    }
    CHECK(retro_atomic_load_acquire_int(&fired_count) == 2 + 4 * GAME_CLOCK_TIMERS,
          "oneshots fire and free their room");
    game_clock_idle(&clock_);

    /* Alone: every iteration a frame holds */
    game_clock_init(&clock_, 60, 1);
    retro_eventcount_init(&wake_ec);
    retro_atomic_int_init(&iterations, 0);
    retro_atomic_int_init(&sleeper_stop, 0);
    retro_atomic_int_init(&sleeper_parked, 0);
    t[0] = sthread_create(sleeper, NULL);
    wait_parked(0);
    for (i = 0; i < 60; i++) {
        game_clock_frame(&clock_);
        wait_frame_used();
    }
    CHECK(game_clock_now(&clock_) == 1000000000u, "a second of the game's time");
    printf("game_clock: alone, %d 1 ms sleeps in 60 frames\n",
           retro_atomic_load_acquire_int(&iterations));
    CHECK(retro_atomic_load_acquire_int(&iterations) >= 999 &&
              retro_atomic_load_acquire_int(&iterations) <= 1000,
          "alone: every 1 ms sleep a second holds");
    retro_atomic_store_release_int(&sleeper_stop, 1);
    retro_eventcount_notify(&wake_ec);
    sthread_join(t[0]);

    /* With a thread that never waits: one iteration a frame */
    game_clock_init(&clock_, 60, 1);
    retro_atomic_int_init(&iterations, 0);
    retro_atomic_int_init(&sleeper_stop, 0);
    retro_atomic_int_init(&spinner_stop, 0);
    retro_atomic_int_init(&sleeper_parked, 0);
    game_clock_busy(&clock_); /* the spinner's, counted before it starts */
    t[1] = sthread_create(spinner, NULL);
    t[0] = sthread_create(sleeper, NULL);
    wait_parked(1);
    for (i = 0; i < 60; i++) {
        const int before = retro_atomic_load_acquire_int(&iterations);
        game_clock_frame(&clock_);
        /* Its deadline, 1 ms into the last frame, is due at this frame's start */
        if (i > 0)
            while (retro_atomic_load_acquire_int(&iterations) == before)
                sthread_yield();
        wait_parked(1);
    }
    printf("game_clock: with a busy thread, %d 1 ms sleeps in 60 frames\n",
           retro_atomic_load_acquire_int(&iterations));
    CHECK(retro_atomic_load_acquire_int(&iterations) == 59, "busy thread: one a frame");
    CHECK(game_clock_now(&clock_) == 59ull * 1000000000u / 60, "no skipping");
    retro_atomic_store_release_int(&sleeper_stop, 1);
    retro_atomic_store_release_int(&spinner_stop, 1);
    retro_eventcount_notify(&wake_ec);
    sthread_join(t[0]);
    sthread_join(t[1]);
    retro_eventcount_free(&wake_ec);
}

int main(void) {
    sthread_t* threads[THREADS];
    uint64_t t;
    int i;

    game_clock_init(&clock_, 60, 1);
    CHECK(game_clock_now(&clock_) == 0, "0 before the first frame");
    CHECK(game_clock_advance(&clock_, 5000000) == 0, "standing before the first frame");

    game_clock_frame(&clock_);
    CHECK(game_clock_now(&clock_) == 0, "the first frame starts at 0");
    CHECK(game_clock_frame_end(&clock_) == 16666666, "and ends a 60th of a second on");
    CHECK(game_clock_advance(&clock_, 1000000) == 1000000, "an advance within it");
    CHECK(game_clock_advance(&clock_, 500000) == 1000000, "never back");
    CHECK(game_clock_advance(&clock_, 99999999) == 16666666, "never past the frame's end");
    game_clock_frame(&clock_);
    CHECK(game_clock_now(&clock_) == 16666666, "the next frame starts where it ended");
    for (i = 2; i < 60; i++)
        game_clock_frame(&clock_);
    CHECK(game_clock_frame_end(&clock_) == 1000000000u, "60 frames: exactly a second");
    for (i = 60; i < 3600; i++)
        game_clock_frame(&clock_);
    CHECK(game_clock_frame_end(&clock_) == 60000000000ull, "3600 frames: exactly a minute");
    CHECK(game_clock_frames(&clock_) == 3600, "frames counted");

    /* What the sound's output waits on: a word a thread is parked on at its current value
     * (waiting), and one moved on since it parked (a wake sent, not yet taken) */
    {
        retro_atomic_int_t word;
        int slot;
        int epoch;
        retro_atomic_int_init(&word, 7);
        CHECK(!game_clock_waiting_on(&clock_, &word), "nobody parked on it yet");
        CHECK(!game_clock_wake_sent(&clock_, &word), "and no wake sent");
        epoch = retro_atomic_load_acquire_int(&clock_.parked_ec.epoch);
        slot = game_clock_park(&clock_, &word, 7);
        CHECK(slot >= 0, "parked");
        CHECK(retro_atomic_load_acquire_int(&clock_.parked_ec.epoch) != epoch,
              "parking tells the output");
        CHECK(game_clock_waiting_on(&clock_, &word), "waiting on it");
        CHECK(!game_clock_wake_sent(&clock_, &word), "not woken");
        retro_atomic_fetch_add_int(&word, 1);
        CHECK(!game_clock_waiting_on(&clock_, &word), "woken: not waiting");
        CHECK(game_clock_wake_sent(&clock_, &word), "a wake sent");
        game_clock_repark(&clock_, slot, 8);
        CHECK(game_clock_waiting_on(&clock_, &word), "waiting again at the new value");
        CHECK(!game_clock_wake_sent(&clock_, &word), "the wake taken");
        game_clock_unpark(&clock_, slot);
        CHECK(!game_clock_waiting_on(&clock_, &word), "gone once it runs");
        CHECK(!game_clock_wake_sent(&clock_, &word), "and no wake left");
        epoch = retro_atomic_load_acquire_int(&clock_.parked_ec.epoch);
        game_clock_frame(&clock_);
        CHECK(retro_atomic_load_acquire_int(&clock_.parked_ec.epoch) != epoch,
              "a frame tells the output too");
    }

    game_clock_init(&clock_, 60000, 1001);
    for (i = 0; i < 3600; i++)
        game_clock_frame(&clock_);
    CHECK(game_clock_frame_end(&clock_) == 60060000000ull, "59.94: 3600 frames, 60.06 s");

    game_clock_init(&clock_, 60, 1);
    game_clock_frame(&clock_);
    retro_atomic_int_init(&stop, 0);
    retro_atomic_int_init(&bad, 0);
    for (i = 0; i < THREADS; i++)
        threads[i] = sthread_create(advancer, (void*)(size_t)i);
    for (i = 0; i < 20000; i++) {
        game_clock_frame(&clock_);
        if ((i & 255) == 0)
            sthread_yield();
    }
    /* Let the advancers use the last frame up */
    while (game_clock_now(&clock_) < game_clock_frame_end(&clock_))
        sthread_yield();
    retro_atomic_store_release_int(&stop, 1);
    for (i = 0; i < THREADS; i++)
        sthread_join(threads[i]);
    t = game_clock_now(&clock_);
    CHECK(retro_atomic_load_acquire_int(&bad) == 0, "never back, never past the frame's end");
    CHECK(t == game_clock_frame_end(&clock_), "ends at the last frame's end");
    CHECK(t == 20001ull * 1000000000u / 60, "exactly that many frames' time");

    timers_test();

    if (failures) {
        printf("game_clock: %d failed\n", failures);
        return 1;
    }
    printf("game_clock: all passed\n");
    return 0;
}
