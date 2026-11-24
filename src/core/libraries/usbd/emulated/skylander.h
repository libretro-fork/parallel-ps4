/* SPDX-FileCopyrightText: Copyright 2025 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef SHADPS4_USBD_EMULATED_SKYLANDER_H
#define SHADPS4_USBD_EMULATED_SKYLANDER_H

/* An emulated Skylanders portal (Activision's) */

#include <retro_common_api.h>

#include "common/types.h"
#include "core/libraries/usbd/usb_backend.h"

RETRO_BEGIN_DECLS

#define SKY_BLOCK_COUNT 0x40
#define SKY_BLOCK_SIZE 0x10
#define SKY_FIGURE_SIZE (SKY_BLOCK_COUNT * SKY_BLOCK_SIZE)
#define MAX_SKYLANDERS 16

extern const usb_emulated_device_t usb_skylander_portal;

/* A figure's dump put on the portal at UI @slot (@pad unused), and taken off */
void skylander_load_figure(const char* file_name, u8 pad, u8 slot);
void skylander_remove_figure(u8 pad, u8 slot, int full_remove);

RETRO_END_DECLS

#endif
