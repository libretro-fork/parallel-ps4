/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

/* libretro's analog values to the pad's bytes, each to the nearest step. */

#ifndef SHADPS4_LIBRETRO_PAD_SCALE_H
#define SHADPS4_LIBRETRO_PAD_SCALE_H

/* A stick axis, -0x8000..0x7fff, to 0..255 around the pad's centre, 128: a 256th of the
 * range a step, rounded to the nearest, the same either side of rest. Cut down instead, as it
 * was, rest came out as 127: a stick always a little off to the left and up for a game that
 * reads it without a dead zone. */
static unsigned pad_scale_stick(int raw)
{
   long q = raw >= 0 ? ((long)raw + 128L) / 256L : -((128L - (long)raw) / 256L);
   long v = 128L + q;
   return (unsigned)(v < 0 ? 0 : v > 255 ? 255 : v);
}

/* A trigger, 0..0x7fff, to 0..255 */
static unsigned pad_scale_trigger(int raw)
{
   long v = (long)raw * 255L + 0x3fffL;
   v /= 0x7fffL;
   return (unsigned)(v < 0 ? 0 : v > 255 ? 255 : v);
}

#endif
