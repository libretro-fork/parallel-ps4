/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

/* audio_wide: segments of stereo and 7.1 written by one side and read by the other, in
 * chunks of every size and width, the reader taking pieces of its own sizes:
 *   - every frame arrives once, in order, with its own channels,
 *   - no piece runs past the ring's end, and every segment starts on a multiple of 8,
 *   - the writer never writes over what the reader has not taken (the samples it reads are
 *     the ones written for them),
 *   - the room the writer is given is used up and given back as the reader takes frames.
 * Once with both sides on one thread in a scripted order, once with a thread each. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <rthreads/rthreads.h>
#include <retro_atomic.h>

#include "../audio_wide.h"

static int failures = 0;

#define CHECK(cond, what)                                                                      \
    do {                                                                                       \
        if (!(cond)) {                                                                         \
            printf("FAIL %s (line %d)\n", what, __LINE__);                                     \
            ++failures;                                                                        \
        }                                                                                      \
    } while (0)

static unsigned char ctl[AUDIO_WIDE_BYTES];
static uint32_t ring[AUDIO_WIDE_RING_SAMPLES];

/* A sample's tag: the frame's number and the channel, so the reader can tell it is the one */
static uint32_t tag(uint32_t frame, uint32_t channel) {
    return frame * 8u + channel;
}

static uint32_t rng = 1;
static uint32_t next(uint32_t n) {
    rng = rng * 1103515245u + 12345u;
    return (rng >> 16) % n;
}

/* The writer's side: frames numbered from 0 */
static audio_wide_writer_t writer;
static uint32_t frames_written;

/* Writes up to @want frames of @channels; returns how many fitted */
static uint32_t write_chunk(uint32_t want, uint32_t channels) {
    audio_wide_piece_t pieces[2];
    unsigned n, p;
    uint32_t i, c;
    const uint32_t frames = audio_wide_room(ctl, &writer, want);
    if (frames == 0)
        return 0;
    audio_wide_plan(&writer, frames, channels, pieces, &n);
    for (p = 0; p < n; p++) {
        CHECK(pieces[p].pos % 8 == 0, "a segment starts on a multiple of 8");
        CHECK(pieces[p].pos + pieces[p].frames * channels <= AUDIO_WIDE_RING_SAMPLES,
              "a piece ends inside the ring");
        for (i = 0; i < pieces[p].frames; i++)
            for (c = 0; c < channels; c++)
                ring[pieces[p].pos + i * channels + c] = tag(frames_written + i, c);
        frames_written += pieces[p].frames;
    }
    audio_wide_publish(ctl, &writer, pieces, n);
    return frames;
}

/* The reader's side */
static uint32_t done, frames_read;
static int bad_order;

/* Takes up to @want frames, in pieces; returns how many */
static uint32_t read_some(uint32_t want) {
    audio_wide_piece_t piece;
    uint32_t got = 0, i, c;
    while (got < want && audio_wide_peek(ctl, done, want - got, &piece)) {
        if (piece.pos + piece.frames * piece.channels > AUDIO_WIDE_RING_SAMPLES ||
            (piece.channels != 2 && piece.channels != 8))
            bad_order = 1;
        for (i = 0; i < piece.frames && !bad_order; i++)
            for (c = 0; c < piece.channels; c++)
                if (ring[piece.pos + i * piece.channels + c] != tag(frames_read + i, c))
                    bad_order = 1;
        frames_read += piece.frames;
        got += piece.frames;
        audio_wide_consume(ctl, &done, piece.frames);
    }
    return got;
}

static void test_scripted(void) {
    unsigned round;
    uint32_t total;
    audio_wide_init(ctl);
    memset(&writer, 0, sizeof(writer));
    frames_written = frames_read = done = 0;
    bad_order = 0;

    /* The room runs out, and comes back as frames are taken */
    for (total = 0; total < 64 && write_chunk(1024, 8); total++) {
    }
    CHECK(frames_written > 15000 && frames_written <= 16384,
          "8-channel frames fill the ring to about its size");
    CHECK(write_chunk(1, 2) == 0, "nothing more fits");
    CHECK(read_some(5000) == 5000, "frames are taken");
    CHECK(write_chunk(1024, 2) == 1024, "room again once frames are taken");

    /* Every size and width, the reader behind and catching up: the writer is three times as
     * likely to go, so the ring is often full */
    for (round = 0; round < 200000 && !bad_order; round++) {
        if (next(4))
            write_chunk(1 + next(1024), next(4) ? 2 : 8);
        else
            read_some(1 + next(3000));
    }
    while (read_some(100000))
        ;
    CHECK(!bad_order, "every frame once, in order, with its channels, inside the ring");
    CHECK(frames_read == frames_written, "everything written is read");
}

/* A thread each: the writer writes a counted number of frames, the reader takes them */
#define THREADED_FRAMES 1000000u
static retro_atomic_int_t writer_done;

static void writer_thread(void* data) {
    (void)data;
    while (frames_written < THREADED_FRAMES) {
        uint32_t want = 1 + next(1024);
        if (want > THREADED_FRAMES - frames_written)
            want = THREADED_FRAMES - frames_written;
        write_chunk(want, next(5) ? 2 : 8);
    }
    retro_atomic_store_release_int(&writer_done, 1);
}

static void test_threaded(void) {
    sthread_t* t;
    unsigned spins = 0;
    audio_wide_init(ctl);
    memset(&writer, 0, sizeof(writer));
    frames_written = frames_read = done = 0;
    bad_order = 0;
    retro_atomic_int_init(&writer_done, 0);
    t = sthread_create(writer_thread, NULL);
    CHECK(t != NULL, "writer thread");
    /* Drained to the end even after a fault, so the writer is never left waiting for room */
    while (frames_read < THREADED_FRAMES && spins < 400000000u) {
        if (read_some(800) == 0 && retro_atomic_load_acquire_int(&writer_done) &&
            read_some(800) == 0)
            break; /* the writer is done and nothing is left */
        spins++;
    }
    sthread_join(t);
    CHECK(!bad_order, "two threads: every frame once, in order, with its channels");
    CHECK(frames_read == THREADED_FRAMES, "two threads: everything written is read");
}

int main(void) {
    setvbuf(stdout, NULL, _IONBF, 0);
    test_scripted();
    test_threaded();
    if (failures) {
        printf("audio_wide: %d failed\n", failures);
        return 1;
    }
    printf("audio_wide: all passed\n");
    return 0;
}
