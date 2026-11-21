/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#include <boolean.h>
#include <formats/rpng.h>
#include <libretro.h>
#include <streams/interface_stream.h>

#include "common/c_bridge.h"
#include "core/libraries/error_codes.h"
#include "core/libraries/libpng/pngenc.h"
#include "core/libraries/libpng/pngenc_error.h"

/* The handle's memory, as the game sizes it with scePngEncQueryMemorySize. An encode needs
 * nothing kept between calls: libretro-common's rpng (formats/rpng.h) encodes from the game's
 * image straight into its output buffer. */
typedef struct png_handler {
    u64 reserved[2];
} png_handler;

s32 PS4_SYSV_ABI scePngEncCreate(const OrbisPngEncCreateParam* param, void* memoryAddress,
                                 u32 memorySize, OrbisPngEncHandle* handle) {
    if (param == NULL || param->attribute != 0) {
        c_log(C_LOG_LIB_PNG, C_LOG_LEVEL_ERROR, "scePngEncCreate", "Invalid param");
        return ORBIS_PNG_ENC_ERROR_INVALID_ADDR;
    }

    if (memoryAddress == NULL) {
        c_log(C_LOG_LIB_PNG, C_LOG_LEVEL_ERROR, "scePngEncCreate", "Invalid memory address");
        return ORBIS_PNG_ENC_ERROR_INVALID_ADDR;
    }

    if (param->max_image_width - 1 > 1000000) {
        c_log(C_LOG_LIB_PNG, C_LOG_LEVEL_ERROR, "scePngEncCreate", "Invalid Size, width = %u",
              param->max_image_width);
        return ORBIS_PNG_ENC_ERROR_INVALID_SIZE;
    }

    *handle = (png_handler*)memoryAddress;

    return ORBIS_OK;
}

s32 PS4_SYSV_ABI scePngEncDelete(OrbisPngEncHandle handle) {
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI scePngEncEncode(OrbisPngEncHandle handle, const OrbisPngEncEncodeParam* param,
                                 OrbisPngEncOutputInfo* outputInfo) {
    bool bgra;
    bool alpha;
    enum rpng_pixfmt format;
    u32 pitch;
    intfstream_t* out;
    bool ok;
    s64 size;

    c_log(C_LOG_LIB_PNG, C_LOG_LEVEL_TRACE, "scePngEncEncode",
          "called png addr = 0x%llx, image addr = 0x%llx, image size = %u",
          (unsigned long long)(uintptr_t)param->png_mem_addr,
          (unsigned long long)(uintptr_t)param->image_mem_addr, param->image_mem_size);

    if (handle == NULL) {
        c_log(C_LOG_LIB_PNG, C_LOG_LEVEL_ERROR, "scePngEncEncode", "Invalid handle");
        return ORBIS_PNG_ENC_ERROR_INVALID_HANDLE;
    }

    if (param == NULL) {
        c_log(C_LOG_LIB_PNG, C_LOG_LEVEL_ERROR, "scePngEncEncode", "Invalid param");
        return ORBIS_PNG_ENC_ERROR_INVALID_PARAM;
    }

    if (param->image_mem_addr == NULL || param->png_mem_addr == NULL) {
        c_log(C_LOG_LIB_PNG, C_LOG_LEVEL_ERROR, "scePngEncEncode",
              "Invalid input or output address");
        return ORBIS_PNG_ENC_ERROR_INVALID_ADDR;
    }

    if (param->png_mem_size == 0 || param->image_mem_size == 0 || param->image_height == 0 ||
        param->image_width == 0) {
        c_log(C_LOG_LIB_PNG, C_LOG_LEVEL_ERROR, "scePngEncEncode", "Invalid Size");
        return ORBIS_PNG_ENC_ERROR_INVALID_SIZE;
    }

    /* 8 bits a channel, RGBA or BGRA in memory, rows image_pitch bytes apart. rpng chooses
     * each row's filter by itself and compresses at its own level, so the game's filter and
     * level only change how small the file comes out, not what it decodes to. */
    if (param->bit_depth != 8) {
        c_log(C_LOG_LIB_PNG, C_LOG_LEVEL_WARNING, "scePngEncEncode",
              "PNG encode at bit depth %u: written at 8", (unsigned)param->bit_depth);
    }
    bgra = param->pixel_format == ORBIS_PNG_ENC_PIXEL_FORMAT_B8G8R8A8;
    alpha = param->color_space == ORBIS_PNG_ENC_COLOR_SPACE_RGBA;
    format = alpha ? (bgra ? RPNG_PIXFMT_ARGB32 : RPNG_PIXFMT_RGBA32)
                   : (bgra ? RPNG_PIXFMT_XRGB8888 : RPNG_PIXFMT_XBGR8888);
    pitch = param->image_pitch ? param->image_pitch : param->image_width * 4;

    out = intfstream_open_memory(param->png_mem_addr, RETRO_VFS_FILE_ACCESS_WRITE,
                                 RETRO_VFS_FILE_ACCESS_HINT_NONE, param->png_mem_size);
    if (!out) {
        return ORBIS_PNG_ENC_ERROR_FATAL;
    }
    ok = rpng_save_image_stream_fmt(param->image_mem_addr, out, param->image_width,
                                    param->image_height, (s32)pitch, format, NULL);
    size = intfstream_tell(out);
    intfstream_close(out);
    free(out);

    if (!ok) {
        if (size >= (s64)param->png_mem_size) {
            c_log(C_LOG_LIB_PNG, C_LOG_LEVEL_ERROR, "scePngEncEncode",
                  "Ran out of room to write PNG");
            return ORBIS_PNG_ENC_ERROR_DATA_OVERFLOW;
        }
        c_log(C_LOG_LIB_PNG, C_LOG_LEVEL_ERROR, "scePngEncEncode", "PNG encode failed");
        return ORBIS_PNG_ENC_ERROR_FATAL;
    }
    if (outputInfo != NULL) {
        outputInfo->data_size = (u32)size;
        outputInfo->processed_height = param->image_height;
    }
    return (s32)size;
}

s32 PS4_SYSV_ABI scePngEncQueryMemorySize(const OrbisPngEncCreateParam* param) {
    if (param == NULL) {
        c_log(C_LOG_LIB_PNG, C_LOG_LEVEL_ERROR, "scePngEncQueryMemorySize", "Invalid Address");
        return ORBIS_PNG_ENC_ERROR_INVALID_ADDR;
    }

    if (param->attribute != 0 || param->max_filter_number > 5) {
        c_log(C_LOG_LIB_PNG, C_LOG_LEVEL_ERROR, "scePngEncQueryMemorySize",
              "Invalid Param, attribute = %u, max_filter_number = %u", param->attribute,
              param->max_filter_number);
        return ORBIS_PNG_ENC_ERROR_INVALID_PARAM;
    }

    if (param->max_image_width - 1 > 1000000) {
        c_log(C_LOG_LIB_PNG, C_LOG_LEVEL_ERROR, "scePngEncQueryMemorySize",
              "Invalid Size, width = %u", param->max_image_width);
        return ORBIS_PNG_ENC_ERROR_INVALID_SIZE;
    }

    return sizeof(png_handler);
}
