/* SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <stdlib.h>
#include <string.h>

#include "common/c_bridge.h"
#include "core/libraries/ajm/ajm_context.h"
#include "core/libraries/ajm/ajm_instance.h"

/* The chunks' identifiers */
#define AJM_IDENT_JOB 0
#define AJM_IDENT_INPUT_RUN_BUF 1
#define AJM_IDENT_INPUT_CONTROL_BUF 2
#define AJM_IDENT_CONTROL_FLAGS 3
#define AJM_IDENT_RUN_FLAGS 4
#define AJM_IDENT_RETURN_ADDRESS_BUF 6
#define AJM_IDENT_INLINE_BUF 7
#define AJM_IDENT_OUTPUT_RUN_BUF 17
#define AJM_IDENT_OUTPUT_CONTROL_BUF 18

/* A chunk's header, a u32: ident 0-5, payload 6-25, the rest reserved */
#define AJM_CHUNK_IDENT(h) ((h) & 0x3Fu)
#define AJM_CHUNK_PAYLOAD(h) (((h) >> 6) & 0xFFFFFu)

/* AjmChunkJob: header, size (8 bytes); AjmChunkFlags: header, flags_low (8 bytes);
 * AjmChunkBuffer: header, size, p_address (16 bytes) */
#define AJM_CHUNK_JOB_SIZE 8
#define AJM_CHUNK_FLAGS_SIZE 8
#define AJM_CHUNK_BUFFER_SIZE 16

/* Output buffers kept on the stack; more are allocated */
#define AJM_JOB_OUTPUT_BUFFERS 16

static u32 rd32(const u8* p) {
    u32 v;
    memcpy(&v, p, sizeof(v));
    return v;
}

static void* rdptr(const u8* p) {
    void* v;
    memcpy(&v, p, sizeof(v));
    return v;
}

static void wr32(u8* p, u32 v) {
    memcpy(p, &v, sizeof(v));
}

static void wrptr(u8* p, const void* v) {
    memcpy(p, &v, sizeof(v));
}

/* A chunk's header written as its bit fields are: the reserved bits stay as they were */
static void wr_header(u8* p, u32 ident, u32 payload) {
    u32 h = rd32(p);
    h = (h & ~0x3Fu) | (ident & 0x3Fu);
    h = (h & ~(0xFFFFFu << 6)) | ((payload & 0xFFFFFu) << 6);
    wr32(p, h);
}

/* A buffer chunk, as the job reads it */
typedef struct ajm_chunk_buffer {
    u32 size;
    u8* p_address;
} ajm_chunk_buffer_t;

static ajm_chunk_buffer_t rd_chunk_buffer(const u8* p) {
    ajm_chunk_buffer_t b;
    b.size = rd32(p + 4);
    b.p_address = (u8*)rdptr(p + 8);
    return b;
}

/* What one pass over a job's chunks finds */
typedef struct ajm_job_chunks {
    bool has_flags;
    u64 flags;
    bool has_input_control;
    ajm_chunk_buffer_t input_control;
    bool has_output_control;
    ajm_chunk_buffer_t output_control;
    size_t num_inputs;   /* input run buffers with bytes in them */
    size_t input_size;   /* their bytes */
    const u8* input_one; /* the first of them */
    size_t num_outputs;  /* output run buffers kept */
} ajm_job_chunks_t;

static void ajm_read_flags(ajm_job_chunks_t* chunks, const u8* p) {
    C_ASSERT_MSG(!chunks->has_flags, "AjmJobFromBatchBuffer",
                 "Only one instance of job flags is allowed per job");
    chunks->has_flags = true;
    chunks->flags = ((u64)AJM_CHUNK_PAYLOAD(rd32(p)) << 32) + rd32(p + 4);
}

static void ajm_read_control(bool* has, ajm_chunk_buffer_t* out, const u8* p, const char* what) {
    ajm_chunk_buffer_t b;
    C_ASSERT_MSG(!*has, "AjmJobFromBatchBuffer", what);
    b = rd_chunk_buffer(p);
    if (b.p_address != NULL && b.size != 0) {
        *has = true;
        *out = b;
    }
}

/* One pass over the chunks of a job, [@p, @end): @outputs, when given, gets the output run
 * buffers, and @input the input run buffers one after another */
static void ajm_scan_job(const u8* p, const u8* end, bool statistics, ajm_job_chunks_t* chunks,
                         ajm_span_t* outputs, u8* input) {
    size_t input_at = 0;
    size_t output_at = 0;
    while (p < end) {
        const u32 ident = AJM_CHUNK_IDENT(rd32(p));
        switch (ident) {
        case AJM_IDENT_INPUT_RUN_BUF: {
            const ajm_chunk_buffer_t b = rd_chunk_buffer(p);
            if (statistics) {
                goto unknown;
            }
            if (input) {
                if (b.size != 0) {
                    memcpy(input + input_at, b.p_address, b.size);
                }
                input_at += b.size;
            } else if (b.size != 0) {
                if (chunks->num_inputs == 0) {
                    chunks->input_one = b.p_address;
                }
                chunks->num_inputs++;
                chunks->input_size += b.size;
            }
            p += AJM_CHUNK_BUFFER_SIZE;
            break;
        }
        case AJM_IDENT_INPUT_CONTROL_BUF:
            if (!outputs && !input) {
                ajm_read_control(&chunks->has_input_control, &chunks->input_control, p,
                                 "Only one instance of input control buffer is allowed per job");
            }
            p += AJM_CHUNK_BUFFER_SIZE;
            break;
        case AJM_IDENT_RUN_FLAGS:
            if (statistics) {
                goto unknown;
            }
            /* fallthrough */
        case AJM_IDENT_CONTROL_FLAGS:
            if (!outputs && !input) {
                ajm_read_flags(chunks, p);
            }
            p += AJM_CHUNK_FLAGS_SIZE;
            break;
        case AJM_IDENT_RETURN_ADDRESS_BUF:
            /* Ignore return address buffers. */
            p += AJM_CHUNK_BUFFER_SIZE;
            break;
        case AJM_IDENT_OUTPUT_RUN_BUF: {
            const ajm_chunk_buffer_t b = rd_chunk_buffer(p);
            if (statistics) {
                goto unknown;
            }
            if (b.p_address != NULL && b.size != 0) {
                if (outputs) {
                    outputs[output_at].data = b.p_address;
                    outputs[output_at].size = b.size;
                    output_at++;
                } else if (!input) {
                    chunks->num_outputs++;
                }
            }
            p += AJM_CHUNK_BUFFER_SIZE;
            break;
        }
        case AJM_IDENT_OUTPUT_CONTROL_BUF:
            if (!outputs && !input) {
                ajm_read_control(&chunks->has_output_control, &chunks->output_control, p,
                                 "Only one instance of output control buffer is allowed per job");
            }
            p += AJM_CHUNK_BUFFER_SIZE;
            break;
        default:
        unknown:
            c_log(C_LOG_LIB_AJM, C_LOG_LEVEL_CRITICAL, "AjmJobFromBatchBuffer", "Unknown chunk: %d",
                  (int)ident);
            C_ASSERT_MSG(0, "AjmJobFromBatchBuffer", "Unreachable code!");
            return;
        }
    }
}

/* A sideband buffer, read and written in order; its outputs zeroed when @zero */
typedef struct ajm_sideband {
    u8* p;
    u8* end;
    bool zero;
} ajm_sideband_t;

static bool sideband_empty(const ajm_sideband_t* s) {
    return s->p >= s->end;
}

/* The next @size bytes */
static void* sideband_take(ajm_sideband_t* s, size_t size) {
    void* at = s->p;
    s->p += size;
    return at;
}

/* The next @size bytes, zeroed: the job's output there */
static void* sideband_out(ajm_sideband_t* s, size_t size) {
    void* at = sideband_take(s, size);
    if (s->zero) {
        memset(at, 0, size);
    }
    return at;
}

static void ajm_job_sideband(ajm_job_t* job, const ajm_job_chunks_t* chunks, bool statistics,
                             bool zero) {
    if (statistics) {
        const u32 stats = AJM_STATISTICS_JOB_FLAGS(job->flags);
        if (chunks->has_input_control) {
            ajm_sideband_t in;
            in.p = chunks->input_control.p_address;
            in.end = in.p + chunks->input_control.size;
            in.zero = false;
            if (stats & AJM_STATISTICS_ENGINE) {
                memcpy(&job->statistics_engine_parameters,
                       sideband_take(&in, sizeof(AjmSidebandStatisticsEngineParameters)),
                       sizeof(AjmSidebandStatisticsEngineParameters));
                job->has_statistics_engine_parameters = true;
            }
        }
        if (chunks->has_output_control) {
            ajm_sideband_t out;
            out.p = chunks->output_control.p_address;
            out.end = out.p + chunks->output_control.size;
            out.zero = zero;
            job->p_result = (AjmSidebandResult*)sideband_out(&out, sizeof(AjmSidebandResult));
            if (stats & AJM_STATISTICS_ENGINE) {
                job->p_engine = (AjmSidebandStatisticsEngine*)sideband_out(
                    &out, sizeof(AjmSidebandStatisticsEngine));
            }
            if (stats & AJM_STATISTICS_ENGINE_PER_CODEC) {
                job->p_engine_per_codec = (AjmSidebandStatisticsEnginePerCodec*)sideband_out(
                    &out, sizeof(AjmSidebandStatisticsEnginePerCodec));
            }
            if (stats & AJM_STATISTICS_MEMORY) {
                job->p_memory = (AjmSidebandStatisticsMemory*)sideband_out(
                    &out, sizeof(AjmSidebandStatisticsMemory));
            }
        }
        return;
    }

    /* Initialize sideband input parameters */
    if (chunks->has_input_control) {
        const u32 sideband_flags = AJM_JOB_FLAGS_SIDEBAND(job->flags);
        const u32 control_flags = AJM_JOB_FLAGS_CONTROL(job->flags);
        ajm_sideband_t in;
        in.p = chunks->input_control.p_address;
        in.end = in.p + chunks->input_control.size;
        in.zero = false;
        if ((sideband_flags & AJM_JOB_SIDEBAND_FORMAT) && !sideband_empty(&in)) {
            memcpy(&job->format, sideband_take(&in, sizeof(AjmSidebandFormat)),
                   sizeof(AjmSidebandFormat));
            job->has_format = true;
        }
        if ((sideband_flags & AJM_JOB_SIDEBAND_GAPLESS_DECODE) && !sideband_empty(&in)) {
            memcpy(&job->gapless_decode, sideband_take(&in, sizeof(AjmSidebandGaplessDecode)),
                   sizeof(AjmSidebandGaplessDecode));
            job->has_gapless_decode = true;
        }
        if (control_flags & AJM_JOB_CONTROL_RESAMPLE) {
            memcpy(&job->resample_parameters,
                   sideband_take(&in, sizeof(AjmSidebandResampleParameters)),
                   sizeof(AjmSidebandResampleParameters));
            job->has_resample_parameters = true;
        }
        if (control_flags & AJM_JOB_CONTROL_INITIALIZE) {
            memcpy(&job->init_params, sideband_take(&in, sizeof(AjmSidebandInitParameters)),
                   sizeof(AjmSidebandInitParameters));
            job->has_init_params = true;
        }
    }

    /* Initialize sideband output parameters */
    if (chunks->has_output_control) {
        const u32 sideband_flags = AJM_JOB_FLAGS_SIDEBAND(job->flags);
        const u32 run_flags = AJM_JOB_FLAGS_RUN(job->flags);
        ajm_sideband_t out;
        out.p = chunks->output_control.p_address;
        out.end = out.p + chunks->output_control.size;
        out.zero = zero;
        job->p_result = (AjmSidebandResult*)sideband_out(&out, sizeof(AjmSidebandResult));
        if ((sideband_flags & AJM_JOB_SIDEBAND_STREAM) && !sideband_empty(&out)) {
            job->p_stream = (AjmSidebandStream*)sideband_out(&out, sizeof(AjmSidebandStream));
        }
        if ((sideband_flags & AJM_JOB_SIDEBAND_FORMAT) && !sideband_empty(&out)) {
            job->p_format = (AjmSidebandFormat*)sideband_out(&out, sizeof(AjmSidebandFormat));
        }
        if ((sideband_flags & AJM_JOB_SIDEBAND_GAPLESS_DECODE) && !sideband_empty(&out)) {
            job->p_gapless_decode =
                (AjmSidebandGaplessDecode*)sideband_out(&out, sizeof(AjmSidebandGaplessDecode));
        }
        if ((run_flags & AJM_JOB_RUN_MULTIPLE_FRAMES) && !sideband_empty(&out)) {
            job->p_mframe = (AjmSidebandMFrame*)sideband_out(&out, sizeof(AjmSidebandMFrame));
        }
        if ((run_flags & AJM_JOB_RUN_GET_CODEC_INFO) && !sideband_empty(&out)) {
            job->p_codec_info = out.p;
        }
    }
}

/* Reads the job in [@p, @end): zeroes the sideband outputs it has, or runs it */
static void ajm_batch_run_job(ajm_context_t* c, u32 batch_id, u32 instance_id, const u8* p,
                              const u8* end, bool run) {
    const bool statistics = instance_id == AJM_INSTANCE_STATISTICS;
    ajm_span_t local_outputs[AJM_JOB_OUTPUT_BUFFERS];
    ajm_span_t* outputs = local_outputs;
    u8* input_copy = NULL;
    ajm_job_chunks_t chunks;
    ajm_job_t job;

    memset(&chunks, 0, sizeof(chunks));
    memset(&job, 0, sizeof(job));
    ajm_scan_job(p, end, statistics, &chunks, NULL, NULL);
    C_ASSERT_MSG(chunks.has_flags, "AjmJobFromBatchBuffer", "job_flags.has_value()");
    job.instance_id = instance_id;
    job.flags = chunks.flags;
    if (!run) {
        ajm_job_sideband(&job, &chunks, statistics, true);
        return;
    }

    /* The input in place when it is one buffer; else the buffers one after another */
    if (chunks.num_inputs == 1) {
        job.input = chunks.input_one;
    } else if (chunks.num_inputs > 1) {
        input_copy = (u8*)malloc(chunks.input_size);
        C_ASSERT_MSG(input_copy != NULL, "AjmJobFromBatchBuffer", "Out of memory");
        ajm_scan_job(p, end, statistics, &chunks, NULL, input_copy);
        job.input = input_copy;
    }
    job.input_size = chunks.input_size;

    if (chunks.num_outputs > AJM_JOB_OUTPUT_BUFFERS) {
        outputs = (ajm_span_t*)malloc(chunks.num_outputs * sizeof(ajm_span_t));
        C_ASSERT_MSG(outputs != NULL, "AjmJobFromBatchBuffer", "Out of memory");
    }
    if (chunks.num_outputs != 0) {
        ajm_scan_job(p, end, statistics, &chunks, outputs, NULL);
    }
    job.output.chunks = outputs;
    job.output.count = chunks.num_outputs;
    job.output.current = 0;

    ajm_job_sideband(&job, &chunks, statistics, false);
    ajm_context_run_job(c, batch_id, &job);

    free(input_copy);
    if (outputs != local_outputs) {
        free(outputs);
    }
}

/* Every job read, its sideband outputs zeroed, before any is run (@run false); then each run
 * (@run true) */
static void ajm_batch_pass(ajm_context_t* c, u32 batch_id, u8* data, size_t size, bool run) {
    const u8* p = data;
    const u8* end = data + size;
    while (p < end) {
        const u32 header = rd32(p);
        const u32 job_size = rd32(p + 4);
        const u8* job;
        p += AJM_CHUNK_JOB_SIZE;
        if (AJM_CHUNK_IDENT(header) == AJM_IDENT_INLINE_BUF) {
            /* Inline buffers are used to store sideband input data.
             * We should just skip them as they do not require any special handling. */
            p += job_size;
            continue;
        }
        C_ASSERT_MSG(AJM_CHUNK_IDENT(header) == AJM_IDENT_JOB, "FromBatchBuffer",
                     "job_chunk.header.ident == AjmIdentJob");
        job = p;
        p += job_size;
        ajm_batch_run_job(c, batch_id, AJM_CHUNK_PAYLOAD(header), job, p, run);
    }
}

void ajm_batch_run(ajm_context_t* c, u32 batch_id, u8* data, size_t size) {
    ajm_batch_pass(c, batch_id, data, size, false);
    ajm_batch_pass(c, batch_id, data, size, true);
}

/*
 * Writing the chunks: the game's buffer is written with no bound, as it gives it
 **/
static u8* wr_chunk_buffer(u8* p, u32 ident, size_t size, const void* address) {
    wr_header(p, ident, 0);
    wr32(p + 4, (u32)size);
    wrptr(p + 8, address);
    return p + AJM_CHUNK_BUFFER_SIZE;
}

static u8* wr_chunk_flags(u8* p, u32 ident, u64 flags) {
    wr_header(p, ident, (u32)(flags >> 32));
    wr32(p + 4, (u32)flags);
    return p + AJM_CHUNK_FLAGS_SIZE;
}

/* The job chunk at @p, its header written: its body starts after it */
static u8* wr_job_start(u8* p, u32 instance_id) {
    wr_header(p, AJM_IDENT_JOB, instance_id);
    return p + AJM_CHUNK_JOB_SIZE;
}

/* The job chunk at @p sized to its body, which ends at @end: @end */
static void* wr_job_end(u8* p, u8* end) {
    wr32(p + 4, (u32)(end - (p + AJM_CHUNK_JOB_SIZE)));
    return end;
}

void* ajm_batch_job_control_buffer_ra(void* p_buffer, u32 instance_id, u64 flags,
                                      void* p_sideband_input, size_t sideband_input_size,
                                      void* p_sideband_output, size_t sideband_output_size,
                                      void* p_return_address) {
    u8* job = (u8*)p_buffer;
    u8* p;
    bool is_statistics;
#ifndef NDEBUG
    c_log(C_LOG_LIB_AJM, C_LOG_LEVEL_TRACE, "BatchJobControlBufferRa", "called");
#endif
    p = wr_job_start(job, instance_id);
    if (p_return_address != NULL) {
        p = wr_chunk_buffer(p, AJM_IDENT_RETURN_ADDRESS_BUF, 0, p_return_address);
    }
    p = wr_chunk_buffer(p, AJM_IDENT_INPUT_CONTROL_BUF, sideband_input_size, p_sideband_input);
    /* 0x0000'0000'C001'8007 (AJM_INSTANCE_STATISTICS):
     * | sideband | reserved      | statistics        | command | codec    | revision |
     * | 000      | 0000000000000 | 11000000000000011 | 0000    | 00000000 | 111      |
     * statistics flags:
     * STATISTICS_ENGINE | STATISTICS_ENGINE_PER_CODEC | ??STATISTICS_UNK?? | STATISTICS_MEMORY
     * 0x0000'6000'0000'E7FF:
     * | sideband | reserved                      | control | run | codec    | revision |
     * | 011      | 00000000000000000000000000000 | 111     | 00  | 11111111 | 111      | */
    is_statistics = instance_id == AJM_INSTANCE_STATISTICS;
    flags &= is_statistics ? (((u64)0x00000000u << 32) | 0xC0018007u)
                           : (((u64)0x00006000u << 32) | 0x0000E7FFu);
    p = wr_chunk_flags(p, AJM_IDENT_CONTROL_FLAGS, flags);
    p = wr_chunk_buffer(p, AJM_IDENT_OUTPUT_CONTROL_BUF, sideband_output_size, p_sideband_output);
    return wr_job_end(job, p);
}

void* ajm_batch_job_inline_buffer(void* p_buffer, const void* p_data_input, size_t data_input_size,
                                  const void** pp_batch_address) {
    u8* job = (u8*)p_buffer;
    u8* data = job + AJM_CHUNK_JOB_SIZE;
    const u32 size = (u32)((data_input_size + 7) & ~(size_t)7);
#ifndef NDEBUG
    c_log(C_LOG_LIB_AJM, C_LOG_LEVEL_TRACE, "BatchJobInlineBuffer", "called");
#endif
    wr_header(job, AJM_IDENT_INLINE_BUF, 0);
    wr32(job + 4, size);
    *pp_batch_address = data;
    memcpy(data, p_data_input, data_input_size);
    return data + size;
}

void* ajm_batch_job_run_buffer_ra(void* p_buffer, u32 instance_id, u64 flags, void* p_data_input,
                                  size_t data_input_size, void* p_data_output,
                                  size_t data_output_size, void* p_sideband_output,
                                  size_t sideband_output_size, void* p_return_address) {
    u8* job = (u8*)p_buffer;
    u8* p;
#ifndef NDEBUG
    c_log(C_LOG_LIB_AJM, C_LOG_LEVEL_TRACE, "BatchJobRunBufferRa", "called");
#endif
    p = wr_job_start(job, instance_id);
    if (p_return_address != NULL) {
        p = wr_chunk_buffer(p, AJM_IDENT_RETURN_ADDRESS_BUF, 0, p_return_address);
    }
    p = wr_chunk_buffer(p, AJM_IDENT_INPUT_RUN_BUF, data_input_size, p_data_input);
    /* 0x0000'E000'0000'1FFF:
     * | sideband | reserved                      | control | run | codec    | revision |
     * | 111      | 00000000000000000000000000000 | 000     | 11  | 11111111 | 111      | */
    flags &= ((u64)0x0000E000u << 32) | 0x00001FFFu;
    p = wr_chunk_flags(p, AJM_IDENT_RUN_FLAGS, flags);
    p = wr_chunk_buffer(p, AJM_IDENT_OUTPUT_RUN_BUF, data_output_size, p_data_output);
    p = wr_chunk_buffer(p, AJM_IDENT_OUTPUT_CONTROL_BUF, sideband_output_size, p_sideband_output);
    return wr_job_end(job, p);
}

void* ajm_batch_job_run_split_buffer_ra(void* p_buffer, u32 instance_id, u64 flags,
                                        const AjmBuffer* p_data_input_buffers,
                                        size_t num_data_input_buffers,
                                        const AjmBuffer* p_data_output_buffers,
                                        size_t num_data_output_buffers, void* p_sideband_output,
                                        size_t sideband_output_size, void* p_return_address) {
    u8* job = (u8*)p_buffer;
    u8* p;
    size_t i;
#ifndef NDEBUG
    c_log(C_LOG_LIB_AJM, C_LOG_LEVEL_TRACE, "BatchJobRunSplitBufferRa", "called");
#endif
    p = wr_job_start(job, instance_id);
    if (p_return_address != NULL) {
        p = wr_chunk_buffer(p, AJM_IDENT_RETURN_ADDRESS_BUF, 0, p_return_address);
    }
    for (i = 0; i < num_data_input_buffers; i++) {
        p = wr_chunk_buffer(p, AJM_IDENT_INPUT_RUN_BUF, (size_t)p_data_input_buffers[i].size,
                            p_data_input_buffers[i].p_address);
    }
    /* 0x0000'E000'0000'1FFF:
     * | sideband | reserved                      | control | run | codec    | revision |
     * | 111      | 00000000000000000000000000000 | 000     | 11  | 11111111 | 111      | */
    flags &= ((u64)0x0000E000u << 32) | 0x00001FFFu;
    p = wr_chunk_flags(p, AJM_IDENT_RUN_FLAGS, flags);
    for (i = 0; i < num_data_output_buffers; i++) {
        p = wr_chunk_buffer(p, AJM_IDENT_OUTPUT_RUN_BUF, (size_t)p_data_output_buffers[i].size,
                            p_data_output_buffers[i].p_address);
    }
    p = wr_chunk_buffer(p, AJM_IDENT_OUTPUT_CONTROL_BUF, sideband_output_size, p_sideband_output);
    return wr_job_end(job, p);
}
