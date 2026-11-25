/* SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef SHADPS4_VIDEOOUT_BUFFER_H
#define SHADPS4_VIDEOOUT_BUFFER_H

/* The game's display buffers, as libSceVideoOut and the presenter see them */

#include <boolean.h>
#include <retro_common_api.h>
#include <stdint.h>

#include "common/types.h"

RETRO_BEGIN_DECLS

#define VIDEO_OUT_MAX_DISPLAY_BUFFERS 16
#define VIDEO_OUT_MAX_DISPLAY_BUFFER_GROUPS 4

/* A buffer's pixel format */
typedef u32 VideoOutPixelFormat;
#define VIDEO_OUT_PIXEL_FORMAT_UNKNOWN 0u
#define VIDEO_OUT_PIXEL_FORMAT_A8R8G8B8_SRGB 0x80000000u
#define VIDEO_OUT_PIXEL_FORMAT_A8B8G8R8_SRGB 0x80002200u
#define VIDEO_OUT_PIXEL_FORMAT_A2R10G10B10 0x88060000u
#define VIDEO_OUT_PIXEL_FORMAT_A2R10G10B10_SRGB 0x88000000u
#define VIDEO_OUT_PIXEL_FORMAT_A2R10G10B10_BT2020_PQ 0x88740000u
#define VIDEO_OUT_PIXEL_FORMAT_A16R16G16B16_FLOAT 0xC1060000u

/* A buffer's tiling */
typedef s32 VideoOutTilingMode;
#define VIDEO_OUT_TILING_MODE_TILE 0
#define VIDEO_OUT_TILING_MODE_LINEAR 1

/* A known pixel format's name; any other stops the emulator (driver.c) */
const char* video_out_pixel_format_string(VideoOutPixelFormat format);

typedef struct BufferAttribute {
    VideoOutPixelFormat pixel_format;
    VideoOutTilingMode tiling_mode;
    s32 aspect_ratio;
    u32 width;
    u32 height;
    u32 pitch_in_pixel;
    u32 option;
    u32 reserved0;
    u64 reserved1;
} BufferAttribute;

typedef struct BufferAttributeGroup {
    bool is_occupied;
    BufferAttribute attrib;
} BufferAttributeGroup;

/* group_index is -1 for a free slot */
typedef struct VideoOutBuffer {
    s32 group_index;
    uintptr_t address_left;
    uintptr_t address_right;
} VideoOutBuffer;

RETRO_END_DECLS

#endif
