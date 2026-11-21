/* SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <stddef.h>

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

s32 PS4_SYSV_ABI sceJpegEncEncode(OrbisJpegEncHandle handle, const OrbisJpegEncEncodeParam* param,
                                  OrbisJpegEncOutputInfo* output_info) {
    s32 handle_ret;
    s32 param_ret;
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

    c_log(C_LOG_LIB_JPEG, C_LOG_LEVEL_ERROR, "sceJpegEncEncode",
          "(STUBBED) image_size = %u , jpeg_size = %u , image_width = %u , image_height = %u , "
          "image_pitch = %u , pixel_format = %s , encode_mode = %s , color_space = %s , "
          "sampling_type = %s , compression_ratio = %u , restart_interval = %d",
          param->image_size, param->jpeg_size, param->image_width, param->image_height,
          param->image_pitch,
          enum_name(&enum_names_OrbisJpegEncEncodeParamPixelFormat, (s64)param->pixel_format),
          enum_name(&enum_names_OrbisJpengEncEncodeParamEncodeMode, (s64)param->encode_mode),
          enum_name(&enum_names_OrbisJpengEncEncodeParamColorSpace, (s64)param->color_space),
          enum_name(&enum_names_OrbisJpengEncEncodeParamSamplingType, (s64)param->sampling_type),
          param->compression_ratio, param->restart_interval);

    if (output_info) {
        output_info->size = param->jpeg_size;
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
