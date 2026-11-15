/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

/* A game's sound on the game's time, against the core's frame-locked mix: the real game_clock
 * and audio_mix, with game threads modelled on what games run, and the frontend and the frame
 * thread as the core and libretro/host.cpp run them.
 *   - a sound engine paced by the game's time: renders a 256-frame buffer, writes it through
 *     the port (sceAudioOutOutput: parked while more than a frame is queued), then sleeps to
 *     the next buffer's time, 5.33 ms on: on a schedule, or 5.33 ms from now;
 *   - a render thread: a frame's work, then waiting for the next vblank;
 *   - the frontend, about 60 times a second, a few ms early or late each time: retro_run
 *     waits for the frame's sound (every input written to since the last frame has a frame of
 *     it, or the game's frame is done), asking the frame thread to finish the frame while it
 *     is not in; mixes exactly 800; counts the frame;
 *   - the frame thread: starts each counted frame's time, tells the port there is room, and
 *     finishes a frame retro_run waits for (game_clock_finish).
 * The cases: the engine and the render thread; a relative sleeper with a thread spinning on
 * sched_yield (counted idle while it yields), and with a thread blocked on the engine's lock
 * (a tracked wait); a render thread busy 15 ms of the host's time a frame; a thread that spins
 * and never yields (the frame finished all the same); an engine that
 * stops after a few buffers (the frame done without them).
 * Each case: from the first frame with sound, exactly 800 samples a run, every one the game's,
 * once and in order: nothing padded, nothing late, nothing dropped, and no latency but the
 * frame's own. The stopped engine: its sound to the end (its last, less than a frame, held back
 * whole while it was still writing, then played), then silence, and no wait that does not end.
 * FMOD's: no slot output before its mixer filled it (built with MODEL_UNPACED, Output as fast
 * as the queue has room, as before: it fails). */

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
#define STOP_AFTER 7       /* the stopping engine's buffers */

static audio_mix_shared_t pool;
static game_clock_t clk;
static retro_atomic_int_t ticks, wanted, frame_done, stop, lock_owner, spin_word;
static retro_eventcount_t tick_ec, sleep_ec, vblank_ec, lock_ec, audio_ec, never_ec;
static audio_mix_input_t* port;

enum {
    CASE_PLAIN,
    CASE_YIELDER,
    CASE_LOCK_WAITER,
    CASE_HEAVY,
    CASE_SPINNER,
    CASE_STOPS,
    CASE_FMOD,
    CASE_COUNT
};
static const char* const case_names[CASE_COUNT] = {
    "engine and render thread",
    "relative sleeper, a thread spinning on sched_yield",
    "relative sleeper, a thread blocked on the engine's lock",
    "a render thread busy 15 ms a frame",
    "a thread spinning without a yield",
    "an engine that stops",
    "FMOD: a mixer two 256-frame slots ahead, an output thread without a wait for it"};
static int which;

static int stopping(void* ctx) {
    (void)ctx;
    return retro_atomic_load_acquire_int(&stop);
}

static int port_ready(void* ctx) {
    (void)ctx;
    return audio_mix_input_ready(port, BUFFER, FRAME) || retro_atomic_load_acquire_int(&stop);
}

/* The count in two 14-bit halves, one a channel, each well inside 16 bits */
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

/* FMOD's engine: its mixer fills 256-frame slots of a 512-frame ring, each when its output
 * thread has taken the one before, and works the host's time at it (1 ms). The output thread
 * outputs the next slot, sceAudioOutOutput paced as the console paces it, then gives the slot
 * back to the mixer: it never waits for the mixer, relying on the call's pace to leave the
 * mixer the time to fill it. Output returning faster (as before: as soon as the queue had room)
 * outputs slots the mixer has not filled again yet: the same sound twice, a click. */
static float fmod_ring[2][BUFFER * 2];
static retro_atomic_int_t fmod_free; /* slots the mixer may fill */
static retro_eventcount_t fmod_ec;

static int fmod_slot_free(void* ctx) {
    (void)ctx;
    for (;;) {
        const int n = retro_atomic_load_acquire_int(&fmod_free);
        if (n == 0)
            return retro_atomic_load_acquire_int(&stop);
        if (retro_atomic_cas_int(&fmod_free, n, n - 1))
            return 1;
    }
}

static void fmod_mixer_thread(void* data) {
    unsigned count = 0, slot = 0, i;
    (void)data;
    while (!retro_atomic_load_acquire_int(&stop)) {
        game_clock_wait(&clk, &fmod_ec, 0, fmod_slot_free, NULL, 1);
        if (retro_atomic_load_acquire_int(&stop))
            break;
        retro_sleep_us(1000); /* its mixing: the host's time, counted running */
        for (i = 0; i < BUFFER; i++, count++) {
            fmod_ring[slot][i * 2 + 0] = (float)(count & 0x3fff);
            fmod_ring[slot][i * 2 + 1] = (float)((count >> 14) & 0x3fff);
        }
        slot ^= 1;
    }
    game_clock_idle(&clk);
}

static void fmod_output_thread(void* data) {
    uint64_t start = 0, count = 0;
    unsigned slot = 0;
    (void)data;
    /* Both slots mixed before it starts */
    while (retro_atomic_load_acquire_int(&fmod_free) != 0 && !retro_atomic_load_acquire_int(&stop))
        retro_sleep_us(100);
    retro_sleep_us(1500);
    while (!retro_atomic_load_acquire_int(&stop)) {
        unsigned left = BUFFER;
        const float* src = fmod_ring[slot];
#ifndef MODEL_UNPACED
        /* sceAudioOutOutput's pace (audioout.cpp, PaceOutput) */
        const uint64_t due = game_clock_cadence(&clk, &start, &count, BUFFER, 48000);
        if (due > game_clock_now(&clk))
            game_clock_wait(&clk, &sleep_ec, due, stopping, NULL, 1);
#else
        (void)start;
        (void)count;
#endif
        game_clock_wait(&clk, audio_mix_input_room_ec(port), 0, port_ready, NULL, 1);
        if (retro_atomic_load_acquire_int(&stop))
            break;
        while (left) {
            float* dst = NULL;
            const unsigned got = audio_mix_input_reserve(port, &dst, left);
            memcpy(dst, src, got * 2 * sizeof(float));
            src += got * 2;
            audio_mix_input_commit(port, got);
            left -= got;
        }
        slot ^= 1;
        retro_atomic_fetch_add_int(&fmod_free, 1);
        retro_eventcount_notify(&fmod_ec);
    }
    game_clock_idle(&clk);
}

static int lock_free(void* ctx) {
    (void)ctx;
    return retro_atomic_cas_int(&lock_owner, 0, 1) || retro_atomic_load_acquire_int(&stop);
}

static void lock_take(void) {
    game_clock_wait(&clk, &lock_ec, 0, lock_free, NULL, 1);
}

static void lock_give(void) {
    retro_atomic_store_release_int(&lock_owner, 0);
    retro_eventcount_notify(&lock_ec);
}

static void engine_thread(void* data) {
    unsigned count = 0, buffers = 0;
    uint64_t deadline = 0;
    const int relative = which == CASE_YIELDER || which == CASE_LOCK_WAITER;
    (void)data;
    while (!retro_atomic_load_acquire_int(&stop)) {
        /* Rendered, then out through the port */
        game_clock_wait(&clk, audio_mix_input_room_ec(port), 0, port_ready, NULL, 1);
        if (retro_atomic_load_acquire_int(&stop))
            break;
        write_buffer(&count);
        if (which == CASE_STOPS && ++buffers == STOP_AFTER) {
            /* Stops: waits for something that does not come */
            game_clock_wait(&clk, &never_ec, 0, stopping, NULL, 1);
            break;
        }
        /* With the lock waiter, the lock held while it sleeps */
        if (which == CASE_LOCK_WAITER)
            lock_take();
        deadline = relative ? game_clock_now(&clk) + BUFFER_NS : deadline + BUFFER_NS;
        game_clock_wait(&clk, &sleep_ec, deadline, stopping, NULL, 1);
        if (which == CASE_LOCK_WAITER)
            lock_give();
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
        /* A heavy game's frame: the host's time for its work, the game's time held */
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
        game_clock_yield(&clk, &yielded);
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

/* Spins on memory, no system call, no yield: counted running all along */
static void spinner_thread(void* data) {
    (void)data;
    while (!retro_atomic_load_acquire_int(&stop))
        retro_atomic_fetch_add_int(&spin_word, 1);
    game_clock_idle(&clk);
}

/* libretro/host.cpp's FrameThread */
static void frame_thread(void* data) {
    int seen = 0, finished = 0;
    (void)data;
    for (;;) {
        const int key = retro_eventcount_prepare_wait(&tick_ec);
        const int now = retro_atomic_load_acquire_int(&ticks);
        const int want = retro_atomic_load_acquire_int(&wanted);
        if (retro_atomic_load_acquire_int(&stop)) {
            retro_eventcount_cancel_wait(&tick_ec);
            return;
        }
        if (now == seen && (want != seen || finished == seen)) {
            retro_eventcount_commit_wait(&tick_ec, key);
            continue;
        }
        retro_eventcount_cancel_wait(&tick_ec);
        if (now != seen) {
            for (; seen != now; seen++)
                game_clock_frame(&clk);
            retro_eventcount_notify(&vblank_ec);
            audio_mix_notify_room();
        }
        if (want == seen && finished != seen) {
            finished = seen;
            game_clock_finish(&clk);
        }
    }
}

static void notify_audio(void* ctx) {
    (void)ctx;
    retro_eventcount_notify(&audio_ec);
}

/* libretro_core.cpp's WaitFrameSound */
static void wait_frame_sound(void) {
    int asked = 0;
    for (;;) {
        const int key = retro_eventcount_prepare_wait(&audio_ec);
        if (audio_mix_frame_ready(FRAME) ||
            retro_atomic_load_acquire_int(&frame_done) == retro_atomic_load_relaxed_int(&ticks)) {
            retro_eventcount_cancel_wait(&audio_ec);
            return;
        }
        if (!asked) {
            asked = 1;
            retro_atomic_store_release_int(&wanted, retro_atomic_load_relaxed_int(&ticks));
            retro_eventcount_notify(&tick_ec);
        }
        retro_eventcount_commit_wait(&audio_ec, key);
    }
}

static int16_t out[FRAME * 2];

/* @return nonzero if the case failed */
static int run_case(int c) {
    sthread_t* threads[5];
    unsigned n = 0, i, run, expect = 0, bad = 0, padded = 0, first = 0, sent = 0;
    int started = 0;

    which = c;
    memset(&pool, 0, sizeof(pool));
    audio_mix_attach(&pool, 2, 1);
    audio_mix_set_written(notify_audio, NULL);
    port = audio_mix_input_open();
    game_clock_init(&clk, 60, 1);
    retro_atomic_int_init(&frame_done, 0);
    game_clock_set_done(&clk, &frame_done, notify_audio, NULL);
    retro_atomic_int_init(&ticks, 0);
    retro_atomic_int_init(&wanted, 0);
    retro_atomic_int_init(&stop, 0);
    retro_atomic_int_init(&lock_owner, 0);
    /* Each game thread counts running from its creation, as the kernel's do */
    if (c == CASE_FMOD) {
        retro_atomic_int_init(&fmod_free, 2);
        game_clock_busy(&clk);
        threads[n++] = sthread_create(fmod_mixer_thread, NULL);
        game_clock_busy(&clk);
        threads[n++] = sthread_create(fmod_output_thread, NULL);
    } else {
        game_clock_busy(&clk);
        threads[n++] = sthread_create(engine_thread, NULL);
    }
    game_clock_busy(&clk);
    threads[n++] = sthread_create(render_thread, NULL);
    if (c == CASE_YIELDER || c == CASE_LOCK_WAITER || c == CASE_SPINNER) {
        game_clock_busy(&clk);
        threads[n++] = sthread_create(c == CASE_YIELDER       ? yielder_thread
                                      : c == CASE_LOCK_WAITER ? lock_waiter_thread
                                                              : spinner_thread,
                                      NULL);
    }
    threads[n++] = sthread_create(frame_thread, NULL);
    retro_sleep_us(2000);

    srand(7);
    for (run = 0; run < RUNS; run++) {
        unsigned ch, got = 0;
        /* retro_run: the frame's sound waited for, exactly a frame of it mixed (none before
         * the game's time has started), then the frame counted */
        if (run != 0) {
            wait_frame_sound();
            got = audio_mix_mix(FRAME, &ch);
            audio_mix_take_s16(out, 0, FRAME, 2);
        }
        retro_atomic_fetch_add_int(&ticks, 1);
        retro_eventcount_notify(&tick_ec);
        if (got && !started) {
            started = 1;
            first = run;
            expect = (unsigned)((uint16_t)out[0] | ((unsigned)(uint16_t)out[1] << 14));
        }
        if (started) {
            /* The stopped engine: what it wrote, then silence */
            const unsigned left = c == CASE_STOPS ? STOP_AFTER * BUFFER - sent : FRAME;
            const unsigned want = left < FRAME ? left : FRAME;
            /* Its last, less than a frame, held back whole while it was still writing */
            if (got != want && !(want < FRAME && got == 0))
                padded++;
            for (i = 0; i < FRAME; i++) {
                const unsigned v =
                    (unsigned)((uint16_t)out[i * 2] | ((unsigned)(uint16_t)out[i * 2 + 1] << 14));
                if (i < got) {
                    if (v != expect)
                        bad++;
                    expect = v + 1;
                } else if (v != 0)
                    bad++;
            }
            sent += got;
            if (got != want && getenv("MODEL_TRACE"))
                printf("  run %u: %u of %u, time %llu frames %llu\n", run, got, want,
                       (unsigned long long)game_clock_now(&clk),
                       (unsigned long long)game_clock_frames(&clk));
        }
        /* The frontend's next frame: 16.7 ms, a few ms early or late */
        retro_sleep_us(16667 - 4000 + (unsigned)(rand() % 8000));
    }
    retro_atomic_store_release_int(&stop, 1);
    retro_eventcount_notify(&tick_ec);
    retro_eventcount_notify(&sleep_ec);
    retro_eventcount_notify(&vblank_ec);
    retro_eventcount_notify(&lock_ec);
    retro_eventcount_notify(&never_ec);
    retro_eventcount_notify(&fmod_ec);
    audio_mix_notify_room();
    for (i = 0; i < n; i++)
        sthread_join(threads[i]);
    audio_mix_set_written(NULL, NULL);
    audio_mix_input_close(port);

    printf("audio_game_model: %s: sound from run %u, %u runs not a whole frame, %u samples "
           "wrong\n",
           case_names[c], first, padded, bad);
    if (c == CASE_STOPS && sent != STOP_AFTER * BUFFER)
        padded++;
    return !started || first != 1 || padded || bad;
}

int main(void) {
    unsigned failures = 0;
    int c;
    retro_eventcount_init(&tick_ec);
    retro_eventcount_init(&sleep_ec);
    retro_eventcount_init(&vblank_ec);
    retro_eventcount_init(&lock_ec);
    retro_eventcount_init(&audio_ec);
    retro_eventcount_init(&never_ec);
    retro_eventcount_init(&fmod_ec);
    retro_atomic_int_init(&spin_word, 0);
    for (c = 0; c < CASE_COUNT; c++)
        failures += run_case(c) ? 1 : 0;
    if (failures) {
        printf("audio_game_model: FAILED\n");
        return 1;
    }
    printf("audio_game_model: all passed\n");
    return 0;
}
