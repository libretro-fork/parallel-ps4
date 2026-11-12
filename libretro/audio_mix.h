/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

/* The sound the emulator hands the frontend: every source (an audio port, the trophy sound,
 * the music player) writes stereo at 48 kHz into an input of its own, as floats counted in
 * 16-bit steps (32767 is full scale), and one thread sums the inputs and rounds the sum to the
 * 16 bits of the ring the core plays from: one rounding, at the end. The console's audio is 48 kHz, so
 * nothing is resampled here; the frontend resamples to its device.
 *
 * Inputs come from a pool made once, at start: opening and closing one allocates nothing.
 * Each is a ring with one producer (its source) and one consumer (the mixing thread), handed
 * over without a lock. A source waits on an eventcount for room, which the mixing thread
 * signals as it takes frames. */

#ifndef SHADPS4_LIBRETRO_AUDIO_MIX_H
#define SHADPS4_LIBRETRO_AUDIO_MIX_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Stereo frames an input holds. Room for the most a port writes at once (2048
 * frames) on top of what a port may keep queued (AUDIO_MIX_QUEUE_FRAMES). */
#define AUDIO_MIX_INPUT_FRAMES 4096
/* What a source keeps queued at most before it waits for the frontend: 32 ms. */
#define AUDIO_MIX_QUEUE_FRAMES 1536
/* Inputs open at once at most. */
#define AUDIO_MIX_INPUTS 32

typedef struct audio_mix_input audio_mix_input_t;

/* Makes the pool. Once, before any input is opened. Returns 0 when there is no memory. */
int audio_mix_init(void);

/* An input for a source, or NULL when all are open. */
audio_mix_input_t* audio_mix_input_open(void);

/* The source is done with the input: the mixing thread drops what it still holds and hands
 * the input back to the pool. The source does not touch it again. */
void audio_mix_input_close(audio_mix_input_t* in);

/* Waits until no more than AUDIO_MIX_QUEUE_FRAMES are queued and @frames more fit, or
 * @timeout_us passes. Returns nonzero when they fit. */
int audio_mix_input_wait(audio_mix_input_t* in, unsigned frames, int64_t timeout_us);

/* Room to write into, in place: up to @frames contiguous frames at *@ptr. Returns how many;
 * where the ring wraps it is fewer, and a second call gives the rest. */
unsigned audio_mix_input_reserve(audio_mix_input_t* in, float** ptr, unsigned frames);

/* Hands @frames written at the last reserve to the mixing thread. */
void audio_mix_input_commit(audio_mix_input_t* in, unsigned frames);

/* @wake(@ctx) is called after every commit and every clip started, from the source's thread:
 * the mixing thread parks on it when it has nothing to mix. Set once, before any source runs. */
void audio_mix_set_wake(void (*wake)(void*), void* ctx);

/* Mixing thread only: sums up to @frames of every open input, rounded to the nearest 16-bit
 * step and clamped, into @ring (16-bit stereo,
 * @ring_mask + 1 frames, a power of two) from frame index @at. An input with fewer frames
 * than another adds silence for the difference. Returns the frames written. */
unsigned audio_mix_pump(int16_t* ring, uint32_t ring_mask, uint32_t at, unsigned frames);

/* A sound played once over everything else, the trophy sound: @frames of stereo at 48 kHz in
 * the inputs' format, borrowed and read by the mixing thread as it plays them, so they stay untouched
 * for the life of the process. Playing it again starts it over. One caller thread. */
void audio_mix_play_clip(const float* frames, unsigned count, float gain);

/* Resampling of sound that is not the console's own (a music file, the trophy sound) to the
 * mix's 48 kHz, with a 16-tap windowed sinc. Reads @src (16-bit, 1 or 2 channels,
 * @src_frames long) in place from *@pos, a 32.32 fixed-point frame position it advances,
 * and writes up to @out_frames of stereo in the inputs' format to @out with @gain applied. Returns the
 * frames written: fewer once the source runs out. */
typedef struct audio_mix_resampler
{
   uint64_t step;      /* source frames per output frame, 32.32 */
   float cutoff;       /* of the sinc, relative to the source's Nyquist */
   unsigned channels;
} audio_mix_resampler_t;

void audio_mix_resampler_init(audio_mix_resampler_t* r, unsigned src_hz, unsigned channels);
unsigned audio_mix_resample(const audio_mix_resampler_t* r, const int16_t* src,
                            uint64_t src_frames, uint64_t* pos, float* out,
                            unsigned out_frames, float gain);
/* The same from a float source, -1..1: a decoder's own output, not rounded to 16 bits first */
unsigned audio_mix_resample_f32(const audio_mix_resampler_t* r, const float* src,
                                uint64_t src_frames, uint64_t* pos, float* out,
                                unsigned out_frames, float gain);

/* Conversion of what a source has into the input's format, with a gain applied
 * (1.0 = unchanged). @channels is 1, 2 or 8; eight are folded to stereo the way the console
 * does for stereo output. */
void audio_mix_convert_s16(float* dst, const int16_t* src, unsigned frames, unsigned channels,
                           float gain);
void audio_mix_convert_f32(float* dst, const float* src, unsigned frames, unsigned channels,
                           float gain);

#ifdef __cplusplus
}
#endif

#endif
