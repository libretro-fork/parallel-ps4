/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <stdlib.h>
#include <string.h>

#include <formats/raac.h>
#include <formats/rflac.h>
#include <formats/rmp3.h>
#include <formats/rmp4_audio.h>

#include "common/audio_stream.h"

/* Frames decoded and dropped at a time, to seek where a decoder can only
 * go forward from the start */
#define SCRATCH_FRAMES 1024

struct audio_stream {
    enum audio_stream_kind kind;
    const uint8_t* data;
    size_t size;
    unsigned channels;
    unsigned rate;
    uint64_t frames;
    float* scratch; /* SCRATCH_FRAMES frames */

    rmp3* mp3;

    rflac_t* flac;

    raac_t* aac;
    unsigned frame_len; /* frames an access unit decodes to */
    size_t adts_pos;    /* the next ADTS frame */
    size_t au_at;       /* the next access unit in the current one */
    size_t au_end;      /* the current one's end */
    unsigned au_blocks; /* access units left in it */
    unsigned crc;       /* it has CRC words between its access units */
    unsigned au_got;    /* frames the last access unit decoded to */
    unsigned au_used;   /* of those, handed out */

    int16_t* pcm16; /* MP4: the whole track */
    uint64_t at;    /* MP4: the next frame */
};

/* -- MP3 --------------------------------------------------------------- */

static int mp3_open(audio_stream_t* s) {
    s->mp3 = (rmp3*)calloc(1, sizeof(*s->mp3));
    if (!s->mp3 || !rmp3_init_memory(s->mp3, s->data, s->size) || s->mp3->channels == 0 ||
        s->mp3->sampleRate == 0)
        return 0;
    s->channels = s->mp3->channels;
    s->rate = s->mp3->sampleRate;
    /* The tag frame gives the length, delay and padding trimmed (gapless). Without one the
     * stream is run through once to count it, as the whole decode used to, with nothing
     * kept. */
    s->frames = s->mp3->totalFrames;
    if (s->frames == 0) {
        s->frames = rmp3_read_f32(s->mp3, (uint64_t)-1, NULL);
        rmp3_seek_to_frame(s->mp3, 0);
    }
    return s->frames != 0;
}

/* -- FLAC -------------------------------------------------------------- */

/* A decoder on the stream from its first byte, its header parsed. A decoder made afresh, as
 * rflac_reset does too (it frees the old one's state); the header parsed again after
 * rflac_reset took the first frame with it. */
static int flac_start(audio_stream_t* s) {
    if (s->flac)
        rflac_free(s->flac);
    s->flac = rflac_new();
    if (!s->flac)
        return 0;
    rflac_set_in(s->flac, s->data, s->size);
    rflac_set_eof(s->flac);
    while (!rflac_format(s->flac))
        if (rflac_process(s->flac, NULL, NULL) != RFLAC_PROCESS_NEXT)
            return 0;
    return 1;
}

static size_t flac_read(audio_stream_t* s, float* out, size_t frames) {
    size_t got = 0;
    unsigned idle = 0; /* calls that gave nothing with room left: a stream that cannot go on */
    while (got < frames && idle < 4) {
        size_t wrote = 0;
        int result;
        rflac_set_out_f32(s->flac, out + got * s->channels, frames - got);
        result = rflac_process(s->flac, NULL, &wrote);
        got += wrote;
        idle = wrote ? 0 : idle + 1;
        if (result != RFLAC_PROCESS_NEXT)
            break;
    }
    return got;
}

static int flac_open(audio_stream_t* s) {
    const rflac_format_t* format;
    if (!flac_start(s))
        return 0;
    format = rflac_format(s->flac);
    s->channels = format->channels;
    s->rate = format->sample_rate;
    if (s->channels == 0 || s->rate == 0)
        return 0;
    s->scratch = (float*)malloc(sizeof(float) * SCRATCH_FRAMES * s->channels);
    if (!s->scratch)
        return 0;
    /* The header usually gives the length; else the stream is run through once to count it */
    s->frames = rflac_total_frames(s->flac);
    if (s->frames == 0) {
        size_t got;
        while ((got = flac_read(s, s->scratch, SCRATCH_FRAMES)) != 0)
            s->frames += got;
        if (!flac_start(s))
            return 0;
    }
    return s->frames != 0;
}

/* -- ADTS AAC ---------------------------------------------------------- */

/* The ADTS frame at adts_pos made current. 0 at the end. */
static int adts_next_frame(audio_stream_t* s) {
    raac_adts_t header;
    if (s->adts_pos + 7 > s->size ||
        raac_adts_parse(s->data + s->adts_pos, s->size - s->adts_pos, &header) != 0 ||
        header.frame_size > s->size - s->adts_pos || header.frame_size < header.header_size)
        return 0;
    s->au_at = s->adts_pos + header.header_size;
    s->au_end = s->adts_pos + header.frame_size;
    s->au_blocks = header.raw_blocks;
    s->crc = header.header_size == 9;
    s->adts_pos = s->au_end;
    return 1;
}

/* The next access unit decoded, held in the decoder. 0 at the end. */
static int adts_decode(audio_stream_t* s) {
    for (;;) {
        int got;
        size_t used;
        if (s->au_blocks == 0 || s->au_at >= s->au_end) {
            if (!adts_next_frame(s))
                return 0;
            continue;
        }
        got = raac_decode(s->aac, s->data + s->au_at, s->au_end - s->au_at);
        used = raac_last_size(s->aac);
        if (got < 0 || used == 0) {
            s->au_blocks = 0; /* the rest of this frame is lost; the next one is tried */
            continue;
        }
        --s->au_blocks;
        /* Each raw_data_block of a frame is an access unit, CRC words between them */
        s->au_at += used + (s->crc && s->au_blocks ? 2 : 0);
        s->au_got = (unsigned)got;
        s->au_used = 0;
        return 1;
    }
}

static size_t adts_read(audio_stream_t* s, float* out, size_t frames) {
    size_t got = 0;
    while (got < frames) {
        size_t n;
        if (s->au_used == s->au_got && !adts_decode(s))
            break;
        n = s->au_got - s->au_used;
        if (n > frames - got)
            n = frames - got;
        raac_copy_f32(s->aac, s->au_used, n, out + got * s->channels);
        s->au_used += (unsigned)n;
        got += n;
    }
    return got;
}

static void adts_rewind(audio_stream_t* s) {
    raac_reset(s->aac);
    s->adts_pos = 0;
    s->au_blocks = 0;
    s->au_at = 0;
    s->au_end = 0;
    s->au_got = 0;
    s->au_used = 0;
}

static int adts_open(audio_stream_t* s) {
    raac_adts_t header;
    if (raac_adts_parse(s->data, s->size, &header) != 0)
        return 0;
    s->aac = raac_open(header.asc, sizeof(header.asc));
    if (!s->aac)
        return 0;
    s->channels = raac_channels(s->aac);
    s->rate = raac_sample_rate(s->aac);
    s->frame_len = raac_frame_len(s->aac);
    if (s->channels == 0 || s->rate == 0 || s->frame_len == 0)
        return 0;
    s->scratch = (float*)malloc(sizeof(float) * SCRATCH_FRAMES * s->channels);
    if (!s->scratch)
        return 0;
    /* The length from the frames' headers, nothing decoded */
    while (adts_next_frame(s))
        s->frames += (uint64_t)s->au_blocks * s->frame_len;
    adts_rewind(s);
    return s->frames != 0;
}

/* -- MP4 --------------------------------------------------------------- */

static int mp4_open(audio_stream_t* s) {
    size_t frames = 0;
    unsigned rate = 0;
    unsigned chans = 0;
    if (!rmp4_audio_decode(s->data, s->size, 0, &s->pcm16, &frames, &rate, &chans))
        return 0;
    s->frames = frames;
    s->rate = rate;
    s->channels = chans;
    return s->pcm16 && frames && rate && chans;
}

static size_t mp4_read(audio_stream_t* s, float* out, size_t frames) {
    size_t i, n;
    const int16_t* src;
    if (s->at >= s->frames)
        return 0;
    if (frames > s->frames - s->at)
        frames = (size_t)(s->frames - s->at);
    n = frames * s->channels;
    src = s->pcm16 + s->at * s->channels;
    for (i = 0; i < n; i++)
        out[i] = (float)src[i] * (1.0f / 32768.0f);
    s->at += frames;
    return frames;
}

/* -- The stream --------------------------------------------------------- */

audio_stream_t* audio_stream_open(const uint8_t* data, size_t size, enum audio_stream_kind kind) {
    int ok = 0;
    audio_stream_t* s;
    if (!data || size == 0)
        return NULL;
    s = (audio_stream_t*)calloc(1, sizeof(*s));
    if (!s)
        return NULL;
    s->kind = kind;
    s->data = data;
    s->size = size;
    switch (kind) {
    case AUDIO_STREAM_MP3:
        ok = mp3_open(s);
        break;
    case AUDIO_STREAM_FLAC:
        ok = flac_open(s);
        break;
    case AUDIO_STREAM_ADTS:
        ok = adts_open(s);
        break;
    case AUDIO_STREAM_MP4:
        ok = mp4_open(s);
        break;
    }
    if (!ok) {
        audio_stream_close(s);
        return NULL;
    }
    return s;
}

void audio_stream_close(audio_stream_t* s) {
    if (!s)
        return;
    if (s->mp3) {
        rmp3_uninit(s->mp3);
        free(s->mp3);
    }
    if (s->flac)
        rflac_free(s->flac);
    if (s->aac)
        raac_close(s->aac);
    free(s->pcm16);
    free(s->scratch);
    free(s);
}

unsigned audio_stream_channels(const audio_stream_t* s) {
    return s->channels;
}

unsigned audio_stream_rate(const audio_stream_t* s) {
    return s->rate;
}

uint64_t audio_stream_frames(const audio_stream_t* s) {
    return s->frames;
}

size_t audio_stream_read(audio_stream_t* s, float* out, size_t frames) {
    switch (s->kind) {
    case AUDIO_STREAM_MP3:
        return (size_t)rmp3_read_f32(s->mp3, frames, out);
    case AUDIO_STREAM_FLAC:
        return flac_read(s, out, frames);
    case AUDIO_STREAM_ADTS:
        return adts_read(s, out, frames);
    case AUDIO_STREAM_MP4:
        return mp4_read(s, out, frames);
    }
    return 0;
}

/* @frames decoded and dropped */
static void skip(audio_stream_t* s, uint64_t frames) {
    while (frames > 0) {
        const size_t want = frames < SCRATCH_FRAMES ? (size_t)frames : SCRATCH_FRAMES;
        const size_t got = audio_stream_read(s, s->scratch, want);
        if (got == 0)
            break;
        frames -= got;
    }
}

int audio_stream_seek(audio_stream_t* s, uint64_t frame) {
    if (frame > s->frames)
        frame = s->frames;
    switch (s->kind) {
    case AUDIO_STREAM_MP3:
        /* rmp3 decodes forward from the start: the bit reservoir reaches back */
        rmp3_seek_to_frame(s->mp3, frame);
        return 1;
    case AUDIO_STREAM_FLAC:
        /* The seek table's offset comes without the frame it is at, so forward from the
         * start too */
        if (!flac_start(s))
            return 0;
        skip(s, frame);
        return 1;
    case AUDIO_STREAM_ADTS: {
        /* Whole ADTS frames are passed over by their headers, up to one access unit
         * before the frame: that one is decoded, to fill the decoder's overlap */
        uint64_t at = 0;
        adts_rewind(s);
        for (;;) {
            const size_t pos = s->adts_pos;
            uint64_t len;
            if (!adts_next_frame(s))
                break;
            len = (uint64_t)s->au_blocks * s->frame_len;
            if (at + len + s->frame_len > frame) {
                s->adts_pos = pos;
                s->au_blocks = 0;
                break;
            }
            at += len;
        }
        s->au_blocks = 0;
        skip(s, frame - at);
        return 1;
    }
    case AUDIO_STREAM_MP4:
        s->at = frame;
        return 1;
    }
    return 0;
}
