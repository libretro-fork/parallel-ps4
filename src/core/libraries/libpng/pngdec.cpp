// SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <cstring>

#include <formats/image.h>
#include <formats/rpng.h>
#include <libretro.h>

#include "common/assert.h"
#include "common/logging/log.h"
#include "core/libraries/libpng/pngdec.h"
#include "core/libraries/libpng/pngdec_error.h"
#include "core/libraries/libs.h"

namespace Libraries::PngDec {

// The handle's memory, as the game sizes it with scePngDecQueryMemorySize. A decode needs
// nothing kept between calls: libretro-common's rpng (formats/rpng.h) reads the file in place
// and decodes straight into the game's image buffer.
struct PngHandler {
    u64 reserved[2];
};

static inline OrbisPngDecColorSpace MapPngColor(int color) {
    switch (color) {
    case 0:
        return OrbisPngDecColorSpace::Grayscale;
    case 4:
        return OrbisPngDecColorSpace::GrayscaleAlpha;
    case 3:
        return OrbisPngDecColorSpace::Clut;
    case 2:
        return OrbisPngDecColorSpace::Rgb;
    case 6:
        return OrbisPngDecColorSpace::Rgba;
    }
    UNREACHABLE_MSG("unknown png color type");
}

// The file's chunks walked, so its header is known. False when it is not a PNG.
static bool ReadPng(rpng_t* rpng, const void* data, u32 size, rpng_info& info) {
    if (!rpng || !rpng_set_buf_ptr(rpng, const_cast<void*>(data), size) || !rpng_start(rpng)) {
        return false;
    }
    while (rpng_iterate_image(rpng)) {
    }
    return rpng_get_info(rpng, &info);
}

static void FillImageInfo(const rpng_info& info, OrbisPngDecImageInfo* image_info) {
    image_info->image_width = info.width;
    image_info->image_height = info.height;
    image_info->color_space = MapPngColor(info.color_type);
    image_info->bit_depth = info.depth;
    image_info->image_flag = OrbisPngDecImageFlag::None;
    if (info.interlace == 1) {
        image_info->image_flag |= OrbisPngDecImageFlag::Adam7Interlace;
    }
    if (info.has_trns) {
        image_info->image_flag |= OrbisPngDecImageFlag::TrnsChunkExist;
    }
}

s32 PS4_SYSV_ABI scePngDecCreate(const OrbisPngDecCreateParam* param, void* memoryAddress,
                                 u32 memorySize, OrbisPngDecHandle* handle) {
    if (param == nullptr || param->attribute > 1) {
        LOG_ERROR(Lib_Png, "Invalid param!");
        return ORBIS_PNG_DEC_ERROR_INVALID_PARAM;
    }
    if (memoryAddress == nullptr) {
        LOG_ERROR(Lib_Png, "Invalid memory address!");
        return ORBIS_PNG_DEC_ERROR_INVALID_ADDR;
    }
    if (param->max_image_width - 1 > 1000000) {
        LOG_ERROR(Lib_Png, "Invalid size! width = {}", param->max_image_width);
        return ORBIS_PNG_DEC_ERROR_INVALID_SIZE;
    }
    auto pngh = (PngHandler*)memoryAddress;
    *handle = pngh;
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI scePngDecDecode(OrbisPngDecHandle handle, const OrbisPngDecDecodeParam* param,
                                 OrbisPngDecImageInfo* imageInfo) {
    if (handle == nullptr) {
        LOG_ERROR(Lib_Png, "invalid handle!");
        return ORBIS_PNG_DEC_ERROR_INVALID_HANDLE;
    }
    if (param == nullptr) {
        LOG_ERROR(Lib_Png, "Invalid param!");
        return ORBIS_PNG_DEC_ERROR_INVALID_PARAM;
    }
    if (param->png_mem_addr == nullptr || param->image_mem_addr == nullptr) {
        LOG_ERROR(Lib_Png, "invalid image address!");
        return ORBIS_PNG_DEC_ERROR_INVALID_ADDR;
    }
    LOG_TRACE(Lib_Png,
              "pngMemSize = {} , imageMemSize = {} , pixelFormat = {} , alphaValue = {} , "
              "imagePitch = {}",
              param->png_mem_size, param->image_mem_size, int(param->pixel_format),
              param->alpha_value, param->image_pitch);

    rpng_t* rpng = rpng_alloc();
    rpng_info info{};
    if (!ReadPng(rpng, param->png_mem_addr, param->png_mem_size, info)) {
        rpng_free(rpng);
        LOG_ERROR(Lib_Png, "Memory doesn't contain a valid png file");
        return ORBIS_PNG_DEC_ERROR_INVALID_DATA;
    }
    if (imageInfo != nullptr) {
        FillImageInfo(info, imageInfo);
    }

    // 32 bits a pixel, 8 bits a channel, rows image_pitch bytes apart, decoded straight into
    // the game's buffer.
    const u32 width = info.width;
    const u32 height = info.height;
    const u64 row_bytes = static_cast<u64>(width) * 4;
    const u64 pitch = param->image_pitch > 0 ? param->image_pitch : row_bytes;
    if (pitch < row_bytes || pitch % 4 != 0 ||
        pitch * (height - 1) + row_bytes > param->image_mem_size) {
        rpng_free(rpng);
        LOG_ERROR(Lib_Png, "Image buffer of {} bytes, pitch {}, does not hold {}x{}",
                  param->image_mem_size, pitch, width, height);
        return ORBIS_PNG_DEC_ERROR_INVALID_SIZE;
    }
    auto* image = reinterpret_cast<u32*>(param->image_mem_addr);
    rpng_set_output(rpng, image, pitch / 4);
    const bool rgba = param->pixel_format != OrbisPngDecPixelFormat::B8G8R8A8;
    void* out = nullptr;
    unsigned out_width = 0;
    unsigned out_height = 0;
    int ret;
    do {
        ret = rpng_process_image(rpng, &out, param->png_mem_size, &out_width, &out_height, rgba);
    } while (ret == IMAGE_PROCESS_NEXT);
    rpng_free(rpng);
    if (ret != IMAGE_PROCESS_END) {
        LOG_ERROR(Lib_Png, "PNG decode failed");
        return ORBIS_PNG_DEC_ERROR_DECODE_ERROR;
    }

    // An image without alpha takes the game's alpha value; rpng makes it opaque.
    const bool has_alpha = info.color_type == 4 || info.color_type == 6 || info.has_trns;
    if (!has_alpha && param->alpha_value != 0xFF) {
        auto* row = reinterpret_cast<u8*>(image);
        for (u32 y = 0; y < height; y++, row += pitch) {
            for (u32 x = 0; x < width; x++) {
                row[x * 4 + 3] = static_cast<u8>(param->alpha_value);
            }
        }
    }

    return (width > 32767 || height > 32767) ? 0 : (width << 16) | height;
}

s32 PS4_SYSV_ABI scePngDecDecodeWithInputControl() {
    LOG_ERROR(Lib_Png, "(STUBBED)called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI scePngDecDelete(OrbisPngDecHandle handle) {
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI scePngDecParseHeader(const OrbisPngDecParseParam* param,
                                      OrbisPngDecImageInfo* imageInfo) {
    if (param == nullptr) {
        LOG_ERROR(Lib_Png, "Invalid param!");
        return ORBIS_PNG_DEC_ERROR_INVALID_PARAM;
    }

    rpng_t* rpng = rpng_alloc();
    rpng_info info{};
    const bool ok = ReadPng(rpng, param->png_mem_addr, param->png_mem_size, info);
    rpng_free(rpng);
    if (!ok) {
        LOG_ERROR(Lib_Png, "Memory doesn't contain a valid png file");
        return ORBIS_PNG_DEC_ERROR_INVALID_DATA;
    }
    FillImageInfo(info, imageInfo);
    LOG_TRACE(
        Lib_Png,
        "imageWidth = {} , imageHeight = {} , colorSpace = {} , bitDepth = {} , imageFlag = {}",
        imageInfo->image_width, imageInfo->image_height, int(imageInfo->color_space),
        imageInfo->bit_depth, int(imageInfo->image_flag));
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI scePngDecQueryMemorySize(const OrbisPngDecCreateParam* param) {
    if (param == nullptr) {
        LOG_ERROR(Lib_Png, "Invalid param!");
        return ORBIS_PNG_DEC_ERROR_INVALID_PARAM;
    }
    if (param->attribute > 1) {
        LOG_ERROR(Lib_Png, "Invalid attribute! attribute = {}", param->attribute);
        return ORBIS_PNG_DEC_ERROR_INVALID_ADDR;
    }
    if (param->max_image_width - 1 > 1000000) {
        LOG_ERROR(Lib_Png, "Invalid size! width = {}", param->max_image_width);
        return ORBIS_PNG_DEC_ERROR_INVALID_SIZE;
    }
    return sizeof(PngHandler);
}

void RegisterLib(Core::Loader::SymbolsResolver* sym) {
    LIB_FUNCTION("m0uW+8pFyaw", "libScePngDec", 1, "libScePngDec", scePngDecCreate);
    LIB_FUNCTION("WC216DD3El4", "libScePngDec", 1, "libScePngDec", scePngDecDecode);
    LIB_FUNCTION("cJ--1xAbj-I", "libScePngDec", 1, "libScePngDec", scePngDecDecodeWithInputControl);
    LIB_FUNCTION("QbD+eENEwo8", "libScePngDec", 1, "libScePngDec", scePngDecDelete);
    LIB_FUNCTION("U6h4e5JRPaQ", "libScePngDec", 1, "libScePngDec", scePngDecParseHeader);
    LIB_FUNCTION("-6srIGbLTIU", "libScePngDec", 1, "libScePngDec", scePngDecQueryMemorySize);
    LIB_FUNCTION("cJ--1xAbj-I", "libScePngDec_jvm", 1, "libScePngDec",
                 scePngDecDecodeWithInputControl);
};

} // namespace Libraries::PngDec
