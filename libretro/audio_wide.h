/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

/* Surround sound on its way from the emulator to the core, for a frontend that takes more
 * than two channels (RETRO_ENVIRONMENT_GET_AUDIO_SAMPLE_BATCH_MULTI).
 *
 * The emulator mixes a chunk at a time, each chunk stereo or 7.1 depending on what played in
 * it, into a ring of samples (float or 16-bit, kept by the caller in the shared memory). Each
 * chunk is a segment: where it starts in the ring, how many frames, how many channels. The
 * core hands the frontend each segment where it lies, with the layout it has; nothing is
 * copied or packed again. A segment never runs past the ring's end (one that would is split
 * in two, the second from the ring's start), and every segment starts on a multiple of 8
 * samples, so a frame of either width never straddles it.
 *
 * This is the control block: the segment queue and the read position, in shared memory, one
 * writer (the emulator) and one reader (the core), handed over with acquire and release, no
 * lock. It is opaque to C++: the shared memory keeps AUDIO_WIDE_BYTES for it. */

#ifndef SHADPS4_LIBRETRO_AUDIO_WIDE_H
#define SHADPS4_LIBRETRO_AUDIO_WIDE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Samples the ring holds: 16384 frames of 7.1. A power of two. */
#define AUDIO_WIDE_RING_SAMPLES 131072u
/* Segments queued at once at most */
#define AUDIO_WIDE_SEGMENTS 1024u
/* What the shared memory keeps for the control block */
#define AUDIO_WIDE_BYTES 32768u

/* Where a piece of sound is in the ring: @pos samples from its start, @frames frames of
 * @channels */
typedef struct audio_wide_piece
{
   uint32_t pos;
   uint32_t frames;
   uint32_t channels;
} audio_wide_piece_t;

/* The writer's own position, kept in its process */
typedef struct audio_wide_writer
{
   uint32_t sample; /* where the next segment starts: counts samples, only grows */
} audio_wide_writer_t;

/* Clears the control block, before either side uses it */
void audio_wide_init(void* ctl);

/* The writer. How many frames of 8 channels fit now, at most @frames (also kept to what the
 * segment queue has room for). Then, for the @frames of @channels it has mixed, the pieces of
 * ring to write them into, *@n of them (1, or 2 where the ring wraps), and once they are
 * written, publish. */
uint32_t audio_wide_room(const void* ctl, const audio_wide_writer_t* w, uint32_t frames);
void audio_wide_plan(const audio_wide_writer_t* w, uint32_t frames, uint32_t channels,
                     audio_wide_piece_t pieces[2], unsigned* n);
void audio_wide_publish(void* ctl, audio_wide_writer_t* w, const audio_wide_piece_t pieces[2],
                        unsigned n);

/* The reader. The next piece to hand the frontend, at most @frames: the part of the oldest
 * segment not yet taken. Returns 0 when nothing is queued. Then consume what the frontend
 * took of it: a segment done with goes back to the writer. @done is the reader's own count
 * of the oldest segment's frames already taken, kept in its process, 0 to start. */
int audio_wide_peek(const void* ctl, uint32_t done, uint32_t frames, audio_wide_piece_t* piece);
void audio_wide_consume(void* ctl, uint32_t* done, uint32_t frames);

#ifdef __cplusplus
}
#endif

#endif
