/* SPDX-FileCopyrightText: Copyright 2024-2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef SHADPS4_AUDIO_AUDIOOUT_BACKEND_H
#define SHADPS4_AUDIO_AUDIOOUT_BACKEND_H

/* Where an AudioOut port's sound goes. The libretro core's ports go to the frontend through
 * the core's mix (frontend_audio_out.c). */

#include <boolean.h>
#include <retro_atomic.h>
#include <retro_common_api.h>

#include "common/types.h"

RETRO_BEGIN_DECLS

struct audio_mix_input;

typedef struct AudioFormatInfo {
    bool is_float;
    u8 sample_size;
    u8 num_channels;
    /* Layout array remapping channel indices, specified in this order:
     * FL, FR, FC, LFE, BL, BR, SL, SR */
    int channel_layout[8];
    bool is_std;
} AudioFormatInfo;

/* A port's way to the frontend: kept in the port, nothing allocated */
typedef struct audioout_port_backend {
    u32 buffer_frames;
    u32 num_channels;
    bool is_float;
    /* Eight channels kept as 7.1: the mix is surround. The port's channels in the mix's order:
     * channel c of the mix is the port's order[c] (the console has two 7.1 orders) */
    bool wide;
    int order[8];
    struct audio_mix_input* input;
    /* Wide: each of the port's channels' volume as the game set it (its order), a float's
     * bits */
    retro_atomic_int_t channel_gain[8];
    /* The stereo volume: the port's loudest channel's, as the game set it, a float's bits */
    retro_atomic_int_t game_gain;
    /* The gains the last buffer ended at (the writers') */
    float applied[8];
} audioout_port_backend_t;

/* Opens @b for a port of @type, @format and @buffer_frames: a port the mix has no input left
 * for, or of a channel count it cannot take, is silent */
void frontend_audio_out_open(audioout_port_backend_t* b, s32 type, const AudioFormatInfo* format,
                             u32 buffer_frames);
void frontend_audio_out_close(audioout_port_backend_t* b);
/* Called in intervals of at least the port's buffer time, with a buffer of the port's size */
void frontend_audio_out_output(audioout_port_backend_t* b, const void* ptr);
/* @ch_volumes: the port's eight channels' */
void frontend_audio_out_set_volume(audioout_port_backend_t* b, const s32* ch_volumes);

RETRO_END_DECLS

#endif
