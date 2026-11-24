/* SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <stdlib.h>
#include <string.h>

#include "common/c_bridge.h"
#include "core/libraries/error_codes.h"
#include "core/libraries/videodec/video_utils.h"
#include "core/libraries/videodec/videodec.h"
#include "core/libraries/videodec/videodec_error.h"

#define ALIGN_UP(v, a) (((v) + (a) - 1) / (a) * (a))

#define VIDEODEC_FALLBACK_MEMORY_SIZE ((u64)16 * 1024 * 1024)

/* A decoder, as the game's control block names it: H.264 */
typedef struct videodec_decoder {
    video_sw_decoder_t decoder;
} videodec_decoder_t;

static u64 compute_frame_size_bytes(s32 width, s32 height) {
    u32 aligned_width;
    u32 aligned_height;
    if (width <= 0 || height <= 0) {
        return 0;
    }
    aligned_width = ALIGN_UP((u32)width, 256u);
    aligned_height = ALIGN_UP((u32)height, 16u);
    return ((u64)aligned_width * (u64)aligned_height * 3) / 2;
}

static s32 compute_dpb_count(const OrbisVideodecConfigInfo* cfg) {
    if (cfg->maxDpbFrameCount > 0) {
        return cfg->maxDpbFrameCount;
    }
    return 8;
}

static void compute_worst_case_dimensions(const OrbisVideodecConfigInfo* cfg, s32* out_width,
                                          s32* out_height) {
    if (cfg->maxFrameWidth > 0 && cfg->maxFrameHeight > 0) {
        *out_width = cfg->maxFrameWidth;
        *out_height = cfg->maxFrameHeight;
        return;
    }
    *out_width = 1920;
    *out_height = 1080;
    if (cfg->maxLevel >= 150) {
        *out_width = 3840;
        *out_height = 2160;
    }
}

int PS4_SYSV_ABI sceVideodecCreateDecoder(const OrbisVideodecConfigInfo* pCfgInfoIn,
                                          const OrbisVideodecResourceInfo* pRsrcInfoIn,
                                          OrbisVideodecCtrl* pCtrlOut) {
    const char* const func = "sceVideodecCreateDecoder";
    videodec_decoder_t* decoder;
    c_log(C_LOG_LIB_VIDEODEC, C_LOG_LEVEL_INFO, func, "called");
    if (!pCfgInfoIn || !pRsrcInfoIn || !pCtrlOut) {
        c_log(C_LOG_LIB_VIDEODEC, C_LOG_LEVEL_ERROR, func, "Invalid arguments");
        return ORBIS_VIDEODEC_ERROR_ARGUMENT_POINTER;
    }
    if (pCfgInfoIn->thisSize != sizeof(OrbisVideodecConfigInfo) ||
        pRsrcInfoIn->thisSize != sizeof(OrbisVideodecResourceInfo)) {
        c_log(C_LOG_LIB_VIDEODEC, C_LOG_LEVEL_ERROR, func, "Invalid struct size");
        return ORBIS_VIDEODEC_ERROR_STRUCT_SIZE;
    }
    decoder = (videodec_decoder_t*)malloc(sizeof(*decoder));
    if (!decoder) {
        c_log(C_LOG_LIB_VIDEODEC, C_LOG_LEVEL_ERROR, func, "No memory for the decoder");
        return ORBIS_VIDEODEC_ERROR_API_FAIL;
    }
    (void)video_sw_decoder_init(&decoder->decoder, false);
    pCtrlOut->thisSize = sizeof(OrbisVideodecCtrl);
    pCtrlOut->handle = decoder;
    pCtrlOut->version = 1;
    return ORBIS_OK;
}

/* The ready picture written to the game's frame buffer */
static s32 decoder_output(videodec_decoder_t* self, OrbisVideodecFrameBuffer* pFrameBufferInOut,
                          OrbisVideodecPictureInfo* pPictureInfoOut) {
    const video_sw_decoder_t* dec = &self->decoder;
    u32 width;
    u32 pitch;
    u32 height;
    if (!video_sw_decoder_write_nv12(dec, (u8*)pFrameBufferInOut->pFrameBuffer,
                                     pFrameBufferInOut->frameBufferSize)) {
        c_log(C_LOG_LIB_VIDEODEC, C_LOG_LEVEL_ERROR, "Output",
              "The %ux%u picture does not fit the frame buffer", dec->width, dec->height);
        return ORBIS_VIDEODEC_ERROR_API_FAIL;
    }

    width = ALIGN_UP(dec->width, 16u);
    pitch = video_sw_decoder_pitch(dec);
    height = video_sw_decoder_aligned_height(dec);

    pPictureInfoOut->isValid = true;
    pPictureInfoOut->codecType = 0;
    pPictureInfoOut->frameWidth = width;
    pPictureInfoOut->framePitch = pitch;
    pPictureInfoOut->frameHeight = height;
    pPictureInfoOut->isErrorPic = false;
    pPictureInfoOut->ptsData = dec->meta.pts;
    pPictureInfoOut->attachedData = dec->meta.attached;

    pPictureInfoOut->codec.avc.colourPrimaries = video_sw_decoder_colour_primaries(dec);
    pPictureInfoOut->codec.avc.frameCropLeftOffset = 0;
    pPictureInfoOut->codec.avc.frameCropRightOffset = pitch - dec->width;
    pPictureInfoOut->codec.avc.frameCropTopOffset = 0;
    pPictureInfoOut->codec.avc.frameCropBottomOffset = height - dec->height;
    return ORBIS_OK;
}

int PS4_SYSV_ABI sceVideodecDecode(OrbisVideodecCtrl* pCtrlIn,
                                   const OrbisVideodecInputData* pInputDataIn,
                                   OrbisVideodecFrameBuffer* pFrameBufferInOut,
                                   OrbisVideodecPictureInfo* pPictureInfoOut) {
    const char* const func = "sceVideodecDecode";
    videodec_decoder_t* decoder;
    video_picture_meta_t meta;
    int ret;
#ifndef NDEBUG
    c_log(C_LOG_LIB_VIDEODEC, C_LOG_LEVEL_TRACE, func, "called");
#endif
    if (!pCtrlIn || !pInputDataIn || !pPictureInfoOut) {
        c_log(C_LOG_LIB_VIDEODEC, C_LOG_LEVEL_ERROR, func, "Invalid arguments");
        return ORBIS_VIDEODEC_ERROR_ARGUMENT_POINTER;
    }
    if (pCtrlIn->thisSize != sizeof(OrbisVideodecCtrl) ||
        pFrameBufferInOut->thisSize != sizeof(OrbisVideodecFrameBuffer)) {
        c_log(C_LOG_LIB_VIDEODEC, C_LOG_LEVEL_ERROR, func, "Invalid struct size");
        return ORBIS_VIDEODEC_ERROR_STRUCT_SIZE;
    }
    decoder = (videodec_decoder_t*)pCtrlIn->handle;
    if (!decoder) {
        c_log(C_LOG_LIB_VIDEODEC, C_LOG_LEVEL_ERROR, func, "Invalid decoder handle");
        return ORBIS_VIDEODEC_ERROR_HANDLE;
    }

    pPictureInfoOut->isValid = false;
    if (!pInputDataIn->pAuData) {
        return ORBIS_VIDEODEC_ERROR_AU_POINTER;
    }
    if (pInputDataIn->auSize == 0) {
        return ORBIS_VIDEODEC_ERROR_AU_SIZE;
    }
    if (!video_sw_decoder_is_open(&decoder->decoder)) {
        return ORBIS_VIDEODEC_ERROR_API_FAIL;
    }

    meta.pts = pInputDataIn->ptsData;
    meta.dts = pInputDataIn->dtsData;
    meta.attached = pInputDataIn->attachedData;
    ret = video_sw_decoder_decode(&decoder->decoder, (const u8*)pInputDataIn->pAuData,
                                  pInputDataIn->auSize, &meta);
    if (ret < 0) {
        c_log(C_LOG_LIB_VIDEODEC, C_LOG_LEVEL_ERROR, "Decode", "Error decoding the access unit: %d",
              ret);
        return ORBIS_VIDEODEC_ERROR_API_FAIL;
    }
    return ret ? decoder_output(decoder, pFrameBufferInOut, pPictureInfoOut) : ORBIS_OK;
}

int PS4_SYSV_ABI sceVideodecDeleteDecoder(OrbisVideodecCtrl* pCtrlIn) {
    videodec_decoder_t* decoder;
    c_log(C_LOG_LIB_VIDEODEC, C_LOG_LEVEL_INFO, "sceVideodecDeleteDecoder", "(STUBBED) called");
    decoder = (videodec_decoder_t*)pCtrlIn->handle;
    if (!decoder) {
        c_log(C_LOG_LIB_VIDEODEC, C_LOG_LEVEL_ERROR, "sceVideodecDeleteDecoder",
              "Invalid decoder handle");
        return ORBIS_VIDEODEC_ERROR_HANDLE;
    }
    video_sw_decoder_free(&decoder->decoder);
    free(decoder);
    return ORBIS_OK;
}

int PS4_SYSV_ABI sceVideodecFlush(OrbisVideodecCtrl* pCtrlIn,
                                  OrbisVideodecFrameBuffer* pFrameBufferInOut,
                                  OrbisVideodecPictureInfo* pPictureInfoOut) {
    const char* const func = "sceVideodecFlush";
    videodec_decoder_t* decoder;
    c_log(C_LOG_LIB_VIDEODEC, C_LOG_LEVEL_INFO, func, "called");
    if (!pFrameBufferInOut || !pPictureInfoOut) {
        c_log(C_LOG_LIB_VIDEODEC, C_LOG_LEVEL_ERROR, func, "Invalid arguments");
        return ORBIS_VIDEODEC_ERROR_ARGUMENT_POINTER;
    }
    if (pFrameBufferInOut->thisSize != sizeof(OrbisVideodecFrameBuffer) ||
        pPictureInfoOut->thisSize != sizeof(OrbisVideodecPictureInfo)) {
        c_log(C_LOG_LIB_VIDEODEC, C_LOG_LEVEL_ERROR, func, "Invalid struct size");
        return ORBIS_VIDEODEC_ERROR_STRUCT_SIZE;
    }
    decoder = (videodec_decoder_t*)pCtrlIn->handle;
    if (!decoder) {
        c_log(C_LOG_LIB_VIDEODEC, C_LOG_LEVEL_ERROR, func, "Invalid decoder handle");
        return ORBIS_VIDEODEC_ERROR_HANDLE;
    }
    pPictureInfoOut->isValid = false;
    /* One held picture a call: games flush once per picture they want */
    if (!video_sw_decoder_is_open(&decoder->decoder) ||
        !video_sw_decoder_drain(&decoder->decoder)) {
        return ORBIS_OK;
    }
    return decoder_output(decoder, pFrameBufferInOut, pPictureInfoOut);
}

int PS4_SYSV_ABI sceVideodecMapMemory(void) {
    c_log(C_LOG_LIB_VIDEODEC, C_LOG_LEVEL_ERROR, "sceVideodecMapMemory", "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI sceVideodecQueryResourceInfo(const OrbisVideodecConfigInfo* pCfgInfoIn,
                                              OrbisVideodecResourceInfo* pRsrcInfoOut) {
    const char* const func = "sceVideodecQueryResourceInfo";
    s32 width = 0;
    s32 height = 0;
    u64 frame_size;
    s32 dpb_count;
    u64 cpu_gpu_size = 0;
    u64 cpu_size = 0;
    u64 max_frame_buffer = 0;
    c_log(C_LOG_LIB_VIDEODEC, C_LOG_LEVEL_INFO, func, "called");
    if (!pCfgInfoIn || !pRsrcInfoOut) {
        c_log(C_LOG_LIB_VIDEODEC, C_LOG_LEVEL_ERROR, func, "Invalid arguments");
        return ORBIS_VIDEODEC_ERROR_ARGUMENT_POINTER;
    }
    if (pCfgInfoIn->thisSize != sizeof(OrbisVideodecConfigInfo) ||
        pRsrcInfoOut->thisSize != sizeof(OrbisVideodecResourceInfo)) {
        c_log(C_LOG_LIB_VIDEODEC, C_LOG_LEVEL_ERROR, func, "Invalid struct size");
        return ORBIS_VIDEODEC_ERROR_STRUCT_SIZE;
    }

    compute_worst_case_dimensions(pCfgInfoIn, &width, &height);
    frame_size = compute_frame_size_bytes(width, height);
    dpb_count = compute_dpb_count(pCfgInfoIn);

    if (frame_size == 0) {
        cpu_gpu_size = VIDEODEC_FALLBACK_MEMORY_SIZE;
        cpu_size = VIDEODEC_FALLBACK_MEMORY_SIZE;
        max_frame_buffer = VIDEODEC_FALLBACK_MEMORY_SIZE;
    } else {
        const u64 padded_frame = ALIGN_UP(frame_size, (u64)256) + 0x4000;
        const u64 surfaces = (u64)dpb_count + 2;
        max_frame_buffer = padded_frame;
        cpu_gpu_size = (padded_frame * surfaces) + (u64)8 * 1024 * 1024;
        cpu_size = (u64)16 * 1024 * 1024;
    }

    pRsrcInfoOut->thisSize = sizeof(OrbisVideodecResourceInfo);
    pRsrcInfoOut->pCpuMemory = NULL;
    pRsrcInfoOut->pCpuGpuMemory = NULL;
    pRsrcInfoOut->cpuGpuMemorySize = cpu_gpu_size;
    pRsrcInfoOut->cpuMemorySize = cpu_size;
    pRsrcInfoOut->maxFrameBufferSize = max_frame_buffer;
    pRsrcInfoOut->frameBufferAlignment = 0x100;
    return ORBIS_OK;
}

int PS4_SYSV_ABI sceVideodecReset(OrbisVideodecCtrl* pCtrlIn) {
    videodec_decoder_t* decoder;
    c_log(C_LOG_LIB_VIDEODEC, C_LOG_LEVEL_INFO, "sceVideodecReset", "(STUBBED) called");
    decoder = (videodec_decoder_t*)pCtrlIn->handle;
    video_sw_decoder_reset(&decoder->decoder);
    return ORBIS_OK;
}
