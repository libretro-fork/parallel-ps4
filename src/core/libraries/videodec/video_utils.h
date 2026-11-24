/* SPDX-FileCopyrightText: Copyright 2024-2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef SHADPS4_VIDEODEC_VIDEO_UTILS_H
#define SHADPS4_VIDEODEC_VIDEO_UTILS_H

/* An H.264 or HEVC decoder (libretro-common's rh264 and rh265) that writes its pictures as the
 * console's NV12 frames: luma then interleaved chroma, the pitch rounded up to 64 and the
 * height to 16, the last row repeated below the picture. MSVC C89. */

#include <stddef.h>

#include <boolean.h>
#include <retro_common_api.h>

#include "common/types.h"

RETRO_BEGIN_DECLS

/* What the game gave with an access unit, handed back with the picture it opens */
typedef struct video_picture_meta {
    u64 pts;
    u64 dts;
    u64 attached;
} video_picture_meta_t;

/* More than any stream holds back */
#define VIDEO_SW_META_RING 64

typedef struct video_sw_decoder {
    struct rh264_video* h264;
    struct rh265_video* h265;
    video_picture_meta_t ring[VIDEO_SW_META_RING];
    u64 seq;
    /* The ready picture's, valid until the next decode, drain or reset */
    video_picture_meta_t meta;
    u32 width;
    u32 height;
} video_sw_decoder_t;

/* Opens @dec's decoder (zeroed first): HEVC or H.264. Whether it opened. */
bool video_sw_decoder_init(video_sw_decoder_t* dec, bool hevc);
void video_sw_decoder_free(video_sw_decoder_t* dec);

bool video_sw_decoder_is_open(const video_sw_decoder_t* dec);
bool video_sw_decoder_is_hevc(const video_sw_decoder_t* dec);

/* The avcC/hvcC record of an MP4 track, for its length-prefixed samples */
bool video_sw_decoder_set_extradata(video_sw_decoder_t* dec, const u8* data, size_t size);

/* 1 when a picture is ready, 0 when none is (yet), negative for a stream the decoder cannot
 * take */
int video_sw_decoder_decode(video_sw_decoder_t* dec, const u8* data, size_t size,
                            const video_picture_meta_t* meta);
/* The next picture still held for display order, at the end of the stream: 1 or 0 */
int video_sw_decoder_drain(video_sw_decoder_t* dec);
/* Forgets the pictures held for display order; the stream goes on from its next IDR */
void video_sw_decoder_reset(video_sw_decoder_t* dec);

/* The ready picture's frame: its pitch, height, size */
u32 video_sw_decoder_pitch(const video_sw_decoder_t* dec);
u32 video_sw_decoder_aligned_height(const video_sw_decoder_t* dec);
u64 video_sw_decoder_nv12_size(const video_sw_decoder_t* dec);
u8 video_sw_decoder_colour_primaries(const video_sw_decoder_t* dec);
/* False, writing nothing, when the frame does not fit in @max_size */
bool video_sw_decoder_write_nv12(const video_sw_decoder_t* dec, u8* dst, u64 max_size);

RETRO_END_DECLS

#endif
