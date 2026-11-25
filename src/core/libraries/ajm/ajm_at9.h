/* SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef SHADPS4_AJM_AJM_AT9_H
#define SHADPS4_AJM_AJM_AT9_H

/* The ATRAC9 codec, on libretro-common's decoder. Defined in ajm_at9.c. */

#include <boolean.h>
#include <retro_common_api.h>

#include "common/types.h"
#include "core/libraries/ajm/ajm_codec.h"

RETRO_BEGIN_DECLS

struct ratrac9;

/* AjmAt9CodecFlags */
#define AJM_AT9_PARSE_RIFF_HEADER 1u
#define AJM_AT9_NON_INTERLEAVED_OUTPUT (1u << 8)

/* A frame's PCM at the most: 256 samples a channel, 8 channels, 4 bytes a sample */
#define AJM_AT9_MAX_PCM_BYTES (256 * 8 * 4)

typedef struct AjmSidebandDecAt9CodecInfo {
    u32 super_frame_size;
    u32 frames_in_super_frame;
    u32 next_frame_size;
    u32 frame_samples;
} AjmSidebandDecAt9CodecInfo;

typedef struct ajm_at9 {
    u32 format; /* AjmFormatEncoding */
    u32 flags;  /* AjmAt9CodecFlags */
    bool is_initialized;
    struct ratrac9* decoder;
    u8 config_data[ORBIS_AT9_CONFIG_DATA_SIZE];
    u32 superframe_bytes_remain;
    u32 num_frames;
    u32 channels;
    u32 sample_rate;
    u32 frame_samples;
    u32 superframe_bytes;
    u32 frames_in_superframe;
    /* The frame's PCM: pcm_bytes of it */
    size_t pcm_bytes;
    union {
        u8 bytes[AJM_AT9_MAX_PCM_BYTES];
        float align;
    } pcm;
} ajm_at9_t;

void ajm_at9_init(ajm_at9_t* d, u32 format, u32 flags);
void ajm_at9_free(ajm_at9_t* d);
void ajm_at9_reset(ajm_at9_t* d);
void ajm_at9_initialize(ajm_at9_t* d, const void* buffer, u32 buffer_size);
void ajm_at9_get_info(const ajm_at9_t* d, void* out_info);
AjmSidebandFormat ajm_at9_get_format(const ajm_at9_t* d);
u32 ajm_at9_get_minimum_input_size(const ajm_at9_t* d);
u32 ajm_at9_get_next_frame_size(const ajm_at9_t* d, const ajm_gapless_t* gapless);
ajm_decoder_result_t ajm_at9_process_data(ajm_at9_t* d, ajm_input_t* input, ajm_output_t* output,
                                          ajm_gapless_t* gapless);

RETRO_END_DECLS

#endif
