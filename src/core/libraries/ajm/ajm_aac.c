/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <string.h>

#include <formats/raac.h>

#include "common/c_bridge.h"
#include "core/libraries/ajm/ajm_aac.h"
#include "core/libraries/ajm/ajm_result.h"

typedef struct bit_writer {
    u8* out;
    size_t size;
    size_t pos;
} bit_writer_t;

static void bit_writer_init(bit_writer_t* w, u8* out, size_t size) {
    w->out = out;
    w->size = size;
    w->pos = 0;
    memset(out, 0, size);
}

static void bit_writer_put(bit_writer_t* w, u32 value, u32 bits) {
    while (bits--) {
        if ((value >> bits) & 1) {
            w->out[w->pos >> 3] |= (u8)(0x80 >> (w->pos & 7));
        }
        ++w->pos;
    }
}

static void bit_writer_align(bit_writer_t* w) {
    w->pos = (w->pos + 7) & ~(size_t)7;
}

static size_t bit_writer_bytes(const bit_writer_t* w) {
    return (w->pos + 7) >> 3;
}

/* The AudioSpecificConfig of a raw stream: AAC-LC at the given rate, with a program config
 * element for the layouts that have no channel configuration of their own: 6.1 (C, L/R,
 * Ls/Rs, Cs, LFE) and 7.1 (C, L/R, Ls/Rs, Lrs/Rrs, LFE). */
static size_t make_raw_config(u8* out, size_t size, u32 sampling_freq_type, u32 channels) {
    bit_writer_t w;
    const u32 config = channels == 0 ? 2 : channels <= 6 ? channels : 0;
    bit_writer_init(&w, out, size);
    bit_writer_put(&w, 2, 5); /* AAC LC */
    bit_writer_put(&w, sampling_freq_type, 4);
    bit_writer_put(&w, config, 4);
    bit_writer_put(&w, 0, 3); /* 1024 frames, no core coder, no extension */
    if (config == 0) {
        const bool back_pair = channels == 8;
        bit_writer_put(&w, 0, 4); /* element_instance_tag */
        bit_writer_put(&w, 1, 2); /* object_type: LC */
        bit_writer_put(&w, sampling_freq_type, 4);
        bit_writer_put(&w, 2, 4); /* front: C, L/R */
        bit_writer_put(&w, 1, 4); /* side: Ls/Rs */
        bit_writer_put(&w, 1, 4); /* back: Cs, or Lrs/Rrs */
        bit_writer_put(&w, 1, 2); /* LFE */
        bit_writer_put(&w, 0, 3); /* assoc data */
        bit_writer_put(&w, 0, 4); /* coupling channels */
        bit_writer_put(&w, 0, 3); /* no mixdowns */
        bit_writer_put(&w, 0, 1);
        bit_writer_put(&w, 0, 4); /* front SCE 0 */
        bit_writer_put(&w, 1, 1);
        bit_writer_put(&w, 0, 4); /* front CPE 0 */
        bit_writer_put(&w, 1, 1);
        bit_writer_put(&w, 1, 4); /* side CPE 1 */
        bit_writer_put(&w, back_pair ? 1 : 0, 1);
        bit_writer_put(&w, back_pair ? 2 : 1, 4); /* back CPE 2, or SCE 1 */
        bit_writer_put(&w, 0, 4);                 /* LFE 0 */
        bit_writer_align(&w);
        bit_writer_put(&w, 0, 8); /* comment_field_bytes */
    }
    return bit_writer_bytes(&w);
}

void ajm_aac_init(ajm_aac_t* d, u32 format, u32 flags, u32 channels) {
    memset(d, 0, sizeof(*d));
    d->format = format;
    d->flags = flags;
    d->channels = channels;
    d->skip_frames = (flags & AJM_AAC_ENABLE_NONDELAY_OUTPUT) ? 0 : 2;
}

static void ajm_aac_close(ajm_aac_t* d) {
    if (d->decoder) {
        raac_close((raac_t*)d->decoder);
        d->decoder = NULL;
    }
    d->asc_size = 0;
}

void ajm_aac_free(ajm_aac_t* d) {
    ajm_aac_close(d);
}

/* Opens the decoder for an AudioSpecificConfig, keeping the open one when it is the same. */
static bool ajm_aac_open(ajm_aac_t* d, const u8* asc, size_t asc_size) {
    if (d->decoder && asc_size == d->asc_size && memcmp(asc, d->asc, asc_size) == 0) {
        return true;
    }
    ajm_aac_close(d);
    if (asc_size > sizeof(d->asc)) {
        return false;
    }
    d->decoder = raac_open(asc, asc_size);
    if (!d->decoder) {
        c_log(C_LOG_LIB_AJM, C_LOG_LEVEL_ERROR, "Open", "Unsupported AAC configuration");
        return false;
    }
    /* Games take the channels in the WAVE order */
    raac_set_channel_order((raac_t*)d->decoder, RAAC_ORDER_WAVE);
    memcpy(d->asc, asc, asc_size);
    d->asc_size = asc_size;
    return true;
}

void ajm_aac_reset(ajm_aac_t* d) {
    if (d->config_type == AJM_AAC_CONFIG_RAW) {
        u8 asc[32];
        ajm_aac_open(d, asc, make_raw_config(asc, sizeof(asc), d->sampling_freq_type, d->channels));
    }
    /* An ADTS stream opens with its first frame's header */
    if (d->decoder) {
        raac_reset((raac_t*)d->decoder);
    }
    d->last_frame_bytes = 0;
    d->skip_frames = (d->flags & AJM_AAC_ENABLE_NONDELAY_OUTPUT) ? 0 : 2;
}

void ajm_aac_initialize(ajm_aac_t* d, const void* buffer, u32 buffer_size) {
    const u8* p = (const u8*)buffer;
    C_ASSERT_MSG(buffer_size == 8, "Initialize", "Incorrect AAC initialization buffer size");
    memcpy(&d->config_type, p, sizeof(u32));
    memcpy(&d->sampling_freq_type, p + 4, sizeof(u32));
    ajm_aac_reset(d);
}

void ajm_aac_get_info(const ajm_aac_t* d, void* out_info) {
    AjmSidebandDecM4aacCodecInfo* codec_info = (AjmSidebandDecM4aacCodecInfo*)out_info;
    /* SBR doubles the frame length */
    codec_info->heaac = d->decoder && raac_frame_len((const raac_t*)d->decoder) == 2048;
    codec_info->reserved = 0;
}

AjmSidebandFormat ajm_aac_get_format(const ajm_aac_t* d) {
    AjmSidebandFormat f;
    u32 channels;
    u32 rate;
    memset(&f, 0, sizeof(f));
    f.sample_encoding = d->format;
    if (!d->decoder) {
        return f;
    }
    channels = raac_channels((const raac_t*)d->decoder);
    rate = raac_sample_rate((const raac_t*)d->decoder);
    f.num_channels = channels;
    f.channel_mask = ajm_channel_mask(channels);
    f.sampl_freq = rate;
    f.bitrate =
        (u32)((u64)d->last_frame_bytes * 8 * rate / raac_frame_len((const raac_t*)d->decoder));
    return f;
}

u32 ajm_aac_get_minimum_input_size(const ajm_aac_t* d) {
    (void)d;
    return 0;
}

u32 ajm_aac_get_next_frame_size(const ajm_aac_t* d, const ajm_gapless_t* gapless) {
    u32 frame;
    u32 skip_samples;
    u32 left;
    u32 samples;
    if (!d->decoder) {
        return 0;
    }
    frame = raac_frame_len((const raac_t*)d->decoder);
    skip_samples = gapless->current.skip_samples < frame ? gapless->current.skip_samples : frame;
    left = frame - skip_samples;
    samples = gapless->init.total_samples != 0
                  ? (gapless->current.total_samples < left ? gapless->current.total_samples : left)
                  : left;
    return samples * raac_channels((const raac_t*)d->decoder) * ajm_pcm_size(d->format);
}

typedef struct aac_fill {
    raac_t* decoder;
    u32 skip_samples;
} aac_fill_t;

static void aac_fill_s16(void* ctx, size_t first, size_t n, void* dst) {
    const aac_fill_t* f = (const aac_fill_t*)ctx;
    raac_copy_s16(f->decoder, f->skip_samples + first, n, (int16_t*)dst);
}

static void aac_fill_f32(void* ctx, size_t first, size_t n, void* dst) {
    const aac_fill_t* f = (const aac_fill_t*)ctx;
    raac_copy_f32(f->decoder, f->skip_samples + first, n, (float*)dst);
}

ajm_decoder_result_t ajm_aac_process_data(ajm_aac_t* d, ajm_input_t* input, ajm_output_t* output,
                                          ajm_gapless_t* gapless) {
    ajm_decoder_result_t result;
    const u8* unit = input->data;
    size_t unit_size = input->size;
    size_t frame_bytes = 0;
    int frame;
    u32 skip_samples = 0;
    u32 max_samples;
    u32 count;
    u32 channels;
    size_t written = 0;
    aac_fill_t fill;
    memset(&result, 0, sizeof(result));

    if (d->config_type == AJM_AAC_CONFIG_ADTS) {
        raac_adts_t header;
        if (input->size < 7) {
            result.result = ORBIS_AJM_RESULT_PARTIAL_INPUT;
            return result;
        }
        if (raac_adts_parse(input->data, input->size, &header) < 0) {
            c_log(C_LOG_LIB_AJM, C_LOG_LEVEL_ERROR, "ProcessData", "No ADTS header");
            result.result = ORBIS_AJM_RESULT_CODEC_ERROR | ORBIS_AJM_RESULT_FATAL;
            return result;
        }
        if (input->size < header.frame_size) {
            result.result = ORBIS_AJM_RESULT_PARTIAL_INPUT;
            return result;
        }
        if (!ajm_aac_open(d, header.asc, sizeof(header.asc))) {
            result.result = ORBIS_AJM_RESULT_CODEC_ERROR | ORBIS_AJM_RESULT_FATAL;
            return result;
        }
        unit += header.header_size;
        unit_size = header.frame_size - header.header_size;
        frame_bytes = header.frame_size;
    } else if (!d->decoder) {
        result.result = ORBIS_AJM_RESULT_CODEC_ERROR | ORBIS_AJM_RESULT_FATAL;
        return result;
    }

    frame = raac_decode((raac_t*)d->decoder, unit, unit_size);
    if (frame < 0) {
        c_log(C_LOG_LIB_AJM, C_LOG_LEVEL_ERROR, "ProcessData", "AAC frame decode failed");
        result.result = ORBIS_AJM_RESULT_CODEC_ERROR | ORBIS_AJM_RESULT_FATAL;
        return result;
    }
    if (!frame_bytes) {
        frame_bytes = raac_last_size((const raac_t*)d->decoder);
    }
    d->last_frame_bytes = (u32)frame_bytes;
    result.frames_decoded += 1;
    ajm_input_advance(input, frame_bytes < input->size ? frame_bytes : input->size);

    if (d->skip_frames > 0) {
        --d->skip_frames;
        return result;
    }

    if (gapless->current.skip_samples > 0) {
        skip_samples =
            (u32)frame < gapless->current.skip_samples ? (u32)frame : gapless->current.skip_samples;
        gapless->current.skip_samples = (u16)(gapless->current.skip_samples - skip_samples);
    }

    max_samples = gapless->init.total_samples != 0 ? gapless->current.total_samples : (u32)frame;
    count = (u32)frame - skip_samples < max_samples ? (u32)frame - skip_samples : max_samples;
    channels = raac_channels((const raac_t*)d->decoder);

    /* Straight from the decoder into the game's buffer */
    fill.decoder = (raac_t*)d->decoder;
    fill.skip_samples = skip_samples;
    switch (d->format) {
    case AJM_FORMAT_ENCODING_S16:
        written =
            ajm_output_write_frames(output, count, channels, sizeof(s16), aac_fill_s16, &fill);
        break;
    case AJM_FORMAT_ENCODING_S32:
        C_ASSERT_MSG(0, "ProcessData", "NOT IMPLEMENTED");
        break;
    case AJM_FORMAT_ENCODING_FLOAT:
        written =
            ajm_output_write_frames(output, count, channels, sizeof(float), aac_fill_f32, &fill);
        break;
    default:
        C_ASSERT_MSG(0, "ProcessData", "Unreachable code!");
        break;
    }

    result.samples_written = (u32)(written / channels);
    gapless->current.skipped_samples =
        (u16)(gapless->current.skipped_samples + (u32)frame - result.samples_written);
    if (gapless->init.total_samples != 0) {
        gapless->current.total_samples -= result.samples_written;
    }
    return result;
}
