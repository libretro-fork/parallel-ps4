/* SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef SHADPS4_LIBRARIES_JPEGENC_H
#define SHADPS4_LIBRARIES_JPEGENC_H

/* libSceJpegEnc, encoded by libretro-common's rjpeg straight from the game's image into its
 * buffer. Registered in sysmodule_internal.cpp. */

#include <retro_common_api.h>

#include "common/types.h"

RETRO_BEGIN_DECLS

/* OrbisJpegEncCreateParam.attr (u32) */
enum OrbisJpegEncCreateParamAttributes { ORBIS_JPEG_ENC_ATTRIBUTE_NONE = 0 };

/* OrbisJpegEncEncodeParam.pixel_format (u16) */
enum OrbisJpegEncEncodeParamPixelFormat {
    ORBIS_JPEG_ENC_PIXEL_FORMAT_R8G8B8A8 = 0,
    ORBIS_JPEG_ENC_PIXEL_FORMAT_B8G8R8A8 = 1,
    ORBIS_JPEG_ENC_PIXEL_FORMAT_Y8U8Y8V8 = 10,
    ORBIS_JPEG_ENC_PIXEL_FORMAT_Y8 = 11
};

/* OrbisJpegEncEncodeParam.encode_mode (u16) */
enum OrbisJpengEncEncodeParamEncodeMode {
    ORBIS_JPEG_ENC_ENCODE_MODE_NORMAL = 0,
    ORBIS_JPEG_ENC_ENCODE_MODE_MJPEG = 1
};

/* OrbisJpegEncEncodeParam.color_space (u16) */
enum OrbisJpengEncEncodeParamColorSpace {
    ORBIS_JPEG_ENC_COLOR_SPACE_YCC = 1,
    ORBIS_JPEG_ENC_COLOR_SPACE_GRAYSCALE = 2
};

/* OrbisJpegEncEncodeParam.sampling_type (u8) */
enum OrbisJpengEncEncodeParamSamplingType {
    ORBIS_JPEG_ENC_SAMPLING_TYPE_FULL = 0,
    ORBIS_JPEG_ENC_SAMPLING_TYPE_422 = 1,
    ORBIS_JPEG_ENC_SAMPLING_TYPE_420 = 2
};

typedef struct OrbisJpegEncHandleInternal {
    struct OrbisJpegEncHandleInternal* handle;
    u32 handle_size;
} OrbisJpegEncHandleInternal;
typedef char
    OrbisJpegEncHandleInternal_size_check[(sizeof(OrbisJpegEncHandleInternal) == 0x10) ? 1 : -1];

typedef OrbisJpegEncHandleInternal* OrbisJpegEncHandle;

typedef struct OrbisJpegEncCreateParam {
    u32 size;
    u32 attr; /* OrbisJpegEncCreateParamAttributes */
} OrbisJpegEncCreateParam;
typedef char OrbisJpegEncCreateParam_size_check[(sizeof(OrbisJpegEncCreateParam) == 0x8) ? 1 : -1];

typedef struct OrbisJpegEncEncodeParam {
    void* image;
    void* jpeg;
    u32 image_size;
    u32 jpeg_size;
    u32 image_width;
    u32 image_height;
    u32 image_pitch;
    u16 pixel_format; /* OrbisJpegEncEncodeParamPixelFormat */
    u16 encode_mode;  /* OrbisJpengEncEncodeParamEncodeMode */
    u16 color_space;  /* OrbisJpengEncEncodeParamColorSpace */
    u8 sampling_type; /* OrbisJpengEncEncodeParamSamplingType */
    u8 compression_ratio;
    s32 restart_interval;
} OrbisJpegEncEncodeParam;
typedef char OrbisJpegEncEncodeParam_size_check[(sizeof(OrbisJpegEncEncodeParam) == 0x30) ? 1 : -1];

typedef struct OrbisJpegEncOutputInfo {
    u32 size;
    u32 height;
} OrbisJpegEncOutputInfo;
typedef char OrbisJpegEncOutputInfo_size_check[(sizeof(OrbisJpegEncOutputInfo) == 0x8) ? 1 : -1];

s32 PS4_SYSV_ABI sceJpegEncCreate(const OrbisJpegEncCreateParam* param, void* memory,
                                  u32 memory_size, OrbisJpegEncHandle* handle);
s32 PS4_SYSV_ABI sceJpegEncDelete(OrbisJpegEncHandle handle);
s32 PS4_SYSV_ABI sceJpegEncEncode(OrbisJpegEncHandle handle, const OrbisJpegEncEncodeParam* param,
                                  OrbisJpegEncOutputInfo* output_info);
s32 PS4_SYSV_ABI sceJpegEncQueryMemorySize(const OrbisJpegEncCreateParam* param);

RETRO_END_DECLS

#endif
