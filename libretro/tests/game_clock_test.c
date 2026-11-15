/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

/* game_clock: the game's time from the frontend's frames.
 *   - 0, standing, until the first frame;
 *   - a frame unused: the next starts where it ended; 60 frames are exactly one second,
 *     3600 exactly a minute (the fraction carried);
 *   - an advance moves within the frame, never past its end, never back;
 *   - 59.94 (60000/1001): a minute of frames is exactly 1001/1000 of a minute;
 *   - four threads advancing to their own deadlines while frames start: the time never
 *     goes back, never passes the frame's end, and ends at the last frame's end. */

#include <stdio.h>

#include <retro_atomic.h>
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

    if (failures) {
        printf("game_clock: %d failed\n", failures);
        return 1;
    }
    printf("game_clock: all passed\n");
    return 0;
}
