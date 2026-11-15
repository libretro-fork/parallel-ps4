/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

/* The frame-locked mix (libretro/audio_mix.c), as the core uses it: one frame of every
 * source's sound, AUDIO_MIX_FRAME_FRAMES, taken as each of the frontend's frames ends.
 *   - a port of 256-frame buffers that writes whenever audio_mix_input_ready lets it (no
 *     more than a frame queued before a buffer) and a source of 1024-frame chunks: every
 *     frame mixes exactly 800 frames, each source's samples in order, none lost or repeated,
 *     and neither ever has more than a frame and a buffer queued;
 *   - ready says no with more than a frame queued, yes again once the frame is taken;
 *   - a source that stops: what it had left plays out with the other's (that frame as long
 *     as it, not padded with silence), then the other's frames are whole again. */

#include <stdio.h>
#include <string.h>

#include "../audio_mix.h"

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



/* Source A writes 1, 2, 3, ... on its left channel, B 100000 + 1, 2, 3, ... on its right,
 * each in buffers of its own size; the mix of the two keeps both counts readable */
static float buf[2048 * 2];

static void write_counts(audio_mix_input_t* in, unsigned* next, unsigned frames, int right,
                         float base)
{
   float* dst = NULL;
   unsigned got = audio_mix_input_reserve(in, &dst, frames);
   unsigned i;
   (void)buf;
   if (got != frames)
   {
      printf("FAIL: reserve gave %u of %u\n", got, frames);
      failures++;
      return;
   }
   for (i = 0; i < frames; i++)
   {
      dst[i * 2 + (right ? 1 : 0)] = base + (float)(*next)++;
      dst[i * 2 + (right ? 0 : 1)] = 0.0f;
   }
   audio_mix_input_commit(in, frames);
}

static int16_t out[AUDIO_MIX_FRAME_FRAMES * 2];

int main(void)
{
   audio_mix_input_t* a;
   audio_mix_input_t* b;
   unsigned next_a = 1, next_b = 1;  /* the next count each writes */
   unsigned want_a = 1, want_b = 1;  /* the next count the mix should give */
   unsigned f, i, ch;
   int order_ok = 1, peak_ok = 1;

   CHECK(audio_mix_init_channels(2), "init");
   a = audio_mix_input_open();
   b = audio_mix_input_open();
   CHECK(a && b, "two inputs");

   /* Counts small enough for 16 bits: A up to 32767 per channel, B offset into the other */
   for (f = 0; f < 30; f++) /* counts kept under 32767: 16-bit samples */
   {
      /* The game's threads write while ready lets them: a frame queued before a buffer */
      while (audio_mix_input_ready(a, 256, AUDIO_MIX_FRAME_FRAMES))
         write_counts(a, &next_a, 256, 0, 0.0f);
      while (audio_mix_input_ready(b, 1024, AUDIO_MIX_FRAME_FRAMES))
         write_counts(b, &next_b, 1024, 1, 0.0f);
      if (next_a - want_a > AUDIO_MIX_FRAME_FRAMES + 256 ||
          next_b - want_b > AUDIO_MIX_FRAME_FRAMES + 1024)
         peak_ok = 0;
      CHECK(!audio_mix_input_ready(a, 256, AUDIO_MIX_FRAME_FRAMES),
            "more than a frame queued: not ready");
      /* The frame ends: exactly one frame of the mix */
      audio_mix_mix(AUDIO_MIX_FRAME_FRAMES, &ch);
      audio_mix_take_s16(out, 0, AUDIO_MIX_FRAME_FRAMES, 2);
      for (i = 0; i < AUDIO_MIX_FRAME_FRAMES; i++)
      {
         if (out[i * 2] != (int16_t)want_a++ || out[i * 2 + 1] != (int16_t)want_b++)
            order_ok = 0;
      }
   }
   CHECK(order_ok, "every frame exactly 800 of each, in order, none lost or repeated");
   CHECK(peak_ok, "never more than a frame and a buffer queued");
   CHECK(audio_mix_input_ready(a, 256, AUDIO_MIX_FRAME_FRAMES) ||
             next_a - want_a > AUDIO_MIX_FRAME_FRAMES,
         "ready again once the frame is taken");

   /* B stops: what it has left plays out with A's (the frame as long as that, not padded),
    * then A's frames are whole again, B's part silent */
   order_ok = 1;
   for (f = 0; f < 4; f++)
   {
      unsigned got;
      const unsigned b_left = next_b - want_b;
      while (audio_mix_input_ready(a, 256, AUDIO_MIX_FRAME_FRAMES))
         write_counts(a, &next_a, 256, 0, 0.0f);
      got = audio_mix_mix(AUDIO_MIX_FRAME_FRAMES, &ch);
      if (got != (b_left && b_left < AUDIO_MIX_FRAME_FRAMES ? b_left : AUDIO_MIX_FRAME_FRAMES))
         order_ok = 0;
      audio_mix_take_s16(out, 0, got, 2);
      for (i = 0; i < got; i++)
      {
         if (out[i * 2] != (int16_t)want_a++)
            order_ok = 0;
         if (want_b < next_b)
         {
            if (out[i * 2 + 1] != (int16_t)want_b++)
               order_ok = 0;
         }
         else if (out[i * 2 + 1] != 0)
            order_ok = 0;
      }
   }
   CHECK(order_ok, "a source that stops: what it had, with the other's, then whole frames of "
                   "the other alone");

   audio_mix_input_close(a);
   audio_mix_input_close(b);
   if (failures)
   {
      printf("audio_frame: %d failed\n", failures);
      return 1;
   }
   printf("audio_frame: all passed\n");
   return 0;
}
