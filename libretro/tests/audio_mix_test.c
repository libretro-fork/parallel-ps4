/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

/* The core's audio mix (libretro/audio_mix.c), tested on its own: conversion and folding,
 * two sources summed and clamped, a source with fewer frames adding silence, ring wrap, a
 * closed input going back to the pool, and a producer thread waiting for room while the
 * mixing thread takes frames, every sample arriving in order. */

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
      int16_t* dst = NULL;
      unsigned got = audio_mix_input_reserve(in, &dst, frames - done);
      if (got == 0)
         break;
      memcpy(dst, stereo + done * 2, got * 4);
      audio_mix_input_commit(in, got);
      done += got;
   }
}

static void test_convert(void)
{
   int16_t src8[8] = {1000, 2000, 100, 9999, 10, 20, 30, 40};
   float f2[4]     = {0.5f, -0.5f, 2.0f, -2.0f};
   int16_t mono[2] = {123, -32768};
   int16_t out[8];

   audio_mix_convert_s16(out, mono, 2, 1, 1.0f);
   CHECK(out[0] == 123 && out[1] == 123 && out[2] == -32768 && out[3] == -32768, "mono s16");

   audio_mix_convert_s16(out, src8, 1, 8, 1.0f);
   /* L = 1000 + 0.7071*100 + 0.7071*(10+30); R = 2000 + 70.71 + 0.7071*(20+40) */
   CHECK(out[0] == 1098 && out[1] == 2113, "7.1 fold s16");

   audio_mix_convert_f32(out, f2, 2, 2, 1.0f);
   CHECK(out[0] == 16384 && out[1] == -16384, "stereo f32");
   CHECK(out[2] == 32767 && out[3] == -32768, "f32 clamp");

   audio_mix_convert_s16(out, mono, 1, 1, 0.5f);
   CHECK(out[0] == 61, "gain");
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
   test_sum_and_close();
   test_pool();
   test_stream();
   if (failures)
      return 1;
   printf("audio_mix: all passed\n");
   return 0;
}
