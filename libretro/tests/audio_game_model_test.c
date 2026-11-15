/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

/* A game's sound on the game's time, against the core's frame-locked mix: the
 * real game_clock and audio_mix, with game threads modelled on what games run.
 *   - a sound engine paced by the game's time: a 256-frame buffer every 5.33 ms
 * of it (a sleep to the next deadline on a schedule, or 5.33 ms from now with
 * the other threads), each written through the port (sceAudioOutOutput: parked
 * while more than a frame is queued);
 *   - a render thread: a frame's work, then waiting for the next vblank;
 *   - the frontend: retro_run mixes exactly one frame (800) of the inputs, then
 * the frame is counted; the frame thread then starts the frame's time and tells
 * the port there is room. About 60 times a second, a few ms early or late each
 * time. With nothing else, the time skips through each frame as the threads
 * wait: a frame's sound is written before the next run mixes it. The cases that
 * held the time at the frame's start (a frame of sound short each run: crackle)
 * are the threads that never waited on anything tracked:
 *   - a thread spinning on sched_yield (a lock's spin, a job queue polled):
 * counted idle while it yields (game_clock_yield);
 *   - a thread blocked on a lock held by the sleeping engine: its wait tracked
 *     (game_clock_wait).
 * A frame's sound is what every source has, up to 800: one late by a few is
 * waited for, not padded with silence. Each case: every sample once, in order,
 * none padded, and never more than a buffer and a bit behind the frames. Built
 * with MODEL_UNTRACKED, the yield and the lock as they were: those two cases
 * fall a frame behind every frame. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <retro_atomic.h>
#include <retro_timers.h>
#include <rthreads/retro_eventcount.h>
#include <rthreads/rthreads.h>

#include "../audio_mix.h"
#include "common/game_clock.h"

#define FRAME AUDIO_MIX_FRAME_FRAMES
#define RUNS 120 /* two seconds */
#define BUFFER 256
#define BUFFER_NS 5333333u /* 256 frames at 48 kHz */
/* The most the sound may fall behind the frames: a buffer and a bit (the
 * engine's buffers do not divide a frame), never a frame's worth more */
#define MOST_OWED (BUFFER + FRAME / 4)

static audio_mix_shared_t pool;
static game_clock_t clk;
static retro_atomic_int_t ticks, stop, lock_owner;
static retro_eventcount_t tick_ec, sleep_ec, vblank_ec, lock_ec;
static audio_mix_input_t* port;

/* What the case runs besides the engine and the render thread */
enum { CASE_PLAIN, CASE_YIELDER, CASE_LOCK_WAITER, CASE_HEAVY, CASE_COUNT };
static const char* const case_names[CASE_COUNT] = {
    "engine and render thread", "and a thread spinning on sched_yield",
    "and a thread blocked on the engine's lock", "with a render thread busy 15 ms a frame"};
static int which;

static int stopping(void* ctx) {
    (void)ctx;
    return retro_atomic_load_acquire_int(&stop);
}

static int port_ready(void* ctx) {
    (void)ctx;
    return audio_mix_input_ready(port, BUFFER, FRAME) || retro_atomic_load_acquire_int(&stop);
}

static void write_buffer(unsigned* count) {
    unsigned left = BUFFER;
    while (left) {
        float* dst = NULL;
        const unsigned got = audio_mix_input_reserve(port, &dst, left);
        unsigned i;
        for (i = 0; i < got; i++, (*count)++) {
            dst[i * 2 + 0] = (float)(*count & 0x3fff);
            dst[i * 2 + 1] = (float)((*count >> 14) & 0x3fff);
        }
        audio_mix_input_commit(port, got);
        left -= got;
    }
}

/* A lock the game's threads take: a waiter waits on it as on anything tracked
 */
static int lock_free(void* ctx) {
    (void)ctx;
    return retro_atomic_cas_int(&lock_owner, 0, 1) || retro_atomic_load_acquire_int(&stop);
}

static void lock_take(void) {
#ifdef MODEL_UNTRACKED /* as before: blocked, counted running */
    while (!lock_free(NULL))
        retro_sleep_us(0);
#else
    game_clock_wait(&clk, &lock_ec, 0, lock_free, NULL, 1);
#endif
}

static void lock_give(void) {
    retro_atomic_store_release_int(&lock_owner, 0);
    retro_eventcount_notify(&lock_ec);
}

/* The engine: renders a buffer, sleeps to the next 5.33 ms of the game's time,
 * outputs it. With the lock waiter, it holds its lock while it sleeps. */
static void engine_thread(void* data) {
    unsigned count = 0;
    uint64_t deadline = 0;
    (void)data;
    while (!retro_atomic_load_acquire_int(&stop)) {
        if (which == CASE_LOCK_WAITER)
            lock_take();
        /* Its next buffer's time: on a schedule, or a buffer's time from now (a
         * relative sleep: it gets no more buffers than the time moves on for) */
        deadline = which == CASE_PLAIN || which == CASE_HEAVY ? deadline + BUFFER_NS
                                                              : game_clock_now(&clk) + BUFFER_NS;
        game_clock_wait(&clk, &sleep_ec, deadline, stopping, NULL, 1);
        if (which == CASE_LOCK_WAITER)
            lock_give();
        game_clock_wait(&clk, audio_mix_input_room_ec(port), 0, port_ready, NULL, 1);
        if (retro_atomic_load_acquire_int(&stop))
            break;
        write_buffer(&count);
    }
    game_clock_idle(&clk);
}

static int next_vblank(void* ctx) {
    return game_clock_frames(&clk) != *(uint64_t*)ctx || retro_atomic_load_acquire_int(&stop);
}

static void render_thread(void* data) {
    (void)data;
    while (!retro_atomic_load_acquire_int(&stop)) {
        uint64_t frame = game_clock_frames(&clk);
        volatile unsigned work = 0;
        unsigned i;
        for (i = 0; i < 20000; i++)
            work += i;
        /* A heavy game's frame: the host's time for its work, the game's time held
         * meanwhile */
        if (which == CASE_HEAVY)
            retro_sleep_us(15000);
        game_clock_wait(&clk, &vblank_ec, 0, next_vblank, &frame, 1);
    }
    game_clock_idle(&clk);
}

static void yielder_thread(void* data) {
    int yielded = 0;
    (void)data;
    while (!retro_atomic_load_acquire_int(&stop)) {
#ifndef MODEL_UNTRACKED /* as before: a yield was nothing to the clock */
        game_clock_yield(&clk, &yielded);
#endif
        retro_sleep_us(0);
    }
    game_clock_resume(&clk, &yielded);
    game_clock_idle(&clk);
}

static void lock_waiter_thread(void* data) {
    (void)data;
    while (!retro_atomic_load_acquire_int(&stop)) {
        lock_take();
        lock_give();
    }
    game_clock_idle(&clk);
}

/* The core's frame thread: each counted frame starts the frame's time, then the
 * port has room */
static void frame_thread(void* data) {
    int seen = 0;
    (void)data;
    for (;;) {
        const int key = retro_eventcount_prepare_wait(&tick_ec);
        const int now = retro_atomic_load_acquire_int(&ticks);
        if (retro_atomic_load_acquire_int(&stop)) {
            retro_eventcount_cancel_wait(&tick_ec);
            return;
        }
        if (now == seen) {
            retro_eventcount_commit_wait(&tick_ec, key);
            continue;
        }
        retro_eventcount_cancel_wait(&tick_ec);
        seen = now;
        game_clock_frame(&clk);
        retro_eventcount_notify(&vblank_ec);
        audio_mix_notify_room();
    }
}

static int16_t out[FRAME * 2];

/* @return the runs short of a frame or out of order after the first */
static unsigned run_case(int c) {
    sthread_t* threads[4];
    unsigned n = 0, i, run, expect = 0, bad = 0, first = 0, sent = 0, most_owed = 0;
    int started = 0;

    which = c;
    memset(&pool, 0, sizeof(pool));
    audio_mix_attach(&pool, 2, 1);
    port = audio_mix_input_open();
    game_clock_init(&clk, 60, 1);
    retro_atomic_int_init(&ticks, 0);
    retro_atomic_int_init(&stop, 0);
    retro_atomic_int_init(&lock_owner, 0);
    /* Each game thread counts running from its creation, as the kernel's do */
    game_clock_busy(&clk);
    threads[n++] = sthread_create(engine_thread, NULL);
    game_clock_busy(&clk);
    threads[n++] = sthread_create(render_thread, NULL);
    if (c == CASE_YIELDER) {
        game_clock_busy(&clk);
        threads[n++] = sthread_create(yielder_thread, NULL);
    } else if (c == CASE_LOCK_WAITER) {
        game_clock_busy(&clk);
        threads[n++] = sthread_create(lock_waiter_thread, NULL);
    }
    threads[n++] = sthread_create(frame_thread, NULL);
    retro_sleep_us(2000);

    srand(7);
    for (run = 0; run < RUNS; run++) {
        unsigned ch, got;
        /* retro_run: this frame's sound, mixed here, then the frame counted */
        got = audio_mix_mix(FRAME, &ch);
        audio_mix_take_s16(out, 0, got, 2);
        retro_atomic_fetch_add_int(&ticks, 1);
        retro_eventcount_notify(&tick_ec);
        if (got && !started) {
            started = 1;
            first = run;
            expect = (unsigned)((uint16_t)out[0] | ((unsigned)(uint16_t)out[1] << 14));
        }
        if (got > FRAME)
            bad++;
        if (started) {
            /* How far the sound sent is behind the frames run since it started: the
             * latency the waits for late sound have added */
            const unsigned owed = (run - first + 1) * FRAME - (sent += got);
            if (owed > most_owed)
                most_owed = owed;
            if (got < FRAME && getenv("MODEL_TRACE"))
                printf("  run %u: %u of %u, time %llu frames %llu\n", run, got, FRAME,
                       (unsigned long long)game_clock_now(&clk),
                       (unsigned long long)game_clock_frames(&clk));
        }
        for (i = 0; i < got; i++) {
            const unsigned v =
                (unsigned)((uint16_t)out[i * 2] | ((unsigned)(uint16_t)out[i * 2 + 1] << 14));
            if (v != expect)
                bad++;
            expect = v + 1;
        }
        /* The frontend's next frame: 16.7 ms, a few ms early or late */
        retro_sleep_us(16667 - 4000 + (unsigned)(rand() % 8000));
    }
    retro_atomic_store_release_int(&stop, 1);
    retro_eventcount_notify(&tick_ec);
    retro_eventcount_notify(&sleep_ec);
    retro_eventcount_notify(&vblank_ec);
    retro_eventcount_notify(&lock_ec);
    audio_mix_notify_room();
    for (i = 0; i < n; i++)
        sthread_join(threads[i]);
    audio_mix_input_close(port);

    printf("audio_game_model: %s: sound from run %u, %u samples out of order, at "
           "most %u "
           "behind\n",
           case_names[c], first, bad, most_owed);
    return !started || bad || most_owed > MOST_OWED;
}

int main(void) {
    unsigned failures = 0;
    int c;
    retro_eventcount_init(&tick_ec);
    retro_eventcount_init(&sleep_ec);
    retro_eventcount_init(&vblank_ec);
    retro_eventcount_init(&lock_ec);
    for (c = 0; c < CASE_COUNT; c++)
        failures += run_case(c) ? 1 : 0;
    if (failures) {
        printf("audio_game_model: FAILED\n");
        return 1;
    }
    printf("audio_game_model: all passed\n");
    return 0;
}
