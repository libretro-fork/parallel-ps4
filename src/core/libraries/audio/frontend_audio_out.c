/* SPDX-FileCopyrightText: Copyright 2024-2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <retro_atomic.h>
#include <string.h>

#include "common/c_bridge.h"
#include "core/emulator_settings.h"
#include "core/libraries/audio/audioout_backend.h"
#include "core/libraries/kernel/kernel_c.h"
#include "libretro/audio_mix.h"

#define INV_VOLUME_0DB (1.0f / 32768.0f)

/* The libretro core plays a port by handing it to the frontend: what the game writes is
 * converted, with the port's volume, straight into its input of the core's mix
 * (libretro/audio_mix.h): to stereo, or a port of eight channels as 7.1 where the mix is
 * surround (the frontend takes it), in the frontend's order. The call is paced a buffer's time
 * at a time on the game's time, as the device paces it (audioout.c, PaceOutput); the mix takes
 * one frame of every port's sound (AUDIO_MIX_FRAME_FRAMES) each retro_run, and a port waits
 * besides while more than that is queued, on the game's time (kernel_game_wait): no buffer kept
 * ahead, no timeout. */

static int float_bits(float f) {
    int i;
    memcpy(&i, &f, sizeof(i));
    return i;
}

static float bits_float(int i) {
    float f;
    memcpy(&f, &i, sizeof(f));
    return f;
}

void frontend_audio_out_open(audioout_port_backend_t* b, s32 type, const AudioFormatInfo* format,
                             u32 buffer_frames) {
    int c;
    float slider;
    b->buffer_frames = buffer_frames;
    b->num_channels = format->num_channels;
    b->is_float = format->is_float;
    b->wide = format->num_channels == 8 && audio_mix_channels() == 8;
    memcpy(b->order, format->channel_layout, sizeof(b->order));
    b->input = b->wide ? audio_mix_input_open_channels(8) : audio_mix_input_open();
    retro_atomic_store_relaxed_int(&b->game_gain, float_bits(1.0f));
    for (c = 0; c < 8; c++) {
        retro_atomic_store_relaxed_int(&b->channel_gain[c], float_bits(1.0f));
    }
    slider = emu_settings_get_volume_slider() * 0.01f;
    for (c = 0; c < 8; c++) {
        b->applied[c] = slider;
    }
    if (!b->input) {
        c_log(C_LOG_LIB_AUDIO_OUT, C_LOG_LEVEL_ERROR, "FrontendPortBackend",
              "No input left in the core's mix: port type %d is silent", (int)type);
        return;
    }
    if (b->num_channels != 1 && b->num_channels != 2 && b->num_channels != 8) {
        c_log(C_LOG_LIB_AUDIO_OUT, C_LOG_LEVEL_ERROR, "FrontendPortBackend",
              "Unsupported channel count %u: port is silent", b->num_channels);
        audio_mix_input_close(b->input);
        b->input = NULL;
        return;
    }
    c_log(C_LOG_LIB_AUDIO_OUT, C_LOG_LEVEL_INFO, "FrontendPortBackend",
          "Opened audio port type %d for the frontend (%u ch%s)", (int)type, b->num_channels,
          b->wide ? ", as 7.1" : "");
}

void frontend_audio_out_close(audioout_port_backend_t* b) {
    audio_mix_input_close(b->input);
    b->input = NULL;
}

static int frontend_audio_out_room(void* ctx) {
    audioout_port_backend_t* b = (audioout_port_backend_t*)ctx;
    return audio_mix_input_ready(b->input, b->buffer_frames, AUDIO_MIX_FRAME_FRAMES * 2) != 0;
}

void frontend_audio_out_output(audioout_port_backend_t* b, const void* ptr) {
    static const float unit[8] = {1, 1, 1, 1, 1, 1, 1, 1};
    float target[8];
    const float* gains;
    const u8* src = (const u8*)ptr;
    float slider;
    u32 width;
    u32 sample_bytes;
    u32 done = 0;
    u32 c;
    bool ramp = false;
    if (!b->input || !ptr) {
        return;
    }
    /* The call is paced by the game's time (PaceOutput), which keeps a frame and a buffer
     * queued at most. This bound is only for a frontend that stopped taking frames: set
     * past what the pace keeps, so that it never holds a paced port back (a port held, then
     * let go, outputs its late buffers at once: FMOD's output overtakes its mixer) */
    kernel_game_wait(audio_mix_input_room_ec(b->input), 0, frontend_audio_out_room, b);
    /* The gains this buffer goes to: the game's port volume times the Volume option's */
    slider = emu_settings_get_volume_slider() * 0.01f;
    if (b->wide) {
        for (c = 0; c < 8; c++) {
            target[c] =
                bits_float(retro_atomic_load_relaxed_int(&b->channel_gain[b->order[c] & 7])) *
                slider;
        }
    } else {
        const float g = bits_float(retro_atomic_load_relaxed_int(&b->game_gain)) * slider;
        for (c = 0; c < 8; c++) {
            target[c] = g;
        }
    }
    width = b->wide ? 8 : 2;
    /* A volume that changed is ramped across this buffer from the last one's, as the
     * device does: stepped, it clicks (a fade, a duck) */
    for (c = 0; c < width; c++) {
        if (!(target[c] == b->applied[c])) {
            ramp = true;
            break;
        }
    }
    gains = ramp ? unit : target;
    sample_bytes = b->is_float ? 4 : 2;
    while (done < b->buffer_frames) {
        float* dst = NULL;
        const void* at;
        const u32 got = audio_mix_input_reserve(b->input, &dst, b->buffer_frames - done);
        if (got == 0) {
            break;
        }
        at = src + (size_t)done * b->num_channels * sample_bytes;
        if (b->wide) {
            if (b->is_float) {
                audio_mix_convert_f32_wide(dst, (const float*)at, got, b->order, gains);
            } else {
                audio_mix_convert_s16_wide(dst, (const s16*)at, got, b->order, gains);
            }
        } else if (b->is_float) {
            audio_mix_convert_f32(dst, (const float*)at, got, b->num_channels, gains[0]);
        } else {
            audio_mix_convert_s16(dst, (const s16*)at, got, b->num_channels, gains[0]);
        }
        if (ramp) {
            audio_mix_ramp(dst, got, width, done, b->buffer_frames, b->applied, target);
        }
        audio_mix_input_commit(b->input, got);
        done += got;
    }
    memcpy(b->applied, target, sizeof(b->applied));
}

void frontend_audio_out_set_volume(audioout_port_backend_t* b, const s32* ch_volumes) {
    float max_channel_gain = 0.0f;
    const u32 channels = b->num_channels < 8u ? b->num_channels : 8u;
    u32 i;
    for (i = 0; i < channels; i++) {
        const float g = (float)ch_volumes[i] * INV_VOLUME_0DB;
        if (max_channel_gain < g) {
            max_channel_gain = g;
        }
    }
    retro_atomic_store_relaxed_int(&b->game_gain, float_bits(max_channel_gain));
    for (i = 0; i < channels; i++) {
        retro_atomic_store_relaxed_int(&b->channel_gain[i],
                                       float_bits((float)ch_volumes[i] * INV_VOLUME_0DB));
    }
}
