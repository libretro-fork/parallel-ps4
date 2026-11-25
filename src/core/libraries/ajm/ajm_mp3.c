/* SPDX-FileCopyrightText: Copyright 2024-2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <limits.h>
#include <math.h>
#include <string.h>

#include <formats/rmp3.h>

#include "common/c_bridge.h"
#include "core/libraries/ajm/ajm_error.h"
#include "core/libraries/ajm/ajm_mp3.h"
#include "core/libraries/ajm/ajm_result.h"
#include "core/libraries/error_codes.h"

/* Following tables have been reversed from AJM library */
static const s32 mp3_sample_rate_table[4][4] = {
    {11025, 12000, 8000, 0},
    {0, 0, 0, 0},
    {22050, 24000, 16000, 0},
    {44100, 48000, 32000, 0},
};

static const s32 mp3_bit_rate_table[4][16] = {
    {0, 8, 16, 24, 32, 40, 48, 56, 64, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 8, 16, 24, 32, 40, 48, 56, 64, 80, 96, 112, 128, 144, 160, 0},
    {0, 32, 40, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320, 0},
};

/* Mp3AudioVersion */
#define MP3_VERSION_V2_5 0u
#define MP3_VERSION_V2 2u
#define MP3_VERSION_V1 3u

/* Mp3ChannelMode */
#define MP3_SINGLE_CHANNEL 3u

/* An MPEG audio header, read big-endian into a u32: the fields by their bits */
#define MP3_HDR_EMPHASIS(h) ((h) & 0x3u)
#define MP3_HDR_ORIGINAL(h) (((h) >> 2) & 0x1u)
#define MP3_HDR_COPYRIGHT(h) (((h) >> 3) & 0x1u)
#define MP3_HDR_MODE_EXT(h) (((h) >> 4) & 0x3u)
#define MP3_HDR_CHANNEL_MODE(h) (((h) >> 6) & 0x3u)
#define MP3_HDR_PADDING(h) (((h) >> 9) & 0x1u)
#define MP3_HDR_SAMPLING_RATE(h) (((h) >> 10) & 0x3u)
#define MP3_HDR_BITRATE(h) (((h) >> 12) & 0xFu)
#define MP3_HDR_PROTECTION(h) (((h) >> 16) & 0x1u)
#define MP3_HDR_LAYER(h) (((h) >> 17) & 0x3u)
#define MP3_HDR_VERSION(h) (((h) >> 19) & 0x3u)
#define MP3_HDR_SYNC(h) (((h) >> 21) & 0x7FFu)

static u32 byteswap32(u32 v) {
    return (v >> 24) | ((v >> 8) & 0xFF00u) | ((v << 8) & 0xFF0000u) | (v << 24);
}

static u16 byteswap16(u16 v) {
    return (u16)((v >> 8) | (v << 8));
}

void ajm_mp3_init(ajm_mp3_t* d, u32 format, u32 flags) {
    memset(d, 0, sizeof(*d));
    d->format = format;
    d->flags = flags;
    rmp3dec_init(&d->decoder);
}

void ajm_mp3_reset(ajm_mp3_t* d) {
    rmp3dec_init(&d->decoder);
    d->has_header = false;
    d->header = 0;
    d->frame_samples = 0;
}

void ajm_mp3_get_info(const ajm_mp3_t* d, void* out_info) {
    AjmSidebandDecMp3CodecInfo* info = (AjmSidebandDecMp3CodecInfo*)out_info;
    if (d->has_header) {
        const u32 h = d->header;
        info->header = byteswap32(d->header);
        info->has_crc = MP3_HDR_PROTECTION(h) != 0;
        info->channel_mode = (u8)MP3_HDR_CHANNEL_MODE(h);
        info->mode_extension = (u8)MP3_HDR_MODE_EXT(h);
        info->copyright = (u8)MP3_HDR_COPYRIGHT(h);
        info->original = (u8)MP3_HDR_ORIGINAL(h);
        info->emphasis = (u8)MP3_HDR_EMPHASIS(h);
    }
}

u32 ajm_mp3_get_minimum_input_size(const ajm_mp3_t* d) {
    (void)d;
    /* 4 bytes is for mp3 header that contains frame_size */
    return 4;
}

/* lrint at the default rounding: to nearest, ties to even */
static double round_even(double v) {
    const double r = floor(v);
    const double diff = v - r;
    if (diff > 0.5) {
        return r + 1.0;
    }
    if (diff == 0.5 && fmod(r, 2.0) != 0.0) {
        return r + 1.0;
    }
    return r;
}

static s32 float_to_s32(float x) {
    if (x >= 1.0f) {
        return INT_MAX;
    }
    if (x <= -1.0f) {
        return INT_MIN;
    }
    return (s32)round_even((double)x * 2147483648.0);
}

typedef struct mp3_fill {
    const ajm_mp3_t* d;
    u32 skip_samples;
    u32 channels;
} mp3_fill_t;

static void mp3_fill_s16(void* ctx, size_t first, size_t n, void* dst) {
    const mp3_fill_t* f = (const mp3_fill_t*)ctx;
    memcpy(dst, f->d->pcm.s16 + (f->skip_samples + first) * f->channels,
           n * f->channels * sizeof(s16));
}

static void mp3_fill_s32(void* ctx, size_t first, size_t n, void* dst) {
    const mp3_fill_t* f = (const mp3_fill_t*)ctx;
    const float* src = f->d->pcm.f32 + (f->skip_samples + first) * f->channels;
    s32* out = (s32*)dst;
    size_t i;
    for (i = 0; i < n * f->channels; ++i) {
        out[i] = float_to_s32(src[i]);
    }
}

static void mp3_fill_f32(void* ctx, size_t first, size_t n, void* dst) {
    const mp3_fill_t* f = (const mp3_fill_t*)ctx;
    memcpy(dst, f->d->pcm.f32 + (f->skip_samples + first) * f->channels,
           n * f->channels * sizeof(float));
}

ajm_decoder_result_t ajm_mp3_process_data(ajm_mp3_t* d, ajm_input_t* in_buf, ajm_output_t* output,
                                          ajm_gapless_t* gapless) {
    ajm_decoder_result_t result;
    rmp3dec_frame_info info;
    rmp3dec* dec = &d->decoder;
    const int mp3_bytes = (int)(in_buf->size < (size_t)INT_MAX ? in_buf->size : (size_t)INT_MAX);
    const bool s16_out = d->format == AJM_FORMAT_ENCODING_S16;
    const u8* data = in_buf->data;
    size_t frame_bytes;
    size_t start = 0;
    u32 raw_header = 0;
    AjmDecMp3ParseFrame parsed;
    u32 skip_samples = 0;
    u32 max_samples;
    u32 count;
    size_t written = 0;
    mp3_fill_t fill;
    int frame;

    memset(&result, 0, sizeof(result));
    memset(&info, 0, sizeof(info));
    frame = s16_out ? rmp3dec_decode_s16(dec, in_buf->data, mp3_bytes, d->pcm.s16, &info)
                    : rmp3dec_decode_f32(dec, in_buf->data, mp3_bytes, d->pcm.f32, &info);

    if (info.frame_bytes <= 0) {
        result.result |= ORBIS_AJM_RESULT_PARTIAL_INPUT;
        return result;
    }

    frame_bytes = (size_t)info.frame_bytes;
    ajm_input_advance(in_buf, frame_bytes < in_buf->size ? frame_bytes : in_buf->size);

    if (frame == 0) {
        if (info.hz == 0) {
            /* Only junk before a frame that isn't all here yet */
            return result;
        }
        /* A frame whose bit reservoir reaches back before the stream: silence, as long as
         * the frame, so the gapless counts stay in step */
        frame = info.layer == 1 ? 384 : (info.layer == 3 && info.hz < 32000 ? 576 : 1152);
        memset(&d->pcm, 0, sizeof(d->pcm));
    }

    /* The frame starts where its header does, after any junk rmp3 skipped */
    while (start + 4 <= frame_bytes && memcmp(data + start, dec->header, 4) != 0) {
        ++start;
    }

    memcpy(&raw_header, dec->header, sizeof(raw_header));
    d->header = byteswap32(raw_header);
    d->has_header = true;

    memset(&parsed, 0, sizeof(parsed));
    if (start + 4 <= frame_bytes &&
        ajm_mp3_parse_header(data + start, (u32)(frame_bytes - start), 1, &parsed) == ORBIS_OK &&
        (parsed.total_samples != 0 || parsed.encoder_delay != 0)) {
        gapless->init.total_samples = parsed.total_samples;
        gapless->init.skip_samples = (u16)parsed.encoder_delay;
        gapless->init.skipped_samples = 0;
        gapless->current = gapless->init;
    }

    d->frame_samples = (u32)frame;
    d->channels = (u32)info.channels;
    d->sample_rate = (u32)info.hz;
    d->bitrate = (u32)info.bitrate_kbps * 1000;

    result.frames_decoded += 1;

    if (gapless->current.skip_samples > 0) {
        skip_samples =
            (u32)frame < gapless->current.skip_samples ? (u32)frame : gapless->current.skip_samples;
        gapless->current.skip_samples = (u16)(gapless->current.skip_samples - skip_samples);
    }

    max_samples = gapless->init.total_samples != 0 ? gapless->current.total_samples : (u32)frame;
    count = (u32)frame - skip_samples < max_samples ? (u32)frame - skip_samples : max_samples;

    fill.d = d;
    fill.skip_samples = skip_samples;
    fill.channels = d->channels;
    switch (d->format) {
    case AJM_FORMAT_ENCODING_S16:
        written =
            ajm_output_write_frames(output, count, fill.channels, sizeof(s16), mp3_fill_s16, &fill);
        break;
    case AJM_FORMAT_ENCODING_S32:
        written =
            ajm_output_write_frames(output, count, fill.channels, sizeof(s32), mp3_fill_s32, &fill);
        break;
    case AJM_FORMAT_ENCODING_FLOAT:
        written = ajm_output_write_frames(output, count, fill.channels, sizeof(float), mp3_fill_f32,
                                          &fill);
        break;
    default:
        C_ASSERT_MSG(0, "ProcessData", "Unreachable code!");
        break;
    }

    result.samples_written = (u32)(written / fill.channels);
    gapless->current.skipped_samples =
        (u16)(gapless->current.skipped_samples + (u32)frame - result.samples_written);
    if (gapless->init.total_samples != 0) {
        gapless->current.total_samples -= result.samples_written;
    }
    return result;
}

u32 ajm_mp3_get_next_frame_size(const ajm_mp3_t* d, const ajm_gapless_t* gapless) {
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

/* Reads bits, most significant first; past the end it reads zeros */
typedef struct bit_reader {
    const u8* data;
    size_t max_bytes;
    size_t bit_offset;
} bit_reader_t;

static u32 bit_reader_get(bit_reader_t* r) {
    u32 bit;
    if (r->bit_offset / 8 >= r->max_bytes) {
        r->bit_offset += 1;
        return 0;
    }
    bit = (r->data[r->bit_offset / 8] >> (7 - (r->bit_offset % 8))) & 1;
    r->bit_offset += 1;
    return bit;
}

static u16 bit_reader_read16(bit_reader_t* r, u32 nbits) {
    u16 accumulator = 0;
    u32 i;
    for (i = 0; i < nbits; ++i) {
        accumulator = (u16)((accumulator << 1) + bit_reader_get(r));
    }
    return accumulator;
}

static u8 fgh_crc(u8 c, u8 v, u8 s) {
    if (((c >> 7) & 1) != ((v >> s) & 1)) {
        return (u8)(c * 2);
    }
    return (u8)((c * 2) ^ 0x45);
}

static void parse_mp3_extended_header(const u8* p_begin, u32 stream_size, u32 header,
                                      AjmDecMp3ParseFrame* frame) {
    const u8* p_end =
        p_begin + ((u64)stream_size < frame->frame_size ? (u64)stream_size : frame->frame_size);
    const u8* p_current = p_begin + 4;
    size_t available_bytes;
    bit_reader_t reader;
    u32 part2_3_length = 0;
    u8 ngr;
    u8 gr;
    size_t byte_offset;

    if (p_current >= p_end) {
        return;
    }
    available_bytes = (size_t)(p_end - p_current);
    reader.data = p_current;
    reader.max_bytes = available_bytes;
    reader.bit_offset = 0;

    if (MP3_HDR_PROTECTION(header) == 0) {
        reader.bit_offset += 16;
    }
    if (MP3_HDR_VERSION(header) == MP3_VERSION_V1) {
        reader.bit_offset += MP3_HDR_CHANNEL_MODE(header) == MP3_SINGLE_CHANNEL ? 18 : 20;
    } else {
        reader.bit_offset += MP3_HDR_CHANNEL_MODE(header) == MP3_SINGLE_CHANNEL ? 9 : 10;
    }

    ngr = MP3_HDR_VERSION(header) == MP3_VERSION_V1 ? 2 : 1;
    for (gr = 0; gr < ngr; ++gr) {
        u32 ch;
        for (ch = 0; ch < frame->num_channels; ++ch) {
            part2_3_length += bit_reader_read16(&reader, 12);
            reader.bit_offset += MP3_HDR_VERSION(header) == MP3_VERSION_V1 ? 47 : 51;
        }
    }
    reader.bit_offset += part2_3_length;

    byte_offset = (reader.bit_offset + 7) / 8;
    if (byte_offset > available_bytes) {
        p_current = p_end;
    } else {
        p_current += byte_offset;
    }

    if (p_current + 8 <= p_end &&
        (memcmp(p_current, "Xing", 4) == 0 || memcmp(p_current, "Info", 4) == 0)) {
        const u8 xing_flags = p_current[7];
        const u8* p_field = p_current + 8;
        u32 num_frames_xing = 0;
        if (xing_flags & 0x01) {
            u32 raw = 0;
            if (p_field + 4 > p_end) {
                return;
            }
            memcpy(&raw, p_field, sizeof(raw));
            num_frames_xing = byteswap32(raw);
            frame->num_frames = num_frames_xing;
            p_field += 4;
        }
        if (xing_flags & 0x02) {
            p_field += 4;
            if (p_field > p_end) {
                return;
            }
        }
        if (xing_flags & 0x04) {
            p_field += 100;
            if (p_field > p_end) {
                return;
            }
        }
        if (xing_flags & 0x08) {
            p_field += 4;
            if (p_field > p_end) {
                return;
            }
        }
        if (p_field + 0x18 <= p_end && p_field[0] == 'L' && p_field[1] == 'A' &&
            p_field[2] == 'M' && p_field[3] == 'E') {
            const u32 enc_delay = ((u32)p_field[0x15] << 4) | ((u32)p_field[0x16] >> 4);
            const u32 padding = (((u32)p_field[0x16] & 0x0f) << 8) | (u32)p_field[0x17];
            if ((xing_flags & 0x01) && num_frames_xing > 0) {
                frame->total_samples =
                    (num_frames_xing * frame->samples_per_channel) - (enc_delay + padding);
            }
            frame->encoder_delay = frame->samples_per_channel + enc_delay + 529;
            frame->ofl_type = AJM_DEC_MP3_OFL_LAME;
        }
    } else if (p_current + 26 <= p_end && memcmp(p_current, "VBRI", 4) == 0) {
        u16 vbri_delay = 0;
        memcpy(&vbri_delay, p_current + 6, sizeof(vbri_delay));
        frame->encoder_delay = byteswap16(vbri_delay);
        frame->ofl_type = AJM_DEC_MP3_OFL_VBRI;
        if (frame->frame_size <= stream_size) {
            const u8* next_frame = p_begin + frame->frame_size;
            const u32 remaining = stream_size - (u32)frame->frame_size;
            AjmDecMp3ParseFrame next_info;
            memset(&next_info, 0, sizeof(next_info));
            if (ajm_mp3_parse_header(next_frame, remaining, 1, &next_info) == ORBIS_OK &&
                next_info.ofl_type == AJM_DEC_MP3_OFL_FGH) {
                frame->encoder_delay += next_info.encoder_delay;
                frame->total_samples = next_info.total_samples;
                frame->ofl_type = AJM_DEC_MP3_OFL_VBRI_AND_FGH;
            }
        }
    } else {
        const u8* p_fgh_end =
            p_begin + ((u64)stream_size < frame->frame_size ? (u64)stream_size : frame->frame_size);
        if (p_current + 10 <= p_fgh_end) {
            const u8 fgh_indicator = 0xB4;
            const u8* p_fgh;
            while ((p_current + 9) < p_fgh_end && *p_current != fgh_indicator) {
                ++p_current;
            }
            p_fgh = p_current;
            if ((p_current + 9) < p_fgh_end && *p_current == fgh_indicator) {
                u8 crc = 0xFF;
                u8 i;
                u8 j;
                for (i = 0; i < 9; ++i, ++p_current) {
                    for (j = 0; j < 8; ++j) {
                        crc = fgh_crc(crc, *p_current, (u8)(7 - j));
                    }
                }
                if (p_fgh[9] == crc) {
                    u16 encoder_delay = 0;
                    u32 total_samples = 0;
                    memcpy(&encoder_delay, p_fgh + 1, sizeof(encoder_delay));
                    memcpy(&total_samples, p_fgh + 3, sizeof(total_samples));
                    frame->encoder_delay = byteswap16(encoder_delay);
                    frame->total_samples = byteswap32(total_samples);
                    frame->ofl_type = AJM_DEC_MP3_OFL_FGH;
                }
            }
        }
    }
}

static int parse_mp3_header_common(const u8* p_begin, u32 stream_size, int parse_ofl,
                                   AjmDecMp3ParseFrame* frame, bool enforce_layer3) {
    u32 raw_header = 0;
    u32 header;
    if (p_begin == NULL || stream_size < 4 || frame == NULL) {
        return ORBIS_AJM_ERROR_INVALID_PARAMETER;
    }
    memcpy(&raw_header, p_begin, sizeof(raw_header));
    header = byteswap32(raw_header);
    if (MP3_HDR_SYNC(header) != 0x7FF) {
        return ORBIS_AJM_ERROR_INVALID_PARAMETER;
    }
    if (enforce_layer3 && MP3_HDR_LAYER(header) != 1) {
        return ORBIS_AJM_ERROR_INVALID_PARAMETER;
    }
    frame->sample_rate =
        (u32)mp3_sample_rate_table[MP3_HDR_VERSION(header)][MP3_HDR_SAMPLING_RATE(header)];
    if (frame->sample_rate == 0) {
        return ORBIS_AJM_ERROR_INVALID_PARAMETER;
    }
    frame->bitrate =
        (u32)mp3_bit_rate_table[MP3_HDR_VERSION(header)][MP3_HDR_BITRATE(header)] * 1000;
    if (frame->bitrate == 0) {
        return ORBIS_AJM_ERROR_INVALID_PARAMETER;
    }
    frame->num_channels = MP3_HDR_CHANNEL_MODE(header) == MP3_SINGLE_CHANNEL ? 1 : 2;
    if (MP3_HDR_VERSION(header) == MP3_VERSION_V1) {
        frame->frame_size = (144 * frame->bitrate) / frame->sample_rate + MP3_HDR_PADDING(header);
        frame->samples_per_channel = 1152;
    } else {
        frame->frame_size = (72 * frame->bitrate) / frame->sample_rate + MP3_HDR_PADDING(header);
        frame->samples_per_channel = 576;
    }
    frame->encoder_delay = 0;
    frame->num_frames = 0;
    frame->total_samples = 0;
    frame->ofl_type = AJM_DEC_MP3_OFL_NONE;
    if (parse_ofl) {
        parse_mp3_extended_header(p_begin, stream_size, header, frame);
    }
    return ORBIS_OK;
}

int ajm_mp3_parse_header(const u8* p_begin, u32 stream_size, int parse_ofl,
                         AjmDecMp3ParseFrame* frame) {
#ifndef NDEBUG
    c_log(C_LOG_LIB_AJM, C_LOG_LEVEL_TRACE, "ParseMp3Header",
          "called stream_size = %u parse_ofl = %d", stream_size, parse_ofl);
#endif
    return parse_mp3_header_common(p_begin, stream_size, parse_ofl, frame, false);
}

int ajm_mp3_parse_frame(const u8* p_begin, u32 stream_size, int parse_ofl,
                        AjmDecMp3ParseFrame* frame) {
#ifndef NDEBUG
    c_log(C_LOG_LIB_AJM, C_LOG_LEVEL_TRACE, "ParseMp3Frame",
          "called stream_size = %u parse_ofl = %d", stream_size, parse_ofl);
#endif
    return parse_mp3_header_common(p_begin, stream_size, parse_ofl, frame, true);
}

AjmSidebandFormat ajm_mp3_get_format(const ajm_mp3_t* d) {
    AjmSidebandFormat f;
    f.num_channels = d->channels;
    f.channel_mask = ajm_channel_mask(d->channels);
    f.sampl_freq = d->sample_rate;
    f.sample_encoding = d->format;
    f.bitrate = d->bitrate;
    f.reserved = 0;
    return f;
}
