/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef SHADPS4_COMMON_AUDIO_STREAM_H
#define SHADPS4_COMMON_AUDIO_STREAM_H

/* An audio file held in memory (an MP3, FLAC, ADTS AAC or MP4/M4A),
 * decoded a block at a time, as it is played, with libretro-common's
 * decoders: for sound that is not the console's own, the music player.
 * Samples come out as floats, -1 to 1, interleaved, decoded straight to
 * float: nothing is rounded to 16 bits on the way. An MP4 is played from
 * its first AAC track, its packets demuxed by rmp4 as they are needed.
 *
 * The file is read where it lies and must outlive the stream. Opening
 * allocates the decoder's state and reading allocates nothing, nor does
 * seeking, with one exception: a seek back in a FLAC with no seek table
 * makes rflac's state again, its only way back to the start (rflac_reset
 * frees it too). A FLAC's seek table is used where it has one; else a seek
 * forward in an MP3 or a FLAC decodes on from where the stream is, and
 * back, from the start. */

#include <stddef.h>
#include <stdint.h>

#include <retro_common_api.h>

RETRO_BEGIN_DECLS

enum audio_stream_kind {
    AUDIO_STREAM_MP3 = 0,
    AUDIO_STREAM_FLAC,
    AUDIO_STREAM_ADTS,
    AUDIO_STREAM_MP4
};

typedef struct audio_stream audio_stream_t;

/* NULL when @data is not a stream of @kind, or there is no memory */
audio_stream_t* audio_stream_open(const uint8_t* data, size_t size, enum audio_stream_kind kind);
void audio_stream_close(audio_stream_t* s);

unsigned audio_stream_channels(const audio_stream_t* s);
unsigned audio_stream_rate(const audio_stream_t* s);
/* Frames in the whole track */
uint64_t audio_stream_frames(const audio_stream_t* s);

/* Up to @frames frames into @out (channels floats a frame); fewer only
 * at the end of the track */
size_t audio_stream_read(audio_stream_t* s, float* out, size_t frames);

/* The next read starts at frame @frame (the end, past it). 0 when the
 * stream cannot be decoded again from its start. */
int audio_stream_seek(audio_stream_t* s, uint64_t frame);

RETRO_END_DECLS

#endif
