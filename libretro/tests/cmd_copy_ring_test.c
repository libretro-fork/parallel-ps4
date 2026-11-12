/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

/* cmd_copy_ring (Copy GPU Buffers): a frame's copies are not written over while
 * the GPU thread has not finished them, the room is used again once it has,
 * and with none free a submission is left where the game has it. */

#include <stdio.h>

#include "../../src/video_core/amdgpu/cmd_copy_ring.h"

static int failures = 0;

#define CHECK(cond, what)                                                                      \
   do                                                                                          \
   {                                                                                           \
      if (!(cond))                                                                             \
      {                                                                                        \
         printf("FAIL %s (line %d)\n", what, __LINE__);                                       \
         failures++;                                                                           \
      }                                                                                        \
   } while (0)

/* One frame's submission of @words dcb words of @value; a CE stream when @ce */
static const uint32_t* submit(cmd_copy_ring_t* ring, uint32_t value, size_t words, int ce,
                              int* copied)
{
   static uint32_t dcb[64];
   static uint32_t ccb[4];
   const uint32_t* d;
   const uint32_t* c;
   size_t i;
   for (i = 0; i < words; i++)
      dcb[i] = value;
   ccb[0] = value;
   *copied = cmd_copy_ring_copy(ring, dcb, words, ccb, ce ? 1 : 0, &d, &c);
   cmd_copy_ring_submitted(ring, ce);
   /* The game writes over its own buffer as soon as it has submitted it */
   for (i = 0; i < words; i++)
      dcb[i] = 0xdeadu;
   return d;
}

int main(void)
{
   cmd_copy_ring_t* ring = cmd_copy_ring_new(16, 4, 3);
   const uint32_t *f1, *f2, *f3, *f4;
   int c1, c2, c3, c4, c5;
   CHECK(ring != NULL, "made");
   CHECK(cmd_copy_ring_new(16, 4, 1) == NULL, "one segment is no ring");

   /* Three frames go out; the GPU thread is still on the first */
   f1 = submit(ring, 1, 10, 1, &c1);
   cmd_copy_ring_frame_done(ring);
   f2 = submit(ring, 2, 10, 0, &c2);
   cmd_copy_ring_frame_done(ring);
   f3 = submit(ring, 3, 10, 1, &c3);
   cmd_copy_ring_frame_done(ring);
   CHECK(c1 && c2 && c3, "three frames copied, one segment each");
   CHECK(f1[0] == 1 && f1[9] == 1, "the first frame's copy is whole, the game's buffer is not");
   CHECK(f2 != f1 && f3 != f1 && f3 != f2, "each frame in a segment of its own");

   /* A fourth: the first segment is not finished, so it is not written over */
   f4 = submit(ring, 4, 10, 0, &c4);
   CHECK(!c4, "no room free: not copied");
   CHECK(f4[0] == 0xdeadu, "...and read where the game has it");
   CHECK(f1[0] == 1 && f1[9] == 1, "the unfinished frame's copy is untouched");

   /* The GPU thread finishes the first frame (its gfx and CE submissions) */
   cmd_copy_ring_gfx_finished(ring);
   cmd_copy_ring_ce_finished(ring);
   cmd_copy_ring_frame_done(ring);
   f4 = submit(ring, 5, 10, 0, &c5);
   CHECK(c5 && f4 == f1 && f4[0] == 5, "the finished segment is used again");
   CHECK(f2[0] == 2 && f3[0] == 3, "the unfinished ones are still whole");

   /* Two submissions in one frame share its segment while they fit */
   {
      int a, b;
      const uint32_t* x;
      const uint32_t* y;
      cmd_copy_ring_free(ring);
      ring = cmd_copy_ring_new(16, 4, 2);
      x = submit(ring, 7, 6, 0, &a);
      y = submit(ring, 8, 6, 0, &b);
      CHECK(a && b && y == x + 6 && x[0] == 7 && y[0] == 8, "a frame's submissions side by side");
      /* One that does not fit moves on to the next segment, which is free */
      y = submit(ring, 9, 6, 0, &b);
      CHECK(b && y[0] == 9 && x[0] == 7, "past the segment: the next one");
      /* Bigger than a segment: never copied */
      y = submit(ring, 10, 17, 0, &b);
      CHECK(!b, "bigger than a segment: not copied");
   }

   cmd_copy_ring_free(ring);
   if (failures)
      return 1;
   printf("cmd_copy_ring: all passed\n");
   return 0;
}
