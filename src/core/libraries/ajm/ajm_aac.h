/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef SHADPS4_AJM_AJM_AAC_H
#define SHADPS4_AJM_AJM_AAC_H

/* The AAC codec, on libretro-common's decoder. Defined in ajm_aac.c. */

#include <stddef.h>

#include <boolean.h>
#include <retro_common_api.h>

#include "common/types.h"
#include "core/libraries/ajm/ajm_codec.h"

RETRO_BEGIN_DECLS

struct raac;

/* ConfigType */
#define AJM_AAC_CONFIG_ADTS 1u
#define AJM_AAC_CONFIG_RAW 2u

/* AjmAacCodecFlags */
#define AJM_AAC_ENABLE_SBR_DECODE 1u
#define AJM_AAC_ENABLE_NONDELAY_OUTPUT 2u
#define AJM_AAC_SURROUND_EXTL_EXTR_LS_RS 4u
#define AJM_AAC_SURROUND_LS_RS_EXTL_EXTR 8u

typedef struct AjmSidebandDecM4aacCodecInfo {
    u32 heaac;
    u32 reserved;
} AjmSidebandDecM4aacCodecInfo;

typedef struct ajm_aac {
    u32 format; /* AjmFormatEncoding */
    u32 flags;  /* AjmAacCodecFlags */
    u32 channels;
    u32 skip_frames;
    u32 last_frame_bytes;
    u32 config_type;        /* ConfigType */
    u32 sampling_freq_type; /* the initialize parameters */
    struct raac* decoder;
    u8 asc[32];
    size_t asc_size;
} ajm_aac_t;

void ajm_aac_init(ajm_aac_t* d, u32 format, u32 flags, u32 channels);
void ajm_aac_free(ajm_aac_t* d);
void ajm_aac_reset(ajm_aac_t* d);
void ajm_aac_initialize(ajm_aac_t* d, const void* buffer, u32 buffer_size);
void ajm_aac_get_info(const ajm_aac_t* d, void* out_info);
AjmSidebandFormat ajm_aac_get_format(const ajm_aac_t* d);
u32 ajm_aac_get_minimum_input_size(const ajm_aac_t* d);
u32 ajm_aac_get_next_frame_size(const ajm_aac_t* d, const ajm_gapless_t* gapless);
ajm_decoder_result_t ajm_aac_process_data(ajm_aac_t* d, ajm_input_t* input, ajm_output_t* output,
                                          ajm_gapless_t* gapless);

RETRO_END_DECLS

#endif
