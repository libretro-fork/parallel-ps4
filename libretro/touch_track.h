/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

/* A controller's touchpad from the frontend (RETRO_DEVICE_TOUCHPAD) to the pad's two touch
 * slots, a finger staying in the slot it went down in.
 *
 * libretro counts the fingers on the surface: when the first of two lifts, the second becomes
 * finger 0. The pad's slots are fingers, each with its own touch ID from the moment it goes
 * down, so a finger is kept in its slot by where it is: each finger the frontend reports goes
 * to the slot whose finger was nearest a poll ago, a new finger to a free slot.
 *
 * A slot is one word, as the shared memory carries it: down in bit 30, y in bits 15 to 29 and
 * x in bits 0 to 14, each 0 (left, top) to 0x7fff (right, bottom); 0 when no finger is on it. */

#ifndef SHADPS4_LIBRETRO_TOUCH_TRACK_H
#define SHADPS4_LIBRETRO_TOUCH_TRACK_H

#define TOUCH_DOWN 0x40000000u
#define TOUCH_X(w) ((w) & 0x7fffu)
#define TOUCH_Y(w) (((w) >> 15) & 0x7fffu)

/* libretro's -0x7fff..0x7fff across the surface to 0..0x7fff */
static unsigned touch_unit(int raw)
{
   long v = ((long)raw + 0x7fffL + 1L) / 2L;
   return (unsigned)(v < 0 ? 0 : v > 0x7fff ? 0x7fff : v);
}

static unsigned touch_word(unsigned x, unsigned y)
{
   return TOUCH_DOWN | (y << 15) | x;
}

static long touch_distance(unsigned a, unsigned b)
{
   const long dx = (long)TOUCH_X(a) - (long)TOUCH_X(b);
   const long dy = (long)TOUCH_Y(a) - (long)TOUCH_Y(b);
   return dx * dx + dy * dy;
}

/* The fingers now on the surface, @count of them (0 to 2, as touch_word gives them, in the
 * frontend's order), into @slots, which hold the last poll's and are updated: a finger kept
 * in the slot it was in, a lifted finger's slot cleared, a new finger in a free one. */
static void touch_track(unsigned slots[2], const unsigned* fingers, unsigned count)
{
   const int was0 = (slots[0] & TOUCH_DOWN) != 0;
   const int was1 = (slots[1] & TOUCH_DOWN) != 0;
   unsigned out0 = 0, out1 = 0;
   if (count >= 2)
   {
      /* Both slots taken: the pairing that moves the fingers least, where both were down;
       * else the one that was down keeps the nearer finger */
      int swap = 0;
      if (was0 && was1)
         swap = touch_distance(slots[0], fingers[1]) + touch_distance(slots[1], fingers[0]) <
                touch_distance(slots[0], fingers[0]) + touch_distance(slots[1], fingers[1]);
      else if (was0)
         swap = touch_distance(slots[0], fingers[1]) < touch_distance(slots[0], fingers[0]);
      else if (was1)
         swap = touch_distance(slots[1], fingers[0]) < touch_distance(slots[1], fingers[1]);
      out0 = swap ? fingers[1] : fingers[0];
      out1 = swap ? fingers[0] : fingers[1];
   }
   else if (count == 1)
   {
      /* One finger: in the slot whose finger was nearest, else the first free one */
      int second;
      if (was0 && was1)
         second = touch_distance(slots[1], fingers[0]) < touch_distance(slots[0], fingers[0]);
      else
         second = was1;
      if (second)
         out1 = fingers[0];
      else
         out0 = fingers[0];
   }
   slots[0] = out0;
   slots[1] = out1;
}

#endif
