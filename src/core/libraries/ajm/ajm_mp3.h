/* SPDX-FileCopyrightText: Copyright 2024-2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef SHADPS4_AJM_AJM_MP3_H
#define SHADPS4_AJM_AJM_MP3_H

/* The MP3 codec, on libretro-common's decoder, and the frame parser the game can call. Defined
 * in ajm_mp3.c. */

#include <boolean.h>
#include <formats/rmp3.h>
#include <retro_common_api.h>

#include "common/types.h"
#include "core/libraries/ajm/ajm_codec.h"

RETRO_BEGIN_DECLS

/* AjmDecMp3OflType */
#define AJM_DEC_MP3_OFL_NONE 0u
#define AJM_DEC_MP3_OFL_LAME 1u
#define AJM_DEC_MP3_OFL_VBRI 2u
#define AJM_DEC_MP3_OFL_FGH 3u
#define AJM_DEC_MP3_OFL_VBRI_AND_FGH 4u

/* AjmMp3CodecFlags */
#define AJM_MP3_IGNORE_OFL 1u
#define AJM_MP3_VLC_REWIND (1u << 8)

typedef struct AjmDecMp3ParseFrame {
    u64 frame_size;
    u32 num_channels;
    u32 samples_per_channel;
    u32 bitrate;
    u32 sample_rate;
    u32 encoder_delay;
    u32 num_frames;
    u32 total_samples;
    u32 ofl_type; /* AjmDecMp3OflType */
} AjmDecMp3ParseFrame;

/* ChannelMode */
#define AJM_MP3_CHANNEL_MODE_STEREO 0u
#define AJM_MP3_CHANNEL_MODE_JOINT_STEREO 1u
#define AJM_MP3_CHANNEL_MODE_DUAL 2u
#define AJM_MP3_CHANNEL_MODE_MONO 3u

typedef struct AjmSidebandDecMp3CodecInfo {
    u32 header;
    bool has_crc;
    u8 channel_mode;
    u8 mode_extension;
    u8 copyright;
    u8 original;
    u8 emphasis;
    u16 reserved[3];
} AjmSidebandDecMp3CodecInfo;

typedef struct ajm_mp3 {
    u32 format; /* AjmFormatEncoding */
    u32 flags;  /* AjmMp3CodecFlags */
    rmp3dec decoder;
    rmp3_frame_buf pcm;
    bool has_header;
    u32 header;
    u32 frame_samples;
    u32 channels;
    u32 sample_rate;
    u32 bitrate;
} ajm_mp3_t;

void ajm_mp3_init(ajm_mp3_t* d, u32 format, u32 flags);
void ajm_mp3_reset(ajm_mp3_t* d);
void ajm_mp3_get_info(const ajm_mp3_t* d, void* out_info);
AjmSidebandFormat ajm_mp3_get_format(const ajm_mp3_t* d);
u32 ajm_mp3_get_minimum_input_size(const ajm_mp3_t* d);
u32 ajm_mp3_get_next_frame_size(const ajm_mp3_t* d, const ajm_gapless_t* gapless);
ajm_decoder_result_t ajm_mp3_process_data(ajm_mp3_t* d, ajm_input_t* input, ajm_output_t* output,
                                          ajm_gapless_t* gapless);
/* The header of the frame at @buf: any layer, or layer III only */
int ajm_mp3_parse_header(const u8* buf, u32 stream_size, int parse_ofl, AjmDecMp3ParseFrame* frame);
int ajm_mp3_parse_frame(const u8* buf, u32 stream_size, int parse_ofl, AjmDecMp3ParseFrame* frame);

RETRO_END_DECLS

#endif
