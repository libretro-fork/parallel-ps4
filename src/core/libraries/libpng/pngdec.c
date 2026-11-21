/* SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <stddef.h>
#include <string.h>

#include <boolean.h>
#include <formats/image.h>
#include <formats/rpng.h>

#include "common/c_bridge.h"
#include "core/libraries/error_codes.h"
#include "core/libraries/libpng/pngdec.h"
#include "core/libraries/libpng/pngdec_error.h"

/* The handle's memory, as the game sizes it with scePngDecQueryMemorySize. A decode needs
 * nothing kept between calls: libretro-common's rpng (formats/rpng.h) reads the file in place
 * and decodes straight into the game's image buffer. */
typedef struct png_handler {
    u64 reserved[2];
} png_handler;

static u16 map_png_color(int color) {
    switch (color) {
    case 0:
        return ORBIS_PNG_DEC_COLOR_SPACE_GRAYSCALE;
    case 4:
        return ORBIS_PNG_DEC_COLOR_SPACE_GRAYSCALE_ALPHA;
    case 3:
        return ORBIS_PNG_DEC_COLOR_SPACE_CLUT;
    case 2:
        return ORBIS_PNG_DEC_COLOR_SPACE_RGB;
    case 6:
        return ORBIS_PNG_DEC_COLOR_SPACE_RGBA;
    }
    c_unreachable(__FILE__, __LINE__, "map_png_color", "unknown png color type");
    return 0;
}

/* The file's chunks walked, so its header is known. False when it is not a PNG. */
static bool read_png(rpng_t* rpng, const void* data, u32 size, struct rpng_info* info) {
    if (!rpng || !rpng_set_buf_ptr(rpng, (void*)data, size) || !rpng_start(rpng)) {
        return false;
    }
    while (rpng_iterate_image(rpng)) {
    }
    return rpng_get_info(rpng, info);
}

static void fill_image_info(const struct rpng_info* info, OrbisPngDecImageInfo* image_info) {
    image_info->image_width = info->width;
    image_info->image_height = info->height;
    image_info->color_space = map_png_color(info->color_type);
    image_info->bit_depth = (u16)info->depth;
    image_info->image_flag = ORBIS_PNG_DEC_IMAGE_FLAG_NONE;
    if (info->interlace == 1) {
        image_info->image_flag |= ORBIS_PNG_DEC_IMAGE_FLAG_ADAM7_INTERLACE;
    }
    if (info->has_trns) {
        image_info->image_flag |= ORBIS_PNG_DEC_IMAGE_FLAG_TRNS_CHUNK_EXIST;
    }
}

s32 PS4_SYSV_ABI scePngDecCreate(const OrbisPngDecCreateParam* param, void* memoryAddress,
                                 u32 memorySize, OrbisPngDecHandle* handle) {
    if (param == NULL || param->attribute > 1) {
        c_log(C_LOG_LIB_PNG, C_LOG_LEVEL_ERROR, "scePngDecCreate", "Invalid param!");
        return ORBIS_PNG_DEC_ERROR_INVALID_PARAM;
    }
    if (memoryAddress == NULL) {
        c_log(C_LOG_LIB_PNG, C_LOG_LEVEL_ERROR, "scePngDecCreate", "Invalid memory address!");
        return ORBIS_PNG_DEC_ERROR_INVALID_ADDR;
    }
    if (param->max_image_width - 1 > 1000000) {
        c_log(C_LOG_LIB_PNG, C_LOG_LEVEL_ERROR, "scePngDecCreate", "Invalid size! width = %u",
              param->max_image_width);
        return ORBIS_PNG_DEC_ERROR_INVALID_SIZE;
    }
    *handle = (png_handler*)memoryAddress;
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI scePngDecDecode(OrbisPngDecHandle handle, const OrbisPngDecDecodeParam* param,
                                 OrbisPngDecImageInfo* imageInfo) {
    rpng_t* rpng;
    struct rpng_info info;
    u32 width;
    u32 height;
    u64 row_bytes;
    u64 pitch;
    u32* image;
    bool rgba;
    bool has_alpha;
    void* out = NULL;
    unsigned out_width = 0;
    unsigned out_height = 0;
    int ret;

    if (handle == NULL) {
        c_log(C_LOG_LIB_PNG, C_LOG_LEVEL_ERROR, "scePngDecDecode", "invalid handle!");
        return ORBIS_PNG_DEC_ERROR_INVALID_HANDLE;
    }
    if (param == NULL) {
        c_log(C_LOG_LIB_PNG, C_LOG_LEVEL_ERROR, "scePngDecDecode", "Invalid param!");
        return ORBIS_PNG_DEC_ERROR_INVALID_PARAM;
    }
    if (param->png_mem_addr == NULL || param->image_mem_addr == NULL) {
        c_log(C_LOG_LIB_PNG, C_LOG_LEVEL_ERROR, "scePngDecDecode", "invalid image address!");
        return ORBIS_PNG_DEC_ERROR_INVALID_ADDR;
    }
    c_log(C_LOG_LIB_PNG, C_LOG_LEVEL_TRACE, "scePngDecDecode",
          "pngMemSize = %u , imageMemSize = %u , pixelFormat = %d , alphaValue = %u , "
          "imagePitch = %u",
          param->png_mem_size, param->image_mem_size, (int)param->pixel_format,
          (unsigned)param->alpha_value, param->image_pitch);

    rpng = rpng_alloc();
    memset(&info, 0, sizeof(info));
    if (!read_png(rpng, param->png_mem_addr, param->png_mem_size, &info)) {
        rpng_free(rpng);
        c_log(C_LOG_LIB_PNG, C_LOG_LEVEL_ERROR, "scePngDecDecode",
              "Memory doesn't contain a valid png file");
        return ORBIS_PNG_DEC_ERROR_INVALID_DATA;
    }
    if (imageInfo != NULL) {
        fill_image_info(&info, imageInfo);
    }

    /* 32 bits a pixel, 8 bits a channel, rows image_pitch bytes apart, decoded straight into
     * the game's buffer. */
    width = info.width;
    height = info.height;
    row_bytes = (u64)width * 4;
    pitch = param->image_pitch > 0 ? param->image_pitch : row_bytes;
    if (pitch < row_bytes || pitch % 4 != 0 ||
        pitch * (height - 1) + row_bytes > param->image_mem_size) {
        rpng_free(rpng);
        c_log(C_LOG_LIB_PNG, C_LOG_LEVEL_ERROR, "scePngDecDecode",
              "Image buffer of %u bytes, pitch %llu, does not hold %ux%u", param->image_mem_size,
              (unsigned long long)pitch, width, height);
        return ORBIS_PNG_DEC_ERROR_INVALID_SIZE;
    }
    image = (u32*)param->image_mem_addr;
    rpng_set_output(rpng, image, (unsigned)(pitch / 4));
    rgba = param->pixel_format != ORBIS_PNG_DEC_PIXEL_FORMAT_B8G8R8A8;
    do {
        ret = rpng_process_image(rpng, &out, param->png_mem_size, &out_width, &out_height, rgba);
    } while (ret == IMAGE_PROCESS_NEXT);
    rpng_free(rpng);
    if (ret != IMAGE_PROCESS_END) {
        c_log(C_LOG_LIB_PNG, C_LOG_LEVEL_ERROR, "scePngDecDecode", "PNG decode failed");
        return ORBIS_PNG_DEC_ERROR_DECODE_ERROR;
    }

    /* An image without alpha takes the game's alpha value; rpng makes it opaque. */
    has_alpha = info.color_type == 4 || info.color_type == 6 || info.has_trns;
    if (!has_alpha && param->alpha_value != 0xFF) {
        u8* row = (u8*)image;
        u32 x, y;
        for (y = 0; y < height; y++, row += pitch) {
            for (x = 0; x < width; x++) {
                row[x * 4 + 3] = (u8)param->alpha_value;
            }
        }
    }

    return (width > 32767 || height > 32767) ? 0 : (s32)((width << 16) | height);
}

s32 PS4_SYSV_ABI scePngDecDecodeWithInputControl(void) {
    c_log(C_LOG_LIB_PNG, C_LOG_LEVEL_ERROR, "scePngDecDecodeWithInputControl", "(STUBBED)called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI scePngDecDelete(OrbisPngDecHandle handle) {
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI scePngDecParseHeader(const OrbisPngDecParseParam* param,
                                      OrbisPngDecImageInfo* imageInfo) {
    rpng_t* rpng;
    struct rpng_info info;
    bool ok;
    if (param == NULL) {
        c_log(C_LOG_LIB_PNG, C_LOG_LEVEL_ERROR, "scePngDecParseHeader", "Invalid param!");
        return ORBIS_PNG_DEC_ERROR_INVALID_PARAM;
    }

    rpng = rpng_alloc();
    memset(&info, 0, sizeof(info));
    ok = read_png(rpng, param->png_mem_addr, param->png_mem_size, &info);
    rpng_free(rpng);
    if (!ok) {
        c_log(C_LOG_LIB_PNG, C_LOG_LEVEL_ERROR, "scePngDecParseHeader",
              "Memory doesn't contain a valid png file");
        return ORBIS_PNG_DEC_ERROR_INVALID_DATA;
    }
    fill_image_info(&info, imageInfo);
    c_log(C_LOG_LIB_PNG, C_LOG_LEVEL_TRACE, "scePngDecParseHeader",
          "imageWidth = %u , imageHeight = %u , colorSpace = %d , bitDepth = %u , imageFlag = %d",
          imageInfo->image_width, imageInfo->image_height, (int)imageInfo->color_space,
          (unsigned)imageInfo->bit_depth, (int)imageInfo->image_flag);
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI scePngDecQueryMemorySize(const OrbisPngDecCreateParam* param) {
    if (param == NULL) {
        c_log(C_LOG_LIB_PNG, C_LOG_LEVEL_ERROR, "scePngDecQueryMemorySize", "Invalid param!");
        return ORBIS_PNG_DEC_ERROR_INVALID_PARAM;
    }
    if (param->attribute > 1) {
        c_log(C_LOG_LIB_PNG, C_LOG_LEVEL_ERROR, "scePngDecQueryMemorySize",
              "Invalid attribute! attribute = %u", param->attribute);
        return ORBIS_PNG_DEC_ERROR_INVALID_ADDR;
    }
    if (param->max_image_width - 1 > 1000000) {
        c_log(C_LOG_LIB_PNG, C_LOG_LEVEL_ERROR, "scePngDecQueryMemorySize",
              "Invalid size! width = %u", param->max_image_width);
        return ORBIS_PNG_DEC_ERROR_INVALID_SIZE;
    }
    return sizeof(png_handler);
}
