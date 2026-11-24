/* SPDX-FileCopyrightText: Copyright 2024-2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <stdlib.h>
#include <string.h>

#include "common/c_bridge.h"
#include "core/libraries/error_codes.h"
#include "core/libraries/videodec/video_utils.h"
#include "core/libraries/videodec/videodec2.h"
#include "core/libraries/videodec/videodec_error.h"

#define ALIGN_UP(v, a) (((v) + (a) - 1) / (a) * (a))

/* Fake minimum memory size for querying */
#define VIDEODEC2_MINIMUM_MEMORY_SIZE ((u64)16 * 1024 * 1024)

/* A decoder: the software decoder its handle names */
struct videodec2_decoder {
    video_sw_decoder_t decoder;
};

static u64 compute_frame_size_bytes(s32 width, s32 height) {
    u32 aligned_width;
    u32 aligned_height;
    if (width <= 0 || height <= 0) {
        return 0;
    }
    aligned_width = ALIGN_UP((u32)width, 64u);
    aligned_height = ALIGN_UP((u32)height, 16u);
    return ((u64)aligned_width * (u64)aligned_height * 3) / 2;
}

static void compute_worst_case_dimensions(const OrbisVideodec2DecoderConfigInfo* cfg,
                                          s32* out_width, s32* out_height) {
    if (cfg->max_frame_width > 0 && cfg->max_frame_height > 0) {
        *out_width = cfg->max_frame_width;
        *out_height = cfg->max_frame_height;
        return;
    }
    *out_width = 1920;
    *out_height = 1080;
    if (cfg->max_level >= 150) {
        *out_width = 3840;
        *out_height = 2160;
    }
}

s32 PS4_SYSV_ABI
sceVideodec2QueryComputeMemoryInfo(OrbisVideodec2ComputeMemoryInfo* compute_mem_info) {
    c_log(C_LOG_LIB_VDEC2, C_LOG_LEVEL_INFO, "sceVideodec2QueryComputeMemoryInfo", "called");
    if (!compute_mem_info) {
        c_log(C_LOG_LIB_VDEC2, C_LOG_LEVEL_ERROR, "sceVideodec2QueryComputeMemoryInfo",
              "Invalid arguments");
        return ORBIS_VIDEODEC2_ERROR_ARGUMENT_POINTER;
    }
    if (compute_mem_info->this_size != sizeof(OrbisVideodec2ComputeMemoryInfo)) {
        c_log(C_LOG_LIB_VDEC2, C_LOG_LEVEL_ERROR, "sceVideodec2QueryComputeMemoryInfo",
              "Invalid struct size");
        return ORBIS_VIDEODEC2_ERROR_STRUCT_SIZE;
    }
    compute_mem_info->cpu_gpu_memory = NULL;
    compute_mem_info->cpu_gpu_memory_size = VIDEODEC2_MINIMUM_MEMORY_SIZE;
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI
sceVideodec2AllocateComputeQueue(const OrbisVideodec2ComputeConfigInfo* compute_cfg_info,
                                 const OrbisVideodec2ComputeMemoryInfo* compute_mem_info,
                                 OrbisVideodec2ComputeQueue* compute_queue) {
    const char* const func = "sceVideodec2AllocateComputeQueue";
    c_log(C_LOG_LIB_VDEC2, C_LOG_LEVEL_WARNING, func, "called");
    if (!compute_cfg_info || !compute_mem_info || !compute_queue) {
        c_log(C_LOG_LIB_VDEC2, C_LOG_LEVEL_ERROR, func, "Invalid arguments");
        return ORBIS_VIDEODEC2_ERROR_ARGUMENT_POINTER;
    }
    if (compute_cfg_info->this_size != sizeof(OrbisVideodec2ComputeConfigInfo) ||
        compute_mem_info->this_size != sizeof(OrbisVideodec2ComputeMemoryInfo)) {
        c_log(C_LOG_LIB_VDEC2, C_LOG_LEVEL_ERROR, func, "Invalid struct size");
        return ORBIS_VIDEODEC2_ERROR_STRUCT_SIZE;
    }
    if (compute_cfg_info->reserved0 != 0 || compute_cfg_info->reserved1 != 0) {
        c_log(C_LOG_LIB_VDEC2, C_LOG_LEVEL_ERROR, func, "Invalid compute config");
        return ORBIS_VIDEODEC2_ERROR_CONFIG_INFO;
    }
    if (compute_cfg_info->compute_pipe_id > 4) {
        c_log(C_LOG_LIB_VDEC2, C_LOG_LEVEL_ERROR, func, "Invalid compute pipe id");
        return ORBIS_VIDEODEC2_ERROR_COMPUTE_PIPE_ID;
    }
    if (compute_cfg_info->compute_queue_id > 7) {
        c_log(C_LOG_LIB_VDEC2, C_LOG_LEVEL_ERROR, func, "Invalid compute queue id");
        return ORBIS_VIDEODEC2_ERROR_COMPUTE_QUEUE_ID;
    }
    if (!compute_mem_info->cpu_gpu_memory) {
        c_log(C_LOG_LIB_VDEC2, C_LOG_LEVEL_ERROR, func, "Invalid memory pointer");
        return ORBIS_VIDEODEC2_ERROR_MEMORY_POINTER;
    }
    /* The real library returns a pointer to memory inside cpuGpuMemory */
    *compute_queue = compute_mem_info->cpu_gpu_memory;
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceVideodec2ReleaseComputeQueue(OrbisVideodec2ComputeQueue compute_queue) {
    c_log(C_LOG_LIB_VDEC2, C_LOG_LEVEL_INFO, "sceVideodec2ReleaseComputeQueue", "called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI
sceVideodec2QueryDecoderMemoryInfo(const OrbisVideodec2DecoderConfigInfo* decoder_cfg_info,
                                   OrbisVideodec2DecoderMemoryInfo* decoder_mem_info) {
    const char* const func = "sceVideodec2QueryDecoderMemoryInfo";
    s32 width = 0;
    s32 height = 0;
    u64 frame_size;
    u64 max_frame_buffer;
    c_log(C_LOG_LIB_VDEC2, C_LOG_LEVEL_INFO, func, "called");
    if (!decoder_cfg_info || !decoder_mem_info) {
        c_log(C_LOG_LIB_VDEC2, C_LOG_LEVEL_ERROR, func, "Invalid arguments");
        return ORBIS_VIDEODEC2_ERROR_ARGUMENT_POINTER;
    }
    if (decoder_cfg_info->this_size != sizeof(OrbisVideodec2DecoderConfigInfo) ||
        decoder_mem_info->this_size != sizeof(OrbisVideodec2DecoderMemoryInfo)) {
        c_log(C_LOG_LIB_VDEC2, C_LOG_LEVEL_ERROR, func, "Invalid struct size");
        return ORBIS_VIDEODEC2_ERROR_STRUCT_SIZE;
    }
    compute_worst_case_dimensions(decoder_cfg_info, &width, &height);
    frame_size = compute_frame_size_bytes(width, height);
    if (frame_size == 0) {
        max_frame_buffer = VIDEODEC2_MINIMUM_MEMORY_SIZE;
    } else {
        max_frame_buffer = ALIGN_UP(frame_size, (u64)256) + 0x4000;
    }
    decoder_mem_info->cpu_memory = NULL;
    decoder_mem_info->gpu_memory = NULL;
    decoder_mem_info->cpu_gpu_memory = NULL;
    decoder_mem_info->cpu_gpu_memory_size = VIDEODEC2_MINIMUM_MEMORY_SIZE;
    decoder_mem_info->cpu_memory_size = VIDEODEC2_MINIMUM_MEMORY_SIZE;
    decoder_mem_info->gpu_memory_size = VIDEODEC2_MINIMUM_MEMORY_SIZE;
    decoder_mem_info->max_frame_buffer_size = max_frame_buffer;
    decoder_mem_info->frame_buffer_alignment = 0x100;
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceVideodec2CreateDecoder(const OrbisVideodec2DecoderConfigInfo* decoder_cfg_info,
                                           const OrbisVideodec2DecoderMemoryInfo* decoder_mem_info,
                                           OrbisVideodec2Decoder* decoder) {
    const char* const func = "sceVideodec2CreateDecoder";
    struct videodec2_decoder* made;
    c_log(C_LOG_LIB_VDEC2, C_LOG_LEVEL_INFO, func, "called");
    if (!decoder_cfg_info || !decoder_mem_info || !decoder) {
        c_log(C_LOG_LIB_VDEC2, C_LOG_LEVEL_ERROR, func, "Invalid arguments");
        return ORBIS_VIDEODEC2_ERROR_ARGUMENT_POINTER;
    }
    if (decoder_cfg_info->this_size != sizeof(OrbisVideodec2DecoderConfigInfo) ||
        decoder_mem_info->this_size != sizeof(OrbisVideodec2DecoderMemoryInfo)) {
        c_log(C_LOG_LIB_VDEC2, C_LOG_LEVEL_ERROR, func, "Invalid struct size");
        return ORBIS_VIDEODEC2_ERROR_STRUCT_SIZE;
    }
    made = (struct videodec2_decoder*)malloc(sizeof(*made));
    if (!made) {
        c_log(C_LOG_LIB_VDEC2, C_LOG_LEVEL_ERROR, func, "No memory for the decoder");
        return ORBIS_VIDEODEC2_ERROR_API_FAIL;
    }
    (void)video_sw_decoder_init(&made->decoder,
                                decoder_cfg_info->codec_type != ORBIS_VIDEODEC2_CODEC_TYPE_AVC);
    *decoder = made;
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceVideodec2DeleteDecoder(OrbisVideodec2Decoder decoder) {
    c_log(C_LOG_LIB_VDEC2, C_LOG_LEVEL_INFO, "sceVideodec2DeleteDecoder", "called");
    if (!decoder) {
        c_log(C_LOG_LIB_VDEC2, C_LOG_LEVEL_ERROR, "sceVideodec2DeleteDecoder", "Invalid arguments");
        return ORBIS_VIDEODEC2_ERROR_DECODER_INSTANCE;
    }
    video_sw_decoder_free(&decoder->decoder);
    free(decoder);
    return ORBIS_OK;
}

/* The ready picture written to the game's frame buffer, its picture info after it */
static s32 decoder_output(struct videodec2_decoder* self, OrbisVideodec2FrameBuffer* frame_buffer,
                          OrbisVideodec2OutputInfo* output_info) {
    const video_sw_decoder_t* dec = &self->decoder;
    const bool is_avc = !video_sw_decoder_is_hevc(dec);
    const u64 info_size =
        is_avc ? sizeof(OrbisVideodec2AvcPictureInfo) : sizeof(OrbisVideodec2HevcPictureInfo);
    u32 width;
    u32 pitch;
    u32 height;
    u8* info;
    if (frame_buffer->frame_buffer_size < info_size ||
        !video_sw_decoder_write_nv12(dec, (u8*)frame_buffer->frame_buffer,
                                     frame_buffer->frame_buffer_size - info_size)) {
        c_log(C_LOG_LIB_VDEC2, C_LOG_LEVEL_ERROR, "Output",
              "The %ux%u picture does not fit the frame buffer", dec->width, dec->height);
        return ORBIS_VIDEODEC2_ERROR_API_FAIL;
    }
    frame_buffer->is_accepted = true;

    width = ALIGN_UP(dec->width, 16u);
    pitch = video_sw_decoder_pitch(dec);
    height = video_sw_decoder_aligned_height(dec);

    output_info->is_valid = true;
    output_info->is_error_frame = false;
    output_info->picture_count = 1; /* TODO: 2 pictures for interlaced video */
    output_info->codec_type =
        is_avc ? ORBIS_VIDEODEC2_CODEC_TYPE_AVC : ORBIS_VIDEODEC2_CODEC_TYPE_HEVC;
    output_info->frame_width = width;
    output_info->frame_pitch = pitch;
    output_info->frame_height = height;
    output_info->frame_buffer = frame_buffer->frame_buffer;
    output_info->frame_buffer_size = video_sw_decoder_nv12_size(dec);

    /* Only set frameFormat and framePitchInBytes if the game uses the newer struct version */
    if (output_info->this_size == sizeof(OrbisVideodec2OutputInfo)) {
        output_info->frame_format = 0;
        output_info->frame_pitch_in_bytes = pitch;
    }

    info = (u8*)output_info->frame_buffer + output_info->frame_buffer_size;
    if (is_avc) {
        OrbisVideodec2AvcPictureInfo* picture_info = (OrbisVideodec2AvcPictureInfo*)(void*)info;
        memset(picture_info, 0, sizeof(*picture_info));
        picture_info->is_valid = true;
        picture_info->pts_data = dec->meta.pts;
        picture_info->dts_data = dec->meta.dts;
        picture_info->attached_data = dec->meta.attached;
        picture_info->frame_crop_top_offset = 0;
        picture_info->frame_crop_left_offset = 0;
        picture_info->frame_crop_right_offset = pitch - dec->width;
        picture_info->frame_crop_bottom_offset = height - dec->height;
    } else {
        OrbisVideodec2HevcPictureInfo* picture_info = (OrbisVideodec2HevcPictureInfo*)(void*)info;
        memset(picture_info, 0, sizeof(*picture_info));
        picture_info->is_valid = true;
        picture_info->pts_data = dec->meta.pts;
        picture_info->dts_data = dec->meta.dts;
        picture_info->attached_data = dec->meta.attached;
        picture_info->frame_crop_top_offset = 0;
        picture_info->frame_crop_left_offset = 0;
        picture_info->frame_crop_right_offset = pitch - dec->width;
        picture_info->frame_crop_bottom_offset = height - dec->height;
    }
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceVideodec2Decode(OrbisVideodec2Decoder decoder,
                                    const OrbisVideodec2InputData* input_data,
                                    OrbisVideodec2FrameBuffer* frame_buffer,
                                    OrbisVideodec2OutputInfo* output_info) {
    const char* const func = "sceVideodec2Decode";
    video_picture_meta_t meta;
    int ret;
#ifndef NDEBUG
    c_log(C_LOG_LIB_VDEC2, C_LOG_LEVEL_TRACE, "sceVideodec2Decode", "called");
#endif
    if (!decoder) {
        c_log(C_LOG_LIB_VDEC2, C_LOG_LEVEL_ERROR, func, "Invalid decoder instance");
        return ORBIS_VIDEODEC2_ERROR_DECODER_INSTANCE;
    }
    if (!input_data || !frame_buffer || !output_info) {
        c_log(C_LOG_LIB_VDEC2, C_LOG_LEVEL_ERROR, func, "Invalid arguments");
        return ORBIS_VIDEODEC2_ERROR_ARGUMENT_POINTER;
    }
    if (input_data->this_size != sizeof(OrbisVideodec2InputData) ||
        frame_buffer->this_size != sizeof(OrbisVideodec2FrameBuffer)) {
        c_log(C_LOG_LIB_VDEC2, C_LOG_LEVEL_ERROR, func, "Invalid struct size");
        return ORBIS_VIDEODEC2_ERROR_STRUCT_SIZE;
    }

    frame_buffer->is_accepted = false;
    output_info->is_valid = false;
    output_info->picture_count = 0;

    if (!input_data->au_data) {
        c_log(C_LOG_LIB_VDEC2, C_LOG_LEVEL_ERROR, "Decode", "Invalid access unit data pointer");
        return ORBIS_VIDEODEC2_ERROR_ACCESS_UNIT_POINTER;
    }
    if (input_data->au_size == 0) {
        c_log(C_LOG_LIB_VDEC2, C_LOG_LEVEL_ERROR, "Decode", "Invalid access unit size");
        return ORBIS_VIDEODEC2_ERROR_ACCESS_UNIT_SIZE;
    }
    if (!video_sw_decoder_is_open(&decoder->decoder)) {
        return ORBIS_VIDEODEC2_ERROR_API_FAIL;
    }

    meta.pts = input_data->pts_data;
    meta.dts = input_data->dts_data;
    meta.attached = input_data->attached_data;
    ret = video_sw_decoder_decode(&decoder->decoder, (const u8*)input_data->au_data,
                                  input_data->au_size, &meta);
    if (ret < 0) {
        c_log(C_LOG_LIB_VDEC2, C_LOG_LEVEL_ERROR, "Decode", "Error decoding the access unit: %d",
              ret);
        return ORBIS_VIDEODEC2_ERROR_API_FAIL;
    }
    return ret ? decoder_output(decoder, frame_buffer, output_info) : ORBIS_OK;
}

s32 PS4_SYSV_ABI sceVideodec2Flush(OrbisVideodec2Decoder decoder,
                                   OrbisVideodec2FrameBuffer* frame_buffer,
                                   OrbisVideodec2OutputInfo* output_info) {
    const char* const func = "sceVideodec2Flush";
    c_log(C_LOG_LIB_VDEC2, C_LOG_LEVEL_INFO, func, "called");
    if (!decoder) {
        c_log(C_LOG_LIB_VDEC2, C_LOG_LEVEL_ERROR, func, "Invalid decoder instance");
        return ORBIS_VIDEODEC2_ERROR_DECODER_INSTANCE;
    }
    if (!frame_buffer || !output_info) {
        c_log(C_LOG_LIB_VDEC2, C_LOG_LEVEL_ERROR, func, "Invalid arguments");
        return ORBIS_VIDEODEC2_ERROR_ARGUMENT_POINTER;
    }
    if (frame_buffer->this_size != sizeof(OrbisVideodec2FrameBuffer) ||
        (output_info->this_size | 8) != sizeof(OrbisVideodec2OutputInfo)) {
        c_log(C_LOG_LIB_VDEC2, C_LOG_LEVEL_ERROR, func, "Invalid struct size");
        return ORBIS_VIDEODEC2_ERROR_STRUCT_SIZE;
    }
    frame_buffer->is_accepted = false;
    output_info->is_valid = false;
    output_info->picture_count = 0;
    if (!video_sw_decoder_is_open(&decoder->decoder) ||
        !video_sw_decoder_drain(&decoder->decoder)) {
        return ORBIS_OK;
    }
    return decoder_output(decoder, frame_buffer, output_info);
}

s32 PS4_SYSV_ABI sceVideodec2Reset(OrbisVideodec2Decoder decoder) {
    c_log(C_LOG_LIB_VDEC2, C_LOG_LEVEL_INFO, "sceVideodec2Reset", "called");
    if (!decoder) {
        c_log(C_LOG_LIB_VDEC2, C_LOG_LEVEL_ERROR, "sceVideodec2Reset", "Invalid decoder instance");
        return ORBIS_VIDEODEC2_ERROR_DECODER_INSTANCE;
    }
    video_sw_decoder_reset(&decoder->decoder);
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceVideodec2GetPictureInfo(const OrbisVideodec2OutputInfo* output_info,
                                            void* p_1st_picture_info, void* p_2nd_picture_info) {
    const char* const func = "sceVideodec2GetPictureInfo";
#ifndef NDEBUG
    c_log(C_LOG_LIB_VDEC2, C_LOG_LEVEL_TRACE, "sceVideodec2GetPictureInfo", "called");
#endif
    if (!output_info) {
        c_log(C_LOG_LIB_VDEC2, C_LOG_LEVEL_ERROR, func, "Invalid arguments");
        return ORBIS_VIDEODEC2_ERROR_ARGUMENT_POINTER;
    }
    if ((output_info->this_size | 8) != sizeof(OrbisVideodec2OutputInfo)) {
        c_log(C_LOG_LIB_VDEC2, C_LOG_LEVEL_ERROR, func, "Invalid struct size");
        return ORBIS_VIDEODEC2_ERROR_STRUCT_SIZE;
    }
    if (output_info->picture_count == 0) {
        c_log(C_LOG_LIB_VDEC2, C_LOG_LEVEL_ERROR, func, "No picture info available");
        return ORBIS_OK;
    }
    if (p_1st_picture_info) {
        u64 size;
        const u8* picture_info =
            (const u8*)output_info->frame_buffer + output_info->frame_buffer_size;
        memcpy(&size, p_1st_picture_info, sizeof(size));
        /* Copy the requested picture data to the output omitting picture size field */
        memcpy((u8*)p_1st_picture_info + sizeof(u64), picture_info + sizeof(u64),
               (size_t)(size - sizeof(u64)));
    }
    if (output_info->picture_count > 1) {
        c_unreachable(__FILE__, __LINE__, func, "");
    }
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceVideodec2GetAvcPictureInfo(const OrbisVideodec2OutputInfo* output_info,
                                               void* p_1st_picture_info, void* p_2nd_picture_info) {
#ifndef NDEBUG
    c_log(C_LOG_LIB_VDEC2, C_LOG_LEVEL_TRACE, "sceVideodec2GetAvcPictureInfo", "called");
#endif
    return sceVideodec2GetPictureInfo(output_info, p_1st_picture_info, p_2nd_picture_info);
}

s32 PS4_SYSV_ABI sceVideodec2GetHevcPictureInfo(const OrbisVideodec2OutputInfo* output_info,
                                                OrbisVideodec2HevcPictureInfo* picture_info) {
#ifndef NDEBUG
    c_log(C_LOG_LIB_VDEC2, C_LOG_LEVEL_TRACE, "sceVideodec2GetHevcPictureInfo", "called");
#endif
    return sceVideodec2GetPictureInfo(output_info, picture_info, NULL);
}
