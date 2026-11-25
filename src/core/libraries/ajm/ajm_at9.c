/* SPDX-FileCopyrightText: Copyright 2024-2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <string.h>

#include <formats/ratrac9.h>

#include "common/c_bridge.h"
#include "core/libraries/ajm/ajm_at9.h"
#include "core/libraries/ajm/ajm_result.h"

/* The RIFF tags, as the u32 their bytes read as */
#define AT9_TAG_RIFF 0x46464952u /* "RIFF" */
#define AT9_TAG_WAVE 0x45564157u /* "WAVE" */
#define AT9_TAG_DATA 0x61746164u /* "data" */
#define AT9_TAG_FMT 0x20746D66u  /* "fmt " */
#define AT9_TAG_FACT 0x74636166u /* "fact" */

#define AT9_RIFF_HEADER_SIZE 12 /* riff, size, wave */
#define AT9_CHUNK_HEADER_SIZE 8 /* tag, length */
#define AT9_SAMPLE_DATA_SIZE 12 /* sample_length, encoder_delay, encoder_delay2 */

static u32 read_u32(const u8* p) {
    u32 v;
    memcpy(&v, p, sizeof(v));
    return v;
}

void ajm_at9_init(ajm_at9_t* d, u32 format, u32 flags) {
    memset(d, 0, sizeof(*d));
    d->format = format;
    d->flags = flags;
}

void ajm_at9_free(ajm_at9_t* d) {
    ratrac9_close((ratrac9_t*)d->decoder);
    d->decoder = NULL;
}

void ajm_at9_reset(ajm_at9_t* d) {
    /* Back to the start of the stream; the decoder's memory is kept */
    if (d->decoder) {
        ratrac9_reset((ratrac9_t*)d->decoder);
    }
    d->num_frames = 0;
    d->superframe_bytes_remain = d->superframe_bytes;
}

void ajm_at9_initialize(ajm_at9_t* d, const void* buffer, u32 buffer_size) {
    const AjmDecAt9InitializeParameters* params = (const AjmDecAt9InitializeParameters*)buffer;
    size_t pcm_bytes;
    C_ASSERT_MSG(buffer_size == sizeof(AjmDecAt9InitializeParameters), "Initialize",
                 "Incorrect At9 initialization buffer size");
    memcpy(d->config_data, params->config_data, ORBIS_AT9_CONFIG_DATA_SIZE);
    ratrac9_close((ratrac9_t*)d->decoder);
    d->decoder = ratrac9_open(d->config_data);
    d->is_initialized = d->decoder != NULL;
    if (!d->decoder) {
        c_log(C_LOG_LIB_AJM, C_LOG_LEVEL_ERROR, "Initialize",
              "Invalid ATRAC9 configuration %02x%02x%02x%02x", d->config_data[0], d->config_data[1],
              d->config_data[2], d->config_data[3]);
        return;
    }
    d->channels = ratrac9_channels((ratrac9_t*)d->decoder);
    d->sample_rate = ratrac9_sample_rate((ratrac9_t*)d->decoder);
    d->frame_samples = ratrac9_frame_samples((ratrac9_t*)d->decoder);
    d->superframe_bytes = ratrac9_superframe_bytes((ratrac9_t*)d->decoder);
    d->frames_in_superframe = ratrac9_frames_per_superframe((ratrac9_t*)d->decoder);
    ajm_at9_reset(d);
    pcm_bytes = (size_t)d->frame_samples * d->channels * ajm_pcm_size(d->format);
    C_ASSERT_MSG(pcm_bytes <= sizeof(d->pcm.bytes), "Initialize", "ATRAC9 frame too large");
    /* What the frame buffer grew by is zeroed, as it was */
    if (pcm_bytes > d->pcm_bytes) {
        memset(d->pcm.bytes + d->pcm_bytes, 0, pcm_bytes - d->pcm_bytes);
    }
    d->pcm_bytes = pcm_bytes;
}

void ajm_at9_get_info(const ajm_at9_t* d, void* out_info) {
    AjmSidebandDecAt9CodecInfo* info = (AjmSidebandDecAt9CodecInfo*)out_info;
    info->super_frame_size = d->superframe_bytes;
    info->frames_in_super_frame = d->frames_in_superframe;
    info->next_frame_size = d->superframe_bytes_remain;
    info->frame_samples = d->frame_samples;
}

static bool ajm_at9_parse_riff_header(ajm_at9_t* d, ajm_input_t* in_buf, ajm_gapless_t* gapless) {
    bool result = false;
    if (in_buf->size < AT9_RIFF_HEADER_SIZE + AT9_CHUNK_HEADER_SIZE) {
        return false;
    }
    if (read_u32(in_buf->data) != AT9_TAG_RIFF || read_u32(in_buf->data + 8) != AT9_TAG_WAVE) {
        return false;
    }
    ajm_input_advance(in_buf, AT9_RIFF_HEADER_SIZE);
    while (in_buf->size >= AT9_CHUNK_HEADER_SIZE) {
        const u32 tag = read_u32(in_buf->data);
        const u32 length = read_u32(in_buf->data + 4);
        size_t advance_len;
        ajm_input_advance(in_buf, AT9_CHUNK_HEADER_SIZE);
        if (tag == AT9_TAG_DATA) {
            break;
        }
        if (in_buf->size < length) {
            return false;
        }
        switch (tag) {
        case AT9_TAG_FMT: {
            /* WAVE_FORMAT_EXTENSIBLE with the ATRAC9 sub-format, holding the config bytes */
            AjmDecAt9InitializeParameters init_params;
            memset(&init_params, 0, sizeof(init_params));
            if (!ratrac9_parse_wave_fmt(in_buf->data, length, init_params.config_data)) {
                return false;
            }
            ajm_at9_initialize(d, &init_params, (u32)sizeof(init_params));
            result = true;
            break;
        }
        case AT9_TAG_FACT:
            if (length != AT9_SAMPLE_DATA_SIZE) {
                return false;
            }
            gapless->init.total_samples = read_u32(in_buf->data);
            gapless->init.skip_samples = (u16)read_u32(in_buf->data + 4);
            ajm_gapless_reset(gapless);
            break;
        default:
            break;
        }
        advance_len = (size_t)((length + 1u) & ~1u);
        if (in_buf->size < advance_len) {
            advance_len = in_buf->size;
        }
        ajm_input_advance(in_buf, advance_len);
    }
    return result;
}

u32 ajm_at9_get_minimum_input_size(const ajm_at9_t* d) {
    return d->superframe_bytes_remain;
}

/* A frame cut short counts the bytes it would have had: never step past the input */
static void ajm_at9_consume(ajm_at9_t* d, ajm_input_t* in_buf, size_t used) {
    if (used > in_buf->size) {
        used = in_buf->size;
    }
    ajm_input_advance(in_buf, used);
    d->superframe_bytes_remain -=
        d->superframe_bytes_remain < (u32)used ? d->superframe_bytes_remain : (u32)used;
}

static int ajm_at9_decode(ajm_at9_t* d, const ajm_input_t* in_buf, u32 format, bool planar,
                          size_t* bytes_used) {
    switch (format) {
    case AJM_FORMAT_ENCODING_S16:
        return ratrac9_decode_s16((ratrac9_t*)d->decoder, in_buf->data, in_buf->size,
                                  (int16_t*)d->pcm.bytes, planar, bytes_used);
    case AJM_FORMAT_ENCODING_S32:
        return ratrac9_decode_s32((ratrac9_t*)d->decoder, in_buf->data, in_buf->size,
                                  (int32_t*)d->pcm.bytes, planar, bytes_used);
    case AJM_FORMAT_ENCODING_FLOAT:
        return ratrac9_decode_f32((ratrac9_t*)d->decoder, in_buf->data, in_buf->size,
                                  (float*)d->pcm.bytes, planar, bytes_used);
    default:
        C_ASSERT_MSG(0, "ProcessData", "Unreachable code!");
        return -1;
    }
}

ajm_decoder_result_t ajm_at9_process_data(ajm_at9_t* d, ajm_input_t* in_buf, ajm_output_t* output,
                                          ajm_gapless_t* gapless) {
    ajm_decoder_result_t result;
    bool planar;
    int ret;
    size_t bytes_used = 0;
    u32 skip_samples = 0;
    u32 max_pcm;
    size_t pcm_written;
    size_t sample_bytes;
    size_t pcm_count;
    const bool is_riff = (d->flags & AJM_AT9_PARSE_RIFF_HEADER) &&
                         in_buf->size >= AT9_RIFF_HEADER_SIZE &&
                         read_u32(in_buf->data) == AT9_TAG_RIFF;
    memset(&result, 0, sizeof(result));

    if (is_riff) {
        if (!ajm_at9_parse_riff_header(d, in_buf, gapless)) {
            c_log(C_LOG_LIB_AJM, C_LOG_LEVEL_ERROR, "ProcessData",
                  "Failed to parse ATRAC9 RIFF header");
            result.result = ORBIS_AJM_RESULT_CODEC_ERROR | ORBIS_AJM_RESULT_FATAL;
            return result;
        }
        result.is_reset = true;
    }

    if (!d->is_initialized) {
        result.result = ORBIS_AJM_RESULT_NOT_INITIALIZED;
        return result;
    }

    if (in_buf->size == 0) {
        return result;
    }

    planar = (d->flags & AJM_AT9_NON_INTERLEAVED_OUTPUT) != 0;
    ret = ajm_at9_decode(d, in_buf, d->format, planar, &bytes_used);
    if (ret != 0) {
        c_log(C_LOG_LIB_AJM, C_LOG_LEVEL_ERROR, "ProcessData", "ATRAC9 decode failed ret = %d",
              ret);
        result.result = ORBIS_AJM_RESULT_CODEC_ERROR | ORBIS_AJM_RESULT_FATAL;
        result.internal_result = ret;
        return result;
    }

    result.frames_decoded += 1;
    ajm_at9_consume(d, in_buf, bytes_used);

    if (gapless->current.skip_samples > 0) {
        skip_samples = (u16)d->frame_samples < gapless->current.skip_samples
                           ? (u16)d->frame_samples
                           : gapless->current.skip_samples;
        gapless->current.skip_samples = (u16)(gapless->current.skip_samples - skip_samples);
    }

    max_pcm = gapless->init.total_samples != 0 ? gapless->current.total_samples * d->channels
                                               : 0xFFFFFFFFu;

    /* The frame's samples after those skipped, at most max_pcm of them */
    sample_bytes = ajm_pcm_size(d->format);
    pcm_count = d->pcm_bytes / sample_bytes;
    C_ASSERT_MSG((size_t)skip_samples * d->channels <= pcm_count, "WriteOutputSamples",
                 "Skipped past the frame");
    pcm_count -= (size_t)skip_samples * d->channels;
    if ((u32)pcm_count > max_pcm) {
        pcm_count = max_pcm;
    } else {
        pcm_count = (u32)pcm_count;
    }
    pcm_written =
        ajm_output_write(output, d->pcm.bytes + (size_t)skip_samples * d->channels * sample_bytes,
                         pcm_count, sample_bytes);

    result.samples_written = (u32)(pcm_written / d->channels);
    gapless->current.skipped_samples =
        (u16)(gapless->current.skipped_samples + d->frame_samples - result.samples_written);
    if (gapless->init.total_samples != 0) {
        gapless->current.total_samples -= result.samples_written;
    }

    d->num_frames += 1;
    if ((d->num_frames % d->frames_in_superframe) == 0) {
        if (d->superframe_bytes_remain) {
            ajm_at9_consume(d, in_buf, d->superframe_bytes_remain);
        }
        d->superframe_bytes_remain = d->superframe_bytes;
        d->num_frames = 0;
    } else if (ajm_gapless_is_end(gapless)) {
        /* Drain the remaining superframe, into the frame buffer whose samples are not used */
        while ((d->num_frames % d->frames_in_superframe) != 0) {
            ret = ajm_at9_decode(d, in_buf, AJM_FORMAT_ENCODING_S16, planar, &bytes_used);
            if (ret == 0) {
                ajm_at9_consume(d, in_buf, bytes_used);
            }
            result.frames_decoded += 1;
            d->num_frames += 1;
        }
        ajm_at9_consume(d, in_buf, d->superframe_bytes_remain);
        d->superframe_bytes_remain = d->superframe_bytes;
        d->num_frames = 0;
    }

    return result;
}

AjmSidebandFormat ajm_at9_get_format(const ajm_at9_t* d) {
    AjmSidebandFormat f;
    f.num_channels = d->channels;
    f.channel_mask = ajm_channel_mask(d->channels);
    f.sampl_freq = d->sample_rate;
    f.sample_encoding = d->format;
    f.bitrate = d->frames_in_superframe && d->frame_samples
                    ? (u32)(((u64)d->sample_rate * d->superframe_bytes * 8) /
                            (d->frames_in_superframe * d->frame_samples))
                    : 0;
    f.reserved = 0;
    return f;
}

u32 ajm_at9_get_next_frame_size(const ajm_at9_t* d, const ajm_gapless_t* gapless) {
    const u32 skip_samples = gapless->current.skip_samples < d->frame_samples
                                 ? gapless->current.skip_samples
                                 : d->frame_samples;
    const u32 left = d->frame_samples - skip_samples;
    const u32 samples =
        gapless->init.total_samples != 0
            ? (gapless->current.total_samples < left ? gapless->current.total_samples : left)
            : left;
    return samples * d->channels * ajm_pcm_size(d->format);
}
