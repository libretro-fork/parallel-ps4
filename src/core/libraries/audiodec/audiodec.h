/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef SHADPS4_LIBRARIES_AUDIODEC_H
#define SHADPS4_LIBRARIES_AUDIODEC_H

/* libSceAudiodec: a game hands over one access unit of ATRAC9, MP3 or AAC at
 * a time and gets its PCM back, 16-bit or float. The decoders are
 * libretro-common's.
 *
 * A decoder is one of 64 slots, taken and given back with a compare-and-swap;
 * a game decodes with a handle from one thread at a time, as the PS4's
 * library asks of it. What a decoder needs is allocated when it is made, not
 * while it decodes (an ADTS stream's decoder when its first header says what
 * it is).
 *
 * These are the functions a game calls, with its calling convention; they
 * are registered for the game in sysmodule_internal.cpp. */

#include <stdint.h>

#include <retro_common_api.h>

#if defined(__GNUC__) && (defined(__x86_64__) || defined(_M_X64))
#define AUDIODEC_GUEST_ABI __attribute__((sysv_abi))
#else
#define AUDIODEC_GUEST_ABI
#endif

RETRO_BEGIN_DECLS

#define AUDIODEC_TYPE_AT9 1u
#define AUDIODEC_TYPE_MP3 2u
#define AUDIODEC_TYPE_M4AAC 3u

#define AUDIODEC_WORD_16BIT 1
#define AUDIODEC_WORD_FLOAT 2

#define AUDIODEC_M4AAC_ADTS 1u
#define AUDIODEC_M4AAC_RAW 2u

#define AUDIODEC_ERROR_API_FAIL ((int32_t)0x807F0000)
#define AUDIODEC_ERROR_INVALID_TYPE ((int32_t)0x807F0001)
#define AUDIODEC_ERROR_INVALID_PARAM_SIZE ((int32_t)0x807F0004)
#define AUDIODEC_ERROR_INVALID_BSI_INFO_SIZE ((int32_t)0x807F0005)
#define AUDIODEC_ERROR_INVALID_AU_INFO_SIZE ((int32_t)0x807F0006)
#define AUDIODEC_ERROR_INVALID_PCM_ITEM_SIZE ((int32_t)0x807F0007)
#define AUDIODEC_ERROR_INVALID_CTRL_POINTER ((int32_t)0x807F0008)
#define AUDIODEC_ERROR_INVALID_PARAM_POINTER ((int32_t)0x807F0009)
#define AUDIODEC_ERROR_INVALID_BSI_INFO_POINTER ((int32_t)0x807F000A)
#define AUDIODEC_ERROR_INVALID_AU_INFO_POINTER ((int32_t)0x807F000B)
#define AUDIODEC_ERROR_INVALID_PCM_ITEM_POINTER ((int32_t)0x807F000C)
#define AUDIODEC_ERROR_INVALID_AU_POINTER ((int32_t)0x807F000D)
#define AUDIODEC_ERROR_INVALID_PCM_POINTER ((int32_t)0x807F000E)
#define AUDIODEC_ERROR_INVALID_HANDLE ((int32_t)0x807F000F)
#define AUDIODEC_ERROR_INVALID_WORD_LENGTH ((int32_t)0x807F0010)
#define AUDIODEC_ERROR_INVALID_AU_SIZE ((int32_t)0x807F0011)
#define AUDIODEC_ERROR_INVALID_PCM_SIZE ((int32_t)0x807F0012)
#define AUDIODEC_ERROR_M4AAC_INVALID_SAMPLING_FREQ ((int32_t)0x807F0300)
#define AUDIODEC_ERROR_M4AAC_INVALID_CONFIG_NUMBER ((int32_t)0x807F0303)
#define AUDIODEC_ERROR_M4AAC_INVALID_MAX_CHANNELS ((int32_t)0x807F0304)
#define AUDIODEC_ERROR_AT9_INVALID_CONFIG_DATA ((int32_t)0x807F1000)

/* The PS4's structures */

typedef struct audiodec_au_info {
    uint32_t size;
    void* au;
    uint32_t au_size;
} audiodec_au_info;

typedef struct audiodec_pcm_item {
    uint32_t size;
    void* pcm;
    uint32_t pcm_size;
} audiodec_pcm_item;

typedef struct audiodec_ctrl {
    void* param;
    void* bsi_info;
    audiodec_au_info* au_info;
    audiodec_pcm_item* pcm_item;
} audiodec_ctrl;

typedef struct audiodec_param_at9 {
    uint32_t size;
    int32_t word;
    uint8_t config[4];
} audiodec_param_at9;

typedef struct audiodec_at9_info {
    uint32_t size;
    uint32_t channels;
    uint32_t bitrate;
    uint32_t sampling_rate;
    uint32_t super_frame_size;
    uint32_t frames_in_super_frame;
    uint32_t next_frame_size;
    uint32_t frame_samples;
    int32_t result;
} audiodec_at9_info;

typedef struct audiodec_param_mp3 {
    uint32_t size;
    int32_t word;
} audiodec_param_mp3;

typedef struct audiodec_mp3_info {
    uint32_t size;
    uint32_t header;
    uint8_t crc;
    uint8_t mode;
    uint8_t mode_extension;
    uint8_t copyright;
    uint8_t original;
    uint8_t emphasis;
    uint8_t reserved[2];
    int32_t result;
} audiodec_mp3_info;

typedef struct audiodec_param_m4aac {
    uint32_t size;
    int32_t word;
    uint32_t config_number;
    uint32_t sampling_freq_index;
    uint32_t max_channels;
    uint32_t enable_heaac;
} audiodec_param_m4aac;

typedef struct audiodec_m4aac_info {
    uint32_t size;
    uint32_t sampling_freq;
    uint32_t channels;
    uint32_t heaac;
    int32_t result;
} audiodec_m4aac_info;

int32_t AUDIODEC_GUEST_ABI sceAudiodecInitLibrary(uint32_t codec_type);
int32_t AUDIODEC_GUEST_ABI sceAudiodecTermLibrary(uint32_t codec_type);
/* The decoder's handle, 1 or more, or an error */
int32_t AUDIODEC_GUEST_ABI sceAudiodecCreateDecoder(audiodec_ctrl* ctrl, uint32_t codec_type);
int32_t AUDIODEC_GUEST_ABI sceAudiodecDeleteDecoder(int32_t handle);
int32_t AUDIODEC_GUEST_ABI sceAudiodecClearContext(int32_t handle);
/* On return the sizes in the access unit's and the PCM's descriptions are the
 * bytes read and written */
int32_t AUDIODEC_GUEST_ABI sceAudiodecDecode(int32_t handle, audiodec_ctrl* ctrl);

RETRO_END_DECLS

#endif
