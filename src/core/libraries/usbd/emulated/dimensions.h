/* SPDX-FileCopyrightText: Copyright 2025 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef SHADPS4_USBD_EMULATED_DIMENSIONS_H
#define SHADPS4_USBD_EMULATED_DIMENSIONS_H

/* An emulated LEGO Dimensions toypad */

#include <retro_common_api.h>

#include "common/types.h"
#include "core/libraries/usbd/usb_backend.h"

RETRO_BEGIN_DECLS

#define DIMENSIONS_BLOCK_COUNT 0x2D
#define DIMENSIONS_BLOCK_SIZE 0x04
#define DIMENSIONS_FIGURE_SIZE (DIMENSIONS_BLOCK_COUNT * DIMENSIONS_BLOCK_SIZE)
#define MAX_DIMENSIONS_FIGURES 7

extern const usb_emulated_device_t usb_dimensions_toypad;

/* A figure's dump put on @pad at @index, taken off (and reported when @full_remove), moved,
 * picked up and put back */
void dimensions_load_figure(const char* file_name, u8 pad, u8 index);
void dimensions_remove_figure(u8 pad, u8 index, int full_remove);
void dimensions_move_figure(u8 new_pad, u8 new_index, u8 old_pad, u8 old_index);
void dimensions_temp_remove_figure(u8 index);
void dimensions_cancel_remove_figure(u8 index);

RETRO_END_DECLS

#endif
