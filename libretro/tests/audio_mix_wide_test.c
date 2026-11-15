/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

/* The mix made for surround (audio_mix_init_channels(8)):
 *   - an input of eight channels and one of two, mixed: the result is 7.1, the stereo input
 *     in the front pair, every channel the sum of what went into it;
 *   - with only stereo playing the result is stereo;
 *   - taken as float (not rounded, clamped past full scale) and as 16-bit (rounded, clamped);
 *   - the console's 7.1 orders put into the mix's, each channel at its own gain;
 *   - an input of eight is refused by a mix made for two (here: none of nine).
 *   - the stereo pump still gives the front pair;
 *   - a volume change ramped across a buffer split in two (a ring's wrap): each frame's gain on
 *     the line from the old to the new, the new reached at the buffer's last frame, the old
 *     not at its first (no step). */

#include <stdio.h>
#include <string.h>

#include "../audio_mix.h"

static int failures = 0;

#define CHECK(cond, what)                                                                          \
    do {                                                                                           \
        if (!(cond)) {                                                                             \
            printf("FAIL %s (line %d)\n", what, __LINE__);                                         \
            ++failures;                                                                            \
        }                                                                                          \
    } while (0)

static void write_frames(audio_mix_input_t* in, const float* src, unsigned frames,
                         unsigned channels) {
    unsigned done = 0;
    while (done < frames) {
        float* dst = NULL;
        unsigned got = audio_mix_input_reserve(in, &dst, frames - done);
        if (!got)
            break;
        memcpy(dst, src + done * channels, got * channels * sizeof(float));
        audio_mix_input_commit(in, got);
        done += got;
    }
}

static void test_mix(void) {
    static const float wide[2 * 8] = {100, 200, 300, 400, 500, 600, 700, 800,
                                      -1,  -2,  -3,  -4,  -5,  -6,  -7,  99999};
    static const float stereo[2 * 2] = {10.25f, 20.5f, 1, 2};
    float f[2 * 8];
    int16_t s[2 * 8];
    unsigned channels = 0, n, c;
    audio_mix_input_t* a = audio_mix_input_open_channels(8);
    audio_mix_input_t* b = audio_mix_input_open();
    CHECK(audio_mix_channels() == 8, "a mix made for 8");
    CHECK(a && b, "an input of 8 and one of 2");
    CHECK(audio_mix_input_open_channels(6) == NULL, "no input of 6");

    /* A frame of two: an input still writing with less than the frame is held back */
    write_frames(a, wide, 2, 8);
    write_frames(b, stereo, 2, 2);
    n = audio_mix_mix(2, &channels);
    CHECK(n == 2 && channels == 8, "eight channels played: the result is 7.1");
    audio_mix_take_f32(f, 0, 2, 8);
    CHECK(f[0] == 110.25f / 32768.0f && f[1] == 220.5f / 32768.0f,
          "the stereo input in the front pair, fractions kept");
    for (c = 2; c < 8; c++)
        if (f[c] != wide[c] / 32768.0f)
            CHECK(0, "the other channels as the 7.1 input has them");
    CHECK(f[15] == 1.0f, "clamped past full scale");
    audio_mix_take_s16(s, 0, 2, 8);
    CHECK(s[0] == 110 && s[1] == 221 && s[7] == 800 && s[15] == 32767,
          "16-bit: rounded to the nearest step, clamped");
    audio_mix_take_f32(f, 1, 1, 8);
    CHECK(f[0] == -0.0f + (-1.0f + 1.0f) / 32768.0f && f[1] == 0.0f, "from a later frame");

    /* The 7.1 input still open, silent this frame: still 7.1, the layout does not flip */
    write_frames(b, stereo, 2, 2);
    n = audio_mix_mix(2, &channels);
    CHECK(n == 2 && channels == 8, "a 7.1 input open but silent: the result stays 7.1");
    audio_mix_take_f32(f, 0, 2, 8);
    CHECK(f[0] == 10.25f / 32768.0f && f[2] == 0.0f && f[8 + 1] == 2.0f / 32768.0f,
          "the stereo in the front pair, the rest silent");

    /* Only stereo open: stereo */
    audio_mix_input_close(a);
    audio_mix_mix(2, &channels);
    write_frames(b, stereo, 2, 2);
    n = audio_mix_mix(2, &channels);
    CHECK(n == 2 && channels == 2, "only stereo open: the result is stereo");
    audio_mix_take_f32(f, 0, 2, 2);
    CHECK(f[0] == 10.25f / 32768.0f && f[3] == 2.0f / 32768.0f, "stereo taken as stereo");

    audio_mix_input_close(b);
    audio_mix_mix(1024, &channels);
}

static void test_convert(void) {
    /* The console's standard 7.1 order has the back and side pairs the other way round */
    static const int std_order[8] = {0, 1, 2, 3, 6, 7, 4, 5};
    static const int16_t src[8] = {1, 2, 3, 4, 5, 6, 7, 8};
    static const float fsrc[8] = {0.5f, -0.5f, 0, 0, 0.25f, 0, 0.125f, 0};
    static const float gains[8] = {1, 1, 1, 1, 2, 2, 0.5f, 0.5f};
    float dst[8];
    audio_mix_convert_s16_wide(dst, src, 1, std_order, gains);
    CHECK(dst[0] == 1 && dst[3] == 4 && dst[4] == 14 && dst[5] == 16 && dst[6] == 2.5f &&
              dst[7] == 3,
          "16-bit: reordered, each channel at its gain");
    audio_mix_convert_f32_wide(dst, fsrc, 1, std_order, gains);
    CHECK(dst[0] == 16384.0f && dst[1] == -16384.0f && dst[4] == 8192.0f && dst[6] == 4096.0f,
          "float: reordered, scaled by 32768, each channel at its gain");
}

static void test_stereo_pump(void) {
    static const float wide[8] = {1000, 2000, 3, 4, 5, 6, 7, 8};
    static int16_t ring[64 * 2];
    unsigned n;
    audio_mix_input_t* a = audio_mix_input_open_channels(8);
    write_frames(a, wide, 1, 8);
    n = audio_mix_pump(ring, 63, 0, 64);
    CHECK(n == 1 && ring[0] == 1000 && ring[1] == 2000, "the stereo pump gives the front pair");
    audio_mix_input_close(a);
    audio_mix_pump(ring, 63, 0, 64);
}

static void test_ramp(void) {
    float buf[8 * 2];
    const float from[2] = {0.0f, 1.0f}, to[2] = {1.0f, 0.0f};
    unsigned i;
    for (i = 0; i < 16; i++)
        buf[i] = 1.0f;
    /* A buffer of 8 frames: 3 then 5 */
    audio_mix_ramp(buf, 3, 2, 0, 8, from, to);
    audio_mix_ramp(buf + 6, 5, 2, 3, 8, from, to);
    for (i = 0; i < 8; i++) {
        const float t = (float)(i + 1) / 8.0f;
        CHECK(buf[i * 2] == t && buf[i * 2 + 1] == 1.0f - t, "the ramp, frame by frame");
    }
}

int main(void) {
    if (!audio_mix_init_channels(8)) {
        printf("FAIL: init\n");
        return 1;
    }
    test_mix();
    test_convert();
    test_stereo_pump();
    test_ramp();
    if (failures) {
        printf("audio_mix_wide: %d failed\n", failures);
        return 1;
    }
    printf("audio_mix_wide: all passed\n");
    return 0;
}
