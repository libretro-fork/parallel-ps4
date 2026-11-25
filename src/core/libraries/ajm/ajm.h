/* SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef SHADPS4_AJM_AJM_H
#define SHADPS4_AJM_AJM_H

/* libSceAjm, the audio decoder. Defined in ajm.c, registered in core/libraries/libs.cpp. */

#include <stddef.h>

#include <boolean.h>
#include <retro_common_api.h>

#include "common/types.h"

RETRO_BEGIN_DECLS

#define ORBIS_AT9_CONFIG_DATA_SIZE 4
#define AJM_INSTANCE_STATISTICS 0x80000u

/* The enums: the values the game passes. Fields and parameters that hold them are u32 or u64. */
enum AjmCodecType {
    AJM_CODEC_TYPE_MP3_DEC = 0,
    AJM_CODEC_TYPE_AT9_DEC = 1,
    AJM_CODEC_TYPE_M4AAC_DEC = 2,
    AJM_CODEC_TYPE_MAX = 23
};

typedef struct AjmBatchInfo {
    void* pBuffer;
    u64 offset;
    u64 size;
} AjmBatchInfo;

typedef struct AjmBatchError {
    int error_code;
    const void* job_addr;
    u32 cmd_offset;
    const void* job_ra;
} AjmBatchError;

typedef struct AjmBuffer {
    u8* p_address;
    u64 size;
} AjmBuffer;

/* AjmJobControlFlags */
#define AJM_JOB_CONTROL_RESET 1u
#define AJM_JOB_CONTROL_INITIALIZE 2u
#define AJM_JOB_CONTROL_RESAMPLE 4u

/* AjmJobRunFlags */
#define AJM_JOB_RUN_GET_CODEC_INFO 1u
#define AJM_JOB_RUN_MULTIPLE_FRAMES 2u

/* AjmJobSidebandFlags */
#define AJM_JOB_SIDEBAND_GAPLESS_DECODE 1u
#define AJM_JOB_SIDEBAND_FORMAT 2u
#define AJM_JOB_SIDEBAND_STREAM 4u

/* AjmJobFlags, a u64: version 0-2, codec 3-10, run flags 11-12, control flags 13-15,
 * sideband flags 45-47 */
#define AJM_JOB_FLAGS_RUN(raw) ((u32)(((raw) >> 11) & 0x3u))
#define AJM_JOB_FLAGS_CONTROL(raw) ((u32)(((raw) >> 13) & 0x7u))
#define AJM_JOB_FLAGS_SIDEBAND(raw) ((u32)(((raw) >> 45) & 0x7u))

/* AjmStatisticsFlags */
#define AJM_STATISTICS_MEMORY 1u
#define AJM_STATISTICS_ENGINE_PER_CODEC (1u << 15)
#define AJM_STATISTICS_ENGINE (1u << 16)

/* AjmStatisticsJobFlags, a u64: version 0-2, statistics flags 15-31 */
#define AJM_STATISTICS_JOB_FLAGS(raw) ((u32)(((raw) >> 15) & 0x1FFFFu))

typedef struct AjmSidebandResult {
    s32 result;
    s32 internal_result;
} AjmSidebandResult;

typedef struct AjmSidebandMFrame {
    u32 num_frames;
    u32 reserved;
} AjmSidebandMFrame;

typedef struct AjmSidebandStream {
    s32 input_consumed;
    s32 output_written;
    u64 total_decoded_samples;
} AjmSidebandStream;

/* AjmFormatEncoding */
#define AJM_FORMAT_ENCODING_S16 0u
#define AJM_FORMAT_ENCODING_S32 1u
#define AJM_FORMAT_ENCODING_FLOAT 2u

typedef struct AjmSidebandFormat {
    u32 num_channels;
    u32 channel_mask;
    u32 sampl_freq;
    u32 sample_encoding; /* AjmFormatEncoding */
    u32 bitrate;
    u32 reserved;
} AjmSidebandFormat;

typedef struct AjmSidebandGaplessDecode {
    u32 total_samples;
    u16 skip_samples;
    u16 skipped_samples;
} AjmSidebandGaplessDecode;

typedef struct AjmSidebandResampleParameters {
    float ratio;
    u32 flags;
} AjmSidebandResampleParameters;

typedef struct AjmDecAt9InitializeParameters {
    u8 config_data[ORBIS_AT9_CONFIG_DATA_SIZE];
    u32 reserved;
} AjmDecAt9InitializeParameters;

typedef union AjmSidebandInitParameters {
    AjmDecAt9InitializeParameters at9;
    u8 reserved[8];
} AjmSidebandInitParameters;

typedef struct AjmSidebandStatisticsEngine {
    float usage_batch;
    float usage_interval[3];
} AjmSidebandStatisticsEngine;

typedef struct AjmSidebandStatisticsEnginePerCodec {
    u8 codec_count;
    u8 codec_id[3];
    float codec_percentage[3];
} AjmSidebandStatisticsEnginePerCodec;

typedef struct AjmSidebandStatisticsMemory {
    u32 instance_free;
    u32 buffer_free;
    u32 batch_size;
    u32 input_size;
    u32 output_size;
    u32 small_size;
} AjmSidebandStatisticsMemory;

typedef struct AjmSidebandStatisticsEngineParameters {
    u32 interval_count;
    float interval[3];
} AjmSidebandStatisticsEngineParameters;

/* AjmInstanceFlags, a u64: version 0-2, channels 3-6, format 7-9, gapless loop 10, codec 32-59 */
#define AJM_INSTANCE_FLAGS_VERSION(raw) ((u32)((raw) & 0x7u))
#define AJM_INSTANCE_FLAGS_CHANNELS(raw) ((u32)(((raw) >> 3) & 0xFu))
#define AJM_INSTANCE_FLAGS_FORMAT(raw) ((u32)(((raw) >> 7) & 0x7u))
#define AJM_INSTANCE_FLAGS_GAPLESS_LOOP(raw) ((u32)(((raw) >> 10) & 0x1u))
#define AJM_INSTANCE_FLAGS_CODEC(raw) ((u32)(((raw) >> 32) & 0xFFFFFFFu))

struct AjmDecMp3ParseFrame;

/* The channel mask of @num_channels channels (1, 2, 4, 6 or 8) */
u32 ajm_channel_mask(u32 num_channels);

int PS4_SYSV_ABI sceAjmBatchCancel(u32 context_id, u32 batch_id);
int PS4_SYSV_ABI sceAjmBatchErrorDump(void);
void* PS4_SYSV_ABI sceAjmBatchJobControlBufferRa(void* p_buffer, u32 instance_id, u64 flags,
                                                 void* p_sideband_input, size_t sideband_input_size,
                                                 void* p_sideband_output,
                                                 size_t sideband_output_size,
                                                 void* p_return_address);
void* PS4_SYSV_ABI sceAjmBatchJobInlineBuffer(void* p_buffer, const void* p_data_input,
                                              size_t data_input_size,
                                              const void** pp_batch_address);
void* PS4_SYSV_ABI sceAjmBatchJobRunBufferRa(void* p_buffer, u32 instance_id, u64 flags,
                                             void* p_data_input, size_t data_input_size,
                                             void* p_data_output, size_t data_output_size,
                                             void* p_sideband_output, size_t sideband_output_size,
                                             void* p_return_address);
void* PS4_SYSV_ABI sceAjmBatchJobRunSplitBufferRa(
    void* p_buffer, u32 instance_id, u64 flags, const AjmBuffer* p_data_input_buffers,
    size_t num_data_input_buffers, const AjmBuffer* p_data_output_buffers,
    size_t num_data_output_buffers, void* p_sideband_output, size_t sideband_output_size,
    void* p_return_address);
int PS4_SYSV_ABI sceAjmBatchStartBuffer(u32 context, u8* batch, u32 batch_size, int priority,
                                        AjmBatchError* batch_error, u32* out_batch_id);
int PS4_SYSV_ABI sceAjmBatchWait(u32 context, u32 batch_id, u32 timeout,
                                 AjmBatchError* batch_error);
int PS4_SYSV_ABI sceAjmDecAt9ParseConfigData(void);
int PS4_SYSV_ABI sceAjmDecMp3ParseFrame(const u8* stream, u32 stream_size, int parse_ofl,
                                        struct AjmDecMp3ParseFrame* frame);
int PS4_SYSV_ABI sceAjmFinalize(void);
int PS4_SYSV_ABI sceAjmInitialize(s64 reserved, u32* out_context);
u32 PS4_SYSV_ABI sceAjmInstanceCodecType(u32 instance_id);
int PS4_SYSV_ABI sceAjmInstanceCreate(u32 context, u32 codec_type, u64 flags, u32* instance);
int PS4_SYSV_ABI sceAjmInstanceDestroy(u32 context, u32 instance);
int PS4_SYSV_ABI sceAjmInstanceExtend(void);
int PS4_SYSV_ABI sceAjmInstanceSwitch(void);
int PS4_SYSV_ABI sceAjmMemoryRegister(u32 context_id, void* ptr, size_t num_pages);
int PS4_SYSV_ABI sceAjmMemoryUnregister(u32 context_id, void* ptr);
int PS4_SYSV_ABI sceAjmModuleRegister(u32 context, u32 codec_type, s64 reserved);
int PS4_SYSV_ABI sceAjmModuleUnregister(void);
int PS4_SYSV_ABI sceAjmStrError(void);

RETRO_END_DECLS

#endif
