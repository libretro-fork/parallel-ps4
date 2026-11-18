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
 *     output, which waits for that;
 *   - work in flight on a thread that is not the game's (kernel AIO's worker): a game thread
 *     waiting for it with a timeout, alone, has the time skip to its deadline at once and
 *     times out before the work is done; counted running from its submit until it is done
 *     (game_clock_busy, game_clock_idle), the work holds the time and the wait ends with it;
 *   - the time the game is told (game_clock_read): a microsecond on at each read while the
 *     time stands, never behind the time, never past the frame's end, standing there until
 *     the next frame, and moving no deadline; a draw seeded from the clock and drawn again
 *     until it differs (what Dead or Alive Xtreme 3 Fortune does, a call deeper each time)
 *     ends at its second draw, where on the standing time it never would; four threads
 *     reading while frames start are each told times that never go back, and no time twice
 *     before a frame's end. */

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

/* ---- work in flight on a thread that is not the game's ---- */

static game_clock_t io_clock;
static retro_eventcount_t io_ec;
static retro_atomic_int_t io_done;
static retro_atomic_int_t io_counted;
static retro_atomic_int_t io_gave_up; /* the wait is over, whatever ended it */
static retro_atomic_64_t io_now_when_done;

static int io_is_done(void* ctx) {
    (void)ctx;
    return retro_atomic_load_acquire_int(&io_done) != 0;
}

static void io_worker(void* data) {
    (void)data;
    if (retro_atomic_load_acquire_int(&io_counted)) {
        /* The read takes until well after the game thread has parked for it */
        while (!game_clock_waiting_on(&io_clock, &io_ec.epoch) &&
               !retro_atomic_load_acquire_int(&io_gave_up))
            sthread_yield();
        sthread_yield();
    } else {
        /* A read slower than the timeout: done only once the game thread's wait has ended
         * without it. (Done as soon as the game thread had parked, it raced the timeout the
         * skipped time fires on that thread, and now and then won.) */
        while (!retro_atomic_load_acquire_int(&io_gave_up))
            sthread_yield();
    }
    retro_atomic_store_release_64(&io_now_when_done, (int64_t)game_clock_now(&io_clock));
    retro_atomic_store_release_int(&io_done, 1);
    if (retro_atomic_load_acquire_int(&io_counted))
        game_clock_idle(&io_clock);
    retro_eventcount_notify(&io_ec);
}

/* 1 if the wait ended with the work, 0 if its timeout ended it first */
static int io_wait(int counted, uint64_t* deadline_out) {
    game_clock_thread_t self = {0, 0};
    sthread_t* worker;
    uint64_t deadline;
    int ok;
    game_clock_init(&io_clock, 60, 1);
    game_clock_frame(&io_clock);
    game_clock_busy(&io_clock); /* the game thread */
    retro_eventcount_init(&io_ec);
    retro_atomic_int_init(&io_done, 0);
    retro_atomic_int_init(&io_counted, counted);
    retro_atomic_int_init(&io_gave_up, 0);
    retro_atomic_64_init(&io_now_when_done, 0);
    /* Submitted: in flight */
    if (counted)
        game_clock_busy(&io_clock);
    worker = sthread_create(io_worker, NULL);
    deadline = game_clock_now(&io_clock) + 5000000u; /* 5 ms, within the frame */
    ok = game_clock_wait(&io_clock, &io_ec, deadline, io_is_done, NULL, &self);
    retro_atomic_store_release_int(&io_gave_up, 1);
    sthread_join(worker);
    game_clock_idle(&io_clock);
    retro_eventcount_free(&io_ec);
    *deadline_out = deadline;
    return ok;
}

static void in_flight_test(void) {
    uint64_t deadline = 0;
    CHECK(!io_wait(0, &deadline), "not counted: the wait times out before the work is done");
    CHECK((uint64_t)retro_atomic_load_acquire_64(&io_now_when_done) >= deadline,
          "the time had skipped to its deadline");
    CHECK(io_wait(1, &deadline), "counted: the wait ends with the work");
    CHECK((uint64_t)retro_atomic_load_acquire_64(&io_now_when_done) < deadline,
          "the time held until it was done");
}

/* The game's draw: a generator seeded from the clock in microseconds, as its
 * sceKernelGetProcessTime gives it, drawn again until the draw is not @taken. Returns the
 * draws it took, or @limit if it never ended. */
static unsigned seeded_draw(uint32_t seed) {
    seed ^= seed >> 30;
    return (unsigned)((seed * 1812433253u + 1u) >> 8) % 26u;
}

static int draws_until_free(game_clock_t* clock, int moving, int limit) {
    unsigned taken = 0;
    int draws;
    for (draws = 1; draws <= limit; draws++) {
        const uint64_t ns = moving ? game_clock_read(clock) : game_clock_now(clock);
        const unsigned slot = seeded_draw((uint32_t)(ns / 1000u));
        if (draws == 1)
            taken = slot; /* the first slot drawn is taken */
        else if (slot != taken)
            return draws;
    }
    return limit;
}

static game_clock_t read_clock;
static retro_atomic_int_t read_stop;
static retro_atomic_int_t read_bad;
static retro_atomic_64_t read_seen[4][4096];

static void reader(void* data) {
    const int me = (int)(size_t)data;
    uint64_t last = 0;
    int n = 0;
    while (!retro_atomic_load_acquire_int(&read_stop)) {
        const uint64_t end_before = game_clock_frame_end(&read_clock);
        const uint64_t t = game_clock_read(&read_clock);
        if (t < last || t > game_clock_frame_end(&read_clock))
            retro_atomic_fetch_add_int(&read_bad, 1);
        last = t;
        /* Kept: the times told short of a frame's end, which no two reads are told */
        if (n < 4096 && t < end_before)
            retro_atomic_store_release_64(&read_seen[me][n++], (int64_t)t);
    }
}

static void read_test(void) {
    game_clock_t* c = &read_clock;
    sthread_t* threads[4];
    uint64_t t, end;
    int i, j, a, b, twice = 0;

    game_clock_init(c, 60, 1);
    CHECK(game_clock_read(c) == 0, "told 0 before the first frame");
    CHECK(game_clock_read(c) == 0, "and again: no frame, no time");
    game_clock_frame(c);
    end = game_clock_frame_end(c);
    t = game_clock_read(c);
    CHECK(t == GAME_CLOCK_READ_STEP, "the first read of a frame: a microsecond in");
    CHECK(game_clock_read(c) == t + GAME_CLOCK_READ_STEP, "the next: a microsecond on");
    CHECK(game_clock_now(c) == 0, "the time itself has not moved");
    /* The game's draw ends on the time it is told, never on the time that stands */
    CHECK(draws_until_free(c, 1, 100000) == 2, "a draw seeded from the clock ends at its second");
    CHECK(draws_until_free(c, 0, 100000) == 100000, "on the standing time it never ends");
    /* The time moves past what was told: told the time */
    game_clock_advance(c, 5000000);
    CHECK(game_clock_read(c) == 5000000, "never behind the time");
    CHECK(game_clock_read(c) == 5000000 + GAME_CLOCK_READ_STEP, "and on from there");
    /* A frame's reads end at the frame's end and stand there */
    for (i = 0; i < 20000; i++)
        t = game_clock_read(c);
    CHECK(t == end, "a frame's reads reach its end");
    CHECK(game_clock_read(c) == end, "and stand there");
    CHECK(game_clock_now(c) == 5000000, "the time still where the game's threads left it");
    game_clock_frame(c);
    CHECK(game_clock_read(c) == end + GAME_CLOCK_READ_STEP, "the next frame: on again");
    CHECK(game_clock_read(c) <= game_clock_frame_end(c), "within it");

    /* Four threads reading while frames start */
    retro_atomic_int_init(&read_stop, 0);
    retro_atomic_int_init(&read_bad, 0);
    for (i = 0; i < 4; i++)
        for (j = 0; j < 4096; j++)
            retro_atomic_64_init(&read_seen[i][j], 0);
    for (i = 0; i < 4; i++)
        threads[i] = sthread_create(reader, (void*)(size_t)i);
    for (i = 0; i < 200; i++) {
        game_clock_frame(c);
        sthread_yield();
    }
    retro_atomic_store_release_int(&read_stop, 1);
    for (i = 0; i < 4; i++)
        sthread_join(threads[i]);
    CHECK(retro_atomic_load_acquire_int(&read_bad) == 0,
          "threads: never back, never past the frame's end");
    /* No time told twice, unless it is a frame's end (where the reads stand) */
    for (a = 0; a < 4; a++)
        for (b = a + 1; b < 4; b++)
            for (i = 0; i < 4096; i += 7)
                for (j = 0; j < 4096; j++) {
                    const uint64_t x = (uint64_t)retro_atomic_load_acquire_64(&read_seen[a][i]);
                    const uint64_t y = (uint64_t)retro_atomic_load_acquire_64(&read_seen[b][j]);
                    if (x != 0 && x == y)
                        twice++;
                }
    CHECK(twice == 0, "threads: no time within a frame told twice");
    retro_eventcount_free(&c->parked_ec);
}

int main(void) {
    sthread_t* threads[THREADS];
    uint64_t t;
    int i;

    game_clock_init(&clock_, 60, 1);
    CHECK(game_clock_fmod(&clock_), "FMOD's output hold on until the game is looked at");
    game_clock_set_fmod(&clock_, 0);
    CHECK(!game_clock_fmod(&clock_), "off for a game without FMOD");
    game_clock_set_fmod(&clock_, 1);
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
        epoch = retro_atomic_load_acquire_int(&clock_.parked_ec.epoch);
        slot = game_clock_park(&clock_, &word, 7);
        CHECK(slot >= 0, "parked");
        CHECK(retro_atomic_load_acquire_int(&clock_.parked_ec.epoch) != epoch,
              "parking tells the output");
        CHECK(game_clock_waiting_on(&clock_, &word), "waiting on it");
        retro_atomic_fetch_add_int(&word, 1);
        CHECK(!game_clock_waiting_on(&clock_, &word), "woken: not waiting");
        game_clock_repark(&clock_, slot, 8);
        CHECK(game_clock_waiting_on(&clock_, &word), "waiting again at the new value");
        game_clock_unpark(&clock_, slot);
        CHECK(!game_clock_waiting_on(&clock_, &word), "gone once it runs");
        epoch = retro_atomic_load_acquire_int(&clock_.parked_ec.epoch);
        game_clock_frame(&clock_);
        CHECK(retro_atomic_load_acquire_int(&clock_.parked_ec.epoch) != epoch,
              "a frame tells the output too");
    }

    /* The output's hold (game_clock_woken_waiting): until the threads it woke wait again, or a
     * whole frame has gone by (the second to start: the first may start at once) */
    {
        retro_atomic_int_t a, b;
        retro_atomic_int_t* words[2];
        int sa, sb;
        uint64_t frame;
        retro_atomic_int_init(&a, 3);
        retro_atomic_int_init(&b, 5);
        words[0] = &a;
        words[1] = &b;
        sa = game_clock_park(&clock_, &a, 3);
        sb = game_clock_park(&clock_, &b, 5);
        frame = game_clock_frames(&clock_);
        CHECK(game_clock_woken_waiting(&clock_, words, 2, frame), "both waiting: no hold");
        retro_atomic_fetch_add_int(&a, 1);
        retro_atomic_fetch_add_int(&b, 1);
        CHECK(!game_clock_woken_waiting(&clock_, words, 2, frame), "both woken: held");
        game_clock_repark(&clock_, sa, 4);
        CHECK(!game_clock_woken_waiting(&clock_, words, 2, frame), "one waiting again: held");
        game_clock_repark(&clock_, sb, 6);
        CHECK(game_clock_woken_waiting(&clock_, words, 2, frame), "both again: let go");
        retro_atomic_fetch_add_int(&a, 1);
        CHECK(!game_clock_woken_waiting(&clock_, words, 1, frame), "woken again: held");
        game_clock_frame(&clock_);
        CHECK(!game_clock_woken_waiting(&clock_, words, 1, frame), "a frame starts: still held");
        game_clock_repark(&clock_, sa, 5);
        CHECK(game_clock_woken_waiting(&clock_, words, 1, frame), "waiting again: let go");
        retro_atomic_fetch_add_int(&a, 1);
        game_clock_frame(&clock_);
        CHECK(game_clock_woken_waiting(&clock_, words, 1, frame), "a whole frame later: let go");
        CHECK(game_clock_woken_waiting(&clock_, words, 0, game_clock_frames(&clock_)),
              "nobody woken: no hold");
        game_clock_unpark(&clock_, sa);
        game_clock_unpark(&clock_, sb);
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
    in_flight_test();
    read_test();

    if (failures) {
        printf("game_clock: %d failed\n", failures);
        return 1;
    }
    printf("game_clock: all passed\n");
    return 0;
}
