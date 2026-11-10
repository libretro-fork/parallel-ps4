// SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <cstdlib>

#include <formats/rpng.h>
#include <libretro.h>
#include <streams/interface_stream.h>

#include "common/logging/log.h"
#include "core/libraries/error_codes.h"
#include "core/libraries/libpng/pngenc.h"
#include "core/libraries/libs.h"

#include "pngenc_error.h"

namespace Libraries::PngEnc {

// The handle's memory, as the game sizes it with scePngEncQueryMemorySize. An encode needs
// nothing kept between calls: libretro-common's rpng (formats/rpng.h) encodes from the game's
// image straight into its output buffer.
struct PngHandler {
    u64 reserved[2];
};

s32 PS4_SYSV_ABI scePngEncCreate(const OrbisPngEncCreateParam* param, void* memoryAddress,
                                 u32 memorySize, OrbisPngEncHandle* handle) {
    if (param == nullptr || param->attribute != 0) {
        LOG_ERROR(Lib_Png, "Invalid param");
        return ORBIS_PNG_ENC_ERROR_INVALID_ADDR;
    }

    if (memoryAddress == nullptr) {
        LOG_ERROR(Lib_Png, "Invalid memory address");
        return ORBIS_PNG_ENC_ERROR_INVALID_ADDR;
    }

    if (param->max_image_width - 1 > 1000000) {
        LOG_ERROR(Lib_Png, "Invalid Size, width = {}", param->max_image_width);
        return ORBIS_PNG_ENC_ERROR_INVALID_SIZE;
    }

    auto pngh = (PngHandler*)memoryAddress;
    *handle = pngh;

    return ORBIS_OK;
}

s32 PS4_SYSV_ABI scePngEncDelete(OrbisPngEncHandle handle) {
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI scePngEncEncode(OrbisPngEncHandle handle, const OrbisPngEncEncodeParam* param,
                                 OrbisPngEncOutputInfo* outputInfo) {
    LOG_TRACE(Lib_Png, "called png addr = {}, image addr = {}, image size = {}",
              (void*)param->png_mem_addr, (void*)param->image_mem_addr, param->image_mem_size);

    if (handle == nullptr) {
        LOG_ERROR(Lib_Png, "Invalid handle");
        return ORBIS_PNG_ENC_ERROR_INVALID_HANDLE;
    }

    if (param == nullptr) {
        LOG_ERROR(Lib_Png, "Invalid param");
        return ORBIS_PNG_ENC_ERROR_INVALID_PARAM;
    }

    if (param->image_mem_addr == nullptr || param->png_mem_addr == nullptr) {
        LOG_ERROR(Lib_Png, "Invalid input or output address");
        return ORBIS_PNG_ENC_ERROR_INVALID_ADDR;
    }

    if (param->png_mem_size == 0 || param->image_mem_size == 0 || param->image_height == 0 ||
        param->image_width == 0) {
        LOG_ERROR(Lib_Png, "Invalid Size");
        return ORBIS_PNG_ENC_ERROR_INVALID_SIZE;
    }

    // 8 bits a channel, RGBA or BGRA in memory, rows image_pitch bytes apart. rpng chooses
    // each row's filter by itself and compresses at its own level, so the game's filter and
    // level only change how small the file comes out, not what it decodes to.
    if (param->bit_depth != 8) {
        LOG_WARNING(Lib_Png, "PNG encode at bit depth {}: written at 8", param->bit_depth);
    }
    const bool bgra = param->pixel_format == OrbisPngEncPixelFormat::B8G8R8A8;
    const bool alpha = param->color_space == OrbisPngEncColorSpace::RGBA;
    const rpng_pixfmt format = alpha ? (bgra ? RPNG_PIXFMT_ARGB32 : RPNG_PIXFMT_RGBA32)
                                     : (bgra ? RPNG_PIXFMT_XRGB8888 : RPNG_PIXFMT_XBGR8888);
    const u32 pitch = param->image_pitch ? param->image_pitch : param->image_width * 4;

    intfstream_t* out =
        intfstream_open_memory(param->png_mem_addr, RETRO_VFS_FILE_ACCESS_WRITE,
                               RETRO_VFS_FILE_ACCESS_HINT_NONE, param->png_mem_size);
    if (!out) {
        return ORBIS_PNG_ENC_ERROR_FATAL;
    }
    const bool ok =
        rpng_save_image_stream_fmt(param->image_mem_addr, out, param->image_width,
                                   param->image_height, static_cast<s32>(pitch), format, nullptr);
    const s64 size = intfstream_tell(out);
    intfstream_close(out);
    free(out);

    if (!ok) {
        if (size >= static_cast<s64>(param->png_mem_size)) {
            LOG_ERROR(Lib_Png, "Ran out of room to write PNG");
            return ORBIS_PNG_ENC_ERROR_DATA_OVERFLOW;
        }
        LOG_ERROR(Lib_Png, "PNG encode failed");
        return ORBIS_PNG_ENC_ERROR_FATAL;
    }
    if (outputInfo != nullptr) {
        outputInfo->data_size = static_cast<u32>(size);
        outputInfo->processed_height = param->image_height;
    }
    return static_cast<s32>(size);
}

s32 PS4_SYSV_ABI scePngEncQueryMemorySize(const OrbisPngEncCreateParam* param) {
    if (param == nullptr) {
        LOG_ERROR(Lib_Png, "Invalid Address");
        return ORBIS_PNG_ENC_ERROR_INVALID_ADDR;
    }

    if (param->attribute != 0 || param->max_filter_number > 5) {
        LOG_ERROR(Lib_Png, "Invalid Param, attribute = {}, max_filter_number = {}",
                  param->attribute, param->max_filter_number);
        return ORBIS_PNG_ENC_ERROR_INVALID_PARAM;
    }

    if (param->max_image_width - 1 > 1000000) {
        LOG_ERROR(Lib_Png, "Invalid Size, width = {}", param->max_image_width);
        return ORBIS_PNG_ENC_ERROR_INVALID_SIZE;
    }

    return sizeof(PngHandler);
}

void RegisterLib(Core::Loader::SymbolsResolver* sym) {
    LIB_FUNCTION("7aGTPfrqT9s", "libScePngEnc", 1, "libScePngEnc", scePngEncCreate);
    LIB_FUNCTION("RUrWdwTWZy8", "libScePngEnc", 1, "libScePngEnc", scePngEncDelete);
    LIB_FUNCTION("xgDjJKpcyHo", "libScePngEnc", 1, "libScePngEnc", scePngEncEncode);
    LIB_FUNCTION("9030RnBDoh4", "libScePngEnc", 1, "libScePngEnc", scePngEncQueryMemorySize);
};

} // namespace Libraries::PngEnc