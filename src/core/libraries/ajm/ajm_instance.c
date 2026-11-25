/* SPDX-FileCopyrightText: Copyright 2024-2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "common/c_bridge.h"
#include "common/enum_names.h"
#include "core/libraries/ajm/ajm_instance.h"
#include "core/libraries/ajm/ajm_result.h"

/*
 * What the codecs share
 **/
u8 ajm_pcm_size(u32 format) {
    switch (format) {
    case AJM_FORMAT_ENCODING_S16:
        return sizeof(s16);
    case AJM_FORMAT_ENCODING_S32:
        return sizeof(s32);
    case AJM_FORMAT_ENCODING_FLOAT:
        return sizeof(float);
    default:
        C_ASSERT_MSG(0, "GetPCMSize", "Unreachable code!");
        return 0;
    }
}

bool ajm_gapless_is_end(const ajm_gapless_t* g) {
    return g->init.total_samples != 0 && g->current.total_samples == 0;
}

void ajm_gapless_reset(ajm_gapless_t* g) {
    g->current.total_samples = g->init.total_samples;
    g->current.skip_samples = g->init.skip_samples;
    g->current.skipped_samples = 0;
}

void ajm_input_advance(ajm_input_t* in, size_t n) {
    in->data += n;
    in->size -= n;
}

bool ajm_output_is_empty(const ajm_output_t* out) {
    return out->current == out->count;
}

size_t ajm_output_size(const ajm_output_t* out) {
    size_t result = 0;
    size_t i;
    for (i = out->current; i < out->count; i++) {
        result += out->chunks[i].size;
    }
    return result;
}

/* The current chunk advanced past @n bytes, and the next one taken once it is used up */
static void ajm_output_advance(ajm_output_t* out, size_t n) {
    ajm_span_t* chunk = &out->chunks[out->current];
    chunk->data += n;
    chunk->size -= n;
    if (chunk->size == 0) {
        out->current++;
    }
}

size_t ajm_output_write(ajm_output_t* out, const void* pcm, size_t count, size_t sample_bytes) {
    const u8* src = (const u8*)pcm;
    size_t samples_written = 0;
    while (count != 0 && !ajm_output_is_empty(out)) {
        ajm_span_t* chunk = &out->chunks[out->current];
        const size_t size = count * sample_bytes < chunk->size ? count * sample_bytes : chunk->size;
        const size_t nsamples = size / sample_bytes;
        memcpy(chunk->data, src, size);
        samples_written += nsamples;
        src += nsamples * sample_bytes;
        count -= nsamples;
        ajm_output_advance(out, size);
    }
    if (count != 0) {
        c_log(C_LOG_LIB_AJM, C_LOG_LEVEL_ERROR, "Write", "Could not write %zu samples", count);
    }
    return samples_written;
}

#define AJM_MAX_CHANNELS 8

size_t ajm_output_write_frames(ajm_output_t* out, size_t count, u32 channels, size_t sample_bytes,
                               void (*fill)(void* ctx, size_t first, size_t n, void* dst),
                               void* ctx) {
    const size_t frame_bytes = channels * sample_bytes;
    size_t done = 0;
    size_t samples = 0;
    while (done < count && !ajm_output_is_empty(out) && channels <= AJM_MAX_CHANNELS) {
        ajm_span_t* chunk = &out->chunks[out->current];
        const size_t whole = chunk->size / frame_bytes;
        size_t n;
        if (whole == 0 || ((uintptr_t)chunk->data % sample_bytes) != 0) {
            union {
                u8 bytes[AJM_MAX_CHANNELS * 4];
                float align;
            } frame;
            fill(ctx, done, 1, frame.bytes);
            samples += ajm_output_write(out, frame.bytes, channels, sample_bytes);
            ++done;
            continue;
        }
        n = whole < count - done ? whole : count - done;
        fill(ctx, done, n, chunk->data);
        ajm_output_advance(out, n * frame_bytes);
        done += n;
        samples += n * channels;
    }
    if (done < count) {
        c_log(C_LOG_LIB_AJM, C_LOG_LEVEL_ERROR, "WriteFrames", "Could not write %zu frames",
              count - done);
    }
    return samples;
}

/*
 * The instance
 **/
ajm_instance_t* ajm_instance_new(u32 codec_type, u64 flags) {
    ajm_instance_t* instance;
    const u32 format = AJM_INSTANCE_FLAGS_FORMAT(flags);
    const u32 codec_flags = AJM_INSTANCE_FLAGS_CODEC(flags);
    const u32 channels = AJM_INSTANCE_FLAGS_CHANNELS(flags);
    if (codec_type != AJM_CODEC_TYPE_AT9_DEC && codec_type != AJM_CODEC_TYPE_MP3_DEC &&
        codec_type != AJM_CODEC_TYPE_M4AAC_DEC) {
        c_log(C_LOG_LIB_AJM, C_LOG_LEVEL_CRITICAL, "AjmInstance", "Unimplemented codec type %s",
              enum_name(&enum_names_AjmCodecType, (int64_t)codec_type));
        C_ASSERT_MSG(0, "AjmInstance", "Unreachable code!");
        return NULL;
    }
    instance = (ajm_instance_t*)calloc(1, sizeof(*instance));
    if (!instance) {
        return NULL;
    }
    instance->codec_type = codec_type;
    instance->flags = flags;
    switch (codec_type) {
    case AJM_CODEC_TYPE_AT9_DEC:
        ajm_at9_init(&instance->codec.at9, format, codec_flags);
        break;
    case AJM_CODEC_TYPE_MP3_DEC:
        ajm_mp3_init(&instance->codec.mp3, format, codec_flags);
        break;
    default:
        ajm_aac_init(&instance->codec.aac, format, codec_flags, channels);
        break;
    }
    return instance;
}

void ajm_instance_delete(ajm_instance_t* instance) {
    if (!instance) {
        return;
    }
    switch (instance->codec_type) {
    case AJM_CODEC_TYPE_AT9_DEC:
        ajm_at9_free(&instance->codec.at9);
        break;
    case AJM_CODEC_TYPE_M4AAC_DEC:
        ajm_aac_free(&instance->codec.aac);
        break;
    default:
        break;
    }
    free(instance);
}

static void codec_reset(ajm_instance_t* instance) {
    switch (instance->codec_type) {
    case AJM_CODEC_TYPE_AT9_DEC:
        ajm_at9_reset(&instance->codec.at9);
        break;
    case AJM_CODEC_TYPE_MP3_DEC:
        ajm_mp3_reset(&instance->codec.mp3);
        break;
    default:
        ajm_aac_reset(&instance->codec.aac);
        break;
    }
}

static void codec_initialize(ajm_instance_t* instance, const void* buffer, u32 size) {
    switch (instance->codec_type) {
    case AJM_CODEC_TYPE_AT9_DEC:
        ajm_at9_initialize(&instance->codec.at9, buffer, size);
        break;
    case AJM_CODEC_TYPE_MP3_DEC:
        break;
    default:
        ajm_aac_initialize(&instance->codec.aac, buffer, size);
        break;
    }
}

static void codec_get_info(const ajm_instance_t* instance, void* out_info) {
    switch (instance->codec_type) {
    case AJM_CODEC_TYPE_AT9_DEC:
        ajm_at9_get_info(&instance->codec.at9, out_info);
        break;
    case AJM_CODEC_TYPE_MP3_DEC:
        ajm_mp3_get_info(&instance->codec.mp3, out_info);
        break;
    default:
        ajm_aac_get_info(&instance->codec.aac, out_info);
        break;
    }
}

static AjmSidebandFormat codec_get_format(const ajm_instance_t* instance) {
    switch (instance->codec_type) {
    case AJM_CODEC_TYPE_AT9_DEC:
        return ajm_at9_get_format(&instance->codec.at9);
    case AJM_CODEC_TYPE_MP3_DEC:
        return ajm_mp3_get_format(&instance->codec.mp3);
    default:
        return ajm_aac_get_format(&instance->codec.aac);
    }
}

static u32 codec_get_minimum_input_size(const ajm_instance_t* instance) {
    switch (instance->codec_type) {
    case AJM_CODEC_TYPE_AT9_DEC:
        return ajm_at9_get_minimum_input_size(&instance->codec.at9);
    case AJM_CODEC_TYPE_MP3_DEC:
        return ajm_mp3_get_minimum_input_size(&instance->codec.mp3);
    default:
        return ajm_aac_get_minimum_input_size(&instance->codec.aac);
    }
}

static u32 codec_get_next_frame_size(const ajm_instance_t* instance) {
    switch (instance->codec_type) {
    case AJM_CODEC_TYPE_AT9_DEC:
        return ajm_at9_get_next_frame_size(&instance->codec.at9, &instance->gapless);
    case AJM_CODEC_TYPE_MP3_DEC:
        return ajm_mp3_get_next_frame_size(&instance->codec.mp3, &instance->gapless);
    default:
        return ajm_aac_get_next_frame_size(&instance->codec.aac, &instance->gapless);
    }
}

static ajm_decoder_result_t codec_process_data(ajm_instance_t* instance, ajm_input_t* input,
                                               ajm_output_t* output) {
    switch (instance->codec_type) {
    case AJM_CODEC_TYPE_AT9_DEC:
        return ajm_at9_process_data(&instance->codec.at9, input, output, &instance->gapless);
    case AJM_CODEC_TYPE_MP3_DEC:
        return ajm_mp3_process_data(&instance->codec.mp3, input, output, &instance->gapless);
    default:
        return ajm_aac_process_data(&instance->codec.aac, input, output, &instance->gapless);
    }
}

static void ajm_instance_reset(ajm_instance_t* instance) {
    instance->total_samples = 0;
    ajm_gapless_reset(&instance->gapless);
    codec_reset(instance);
}

static bool ajm_instance_has_enough_space(const ajm_instance_t* instance,
                                          const ajm_output_t* output) {
    if (ajm_gapless_is_end(&instance->gapless)) {
        return true;
    }
    return ajm_output_size(output) >= codec_get_next_frame_size(instance);
}

/* The gapless counts the job gives: taken where they are not behind what was decoded */
static void ajm_instance_set_gapless(ajm_instance_t* instance, ajm_job_t* job) {
    const AjmSidebandGaplessDecode* params = &job->gapless_decode;
    ajm_gapless_t* g = &instance->gapless;
    const u32 samples_processed = g->init.total_samples - g->current.total_samples;
    const int samples_skipped = (int)g->init.skip_samples - (int)g->current.skip_samples;
    if (params->total_samples != 0 || params->skip_samples == 0) {
        if (params->total_samples >= samples_processed) {
            const s64 sample_difference = (s64)g->init.total_samples - params->total_samples;
            g->init.total_samples = params->total_samples;
            g->current.total_samples = (u32)((s64)g->current.total_samples - sample_difference);
        } else {
            c_log(C_LOG_LIB_AJM, C_LOG_LEVEL_WARNING, "ExecuteJob",
                  "ORBIS_AJM_RESULT_INVALID_PARAMETER");
            job->p_result->result |= ORBIS_AJM_RESULT_INVALID_PARAMETER;
        }
    }
    if (params->skip_samples != 0 || params->total_samples == 0) {
        if ((int)params->skip_samples >= samples_skipped) {
            const s32 sample_difference = (s32)g->init.skip_samples - params->skip_samples;
            g->init.skip_samples = params->skip_samples;
            g->current.skip_samples = (u16)((int)g->current.skip_samples - sample_difference);
        } else {
            c_log(C_LOG_LIB_AJM, C_LOG_LEVEL_WARNING, "ExecuteJob",
                  "ORBIS_AJM_RESULT_INVALID_PARAMETER");
            job->p_result->result |= ORBIS_AJM_RESULT_INVALID_PARAMETER;
        }
    }
}

void ajm_instance_execute_job(ajm_instance_t* instance, ajm_job_t* job) {
    const u32 control_flags = AJM_JOB_FLAGS_CONTROL(job->flags);
    ajm_input_t in_buf;
    size_t in_size;
    size_t out_size;
    u32 frames_decoded = 0;

    C_ASSERT_MSG(job->p_result != NULL, "ExecuteJob", "A job with no sideband output");
    job->p_result->result = 0;

    if (control_flags & AJM_JOB_CONTROL_RESET) {
#ifndef NDEBUG
        c_log(C_LOG_LIB_AJM, C_LOG_LEVEL_TRACE, "ExecuteJob", "Resetting instance %u",
              job->instance_id);
#endif
        ajm_instance_reset(instance);
    }
    if (job->has_init_params) {
#ifndef NDEBUG
        c_log(C_LOG_LIB_AJM, C_LOG_LEVEL_TRACE, "ExecuteJob", "Initializing instance %u",
              job->instance_id);
#endif
        codec_initialize(instance, &job->init_params, (u32)sizeof(job->init_params));
    }
    if (job->has_resample_parameters) {
        c_log(C_LOG_LIB_AJM, C_LOG_LEVEL_ERROR, "ExecuteJob", "Unimplemented: resample parameters");
        instance->resample_parameters = job->resample_parameters;
    }
    if (job->has_format) {
        c_log(C_LOG_LIB_AJM, C_LOG_LEVEL_ERROR, "ExecuteJob", "Unimplemented: format parameters");
        instance->format = job->format;
    }
    if (job->has_gapless_decode) {
        ajm_instance_set_gapless(instance, job);
    }

    in_buf.data = job->input;
    in_buf.size = job->input_size;
    in_size = in_buf.size;
    out_size = ajm_output_size(&job->output);

    if (job->input_size != 0) {
        for (;;) {
            ajm_decoder_result_t result;
            if (AJM_INSTANCE_FLAGS_GAPLESS_LOOP(instance->flags) &&
                ajm_gapless_is_end(&instance->gapless)) {
                ajm_gapless_reset(&instance->gapless);
                instance->total_samples = 0;
            }
            if (!ajm_instance_has_enough_space(instance, &job->output)) {
#ifndef NDEBUG
                c_log(C_LOG_LIB_AJM, C_LOG_LEVEL_TRACE, "ExecuteJob",
                      "ORBIS_AJM_RESULT_NOT_ENOUGH_ROOM (%zu < %u)", ajm_output_size(&job->output),
                      codec_get_next_frame_size(instance));
#endif
                job->p_result->result |= ORBIS_AJM_RESULT_NOT_ENOUGH_ROOM;
            }
            if (in_buf.size < codec_get_minimum_input_size(instance)) {
                job->p_result->result |= ORBIS_AJM_RESULT_PARTIAL_INPUT;
            }
            if (job->p_result->result != 0) {
                break;
            }
            result = codec_process_data(instance, &in_buf, &job->output);
            if (result.is_reset) {
                instance->total_samples = 0;
            }
            instance->total_samples += result.samples_written;
            frames_decoded += result.frames_decoded;
            if (result.result != 0) {
                job->p_result->result |= result.result;
                job->p_result->internal_result = result.internal_result;
                break;
            }
            if (!(AJM_JOB_FLAGS_RUN(job->flags) & AJM_JOB_RUN_MULTIPLE_FRAMES)) {
                break;
            }
        }
    }

    if (job->p_mframe) {
        job->p_mframe->num_frames = frames_decoded;
    }
    if (job->p_stream) {
        job->p_stream->input_consumed = (s32)(in_size - in_buf.size);
        job->p_stream->output_written = (s32)(out_size - ajm_output_size(&job->output));
        job->p_stream->total_decoded_samples = instance->total_samples;
    }
    if (job->p_format != NULL) {
        *job->p_format = codec_get_format(instance);
    }
    if (job->p_gapless_decode != NULL) {
        *job->p_gapless_decode = instance->gapless.current;
    }
    if (job->p_codec_info != NULL) {
        codec_get_info(instance, job->p_codec_info);
    }
}

/*
 * The statistics instance
 **/
void ajm_statistics_execute_job(ajm_job_t* job) {
    if (job->p_engine) {
        u32 idx;
        u32 ic;
        C_ASSERT_MSG(job->has_statistics_engine_parameters, "ExecuteJob",
                     "Engine statistics with no parameters");
        ic = job->statistics_engine_parameters.interval_count;
        job->p_engine->usage_batch = 0.05f;
        for (idx = 0; idx < ic; ++idx) {
            job->p_engine->usage_interval[idx] = 0.01f;
        }
    }
    if (job->p_engine_per_codec) {
        job->p_engine_per_codec->codec_count = 1;
        job->p_engine_per_codec->codec_id[0] = (u8)AJM_CODEC_TYPE_AT9_DEC;
        job->p_engine_per_codec->codec_percentage[0] = 0.01f;
    }
    if (job->p_memory) {
        job->p_memory->instance_free = 0x400000;
        job->p_memory->buffer_free = 0x400000;
        job->p_memory->batch_size = 0x4200;
        job->p_memory->input_size = 0x2000;
        job->p_memory->output_size = 0x2000;
        job->p_memory->small_size = 0x400;
    }
}
