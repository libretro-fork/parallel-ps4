/* SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef SHADPS4_AJM_AJM_INSTANCE_H
#define SHADPS4_AJM_AJM_INSTANCE_H

/* A decoder instance and the jobs a batch runs on it. Defined in ajm_instance.c, its codecs in
 * ajm_at9.c, ajm_mp3.c and ajm_aac.c. */

#include <stddef.h>

#include <boolean.h>
#include <retro_common_api.h>

#include "common/types.h"
#include "core/libraries/ajm/ajm.h"
#include "core/libraries/ajm/ajm_aac.h"
#include "core/libraries/ajm/ajm_at9.h"
#include "core/libraries/ajm/ajm_codec.h"
#include "core/libraries/ajm/ajm_mp3.h"

RETRO_BEGIN_DECLS

/* What a job asks, read from the batch: its input and its sideband, in the game's memory */
typedef struct ajm_job {
    u32 instance_id;
    u64 flags; /* AjmJobFlags */
    bool has_init_params;
    AjmSidebandInitParameters init_params;
    bool has_resample_parameters;
    AjmSidebandResampleParameters resample_parameters;
    bool has_statistics_engine_parameters;
    AjmSidebandStatisticsEngineParameters statistics_engine_parameters;
    bool has_format;
    AjmSidebandFormat format;
    bool has_gapless_decode;
    AjmSidebandGaplessDecode gapless_decode;
    /* The input: in place for one input chunk, else a copy of them one after another */
    const u8* input;
    size_t input_size;
    ajm_output_t output;
    AjmSidebandResult* p_result;
    AjmSidebandStream* p_stream;
    AjmSidebandFormat* p_format;
    AjmSidebandStatisticsMemory* p_memory;
    AjmSidebandStatisticsEnginePerCodec* p_engine_per_codec;
    AjmSidebandStatisticsEngine* p_engine;
    AjmSidebandGaplessDecode* p_gapless_decode;
    AjmSidebandMFrame* p_mframe;
    u8* p_codec_info;
} ajm_job_t;

typedef struct ajm_instance {
    u32 codec_type; /* AjmCodecType */
    u64 flags;      /* AjmInstanceFlags */
    AjmSidebandFormat format;
    ajm_gapless_t gapless;
    AjmSidebandResampleParameters resample_parameters;
    u32 total_samples;
    union {
        ajm_at9_t at9;
        ajm_mp3_t mp3;
        ajm_aac_t aac;
    } codec;
} ajm_instance_t;

/* NULL if it cannot be made */
ajm_instance_t* ajm_instance_new(u32 codec_type, u64 flags);
void ajm_instance_delete(ajm_instance_t* instance);
void ajm_instance_execute_job(ajm_instance_t* instance, ajm_job_t* job);
/* The job of AJM_INSTANCE_STATISTICS */
void ajm_statistics_execute_job(ajm_job_t* job);

RETRO_END_DECLS

#endif
