/* SPDX-FileCopyrightText: Copyright 2025 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef SHADPS4_USBD_EMULATED_INFINITY_H
#define SHADPS4_USBD_EMULATED_INFINITY_H

/* An emulated Disney Infinity base */

#include <retro_common_api.h>

#include "common/types.h"
#include "core/libraries/usbd/usb_backend.h"

RETRO_BEGIN_DECLS

#define INFINITY_BLOCK_COUNT 0x14
#define INFINITY_BLOCK_SIZE 0x10
#define INFINITY_FIGURE_SIZE (INFINITY_BLOCK_COUNT * INFINITY_BLOCK_SIZE)
#define MAX_INFINITY_FIGURES 9

extern const usb_emulated_device_t usb_infinity_base;

/* A figure's dump put on the base at @slot (@pad unused), and taken off */
void infinity_load_figure(const char* file_name, u8 pad, u8 slot);
void infinity_remove_figure(u8 pad, u8 slot, int full_remove);

RETRO_END_DECLS

#endif
