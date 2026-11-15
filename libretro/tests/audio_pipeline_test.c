/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

/* The sound's whole path, on threads, as the core runs it:
 *   - a game's audio port: 256-frame buffers of a running count, written whenever
 *     audio_mix_input_ready lets it (no more than a frame queued), parked on the input's
 *     room otherwise;
 *   - the frame thread: as each frame is counted, exactly one frame (800) of the mix into
 *     the core's ring;
 *   - retro_run: counts a frame, then at once hands the frontend what audio_pace says, about
 *     60 times a second with a few ms of jitter either way;
 * Once primed, what the frontend gets is the count, every sample once, in order: no
 * silence, no gap, nothing twice. That is: no crackle. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <retro_atomic.h>
#include <retro_timers.h>
#include <rthreads/retro_eventcount.h>
#include <rthreads/rthreads.h>

#include "../audio_mix.h"
#include "../audio_pace.h"

#define FRAME AUDIO_MIX_FRAME_FRAMES
#define RUNS 180 /* three seconds */
#define RING 16384u

static int16_t ring[RING * 2];
static retro_atomic_int_t written, read_pos, ticks, stop;
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
            printf("FAIL reserve\n");
            exit(1);
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
        unsigned ch;
        if (retro_atomic_load_acquire_int(&stop)) {
            retro_eventcount_cancel_wait(&tick_ec);
            return;
        }
        if (now == seen) {
            retro_eventcount_commit_wait(&tick_ec, key);
            continue;
        }
        retro_eventcount_cancel_wait(&tick_ec);
        for (; seen != now; seen++) {
            const uint32_t w = (uint32_t)retro_atomic_load_relaxed_int(&written);
            const uint32_t at = w & (RING - 1);
            const uint32_t head = FRAME < RING - at ? FRAME : RING - at;
            audio_mix_mix(FRAME, &ch);
            audio_mix_take_s16(ring + at * 2, 0, head, 2);
            audio_mix_take_s16(ring, head, FRAME - head, 2);
            retro_atomic_store_release_int(&written, (int)(w + FRAME));
        }
    }
}

int main(void) {
    sthread_t* threads[2];
    audio_pace_t pace = {0};
    unsigned expect = 0, run, primed_at = 0, bad = 0, silent = 0;
    int started = 0;

    audio_mix_init_channels(2);
    port = audio_mix_input_open();
    retro_eventcount_init(&tick_ec);
    retro_atomic_int_init(&written, 0);
    retro_atomic_int_init(&read_pos, 0);
    retro_atomic_int_init(&ticks, 0);
    retro_atomic_int_init(&stop, 0);
    threads[0] = sthread_create(port_thread, NULL);
    threads[1] = sthread_create(frame_thread, NULL);

    srand(7);
    for (run = 0; run < RUNS; run++) {
        uint32_t skip, take, r, w, i;
        /* retro_run: the frame counted, then the sound handed over at once */
        retro_atomic_fetch_add_int(&ticks, 1);
        retro_eventcount_notify(&tick_ec);
        r = (uint32_t)retro_atomic_load_relaxed_int(&read_pos);
        w = (uint32_t)retro_atomic_load_acquire_int(&written);
        take = audio_pace_take(&pace, w - r, FRAME, &skip);
        r += skip;
        if (take && !started) {
            started = 1;
            primed_at = run;
            expect = (unsigned)((uint16_t)ring[(r & (RING - 1)) * 2] |
                                ((unsigned)(uint16_t)ring[(r & (RING - 1)) * 2 + 1] << 14));
        }
        if (started && (take < FRAME || skip))
            silent++;
        for (i = 0; i < take; i++, r++) {
            const unsigned got = (unsigned)((uint16_t)ring[(r & (RING - 1)) * 2] |
                                            ((unsigned)(uint16_t)ring[(r & (RING - 1)) * 2 + 1] << 14));
            if (got != expect)
                bad++;
            expect = got + 1;
        }
        retro_atomic_store_release_int(&read_pos, (int)r);
        /* The frontend's next frame: 16.7 ms, a few ms early or late */
        retro_sleep_us(16667 - 4000 + (unsigned)(rand() % 8000));
    }
    retro_atomic_store_release_int(&stop, 1);
    retro_eventcount_notify(&tick_ec);
    retro_eventcount_notify(audio_mix_input_room_ec(port));
    sthread_join(threads[0]);
    sthread_join(threads[1]);

    printf("audio_pipeline: primed at run %u; %u runs short or skipping, %u samples out of order\n",
           primed_at, silent, bad);
    if (!started || bad || silent || primed_at > 3) {
        printf("audio_pipeline: FAILED\n");
        return 1;
    }
    printf("audio_pipeline: all passed\n");
    return 0;
}
