/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

/* audio_stream (src/common/audio_stream.c), the music player's block
 * decoder, on files given on the command line (run.sh makes them with
 * ffmpeg, a sine in each format; skipped when there are none):
 *   - the length it gives is what it reads,
 *   - reading in blocks of any size gives the same samples as in one go,
 *   - a seek, forward or back, lands on the frame asked for: the samples
 *     after it are the ones a read from the start has there (exactly;
 *     within 1e-3 for AAC, ADTS or MP4, whose decoder is primed with two
 *     access units before the frame; with SBR one is not enough),
 *   - the samples are not rounded to 16 bits (a 24-bit FLAC, MP3 and AAC
 *     have samples off the 16-bit grid),
 *   - an MP4 gives the same samples as rmp4_audio's whole decode of it.
 *
 *   build-tests/audio_stream_test sine.flac sine.mp3 sine.aac sine.m4a */

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <formats/rmp4_audio.h>

#include "../../src/common/audio_stream.h"

static int failures = 0;

#define CHECK(cond, what)                                                                      \
    do {                                                                                       \
        if (!(cond)) {                                                                         \
            printf("FAIL %s: %s (line %d)\n", name, what, __LINE__);                           \
            ++failures;                                                                        \
        }                                                                                      \
    } while (0)

static unsigned char* load(const char* path, size_t* size) {
    FILE* f = fopen(path, "rb");
    unsigned char* data;
    long n;
    if (!f)
        return NULL;
    fseek(f, 0, SEEK_END);
    n = ftell(f);
    fseek(f, 0, SEEK_SET);
    data = (unsigned char*)malloc(n > 0 ? (size_t)n : 1);
    if (data && fread(data, 1, (size_t)n, f) != (size_t)n) {
        free(data);
        data = NULL;
    }
    fclose(f);
    *size = (size_t)n;
    return data;
}

static enum audio_stream_kind kind_of(const char* path) {
    const char* dot = strrchr(path, '.');
    if (dot && strcmp(dot, ".flac") == 0)
        return AUDIO_STREAM_FLAC;
    if (dot && strcmp(dot, ".mp3") == 0)
        return AUDIO_STREAM_MP3;
    if (dot && strcmp(dot, ".aac") == 0)
        return AUDIO_STREAM_ADTS;
    return AUDIO_STREAM_MP4;
}

/* Reads the whole stream in blocks of @block frames (cycling through a few sizes when 0) */
static size_t read_all(audio_stream_t* s, float* out, size_t cap, size_t block) {
    static const size_t sizes[] = {1, 777, 4096, 5000, 13};
    size_t got = 0, i = 0;
    const unsigned ch = audio_stream_channels(s);
    for (;;) {
        size_t want = block ? block : sizes[i++ % 5];
        size_t n;
        if (want > cap - got)
            want = cap - got;
        if (want == 0)
            break;
        n = audio_stream_read(s, out + got * ch, want);
        got += n;
        if (n < want)
            break;
    }
    return got;
}

static void run(const char* name) {
    size_t size = 0, frames, got, i, n;
    unsigned char* data = load(name, &size);
    const enum audio_stream_kind kind = kind_of(name);
    audio_stream_t* s;
    unsigned ch;
    float *whole, *blocks, *after;
    int off_grid = 0, same = 1;
    /* forward from where the stream is, then back, to the start and to the middle */
    static const double seeks[] = {0.0, 0.1, 0.37, 0.5, 0.93, 0.0, 0.6, 0.2};
    if (!data) {
        printf("FAIL %s: cannot read it\n", name);
        ++failures;
        return;
    }
    s = audio_stream_open(data, size, kind);
    CHECK(s != NULL, "opens");
    if (!s) {
        free(data);
        return;
    }
    ch = audio_stream_channels(s);
    frames = (size_t)audio_stream_frames(s);
    CHECK(ch >= 1 && ch <= 2 && audio_stream_rate(s) >= 8000 && frames > 0, "geometry");
    whole = (float*)malloc(sizeof(float) * (frames + 4096) * ch);
    blocks = (float*)malloc(sizeof(float) * (frames + 4096) * ch);
    after = (float*)malloc(sizeof(float) * 4096 * ch);

    got = read_all(s, whole, frames + 4096, 1u << 20);
    CHECK(got == frames, "reads as many frames as it says it has");
    if (got != frames)
        printf("  %lu read, %lu said\n", (unsigned long)got, (unsigned long)frames);

    CHECK(audio_stream_seek(s, 0), "seeks to the start");
    n = read_all(s, blocks, frames + 4096, 0);
    CHECK(n == got, "blocks of any size read as many");
    for (i = 0; i < n * ch && i < got * ch; i++)
        if (blocks[i] != whole[i]) {
            same = 0;
            break;
        }
    CHECK(same, "blocks of any size read the same samples");

    for (i = 0; i < sizeof(seeks) / sizeof(seeks[0]); i++) {
        const size_t at = (size_t)(seeks[i] * (double)got);
        const size_t want = got - at < 4096 ? got - at : 4096;
        const float tolerance =
            kind == AUDIO_STREAM_ADTS || kind == AUDIO_STREAM_MP4 ? 1e-3f : 0.0f;
        size_t k, bad = 0;
        CHECK(audio_stream_seek(s, at), "seeks");
        n = audio_stream_read(s, after, want);
        CHECK(n == want, "reads after a seek");
        for (k = 0; k < n * ch; k++)
            if (fabsf(after[k] - whole[at * ch + k]) > tolerance)
                ++bad;
        if (bad)
            printf("  %s: after a seek to frame %lu, %lu samples differ\n", name,
                   (unsigned long)at, (unsigned long)bad);
        CHECK(bad == 0, "a seek lands on the frame");
    }
    CHECK(audio_stream_seek(s, got + 100) && audio_stream_read(s, after, 16) == 0,
          "a seek past the end reads nothing");

    for (i = 0; i < got * ch; i++) {
        const float q = whole[i] * 32768.0f;
        if (fabsf(q - floorf(q + 0.5f)) > 1e-3f) {
            off_grid = 1;
            break;
        }
    }
    CHECK(off_grid, "samples are not rounded to 16 bits");
    if (kind == AUDIO_STREAM_MP4) {
        /* The same samples as rmp4_audio's whole decode of the file, its edit list honoured */
        float* ref = NULL;
        size_t ref_frames = 0, k, bad = 0;
        unsigned rate = 0, chans = 0;
        CHECK(rmp4_audio_decode_f32(data, size, 0, &ref, &ref_frames, &rate, &chans),
              "rmp4_audio decodes it");
        CHECK(ref_frames == got && chans == ch && rate == audio_stream_rate(s),
              "as long as rmp4_audio's decode");
        for (k = 0; ref && k < ref_frames * ch && k < got * ch; k++)
            if (ref[k] != whole[k])
                ++bad;
        if (bad)
            printf("  %lu samples differ from rmp4_audio's\n", (unsigned long)bad);
        CHECK(bad == 0, "the same samples as rmp4_audio's decode");
        free(ref);
    }
    {
        double sum = 0.0;
        for (i = 0; i < got * ch; i++)
            sum += (double)whole[i] * whole[i];
        sum = sqrt(sum / (double)(got * ch));
        /* ffmpeg's sine source is at 1/8 of full scale: an RMS of 0.088, 3 dB less made
         * stereo */
        if (!(sum > 0.055 && sum < 0.1))
            printf("  RMS %f\n", sum);
        CHECK(sum > 0.055 && sum < 0.1, "the sine's level");
    }
    printf("%s: %lu frames, %u channels, %u Hz\n", name, (unsigned long)got, ch,
           audio_stream_rate(s));
    free(whole);
    free(blocks);
    free(after);
    audio_stream_close(s);
    free(data);
}

int main(int argc, char** argv) {
    int i;
    const char* name = "audio_stream";
    if (argc < 2) {
        printf("audio_stream: no files, skipped\n");
        return 0;
    }
    CHECK(audio_stream_open(NULL, 0, AUDIO_STREAM_MP3) == NULL, "nothing opens as nothing");
    {
        static const unsigned char junk[64] = {1, 2, 3};
        CHECK(audio_stream_open(junk, sizeof(junk), AUDIO_STREAM_FLAC) == NULL, "junk is no FLAC");
        CHECK(audio_stream_open(junk, sizeof(junk), AUDIO_STREAM_ADTS) == NULL, "junk is no AAC");
    }
    for (i = 1; i < argc; i++)
        run(argv[i]);
    if (failures) {
        printf("audio_stream: %d failed\n", failures);
        return 1;
    }
    printf("audio_stream: all passed\n");
    return 0;
}
