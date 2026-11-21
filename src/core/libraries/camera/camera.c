/* SPDX-FileCopyrightText: Copyright 2025-2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <stddef.h>
#include <string.h>

#include "common/c_bridge.h"
#include "core/libraries/camera/camera.h"
#include "core/libraries/camera/camera_backend.h"
#include "core/libraries/camera/camera_helpers.h"
#include "core/libraries/error_codes.h"
#include "core/libraries/kernel/kernel_c.h"

bool g_library_opened = false;
s32 g_firmware_version = 0;

static s32 g_handles = 0;
#define c_width ((s32)CAMERA_WIDTH)
#define c_height ((s32)CAMERA_HEIGHT)

static u16 *raw16_buffer1 = NULL, *raw16_buffer2 = NULL;
static u8 *raw8_buffer1 = NULL, *raw8_buffer2 = NULL;

OrbisCameraConfigExtention output_config0, output_config1;

/* No camera: what the core has until the frontend's is passed through (camera_backend.h). */
static bool no_camera_is_attached(void) {
    return false;
}

static bool no_camera_start(u32 fps) {
    return false;
}

static void no_camera_stop(void) {}

static bool no_camera_acquire_frame(camera_frame_t* frame) {
    return false;
}

static bool camera_started = false;

const camera_backend_t* camera_get_backend(void) {
    static const camera_backend_t none = {no_camera_is_attached, no_camera_start, no_camera_stop,
                                          no_camera_acquire_frame};
    return &none;
}

s32 PS4_SYSV_ABI sceCameraAccGetData(void) {
    c_log(C_LOG_LIB_CAMERA, C_LOG_LEVEL_ERROR, "sceCameraAccGetData", "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceCameraChangeAppModuleState(void) {
    c_log(C_LOG_LIB_CAMERA, C_LOG_LEVEL_ERROR, "sceCameraChangeAppModuleState", "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceCameraClose(s32 handle) {
    c_log(C_LOG_LIB_CAMERA, C_LOG_LEVEL_INFO, "sceCameraClose", "called, handle: %d", handle);
    if (handle < 1) {
        return ORBIS_CAMERA_ERROR_PARAM;
    }
    if (!g_library_opened) {
        return ORBIS_CAMERA_ERROR_NOT_OPEN;
    }

    /* Decrement handles on close. */
    /* If no handles remain, then the library itself is considered closed. */
    if (--g_handles == 0) {
        g_library_opened = false;
    }

    if (camera_started) {
        camera_get_backend()->stop();
        camera_started = false;
    }

    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceCameraCloseByHandle(s32 handle) {
    c_log(C_LOG_LIB_CAMERA, C_LOG_LEVEL_DEBUG, "sceCameraCloseByHandle", "called");
    if (handle < 1) {
        return ORBIS_CAMERA_ERROR_PARAM;
    }
    if (!g_library_opened) {
        return ORBIS_CAMERA_ERROR_NOT_OPEN;
    }

    /* Decrement handles on close. */
    /* If no handles remain, then the library itself is considered closed. */
    if (--g_handles == 0) {
        g_library_opened = false;
    }
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceCameraGetFrameData(s32 handle, OrbisCameraFrameData* frame_data) {
    camera_frame_t frame;
    bool has_frame;
    if (frame_data == NULL) {
        return ORBIS_CAMERA_ERROR_PARAM;
    }
    c_log(C_LOG_LIB_CAMERA, C_LOG_LEVEL_DEBUG, "sceCameraGetFrameData", "called, read mode: 0x%x",
          frame_data->readMode);
    frame_data->status[0] = -1;
    frame_data->status[1] = -1;
    if (handle < 1 || frame_data->sizeThis > 584) {
        return ORBIS_CAMERA_ERROR_PARAM;
    }
    if (!g_library_opened || !camera_started) {
        return ORBIS_CAMERA_ERROR_NOT_OPEN;
    }
    if (!camera_get_backend()->is_attached()) {
        return ORBIS_CAMERA_ERROR_NOT_CONNECTED;
    }
    memset(&frame, 0, sizeof(frame));
    has_frame = camera_get_backend()->acquire_frame(&frame);

    frame_data->status[0] = has_frame ? 1 : -1;
    frame_data->status[1] = has_frame ? 1 : -1;
    if (!has_frame) {
        switch (output_config0.format.formatLevel0) {
        case ORBIS_CAMERA_FORMAT_YUV422:
            frame_data->pFramePointerList[0][0] = raw16_buffer1;
            break;
        case ORBIS_CAMERA_FORMAT_RAW16:
            frame_data->pFramePointerList[0][0] = raw16_buffer1;
            break;
        case ORBIS_CAMERA_FORMAT_RAW8:
            frame_data->pFramePointerList[0][0] = raw8_buffer1;
            break;
        default:
            c_unreachable(__FILE__, __LINE__, "sceCameraGetFrameData", "Unreachable code!");
        }
        switch (output_config1.format.formatLevel0) {
        case ORBIS_CAMERA_FORMAT_YUV422:
            frame_data->pFramePointerList[1][0] = raw16_buffer2;
            break;
        case ORBIS_CAMERA_FORMAT_RAW16:
            frame_data->pFramePointerList[1][0] = raw16_buffer2;
            break;
        case ORBIS_CAMERA_FORMAT_RAW8:
            frame_data->pFramePointerList[1][0] = raw8_buffer2;
            break;
        default:
            c_unreachable(__FILE__, __LINE__, "sceCameraGetFrameData", "Unreachable code!");
        }
        return frame_data->pFramePointerList[0][0] == NULL ? ORBIS_CAMERA_ERROR_BUSY : ORBIS_OK;
    }

    switch (output_config0.format.formatLevel0) {
    case ORBIS_CAMERA_FORMAT_YUV422:
        memcpy(raw16_buffer1, frame.yuy2, c_width * c_height * sizeof(u16));
        frame_data->pFramePointerList[0][0] = raw16_buffer1;
        break;
    case ORBIS_CAMERA_FORMAT_RAW16:
        camera_yuy2_to_raw16(frame.yuy2, raw16_buffer1, c_width, c_height);
        frame_data->pFramePointerList[0][0] = raw16_buffer1;
        break;
    case ORBIS_CAMERA_FORMAT_RAW8:
        camera_yuy2_to_raw8(frame.yuy2, raw8_buffer1, c_width, c_height);
        frame_data->pFramePointerList[0][0] = raw8_buffer1;
        break;
    default:
        c_unreachable(__FILE__, __LINE__, "sceCameraGetFrameData", "Unreachable code!");
    }
    switch (output_config1.format.formatLevel0) {
    case ORBIS_CAMERA_FORMAT_YUV422:
        memcpy(raw16_buffer2, frame.yuy2, c_width * c_height * sizeof(u16));
        frame_data->pFramePointerList[1][0] = raw16_buffer2;
        break;
    case ORBIS_CAMERA_FORMAT_RAW16:
        camera_yuy2_to_raw16(frame.yuy2, raw16_buffer2, c_width, c_height);
        frame_data->pFramePointerList[1][0] = raw16_buffer2;
        break;
    case ORBIS_CAMERA_FORMAT_RAW8:
        camera_yuy2_to_raw8(frame.yuy2, raw8_buffer2, c_width, c_height);
        frame_data->pFramePointerList[1][0] = raw8_buffer2;
        break;
    default:
        c_unreachable(__FILE__, __LINE__, "sceCameraGetFrameData", "Unreachable code!");
    }
    frame_data->meta.format[0][0] = output_config0.format.formatLevel0;
    frame_data->meta.format[1][0] = output_config1.format.formatLevel0;
    frame_data->frameSize[0][0] =
        c_width * c_height * camera_size_of_base_format(output_config0.format.formatLevel0);
    frame_data->frameSize[1][0] =
        c_width * c_height * camera_size_of_base_format(output_config1.format.formatLevel0);

    /* on older firmwares, this wasn't present, and the original library also checks struct size */
    /* instead of the SDK version, and without this check, we'd smash the stack in those games */
    if (frame_data->sizeThis == 584) {
        /* not fully correct, but good enough */
        frame_data->pFramePointerListGarlic[0][0] = frame_data->pFramePointerList[0][0];
        frame_data->pFramePointerListGarlic[1][0] = frame_data->pFramePointerList[1][0];
    }
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceCameraIsAttached(s32 index) {
    static bool first_log = true;
    if (first_log) {
        c_log(C_LOG_LIB_CAMERA, C_LOG_LEVEL_INFO, "sceCameraIsAttached", "called");
        first_log = false;
    } else {
        c_log(C_LOG_LIB_CAMERA, C_LOG_LEVEL_DEBUG, "sceCameraIsAttached", "called");
    }
    if (index != 0) {
        return ORBIS_CAMERA_ERROR_PARAM;
    }
    /* 0 = disconnected, 1 = connected */
    return camera_get_backend()->is_attached() ? 1 : 0;
}

s32 PS4_SYSV_ABI sceCameraIsConfigChangeDone(void) {
    c_log(C_LOG_LIB_CAMERA, C_LOG_LEVEL_ERROR, "sceCameraIsConfigChangeDone", "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceCameraIsValidFrameData(s32 handle, OrbisCameraFrameData* frame_data) {
    c_log(C_LOG_LIB_CAMERA, C_LOG_LEVEL_DEBUG, "sceCameraIsValidFrameData", "called");
    if (handle < 1 || frame_data == NULL || frame_data->sizeThis > 584) {
        return ORBIS_CAMERA_ERROR_PARAM;
    }
    if (!g_library_opened) {
        return ORBIS_CAMERA_ERROR_NOT_OPEN;
    }

    return frame_data->status[0] == 1 && frame_data->status[1] == 1;
}

s32 PS4_SYSV_ABI sceCameraOpen(s32 user_id, s32 type, s32 index, OrbisCameraOpenParameter* param) {
    /* The frames' buffers, in system memory the GPU reads, mapped once */
    static bool buffers_initialized = false;
    c_log(C_LOG_LIB_CAMERA, C_LOG_LEVEL_INFO, "sceCameraOpen", "called");
    if (user_id != CAMERA_USER_ID_SYSTEM || type != 0 || index != 0) {
        c_log(C_LOG_LIB_CAMERA, C_LOG_LEVEL_ERROR, "sceCameraOpen", "ORBIS_CAMERA_ERROR_PARAM");
        return ORBIS_CAMERA_ERROR_PARAM;
    }

    if (!buffers_initialized) {
        const s32 camera_system_mem_size = 0xfd0000;
        const s32 raw8_buffer_size = c_width * c_height * (s32)sizeof(u8);
        const s32 raw16_buffer_size = c_width * c_height * (s32)sizeof(u16);
        void* camera_garlic_pool = (void*)(uintptr_t)0xfd0000000ull;
        u8* remaining_camera_buf;
        s32 ret = kernel_map_named_system_flexible_memory(
            &camera_garlic_pool, camera_system_mem_size,
            (s32)(KERNEL_PROT_CPU_READ_WRITE | KERNEL_PROT_GPU_READ), 0, "SceCameraGpuGarlicPool");
        C_ASSERT(ret == ORBIS_OK, "sceCameraOpen");

        remaining_camera_buf = (u8*)camera_garlic_pool;

        raw8_buffer1 = remaining_camera_buf;
        remaining_camera_buf += raw8_buffer_size;
        raw8_buffer2 = remaining_camera_buf;
        remaining_camera_buf += raw8_buffer_size;
        raw16_buffer1 = (u16*)remaining_camera_buf;
        remaining_camera_buf += raw16_buffer_size;
        raw16_buffer2 = (u16*)remaining_camera_buf;
        remaining_camera_buf += raw16_buffer_size;

        C_ASSERT(remaining_camera_buf <= (u8*)camera_garlic_pool + camera_system_mem_size,
                 "sceCameraOpen");
        C_ASSERT(
            kernel_is_valid_gpu_mapping((uintptr_t)camera_garlic_pool, (u64)camera_system_mem_size),
            "sceCameraOpen");
        buffers_initialized = true;
    }

    g_library_opened = true;
    return ++g_handles;
}

s32 PS4_SYSV_ABI sceCameraOpenByModuleId(void) {
    c_log(C_LOG_LIB_CAMERA, C_LOG_LEVEL_ERROR, "sceCameraOpenByModuleId", "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceCameraSetAppModuleFocus(void) {
    c_log(C_LOG_LIB_CAMERA, C_LOG_LEVEL_ERROR, "sceCameraSetAppModuleFocus", "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceCameraRemoveAppModuleFocus(void) {
    c_log(C_LOG_LIB_CAMERA, C_LOG_LEVEL_ERROR, "sceCameraRemoveAppModuleFocus", "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceCameraStart(s32 handle, OrbisCameraStartParameter* param) {
    c_log(C_LOG_LIB_CAMERA, C_LOG_LEVEL_INFO, "sceCameraStart", "called");
    if (handle < 1 || param == NULL || param->sizeThis != sizeof(OrbisCameraStartParameter)) {
        c_log(C_LOG_LIB_CAMERA, C_LOG_LEVEL_ERROR, "sceCameraStart", "ORBIS_CAMERA_ERROR_PARAM");
        return ORBIS_CAMERA_ERROR_PARAM;
    }
    if (!g_library_opened) {
        c_log(C_LOG_LIB_CAMERA, C_LOG_LEVEL_ERROR, "sceCameraStart", "ORBIS_CAMERA_ERROR_NOT_OPEN");
        return ORBIS_CAMERA_ERROR_NOT_OPEN;
    }
    if (g_firmware_version >= KERNEL_FW_250 &&
        (param->formatLevel[0] > 0xf || param->formatLevel[1] > 0xf ||
         (param->formatLevel[0] | param->formatLevel[1]) == 0)) {
        c_log(C_LOG_LIB_CAMERA, C_LOG_LEVEL_ERROR, "sceCameraStart",
              "ORBIS_CAMERA_ERROR_FORMAT_UNKNOWN");
        return ORBIS_CAMERA_ERROR_FORMAT_UNKNOWN;
    }

    if (param->formatLevel[0] > 1 || param->formatLevel[1] > 1) {
        c_log(C_LOG_LIB_CAMERA, C_LOG_LEVEL_ERROR, "sceCameraStart",
              "Downscaled image retrieval isn't supported yet!");
    }

    if (!camera_get_backend()->is_attached()) {
        c_log(C_LOG_LIB_CAMERA, C_LOG_LEVEL_INFO, "sceCameraStart", "No camera devices connected");
        return ORBIS_CAMERA_ERROR_NOT_CONNECTED;
    }
    if (!camera_started) {
        if (!camera_get_backend()->start(60)) {
            c_log(C_LOG_LIB_CAMERA, C_LOG_LEVEL_ERROR, "sceCameraStart",
                  "Failed to start the camera");
            return ORBIS_CAMERA_ERROR_FATAL;
        }
        camera_started = true;
    }
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceCameraStartByHandle(s32 handle, OrbisCameraStartParameter* param) {
    c_log(C_LOG_LIB_CAMERA, C_LOG_LEVEL_DEBUG, "sceCameraStartByHandle", "called");
    if (handle < 1 || param == NULL || param->sizeThis != sizeof(OrbisCameraStartParameter)) {
        return ORBIS_CAMERA_ERROR_PARAM;
    }
    if (!g_library_opened) {
        return ORBIS_CAMERA_ERROR_NOT_OPEN;
    }

    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceCameraStop(s32 handle) {
    c_log(C_LOG_LIB_CAMERA, C_LOG_LEVEL_INFO, "sceCameraStop", "called, handle: %d", handle);
    if (handle < 1) {
        return ORBIS_CAMERA_ERROR_PARAM;
    }
    if (!g_library_opened) {
        return ORBIS_CAMERA_ERROR_NOT_OPEN;
    }

    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceCameraStopByHandle(s32 handle) {
    c_log(C_LOG_LIB_CAMERA, C_LOG_LEVEL_DEBUG, "sceCameraStopByHandle", "called");
    return sceCameraStop(handle);
}
