/* SPDX-FileCopyrightText: Copyright 2024-2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <stdint.h>
#include <string.h>

#include <formats/rh264.h>
#include <formats/rh265.h>

#include "core/libraries/videodec/video_utils.h"

#define ALIGN_UP(v, a) (((v) + (a) - 1) / (a) * (a))

bool video_sw_decoder_init(video_sw_decoder_t* dec, bool hevc) {
    memset(dec, 0, sizeof(*dec));
    if (hevc) {
        dec->h265 = rh265_video_open();
    } else {
        dec->h264 = rh264_video_open();
    }
    return video_sw_decoder_is_open(dec);
}

void video_sw_decoder_free(video_sw_decoder_t* dec) {
    if (dec->h264) {
        rh264_video_close(dec->h264);
    }
    if (dec->h265) {
        rh265_video_close(dec->h265);
    }
    dec->h264 = NULL;
    dec->h265 = NULL;
}

bool video_sw_decoder_is_open(const video_sw_decoder_t* dec) {
    return dec->h264 || dec->h265;
}

bool video_sw_decoder_is_hevc(const video_sw_decoder_t* dec) {
    return dec->h265 != NULL;
}

bool video_sw_decoder_set_extradata(video_sw_decoder_t* dec, const u8* data, size_t size) {
    return dec->h264 ? rh264_video_set_extradata(dec->h264, data, size) == 0
                     : dec->h265 && rh265_video_set_extradata(dec->h265, data, size) == 0;
}

/* The picture the decoder has ready, with the meta of the access unit that opened it */
static bool ready(video_sw_decoder_t* dec, int tag) {
    int stride = 0;
    int w = 0;
    int h = 0;
    const bool ok = dec->h264 ? rh264_video_plane(dec->h264, 0, &stride, &w, &h) != NULL
                              : rh265_video_plane(dec->h265, 0, &stride, &w, &h) != NULL;
    if (!ok || w <= 0 || h <= 0) {
        return false;
    }
    dec->width = (u32)w;
    dec->height = (u32)h;
    dec->meta = dec->ring[(size_t)tag];
    return true;
}

int video_sw_decoder_decode(video_sw_decoder_t* dec, const u8* data, size_t size,
                            const video_picture_meta_t* meta) {
    const u64 seq = dec->seq++;
    int ret;
    s64 tag = 0;
    dec->ring[seq % VIDEO_SW_META_RING] = *meta;
    if (dec->h264) {
        rh264_video_set_tag(dec->h264, seq);
        ret = rh264_video_decode(dec->h264, data, size);
        tag = (s64)rh264_video_tag(dec->h264);
    } else {
        int64_t out = 0;
        ret = rh265_video_decode_timed(dec->h265, data, size, (int64_t)seq, &out);
        tag = out;
    }
    if (ret < 0) {
        return ret;
    }
    return ret == 1 && ready(dec, (int)(tag % VIDEO_SW_META_RING)) ? 1 : 0;
}

int video_sw_decoder_drain(video_sw_decoder_t* dec) {
    int ret;
    s64 tag = 0;
    if (dec->h264) {
        ret = rh264_video_drain(dec->h264);
        tag = (s64)rh264_video_tag(dec->h264);
    } else {
        int64_t out = 0;
        ret = rh265_video_drain_timed(dec->h265, &out);
        tag = out;
    }
    return ret == 0 && ready(dec, (int)(tag % VIDEO_SW_META_RING)) ? 1 : 0;
}

void video_sw_decoder_reset(video_sw_decoder_t* dec) {
    int i;
    /* Handing the held pictures out empties the reorder queue; nothing is freed */
    for (i = 0; i < 64 && video_sw_decoder_drain(dec); ++i) {
    }
    dec->width = dec->height = 0;
}

u32 video_sw_decoder_pitch(const video_sw_decoder_t* dec) {
    return ALIGN_UP(dec->width, 64u);
}

u32 video_sw_decoder_aligned_height(const video_sw_decoder_t* dec) {
    return ALIGN_UP(dec->height, 16u);
}

u64 video_sw_decoder_nv12_size(const video_sw_decoder_t* dec) {
    return (u64)video_sw_decoder_pitch(dec) * video_sw_decoder_aligned_height(dec) * 3 / 2;
}

u8 video_sw_decoder_colour_primaries(const video_sw_decoder_t* dec) {
    if (dec->h264) {
        rh264_video_color_t color;
        if (rh264_video_get_color(dec->h264, &color) == 0) {
            return (u8)color.primaries;
        }
    }
    return 2; /* unspecified */
}

static u32 min_u32(u32 a, u32 b) {
    return a < b ? a : b;
}

/* The planes as the console's NV12, 8 bits a sample: @wide the planes' samples are 16 bits,
 * shifted down by @shift */
static void write_planes(u8* dst, u32 pitch, u32 aligned_h, u32 w, u32 h, const u8* const planes[3],
                         const int strides[3], u32 chroma_w, u32 chroma_h, bool wide, int shift) {
    u8* luma = dst;
    u8* chroma = dst + (size_t)pitch * aligned_h;
    const size_t sample = wide ? 2 : 1;
    const size_t ys = (size_t)strides[0] * sample;
    const size_t us = (size_t)strides[1] * sample;
    const size_t vs = (size_t)strides[2] * sample;
    /* 4:2:2 has a chroma row for every luma row: every other one is taken */
    const u32 step = chroma_h >= h ? 2 : 1;
    const u32 rows = (h + 1) / 2;
    const u32 n = min_u32(chroma_w, pitch / 2);
    u32 y;
    for (y = 0; y < aligned_h; ++y) {
        u8* d = luma + (size_t)y * pitch;
        const u8* s = planes[0] + min_u32(y, h - 1) * ys;
        if (!wide) {
            memcpy(d, s, w);
        } else {
            const u16* s16 = (const u16*)(const void*)s;
            u32 x;
            for (x = 0; x < w; ++x) {
                d[x] = (u8)(s16[x] >> shift);
            }
        }
    }
    for (y = 0; y < aligned_h / 2; ++y) {
        const u32 sy = min_u32(min_u32(y, rows - 1) * step, chroma_h - 1);
        u8* d = chroma + (size_t)y * pitch;
        const u8* su = planes[1] + sy * us;
        const u8* sv = planes[2] + sy * vs;
        u32 x;
        if (!wide) {
            for (x = 0; x < n; ++x) {
                d[2 * x] = su[x];
                d[2 * x + 1] = sv[x];
            }
        } else {
            const u16* su16 = (const u16*)(const void*)su;
            const u16* sv16 = (const u16*)(const void*)sv;
            for (x = 0; x < n; ++x) {
                d[2 * x] = (u8)(su16[x] >> shift);
                d[2 * x + 1] = (u8)(sv16[x] >> shift);
            }
        }
    }
}

bool video_sw_decoder_write_nv12(const video_sw_decoder_t* dec, u8* dst, u64 max_size) {
    const u8* planes[3];
    int strides[3];
    int cw = 0;
    int ch = 0;
    int p;
    int depth;
    if (dec->width == 0 || video_sw_decoder_nv12_size(dec) > max_size) {
        return false;
    }
    for (p = 0; p < 3; ++p) {
        int w = 0;
        int h = 0;
        planes[p] = dec->h264 ? rh264_video_plane(dec->h264, p, &strides[p], &w, &h)
                              : rh265_video_plane(dec->h265, p, &strides[p], &w, &h);
        if (!planes[p]) {
            return false;
        }
        if (p == 1) {
            cw = w;
            ch = h;
        }
    }
    depth = dec->h264 ? rh264_video_bit_depth(dec->h264) : rh265_video_bit_depth(dec->h265);
    write_planes(dst, video_sw_decoder_pitch(dec), video_sw_decoder_aligned_height(dec), dec->width,
                 dec->height, planes, strides, (u32)cw, (u32)ch, depth > 8,
                 depth > 8 ? depth - 8 : 0);
    return true;
}
