/* SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <stddef.h>
#include <string.h>

#include <boolean.h>
#include <formats/rjpeg_encode.h>

#include "common/c_bridge.h"
#include "common/enum_names.h"
#include "core/libraries/error_codes.h"
#include "core/libraries/jpeg/jpeg_error.h"
#include "core/libraries/jpeg/jpegenc.h"

#define ORBIS_JPEG_ENC_MINIMUM_MEMORY_SIZE ((int32_t)0x800)
#define ORBIS_JPEG_ENC_MAX_IMAGE_DIMENSION ((uint32_t)0xFFFF)
#define ORBIS_JPEG_ENC_MAX_IMAGE_PITCH ((uint32_t)0xFFFFFFF)
#define ORBIS_JPEG_ENC_MAX_IMAGE_SIZE ((uint32_t)0x7FFFFFFF)

static s32 ValidateJpegEncCreateParam(const OrbisJpegEncCreateParam* param) {
    if (!param) {
        return ORBIS_JPEG_ENC_ERROR_INVALID_ADDR;
    }
    if (param->size != sizeof(OrbisJpegEncCreateParam)) {
        return ORBIS_JPEG_ENC_ERROR_INVALID_SIZE;
    }
    if (param->attr != ORBIS_JPEG_ENC_ATTRIBUTE_NONE) {
        return ORBIS_JPEG_ENC_ERROR_INVALID_PARAM;
    }
    return ORBIS_OK;
}

static s32 ValidateJpegEncMemory(const void* memory, const u32 memory_size) {
    if (!memory) {
        return ORBIS_JPEG_ENC_ERROR_INVALID_ADDR;
    }
    if (memory_size < ORBIS_JPEG_ENC_MINIMUM_MEMORY_SIZE) {
        return ORBIS_JPEG_ENC_ERROR_INVALID_SIZE;
    }
    return ORBIS_OK;
}

static s32 ValidateJpegEncEncodeParam(const OrbisJpegEncEncodeParam* param) {
    u32 calculated_size;

    /* Validate addresses */
    if (!param) {
        return ORBIS_JPEG_ENC_ERROR_INVALID_ADDR;
    }
    if (!param->image || (param->pixel_format != ORBIS_JPEG_ENC_PIXEL_FORMAT_Y8 &&
                          !(((VAddr)param->image & 3) == 0))) {
        return ORBIS_JPEG_ENC_ERROR_INVALID_ADDR;
    }
    if (!param->jpeg) {
        return ORBIS_JPEG_ENC_ERROR_INVALID_ADDR;
    }

    /* Validate sizes */
    if (param->image_size == 0 || param->jpeg_size == 0) {
        return ORBIS_JPEG_ENC_ERROR_INVALID_SIZE;
    }

    /* Validate parameters */
    if (param->image_width > ORBIS_JPEG_ENC_MAX_IMAGE_DIMENSION ||
        param->image_height > ORBIS_JPEG_ENC_MAX_IMAGE_DIMENSION) {
        return ORBIS_JPEG_ENC_ERROR_INVALID_PARAM;
    }
    if (param->image_pitch == 0 || param->image_pitch > ORBIS_JPEG_ENC_MAX_IMAGE_PITCH ||
        (param->pixel_format != ORBIS_JPEG_ENC_PIXEL_FORMAT_Y8 &&
         !((param->image_pitch & 3) == 0))) {
        return ORBIS_JPEG_ENC_ERROR_INVALID_PARAM;
    }
    calculated_size = param->image_height * param->image_pitch;
    if (calculated_size > ORBIS_JPEG_ENC_MAX_IMAGE_SIZE || calculated_size > param->image_size) {
        return ORBIS_JPEG_ENC_ERROR_INVALID_PARAM;
    }
    if (param->encode_mode != ORBIS_JPEG_ENC_ENCODE_MODE_NORMAL &&
        param->encode_mode != ORBIS_JPEG_ENC_ENCODE_MODE_MJPEG) {
        return ORBIS_JPEG_ENC_ERROR_INVALID_PARAM;
    }
    if (param->color_space != ORBIS_JPEG_ENC_COLOR_SPACE_YCC &&
        param->color_space != ORBIS_JPEG_ENC_COLOR_SPACE_GRAYSCALE) {
        return ORBIS_JPEG_ENC_ERROR_INVALID_PARAM;
    }
    if (param->sampling_type != ORBIS_JPEG_ENC_SAMPLING_TYPE_FULL &&
        param->sampling_type != ORBIS_JPEG_ENC_SAMPLING_TYPE_422 &&
        param->sampling_type != ORBIS_JPEG_ENC_SAMPLING_TYPE_420) {
        return ORBIS_JPEG_ENC_ERROR_INVALID_PARAM;
    }
    if (param->restart_interval > ORBIS_JPEG_ENC_MAX_IMAGE_DIMENSION) {
        return ORBIS_JPEG_ENC_ERROR_INVALID_PARAM;
    }
    switch (param->pixel_format) {
    case ORBIS_JPEG_ENC_PIXEL_FORMAT_R8G8B8A8:
    case ORBIS_JPEG_ENC_PIXEL_FORMAT_B8G8R8A8:
        if (param->image_pitch >> 2 < param->image_width ||
            param->color_space != ORBIS_JPEG_ENC_COLOR_SPACE_YCC ||
            param->sampling_type == ORBIS_JPEG_ENC_SAMPLING_TYPE_FULL) {
            return ORBIS_JPEG_ENC_ERROR_INVALID_PARAM;
        }
        break;
    case ORBIS_JPEG_ENC_PIXEL_FORMAT_Y8U8Y8V8:
        if (param->image_pitch >> 1 < ((param->image_width + 1) & ~1u) ||
            param->color_space != ORBIS_JPEG_ENC_COLOR_SPACE_YCC ||
            param->sampling_type == ORBIS_JPEG_ENC_SAMPLING_TYPE_FULL) {
            return ORBIS_JPEG_ENC_ERROR_INVALID_PARAM;
        }
        break;
    case ORBIS_JPEG_ENC_PIXEL_FORMAT_Y8:
        if (param->image_pitch < param->image_width ||
            param->color_space != ORBIS_JPEG_ENC_COLOR_SPACE_GRAYSCALE ||
            param->sampling_type != ORBIS_JPEG_ENC_SAMPLING_TYPE_FULL) {
            return ORBIS_JPEG_ENC_ERROR_INVALID_PARAM;
        }
        break;
    default:
        return ORBIS_JPEG_ENC_ERROR_INVALID_PARAM;
    }

    return ORBIS_OK;
}

static s32 ValidateJpecEngHandle(OrbisJpegEncHandle handle) {
    if (!handle || !(((VAddr)handle & 0x1F) == 0) || handle->handle != handle) {
        return ORBIS_JPEG_ENC_ERROR_INVALID_HANDLE;
    }
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceJpegEncCreate(const OrbisJpegEncCreateParam* param, void* memory,
                                  const u32 memory_size, OrbisJpegEncHandle* handle) {
    s32 param_ret;
    s32 memory_ret;
    OrbisJpegEncHandleInternal* handle_internal;
    param_ret = ValidateJpegEncCreateParam(param);
    if (param_ret != ORBIS_OK) {
        c_log(C_LOG_LIB_JPEG, C_LOG_LEVEL_ERROR, "sceJpegEncCreate", "Invalid create param");
        return param_ret;
    }
    memory_ret = ValidateJpegEncMemory(memory, memory_size);
    if (memory_ret != ORBIS_OK) {
        c_log(C_LOG_LIB_JPEG, C_LOG_LEVEL_ERROR, "sceJpegEncCreate", "Invalid memory");
        return memory_ret;
    }
    if (!handle) {
        c_log(C_LOG_LIB_JPEG, C_LOG_LEVEL_ERROR, "sceJpegEncCreate", "Invalid handle output");
        return ORBIS_JPEG_ENC_ERROR_INVALID_ADDR;
    }

    handle_internal = (OrbisJpegEncHandleInternal*)(((VAddr)memory + 0x1F) & ~(VAddr)0x1F);
    handle_internal->handle = handle_internal;
    handle_internal->handle_size = sizeof(OrbisJpegEncHandleInternal*);
    *handle = handle_internal;

    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceJpegEncDelete(OrbisJpegEncHandle handle) {
    s32 handle_ret;
    handle_ret = ValidateJpecEngHandle(handle);
    if (handle_ret != ORBIS_OK) {
        c_log(C_LOG_LIB_JPEG, C_LOG_LEVEL_ERROR, "sceJpegEncDelete", "Invalid handle");
        return handle_ret;
    }
    handle->handle = NULL;
    return ORBIS_OK;
}

/* The game's image, read a block at a time as the encoder asks: no planes made of it */
typedef struct jpeg_source {
    const u8* image;
    u32 width;
    u32 height;
    u32 pitch;
    u16 pixel_format;
    bool chroma_rows_halved; /* 4:2:0 */
} jpeg_source;

/* JFIF's full-range BT.601, in 16 fractional bits */
#define JPEG_Y(r, g, b) (19595 * (s32)(r) + 38470 * (s32)(g) + 7471 * (s32)(b))
#define JPEG_CB(r, g, b) (-11059 * (s32)(r) - 21709 * (s32)(g) + 32768 * (s32)(b))
#define JPEG_CR(r, g, b) (32768 * (s32)(r) - 27439 * (s32)(g) - 5329 * (s32)(b))

static u8 jpeg_clamp(s32 v) {
    return (u8)(v < 0 ? 0 : v > 255 ? 255 : v);
}

/* A pixel's R, G, B from an RGBA or BGRA image */
static void jpeg_rgb(const jpeg_source* s, u32 x, u32 y, s32* r, s32* g, s32* b) {
    const u8* p = s->image + (size_t)y * s->pitch + (size_t)x * 4;
    if (s->pixel_format == ORBIS_JPEG_ENC_PIXEL_FORMAT_B8G8R8A8) {
        *r = p[2];
        *g = p[1];
        *b = p[0];
    } else {
        *r = p[0];
        *g = p[1];
        *b = p[2];
    }
}

static void jpeg_block(void* user, unsigned c, unsigned x, unsigned y, uint8_t* px) {
    const jpeg_source* s = (const jpeg_source*)user;
    const u32 cw = (s->width + 1) / 2;
    const u32 ch = s->chroma_rows_halved ? (s->height + 1) / 2 : s->height;
    unsigned i, j;
    for (j = 0; j < 8; j++) {
        for (i = 0; i < 8; i++) {
            u8 v;
            if (c == 0) {
                /* Y of the pixel, the picture's edge repeated */
                const u32 sx = x + i < s->width ? x + i : s->width - 1;
                const u32 sy = y + j < s->height ? y + j : s->height - 1;
                const u8* row = s->image + (size_t)sy * s->pitch;
                if (s->pixel_format == ORBIS_JPEG_ENC_PIXEL_FORMAT_Y8) {
                    v = row[sx];
                } else if (s->pixel_format == ORBIS_JPEG_ENC_PIXEL_FORMAT_Y8U8Y8V8) {
                    v = row[(sx / 2) * 4 + (sx & 1) * 2];
                } else {
                    s32 r, g, b;
                    jpeg_rgb(s, sx, sy, &r, &g, &b);
                    v = jpeg_clamp((JPEG_Y(r, g, b) + 32768) >> 16);
                }
            } else {
                /* Cb or Cr of a chroma sample: the pixels it covers averaged, two across, two
                 * down under 4:2:0, the chroma plane's edge repeated */
                const u32 cx = x + i < cw ? x + i : cw - 1;
                const u32 cy = y + j < ch ? y + j : ch - 1;
                const u32 y0 = s->chroma_rows_halved ? cy * 2 : cy;
                const u32 y1 = s->chroma_rows_halved && y0 + 1 < s->height ? y0 + 1 : y0;
                if (s->pixel_format == ORBIS_JPEG_ENC_PIXEL_FORMAT_Y8U8Y8V8) {
                    const size_t at = (size_t)cx * 4 + (c == 1 ? 1 : 3);
                    const u32 sum =
                        s->image[(size_t)y0 * s->pitch + at] + s->image[(size_t)y1 * s->pitch + at];
                    v = (u8)((sum + 1) / 2);
                } else {
                    const u32 x0 = cx * 2;
                    const u32 x1 = x0 + 1 < s->width ? x0 + 1 : x0;
                    s32 sum = 0, r, g, b;
                    unsigned k;
                    for (k = 0; k < 4; k++) {
                        jpeg_rgb(s, k & 1 ? x1 : x0, k & 2 ? y1 : y0, &r, &g, &b);
                        sum += c == 1 ? JPEG_CB(r, g, b) : JPEG_CR(r, g, b);
                    }
                    /* the mean of four, 128 added, rounded */
                    v = jpeg_clamp((sum + (128 << 18) + (1 << 17)) >> 18);
                }
            }
            px[j * 8 + i] = v;
        }
    }
}

/* The game's compression ratio as the encoder's quality: 1 the least compression (quality
 * 100) to 255 the most (quality 1), 0 as 1 */
static unsigned jpeg_quality(u8 compression_ratio) {
    const unsigned ratio = compression_ratio ? compression_ratio : 1;
    return 100 - ((ratio - 1) * 99) / 254;
}

s32 PS4_SYSV_ABI sceJpegEncEncode(OrbisJpegEncHandle handle, const OrbisJpegEncEncodeParam* param,
                                  OrbisJpegEncOutputInfo* output_info) {
    s32 handle_ret;
    s32 param_ret;
    rjpeg_encoder_t encoder;
    struct rjpeg_encode_source src;
    jpeg_source image;
    size_t size;

    handle_ret = ValidateJpecEngHandle(handle);
    if (handle_ret != ORBIS_OK) {
        c_log(C_LOG_LIB_JPEG, C_LOG_LEVEL_ERROR, "sceJpegEncEncode", "Invalid handle");
        return handle_ret;
    }
    param_ret = ValidateJpegEncEncodeParam(param);
    if (param_ret != ORBIS_OK) {
        c_log(C_LOG_LIB_JPEG, C_LOG_LEVEL_ERROR, "sceJpegEncEncode", "Invalid encode param");
        return param_ret;
    }

    c_log(C_LOG_LIB_JPEG, C_LOG_LEVEL_TRACE, "sceJpegEncEncode",
          "image_size = %u , jpeg_size = %u , image_width = %u , image_height = %u , "
          "image_pitch = %u , pixel_format = %s , encode_mode = %s , color_space = %s , "
          "sampling_type = %s , compression_ratio = %u , restart_interval = %d",
          param->image_size, param->jpeg_size, param->image_width, param->image_height,
          param->image_pitch,
          enum_name(&enum_names_OrbisJpegEncEncodeParamPixelFormat, (s64)param->pixel_format),
          enum_name(&enum_names_OrbisJpengEncEncodeParamEncodeMode, (s64)param->encode_mode),
          enum_name(&enum_names_OrbisJpengEncEncodeParamColorSpace, (s64)param->color_space),
          enum_name(&enum_names_OrbisJpengEncEncodeParamSamplingType, (s64)param->sampling_type),
          (unsigned)param->compression_ratio, param->restart_interval);

    /* libretro-common's rjpeg encodes from the game's image straight into its buffer. The
     * tables are made on the stack for this picture: nothing is allocated. */
    image.image = (const u8*)param->image;
    image.width = param->image_width;
    image.height = param->image_height;
    image.pitch = param->image_pitch;
    image.pixel_format = param->pixel_format;
    image.chroma_rows_halved = param->sampling_type == ORBIS_JPEG_ENC_SAMPLING_TYPE_420;

    memset(&src, 0, sizeof(src));
    src.block = jpeg_block;
    src.user = &image;
    src.sampling = image.chroma_rows_halved ? RJPEG_ENCODE_420 : RJPEG_ENCODE_422;
    src.grey = param->color_space == ORBIS_JPEG_ENC_COLOR_SPACE_GRAYSCALE;
    src.restart_interval = param->restart_interval > 0 ? (unsigned)param->restart_interval : 0;

    rjpeg_encoder_init(&encoder, jpeg_quality(param->compression_ratio));
    size = rjpeg_encode_blocks(&encoder, (uint8_t*)param->jpeg, param->jpeg_size,
                               param->image_width, param->image_height, &src);
    if (size == 0) {
        c_log(C_LOG_LIB_JPEG, C_LOG_LEVEL_ERROR, "sceJpegEncEncode",
              "A %ux%u picture does not fit in %u bytes", param->image_width, param->image_height,
              param->jpeg_size);
        return ORBIS_JPEG_ENC_ERROR_INVALID_SIZE;
    }

    if (output_info) {
        output_info->size = (u32)size;
        output_info->height = param->image_height;
    }
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceJpegEncQueryMemorySize(const OrbisJpegEncCreateParam* param) {
    s32 param_ret;
    param_ret = ValidateJpegEncCreateParam(param);
    if (param_ret != ORBIS_OK) {
        c_log(C_LOG_LIB_JPEG, C_LOG_LEVEL_ERROR, "sceJpegEncQueryMemorySize",
              "Invalid create param");
        return param_ret;
    }
    return ORBIS_JPEG_ENC_MINIMUM_MEMORY_SIZE;
}
