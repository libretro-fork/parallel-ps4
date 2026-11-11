/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <math.h>
#include <string.h>

#include "ngs2_util.h"

#define PI_F 3.14159265f
#define DEGREES 360.0f
#define RADIANS (2.0f * PI_F)
#define MAX_SAMPLE_RATE 192000u
#define VAG_SAMPLES 28u
#define VAG_BYTES 16u
#define VAG_HEADER 0x30u
#define SCAN_BYTES 4096u

/* ------------------------------------------------------------------ */
/* Waveforms                                                           */

static const uint32_t pcm_width[12] = {1, 1, 2, 2, 3, 3, 4, 4, 4, 4, 8, 8}; /* types 0x10.. */

typedef struct at9_info {
    uint32_t config; /* its four bytes, the first the highest */
    uint32_t channels;
    uint32_t rate;
    uint32_t frame_samples;
    uint32_t frames; /* in a superframe */
    uint32_t superframe_bytes;
} at9_info;

static int at9_read(uint32_t config, at9_info* a) {
    static const uint32_t rates[16] = {11025, 12000, 16000, 22050, 24000, 32000,  44100,  48000,
                                       44100, 48000, 64000, 88200, 96000, 128000, 176400, 192000};
    static const uint8_t powers[16] = {6, 6, 7, 7, 7, 8, 8, 8, 6, 6, 7, 7, 7, 8, 8, 8};
    static const uint8_t channels[8] = {1, 2, 2, 6, 8, 4, 0, 0};
    uint32_t shift;
    /* A game may have the bytes as a word of its own machine */
    if ((config >> 24) != 0xfeu && (config & 0xffu) == 0xfeu)
        config = (config >> 24) | ((config >> 8) & 0xff00u) | ((config << 8) & 0xff0000u) |
                 (config << 24);
    if ((config >> 24) != 0xfeu)
        return 0;
    a->config = config;
    a->channels = channels[(config >> 17) & 7u];
    a->rate = rates[(config >> 20) & 0xfu];
    a->frame_samples = 1u << powers[(config >> 20) & 0xfu];
    shift = (config >> 3) & 3u;
    a->frames = 1u << shift;
    a->superframe_bytes = (((config >> 5) & 0x7ffu) + 1u) << shift;
    return a->channels != 0;
}

static void pcm_block(uint32_t type, uint32_t channels, uint32_t pos, uint32_t count,
                      ngs2_waveform_block* b) {
    uint32_t stride = channels * pcm_width[type - 0x10u];
    memset(b, 0, sizeof(*b));
    b->data_offset = pos * stride;
    b->data_size = count * stride;
    b->num_samples = count;
}

static void vag_block(uint32_t channels, uint32_t pos, uint32_t count, ngs2_waveform_block* b) {
    uint32_t frame_bytes = VAG_BYTES * channels;
    uint32_t first = pos / VAG_SAMPLES;
    uint32_t past = (pos + count + VAG_SAMPLES - 1u) / VAG_SAMPLES;
    memset(b, 0, sizeof(*b));
    b->data_offset = first * frame_bytes;
    b->data_size = (past - first) * frame_bytes;
    b->num_samples = (past - first) * VAG_SAMPLES;
}

/* Whole superframes; the decoder's first frame of delay and the way into the
 * first superframe are samples to skip */
static void at9_block(const at9_info* a, uint32_t pos, uint32_t count, ngs2_waveform_block* b) {
    uint32_t superframe_samples = a->frame_samples * a->frames;
    uint32_t first = pos / superframe_samples;
    uint32_t lead = a->frame_samples + pos;
    uint32_t past = (count + lead + superframe_samples - 1u) / superframe_samples;
    memset(b, 0, sizeof(*b));
    b->data_offset = first * a->superframe_bytes;
    b->data_size = (past - first) * a->superframe_bytes;
    b->num_skip_samples = lead - first * superframe_samples;
    b->num_samples = count;
}

int ngs2_calc_waveform_block(const ngs2_waveform_format* format, uint32_t pos, uint32_t count,
                             ngs2_waveform_block* block) {
    at9_info a;
    if (!block)
        return NGS2_ERROR_INVALID_OUT_ADDRESS;
    memset(block, 0, sizeof(*block));
    if (!format)
        return NGS2_ERROR_INVALID_WAVEFORM_FORMAT;
    if (format->type >= 0x10u && format->type <= NGS2_WAVE_PCM_F64B) {
        pcm_block(format->type, format->channels, pos, count, block);
        return NGS2_OK;
    }
    if (format->type == NGS2_WAVE_VAG) {
        vag_block(format->channels, pos, count, block);
        return NGS2_OK;
    }
    if (format->type != NGS2_WAVE_ATRAC9)
        return NGS2_ERROR_UNKNOWN_WAVEFORM_FORMAT;
    if (!at9_read(format->config_data, &a))
        return NGS2_ERROR_INVALID_WAVEFORM_FORMAT;
    at9_block(&a, pos, count, block);
    return NGS2_OK;
}

int ngs2_waveform_frame_info(const ngs2_waveform_format* format, uint32_t* frame_size,
                             uint32_t* frame_samples, uint32_t* units_per_frame,
                             uint32_t* delay_samples) {
    at9_info a;
    if (!frame_size || !frame_samples || !units_per_frame || !delay_samples)
        return NGS2_ERROR_INVALID_OUT_ADDRESS;
    if (!format || !format->channels || format->channels > NGS2_MAX_CHANNELS ||
        !format->sample_rate || format->sample_rate > MAX_SAMPLE_RATE)
        return NGS2_ERROR_INVALID_WAVEFORM_FORMAT;
    *units_per_frame = 1;
    *delay_samples = 0;
    if (format->type >= 0x10u && format->type <= NGS2_WAVE_PCM_F64B) {
        *frame_size = format->channels * pcm_width[format->type - 0x10u];
        *frame_samples = 1;
        return NGS2_OK;
    }
    if (format->type == NGS2_WAVE_VAG) {
        *frame_size = format->channels * VAG_BYTES;
        *frame_samples = VAG_SAMPLES;
        return NGS2_OK;
    }
    if (format->type != NGS2_WAVE_ATRAC9 || !at9_read(format->config_data, &a) ||
        a.channels != format->channels || a.rate != format->sample_rate)
        return NGS2_ERROR_INVALID_WAVEFORM_FORMAT;
    *frame_size = a.superframe_bytes;
    *frame_samples = a.frame_samples * a.frames;
    *units_per_frame = a.frames;
    *delay_samples = a.frame_samples;
    return NGS2_OK;
}

static uint32_t le16(const uint8_t* p) {
    return (uint32_t)p[0] | (uint32_t)p[1] << 8;
}

static uint32_t le32(const uint8_t* p) {
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}

static uint32_t be32(const uint8_t* p) {
    return (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 8 | (uint32_t)p[3];
}

/* The blocks a sampler plays for it: up to the loop, the loop for ever, and
 * what is after it; all of it as one block where it has no loop */
static void add_block(ngs2_waveform_info* info, const at9_info* a, uint32_t begin, uint32_t samples,
                      uint32_t repeats) {
    ngs2_waveform_block* b = &info->blocks[info->num_blocks++];
    if (info->format.type == NGS2_WAVE_ATRAC9)
        at9_block(a, begin, samples, b);
    else if (info->format.type == NGS2_WAVE_VAG)
        vag_block(info->format.channels, begin, samples, b);
    else
        pcm_block(info->format.type, info->format.channels, begin, samples, b);
    /* Not past the data there is */
    if (b->data_offset > info->data_size)
        b->data_offset = info->data_size;
    if (b->data_size > info->data_size - b->data_offset)
        b->data_size = info->data_size - b->data_offset;
    b->data_offset += info->data_offset;
    b->num_repeats = repeats;
}

static void loop_blocks(ngs2_waveform_info* info, const at9_info* a) {
    uint32_t begin = info->loop_begin, end = info->loop_end;
    if (end > info->num_samples)
        end = info->num_samples;
    if (end <= begin)
        return;
    info->num_blocks = 0;
    if (begin)
        add_block(info, a, 0, begin, 0);
    add_block(info, a, begin, end - begin, NGS2_REPEAT_ENDLESS);
    if (end != info->num_samples)
        add_block(info, a, end, info->num_samples - end, 0);
}

static int parse_vag(ngs2_read_fn read, void* user, uint64_t size, ngs2_waveform_info* info) {
    uint8_t header[VAG_HEADER];
    uint8_t scan[SCAN_BYTES];
    uint32_t channels, frame_bytes, data_size, frames, frame = 0;
    int drops;
    if (size < VAG_HEADER || read(user, 0, header, VAG_HEADER))
        return NGS2_ERROR_INVALID_WAVEFORM_DATA;
    channels = header[0x1e] ? header[0x1e] : 1u;
    if (channels > NGS2_MAX_CHANNELS)
        return NGS2_ERROR_INVALID_WAVEFORM_FORMAT;
    frame_bytes = VAG_BYTES * channels;
    data_size = be32(header + 0x0c);
    if (data_size > size - VAG_HEADER || size - VAG_HEADER < (uint64_t)frame_bytes + VAG_BYTES ||
        read(user, (uint64_t)VAG_HEADER + frame_bytes, scan, VAG_BYTES))
        return NGS2_ERROR_INVALID_WAVEFORM_DATA;
    /* The second frame says whether the first is one to leave out */
    drops = !(scan[1] & 2u);
    frames = data_size / frame_bytes;
    info->format.type = NGS2_WAVE_VAG;
    info->format.channels = channels;
    info->format.sample_rate = be32(header + 0x10);
    info->data_offset = VAG_HEADER;
    info->data_size = data_size;
    info->num_samples = frames * VAG_SAMPLES;
    info->audio_unit_size = info->audio_frame_size = frame_bytes;
    info->audio_unit_samples = info->audio_frame_samples = VAG_SAMPLES;
    info->audio_units_per_frame = 1;
    if (drops) {
        info->num_samples = info->num_samples > VAG_SAMPLES ? info->num_samples - VAG_SAMPLES : 0;
    } else {
        /* Each frame's flag: 6 where the loop starts, 3 where it ends */
        while (frame < frames) {
            uint32_t n = SCAN_BYTES / frame_bytes, i;
            if (n > frames - frame)
                n = frames - frame;
            if (read(user, (uint64_t)VAG_HEADER + (uint64_t)frame * frame_bytes, scan,
                     (size_t)n * frame_bytes))
                return NGS2_ERROR_INVALID_WAVEFORM_DATA;
            for (i = 0; i < n; i++) {
                uint32_t flag = scan[(size_t)i * frame_bytes + 1u] & 0xfu;
                if (flag == 3u)
                    info->loop_end = (frame + i + 1u) * VAG_SAMPLES;
                else if (flag == 6u)
                    info->loop_begin = (frame + i) * VAG_SAMPLES;
            }
            frame += n;
        }
    }
    add_block(info, NULL, 0, info->num_samples, 0);
    loop_blocks(info, NULL);
    return NGS2_OK;
}

static const uint8_t guid_pcm[16] = {0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x00,
                                     0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71};
static const uint8_t guid_at9[16] = {0xd2, 0x42, 0xe1, 0x47, 0xba, 0x36, 0x8d, 0x4d,
                                     0x88, 0xfc, 0x61, 0x65, 0x4f, 0x8c, 0x83, 0x6c};

int ngs2_parse_waveform(ngs2_read_fn read, void* user, uint64_t size, ngs2_waveform_info* info) {
    uint8_t head[12], fmt[64], fact[12], smpl[52];
    uint32_t fmt_size = 0, fact_size = 0, smpl_size = 0, tag;
    uint64_t riff_end, end, offset, data_offset = 0, data_size = 0;
    at9_info a;
    int is_float = 0, is_at9 = 0;
    if (!info)
        return NGS2_ERROR_INVALID_OUT_ADDRESS;
    memset(info, 0, sizeof(*info));
    memset(&a, 0, sizeof(a));
    if (!read || size < 4 || read(user, 0, head, 4))
        return NGS2_ERROR_INVALID_WAVEFORM_DATA;
    if (!memcmp(head, "VAGp", 4))
        return parse_vag(read, user, size, info);
    if (size < 12 || read(user, 0, head, 12))
        return NGS2_ERROR_INVALID_WAVEFORM_DATA;
    if (memcmp(head, "RIFF", 4) || memcmp(head + 8, "WAVE", 4))
        return NGS2_ERROR_UNKNOWN_WAVEFORM_FORMAT;
    riff_end = 8u + (uint64_t)le32(head + 4);
    if (riff_end < 12)
        return NGS2_ERROR_INVALID_WAVEFORM_DATA;
    end = size < riff_end ? size : riff_end;
    for (offset = 12; offset + 8 <= end;) {
        uint8_t chunk[8];
        uint64_t payload = offset + 8, chunk_size;
        uint8_t* into = NULL;
        uint32_t room = 0, *got = NULL;
        if (read(user, offset, chunk, 8))
            return NGS2_ERROR_INVALID_WAVEFORM_DATA;
        chunk_size = le32(chunk + 4);
        if (chunk_size > riff_end - payload)
            return NGS2_ERROR_INVALID_WAVEFORM_DATA;
        if (!memcmp(chunk, "data", 4)) {
            data_offset = payload;
            data_size = chunk_size;
            /* A stream's data may go on past what is here */
            if (chunk_size > end - payload)
                break;
        }
        if (chunk_size > end - payload)
            return NGS2_ERROR_INVALID_WAVEFORM_DATA;
        if (!memcmp(chunk, "fmt ", 4)) {
            into = fmt;
            room = sizeof(fmt);
            got = &fmt_size;
        } else if (!memcmp(chunk, "fact", 4)) {
            into = fact;
            room = sizeof(fact);
            got = &fact_size;
        } else if (!memcmp(chunk, "smpl", 4)) {
            into = smpl;
            room = sizeof(smpl);
            got = &smpl_size;
        }
        if (into) {
            *got = chunk_size < room ? (uint32_t)chunk_size : room;
            if (*got && read(user, payload, into, *got))
                return NGS2_ERROR_INVALID_WAVEFORM_DATA;
        }
        offset = payload + chunk_size + (chunk_size & 1u);
    }
    if (!fmt_size || !data_offset || data_offset > 0xffffffffu)
        return NGS2_ERROR_INVALID_WAVEFORM_DATA;
    if (fmt_size < 2)
        return NGS2_ERROR_INVALID_WAVEFORM_FORMAT;
    tag = le16(fmt);
    if (tag == 0xfffeu) {
        if (fmt_size < 40)
            return NGS2_ERROR_UNKNOWN_WAVEFORM_FORMAT;
        if (!memcmp(fmt + 24, guid_at9, 16))
            is_at9 = 1;
        else if (memcmp(fmt + 24, guid_pcm, 16))
            return NGS2_ERROR_UNKNOWN_WAVEFORM_FORMAT;
    } else if (tag == 3u) {
        is_float = 1;
    } else if (tag != 1u) {
        return NGS2_ERROR_UNKNOWN_WAVEFORM_FORMAT;
    }
    if (data_size > size - data_offset)
        return NGS2_ERROR_INVALID_WAVEFORM_DATA;
    info->data_offset = (uint32_t)data_offset;
    info->data_size = (uint32_t)data_size;
    if (is_at9) {
        if (fmt_size < 52 || fact_size < 12)
            return NGS2_ERROR_INVALID_WAVEFORM_FORMAT;
        if (!at9_read(be32(fmt + 44), &a) || le16(fmt + 2) != a.channels ||
            le32(fmt + 4) != a.rate || le16(fmt + 12) != a.superframe_bytes ||
            le16(fmt + 18) != a.frame_samples * a.frames)
            return NGS2_ERROR_INVALID_WAVEFORM_FORMAT;
        info->format.type = NGS2_WAVE_ATRAC9;
        info->format.channels = a.channels;
        info->format.sample_rate = a.rate;
        info->format.config_data = a.config;
        info->num_samples = le32(fact);
        info->audio_unit_size = a.superframe_bytes / a.frames;
        info->audio_unit_samples = a.frame_samples;
        info->audio_units_per_frame = a.frames;
        info->audio_frame_size = a.superframe_bytes;
        info->audio_frame_samples = a.frame_samples * a.frames;
        info->delay_samples = a.frame_samples;
        info->num_blocks = 1;
        info->blocks[0].data_offset = info->data_offset;
        info->blocks[0].data_size = info->data_size;
        info->blocks[0].num_skip_samples = le32(fact + 8);
        info->blocks[0].num_samples = info->num_samples;
    } else {
        uint32_t bits, channels, rate, frame_bytes, type = 0;
        if (fmt_size < 16)
            return NGS2_ERROR_INVALID_WAVEFORM_FORMAT;
        bits = le16(fmt + 14);
        channels = le16(fmt + 2);
        rate = le32(fmt + 4);
        if (is_float)
            type = bits == 32 ? NGS2_WAVE_PCM_F32L : bits == 64 ? NGS2_WAVE_PCM_F64L : 0;
        else
            type = bits == 8    ? NGS2_WAVE_PCM_U8
                   : bits == 16 ? NGS2_WAVE_PCM_I16L
                   : bits == 24 ? NGS2_WAVE_PCM_I24L
                   : bits == 32 ? NGS2_WAVE_PCM_I32L
                                : 0;
        if (!type)
            return NGS2_ERROR_UNKNOWN_WAVEFORM_FORMAT;
        frame_bytes = channels * (bits / 8u);
        if (!channels || channels > NGS2_MAX_CHANNELS || !rate || rate > MAX_SAMPLE_RATE ||
            le16(fmt + 12) != frame_bytes)
            return NGS2_ERROR_INVALID_WAVEFORM_FORMAT;
        if (data_size % frame_bytes)
            return NGS2_ERROR_INVALID_WAVEFORM_DATA;
        info->format.type = type;
        info->format.channels = channels;
        info->format.sample_rate = rate;
        info->num_samples = (uint32_t)(data_size / frame_bytes);
        info->audio_unit_size = info->audio_frame_size = frame_bytes;
        info->audio_unit_samples = info->audio_frame_samples = 1;
        info->audio_units_per_frame = 1;
        info->num_blocks = 1;
        info->blocks[0].data_offset = info->data_offset;
        info->blocks[0].data_size = info->data_size;
        info->blocks[0].num_samples = info->num_samples;
    }
    /* The first loop of the sampler chunk: its start and its last sample. An
     * ATRAC9 file counts them with the decoder's delay in front. */
    if (smpl_size >= 52 && le32(smpl + 28)) {
        uint32_t begin = le32(smpl + 44), last = le32(smpl + 48);
        if (begin >= info->delay_samples && last >= info->delay_samples) {
            info->loop_begin = begin - info->delay_samples;
            info->loop_end = last - info->delay_samples + 1u;
            loop_blocks(info, &a);
        }
    }
    return NGS2_OK;
}

typedef struct memory_source {
    const uint8_t* data;
    size_t size;
} memory_source;

static int memory_read(void* user, uint64_t offset, void* dst, size_t size) {
    const memory_source* m = (const memory_source*)user;
    if (offset > m->size || size > m->size - (size_t)offset)
        return -1;
    memcpy(dst, m->data + (size_t)offset, size);
    return 0;
}

int ngs2_parse_waveform_data(const void* data, size_t size, ngs2_waveform_info* info) {
    memory_source m;
    if (!info)
        return NGS2_ERROR_INVALID_OUT_ADDRESS;
    if (!data) {
        memset(info, 0, sizeof(*info));
        return NGS2_ERROR_INVALID_WAVEFORM_DATA;
    }
    m.data = (const uint8_t*)data;
    m.size = size;
    return ngs2_parse_waveform(memory_read, &m, size, info);
}

/* ------------------------------------------------------------------ */
/* Panning                                                             */

/* The speakers of a layout, clockwise from the front: where each is among
 * the angles a game gives, which channel of the mix it is, and where it
 * stands when the game gives no angles. */
typedef struct pan_layout {
    uint8_t input[7];
    uint8_t channel[7];
    float degrees[7];
    uint32_t mix_channels;
} pan_layout;

static const pan_layout layout_2 = {{1, 0}, {1, 0}, {90, 270}, 2};
static const pan_layout layout_4 = {{1, 3, 2, 0}, {1, 5, 4, 0}, {40, 110, 250, 320}, 6};
static const pan_layout layout_5 = {{2, 1, 4, 3, 0}, {2, 1, 5, 4, 0}, {0, 40, 110, 250, 320}, 6};
static const pan_layout layout_7 = {
    {2, 1, 4, 6, 5, 3, 0}, {2, 1, 5, 7, 6, 4, 0}, {0, 40, 110, 150, 210, 250, 320}, 8};

static const pan_layout* layout_for(uint32_t speakers) {
    switch (speakers) {
    case 2:
        return &layout_2;
    case 4:
        return &layout_4;
    case 5:
        return &layout_5;
    case 7:
        return &layout_7;
    default:
        return NULL;
    }
}

/* Into [from, from + unit) */
static float wrap(float angle, float from, float unit) {
    if (!(angle == angle) || angle > 1e9f || angle < -1e9f)
        return from;
    angle = from + (float)fmod((double)(angle - from), (double)unit);
    if (angle < from)
        angle += unit;
    if (!(angle < from + unit))
        angle = from;
    return angle;
}

int ngs2_pan_init(ngs2_pan_work* work, const float* speaker_angles, float unit_angle,
                  uint32_t num_speakers) {
    const pan_layout* layout;
    uint32_t i;
    if (!work)
        return NGS2_ERROR_INVALID_OUT_ADDRESS;
    if (speaker_angles && unit_angle != DEGREES && unit_angle != RADIANS)
        return NGS2_ERROR_INVALID_PAN_UNIT_ANGLE;
    if (!speaker_angles && !num_speakers)
        num_speakers = 7;
    layout = layout_for(num_speakers);
    if (!layout)
        return NGS2_ERROR_INVALID_PAN_SPEAKER;
    memset(work, 0, sizeof(*work));
    work->num_speakers = num_speakers;
    if (!speaker_angles) {
        work->unit_angle = unit_angle == RADIANS ? RADIANS : DEGREES;
        for (i = 0; i < num_speakers; i++)
            work->speaker_angle[i] = layout->degrees[i] / DEGREES * work->unit_angle;
    } else {
        work->unit_angle = unit_angle;
        for (i = 0; i < num_speakers; i++)
            work->speaker_angle[i] = wrap(speaker_angles[layout->input[i]], 0.0f, unit_angle);
    }
    /* Once round: the first speaker again */
    work->speaker_angle[num_speakers] = work->speaker_angle[0] + work->unit_angle;
    return NGS2_OK;
}

/* A sound between two speakers goes to both, as the sine of how near it is to each */
static void between(const ngs2_pan_work* work, uint32_t speaker, float angle, float weight,
                    float* gains) {
    float from = work->speaker_angle[speaker - 1u], to = work->speaker_angle[speaker], at;
    if (!(angle >= from) || !(to > angle))
        return;
    at = (angle - from) / (to - from);
    gains[speaker - 1u] += (float)sin((double)((1.0f - at) * (PI_F / 2.0f))) * weight;
    gains[speaker] += (float)sin((double)(at * (PI_F / 2.0f))) * weight;
}

int ngs2_pan_matrix(const ngs2_pan_work* work, const ngs2_pan_param* params, uint32_t count,
                    uint32_t matrix_format, float* matrix) {
    const pan_layout* layout;
    uint32_t p, speaker, channel, dest, n;
    float unit, first;
    if (!work)
        return NGS2_ERROR_INVALID_PAN_WORK;
    if (!params)
        return NGS2_ERROR_INVALID_PAN_PARAM;
    if (!matrix)
        return NGS2_ERROR_INVALID_OUT_ADDRESS;
    if (matrix_format != 1 && matrix_format != 2 && matrix_format != 6 && matrix_format != 8)
        return NGS2_ERROR_INVALID_PAN_MATRIX_FORMAT;
    layout = layout_for(work->num_speakers);
    if (!layout)
        return NGS2_ERROR_INVALID_PAN_SPEAKER;
    if (work->unit_angle != DEGREES && work->unit_angle != RADIANS)
        return NGS2_ERROR_INVALID_PAN_UNIT_ANGLE;
    if (count > NGS2_MAX_CHANNELS)
        return NGS2_ERROR_INVALID_NUM_CHANNELS;
    memset(matrix, 0, (size_t)count * matrix_format * sizeof(float));
    n = work->num_speakers;
    unit = work->unit_angle;
    first = work->speaker_angle[0];
    for (p = 0; p < count; p++) {
        float gains[NGS2_MAX_CHANNELS + 1], mix[NGS2_MAX_CHANNELS];
        float distance = params[p].distance, toward, away, angle, opposite;
        /* At distance 1 all of it is where it points; at 0 as much opposite */
        if (!(distance <= 1.0f))
            distance = 1.0f;
        else if (distance < -1.0f)
            distance = -1.0f;
        toward = (float)sin((double)((distance + 1.0f) * 0.25f * PI_F));
        away = (float)sin((double)((1.0f - distance) * 0.25f * PI_F));
        angle = wrap(params[p].angle, first, unit);
        opposite = wrap(angle + unit * 0.5f, first, unit);
        memset(gains, 0, sizeof(gains));
        memset(mix, 0, sizeof(mix));
        for (speaker = 1; speaker <= n; speaker++) {
            between(work, speaker, angle, toward, gains);
            between(work, speaker, opposite, away, gains);
        }
        mix[layout->channel[0]] = gains[0] + gains[n];
        for (speaker = 1; speaker < n; speaker++)
            mix[layout->channel[speaker]] = gains[speaker];
        for (channel = 0; channel < NGS2_MAX_CHANNELS; channel++)
            mix[channel] = channel == 3 ? params[p].lfe_level : mix[channel] * params[p].fbw_level;
        for (dest = 0; dest < matrix_format; dest++) {
            float level = 0.0f;
            for (channel = 0; channel < layout->mix_channels; channel++)
                level += mix[channel] * (layout->mix_channels == matrix_format
                                             ? (channel == dest ? 1.0f : 0.0f)
                                             : ngs2_default_level(layout->mix_channels, channel,
                                                                  matrix_format, dest));
            matrix[(size_t)dest * count + p] = level;
        }
    }
    return NGS2_OK;
}

/* ------------------------------------------------------------------ */
/* A sound's place in a world                                          */

static double dot3(const double* a, const double* b) {
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}

static void cross3(const double* a, const double* b, double* out) {
    out[0] = a[1] * b[2] - a[2] * b[1];
    out[1] = a[2] * b[0] - a[0] * b[2];
    out[2] = a[0] * b[1] - a[1] * b[0];
}

/* Returns the length it had; a vector of none is left as it is */
static double unit3(double* v) {
    double length = sqrt(dot3(v, v));
    if (length > 0.0 && length == length) {
        v[0] /= length;
        v[1] /= length;
        v[2] /= length;
        return length;
    }
    return 0.0;
}

static void vec3(const ngs2_vector* v, double* out) {
    out[0] = v->x == v->x ? v->x : 0.0;
    out[1] = v->y == v->y ? v->y : 0.0;
    out[2] = v->z == v->z ? v->z : 0.0;
}

int ngs2_geom_reset_listener(ngs2_geom_listener* listener) {
    if (!listener)
        return NGS2_ERROR_INVALID_OUT_ADDRESS;
    memset(listener, 0, sizeof(*listener));
    listener->front.z = 1.0f;
    listener->up.y = 1.0f;
    listener->sound_speed = 343.0f;
    return NGS2_OK;
}

int ngs2_geom_reset_source(ngs2_geom_source* source) {
    if (!source)
        return NGS2_ERROR_INVALID_OUT_ADDRESS;
    memset(source, 0, sizeof(*source));
    source->direction.z = 1.0f;
    source->cone_inner_level = 1.0f;
    source->cone_inner_angle = 360.0f;
    source->cone_outer_level = 1.0f;
    source->cone_outer_angle = 360.0f;
    source->max_distance = 1000000.0f;
    source->rolloff_factor = 1.0f;
    source->reference_distance = 1.0f;
    source->doppler_factor = 1.0f;
    source->fbw_level = 1.0f;
    source->lfe_level = 1.0f;
    source->max_level = 1.0f;
    source->num_speakers = 2;
    source->matrix_format = 2;
    return NGS2_OK;
}

/* The rows of the matrix are the listener's right, up and front, each with
 * what brings the listener's own place to nothing: a place in the world
 * through it is that place as the listener has it. */
int ngs2_geom_calc_listener(const ngs2_geom_listener* listener, ngs2_geom_work* work,
                            uint32_t flags) {
    double position[3], axes[3][3], hint[3];
    uint32_t row, column;
    if (!listener)
        return NGS2_ERROR_INVALID_GEOM_LISTENER_ADDRESS;
    if (!work)
        return NGS2_ERROR_INVALID_OUT_ADDRESS;
    if (flags > 1u)
        return NGS2_ERROR_INVALID_GEOM_FLAG;
    vec3(&listener->position, position);
    vec3(&listener->front, axes[2]);
    vec3(&listener->up, axes[1]);
    if (unit3(axes[2]) == 0.0) {
        axes[2][0] = axes[2][1] = 0.0;
        axes[2][2] = 1.0;
    }
    unit3(axes[1]);
    cross3(axes[1], axes[2], axes[0]);
    if (unit3(axes[0]) == 0.0) {
        /* No up, or up along the front: any up that is across it */
        hint[0] = fabs(axes[2][1]) < 0.9 ? 0.0 : 1.0;
        hint[1] = fabs(axes[2][1]) < 0.9 ? 1.0 : 0.0;
        hint[2] = 0.0;
        cross3(hint, axes[2], axes[0]);
        unit3(axes[0]);
    }
    cross3(axes[2], axes[0], axes[1]);
    if (flags == 1u)
        for (column = 0; column < 3; column++)
            axes[0][column] = -axes[0][column];
    memset(work, 0, sizeof(*work));
    for (row = 0; row < 3; row++) {
        for (column = 0; column < 3; column++)
            work->matrix[row][column] = (float)axes[row][column];
        work->matrix[row][3] = (float)-dot3(axes[row], position);
    }
    work->matrix[3][3] = 1.0f;
    work->velocity = listener->velocity;
    work->sound_speed = listener->sound_speed;
    work->coordinate = flags;
    return NGS2_OK;
}

int ngs2_geom_apply(const ngs2_geom_work* work, const ngs2_geom_source* source,
                    ngs2_geom_attribute* attribute, uint32_t flags) {
    double world[3], place[3], toward[3], direction[3], velocity[3], listener_velocity[3];
    double distance, level = 1.0, pitch = 1.0, reference, far, heard;
    ngs2_pan_work pan;
    ngs2_pan_param param;
    float column[NGS2_MAX_CHANNELS];
    uint32_t row, i, format, speakers;
    (void)flags;
    if (!work)
        return NGS2_ERROR_INVALID_GEOM_LISTENER_ADDRESS;
    if (!source)
        return NGS2_ERROR_INVALID_GEOM_SOURCE_ADDRESS;
    if (!attribute)
        return NGS2_ERROR_INVALID_OUT_ADDRESS;
    memset(attribute, 0, sizeof(*attribute));
    vec3(&source->position, world);
    for (row = 0; row < 3; row++)
        place[row] = work->matrix[row][0] * world[0] + work->matrix[row][1] * world[1] +
                     work->matrix[row][2] * world[2] + work->matrix[row][3];
    distance = sqrt(dot3(place, place));

    /* Quieter with distance: past the reference distance, as that distance over
     * the way there, the way counted by the rolloff factor */
    reference = source->reference_distance > 0.0f ? source->reference_distance : 1.0;
    far = source->max_distance > reference ? source->max_distance : reference;
    heard = distance < reference ? reference : distance > far ? far : distance;
    if (source->rolloff_factor > 0.0f)
        level = reference / (reference + source->rolloff_factor * (heard - reference));

    /* From the source to the listener, in the world */
    for (i = 0; i < 3; i++)
        toward[i] = -(work->matrix[0][i] * place[0] + work->matrix[1][i] * place[1] +
                      work->matrix[2][i] * place[2]);
    if (unit3(toward) != 0.0) {
        /* Quieter off the way the source points, between its two cones */
        double inner = source->cone_inner_angle, outer = source->cone_outer_angle;
        vec3(&source->direction, direction);
        if (unit3(direction) != 0.0 && inner < 360.0 && outer >= inner) {
            double c = dot3(direction, toward);
            double off = 2.0 *
                         acos(c > 1.0    ? 1.0
                              : c < -1.0 ? -1.0
                                         : c) *
                         (180.0 / 3.14159265358979);
            if (off >= outer)
                level *= source->cone_outer_level;
            else if (off > inner)
                level *= source->cone_inner_level +
                         (source->cone_outer_level - source->cone_inner_level) * (off - inner) /
                             (outer - inner);
            else
                level *= source->cone_inner_level;
        }
        /* Higher when the two come nearer, lower when they part */
        if (work->sound_speed > 0.0f && source->doppler_factor > 0.0f) {
            double speed = work->sound_speed, limit = speed / source->doppler_factor * 0.99;
            double vl, vs;
            vec3(&source->velocity, velocity);
            vec3(&work->velocity, listener_velocity);
            vl = dot3(listener_velocity, toward);
            vs = dot3(velocity, toward);
            vl = vl > limit ? limit : vl < -limit ? -limit : vl;
            vs = vs > limit ? limit : vs < -limit ? -limit : vs;
            pitch = (speed - source->doppler_factor * vl) / (speed - source->doppler_factor * vs);
            pitch = pitch > 4.0 ? 4.0 : pitch < 0.25 ? 0.25 : pitch;
        }
    }
    if (source->max_level > 0.0f && level > source->max_level)
        level = source->max_level;
    if (level < source->min_level)
        level = source->min_level;

    attribute->pitch_ratio = (float)pitch;
    attribute->a3d_position.x = (float)place[0];
    attribute->a3d_position.y = (float)place[1];
    attribute->a3d_position.z = (float)place[2];
    attribute->a3d_volume = (float)level;

    /* Where it is round the listener, clockwise from the front; inside its
     * radius it comes from all round */
    format = source->matrix_format;
    if (format != 1 && format != 2 && format != 6 && format != 8)
        format = 2;
    speakers = source->num_speakers;
    if (ngs2_pan_init(&pan, NULL, DEGREES, speakers) != NGS2_OK)
        ngs2_pan_init(&pan, NULL, DEGREES, format == 8 ? 7 : format == 6 ? 5 : 2);
    param.angle = (float)(atan2(place[0], place[2]) * (180.0 / 3.14159265358979));
    param.distance = source->radius > 0.0f && distance < source->radius
                         ? (float)(distance / source->radius)
                         : 1.0f;
    param.fbw_level = (float)level * source->fbw_level;
    param.lfe_level = (float)level * source->lfe_level;
    ngs2_pan_matrix(&pan, &param, 1, format, column);
    for (i = 0; i < format; i++)
        attribute->levels[i] = column[i];
    return NGS2_OK;
}
