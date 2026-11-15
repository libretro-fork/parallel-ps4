/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

/* The sound's whole path, on threads, as the core runs it:
 *   - a game's audio port: 256-frame buffers of a running count into its input of the mix,
 *     written whenever audio_mix_input_ready lets it (no more than a frame queued), parked on
 *     the room otherwise;
 *   - retro_run: mixes exactly one frame (800) of the inputs straight to the frontend, then
 *     counts the frame; about 60 times a second, a few ms early or late each time;
 *   - the frame thread: as each frame is counted, the sources are told there is room.
 * From the first frame with sound on, the frontend gets the count, every sample once, in
 * order, 800 a run: no silence, no gap, nothing twice, nothing held back. That is: no
 * crackle, and no latency added (each run's sound was written in the run before). */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <retro_atomic.h>
#include <retro_timers.h>
#include <rthreads/retro_eventcount.h>
#include <rthreads/rthreads.h>

#include "../audio_mix.h"

#define FRAME AUDIO_MIX_FRAME_FRAMES
#define RUNS 180 /* three seconds */

static audio_mix_shared_t pool; /* the memory the two processes share */
static retro_atomic_int_t ticks, stop;
static retro_eventcount_t tick_ec;
static audio_mix_input_t* port;

/* The count in two 14-bit halves, one a channel, each well inside 16 bits */
static void port_thread(void* data) {
    unsigned count = 0;
    (void)data;
    while (!retro_atomic_load_acquire_int(&stop)) {
        float* dst = NULL;
        unsigned i;
        for (;;) {
            const int key = retro_eventcount_prepare_wait(audio_mix_input_room_ec(port));
            if (audio_mix_input_ready(port, 256, FRAME) || retro_atomic_load_acquire_int(&stop)) {
                retro_eventcount_cancel_wait(audio_mix_input_room_ec(port));
                break;
            }
            retro_eventcount_commit_wait(audio_mix_input_room_ec(port), key);
        }
        if (retro_atomic_load_acquire_int(&stop))
            break;
        if (audio_mix_input_reserve(port, &dst, 256) != 256) {
            /* where the ring wraps: two reserves */
            unsigned first = audio_mix_input_reserve(port, &dst, 256), j;
            for (j = 0; j < first; j++, count++) {
                dst[j * 2 + 0] = (float)(count & 0x3fff);
                dst[j * 2 + 1] = (float)((count >> 14) & 0x3fff);
            }
            audio_mix_input_commit(port, first);
            audio_mix_input_reserve(port, &dst, 256 - first);
            for (i = 0; i < 256 - first; i++, count++) {
                dst[i * 2 + 0] = (float)(count & 0x3fff);
                dst[i * 2 + 1] = (float)((count >> 14) & 0x3fff);
            }
            audio_mix_input_commit(port, 256 - first);
            continue;
        }
        for (i = 0; i < 256; i++, count++) {
            dst[i * 2 + 0] = (float)(count & 0x3fff);
            dst[i * 2 + 1] = (float)((count >> 14) & 0x3fff);
        }
        audio_mix_input_commit(port, 256);
    }
}

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
        audio_mix_notify_room();
    }
}

static int16_t out[FRAME * 2];

int main(void) {
    sthread_t* threads[2];
    unsigned expect = 0, run, first_sound = 0, bad = 0, short_runs = 0;
    int started = 0;

    audio_mix_attach(&pool, 2, 1);
    port = audio_mix_input_open();
    retro_eventcount_init(&tick_ec);
    retro_atomic_int_init(&ticks, 0);
    retro_atomic_int_init(&stop, 0);
    threads[0] = sthread_create(port_thread, NULL);
    threads[1] = sthread_create(frame_thread, NULL);
    retro_sleep_us(2000); /* the game starts: its first frame's sound */

    srand(7);
    for (run = 0; run < RUNS; run++) {
        unsigned ch, i, got;
        /* retro_run: this frame's sound, mixed here, then the frame counted */
        got = audio_mix_mix(FRAME, &ch);
        audio_mix_take_s16(out, 0, FRAME, 2);
        retro_atomic_fetch_add_int(&ticks, 1);
        retro_eventcount_notify(&tick_ec);
        if (got && !started) {
            started = 1;
            first_sound = run;
            expect = (unsigned)((uint16_t)out[0] | ((unsigned)(uint16_t)out[1] << 14));
        }
        if (started && got < FRAME)
            short_runs++;
        for (i = 0; started && i < FRAME; i++) {
            const unsigned v = (unsigned)((uint16_t)out[i * 2] |
                                          ((unsigned)(uint16_t)out[i * 2 + 1] << 14));
            if (v != expect)
                bad++;
            expect = v + 1;
        }
        /* The frontend's next frame: 16.7 ms, a few ms early or late */
        retro_sleep_us(16667 - 4000 + (unsigned)(rand() % 8000));
    }
    retro_atomic_store_release_int(&stop, 1);
    retro_eventcount_notify(&tick_ec);
    audio_mix_notify_room();
    sthread_join(threads[0]);
    sthread_join(threads[1]);

    printf("audio_pipeline: sound from run %u; %u runs short, %u samples out of order\n",
           first_sound, short_runs, bad);
    if (!started || bad || short_runs || first_sound != 0) {
        printf("audio_pipeline: FAILED\n");
        return 1;
    }
    printf("audio_pipeline: all passed\n");
    return 0;
}
