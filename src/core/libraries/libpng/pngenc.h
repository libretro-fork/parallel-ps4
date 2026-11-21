/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef SHADPS4_LIBRARIES_PNGENC_H
#define SHADPS4_LIBRARIES_PNGENC_H

/* libScePngEnc, encoded by libretro-common's rpng. Registered in sysmodule_internal.cpp. */

#include <retro_common_api.h>

#include "common/types.h"

RETRO_BEGIN_DECLS

enum OrbisPngEncAttribute { ORBIS_PNG_ENC_ATTRIBUTE_NONE = 0 };

/* OrbisPngEncEncodeParam.color_space (u16) */
enum OrbisPngEncColorSpace {
    ORBIS_PNG_ENC_COLOR_SPACE_RGB = 3,
    ORBIS_PNG_ENC_COLOR_SPACE_RGBA = 19
};

/* OrbisPngEncEncodeParam.pixel_format (u16) */
enum OrbisPngEncPixelFormat {
    ORBIS_PNG_ENC_PIXEL_FORMAT_R8G8B8A8 = 0,
    ORBIS_PNG_ENC_PIXEL_FORMAT_B8G8R8A8 = 1
};

/* OrbisPngEncEncodeParam.filter_type bits (u16) */
enum OrbisPngEncFilterType {
    ORBIS_PNG_ENC_FILTER_NONE = 0,
    ORBIS_PNG_ENC_FILTER_SUB = 1,
    ORBIS_PNG_ENC_FILTER_UP = 2,
    ORBIS_PNG_ENC_FILTER_AVERAGE = 4,
    ORBIS_PNG_ENC_FILTER_PAETH = 8,
    ORBIS_PNG_ENC_FILTER_ALL = 15
};

typedef struct OrbisPngEncCreateParam {
    u32 this_size;
    u32 attribute;
    u32 max_image_width;
    u32 max_filter_number;
} OrbisPngEncCreateParam;

typedef struct OrbisPngEncEncodeParam {
    const u8* image_mem_addr;
    u8* png_mem_addr;
    u32 image_mem_size;
    u32 png_mem_size;
    u32 image_width;
    u32 image_height;
    u32 image_pitch;
    u16 pixel_format; /* OrbisPngEncPixelFormat */
    u16 color_space;  /* OrbisPngEncColorSpace */
    u16 bit_depth;
    u16 clut_number;
    u16 filter_type;
    u16 compression_level;
} OrbisPngEncEncodeParam;

typedef struct OrbisPngEncOutputInfo {
    u32 data_size;
    u32 processed_height;
} OrbisPngEncOutputInfo;

typedef void* OrbisPngEncHandle;

s32 PS4_SYSV_ABI scePngEncCreate(const OrbisPngEncCreateParam* param, void* memoryAddress,
                                 u32 memorySize, OrbisPngEncHandle* handle);
s32 PS4_SYSV_ABI scePngEncDelete(OrbisPngEncHandle handle);
s32 PS4_SYSV_ABI scePngEncEncode(OrbisPngEncHandle handle, const OrbisPngEncEncodeParam* param,
                                 OrbisPngEncOutputInfo* outputInfo);
s32 PS4_SYSV_ABI scePngEncQueryMemorySize(const OrbisPngEncCreateParam* param);

RETRO_END_DECLS

#endif
