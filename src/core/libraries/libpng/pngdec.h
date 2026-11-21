/* SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef SHADPS4_LIBRARIES_PNGDEC_H
#define SHADPS4_LIBRARIES_PNGDEC_H

/* libScePngDec, decoded by libretro-common's rpng. Registered in sysmodule_internal.cpp. */

#include <retro_common_api.h>

#include "common/types.h"

RETRO_BEGIN_DECLS

/* OrbisPngDecImageInfo.color_space (u16) */
enum OrbisPngDecColorSpace {
    ORBIS_PNG_DEC_COLOR_SPACE_GRAYSCALE = 2,
    ORBIS_PNG_DEC_COLOR_SPACE_RGB = 3,
    ORBIS_PNG_DEC_COLOR_SPACE_CLUT = 4,
    ORBIS_PNG_DEC_COLOR_SPACE_GRAYSCALE_ALPHA = 18,
    ORBIS_PNG_DEC_COLOR_SPACE_RGBA = 19
};

/* OrbisPngDecImageInfo.image_flag bits (u32) */
#define ORBIS_PNG_DEC_IMAGE_FLAG_NONE 0u
#define ORBIS_PNG_DEC_IMAGE_FLAG_ADAM7_INTERLACE 1u
#define ORBIS_PNG_DEC_IMAGE_FLAG_TRNS_CHUNK_EXIST 2u

/* OrbisPngDecDecodeParam.pixel_format (u16) */
enum OrbisPngDecPixelFormat {
    ORBIS_PNG_DEC_PIXEL_FORMAT_R8G8B8A8 = 0,
    ORBIS_PNG_DEC_PIXEL_FORMAT_B8G8R8A8 = 1
};

enum OrbisPngDecAttribute {
    ORBIS_PNG_DEC_ATTRIBUTE_NONE = 0,
    ORBIS_PNG_DEC_ATTRIBUTE_BIT_DEPTH_16
};

typedef struct OrbisPngDecParseParam {
    const u8* png_mem_addr;
    u32 png_mem_size;
    u32 reserved;
} OrbisPngDecParseParam;

typedef struct OrbisPngDecImageInfo {
    u32 image_width;
    u32 image_height;
    u16 color_space; /* OrbisPngDecColorSpace */
    u16 bit_depth;
    u32 image_flag; /* ORBIS_PNG_DEC_IMAGE_FLAG_* */
} OrbisPngDecImageInfo;

typedef struct OrbisPngDecCreateParam {
    u32 this_size;
    u32 attribute;
    u32 max_image_width;
} OrbisPngDecCreateParam;

typedef void* OrbisPngDecHandle;

typedef struct OrbisPngDecDecodeParam {
    const u8* png_mem_addr;
    u8* image_mem_addr;
    u32 png_mem_size;
    u32 image_mem_size;
    u16 pixel_format; /* OrbisPngDecPixelFormat */
    u16 alpha_value;
    u32 image_pitch;
} OrbisPngDecDecodeParam;

s32 PS4_SYSV_ABI scePngDecCreate(const OrbisPngDecCreateParam* param, void* memoryAddress,
                                 u32 memorySize, OrbisPngDecHandle* handle);
s32 PS4_SYSV_ABI scePngDecDecode(OrbisPngDecHandle handle, const OrbisPngDecDecodeParam* param,
                                 OrbisPngDecImageInfo* imageInfo);
s32 PS4_SYSV_ABI scePngDecDecodeWithInputControl(void);
s32 PS4_SYSV_ABI scePngDecDelete(OrbisPngDecHandle handle);
s32 PS4_SYSV_ABI scePngDecParseHeader(const OrbisPngDecParseParam* param,
                                      OrbisPngDecImageInfo* imageInfo);
s32 PS4_SYSV_ABI scePngDecQueryMemorySize(const OrbisPngDecCreateParam* param);

RETRO_END_DECLS

#endif
