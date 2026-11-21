/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef SHADPS4_LIBRARIES_CAMERA_HELPERS_H
#define SHADPS4_LIBRARIES_CAMERA_HELPERS_H

/* The camera's frames, YUY2, into the formats a game asks for */

#include <retro_common_api.h>

#include "core/libraries/camera/camera.h"

RETRO_BEGIN_DECLS

void camera_yuy2_to_y8(const u8* src, u8* dst, int width, int height);
void camera_yuy2_to_y16(const u8* src, u16* dst, int width, int height);
void camera_yuy2_to_raw8(const u8* src, u8* dst, int width, int height);
void camera_yuy2_to_raw16(const u8* src, u16* dst, int width, int height);
s32 camera_size_of_base_format(OrbisCameraBaseFormat f);
s32 camera_size_of_scale_format(OrbisCameraScaleFormat f);

RETRO_END_DECLS

#endif
