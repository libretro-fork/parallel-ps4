/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef SHADPS4_LIBRETRO_VBLANK_C_H
#define SHADPS4_LIBRETRO_VBLANK_C_H

/* The frontend's frames as the console's vblanks count them, for code written in C: the
 * Libretro::VblankTicks, WaitVblankTicks and WakeVblankWaits of libretro_bridge.h. Defined in
 * host.cpp. */

#include <stdint.h>
#include <boolean.h>
#include <retro_atomic.h>
#include <retro_common_api.h>

RETRO_BEGIN_DECLS

/* The frontend's frames so far, one a retro_run; it wraps */
uint32_t libretro_vblank_ticks(void);
/* Until the frontend's frames are past @seen, the emulator quits, or *@stop is nonzero (set it,
 * then libretro_wake_vblank_waits). Nothing is timed. false when stopped or quitting. */
bool libretro_wait_vblank_ticks(uint32_t seen, retro_atomic_int_t* stop);
/* Wakes every libretro_wait_vblank_ticks, to look at its @stop again */
void libretro_wake_vblank_waits(void);

RETRO_END_DECLS

#endif
