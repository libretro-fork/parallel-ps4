/* SPDX-FileCopyrightText: Copyright 2024-2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef SHADPS4_VIDEOOUT_VIDEO_OUT_H
#define SHADPS4_VIDEOOUT_VIDEO_OUT_H

/* libSceVideoOut. Defined in video_out.c over driver.c, registered in core/libraries/libs.cpp. */

#include <retro_common_api.h>
#include <stdint.h>

#include "common/types.h"
#include "core/libraries/kernel/kernel_c.h"
#include "core/libraries/videoout/buffer.h"

RETRO_BEGIN_DECLS

/* SceVideoOutBusType */
#define SCE_VIDEO_OUT_BUS_TYPE_MAIN 0                    /* Main output */
#define SCE_VIDEO_OUT_BUS_TYPE_AUX_SOCIAL_SCREEN 5       /* Aux output for social */
#define SCE_VIDEO_OUT_BUS_TYPE_AUX_GAME_LIVE_STREAMING 6 /* Aux output for streaming */

/* SceVideoOutRefreshRate */
#define SCE_VIDEO_OUT_REFRESH_RATE_UNKNOWN 0
#define SCE_VIDEO_OUT_REFRESH_RATE_23_98HZ 1
#define SCE_VIDEO_OUT_REFRESH_RATE_50HZ 2
#define SCE_VIDEO_OUT_REFRESH_RATE_59_94HZ 3
#define SCE_VIDEO_OUT_REFRESH_RATE_119_88HZ 13
#define SCE_VIDEO_OUT_REFRESH_RATE_89_91HZ 35
#define SCE_VIDEO_OUT_REFRESH_RATE_ANY ((s64) - 1)

#define SCE_VIDEO_OUT_PIXEL_FORMAT_A8R8G8B8_SRGB ((int)0x80000000)
#define SCE_VIDEO_OUT_PIXEL_FORMAT_A8B8G8R8_SRGB ((int)0x80002200)
#define SCE_VIDEO_OUT_PIXEL_FORMAT_A2R10G10B10 ((int)0x88060000)
#define SCE_VIDEO_OUT_PIXEL_FORMAT_A2R10G10B10_SRGB ((int)0x88000000)
#define SCE_VIDEO_OUT_PIXEL_FORMAT_A2R10G10B10_BT2020_PQ ((int)0x88740000)
#define SCE_VIDEO_OUT_PIXEL_FORMAT_A16R16G16B16_FLOAT ((int)0xC1060000)
#define SCE_VIDEO_OUT_PIXEL_FORMAT_YCBCR420_BT709 ((int)0x08322200)

#define SCE_VIDEO_OUT_BUFFER_ATTRIBUTE_OPTION_NONE 0
#define SCE_VIDEO_OUT_BUFFER_ATTRIBUTE_OPTION_VR 7
#define SCE_VIDEO_OUT_BUFFER_ATTRIBUTE_OPTION_STRICT_COLORIMETRY 8

#define ORBIS_VIDEO_OUT_DEVICE_CAPABILITY_BT2020_PQ 0x80

/* OrbisVideoOutColorimetry */
#define ORBIS_VIDEO_OUT_COLORIMETRY_BT2020_PQ 12u
#define ORBIS_VIDEO_OUT_COLORIMETRY_ANY 0xFFu

/* OrbisVideoOutEventId: what sceVideoOutGetEventId gives */
#define ORBIS_VIDEO_OUT_EVENT_FLIP 0
#define ORBIS_VIDEO_OUT_EVENT_VBLANK 1
#define ORBIS_VIDEO_OUT_EVENT_PRE_VBLANK_START 2
#define ORBIS_VIDEO_OUT_EVENT_SET_MODE 8
#define ORBIS_VIDEO_OUT_EVENT_POSITION 12

/* OrbisVideoOutInternalEventId: an event's ident in the queue */
#define ORBIS_VIDEO_OUT_INTERNAL_EVENT_FLIP 0x6
#define ORBIS_VIDEO_OUT_INTERNAL_EVENT_VBLANK 0x7
#define ORBIS_VIDEO_OUT_INTERNAL_EVENT_SET_MODE 0x51
#define ORBIS_VIDEO_OUT_INTERNAL_EVENT_POSITION 0x58
#define ORBIS_VIDEO_OUT_INTERNAL_EVENT_PRE_VBLANK_START 0x59
#define ORBIS_VIDEO_OUT_INTERNAL_EVENT_SYS_VBLANK 0x63

typedef struct FlipStatus {
    u64 count;
    u64 process_time;
    u64 tsc;
    s64 flip_arg;
    u64 submit_tsc;
    u64 reserved0;
    s32 gc_queue_num;
    s32 flip_pending_num;
    s32 current_buffer;
    u32 reserved1;
} FlipStatus;

/* A flip status before any flip: no flip argument, no current buffer */
void flip_status_init(FlipStatus* status);

typedef struct SceVideoOutResolutionStatus {
    s32 full_width;
    s32 full_height;
    s32 pane_width;
    s32 pane_height;
    u64 refresh_rate;
    float screen_size_in_inch;
    u16 flags;
    u16 reserved0;
    u32 reserved1[3];
} SceVideoOutResolutionStatus;

typedef struct SceVideoOutVblankStatus {
    u64 count;
    u64 process_time;
    u64 tsc;
    u64 reserved[1];
    u8 flags;
    u8 pad1[7];
} SceVideoOutVblankStatus;

typedef struct SceVideoOutDeviceCapabilityInfo {
    u64 capability;
} SceVideoOutDeviceCapabilityInfo;

typedef struct SceVideoOutColorSettings {
    float gamma;
    u32 reserved[3];
} SceVideoOutColorSettings;

/* OrbisVideoOutEventData: an event's data, bits 0-11 the time, 12-15 the count, the rest the
 * flip argument */
#define ORBIS_VIDEO_OUT_EVENT_DATA_COUNT(data) ((u32)(((u64)(data) >> 12) & 0xFu))

typedef struct OrbisVideoOutMode {
    u32 size;
    u8 encoding;
    u8 range;
    u8 colorimetry;
    u8 depth;
    u64 refresh_rate;
    u64 resolution;
    u8 reserved[8];
} OrbisVideoOutMode;

/* Makes the driver, with its port's resolution, and starts its present thread; once */
void video_out_library_init(u32 width, u32 height);
/* Stops the present thread and frees the driver */
void video_out_library_deinit(void);

void PS4_SYSV_ABI sceVideoOutSetBufferAttribute(BufferAttribute* attribute,
                                                VideoOutPixelFormat pixelFormat, u32 tilingMode,
                                                u32 aspectRatio, u32 width, u32 height,
                                                u32 pitchInPixel);
s32 PS4_SYSV_ABI sceVideoOutAddFlipEvent(s64 eq, s32 handle, void* udata);
s32 PS4_SYSV_ABI sceVideoOutDeleteFlipEvent(s64 eq, s32 handle);
s32 PS4_SYSV_ABI sceVideoOutAddVblankEvent(s64 eq, s32 handle, void* udata);
s32 PS4_SYSV_ABI sceVideoOutDeleteVblankEvent(s64 eq, s32 handle);
s32 PS4_SYSV_ABI sceVideoOutRegisterBuffers(s32 handle, s32 startIndex, void* const* addresses,
                                            s32 bufferNum, const BufferAttribute* attribute);
s32 PS4_SYSV_ABI sceVideoOutGetBufferLabelAddress(s32 handle, uintptr_t* label_addr);
s32 PS4_SYSV_ABI sceVideoOutSetFlipRate(s32 handle, s32 rate);
s32 PS4_SYSV_ABI sceVideoOutIsFlipPending(s32 handle);
s32 PS4_SYSV_ABI sceVideoOutWaitVblank(s32 handle);
s32 PS4_SYSV_ABI sceVideoOutSubmitFlip(s32 handle, s32 bufferIndex, s32 flipMode, s64 flipArg);
s32 PS4_SYSV_ABI sceVideoOutGetFlipStatus(s32 handle, FlipStatus* status);
s32 PS4_SYSV_ABI sceVideoOutGetVblankStatus(int handle, SceVideoOutVblankStatus* status);
s32 PS4_SYSV_ABI sceVideoOutGetResolutionStatus(s32 handle, SceVideoOutResolutionStatus* status);
s32 PS4_SYSV_ABI sceVideoOutOpen(s32 userId, s32 busType, s32 index, const void* param);
s32 PS4_SYSV_ABI sceVideoOutClose(s32 handle);
s32 PS4_SYSV_ABI sceVideoOutUnregisterBuffers(s32 handle, s32 attributeIndex);
s32 PS4_SYSV_ABI sceVideoOutGetEventId(const kernel_event_t* ev);
s32 PS4_SYSV_ABI sceVideoOutGetEventData(const kernel_event_t* ev, s64* data);
s32 PS4_SYSV_ABI sceVideoOutGetEventCount(const kernel_event_t* ev);
s32 PS4_SYSV_ABI sceVideoOutGetDeviceCapabilityInfo(
    s32 handle, SceVideoOutDeviceCapabilityInfo* pDeviceCapabilityInfo);
s32 PS4_SYSV_ABI sceVideoOutColorSettingsSetGamma(SceVideoOutColorSettings* settings, float gamma);
s32 PS4_SYSV_ABI sceVideoOutAdjustColor(s32 handle, const SceVideoOutColorSettings* settings);
void PS4_SYSV_ABI sceVideoOutModeSetAny_(OrbisVideoOutMode* mode, u32 size);
s32 PS4_SYSV_ABI sceVideoOutConfigureOutputMode_(s32 handle, u32 reserved,
                                                 const OrbisVideoOutMode* mode, const void* options,
                                                 u32 size_mode, u32 size_options);
s32 PS4_SYSV_ABI sceVideoOutSubmitChangeBufferAttribute(s32 handle, s32 attributeIndex,
                                                        const BufferAttribute* attribute);
s32 PS4_SYSV_ABI sceVideoOutSetWindowModeMargins(s32 handle, s32 top, s32 bottom);

/* Internal system functions */
s32 sceVideoOutSubmitEopFlip(s32 handle, u32 buf_id, u32 mode, s64 flip_arg, void** unk);

RETRO_END_DECLS

#endif
