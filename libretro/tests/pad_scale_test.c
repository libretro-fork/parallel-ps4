/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

/* libretro/pad_scale.h: analog values to the pad's bytes, rounded, a stick at rest centred */

#include <stdio.h>

#include "../pad_scale.h"

static int failures = 0;

#define CHECK(cond)                                                                           \
   do                                                                                         \
   {                                                                                          \
      if (!(cond))                                                                            \
      {                                                                                       \
         printf("FAIL: %s (line %d)\n", #cond, __LINE__);                                     \
         failures++;                                                                          \
      }                                                                                       \
   } while (0)

int main(void)
{
   int raw, below = 0, above = 0;
   CHECK(pad_scale_stick(0) == 128);
   CHECK(pad_scale_stick(-32768) == 0);
   CHECK(pad_scale_stick(32767) == 255);
   CHECK(pad_scale_stick(-1) == 128 && pad_scale_stick(1) == 128);
   CHECK(pad_scale_stick(-128) == 127 && pad_scale_stick(128) == 129);
   /* As many values above the centre as below it */
   for (raw = -32768; raw <= 32767; raw++)
   {
      unsigned v = pad_scale_stick(raw);
      if (v < 128)
         below++;
      else if (v > 128)
         above++;
   }
   CHECK(below - above <= 1 && above - below <= 1);
   CHECK(pad_scale_trigger(0) == 0);
   CHECK(pad_scale_trigger(32767) == 255);
   CHECK(pad_scale_trigger(16384) == 128);
   CHECK(pad_scale_trigger(-5) == 0);
   if (failures)
      return 1;
   printf("pad_scale: all passed\n");
   return 0;
}
