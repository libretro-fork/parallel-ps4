/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef SHADPS4_LIBRARIES_AUDIOOUT_C_H
#define SHADPS4_LIBRARIES_AUDIOOUT_C_H

/* AudioOut's calls the libraries written in C make. Defined in audioout.c. */

#include <retro_common_api.h>

#include "common/types.h"

RETRO_BEGIN_DECLS

/* OrbisAudioOutParamFormat: an OrbisAudioOutParamExtendedInformation's bits 0-7 */
#define AUDIOOUT_FORMAT_S16_MONO 0
#define AUDIOOUT_FORMAT_S16_STEREO 1
#define AUDIOOUT_FORMAT_S16_8CH 2
#define AUDIOOUT_FORMAT_FLOAT_MONO 3
#define AUDIOOUT_FORMAT_FLOAT_STEREO 4
#define AUDIOOUT_FORMAT_FLOAT_8CH 5
#define AUDIOOUT_FORMAT_S16_8CH_STD 6
#define AUDIOOUT_FORMAT_FLOAT_8CH_STD 7
#define AUDIOOUT_FORMAT_MASK 0xFFu

/* OrbisAudioOutPort::Audio3d */
#define AUDIOOUT_PORT_AUDIO3D 126

/* ORBIS_AUDIO_OUT_ERROR_ALREADY_INIT */
#define AUDIOOUT_ERROR_ALREADY_INIT ((s32)0x8026000E)

/* OrbisAudioOutOutputParam */
typedef struct audioout_output_param {
    s32 handle;
    void* ptr;
} audioout_output_param_t;

/* sceAudioOutInit, sceAudioOutOpen (@param the OrbisAudioOutParamExtendedInformation's bits),
 * sceAudioOutOutput and sceAudioOutClose */
s32 audioout_init(void);
s32 audioout_open(s32 user_id, s32 port_type, s32 index, u32 length, u32 sample_rate, u32 param);
s32 audioout_output(s32 handle, void* ptr);
s32 audioout_close(s32 handle);

RETRO_END_DECLS

#endif
