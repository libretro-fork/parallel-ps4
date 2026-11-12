/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

/* The core's audio mix (libretro/audio_mix.c), tested on its own: conversion and folding,
 * one rounding to 16 bits (to the nearest step, after the sum), two sources summed and clamped, a source with fewer frames adding silence, ring wrap, a
 * closed input going back to the pool, and a producer thread waiting for room while the
 * mixing thread takes frames, every sample arriving in order. */

#include <math.h>
#include <stdio.h>
#include <string.h>

#include <rthreads/rthreads.h>

#include "../audio_mix.h"

#define RING_FRAMES 8192

static int failures = 0;

#define CHECK(cond, what)                                                                     \
   do                                                                                         \
   {                                                                                          \
      if (!(cond))                                                                            \
      {                                                                                       \
         printf("FAIL: %s (line %d)\n", what, __LINE__);                                      \
         failures++;                                                                          \
      }                                                                                       \
   } while (0)

static int16_t ring[RING_FRAMES * 2];

static void write_frames(audio_mix_input_t* in, const int16_t* stereo, unsigned frames)
{
   unsigned done = 0;
   while (done < frames)
   {
      float* dst   = NULL;
      unsigned got = audio_mix_input_reserve(in, &dst, frames - done);
      if (got == 0)
         break;
      audio_mix_convert_s16(dst, stereo + done * 2, got, 2, 1.0f);
      audio_mix_input_commit(in, got);
      done += got;
   }
}

static void write_float(audio_mix_input_t* in, const float* stereo, unsigned frames)
{
   float* dst = NULL;
   if (audio_mix_input_reserve(in, &dst, frames) == frames)
   {
      memcpy(dst, stereo, frames * 2 * sizeof(float));
      audio_mix_input_commit(in, frames);
   }
}

static int near(float a, float b)
{
   return fabsf(a - b) < 0.01f;
}

static void test_convert(void)
{
   int16_t src8[8] = {1000, 2000, 100, 9999, 10, 20, 30, 40};
   float f2[4]     = {0.5f, -0.5f, 2.0f, -2.0f};
   int16_t mono[2] = {123, -32768};
   float out[8];

   audio_mix_convert_s16(out, mono, 2, 1, 1.0f);
   CHECK(out[0] == 123.0f && out[1] == 123.0f && out[2] == -32768.0f && out[3] == -32768.0f,
         "mono s16");

   audio_mix_convert_s16(out, src8, 1, 8, 1.0f);
   /* L = 1000 + 0.7071*100 + 0.7071*(10+30); R = 2000 + 70.71 + 0.7071*(20+40) */
   CHECK(near(out[0], 1098.994f) && near(out[1], 2113.136f), "7.1 fold s16, kept whole");

   audio_mix_convert_f32(out, f2, 2, 2, 1.0f);
   CHECK(out[0] == 16384.0f && out[1] == -16384.0f, "stereo f32");
   CHECK(out[2] == 65536.0f && out[3] == -65536.0f, "f32 not clamped before the mix");

   audio_mix_convert_s16(out, mono, 1, 1, 0.5f);
   CHECK(out[0] == 61.5f, "gain, kept whole");
}

/* What the sum is rounded to: the nearest step, halves away from zero, clamped. Cutting
 * towards zero instead would make 0.6 and -0.6 silence, and rounding each source before the
 * sum would make two sources of 0.6 silence too. */
static void test_rounding(void)
{
   static const float a[10] = {0.4f, -0.4f, 0.6f, -0.6f, 1.5f, -1.5f, 32766.6f, -32768.4f,
                               99999.0f, -99999.0f};
   static const float half[2] = {0.6f, 0.6f};
   audio_mix_input_t* ia = audio_mix_input_open();
   audio_mix_input_t* ib = audio_mix_input_open();
   unsigned n;
   CHECK(ia && ib, "open two");

   write_float(ia, a, 5);
   n = audio_mix_pump(ring, RING_FRAMES - 1, 0, 1024);
   CHECK(n == 5, "five frames");
   CHECK(ring[0] == 0 && ring[1] == 0, "under half a step is silence");
   CHECK(ring[2] == 1 && ring[3] == -1, "over half a step is a step, either sign");
   CHECK(ring[4] == 2 && ring[5] == -2, "halves away from zero");
   CHECK(ring[6] == 32767 && ring[7] == -32768, "the ends");
   CHECK(ring[8] == 32767 && ring[9] == -32768, "clamped");

   write_float(ia, half, 1);
   write_float(ib, half, 1);
   n = audio_mix_pump(ring, RING_FRAMES - 1, 0, 1024);
   CHECK(n == 1 && ring[0] == 1 && ring[1] == 1, "rounded once, after the sum");

   audio_mix_input_close(ia);
   audio_mix_input_close(ib);
   audio_mix_pump(ring, RING_FRAMES - 1, 0, 1024);
}

static void test_sum_and_close(void)
{
   int16_t a[4]          = {30000, 1, 30000, 2};
   int16_t b[2]          = {30000, 5};
   audio_mix_input_t* ia = audio_mix_input_open();
   audio_mix_input_t* ib = audio_mix_input_open();
   unsigned n;
   CHECK(ia && ib && ia != ib, "open two");

   write_frames(ia, a, 2);
   write_frames(ib, b, 1);
   n = audio_mix_pump(ring, RING_FRAMES - 1, RING_FRAMES - 1, 1024);
   CHECK(n == 2, "the longer source decides the frames");
   /* frame 0 at the ring's last slot, frame 1 wrapped to slot 0 */
   CHECK(ring[(RING_FRAMES - 1) * 2] == 32767 && ring[(RING_FRAMES - 1) * 2 + 1] == 6,
         "sum clamped");
   CHECK(ring[0] == 30000 && ring[1] == 2, "shorter source adds silence, ring wraps");

   write_frames(ib, b, 1);
   audio_mix_input_close(ib);
   n = audio_mix_pump(ring, RING_FRAMES - 1, 0, 1024);
   CHECK(n == 0, "closed input's frames dropped");
   audio_mix_input_close(ia);
   audio_mix_pump(ring, RING_FRAMES - 1, 0, 1024);
}

static void test_pool(void)
{
   audio_mix_input_t* all[AUDIO_MIX_INPUTS];
   unsigned i;
   for (i = 0; i < AUDIO_MIX_INPUTS; i++)
      all[i] = audio_mix_input_open();
   CHECK(all[AUDIO_MIX_INPUTS - 1] != NULL, "whole pool opens");
   CHECK(audio_mix_input_open() == NULL, "full pool says so");
   for (i = 0; i < AUDIO_MIX_INPUTS; i++)
      audio_mix_input_close(all[i]);
   audio_mix_pump(ring, RING_FRAMES - 1, 0, 1024);
   CHECK(audio_mix_input_open() != NULL, "closed inputs come back");
}

static void test_clip(void)
{
   static const float clip[6] = {100, -100, 200, -200, 300, -300};
   unsigned n;
   audio_mix_play_clip(clip, 3, 2.0f);
   n = audio_mix_pump(ring, RING_FRAMES - 1, 0, 2);
   CHECK(n == 2 && ring[0] == 200 && ring[1] == -200 && ring[2] == 400, "clip plays, gain");
   n = audio_mix_pump(ring, RING_FRAMES - 1, 2, 1024);
   CHECK(n == 1 && ring[4] == 600 && ring[5] == -600, "clip ends where it ends");
   n = audio_mix_pump(ring, RING_FRAMES - 1, 3, 1024);
   CHECK(n == 0, "nothing after the clip");
}

static void test_resample(void)
{
   static int16_t src[2000 * 2];
   static float out[5000 * 2];
   audio_mix_resampler_t r;
   uint64_t pos;
   unsigned i, n;
   int exact = 1, dc = 1;

   for (i = 0; i < 2000; i++)
   {
      src[i * 2 + 0] = (int16_t)(i * 7);
      src[i * 2 + 1] = (int16_t)(-(int)i * 3);
   }
   audio_mix_resampler_init(&r, 48000, 2);
   pos = 0;
   n   = audio_mix_resample(&r, src, 2000, &pos, out, 5000, 1.0f);
   for (i = 0; i < n * 2; i++)
      if (!near(out[i], src[i]))
         exact = 0;
   CHECK(n == 2000 && exact, "48 kHz passes through unchanged");

   for (i = 0; i < 2000 * 2; i++)
      src[i] = 10000;
   audio_mix_resampler_init(&r, 24000, 2);
   pos = 0;
   n   = audio_mix_resample(&r, src, 2000, &pos, out, 5000, 1.0f);
   for (i = 16; i < n - 16; i++)
      if (out[i * 2] < 9995 || out[i * 2] > 10005)
         dc = 0;
   CHECK(n == 4000 && dc, "24 kHz doubles, level kept");

   audio_mix_resampler_init(&r, 96000, 1);
   pos = 0;
   n   = audio_mix_resample(&r, src, 4000, &pos, out, 5000, 0.5f);
   dc  = 1;
   for (i = 16; i < n - 16; i++)
      if (out[i * 2] < 4995 || out[i * 2] > 5005 || out[i * 2 + 1] != out[i * 2])
         dc = 0;
   CHECK(n == 2000 && dc, "96 kHz mono halves, level and gain kept");

   {
      /* From float: scaled to 16-bit steps, and not rounded on the way */
      static float fsrc[2000 * 2];
      for (i = 0; i < 2000 * 2; i++)
         fsrc[i] = (float)(i & 1 ? -0.25 : 0.25) + 0.3f / 32768.0f;
      audio_mix_resampler_init(&r, 48000, 2);
      pos   = 0;
      n     = audio_mix_resample_f32(&r, fsrc, 2000, &pos, out, 5000, 1.0f);
      exact = 1;
      for (i = 0; i < n * 2; i++)
         if (!near(out[i], (i & 1 ? -8192.0f : 8192.0f) + 0.3f))
            exact = 0;
      CHECK(n == 2000 && exact, "float source scaled, its fraction of a step kept");
   }
}

/* A producer writes a counting sequence in port-sized buffers, waiting for room; the main
 * thread mixes until all of it has arrived. */
#define BUFFER_FRAMES 1024
#define STREAM_FRAMES (196 * BUFFER_FRAMES)
static audio_mix_input_t* stream_in;

static void producer(void* data)
{
   static int16_t buf[BUFFER_FRAMES * 2];
   unsigned sent = 0;
   (void)data;
   while (sent < STREAM_FRAMES)
   {
      unsigned i;
      if (!audio_mix_input_wait(stream_in, BUFFER_FRAMES, 5000000))
         break;
      for (i = 0; i < BUFFER_FRAMES; i++)
      {
         buf[i * 2 + 0] = (int16_t)((sent + i) & 0x7fff);
         buf[i * 2 + 1] = (int16_t)(-(int)((sent + i) & 0x7fff));
      }
      write_frames(stream_in, buf, BUFFER_FRAMES);
      sent += BUFFER_FRAMES;
   }
}

static void test_stream(void)
{
   sthread_t* t;
   unsigned got   = 0;
   unsigned at    = 0;
   int in_order   = 1;
   unsigned spins = 0;
   stream_in      = audio_mix_input_open();
   t              = sthread_create(producer, NULL);
   CHECK(t != NULL, "producer thread");
   while (got < STREAM_FRAMES && spins < 100000000u)
   {
      unsigned n = audio_mix_pump(ring, RING_FRAMES - 1, at, 700);
      unsigned i;
      for (i = 0; i < n; i++)
      {
         unsigned pos = ((at + i) & (RING_FRAMES - 1)) * 2;
         if (ring[pos] != (int16_t)((got + i) & 0x7fff) ||
             ring[pos + 1] != (int16_t)(-(int)((got + i) & 0x7fff)))
            in_order = 0;
      }
      at += n;
      got += n;
      spins++;
   }
   sthread_join(t);
   CHECK(got == STREAM_FRAMES, "every frame arrives");
   CHECK(in_order, "frames arrive in order, unchanged");
   audio_mix_input_close(stream_in);
}

int main(void)
{
   if (!audio_mix_init())
   {
      printf("FAIL: init\n");
      return 1;
   }
   test_convert();
   test_rounding();
   test_sum_and_close();
   test_pool();
   test_clip();
   test_resample();
   test_stream();
   if (failures)
      return 1;
   printf("audio_mix: all passed\n");
   return 0;
}
