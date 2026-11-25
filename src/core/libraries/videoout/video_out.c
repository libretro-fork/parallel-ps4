/* SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <retro_atomic.h>
#include <string.h>

#include "common/c_bridge.h"
#include "core/file_format/psf.h"
#include "core/libraries/error_codes.h"
#include "core/libraries/kernel/kernel_c.h"
#include "core/libraries/videoout/driver.h"
#include "core/libraries/videoout/video_out.h"
#include "core/libraries/videoout/videoout_error.h"
#include "core/libraries/videoout/videoout_gpu_c.h"

/* The game's param.sfo says it supports HDR (Common::PSFAttributes::support_hdr) */
static bool video_out_game_supports_hdr(void) {
    s32 attributes = 0;
    if (!psf_game_get_integer("ATTRIBUTE", &attributes)) {
        return false;
    }
    return (((u32)attributes >> 29) & 1u) != 0;
}

void video_out_library_init(u32 width, u32 height) {
    videoout_driver_init(width, height);
}

void video_out_library_deinit(void) {
    videoout_driver_deinit();
}

void PS4_SYSV_ABI sceVideoOutSetBufferAttribute(BufferAttribute* attribute,
                                                VideoOutPixelFormat pixelFormat, u32 tilingMode,
                                                u32 aspectRatio, u32 width, u32 height,
                                                u32 pitchInPixel) {
    c_log(C_LOG_LIB_VIDEO_OUT, C_LOG_LEVEL_INFO, "sceVideoOutSetBufferAttribute",
          "pixelFormat = %s, tilingMode = %u, aspectRatio = %u, width = %u, height = %u, "
          "pitchInPixel = %u",
          video_out_pixel_format_string(pixelFormat), tilingMode, aspectRatio, width, height,
          pitchInPixel);

    memset(attribute, 0, sizeof(BufferAttribute));
    attribute->pixel_format = pixelFormat;
    attribute->tiling_mode = (VideoOutTilingMode)tilingMode;
    attribute->aspect_ratio = (s32)aspectRatio;
    attribute->width = width;
    attribute->height = height;
    attribute->pitch_in_pixel = pitchInPixel;
    attribute->option = SCE_VIDEO_OUT_BUFFER_ATTRIBUTE_OPTION_NONE;
}

/* The flip or vblank event (@ident) added to @eq for @port */
static s32 video_out_add_event(const char* func, s64 eq, s32 handle, void* udata, u64 ident,
                               retro_atomic_64_t* queues, const char* no_room) {
    VideoOutPort* port;
    kernel_event_t event;
    c_log(C_LOG_LIB_VIDEO_OUT, C_LOG_LEVEL_INFO, func, "handle = %d", handle);

    port = videoout_driver_get_port(handle);
    if (port == NULL) {
        return ORBIS_VIDEO_OUT_ERROR_INVALID_HANDLE;
    }
    if (!kernel_equeue_exists(eq)) {
        return ORBIS_VIDEO_OUT_ERROR_INVALID_EVENT_QUEUE;
    }

    memset(&event, 0, sizeof(event));
    event.ident = ident;
    event.filter = KERNEL_EVFILT_VIDEO_OUT;
    event.flags = KERNEL_EV_ADD;
    event.udata = udata;
    event.fflags = 0;
    event.data = 0;
    if (!videoout_port_add_event_queue(queues, eq)) {
        c_log(C_LOG_LIB_VIDEO_OUT, C_LOG_LEVEL_ERROR, func, "%s", no_room);
    }
    kernel_equeue_add_event(eq, &event, port);
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceVideoOutAddFlipEvent(s64 eq, s32 handle, void* udata) {
    VideoOutPort* port = videoout_driver_get_port(handle);
    return video_out_add_event("sceVideoOutAddFlipEvent", eq, handle, udata,
                               ORBIS_VIDEO_OUT_INTERNAL_EVENT_FLIP, port ? port->flip_events : NULL,
                               "No room for another flip event queue");
}

s32 PS4_SYSV_ABI sceVideoOutAddVblankEvent(s64 eq, s32 handle, void* udata) {
    VideoOutPort* port = videoout_driver_get_port(handle);
    return video_out_add_event(
        "sceVideoOutAddVblankEvent", eq, handle, udata, ORBIS_VIDEO_OUT_INTERNAL_EVENT_VBLANK,
        port ? port->vblank_events : NULL, "No room for another vblank event queue");
}

static s32 video_out_delete_event(s64 eq, s32 handle, int vblank) {
    VideoOutPort* port = videoout_driver_get_port(handle);
    if (port == NULL) {
        return ORBIS_VIDEO_OUT_ERROR_INVALID_HANDLE;
    }
    if (!kernel_equeue_exists(eq)) {
        return ORBIS_VIDEO_OUT_ERROR_INVALID_EVENT_QUEUE;
    }
    /* By the handle, as it always has been here */
    kernel_equeue_remove_event(eq, (u64)(s64)handle, KERNEL_EVFILT_VIDEO_OUT);
    videoout_port_remove_event_queue(vblank ? port->vblank_events : port->flip_events, eq);
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceVideoOutDeleteFlipEvent(s64 eq, s32 handle) {
    return video_out_delete_event(eq, handle, 0);
}

s32 PS4_SYSV_ABI sceVideoOutDeleteVblankEvent(s64 eq, s32 handle) {
    return video_out_delete_event(eq, handle, 1);
}

s32 PS4_SYSV_ABI sceVideoOutRegisterBuffers(s32 handle, s32 startIndex, void* const* addresses,
                                            s32 bufferNum, const BufferAttribute* attribute) {
    VideoOutPort* port;
    if (!addresses || !attribute) {
        c_log(C_LOG_LIB_VIDEO_OUT, C_LOG_LEVEL_ERROR, "sceVideoOutRegisterBuffers",
              "Addresses are null");
        return ORBIS_VIDEO_OUT_ERROR_INVALID_ADDRESS;
    }

    port = videoout_driver_get_port(handle);
    if (!port || !port->is_open) {
        c_log(C_LOG_LIB_VIDEO_OUT, C_LOG_LEVEL_ERROR, "sceVideoOutRegisterBuffers",
              "Invalid handle = %d", handle);
        return ORBIS_VIDEO_OUT_ERROR_INVALID_HANDLE;
    }

    return videoout_driver_register_buffers(port, startIndex, addresses, bufferNum, attribute);
}

s32 PS4_SYSV_ABI sceVideoOutSetFlipRate(s32 handle, s32 rate) {
#ifndef NDEBUG
    c_log(C_LOG_LIB_VIDEO_OUT, C_LOG_LEVEL_TRACE, "sceVideoOutSetFlipRate", "called");
#endif
    videoout_driver_get_port(handle)->flip_rate = rate;
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceVideoOutIsFlipPending(s32 handle) {
    VideoOutPort* port;
#ifndef NDEBUG
    c_log(C_LOG_LIB_VIDEO_OUT, C_LOG_LEVEL_TRACE, "sceVideoOutIsFlipPending", "called");
#endif
    port = videoout_driver_get_port(handle);
    return (s32)retro_atomic_load_relaxed_int(&port->flip_pending);
}

s32 PS4_SYSV_ABI sceVideoOutSubmitFlip(s32 handle, s32 bufferIndex, s32 flipMode, s64 flipArg) {
    VideoOutPort* port = videoout_driver_get_port(handle);
    if (!port) {
        c_log(C_LOG_LIB_VIDEO_OUT, C_LOG_LEVEL_ERROR, "sceVideoOutSubmitFlip",
              "Invalid handle = %d", handle);
        return ORBIS_VIDEO_OUT_ERROR_INVALID_HANDLE;
    }

    if (flipMode != 1) {
        c_log(C_LOG_LIB_VIDEO_OUT, C_LOG_LEVEL_WARNING, "sceVideoOutSubmitFlip", "flipmode = %d",
              flipMode);
    }

    if (bufferIndex < -1 || bufferIndex > 15) {
        c_log(C_LOG_LIB_VIDEO_OUT, C_LOG_LEVEL_ERROR, "sceVideoOutSubmitFlip",
              "Invalid bufferIndex = %d", bufferIndex);
        return ORBIS_VIDEO_OUT_ERROR_INVALID_INDEX;
    }

    if (bufferIndex != -1 && port->buffer_slots[bufferIndex].group_index < 0) {
        c_log(C_LOG_LIB_VIDEO_OUT, C_LOG_LEVEL_ERROR, "sceVideoOutSubmitFlip",
              "Slot in bufferIndex = %d is not registered", bufferIndex);
        return ORBIS_VIDEO_OUT_ERROR_INVALID_INDEX;
    }

    c_log(C_LOG_LIB_VIDEO_OUT, C_LOG_LEVEL_DEBUG, "sceVideoOutSubmitFlip",
          "bufferIndex = %d, flipMode = %d, flipArg = %lld", bufferIndex, flipMode,
          (long long)flipArg);

    if (!videoout_driver_submit_flip(port, bufferIndex, flipArg, false)) {
        c_log(C_LOG_LIB_VIDEO_OUT, C_LOG_LEVEL_ERROR, "sceVideoOutSubmitFlip",
              "Flip queue is full");
        return ORBIS_VIDEO_OUT_ERROR_FLIP_QUEUE_FULL;
    }

    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceVideoOutGetEventId(const kernel_event_t* ev) {
    if (ev == NULL) {
        return ORBIS_VIDEO_OUT_ERROR_INVALID_ADDRESS;
    }
    if (ev->filter != KERNEL_EVFILT_VIDEO_OUT) {
        return ORBIS_VIDEO_OUT_ERROR_INVALID_EVENT;
    }

    /* The ident as the s16 it was made from */
    switch ((s16)ev->ident) {
    case ORBIS_VIDEO_OUT_INTERNAL_EVENT_FLIP:
        return ORBIS_VIDEO_OUT_EVENT_FLIP;
    case ORBIS_VIDEO_OUT_INTERNAL_EVENT_VBLANK:
    case ORBIS_VIDEO_OUT_INTERNAL_EVENT_SYS_VBLANK:
        return ORBIS_VIDEO_OUT_EVENT_VBLANK;
    case ORBIS_VIDEO_OUT_INTERNAL_EVENT_PRE_VBLANK_START:
        return ORBIS_VIDEO_OUT_EVENT_PRE_VBLANK_START;
    case ORBIS_VIDEO_OUT_INTERNAL_EVENT_SET_MODE:
        return ORBIS_VIDEO_OUT_EVENT_SET_MODE;
    case ORBIS_VIDEO_OUT_INTERNAL_EVENT_POSITION:
        return ORBIS_VIDEO_OUT_EVENT_POSITION;
    default:
        return ORBIS_VIDEO_OUT_ERROR_INVALID_EVENT;
    }
}

s32 PS4_SYSV_ABI sceVideoOutGetEventData(const kernel_event_t* ev, s64* data) {
    u64 event_data;
    if (ev == NULL || data == NULL) {
        return ORBIS_VIDEO_OUT_ERROR_INVALID_ADDRESS;
    }
    if (ev->filter != KERNEL_EVFILT_VIDEO_OUT) {
        return ORBIS_VIDEO_OUT_ERROR_INVALID_EVENT;
    }

    event_data = ev->data >> 0x10;
    /* ev->data is unsigned: never below 0 */
    *data = (s64)event_data;
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceVideoOutGetEventCount(const kernel_event_t* ev) {
    if (ev == NULL) {
        return ORBIS_VIDEO_OUT_ERROR_INVALID_ADDRESS;
    }
    if (ev->filter != KERNEL_EVFILT_VIDEO_OUT) {
        return ORBIS_VIDEO_OUT_ERROR_INVALID_EVENT;
    }
    return (s32)ORBIS_VIDEO_OUT_EVENT_DATA_COUNT(ev->data);
}

s32 PS4_SYSV_ABI sceVideoOutGetFlipStatus(s32 handle, FlipStatus* status) {
    VideoOutPort* port;
    if (!status) {
        c_log(C_LOG_LIB_VIDEO_OUT, C_LOG_LEVEL_ERROR, "sceVideoOutGetFlipStatus",
              "Flip status is null");
        return ORBIS_VIDEO_OUT_ERROR_INVALID_ADDRESS;
    }

    port = videoout_driver_get_port(handle);
    if (!port) {
        c_log(C_LOG_LIB_VIDEO_OUT, C_LOG_LEVEL_ERROR, "sceVideoOutGetFlipStatus",
              "Invalid port handle = %d", handle);
        return ORBIS_VIDEO_OUT_ERROR_INVALID_HANDLE;
    }

    videoout_port_get_flip_status(port, status);

#ifndef NDEBUG
    c_log(C_LOG_LIB_VIDEO_OUT, C_LOG_LEVEL_TRACE, "sceVideoOutGetFlipStatus",
          "count = %llu, processTime = %llu, tsc = %llu, submitTsc = %llu, flipArg = %lld, "
          "gcQueueNum = %d, flipPendingNum = %d, currentBuffer = %d",
          (unsigned long long)status->count, (unsigned long long)status->process_time,
          (unsigned long long)status->tsc, (unsigned long long)status->submit_tsc,
          (long long)status->flip_arg, status->gc_queue_num, status->flip_pending_num,
          status->current_buffer);
#endif
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceVideoOutGetVblankStatus(int handle, SceVideoOutVblankStatus* status) {
    VideoOutPort* port;
    if (status == NULL) {
        return ORBIS_VIDEO_OUT_ERROR_INVALID_ADDRESS;
    }

    port = videoout_driver_get_port(handle);
    if (!port) {
        c_log(C_LOG_LIB_VIDEO_OUT, C_LOG_LEVEL_ERROR, "sceVideoOutGetVblankStatus",
              "Invalid port handle = %d", handle);
        return ORBIS_VIDEO_OUT_ERROR_INVALID_HANDLE;
    }

    videoout_port_get_vblank_status(port, status);
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceVideoOutGetResolutionStatus(s32 handle, SceVideoOutResolutionStatus* status) {
    VideoOutPort* port;
    c_log(C_LOG_LIB_VIDEO_OUT, C_LOG_LEVEL_INFO, "sceVideoOutGetResolutionStatus", "called");
    port = videoout_driver_get_port(handle);
    if (!port || !port->is_open) {
        return ORBIS_VIDEO_OUT_ERROR_INVALID_HANDLE;
    }

    *status = port->resolution;
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceVideoOutOpen(s32 userId, s32 busType, s32 index, const void* param) {
    int handle;
    (void)userId;
    c_log(C_LOG_LIB_VIDEO_OUT, C_LOG_LEVEL_INFO, "sceVideoOutOpen", "called");
    C_ASSERT(busType == SCE_VIDEO_OUT_BUS_TYPE_MAIN, "sceVideoOutOpen");

    if (index != 0) {
        c_log(C_LOG_LIB_VIDEO_OUT, C_LOG_LEVEL_ERROR, "sceVideoOutOpen", "Index != 0");
        return ORBIS_VIDEO_OUT_ERROR_INVALID_VALUE;
    }

    handle = videoout_driver_open((const ServiceThreadParams*)param);
    if (handle < 0) {
        c_log(C_LOG_LIB_VIDEO_OUT, C_LOG_LEVEL_ERROR, "sceVideoOutOpen",
              "All available handles are open");
        return ORBIS_VIDEO_OUT_ERROR_RESOURCE_BUSY;
    }

    return handle;
}

s32 PS4_SYSV_ABI sceVideoOutClose(s32 handle) {
    videoout_driver_close(handle);
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceVideoOutUnregisterBuffers(s32 handle, s32 attributeIndex) {
    VideoOutPort* port = videoout_driver_get_port(handle);
    if (!port || !port->is_open) {
        return ORBIS_VIDEO_OUT_ERROR_INVALID_HANDLE;
    }
    return videoout_driver_unregister_buffers(port, attributeIndex);
}

s32 PS4_SYSV_ABI sceVideoOutGetBufferLabelAddress(s32 handle, uintptr_t* label_addr) {
    VideoOutPort* port;
    if (label_addr == NULL) {
        return ORBIS_VIDEO_OUT_ERROR_INVALID_ADDRESS;
    }
    port = videoout_driver_get_port(handle);
    if (!port) {
        return ORBIS_VIDEO_OUT_ERROR_INVALID_HANDLE;
    }
    *label_addr = (uintptr_t)port->buffer_labels;
    return 16;
}

/* The EOP flip, when its interrupt comes */
static void video_out_eop_flip(VideoOutPort* port, u32 buf_id, s64 flip_arg) {
    bool result;
    /* Seen on a slow machine (box64 on an Adreno 618, Sonic Mania): the label was already
     * cleared again here, and the assert ended the emulator mid-game. The flip is still the
     * game's, so it is done. */
    if (port->buffer_labels[buf_id] != 1) {
        c_log(C_LOG_LIB_VIDEO_OUT, C_LOG_LEVEL_WARNING, "sceVideoOutSubmitEopFlip",
              "Out of order flip IRQ for buffer %u", buf_id);
    }
    result = videoout_driver_submit_flip(port, (s32)buf_id, flip_arg, true);
    C_ASSERT_MSG(result, "sceVideoOutSubmitEopFlip", "EOP flip submission failed");
}

s32 sceVideoOutSubmitEopFlip(s32 handle, u32 buf_id, u32 mode, s64 flip_arg, void** unk) {
    VideoOutPort* port = videoout_driver_get_port(handle);
    (void)mode;
    (void)unk;
    if (!port) {
        return ORBIS_VIDEO_OUT_ERROR_INVALID_HANDLE;
    }
    videoout_gpu_on_flip_irq(video_out_eop_flip, port, buf_id, flip_arg);
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceVideoOutGetDeviceCapabilityInfo(
    s32 handle, SceVideoOutDeviceCapabilityInfo* pDeviceCapabilityInfo) {
    (void)handle;
    pDeviceCapabilityInfo->capability = 0;
    if (videoout_gpu_is_hdr_supported()) {
        if (video_out_game_supports_hdr()) {
            pDeviceCapabilityInfo->capability |= ORBIS_VIDEO_OUT_DEVICE_CAPABILITY_BT2020_PQ;
        }
    }
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceVideoOutWaitVblank(s32 handle) {
    VideoOutPort* port = videoout_driver_get_port(handle);
    if (!port) {
        return ORBIS_VIDEO_OUT_ERROR_INVALID_HANDLE;
    }
    videoout_port_wait_vblank(port);
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceVideoOutColorSettingsSetGamma(SceVideoOutColorSettings* settings, float gamma) {
    if (gamma < 0.1f || gamma > 2.0f) {
        return ORBIS_VIDEO_OUT_ERROR_INVALID_VALUE;
    }
    settings->gamma = gamma;
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceVideoOutAdjustColor(s32 handle, const SceVideoOutColorSettings* settings) {
    VideoOutPort* port;
    if (settings == NULL) {
        return ORBIS_VIDEO_OUT_ERROR_INVALID_ADDRESS;
    }
    port = videoout_driver_get_port(handle);
    if (!port) {
        return ORBIS_VIDEO_OUT_ERROR_INVALID_HANDLE;
    }
    videoout_gpu_set_gamma(settings->gamma);
    return ORBIS_OK;
}

void PS4_SYSV_ABI sceVideoOutModeSetAny_(OrbisVideoOutMode* mode, u32 size) {
    memset(mode, 0xff, size);
    mode->size = size;
}

s32 PS4_SYSV_ABI sceVideoOutConfigureOutputMode_(s32 handle, u32 reserved,
                                                 const OrbisVideoOutMode* mode, const void* options,
                                                 u32 size_mode, u32 size_options) {
    VideoOutPort* port = videoout_driver_get_port(handle);
    (void)options;
    (void)size_mode;
    (void)size_options;
    if (!port) {
        return ORBIS_VIDEO_OUT_ERROR_INVALID_HANDLE;
    }
    if (reserved != 0) {
        return ORBIS_VIDEO_OUT_ERROR_INVALID_VALUE;
    }

    switch (mode->colorimetry) {
    case ORBIS_VIDEO_OUT_COLORIMETRY_ANY:
        port->is_hdr = false;
        break;
    case ORBIS_VIDEO_OUT_COLORIMETRY_BT2020_PQ:
        if (video_out_game_supports_hdr()) {
            port->is_hdr = true;
        }
        break;
    default:
        return ORBIS_VIDEO_OUT_ERROR_INVALID_VALUE;
    }
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceVideoOutSubmitChangeBufferAttribute(s32 handle, s32 attributeIndex,
                                                        const BufferAttribute* attribute) {
    VideoOutPort* port = videoout_driver_get_port(handle);
    if (!port || !port->is_open) {
        return ORBIS_VIDEO_OUT_ERROR_INVALID_HANDLE;
    }
    return videoout_driver_change_buffer_attribute(port, attributeIndex, attribute);
}

s32 PS4_SYSV_ABI sceVideoOutSetWindowModeMargins(s32 handle, s32 top, s32 bottom) {
    (void)handle;
    c_log(C_LOG_LIB_VIDEO_OUT, C_LOG_LEVEL_ERROR, "sceVideoOutSetWindowModeMargins",
          "(STUBBED) called top = %d, bottom = %d", top, bottom);
    return ORBIS_OK;
}
