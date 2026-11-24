/* SPDX-FileCopyrightText: Copyright 2025-2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <stddef.h>
#include <string.h>

#include <boolean.h>
#include <retro_atomic.h>

#include "common/c_bridge.h"
#include "common/enum_names.h"
#include "core/libraries/audio/audioout_c.h"
#include "core/libraries/audio3d/audio3d.h"
#include "core/libraries/audio3d/audio3d_error.h"
#include "core/libraries/error_codes.h"
#include "core/libraries/kernel/kernel_c.h"
#include "libretro/audio_mix.h"

#define AUDIO3D_SAMPLE_RATE 48000

enum { PORT_FREE = 0, PORT_OPENING, PORT_OPEN, PORT_CLOSING };
enum { ASSOCIATED_FREE = 0, ASSOCIATED_OPENING, ASSOCIATED_LIVE };

/* The ports, made at sceAudio3dInitialize: g_state points at them until sceAudio3dTerminate */
static audio3d_lib_port_t g_ports[AUDIO3D_MAX_PORTS];
static audio3d_lib_port_t* g_state;

/* The port the mix is played through, which the game never sees: float, so that the mix
 * reaches the core's own mix whole. 7.1 where the core's mix is surround (the frontend takes
 * it): a bed of 6 or 8 channels then goes through unfolded. Stereo otherwise. */
static u32 output_channels(void) {
    return audio_mix_channels() == 8 ? 8 : 2;
}
static u32 output_format(void) {
    return output_channels() == 8 ? AUDIOOUT_FORMAT_FLOAT_8CH : AUDIOOUT_FORMAT_FLOAT_STEREO;
}

/* A call's hold on an open port: the port is not closed under it until port_unpin. NULL if
 * the id names no open port. */
static audio3d_lib_port_t* port_pin(OrbisAudio3dPortId port_id) {
    audio3d_lib_port_t* candidate;
    if (!g_state || port_id >= AUDIO3D_MAX_PORTS) {
        return NULL;
    }
    candidate = &g_state[port_id];
    if (!pin_gate_enter(&candidate->gate)) {
        return NULL;
    }
    if (retro_atomic_load_acquire_int(&candidate->state) != PORT_OPEN) {
        pin_gate_leave(&candidate->gate);
        return NULL;
    }
    return candidate;
}

static void port_unpin(audio3d_lib_port_t* port) {
    if (port) {
        pin_gate_leave(&port->gate);
    }
}

/* An AudioOut format's channels and bytes a sample */
static void audioout_buffer_info(u32 format, u32* channels, u32* sample_size) {
    switch (format) {
    case AUDIOOUT_FORMAT_S16_MONO:
        *channels = 1;
        *sample_size = sizeof(s16);
        return;
    case AUDIOOUT_FORMAT_S16_STEREO:
        *channels = 2;
        *sample_size = sizeof(s16);
        return;
    case AUDIOOUT_FORMAT_S16_8CH:
    case AUDIOOUT_FORMAT_S16_8CH_STD:
        *channels = 8;
        *sample_size = sizeof(s16);
        return;
    case AUDIOOUT_FORMAT_FLOAT_MONO:
        *channels = 1;
        *sample_size = sizeof(float);
        return;
    case AUDIOOUT_FORMAT_FLOAT_STEREO:
        *channels = 2;
        *sample_size = sizeof(float);
        return;
    case AUDIOOUT_FORMAT_FLOAT_8CH:
    case AUDIOOUT_FORMAT_FLOAT_8CH_STD:
        *channels = 8;
        *sample_size = sizeof(float);
        return;
    default:
        *channels = 2;
        *sample_size = sizeof(s16);
        return;
    }
}

/* What the game output to its own AudioOut ports of this port, sent on: straight from where it
 * waits, no copy */
static s32 drain_associated_ports(audio3d_lib_port_t* port) {
    unsigned i;
    for (i = 0; i < AUDIO3D_MAX_ASSOCIATED_PORTS; i++) {
        audio3d_associated_port_t* aout = &port->audioout_ports[i];
        s32 handle;
        if (retro_atomic_load_acquire_int(&aout->state) != ASSOCIATED_LIVE) {
            continue;
        }
        handle = aout->handle;
        for (;;) {
            s32 ret;
            const unsigned char* buffer = audio3d_ring_take_begin(&aout->pending, NULL);
            if (!buffer) {
                break;
            }
            ret = audioout_output(handle, (void*)buffer);
            audio3d_ring_take_end(&aout->pending);
            if (ret < 0) {
                return ret;
            }
        }
    }
    return ORBIS_OK;
}

/* The AudioOut port the mix goes out through, opened by the first thread to need it */
static s32 port_output_handle(audio3d_lib_port_t* port) {
    s32 opened;
    const s32 handle = retro_atomic_load_acquire_int(&port->audio_out_handle);
    if (handle >= 0) {
        return handle;
    }
    opened = audioout_open(0xFF, AUDIOOUT_PORT_AUDIO3D, 0, port->parameters.granularity,
                           AUDIO3D_SAMPLE_RATE, output_format());
    if (opened < 0) {
        return opened;
    }
    if (!retro_atomic_cas_int(&port->audio_out_handle, -1, opened)) {
        audioout_close(opened);
        return retro_atomic_load_acquire_int(&port->audio_out_handle);
    }
    return opened;
}

/* The oldest mixed block out: *@sent true if one was sent, false if there was none or another
 * thread is sending one */
static s32 submit_mixed(audio3d_lib_port_t* port, s32 handle, bool* sent) {
    s32 ret;
    const unsigned char* block = audio3d_ring_take_begin(&port->core.mixed, NULL);
    if (!block) {
        *sent = false;
        return ORBIS_OK;
    }
    ret = audioout_output(handle, (void*)block);
    audio3d_ring_take_end(&port->core.mixed);
    *sent = true;
    return ret < 0 ? ret : ORBIS_OK;
}

s32 PS4_SYSV_ABI sceAudio3dAudioOutClose(s32 handle) {
    bool found = false;
    OrbisAudio3dPortId id;
    c_log(C_LOG_LIB_AUDIO3D, C_LOG_LEVEL_INFO, "sceAudio3dAudioOutClose", "called, handle = %d",
          handle);

    for (id = 0; id < AUDIO3D_MAX_PORTS && !found; id++) {
        unsigned i;
        audio3d_lib_port_t* port = port_pin(id);
        if (!port) {
            continue;
        }
        for (i = 0; i < AUDIO3D_MAX_ASSOCIATED_PORTS; i++) {
            audio3d_associated_port_t* aout = &port->audioout_ports[i];
            /* Its ring stays until the entry is opened again or the port closes: a push may
             * still be sending from it */
            if (retro_atomic_load_acquire_int(&aout->state) == ASSOCIATED_LIVE &&
                aout->handle == handle &&
                retro_atomic_cas_int(&aout->state, ASSOCIATED_LIVE, ASSOCIATED_FREE)) {
                found = true;
                break;
            }
        }
        port_unpin(port);
    }
    if (!found) {
        c_log(C_LOG_LIB_AUDIO3D, C_LOG_LEVEL_ERROR, "sceAudio3dAudioOutClose",
              "handle %d was not opened via sceAudio3dAudioOutOpen", handle);
        return ORBIS_AUDIO3D_ERROR_INVALID_PORT;
    }

    return audioout_close(handle);
}

/* A ring of an entry closed earlier, had for good if no thread is still putting into it or
 * taking from it (they are turned away from here): then it may be made anew */
static bool claim_ring(audio3d_ring_t* ring) {
    if (!ring->data) {
        return true;
    }
    if (!retro_atomic_cas_int(&ring->taking, 0, 1)) {
        return false;
    }
    if (!retro_atomic_cas_int(&ring->putting, 0, 1)) {
        retro_atomic_store_release_int(&ring->taking, 0);
        return false;
    }
    return true;
}

s32 PS4_SYSV_ABI sceAudio3dAudioOutOpen(OrbisAudio3dPortId port_id, s32 user_id, s32 type,
                                        s32 index, u32 len, u32 freq, u32 param) {
    audio3d_lib_port_t* port;
    audio3d_associated_port_t* entry = NULL;
    unsigned i;
    s32 handle;
    u32 channels;
    u32 sample_size;
    u32 buffer_bytes;
    c_log(C_LOG_LIB_AUDIO3D, C_LOG_LEVEL_INFO, "sceAudio3dAudioOutOpen",
          "called, port_id = %u, user_id = %d, type = %d, index = %d, len = %u, freq = %u", port_id,
          user_id, type, index, len, freq);

    port = port_pin(port_id);
    if (!port) {
        c_log(C_LOG_LIB_AUDIO3D, C_LOG_LEVEL_ERROR, "sceAudio3dAudioOutOpen",
              "!state->ports.contains(port_id)");
        return ORBIS_AUDIO3D_ERROR_INVALID_PORT;
    }

    if (len != port->parameters.granularity) {
        c_log(C_LOG_LIB_AUDIO3D, C_LOG_LEVEL_ERROR, "sceAudio3dAudioOutOpen",
              "len != state->ports[port_id].parameters.granularity");
        port_unpin(port);
        return ORBIS_AUDIO3D_ERROR_INVALID_PARAMETER;
    }

    /* A free entry whose ring no thread is still in: had for good by this thread */
    for (i = 0; i < AUDIO3D_MAX_ASSOCIATED_PORTS; i++) {
        audio3d_associated_port_t* aout = &port->audioout_ports[i];
        if (!retro_atomic_cas_int(&aout->state, ASSOCIATED_FREE, ASSOCIATED_OPENING)) {
            continue;
        }
        if (claim_ring(&aout->pending)) {
            entry = aout;
            break;
        }
        retro_atomic_store_release_int(&aout->state, ASSOCIATED_FREE);
    }
    if (!entry) {
        c_log(C_LOG_LIB_AUDIO3D, C_LOG_LEVEL_ERROR, "sceAudio3dAudioOutOpen",
              "no free AudioOut port on port %u", port_id);
        port_unpin(port);
        return ORBIS_AUDIO3D_ERROR_OUT_OF_RESOURCES;
    }

    handle = audioout_open(user_id, type, index, len, freq, param);
    if (handle < 0) {
        if (entry->pending.data) {
            retro_atomic_store_release_int(&entry->pending.putting, 0);
            retro_atomic_store_release_int(&entry->pending.taking, 0);
        }
        retro_atomic_store_release_int(&entry->state, ASSOCIATED_FREE);
        port_unpin(port);
        return handle;
    }

    audioout_buffer_info(param & AUDIOOUT_FORMAT_MASK, &channels, &sample_size);
    buffer_bytes = len * channels * sample_size;
    /* Its queue, made now rather than for each buffer the game sends */
    audio3d_ring_free(&entry->pending);
    if (!audio3d_ring_init(&entry->pending,
                           port->parameters.queue_depth > 1 ? port->parameters.queue_depth : 1,
                           buffer_bytes)) {
        audioout_close(handle);
        retro_atomic_store_release_int(&entry->state, ASSOCIATED_FREE);
        port_unpin(port);
        return ORBIS_AUDIO3D_ERROR_OUT_OF_MEMORY;
    }
    entry->handle = handle;
    entry->samples_per_buffer = len * channels;
    entry->is_float = sample_size == sizeof(float);
    retro_atomic_store_release_int(&entry->state, ASSOCIATED_LIVE);
    port_unpin(port);
    return handle;
}

s32 PS4_SYSV_ABI sceAudio3dAudioOutOutput(s32 handle, void* ptr) {
    OrbisAudio3dPortId id;
    c_log(C_LOG_LIB_AUDIO3D, C_LOG_LEVEL_DEBUG, "sceAudio3dAudioOutOutput",
          "called, handle = %d, ptr = 0x%llx", handle, (unsigned long long)(uintptr_t)ptr);

    if (!ptr) {
        c_log(C_LOG_LIB_AUDIO3D, C_LOG_LEVEL_ERROR, "sceAudio3dAudioOutOutput", "!ptr");
        return ORBIS_AUDIO3D_ERROR_INVALID_PARAMETER;
    }

    if (handle < 0 || (handle & 0xFFFF) > 25) {
        c_log(C_LOG_LIB_AUDIO3D, C_LOG_LEVEL_ERROR, "sceAudio3dAudioOutOutput",
              "handle < 0 || (handle & 0xFFFF) > 25");
        return ORBIS_AUDIO3D_ERROR_INVALID_PORT;
    }

    for (id = 0; id < AUDIO3D_MAX_PORTS; id++) {
        unsigned i;
        audio3d_lib_port_t* port = port_pin(id);
        if (!port) {
            continue;
        }
        for (i = 0; i < AUDIO3D_MAX_ASSOCIATED_PORTS; i++) {
            audio3d_associated_port_t* aout = &port->audioout_ports[i];
            uintptr_t align_mask;
            unsigned char* slot;
            s32 sent;
            if (retro_atomic_load_acquire_int(&aout->state) != ASSOCIATED_LIVE ||
                aout->handle != handle) {
                continue;
            }

            align_mask = aout->is_float ? 3u : 1u;
            if (((uintptr_t)ptr & align_mask) != 0) {
                c_log(C_LOG_LIB_AUDIO3D, C_LOG_LEVEL_ERROR, "sceAudio3dAudioOutOutput",
                      "sample buffer for handle %d is misaligned", handle);
                port_unpin(port);
                return ORBIS_AUDIO3D_ERROR_INVALID_PARAMETER;
            }

            slot = audio3d_ring_put_begin(&aout->pending);
            if (!slot) {
                port_unpin(port);
                return ORBIS_AUDIO3D_ERROR_NOT_READY;
            }
            memcpy(slot, ptr, aout->pending.block_bytes);
            audio3d_ring_put_end(&aout->pending, aout->samples_per_buffer, 1, AUDIO3D_S16);

            /* Mirror sceAudioOutOutput's return of samples sent. */
            sent = (s32)aout->samples_per_buffer;
            port_unpin(port);
            return sent;
        }
        port_unpin(port);
    }

    c_log(C_LOG_LIB_AUDIO3D, C_LOG_LEVEL_ERROR, "sceAudio3dAudioOutOutput",
          "handle %d was not opened via sceAudio3dAudioOutOpen", handle);
    return ORBIS_AUDIO3D_ERROR_INVALID_PORT;
}

s32 PS4_SYSV_ABI sceAudio3dAudioOutOutputs(audioout_output_param_t* param, u32 num) {
    u32 i;
    c_log(C_LOG_LIB_AUDIO3D, C_LOG_LEVEL_DEBUG, "sceAudio3dAudioOutOutputs",
          "called, param = 0x%llx, num = %u", (unsigned long long)(uintptr_t)param, num);

    if (!param || !num) {
        c_log(C_LOG_LIB_AUDIO3D, C_LOG_LEVEL_ERROR, "sceAudio3dAudioOutOutputs", "!param || !num");
        return ORBIS_AUDIO3D_ERROR_INVALID_PARAMETER;
    }

    for (i = 0; i < num; i++) {
        const s32 ret = sceAudio3dAudioOutOutput(param[i].handle, param[i].ptr);
        if (ret < 0) {
            return ret;
        }
    }

    return ORBIS_OK;
}

/* The game's sound into the next block of @ring: a block's worth, the rest silent */
static s32 convert_and_enqueue(audio3d_ring_t* ring, const OrbisAudio3dPcm* pcm, u32 num_channels,
                               u32 granularity) {
    u32 bytes_per_sample;
    if (!pcm->sample_buffer || !pcm->num_samples) {
        c_log(C_LOG_LIB_AUDIO3D, C_LOG_LEVEL_ERROR, "ConvertAndEnqueue",
              "!pcm.sample_buffer || !pcm.num_samples");
        return ORBIS_AUDIO3D_ERROR_INVALID_PARAMETER;
    }

    bytes_per_sample = (pcm->format == ORBIS_AUDIO3D_FORMAT_S16) ? sizeof(s16) : sizeof(float);

    if (((uintptr_t)pcm->sample_buffer & (bytes_per_sample - 1)) != 0) {
        c_log(C_LOG_LIB_AUDIO3D, C_LOG_LEVEL_ERROR, "ConvertAndEnqueue",
              "pcm.sample_buffer is misaligned");
        return ORBIS_AUDIO3D_ERROR_INVALID_PARAMETER;
    }

    switch (audio3d_ring_put_pcm(
        ring, pcm->sample_buffer, pcm->num_samples, granularity, num_channels,
        pcm->format == ORBIS_AUDIO3D_FORMAT_S16 ? AUDIO3D_S16 : AUDIO3D_FLOAT)) {
    case 0:
        return ORBIS_OK;
    case AUDIO3D_RING_FULL:
        c_log(C_LOG_LIB_AUDIO3D, C_LOG_LEVEL_ERROR, "ConvertAndEnqueue",
              "queue.size() >= max_entries");
        return ORBIS_AUDIO3D_ERROR_NOT_READY;
    default:
        c_log(C_LOG_LIB_AUDIO3D, C_LOG_LEVEL_ERROR, "ConvertAndEnqueue", "out of memory");
        return ORBIS_AUDIO3D_ERROR_OUT_OF_MEMORY;
    }
}

s32 PS4_SYSV_ABI sceAudio3dBedWrite(OrbisAudio3dPortId port_id, u32 num_channels, u32 format,
                                    void* buffer, u32 num_samples) {
    return sceAudio3dBedWrite2(port_id, num_channels, format, buffer, num_samples,
                               ORBIS_AUDIO3D_OUTPUT_BOTH, false);
}

/* After the port is pinned */
static s32 bed_write(audio3d_lib_port_t* port, u32 num_channels, u32 format, void* buffer,
                     u32 num_samples, u32 output_route) {
    OrbisAudio3dPcm pcm;
    if (output_route > ORBIS_AUDIO3D_OUTPUT_BOTH) {
        c_log(C_LOG_LIB_AUDIO3D, C_LOG_LEVEL_ERROR, "sceAudio3dBedWrite2",
              "output_route > ORBIS_AUDIO3D_OUTPUT_BOTH");
        return ORBIS_AUDIO3D_ERROR_INVALID_PARAMETER;
    }

    if (format > ORBIS_AUDIO3D_FORMAT_FLOAT) {
        c_log(C_LOG_LIB_AUDIO3D, C_LOG_LEVEL_ERROR, "sceAudio3dBedWrite2",
              "format > ORBIS_AUDIO3D_FORMAT_FLOAT");
        return ORBIS_AUDIO3D_ERROR_INVALID_PARAMETER;
    }

    if (num_channels != 2 && num_channels != 6 && num_channels != 8) {
        c_log(C_LOG_LIB_AUDIO3D, C_LOG_LEVEL_ERROR, "sceAudio3dBedWrite2",
              "num_channels must be 2, 6, or 8");
        return ORBIS_AUDIO3D_ERROR_INVALID_PARAMETER;
    }

    if (!buffer || !num_samples) {
        c_log(C_LOG_LIB_AUDIO3D, C_LOG_LEVEL_ERROR, "sceAudio3dBedWrite2",
              "!buffer || !num_samples");
        return ORBIS_AUDIO3D_ERROR_INVALID_PARAMETER;
    }

    if (format == ORBIS_AUDIO3D_FORMAT_FLOAT) {
        if (((uintptr_t)buffer & 3) != 0) {
            c_log(C_LOG_LIB_AUDIO3D, C_LOG_LEVEL_ERROR, "sceAudio3dBedWrite2", "buffer & 3 != 0");
            return ORBIS_AUDIO3D_ERROR_INVALID_PARAMETER;
        }
    } else if (format == ORBIS_AUDIO3D_FORMAT_S16) {
        if (((uintptr_t)buffer & 1) != 0) {
            c_log(C_LOG_LIB_AUDIO3D, C_LOG_LEVEL_ERROR, "sceAudio3dBedWrite2", "buffer & 1 != 0");
            return ORBIS_AUDIO3D_ERROR_INVALID_PARAMETER;
        }
    }

    pcm.format = format;
    pcm.sample_buffer = buffer;
    pcm.num_samples = num_samples;
    return convert_and_enqueue(&port->core.bed, &pcm, num_channels, port->parameters.granularity);
}

s32 PS4_SYSV_ABI sceAudio3dBedWrite2(OrbisAudio3dPortId port_id, u32 num_channels, u32 format,
                                     void* buffer, u32 num_samples, u32 output_route,
                                     bool restricted) {
    audio3d_lib_port_t* port;
    s32 ret;
    c_log(C_LOG_LIB_AUDIO3D, C_LOG_LEVEL_DEBUG, "sceAudio3dBedWrite2",
          "called, port_id = %u, num_channels = %u, format = %s, num_samples = %u, output_route "
          "= %s, restricted = %s",
          port_id, num_channels, enum_name(&enum_names_OrbisAudio3dFormat, (s64)format),
          num_samples, enum_name(&enum_names_OrbisAudio3dOutputRoute, (s64)output_route),
          (restricted) ? "true" : "false");

    port = port_pin(port_id);
    if (!port) {
        c_log(C_LOG_LIB_AUDIO3D, C_LOG_LEVEL_ERROR, "sceAudio3dBedWrite2",
              "!state->ports.contains(port_id)");
        return ORBIS_AUDIO3D_ERROR_INVALID_PORT;
    }
    ret = bed_write(port, num_channels, format, buffer, num_samples, output_route);
    port_unpin(port);
    return ret;
}

s32 PS4_SYSV_ABI sceAudio3dCreateSpeakerArray(void) {
    c_log(C_LOG_LIB_AUDIO3D, C_LOG_LEVEL_ERROR, "sceAudio3dCreateSpeakerArray", "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceAudio3dDeleteSpeakerArray(void) {
    c_log(C_LOG_LIB_AUDIO3D, C_LOG_LEVEL_ERROR, "sceAudio3dDeleteSpeakerArray", "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceAudio3dGetDefaultOpenParameters(OrbisAudio3dOpenParameters* params) {
    c_log(C_LOG_LIB_AUDIO3D, C_LOG_LEVEL_DEBUG, "sceAudio3dGetDefaultOpenParameters", "called");
    if (params) {
        OrbisAudio3dOpenParameters default_params;
        memset(&default_params, 0, sizeof(default_params));
        default_params.size_this = 0x20;
        default_params.granularity = 0x100;
        default_params.rate = ORBIS_AUDIO3D_RATE_48000;
        default_params.max_objects = 512;
        default_params.queue_depth = 2;
        default_params.buffer_mode = ORBIS_AUDIO3D_BUFFER_ADVANCE_AND_PUSH;
        memcpy(params, &default_params, 0x20);
    }
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceAudio3dGetSpeakerArrayMemorySize(void) {
    c_log(C_LOG_LIB_AUDIO3D, C_LOG_LEVEL_ERROR, "sceAudio3dGetSpeakerArrayMemorySize",
          "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceAudio3dGetSpeakerArrayMixCoefficients(void) {
    c_log(C_LOG_LIB_AUDIO3D, C_LOG_LEVEL_ERROR, "sceAudio3dGetSpeakerArrayMixCoefficients",
          "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceAudio3dGetSpeakerArrayMixCoefficients2(void) {
    c_log(C_LOG_LIB_AUDIO3D, C_LOG_LEVEL_ERROR, "sceAudio3dGetSpeakerArrayMixCoefficients2",
          "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceAudio3dInitialize(s64 reserved) {
    unsigned i;
    s32 init_ret;
    c_log(C_LOG_LIB_AUDIO3D, C_LOG_LEVEL_INFO, "sceAudio3dInitialize", "called, reserved = %lld",
          (long long)reserved);

    if (reserved != 0) {
        c_log(C_LOG_LIB_AUDIO3D, C_LOG_LEVEL_ERROR, "sceAudio3dInitialize", "reserved != 0");
        return ORBIS_AUDIO3D_ERROR_INVALID_PARAMETER;
    }

    if (g_state) {
        c_log(C_LOG_LIB_AUDIO3D, C_LOG_LEVEL_ERROR, "sceAudio3dInitialize", "already initialized");
        return ORBIS_AUDIO3D_ERROR_NOT_READY;
    }

    memset(g_ports, 0, sizeof(g_ports));
    for (i = 0; i < AUDIO3D_MAX_PORTS; i++) {
        unsigned j;
        audio3d_lib_port_t* port = &g_ports[i];
        retro_atomic_store_release_int(&port->audio_out_handle, -1);
        for (j = 0; j < AUDIO3D_MAX_ASSOCIATED_PORTS; j++) {
            port->audioout_ports[j].handle = -1;
        }
        pin_gate_init(&port->gate);
    }
    g_state = g_ports;

    init_ret = audioout_init();
    if (init_ret < 0 && init_ret != AUDIOOUT_ERROR_ALREADY_INIT) {
        return init_ret;
    }

    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceAudio3dObjectReserve(OrbisAudio3dPortId port_id,
                                         OrbisAudio3dObjectId* object_id) {
    audio3d_lib_port_t* port;
    unsigned id = 0;
    c_log(C_LOG_LIB_AUDIO3D, C_LOG_LEVEL_INFO, "sceAudio3dObjectReserve",
          "called, port_id = %u, object_id = 0x%llx", port_id,
          (unsigned long long)(uintptr_t)object_id);

    if (!object_id) {
        c_log(C_LOG_LIB_AUDIO3D, C_LOG_LEVEL_ERROR, "sceAudio3dObjectReserve", "!object_id");
        return ORBIS_AUDIO3D_ERROR_INVALID_PARAMETER;
    }

    *object_id = ORBIS_AUDIO3D_OBJECT_INVALID;

    port = port_pin(port_id);
    if (!port) {
        c_log(C_LOG_LIB_AUDIO3D, C_LOG_LEVEL_ERROR, "sceAudio3dObjectReserve",
              "!state->ports.contains(port_id)");
        return ORBIS_AUDIO3D_ERROR_INVALID_PORT;
    }

    /* As many as max_objects at PortOpen time, made then */
    if (!audio3d_object_reserve(&port->core, &id)) {
        c_log(C_LOG_LIB_AUDIO3D, C_LOG_LEVEL_ERROR, "sceAudio3dObjectReserve",
              "port has no available objects (max_objects = %u)", port->parameters.max_objects);
        port_unpin(port);
        return ORBIS_AUDIO3D_ERROR_OUT_OF_RESOURCES;
    }
    *object_id = id;
    c_log(C_LOG_LIB_AUDIO3D, C_LOG_LEVEL_INFO, "sceAudio3dObjectReserve", "reserved object_id = %u",
          *object_id);

    port_unpin(port);
    return ORBIS_OK;
}

/* One attribute of a live object. Only the gain is read by the mix: others are taken and not
 * kept. */
static s32 apply_attribute(audio3d_lib_port_t* port, audio3d_object_t* object, u32 attribute_id,
                           const void* value, u64 value_size) {
    switch (attribute_id) {
    case ORBIS_AUDIO3D_ATTRIBUTE_RESET_STATE:
        audio3d_object_reset(object);
        return ORBIS_OK;
    case ORBIS_AUDIO3D_ATTRIBUTE_PCM:
        if (!value || value_size < sizeof(OrbisAudio3dPcm)) {
            c_log(C_LOG_LIB_AUDIO3D, C_LOG_LEVEL_ERROR, "ApplyAttribute",
                  "PCM attribute value_size too small");
            return ORBIS_OK;
        }
        return convert_and_enqueue(&object->pcm, (const OrbisAudio3dPcm*)value, 1,
                                   port->parameters.granularity);
    case ORBIS_AUDIO3D_ATTRIBUTE_GAIN:
        if (value && value_size >= sizeof(float)) {
            float gain;
            memcpy(&gain, value, sizeof(gain));
            audio3d_object_set_gain(object, gain);
        }
        return ORBIS_OK;
    default:
        c_log(C_LOG_LIB_AUDIO3D, C_LOG_LEVEL_DEBUG, "ApplyAttribute", "Attribute 0x%x taken",
              attribute_id);
        return ORBIS_OK;
    }
}

/* After the port is pinned */
static s32 object_set_attribute(audio3d_lib_port_t* port, OrbisAudio3dObjectId object_id,
                                u32 attribute_id, const void* attribute, u64 attribute_size) {
    audio3d_object_t* object;
    const int object_state = audio3d_object_state(&port->core, object_id);
    if (object_state != AUDIO3D_OBJECT_LIVE && object_state != AUDIO3D_OBJECT_LEAVING) {
        c_log(C_LOG_LIB_AUDIO3D, C_LOG_LEVEL_DEBUG, "sceAudio3dObjectSetAttribute",
              "object_id %u not reserved (race with Unreserve?), no-op", object_id);
        return ORBIS_OK;
    }

    if (!attribute_size && attribute_id != ORBIS_AUDIO3D_ATTRIBUTE_RESET_STATE) {
        c_log(C_LOG_LIB_AUDIO3D, C_LOG_LEVEL_ERROR, "sceAudio3dObjectSetAttribute",
              "!attribute_size for non-reset attribute");
        return ORBIS_AUDIO3D_ERROR_INVALID_PARAMETER;
    }

    object = audio3d_object_get(&port->core, object_id);
    if (!object) {
        c_log(C_LOG_LIB_AUDIO3D, C_LOG_LEVEL_ERROR, "sceAudio3dObjectSetAttribute",
              "object_id not reserved");
        return ORBIS_AUDIO3D_ERROR_INVALID_OBJECT;
    }

    return apply_attribute(port, object, attribute_id, attribute, attribute_size);
}

s32 PS4_SYSV_ABI sceAudio3dObjectSetAttribute(OrbisAudio3dPortId port_id,
                                              OrbisAudio3dObjectId object_id, u32 attribute_id,
                                              const void* attribute, u64 attribute_size) {
    audio3d_lib_port_t* port;
    s32 ret;
    c_log(C_LOG_LIB_AUDIO3D, C_LOG_LEVEL_DEBUG, "sceAudio3dObjectSetAttribute",
          "called, port_id = %u, object_id = %u, attribute_id = 0x%x, size = %llu", port_id,
          object_id, attribute_id, (unsigned long long)attribute_size);

    port = port_pin(port_id);
    if (!port) {
        c_log(C_LOG_LIB_AUDIO3D, C_LOG_LEVEL_ERROR, "sceAudio3dObjectSetAttribute",
              "!state->ports.contains(port_id)");
        return ORBIS_AUDIO3D_ERROR_INVALID_PORT;
    }
    ret = object_set_attribute(port, object_id, attribute_id, attribute, attribute_size);
    port_unpin(port);
    return ret;
}

/* After the port is pinned */
static s32 object_set_attributes(audio3d_lib_port_t* port, OrbisAudio3dObjectId object_id,
                                 u64 num_attributes, const OrbisAudio3dAttribute* attribute_array) {
    audio3d_object_t* object;
    u64 i;
    int object_state;
    if (!num_attributes || !attribute_array) {
        c_log(C_LOG_LIB_AUDIO3D, C_LOG_LEVEL_ERROR, "sceAudio3dObjectSetAttributes",
              "!num_attributes || !attribute_array");
        return ORBIS_AUDIO3D_ERROR_INVALID_PARAMETER;
    }

    object_state = audio3d_object_state(&port->core, object_id);
    if (object_state != AUDIO3D_OBJECT_LIVE && object_state != AUDIO3D_OBJECT_LEAVING) {
        c_log(C_LOG_LIB_AUDIO3D, C_LOG_LEVEL_DEBUG, "sceAudio3dObjectSetAttributes",
              "object_id %u not reserved", object_id);
        return ORBIS_OK;
    }

    object = audio3d_object_get(&port->core, object_id);
    if (!object) {
        c_log(C_LOG_LIB_AUDIO3D, C_LOG_LEVEL_ERROR, "sceAudio3dObjectSetAttributes",
              "object_id not reserved");
        return ORBIS_AUDIO3D_ERROR_INVALID_OBJECT;
    }

    /* A reset first, however often and wherever it is listed */
    for (i = 0; i < num_attributes; i++) {
        if (attribute_array[i].attribute_id == ORBIS_AUDIO3D_ATTRIBUTE_RESET_STATE) {
            audio3d_object_reset(object);
            break;
        }
    }

    for (i = 0; i < num_attributes; i++) {
        s32 ret;
        const OrbisAudio3dAttribute* attribute = &attribute_array[i];
        if (attribute->attribute_id == ORBIS_AUDIO3D_ATTRIBUTE_RESET_STATE) {
            continue;
        }
        ret = apply_attribute(port, object, attribute->attribute_id, attribute->value,
                              attribute->value_size);
        if (ret != ORBIS_OK) {
            return ret;
        }
    }

    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceAudio3dObjectSetAttributes(OrbisAudio3dPortId port_id,
                                               OrbisAudio3dObjectId object_id, u64 num_attributes,
                                               const OrbisAudio3dAttribute* attribute_array) {
    audio3d_lib_port_t* port;
    s32 ret;
    c_log(C_LOG_LIB_AUDIO3D, C_LOG_LEVEL_DEBUG, "sceAudio3dObjectSetAttributes",
          "called, port_id = %u, object_id = %u, num_attributes = %llu, attribute_array = 0x%llx",
          port_id, object_id, (unsigned long long)num_attributes,
          (unsigned long long)(uintptr_t)attribute_array);

    port = port_pin(port_id);
    if (!port) {
        c_log(C_LOG_LIB_AUDIO3D, C_LOG_LEVEL_ERROR, "sceAudio3dObjectSetAttributes",
              "!state->ports.contains(port_id)");
        return ORBIS_AUDIO3D_ERROR_INVALID_PORT;
    }
    ret = object_set_attributes(port, object_id, num_attributes, attribute_array);
    port_unpin(port);
    return ret;
}

s32 PS4_SYSV_ABI sceAudio3dObjectUnreserve(OrbisAudio3dPortId port_id,
                                           OrbisAudio3dObjectId object_id) {
    audio3d_lib_port_t* port;
    c_log(C_LOG_LIB_AUDIO3D, C_LOG_LEVEL_DEBUG, "sceAudio3dObjectUnreserve",
          "called, port_id = %u, object_id = %u", port_id, object_id);

    port = port_pin(port_id);
    if (!port) {
        c_log(C_LOG_LIB_AUDIO3D, C_LOG_LEVEL_ERROR, "sceAudio3dObjectUnreserve",
              "!state->ports.contains(port_id)");
        return ORBIS_AUDIO3D_ERROR_INVALID_PORT;
    }

    /* Its queued sound is still mixed; then it is free */
    if (!audio3d_object_unreserve(&port->core, object_id)) {
        c_log(C_LOG_LIB_AUDIO3D, C_LOG_LEVEL_ERROR, "sceAudio3dObjectUnreserve",
              "object_id not reserved");
        port_unpin(port);
        return ORBIS_AUDIO3D_ERROR_INVALID_OBJECT;
    }
    port_unpin(port);
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceAudio3dPortAdvance(OrbisAudio3dPortId port_id) {
    audio3d_lib_port_t* port;
    c_log(C_LOG_LIB_AUDIO3D, C_LOG_LEVEL_DEBUG, "sceAudio3dPortAdvance", "called, port_id = %u",
          port_id);

    port = port_pin(port_id);
    if (!port) {
        c_log(C_LOG_LIB_AUDIO3D, C_LOG_LEVEL_ERROR, "sceAudio3dPortAdvance",
              "!state->ports.contains(port_id)");
        return ORBIS_AUDIO3D_ERROR_INVALID_PORT;
    }

    if (port->parameters.buffer_mode == ORBIS_AUDIO3D_BUFFER_NO_ADVANCE) {
        c_log(C_LOG_LIB_AUDIO3D, C_LOG_LEVEL_ERROR, "sceAudio3dPortAdvance",
              "port doesn't have advance capability");
        port_unpin(port);
        return ORBIS_AUDIO3D_ERROR_NOT_SUPPORTED;
    }

    /* The mix is made in the mixed queue's own next block (audio3d_port.c) */
    if (!audio3d_port_mix(&port->core)) {
        c_log(C_LOG_LIB_AUDIO3D, C_LOG_LEVEL_WARNING, "sceAudio3dPortAdvance",
              "mixed queue full (depth=%u), dropping advance", port->parameters.queue_depth);
        port_unpin(port);
        return ORBIS_AUDIO3D_ERROR_NOT_READY;
    }
    port_unpin(port);
    return ORBIS_OK;
}

static int port_gate_drained(void* ctx) {
    audio3d_lib_port_t* port = (audio3d_lib_port_t*)ctx;
    return pin_gate_drained(&port->gate) != 0;
}

s32 PS4_SYSV_ABI sceAudio3dPortClose(OrbisAudio3dPortId port_id) {
    audio3d_lib_port_t* port;
    unsigned i;
    s32 handle;
    c_log(C_LOG_LIB_AUDIO3D, C_LOG_LEVEL_INFO, "sceAudio3dPortClose", "called, port_id = %u",
          port_id);

    if (!g_state || port_id >= AUDIO3D_MAX_PORTS ||
        !retro_atomic_cas_int(&g_state[port_id].state, PORT_OPEN, PORT_CLOSING)) {
        c_log(C_LOG_LIB_AUDIO3D, C_LOG_LEVEL_ERROR, "sceAudio3dPortClose",
              "!state->ports.contains(port_id)");
        return ORBIS_AUDIO3D_ERROR_INVALID_PORT;
    }
    port = &g_state[port_id];

    /* Calls already in it (a push sending a block, which waits on the game's time) leave
     * first: waited for as a game thread, so the time goes on meanwhile */
    pin_gate_shut(&port->gate);
    kernel_game_wait(&port->gate.drained, 0, port_gate_drained, port);

    handle = retro_atomic_exchange_int(&port->audio_out_handle, -1);
    if (handle >= 0) {
        audioout_close(handle);
    }
    for (i = 0; i < AUDIO3D_MAX_ASSOCIATED_PORTS; i++) {
        audio3d_associated_port_t* aout = &port->audioout_ports[i];
        if (retro_atomic_exchange_int(&aout->state, ASSOCIATED_FREE) == ASSOCIATED_LIVE) {
            audioout_close(aout->handle);
        }
        audio3d_ring_free(&aout->pending);
        aout->handle = -1;
    }
    audio3d_port_free(&port->core);

    pin_gate_open(&port->gate);
    retro_atomic_store_release_int(&port->state, PORT_FREE);
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceAudio3dPortCreate(u32 granularity, u32 rate, s64 reserved,
                                      OrbisAudio3dPortId* port_id) {
    OrbisAudio3dOpenParameters local_params;
    c_log(C_LOG_LIB_AUDIO3D, C_LOG_LEVEL_INFO, "sceAudio3dPortCreate",
          "called, granularity = %u, rate = %u, reserved = %lld, port_id = 0x%llx", granularity,
          rate, (long long)reserved, (unsigned long long)(uintptr_t)port_id);

    if (!port_id || reserved) {
        c_log(C_LOG_LIB_AUDIO3D, C_LOG_LEVEL_INFO, "sceAudio3dPortCreate", "!port_id || reserved");
        return ORBIS_AUDIO3D_ERROR_INVALID_PARAMETER;
    }

    memset(&local_params, 0, sizeof(local_params));
    local_params.size_this = 0x10;
    local_params.granularity = granularity;
    local_params.rate = rate;
    return sceAudio3dPortOpen(0xFF, &local_params, port_id);
}

s32 PS4_SYSV_ABI sceAudio3dPortDestroy(void) {
    c_log(C_LOG_LIB_AUDIO3D, C_LOG_LEVEL_ERROR, "sceAudio3dPortDestroy", "(STUBBED) called");
    return ORBIS_OK;
}

/* After the port is pinned */
static s32 port_flush(audio3d_lib_port_t* port) {
    bool any_associated = false;
    unsigned i;
    s32 handle;
    for (i = 0; i < AUDIO3D_MAX_ASSOCIATED_PORTS; i++) {
        if (retro_atomic_load_acquire_int(&port->audioout_ports[i].state) == ASSOCIATED_LIVE) {
            any_associated = true;
        }
    }
    if (any_associated) {
        s32 ret = drain_associated_ports(port);
        if (ret < 0) {
            return ret;
        }
        for (i = 0; i < AUDIO3D_MAX_ASSOCIATED_PORTS; i++) {
            audio3d_associated_port_t* aout = &port->audioout_ports[i];
            if (retro_atomic_load_acquire_int(&aout->state) != ASSOCIATED_LIVE) {
                continue;
            }
            ret = audioout_output(aout->handle, NULL);
            if (ret < 0) {
                return ret;
            }
        }
        return ORBIS_OK;
    }

    /* Only mix if there's actually something to mix. */
    if (audio3d_ring_level(&port->core.mixed) == 0 && audio3d_port_has_input(&port->core)) {
        (void)audio3d_port_mix(&port->core);
    }

    if (audio3d_ring_level(&port->core.mixed) == 0) {
        return ORBIS_OK;
    }

    handle = port_output_handle(port);
    if (handle < 0) {
        return handle;
    }

    /* Drain all queued mixed frames, blocking on each until consumed. */
    for (;;) {
        bool sent = false;
        const s32 ret = submit_mixed(port, handle, &sent);
        if (ret < 0) {
            return ret;
        }
        if (!sent) {
            return ORBIS_OK;
        }
    }
}

s32 PS4_SYSV_ABI sceAudio3dPortFlush(OrbisAudio3dPortId port_id) {
    audio3d_lib_port_t* port;
    s32 ret;
    c_log(C_LOG_LIB_AUDIO3D, C_LOG_LEVEL_DEBUG, "sceAudio3dPortFlush", "called, port_id = %u",
          port_id);

    port = port_pin(port_id);
    if (!port) {
        c_log(C_LOG_LIB_AUDIO3D, C_LOG_LEVEL_ERROR, "sceAudio3dPortFlush",
              "!state->ports.contains(port_id)");
        return ORBIS_AUDIO3D_ERROR_INVALID_PORT;
    }
    ret = port_flush(port);
    port_unpin(port);
    return ret;
}

s32 PS4_SYSV_ABI sceAudio3dPortFreeState(void) {
    c_log(C_LOG_LIB_AUDIO3D, C_LOG_LEVEL_ERROR, "sceAudio3dPortFreeState", "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceAudio3dPortGetAttributesSupported(OrbisAudio3dPortId port_id, u32* capabilities,
                                                      u32* num_capabilities) {
    audio3d_lib_port_t* port;
    c_log(C_LOG_LIB_AUDIO3D, C_LOG_LEVEL_DEBUG, "sceAudio3dPortGetAttributesSupported", "called");
    if (!num_capabilities) {
        return ORBIS_AUDIO3D_ERROR_INVALID_PARAMETER;
    }

    port = port_pin(port_id);
    if (!port) {
        return ORBIS_AUDIO3D_ERROR_INVALID_PORT;
    }
    port_unpin(port);

    if (capabilities) {
        /* Writes up to num_capabilities supported capabilities, then sets num_capabilities to
         * how many were written. */
        const u32 caps_to_write = *num_capabilities;
        if (caps_to_write >= 1) {
            capabilities[0] = ORBIS_AUDIO3D_ATTRIBUTE_PCM;
        }
        if (caps_to_write >= 2) {
            capabilities[1] = ORBIS_AUDIO3D_ATTRIBUTE_GAIN;
        }
        if (caps_to_write >= 3) {
            capabilities[2] = ORBIS_AUDIO3D_ATTRIBUTE_RESET_STATE;
        }
        *num_capabilities = caps_to_write < 3 ? caps_to_write : 3;
    } else {
        /* If capabilities is null, then just report the number of supported capabilities. */
        *num_capabilities = 3;
    }
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceAudio3dPortGetList(void) {
    c_log(C_LOG_LIB_AUDIO3D, C_LOG_LEVEL_ERROR, "sceAudio3dPortGetList", "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceAudio3dPortGetParameters(void) {
    c_log(C_LOG_LIB_AUDIO3D, C_LOG_LEVEL_ERROR, "sceAudio3dPortGetParameters", "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceAudio3dPortGetQueueLevel(OrbisAudio3dPortId port_id, u32* queue_level,
                                             u32* queue_available) {
    audio3d_lib_port_t* port;
    u32 size;
    c_log(C_LOG_LIB_AUDIO3D, C_LOG_LEVEL_DEBUG, "sceAudio3dPortGetQueueLevel",
          "called, port_id = %u, queue_level = 0x%llx, queue_available = 0x%llx", port_id,
          (unsigned long long)(uintptr_t)queue_level,
          (unsigned long long)(uintptr_t)queue_available);

    port = port_pin(port_id);
    if (!port) {
        c_log(C_LOG_LIB_AUDIO3D, C_LOG_LEVEL_ERROR, "sceAudio3dPortGetQueueLevel",
              "!state->ports.contains(port_id)");
        return ORBIS_AUDIO3D_ERROR_INVALID_PORT;
    }

    if (!queue_level && !queue_available) {
        port_unpin(port);
        return ORBIS_AUDIO3D_ERROR_INVALID_PARAMETER;
    }

    size = audio3d_ring_level(&port->core.mixed);

    if (queue_level) {
        *queue_level = size;
    }

    if (queue_available) {
        const u32 depth = port->parameters.queue_depth;
        *queue_available = (size < depth) ? depth - size : 0u;
    }

    port_unpin(port);
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceAudio3dPortGetState(void) {
    c_log(C_LOG_LIB_AUDIO3D, C_LOG_LEVEL_ERROR, "sceAudio3dPortGetState", "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceAudio3dPortGetStatus(void) {
    c_log(C_LOG_LIB_AUDIO3D, C_LOG_LEVEL_ERROR, "sceAudio3dPortGetStatus", "(STUBBED) called");
    return ORBIS_OK;
}

/* A port's parameter checks, as sceAudio3dPortOpen logs them: 0 or the error */
static s32 port_parameters_check(const OrbisAudio3dOpenParameters* effective) {
    const char* const func = "sceAudio3dPortOpen";
    if (effective->rate != ORBIS_AUDIO3D_RATE_48000) {
        c_log(C_LOG_LIB_AUDIO3D, C_LOG_LEVEL_ERROR, func, "unsupported rate");
        return ORBIS_AUDIO3D_ERROR_INVALID_PARAMETER;
    }

    if (effective->granularity < 0x100) {
        c_log(C_LOG_LIB_AUDIO3D, C_LOG_LEVEL_ERROR, func, "granularity < 0x100");
        return ORBIS_AUDIO3D_ERROR_INVALID_PARAMETER;
    }

    if ((effective->granularity & 0xFF) != 0) {
        c_log(C_LOG_LIB_AUDIO3D, C_LOG_LEVEL_ERROR, func, "granularity not aligned to 0x100");
        return ORBIS_AUDIO3D_ERROR_INVALID_PARAMETER;
    }

    if (effective->max_objects == 0) {
        c_log(C_LOG_LIB_AUDIO3D, C_LOG_LEVEL_ERROR, func, "max_objects == 0");
        return ORBIS_AUDIO3D_ERROR_INVALID_PARAMETER;
    }

    if (effective->queue_depth == 0) {
        c_log(C_LOG_LIB_AUDIO3D, C_LOG_LEVEL_ERROR, func, "queue_depth == 0");
        return ORBIS_AUDIO3D_ERROR_INVALID_PARAMETER;
    }

    if (effective->granularity == 0x100 && effective->queue_depth > 0x40) {
        c_log(C_LOG_LIB_AUDIO3D, C_LOG_LEVEL_ERROR, func,
              "queue_depth too large for 0x100 granularity");
        return ORBIS_AUDIO3D_ERROR_INVALID_PARAMETER;
    }

    if (effective->granularity == 0x200 && effective->queue_depth > 0x1F) {
        c_log(C_LOG_LIB_AUDIO3D, C_LOG_LEVEL_ERROR, func,
              "queue_depth too large for 0x200 granularity");
        return ORBIS_AUDIO3D_ERROR_INVALID_PARAMETER;
    }

    if (effective->granularity == 0x300 && effective->queue_depth > 0x14) {
        c_log(C_LOG_LIB_AUDIO3D, C_LOG_LEVEL_ERROR, func,
              "queue_depth too large for 0x300 granularity");
        return ORBIS_AUDIO3D_ERROR_INVALID_PARAMETER;
    }

    if (effective->queue_depth > 0xF && effective->granularity > 0x3FF) {
        c_log(C_LOG_LIB_AUDIO3D, C_LOG_LEVEL_ERROR, func,
              "queue_depth invalid for large granularity");
        return ORBIS_AUDIO3D_ERROR_INVALID_PARAMETER;
    }

    if (effective->buffer_mode > 2) {
        c_log(C_LOG_LIB_AUDIO3D, C_LOG_LEVEL_ERROR, func, "invalid buffer_mode");
        return ORBIS_AUDIO3D_ERROR_INVALID_PARAMETER;
    }

    if ((effective->num_beds & 0xfffffffe) != 2) {
        c_log(C_LOG_LIB_AUDIO3D, C_LOG_LEVEL_ERROR, func, "invalid num_beds");
        return ORBIS_AUDIO3D_ERROR_INVALID_PARAMETER;
    }
    return 0;
}

s32 PS4_SYSV_ABI sceAudio3dPortOpen(s32 user_id, const OrbisAudio3dOpenParameters* parameters,
                                    OrbisAudio3dPortId* port_id) {
    OrbisAudio3dOpenParameters effective;
    OrbisAudio3dPortId id;
    s32 ret;
    c_log(C_LOG_LIB_AUDIO3D, C_LOG_LEVEL_INFO, "sceAudio3dPortOpen",
          "called, user_id = %d, parameters = 0x%llx, port_id = 0x%llx", user_id,
          (unsigned long long)(uintptr_t)parameters, (unsigned long long)(uintptr_t)port_id);

    if (user_id != 0xFF || !parameters || !port_id) {
        c_log(C_LOG_LIB_AUDIO3D, C_LOG_LEVEL_ERROR, "sceAudio3dPortOpen",
              "user_id != 0xFF || !parameters || !port_id");
        if (port_id) {
            *port_id = ORBIS_AUDIO3D_PORT_INVALID;
        }

        return ORBIS_AUDIO3D_ERROR_INVALID_PARAMETER;
    }

    *port_id = ORBIS_AUDIO3D_PORT_INVALID;

    if (!g_state) {
        c_log(C_LOG_LIB_AUDIO3D, C_LOG_LEVEL_ERROR, "sceAudio3dPortOpen", "!initialized");
        return ORBIS_AUDIO3D_ERROR_NOT_READY;
    }

    effective.size_this = 0x28;
    effective.granularity = parameters->granularity;
    effective.rate = parameters->rate;
    effective.max_objects = 512;
    effective.queue_depth = 2;
    effective.buffer_mode = ORBIS_AUDIO3D_BUFFER_NO_ADVANCE;
    effective._pad = 0;
    effective.num_beds = 2;

    switch (parameters->size_this & ~(u64)0x7) {
    case 0x10:
        break;

    case 0x18:
        effective.max_objects = parameters->max_objects;
        effective.queue_depth = parameters->queue_depth;
        effective.buffer_mode = ORBIS_AUDIO3D_BUFFER_ADVANCE_NO_PUSH;
        break;

    case 0x20:
        effective.max_objects = parameters->max_objects;
        effective.queue_depth = parameters->queue_depth;
        effective.buffer_mode = parameters->buffer_mode;
        break;

    case 0x28:
        effective.max_objects = parameters->max_objects;
        effective.queue_depth = parameters->queue_depth;
        effective.buffer_mode = parameters->buffer_mode;
        effective.num_beds = parameters->num_beds;
        break;

    default:
        c_log(C_LOG_LIB_AUDIO3D, C_LOG_LEVEL_ERROR, "sceAudio3dPortOpen", "invalid size_this");
        return ORBIS_AUDIO3D_ERROR_INVALID_PARAMETER;
    }

    ret = port_parameters_check(&effective);
    if (ret != 0) {
        return ret;
    }

    if (effective.max_objects > 0x200) {
        c_log(C_LOG_LIB_AUDIO3D, C_LOG_LEVEL_WARNING, "sceAudio3dPortOpen",
              "max_objects %u exceeds limit, clamping to 512", effective.max_objects);
        effective.max_objects = 0x200;
    }

    /* A free port, had by this thread from here; everything it holds made now */
    for (id = 0; id < AUDIO3D_MAX_PORTS; id++) {
        audio3d_lib_port_t* port = &g_state[id];
        if (!retro_atomic_cas_int(&port->state, PORT_FREE, PORT_OPENING)) {
            continue;
        }
        port->parameters = effective;
        retro_atomic_store_release_int(&port->audio_out_handle, -1);
        if (!audio3d_port_init(&port->core, effective.granularity, effective.queue_depth,
                               effective.max_objects, output_channels())) {
            retro_atomic_store_release_int(&port->state, PORT_FREE);
            c_log(C_LOG_LIB_AUDIO3D, C_LOG_LEVEL_ERROR, "sceAudio3dPortOpen", "out of memory");
            return ORBIS_AUDIO3D_ERROR_OUT_OF_MEMORY;
        }
        retro_atomic_store_release_int(&port->state, PORT_OPEN);
        *port_id = id;
        return ORBIS_OK;
    }

    c_log(C_LOG_LIB_AUDIO3D, C_LOG_LEVEL_ERROR, "sceAudio3dPortOpen", "no free ports");
    return ORBIS_AUDIO3D_ERROR_OUT_OF_RESOURCES;
}

/* After the port is pinned */
static s32 port_push(audio3d_lib_port_t* port, u32 blocking) {
    u32 depth;
    s32 handle;
    s32 ret;
    bool sent = false;
    if (port->parameters.buffer_mode != ORBIS_AUDIO3D_BUFFER_ADVANCE_AND_PUSH) {
        c_log(C_LOG_LIB_AUDIO3D, C_LOG_LEVEL_ERROR, "sceAudio3dPortPush",
              "port doesn't have push capability");
        return ORBIS_AUDIO3D_ERROR_NOT_SUPPORTED;
    }

    depth = port->parameters.queue_depth;

    ret = drain_associated_ports(port);
    if (ret < 0) {
        return ret;
    }

    handle = port_output_handle(port);
    if (handle < 0) {
        return handle;
    }

    if (audio3d_ring_level(&port->core.mixed) < depth) {
        return ORBIS_OK;
    }

    ret = submit_mixed(port, handle, &sent);
    if (ret < 0) {
        return ret;
    }
    if (!sent || blocking == ORBIS_AUDIO3D_BLOCKING_ASYNC) {
        return ORBIS_OK;
    }

    while (audio3d_ring_level(&port->core.mixed) >= depth) {
        ret = submit_mixed(port, handle, &sent);
        if (ret < 0) {
            return ret;
        }
        if (!sent) {
            break;
        }
    }

    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceAudio3dPortPush(OrbisAudio3dPortId port_id, u32 blocking) {
    audio3d_lib_port_t* port;
    s32 ret;
    c_log(C_LOG_LIB_AUDIO3D, C_LOG_LEVEL_DEBUG, "sceAudio3dPortPush",
          "called, port_id = %u, blocking = %s", port_id,
          enum_name(&enum_names_OrbisAudio3dBlocking, (s64)blocking));

    port = port_pin(port_id);
    if (!port) {
        c_log(C_LOG_LIB_AUDIO3D, C_LOG_LEVEL_ERROR, "sceAudio3dPortPush",
              "!state->ports.contains(port_id)");
        return ORBIS_AUDIO3D_ERROR_INVALID_PORT;
    }
    ret = port_push(port, blocking);
    port_unpin(port);
    return ret;
}

s32 PS4_SYSV_ABI sceAudio3dPortQueryDebug(void) {
    c_log(C_LOG_LIB_AUDIO3D, C_LOG_LEVEL_ERROR, "sceAudio3dPortQueryDebug", "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceAudio3dPortSetAttribute(OrbisAudio3dPortId port_id, u32 attribute_id,
                                            void* attribute, u64 attribute_size) {
    audio3d_lib_port_t* port;
    c_log(C_LOG_LIB_AUDIO3D, C_LOG_LEVEL_INFO, "sceAudio3dPortSetAttribute",
          "called, port_id = %u, attribute_id = %u, attribute = 0x%llx, attribute_size = %llu",
          port_id, attribute_id, (unsigned long long)(uintptr_t)attribute,
          (unsigned long long)attribute_size);

    port = port_pin(port_id);
    if (!port) {
        c_log(C_LOG_LIB_AUDIO3D, C_LOG_LEVEL_ERROR, "sceAudio3dPortSetAttribute",
              "!state->ports.contains(port_id)");
        return ORBIS_AUDIO3D_ERROR_INVALID_PORT;
    }
    port_unpin(port);

    if (!attribute) {
        c_log(C_LOG_LIB_AUDIO3D, C_LOG_LEVEL_ERROR, "sceAudio3dPortSetAttribute", "!attribute");
        return ORBIS_AUDIO3D_ERROR_INVALID_PARAMETER;
    }

    /* TODO */

    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceAudio3dReportRegisterHandler(void) {
    c_log(C_LOG_LIB_AUDIO3D, C_LOG_LEVEL_ERROR, "sceAudio3dReportRegisterHandler",
          "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceAudio3dReportUnregisterHandler(void) {
    c_log(C_LOG_LIB_AUDIO3D, C_LOG_LEVEL_ERROR, "sceAudio3dReportUnregisterHandler",
          "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceAudio3dSetGpuRenderer(void) {
    c_log(C_LOG_LIB_AUDIO3D, C_LOG_LEVEL_ERROR, "sceAudio3dSetGpuRenderer", "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceAudio3dStrError(void) {
    c_log(C_LOG_LIB_AUDIO3D, C_LOG_LEVEL_ERROR, "sceAudio3dStrError", "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceAudio3dTerminate(void) {
    OrbisAudio3dPortId id;
    c_log(C_LOG_LIB_AUDIO3D, C_LOG_LEVEL_INFO, "sceAudio3dTerminate", "called");
    if (!g_state) {
        return ORBIS_AUDIO3D_ERROR_NOT_READY;
    }

    for (id = 0; id < AUDIO3D_MAX_PORTS; id++) {
        if (retro_atomic_load_acquire_int(&g_state[id].state) == PORT_OPEN) {
            sceAudio3dPortClose(id);
        }
    }
    for (id = 0; id < AUDIO3D_MAX_PORTS; id++) {
        pin_gate_free(&g_state[id].gate);
    }

    g_state = NULL;
    return ORBIS_OK;
}
