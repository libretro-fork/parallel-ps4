/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef SHADPS4_LIBRARIES_JPEGDEC_H
#define SHADPS4_LIBRARIES_JPEGDEC_H

/* libSceJpegDec, decoded by libretro-common's rjpeg into the game's buffer. The layouts, the
 * argument checks and the memory size follow upstream shadPS4's. Registered in
 * sysmodule_internal.cpp. */

#include <retro_common_api.h>

#include "common/types.h"

RETRO_BEGIN_DECLS

/* OrbisJpegDecDecodeParam.pixel_format (u16) */
enum OrbisJpegDecPixelFormat {
    ORBIS_JPEG_DEC_PIXEL_FORMAT_R8G8B8A8 = 0,
    ORBIS_JPEG_DEC_PIXEL_FORMAT_B8G8R8A8 = 1,
    ORBIS_JPEG_DEC_PIXEL_FORMAT_Y8U8Y8V8 = 10,
    ORBIS_JPEG_DEC_PIXEL_FORMAT_Y8 = 11
};

/* The decode_mode fields (u16 in the parameters) */
enum OrbisJpegDecDecodeMode {
    ORBIS_JPEG_DEC_DECODE_MODE_NORMAL = 0,
    ORBIS_JPEG_DEC_DECODE_MODE_MJPEG = 1
};

/* OrbisJpegDecImageInfo.color_space (u16) */
enum OrbisJpegDecColorSpace {
    ORBIS_JPEG_DEC_COLOR_SPACE_UNKNOWN = 0,
    ORBIS_JPEG_DEC_COLOR_SPACE_YCC = 1,
    ORBIS_JPEG_DEC_COLOR_SPACE_GRAYSCALE = 2
};

/* The downscale_ratio fields (u16): 1, 2, 4 or 8 */

typedef struct OrbisJpegDecHandleInternal {
    struct OrbisJpegDecHandleInternal* handle;
    u32 handle_size;
    u32 decode_mode;
    u32 max_width;
    u32 stride_or_aux;
} OrbisJpegDecHandleInternal;
typedef char
    OrbisJpegDecHandleInternal_size_check[(sizeof(OrbisJpegDecHandleInternal) == 0x18) ? 1 : -1];

typedef OrbisJpegDecHandleInternal* OrbisJpegDecHandle;

typedef struct OrbisJpegDecCreateParam {
    u32 size;
    u32 decode_mode;
    u32 max_width;
} OrbisJpegDecCreateParam;
typedef char OrbisJpegDecCreateParam_size_check[(sizeof(OrbisJpegDecCreateParam) == 0xc) ? 1 : -1];

typedef struct OrbisJpegDecParseParam {
    const void* jpeg_addr;
    u32 jpeg_size;
    u16 decode_mode;     /* OrbisJpegDecDecodeMode */
    u16 downscale_ratio; /* 1, 2, 4 or 8 */
} OrbisJpegDecParseParam;
typedef char OrbisJpegDecParseParam_size_check[(sizeof(OrbisJpegDecParseParam) == 0x10) ? 1 : -1];

typedef struct OrbisJpegDecImageInfo {
    u32 image_width;
    u32 image_height;
    u16 color_space; /* OrbisJpegDecColorSpace */
    u16 num_components;
    u8 component_id[4];
    u8 sampling_factor[4];
    u32 decode_mode;
    u32 required_decode_mode;
    u32 output_width;
    u32 output_height;
} OrbisJpegDecImageInfo;
typedef char OrbisJpegDecImageInfo_size_check[(sizeof(OrbisJpegDecImageInfo) == 0x24) ? 1 : -1];

typedef struct OrbisJpegDecDecodeParam {
    const void* jpeg_addr;
    void* image_addr;
    void* aux_addr;
    u32 jpeg_size;
    u32 image_size;
    u32 aux_size;
    u16 decode_mode;     /* OrbisJpegDecDecodeMode */
    u16 downscale_ratio; /* 1, 2, 4 or 8 */
    u16 pixel_format;    /* OrbisJpegDecPixelFormat */
    u16 alpha_value;
    u32 image_pitch;
} OrbisJpegDecDecodeParam;
typedef char OrbisJpegDecDecodeParam_size_check[(sizeof(OrbisJpegDecDecodeParam) == 0x30) ? 1 : -1];

s32 PS4_SYSV_ABI sceJpegDecQueryMemorySize(const OrbisJpegDecCreateParam* param);
s32 PS4_SYSV_ABI sceJpegDecCreate(const OrbisJpegDecCreateParam* param, void* memory,
                                  u32 memory_size, OrbisJpegDecHandle* handle);
s32 PS4_SYSV_ABI sceJpegDecDelete(OrbisJpegDecHandle handle);
s32 PS4_SYSV_ABI sceJpegDecParseHeader(const OrbisJpegDecParseParam* param,
                                       OrbisJpegDecImageInfo* image_info);
s32 PS4_SYSV_ABI sceJpegDecDecode(OrbisJpegDecHandle handle, const OrbisJpegDecDecodeParam* param,
                                  OrbisJpegDecImageInfo* image_info);
s32 PS4_SYSV_ABI sceJpegDecDecodeWithInputControl(OrbisJpegDecHandle handle,
                                                  const OrbisJpegDecDecodeParam* param,
                                                  OrbisJpegDecImageInfo* image_info);

RETRO_END_DECLS

#endif
