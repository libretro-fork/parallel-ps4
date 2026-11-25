/* SPDX-FileCopyrightText: Copyright 2024-2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <retro_atomic.h>
#include <string.h>

#include "common/c_bridge.h"
#include "common/combine.h"
#include "common/enum_names.h"
#include "common/game_clock.h"
#include "core/libraries/audio/audioout.h"
#include "core/libraries/audio/audioout_backend.h"
#include "core/libraries/audio/audioout_c.h"
#include "core/libraries/audio/audioout_error.h"
#include "core/libraries/error_codes.h"
#include "core/libraries/kernel/kernel_c.h"

/* The ports, read by any thread with no lock; opening and closing run one at a time. Each
 * port lives for the program: nothing is allocated. */
static PortOut port_table[ORBIS_AUDIO_OUT_NUM_PORTS];
static combiner_t port_allocation;
/* The last open's number (0 is none; under port_allocation) */
static int open_count;

static retro_atomic_int_t lazy_init;
/* Set once sceAudioOutInit is done: the frontend plays the sound (frontend_audio_out.c) */
static retro_atomic_int_t audio_ready;

void audio_out_library_init(void) {
    int i;
    combiner_init(&port_allocation);
    for (i = 0; i < ORBIS_AUDIO_OUT_NUM_PORTS; i++) {
        combiner_init(&port_table[i].writers);
        combiner_init(&port_table[i].control);
    }
}

static bool audio_out_ready(void) {
    return retro_atomic_load_relaxed_int(&lazy_init) != 0 &&
           retro_atomic_load_acquire_int(&audio_ready) != 0;
}

/* Port allocation ranges */
typedef struct PortRange {
    s32 start;
    s32 end;
} PortRange;

static const PortRange port_ranges[] = {
    {0, 7},   /* MAIN */
    {8, 8},   /* BGM */
    {9, 12},  /* VOICE */
    {13, 16}, /* PERSONAL */
    {17, 20}, /* PADSPK */
    {21, 21}, /* Type 5-8 */
    {22, 22}, /* Audio3d (126) */
    {23, 23}, /* AUX (127) */
    {24, 24}, /* Type 125 */
};

static const AudioFormatInfo format_infos[8] = {
    /* S16Mono */
    {false, 2, 1, {0}, false},
    /* S16Stereo */
    {false, 2, 2, {0, 1}, false},
    /* S16_8CH */
    {false, 2, 8, {0, 1, 2, 3, 4, 5, 6, 7}, false},
    /* FloatMono */
    {true, 4, 1, {0}, false},
    /* FloatStereo */
    {true, 4, 2, {0, 1}, false},
    /* Float_8CH */
    {true, 4, 8, {0, 1, 2, 3, 4, 5, 6, 7}, false},
    /* S16_8CH_Std */
    {false, 2, 8, {0, 1, 2, 3, 6, 7, 4, 5}, true},
    /* Float_8CH_Std */
    {true, 4, 8, {0, 1, 2, 3, 6, 7, 4, 5}, true},
};

/*
 * Helper functions
 **/
static int GetPortRange(s32 type) {
    switch (type) {
    case 0:
        return 0; /* MAIN */
    case 1:
        return 1; /* BGM */
    case 2:
        return 2; /* VOICE */
    case 3:
        return 3; /* PERSONAL */
    case 4:
        return 4; /* PADSPK */
    case 5:
        return 5; /* Type 5 */
    case 6:
        return 5; /* Type 6 */
    case 7:
        return 5; /* Type 7 */
    case 8:
        return 5; /* Type 8 */
    case 126:
        return 6; /* Audio3d */
    case 125:
        return 8; /* Type 125 */
    case 127:
        return 7; /* AUX */
    default:
        return -1;
    }
}

static int GetPortId(s32 handle) {
    const int port_id = handle & 0xFF;

    if (port_id >= ORBIS_AUDIO_OUT_NUM_PORTS) {
        c_log(C_LOG_LIB_AUDIO_OUT, C_LOG_LEVEL_DEBUG, "GetPortId",
              "Invalid handle 0x%x: port index %d out of range (max %d)", handle, port_id,
              ORBIS_AUDIO_OUT_NUM_PORTS - 1);
        return ORBIS_AUDIO_OUT_ERROR_INVALID_PORT;
    }

    if ((handle & 0x3F000000) != 0x20000000) {
        c_log(C_LOG_LIB_AUDIO_OUT, C_LOG_LEVEL_DEBUG, "GetPortId",
              "Invalid handle 0x%x: bad magic 0x%x, expected 0x%x", handle, handle & 0x3F000000,
              0x20000000);
        return ORBIS_AUDIO_OUT_ERROR_INVALID_PORT;
    }

    return port_id;
}

static s32 GetPortType(s32 handle) {
    return (handle >> 16) & 0xFF;
}

/* The port types a handle may carry for the calls that output and close: 0-4, 14, 126, 127 */
static bool IsValidHandleType(s32 port_type) {
    if (port_type >= 5 && port_type <= 13) {
        return false;
    }
    return (port_type >= 0 && port_type <= 4) || port_type == 14 || port_type == 126 ||
           port_type == 127;
}

/* Under port_allocation */
static int AllocatePort(s32 type) {
    int i;
    int range_idx = GetPortRange(type);
    if (range_idx < 0) {
        return -1;
    }

    for (i = port_ranges[range_idx].start; i <= port_ranges[range_idx].end; i++) {
        if (retro_atomic_load_relaxed_int(&port_table[i].open) == 0) {
            return i;
        }
    }
    return -1;
}

/* A call on a port: the port, and the open it found */
typedef struct audio_out_call {
    PortOut* port;
    int open;
} audio_out_call_t;

/* The port's open now, and the call's port; 0 when it is not open */
static int audio_out_find(int port_id, audio_out_call_t* call) {
    call->port = &port_table[port_id];
    call->open = retro_atomic_load_acquire_int(&call->port->open);
    return call->open;
}

typedef struct audio_out_pace_wait {
    game_clock_t* clock;
    retro_atomic_int_t** woken;
    int woke;
    u64 frame;
} audio_out_pace_wait_t;

static int audio_out_woken_waiting(void* ctx) {
    audio_out_pace_wait_t* w = (audio_out_pace_wait_t*)ctx;
    return game_clock_woken_waiting(w->clock, w->woken, w->woke, w->frame) != 0;
}

/* The console's pace: sceAudioOutOutput returns once a buffer's time, as the device takes
 * each buffer, not as fast as there is room. An engine that mixes ahead into a small ring
 * and outputs from it (FMOD: 256-frame slots of a 512-frame ring) relies on that: returning
 * a few buffers at once lets its output overtake its mixer and play halves it has not
 * written yet, a click every buffer. Waited for on the game's time (a sleep to the buffer's
 * deadline): the first buffer, and the first after the game stopped outputting for a while,
 * go at once, each one after a buffer's time later, counted in whole frames of the port's
 * rate so the pace does not drift. Among the port's writers. */
static void PaceOutput(PortOut* port) {
    /* The threads this one woke since its last buffer (FMOD's mixer, for the slot it gave
     * back) wait again before the next buffer goes, late or not: the output never takes the
     * next slot before its mixer has refilled it. For a whole frame at most (the second to
     * start since: the first may start while the mixer is at it).
     * Only for a game with FMOD in it (game_clock_set_fmod): any other's output goes at the
     * device's pace alone */
    retro_atomic_int_t* woken[4];
    u64 due;
    const int woke = kernel_game_take_wakes(woken, 4);
    if (woke != 0 && game_clock_fmod(kernel_game_clock())) {
        audio_out_pace_wait_t w;
        w.clock = kernel_game_clock();
        w.woken = woken;
        w.woke = woke;
        w.frame = game_clock_frames(w.clock);
        kernel_game_wait(&w.clock->parked_ec, 0, audio_out_woken_waiting, &w);
    }
    due = game_clock_cadence(kernel_game_clock(), &port->pace_start, &port->pace_buffers,
                             port->buffer_frames, port->sample_rate);
    if (due > kernel_game_now()) {
        kernel_game_sleep_until(due);
    }
}

/* Hands @ptr, a buffer of the port's, to its output. Among the port's writers. */
static bool OutputBuffer(const audio_out_call_t* call, const void* ptr) {
    PortOut* port = call->port;
    if (retro_atomic_load_acquire_int(&port->open) != call->open) {
        return false;
    }
    frontend_audio_out_output(&port->impl, ptr);
    retro_atomic_store_relaxed_64(&port->last_output_time, kernel_get_process_time());
    return true;
}

/*
 * sceAudioOut implementation
 **/
s32 PS4_SYSV_ABI sceAudioOutInit(void) {
#ifndef NDEBUG
    c_log(C_LOG_LIB_AUDIO_OUT, C_LOG_LEVEL_TRACE, "sceAudioOutInit", "called");
#endif

    if (!retro_atomic_cas_int(&lazy_init, 0, 1)) {
        return ORBIS_AUDIO_OUT_ERROR_ALREADY_INIT;
    }

    /* The frontend plays the sound (frontend_audio_out.c). */
    retro_atomic_store_release_int(&audio_ready, 1);

    c_log(C_LOG_LIB_AUDIO_OUT, C_LOG_LEVEL_INFO, "sceAudioOutInit", "Audio system initialized");
    return ORBIS_OK;
}

typedef struct audio_out_open_call {
    s32 user_id;
    s32 type;
    u32 format;
    u32 length;
    u32 sample_rate;
    bool is_restricted;
    bool is_mix_to_main;
    int port_id;
    int open;
    s32 result;
} audio_out_open_call_t;

/* Under the port's writers and control: its fields and backend set for the open
 * @call->open */
static void audio_out_setup_locked(void* ctx) {
    audio_out_open_call_t* call = (audio_out_open_call_t*)ctx;
    PortOut* port = &port_table[call->port_id];
    int i;

    /* A backend a close has not taken down yet: the port's last open's */
    if (port->impl_open != 0) {
        frontend_audio_out_close(&port->impl);
        port->impl_open = 0;
    }

    port->userId = call->user_id;
    port->type = call->type;
    port->format_info = format_infos[call->format];
    port->sample_rate = call->sample_rate;
    port->buffer_frames = call->length;
    for (i = 0; i < 8; i++) {
        port->volume[i] = ORBIS_AUDIO_OUT_VOLUME_0DB;
    }
    port->mixLevelPadSpk = ORBIS_AUDIO_OUT_MIXLEVEL_PADSPK_DEFAULT;
    port->pace_start = 0;
    port->pace_buffers = 0;
    retro_atomic_store_relaxed_64(&port->last_output_time, 0);

    /* Set attributes */
    port->is_restricted = call->is_restricted;
    port->is_mix_to_main = call->is_mix_to_main;

    /* Log attributes if present */
    if (call->is_restricted) {
        c_log(C_LOG_LIB_AUDIO_OUT, C_LOG_LEVEL_INFO, "sceAudioOutOpen",
              "Audio port opened with RESTRICTED attribute");
    }
    if (call->is_mix_to_main) {
        c_log(C_LOG_LIB_AUDIO_OUT, C_LOG_LEVEL_INFO, "sceAudioOutOpen",
              "Audio port opened with MIX_TO_MAIN attribute");
    }

    /* Create backend */
    frontend_audio_out_open(&port->impl, port->type, &port->format_info, port->buffer_frames);

    /* Set initial volume */
    frontend_audio_out_set_volume(&port->impl, port->volume);
    port->impl_open = call->open;
}

static void audio_out_setup_writers(void* ctx) {
    audio_out_open_call_t* call = (audio_out_open_call_t*)ctx;
    combiner_run(&port_table[call->port_id].control, audio_out_setup_locked, call);
}

/* Under port_allocation */
static void audio_out_open_locked(void* ctx) {
    audio_out_open_call_t* call = (audio_out_open_call_t*)ctx;
    s32 handle;

    /* Allocate port */
    call->port_id = AllocatePort(call->type);
    if (call->port_id < 0) {
        c_log(C_LOG_LIB_AUDIO_OUT, C_LOG_LEVEL_ERROR, "sceAudioOutOpen", "Error allocated port");
        call->result = ORBIS_AUDIO_OUT_ERROR_PORT_FULL;
        return;
    }

    if (++open_count <= 0) {
        open_count = 1;
    }
    call->open = open_count;
    combiner_run(&port_table[call->port_id].writers, audio_out_setup_writers, call);
    retro_atomic_store_release_int(&port_table[call->port_id].open, call->open);

    /* Create handle */
    handle = (call->type << 16) | call->port_id | 0x20000000;
    c_log(C_LOG_LIB_AUDIO_OUT, C_LOG_LEVEL_INFO, "sceAudioOutOpen",
          "opened port_type=%s(%d) -> port_id=%d, handle=0x%x",
          enum_name(&enum_names_OrbisAudioOutPort, (int64_t)call->type), call->type, call->port_id,
          handle);
    call->result = handle;
}

s32 PS4_SYSV_ABI sceAudioOutOpen(s32 user_id, s32 port_type, s32 index, u32 length, u32 sample_rate,
                                 u32 param_type) {
    audio_out_open_call_t call;
    const u32 format = ORBIS_AUDIO_OUT_PARAM_DATA_FORMAT(param_type);
    const u32 attr = ORBIS_AUDIO_OUT_PARAM_ATTRIBUTES(param_type);
    bool is_restricted;
    bool is_mix_to_main;

    c_log(C_LOG_LIB_AUDIO_OUT, C_LOG_LEVEL_INFO, "sceAudioOutOpen",
          "called, user_id=%d, port_type=%s(%u), index=%d, length=%u, "
          "sample_rate=%u, data_format=%s(%u), attributes=%s(%u)",
          user_id, enum_name(&enum_names_OrbisAudioOutPort, (int64_t)port_type), (u32)port_type,
          index, length, sample_rate,
          enum_name(&enum_names_OrbisAudioOutParamFormat, (int64_t)format), format,
          enum_name(&enum_names_OrbisAudioOutParamAttr, (int64_t)attr), attr);

    if (!audio_out_ready()) {
        c_log(C_LOG_LIB_AUDIO_OUT, C_LOG_LEVEL_ERROR, "sceAudioOutOpen",
              "Audio out not initialized");
        return ORBIS_AUDIO_OUT_ERROR_NOT_INIT;
    }

    if (length == 0 || length > 2048 || (length & 0xFF) != 0) {
        c_log(C_LOG_LIB_AUDIO_OUT, C_LOG_LEVEL_ERROR, "sceAudioOutOpen", "Invalid size");
        return ORBIS_AUDIO_OUT_ERROR_INVALID_SIZE;
    }

    /* Extract attributes */
    is_restricted = (param_type & ORBIS_AUDIO_OUT_PARAM_ATTR_RESTRICTED) != 0;
    is_mix_to_main = (param_type & ORBIS_AUDIO_OUT_PARAM_ATTR_MIX_TO_MAIN) != 0;

    if (port_type != 3 && is_mix_to_main) {
        c_log(C_LOG_LIB_AUDIO_OUT, C_LOG_LEVEL_ERROR, "sceAudioOutOpen", "Invalid format");
        return ORBIS_AUDIO_OUT_ERROR_INVALID_FORMAT;
    }

    if (port_type != 0 && (param_type & 0x70000000) != 0) {
        c_log(C_LOG_LIB_AUDIO_OUT, C_LOG_LEVEL_ERROR, "sceAudioOutOpen", "Invalid format");
        return ORBIS_AUDIO_OUT_ERROR_INVALID_FORMAT;
    }

    if ((port_type < ORBIS_AUDIO_OUT_PORT_MAIN || port_type > ORBIS_AUDIO_OUT_PORT_PAD_SPK) &&
        (port_type != ORBIS_AUDIO_OUT_PORT_AUDIO3D && port_type != ORBIS_AUDIO_OUT_PORT_AUX)) {
        c_log(C_LOG_LIB_AUDIO_OUT, C_LOG_LEVEL_ERROR, "sceAudioOutOpen", "Invalid port type");
        return ORBIS_AUDIO_OUT_ERROR_INVALID_PORT_TYPE;
    }
    if (sample_rate != 48000) {
        c_log(C_LOG_LIB_AUDIO_OUT, C_LOG_LEVEL_ERROR, "sceAudioOutOpen", "Invalid sample rate");
        return ORBIS_AUDIO_OUT_ERROR_INVALID_SAMPLE_FREQ;
    }

    if (index != 0) {
        c_log(C_LOG_LIB_AUDIO_OUT, C_LOG_LEVEL_ERROR, "sceAudioOutOpen",
              "index is not valid !=0 %d", index);
    }

    if (format > ORBIS_AUDIO_OUT_PARAM_FORMAT_FLOAT_8CH_STD) {
        c_log(C_LOG_LIB_AUDIO_OUT, C_LOG_LEVEL_ERROR, "sceAudioOutOpen", "Invalid format");
        return ORBIS_AUDIO_OUT_ERROR_INVALID_FORMAT;
    }
    if (attr > ORBIS_AUDIO_OUT_PARAM_ATTR_VALUE_MIX_TO_MAIN) {
        /* TODO Handle attributes in output audio device */
        c_log(C_LOG_LIB_AUDIO_OUT, C_LOG_LEVEL_ERROR, "sceAudioOutOpen",
              "Invalid format attribute");
        return ORBIS_AUDIO_OUT_ERROR_INVALID_FORMAT;
    }

    call.user_id = user_id;
    call.type = port_type;
    call.format = format;
    call.length = length;
    call.sample_rate = sample_rate;
    call.is_restricted = is_restricted;
    call.is_mix_to_main = is_mix_to_main;
    call.port_id = -1;
    call.open = 0;
    call.result = 0;
    combiner_run(&port_allocation, audio_out_open_locked, &call);
    return call.result;
}

static void audio_out_take_locked(void* ctx) {
    audio_out_call_t* call = (audio_out_call_t*)ctx;
    call->open = retro_atomic_exchange_int(&call->port->open, 0);
}

/* Under the port's writers and control: its backend taken down, if still the open's */
static void audio_out_reset_locked(void* ctx) {
    audio_out_call_t* call = (audio_out_call_t*)ctx;
    if (call->port->impl_open == call->open) {
        frontend_audio_out_close(&call->port->impl);
        call->port->impl_open = 0;
    }
}

static void audio_out_reset_writers(void* ctx) {
    audio_out_call_t* call = (audio_out_call_t*)ctx;
    combiner_run(&call->port->control, audio_out_reset_locked, call);
}

s32 PS4_SYSV_ABI sceAudioOutClose(s32 handle) {
    audio_out_call_t call;
    int port_id;

    c_log(C_LOG_LIB_AUDIO_OUT, C_LOG_LEVEL_INFO, "sceAudioOutClose", "handle = 0x%x", handle);

    if (!audio_out_ready()) {
        c_log(C_LOG_LIB_AUDIO_OUT, C_LOG_LEVEL_ERROR, "sceAudioOutClose",
              "Audio out not initialized");
        return ORBIS_AUDIO_OUT_ERROR_NOT_INIT;
    }

    port_id = GetPortId(handle);
    if (port_id < 0) {
        c_log(C_LOG_LIB_AUDIO_OUT, C_LOG_LEVEL_ERROR, "sceAudioOutClose",
              "Invalid port id for handle 0x%x", handle);
        return port_id;
    }

    if (!IsValidHandleType(GetPortType(handle))) {
        c_log(C_LOG_LIB_AUDIO_OUT, C_LOG_LEVEL_ERROR, "sceAudioOutClose", "Invalid port type");
        return ORBIS_AUDIO_OUT_ERROR_INVALID_PORT_TYPE;
    }

    call.port = &port_table[port_id];
    call.open = 0;
    combiner_run(&port_allocation, audio_out_take_locked, &call);

    if (!call.open) {
        c_log(C_LOG_LIB_AUDIO_OUT, C_LOG_LEVEL_ERROR, "sceAudioOutClose", "Port wasn't open %d",
              port_id);
        return ORBIS_AUDIO_OUT_ERROR_NOT_OPENED;
    }

    c_log(C_LOG_LIB_AUDIO_OUT, C_LOG_LEVEL_DEBUG, "sceAudioOutClose", "Closed audio port %d",
          port_id);

    /* A call that comes later sees it closed; one under way is let finish first. */
    combiner_run(&call.port->writers, audio_out_reset_writers, &call);
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceAudioOutGetLastOutputTime(s32 handle, u64* output_time) {
    audio_out_call_t call;
    int port_id;

    if (!audio_out_ready()) {
        c_log(C_LOG_LIB_AUDIO_OUT, C_LOG_LEVEL_ERROR, "sceAudioOutGetLastOutputTime",
              "audio is not init");
        return ORBIS_AUDIO_OUT_ERROR_NOT_INIT;
    }

    port_id = GetPortId(handle);
    if (port_id < 0) {
        c_log(C_LOG_LIB_AUDIO_OUT, C_LOG_LEVEL_DEBUG, "sceAudioOutGetLastOutputTime",
              "Invalid port id for handle 0x%x", handle);
        return port_id;
    }

    if (!output_time) {
        c_log(C_LOG_LIB_AUDIO_OUT, C_LOG_LEVEL_ERROR, "sceAudioOutGetLastOutputTime",
              "Invalid pointer");
        return ORBIS_AUDIO_OUT_ERROR_INVALID_POINTER;
    }

    if (!audio_out_find(port_id, &call)) {
        c_log(C_LOG_LIB_AUDIO_OUT, C_LOG_LEVEL_ERROR, "sceAudioOutGetLastOutputTime",
              "Port not opened %d", port_id);
        return ORBIS_AUDIO_OUT_ERROR_NOT_OPENED;
    }

    *output_time = (u64)retro_atomic_load_relaxed_64(&call.port->last_output_time);

    return ORBIS_OK;
}

typedef struct audio_out_shape_call {
    audio_out_call_t c;
    bool found;
    s32 type;
    u32 num_channels;
    u32 buffer_frames;
} audio_out_shape_call_t;

/* Under the port's control: the open's type, channels and buffer size */
static void audio_out_shape_locked(void* ctx) {
    audio_out_shape_call_t* call = (audio_out_shape_call_t*)ctx;
    PortOut* port = call->c.port;
    call->found = port->impl_open == call->c.open;
    if (call->found) {
        call->type = port->type;
        call->num_channels = port->format_info.num_channels;
        call->buffer_frames = port->buffer_frames;
    }
}

/* The shape of the port the call found open; false when it has been opened again since */
static bool audio_out_shape(audio_out_shape_call_t* call) {
    combiner_run(&call->c.port->control, audio_out_shape_locked, call);
    return call->found;
}

s32 PS4_SYSV_ABI sceAudioOutGetPortState(s32 handle, OrbisAudioOutPortState* state) {
    audio_out_shape_call_t call;
    int port_id;

    if (!audio_out_ready()) {
        c_log(C_LOG_LIB_AUDIO_OUT, C_LOG_LEVEL_ERROR, "sceAudioOutGetPortState",
              "audio is not init");
        return ORBIS_AUDIO_OUT_ERROR_NOT_INIT;
    }

    port_id = GetPortId(handle);
    if (port_id < 0) {
        c_log(C_LOG_LIB_AUDIO_OUT, C_LOG_LEVEL_DEBUG, "sceAudioOutGetPortState",
              "Invalid port id for handle 0x%x", handle);
        return port_id;
    }

    if (!state) {
        c_log(C_LOG_LIB_AUDIO_OUT, C_LOG_LEVEL_ERROR, "sceAudioOutGetPortState", "Invalid pointer");
        return ORBIS_AUDIO_OUT_ERROR_INVALID_POINTER;
    }

    if (!audio_out_find(port_id, &call.c) || !audio_out_shape(&call)) {
        c_log(C_LOG_LIB_AUDIO_OUT, C_LOG_LEVEL_ERROR, "sceAudioOutGetPortState",
              "Port is not open %d", port_id);
        return ORBIS_AUDIO_OUT_ERROR_NOT_OPENED;
    }

    switch (call.type) {
    case ORBIS_AUDIO_OUT_PORT_MAIN:
    case ORBIS_AUDIO_OUT_PORT_BGM:
    case ORBIS_AUDIO_OUT_PORT_AUDIO3D:
        state->output = ORBIS_AUDIO_OUT_STATE_OUTPUT_CONNECTED_PRIMARY;
        state->channel = (u8)(call.num_channels > 2 ? 2 : call.num_channels);
        break;
    case ORBIS_AUDIO_OUT_PORT_VOICE:
    case ORBIS_AUDIO_OUT_PORT_PERSONAL:
        state->output = ORBIS_AUDIO_OUT_STATE_OUTPUT_CONNECTED_HEADPHONE;
        state->channel = 1;
        break;
    case ORBIS_AUDIO_OUT_PORT_PAD_SPK:
        state->output = ORBIS_AUDIO_OUT_STATE_OUTPUT_CONNECTED_TERTIARY;
        state->channel = 1;
        state->volume = 127; /* max */
        break;
    case ORBIS_AUDIO_OUT_PORT_AUX:
        state->output = ORBIS_AUDIO_OUT_STATE_OUTPUT_CONNECTED_EXTERNAL;
        state->channel = 0;
        break;
    default:
        state->output = ORBIS_AUDIO_OUT_STATE_OUTPUT_UNKNOWN;
        state->channel = 0;
        break;
    }

    if (call.type != ORBIS_AUDIO_OUT_PORT_PAD_SPK) {
        state->volume = -1; /* invalid */
    }

    state->rerouteCounter = 0;
    state->flag = 0;
    c_log(C_LOG_LIB_AUDIO_OUT, C_LOG_LEVEL_DEBUG, "sceAudioOutGetPortState",
          "called, handle=0x%x, state=0x%llx, output=%u, channel=%u, volume=%d, "
          "rerouteCounter=%u, flag=%llu",
          handle, (unsigned long long)(uintptr_t)state, state->output, state->channel,
          state->volume, state->rerouteCounter, (unsigned long long)state->flag);
    return ORBIS_OK;
}

typedef struct audio_out_output_call {
    audio_out_call_t c;
    const void* ptr;
    int port_id;
    s32 result;
} audio_out_output_call_t;

/* Among the port's writers */
static void audio_out_output_locked(void* ctx) {
    audio_out_output_call_t* call = (audio_out_output_call_t*)ctx;
    PortOut* port = call->c.port;
    if (call->ptr == NULL) {
        /* Nothing is left to drain: the last buffer went to the mix in its own call */
        call->result = retro_atomic_load_acquire_int(&port->open) != call->c.open
                           ? ORBIS_AUDIO_OUT_ERROR_NOT_OPENED
                           : 0;
        return;
    }
    /* The port opened again since the call found it: the open it found is gone */
    if (port->impl_open != call->c.open) {
        c_log(C_LOG_LIB_AUDIO_OUT, C_LOG_LEVEL_DEBUG, "sceAudioOutOutput", "Port %d closed",
              call->port_id);
        call->result = ORBIS_AUDIO_OUT_ERROR_NOT_OPENED;
        return;
    }
    PaceOutput(port);
    if (!OutputBuffer(&call->c, call->ptr)) {
        c_log(C_LOG_LIB_AUDIO_OUT, C_LOG_LEVEL_DEBUG, "sceAudioOutOutput", "Port %d closed",
              call->port_id);
        call->result = ORBIS_AUDIO_OUT_ERROR_NOT_OPENED;
        return;
    }
    call->result = (s32)(port->buffer_frames * port->format_info.num_channels);
}

s32 PS4_SYSV_ABI sceAudioOutOutput(s32 handle, void* ptr) {
    audio_out_output_call_t call;
    int port_id;

#ifndef NDEBUG
    c_log(C_LOG_LIB_AUDIO_OUT, C_LOG_LEVEL_TRACE, "sceAudioOutOutput",
          "(STUBBED) called, handle=0x%x, ptr=0x%llx", handle, (unsigned long long)(uintptr_t)ptr);
#endif

    if (!audio_out_ready()) {
        c_log(C_LOG_LIB_AUDIO_OUT, C_LOG_LEVEL_ERROR, "sceAudioOutOutput", "audio is not init");
        return ORBIS_AUDIO_OUT_ERROR_NOT_INIT;
    }

    port_id = GetPortId(handle);
    if (port_id < 0) {
        c_log(C_LOG_LIB_AUDIO_OUT, C_LOG_LEVEL_ERROR, "sceAudioOutOutput",
              "invalid port id for handle 0x%x", handle);
        return port_id;
    }

    if (!IsValidHandleType(GetPortType(handle))) {
        c_log(C_LOG_LIB_AUDIO_OUT, C_LOG_LEVEL_ERROR, "sceAudioOutOutput", "Invalid port type");
        return ORBIS_AUDIO_OUT_ERROR_INVALID_PORT_TYPE;
    }

    if (!audio_out_find(port_id, &call.c)) {
        c_log(C_LOG_LIB_AUDIO_OUT, C_LOG_LEVEL_ERROR, "sceAudioOutOutput", "Port is not opened %d",
              port_id);
        return ORBIS_AUDIO_OUT_ERROR_NOT_OPENED;
    }

    call.ptr = ptr;
    call.port_id = port_id;
    call.result = 0;
    combiner_run(&call.c.port->writers, audio_out_output_locked, &call);
    return call.result;
}

typedef struct audio_out_outputs_call {
    audio_out_call_t c;
    const void* ptr;
    bool* paced;
    bool ok;
} audio_out_outputs_call_t;

/* Among the port's writers */
static void audio_out_outputs_locked(void* ctx) {
    audio_out_outputs_call_t* call = (audio_out_outputs_call_t*)ctx;
    if (call->c.port->impl_open != call->c.open) {
        call->ok = false;
        return;
    }
    if (!*call->paced) {
        *call->paced = true;
        PaceOutput(call->c.port);
    }
    call->ok = OutputBuffer(&call->c, call->ptr);
}

s32 PS4_SYSV_ABI sceAudioOutOutputs(OrbisAudioOutOutputParam* param, u32 num) {
    /* At most 25, checked below */
    audio_out_call_t ports[ORBIS_AUDIO_OUT_NUM_PORTS];
    u32 buffer_frames = 0;
    bool paced = false;
    u32 i;

#ifndef NDEBUG
    if (param) {
        c_log(C_LOG_LIB_AUDIO_OUT, C_LOG_LEVEL_TRACE, "sceAudioOutOutputs",
              "(STUBBED) called, param=0x%llx, num=%u", (unsigned long long)(uintptr_t)param, num);
        for (i = 0; i < num; i++) {
            c_log(C_LOG_LIB_AUDIO_OUT, C_LOG_LEVEL_TRACE, "sceAudioOutOutputs",
                  "  [%u] handle=0x%x, ptr=0x%llx", i, param[i].handle,
                  (unsigned long long)(uintptr_t)param[i].ptr);
        }
    } else {
        c_log(C_LOG_LIB_AUDIO_OUT, C_LOG_LEVEL_TRACE, "sceAudioOutOutputs",
              "(STUBBED) called, param=nullptr, num=%u", num);
    }
#endif
    if (!audio_out_ready()) {
        c_log(C_LOG_LIB_AUDIO_OUT, C_LOG_LEVEL_ERROR, "sceAudioOutOutputs", "audio is not init");
        return ORBIS_AUDIO_OUT_ERROR_NOT_INIT;
    }

    if (num == 0 || num > 25) {
        c_log(C_LOG_LIB_AUDIO_OUT, C_LOG_LEVEL_ERROR, "sceAudioOutOutputs", "ports is full");
        return ORBIS_AUDIO_OUT_ERROR_PORT_FULL;
    }

    if (!param) {
        c_log(C_LOG_LIB_AUDIO_OUT, C_LOG_LEVEL_ERROR, "sceAudioOutOutputs", "invalid pointer");
        return ORBIS_AUDIO_OUT_ERROR_INVALID_POINTER;
    }

    for (i = 0; i < num; i++) {
        audio_out_shape_call_t shape;
        u32 j;
        int port_id = GetPortId(param[i].handle);
        if (port_id < 0) {
            c_log(C_LOG_LIB_AUDIO_OUT, C_LOG_LEVEL_ERROR, "sceAudioOutOutputs", "invalid port id");
            return port_id;
        }

        if (!IsValidHandleType(GetPortType(param[i].handle))) {
            c_log(C_LOG_LIB_AUDIO_OUT, C_LOG_LEVEL_ERROR, "sceAudioOutOutputs",
                  "Invalid port type");
            return ORBIS_AUDIO_OUT_ERROR_INVALID_PORT_TYPE;
        }

        /* Check for duplicate handles */
        for (j = 0; j < i; j++) {
            if (param[i].handle == param[j].handle) {
                c_log(C_LOG_LIB_AUDIO_OUT, C_LOG_LEVEL_ERROR, "sceAudioOutOutputs",
                      "Duplicate audio handles: 0x%x", param[i].handle);
                return ORBIS_AUDIO_OUT_ERROR_INVALID_PORT;
            }
        }

        /* Get port */
        if (!audio_out_find(port_id, &shape.c) || !audio_out_shape(&shape)) {
            c_log(C_LOG_LIB_AUDIO_OUT, C_LOG_LEVEL_ERROR, "sceAudioOutOutputs",
                  "Port not opened %d", port_id);
            return ORBIS_AUDIO_OUT_ERROR_NOT_OPENED;
        }

        ports[i] = shape.c;

        /* Check consistent buffer size */
        if (i == 0) {
            buffer_frames = shape.buffer_frames;
        } else if (shape.buffer_frames != buffer_frames) {
            c_log(C_LOG_LIB_AUDIO_OUT, C_LOG_LEVEL_ERROR, "sceAudioOutOutputs",
                  "Invalid port size");
            return ORBIS_AUDIO_OUT_ERROR_INVALID_SIZE;
        }
    }

    /* Each buffer to its port's mix, among the port's writers, from where the game wrote it.
     * One buffer's time for the call, not one a port: the ports are fed together (FMOD's main
     * and BGM), so pacing each would halve the rate. The first port given a buffer paces it. */
    for (i = 0; i < num; i++) {
        audio_out_outputs_call_t call;
        if (param[i].ptr == NULL) {
            continue;
        }
        call.c = ports[i];
        call.ptr = param[i].ptr;
        call.paced = &paced;
        call.ok = false;
        combiner_run(&call.c.port->writers, audio_out_outputs_locked, &call);
        if (!call.ok) {
            c_log(C_LOG_LIB_AUDIO_OUT, C_LOG_LEVEL_DEBUG, "sceAudioOutOutputs", "Port closed");
            return ORBIS_AUDIO_OUT_ERROR_NOT_OPENED;
        }
    }

    return (s32)buffer_frames;
}

typedef struct audio_out_volume_call {
    audio_out_call_t c;
    s32 flag;
    s32 value;
} audio_out_volume_call_t;

/* Under the port's control */
static void SetPortVolume(void* ctx) {
    audio_out_volume_call_t* call = (audio_out_volume_call_t*)ctx;
    PortOut* port = call->c.port;
    const s32 flag = call->flag;
    if (port->impl_open != call->c.open) {
        return;
    }
    /* Set volumes based on flags */
    if (flag & ORBIS_AUDIO_VOLUME_FLAG_L_CH)
        port->volume[0] = call->value;
    if (flag & ORBIS_AUDIO_VOLUME_FLAG_R_CH)
        port->volume[1] = call->value;
    if (flag & ORBIS_AUDIO_VOLUME_FLAG_C_CH)
        port->volume[2] = call->value;
    if (flag & ORBIS_AUDIO_VOLUME_FLAG_LFE_CH)
        port->volume[3] = call->value;
    if (flag & ORBIS_AUDIO_VOLUME_FLAG_LS_CH)
        port->volume[4] = call->value;
    if (flag & ORBIS_AUDIO_VOLUME_FLAG_RS_CH)
        port->volume[5] = call->value;
    if (flag & ORBIS_AUDIO_VOLUME_FLAG_LE_CH)
        port->volume[6] = call->value;
    if (flag & ORBIS_AUDIO_VOLUME_FLAG_RE_CH)
        port->volume[7] = call->value;

    frontend_audio_out_set_volume(&port->impl, port->volume);
}

s32 PS4_SYSV_ABI sceAudioOutSetVolume(s32 handle, s32 flag, s32* vol) {
    audio_out_volume_call_t call;
    int port_id;

    if (!audio_out_ready()) {
        c_log(C_LOG_LIB_AUDIO_OUT, C_LOG_LEVEL_ERROR, "sceAudioOutSetVolume", "audio is not init");
        return ORBIS_AUDIO_OUT_ERROR_NOT_INIT;
    }

    port_id = GetPortId(handle);
    if (port_id < 0) {
        c_log(C_LOG_LIB_AUDIO_OUT, C_LOG_LEVEL_ERROR, "sceAudioOutSetVolume", "invalid port_id");
        return port_id;
    }

    if (!IsValidHandleType(GetPortType(handle))) {
        c_log(C_LOG_LIB_AUDIO_OUT, C_LOG_LEVEL_ERROR, "sceAudioOutSetVolume", "Invalid port type");
        return ORBIS_AUDIO_OUT_ERROR_INVALID_PORT_TYPE;
    }

    if (!vol) {
        c_log(C_LOG_LIB_AUDIO_OUT, C_LOG_LEVEL_ERROR, "sceAudioOutSetVolume", "Invalid pointer");
        return ORBIS_AUDIO_OUT_ERROR_INVALID_POINTER;
    }

    if (*vol > ORBIS_AUDIO_OUT_VOLUME_0DB) {
        c_log(C_LOG_LIB_AUDIO_OUT, C_LOG_LEVEL_ERROR, "sceAudioOutSetVolume", "Invalid volume");
        return ORBIS_AUDIO_OUT_ERROR_INVALID_VOLUME;
    }

    if (!audio_out_find(port_id, &call.c)) {
        c_log(C_LOG_LIB_AUDIO_OUT, C_LOG_LEVEL_ERROR, "sceAudioOutSetVolume", "Port not opened %d",
              port_id);
        return ORBIS_AUDIO_OUT_ERROR_NOT_OPENED;
    }

    call.flag = flag;
    call.value = *vol;
    combiner_run(&call.c.port->control, SetPortVolume, &call);
    return ORBIS_OK;
}

typedef struct audio_out_mix_level_call {
    audio_out_call_t c;
    s32 mix_level;
} audio_out_mix_level_call_t;

/* Under the port's control */
static void audio_out_mix_level_locked(void* ctx) {
    audio_out_mix_level_call_t* call = (audio_out_mix_level_call_t*)ctx;
    if (call->c.port->impl_open == call->c.open) {
        call->c.port->mixLevelPadSpk = call->mix_level;
    }
}

s32 PS4_SYSV_ABI sceAudioOutSetMixLevelPadSpk(s32 handle, s32 mixLevel) {
    audio_out_mix_level_call_t call;
    int port_id;

    c_log(C_LOG_LIB_AUDIO_OUT, C_LOG_LEVEL_INFO, "sceAudioOutSetMixLevelPadSpk",
          "(STUBBED) called");
    if (!audio_out_ready()) {
        c_log(C_LOG_LIB_AUDIO_OUT, C_LOG_LEVEL_ERROR, "sceAudioOutSetMixLevelPadSpk",
              "audio is not init");
        return ORBIS_AUDIO_OUT_ERROR_NOT_INIT;
    }

    port_id = GetPortId(handle);
    if (port_id < 0) {
        c_log(C_LOG_LIB_AUDIO_OUT, C_LOG_LEVEL_ERROR, "sceAudioOutSetMixLevelPadSpk",
              "Invalid port_id");
        return port_id;
    }

    if (GetPortType(handle) != 4) { /* PadSpk */
        c_log(C_LOG_LIB_AUDIO_OUT, C_LOG_LEVEL_ERROR, "sceAudioOutSetMixLevelPadSpk",
              "Invalid port type");
        return ORBIS_AUDIO_OUT_ERROR_INVALID_PORT_TYPE;
    }

    if (mixLevel > ORBIS_AUDIO_OUT_VOLUME_0DB) {
        c_log(C_LOG_LIB_AUDIO_OUT, C_LOG_LEVEL_ERROR, "sceAudioOutSetMixLevelPadSpk",
              "Invalid mix level");
        return ORBIS_AUDIO_OUT_ERROR_INVALID_MIXLEVEL;
    }

    if (!audio_out_find(port_id, &call.c)) {
        c_log(C_LOG_LIB_AUDIO_OUT, C_LOG_LEVEL_ERROR, "sceAudioOutSetMixLevelPadSpk",
              "Port not opened");
        return ORBIS_AUDIO_OUT_ERROR_NOT_OPENED;
    }

    call.mix_level = mixLevel;
    combiner_run(&call.c.port->control, audio_out_mix_level_locked, &call);
    /* TODO: Apply mix level to backend */

    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceAudioOutGetSystemState(OrbisAudioOutSystemState* state) {
    if (!audio_out_ready()) {
        c_log(C_LOG_LIB_AUDIO_OUT, C_LOG_LEVEL_ERROR, "sceAudioOutGetSystemState",
              "audio is not init");
        return ORBIS_AUDIO_OUT_ERROR_NOT_INIT;
    }
    if (state == NULL) {
        return ORBIS_AUDIO_OUT_ERROR_INVALID_POINTER;
    }
    memset(state, 0, sizeof(*state));
    c_log(C_LOG_LIB_AUDIO_OUT, C_LOG_LEVEL_DEBUG, "sceAudioOutGetSystemState", "called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceAudioOutMasteringInit(u32 flags) {
    c_log(C_LOG_LIB_AUDIO_OUT, C_LOG_LEVEL_ERROR, "sceAudioOutMasteringInit", "(STUBBED) called");
    if (flags != 0) {
        return ORBIS_AUDIO_OUT_ERROR_MASTERING_INVALID_API_PARAM;
    }
    return ORBIS_OK;
}

/*
 * Stubbed functions
 **/
s32 PS4_SYSV_ABI sceAudioOutDeviceIdOpen(void) {
    c_log(C_LOG_LIB_AUDIO_OUT, C_LOG_LEVEL_ERROR, "sceAudioOutDeviceIdOpen", "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceAudioDeviceControlGet(void) {
    c_log(C_LOG_LIB_AUDIO_OUT, C_LOG_LEVEL_ERROR, "sceAudioDeviceControlGet", "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceAudioDeviceControlSet(void) {
    c_log(C_LOG_LIB_AUDIO_OUT, C_LOG_LEVEL_ERROR, "sceAudioDeviceControlSet", "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceAudioOutA3dControl(void) {
    c_log(C_LOG_LIB_AUDIO_OUT, C_LOG_LEVEL_ERROR, "sceAudioOutA3dControl", "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceAudioOutA3dExit(void) {
    c_log(C_LOG_LIB_AUDIO_OUT, C_LOG_LEVEL_ERROR, "sceAudioOutA3dExit", "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceAudioOutA3dInit(void) {
    c_log(C_LOG_LIB_AUDIO_OUT, C_LOG_LEVEL_ERROR, "sceAudioOutA3dInit", "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceAudioOutAttachToApplicationByPid(void) {
    c_log(C_LOG_LIB_AUDIO_OUT, C_LOG_LEVEL_ERROR, "sceAudioOutAttachToApplicationByPid",
          "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceAudioOutChangeAppModuleState(void) {
    c_log(C_LOG_LIB_AUDIO_OUT, C_LOG_LEVEL_ERROR, "sceAudioOutChangeAppModuleState",
          "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceAudioOutDetachFromApplicationByPid(void) {
    c_log(C_LOG_LIB_AUDIO_OUT, C_LOG_LEVEL_ERROR, "sceAudioOutDetachFromApplicationByPid",
          "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceAudioOutExConfigureOutputMode(void) {
    c_log(C_LOG_LIB_AUDIO_OUT, C_LOG_LEVEL_ERROR, "sceAudioOutExConfigureOutputMode",
          "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceAudioOutExGetSystemInfo(void) {
    c_log(C_LOG_LIB_AUDIO_OUT, C_LOG_LEVEL_ERROR, "sceAudioOutExGetSystemInfo", "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceAudioOutExPtClose(void) {
    c_log(C_LOG_LIB_AUDIO_OUT, C_LOG_LEVEL_ERROR, "sceAudioOutExPtClose", "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceAudioOutExPtGetLastOutputTime(void) {
    c_log(C_LOG_LIB_AUDIO_OUT, C_LOG_LEVEL_ERROR, "sceAudioOutExPtGetLastOutputTime",
          "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceAudioOutExPtOpen(void) {
    c_log(C_LOG_LIB_AUDIO_OUT, C_LOG_LEVEL_ERROR, "sceAudioOutExPtOpen", "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceAudioOutExSystemInfoIsSupportedAudioOutExMode(void) {
    c_log(C_LOG_LIB_AUDIO_OUT, C_LOG_LEVEL_ERROR,
          "sceAudioOutExSystemInfoIsSupportedAudioOutExMode", "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceAudioOutGetFocusEnablePid(void) {
    c_log(C_LOG_LIB_AUDIO_OUT, C_LOG_LEVEL_ERROR, "sceAudioOutGetFocusEnablePid",
          "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceAudioOutGetHandleStatusInfo(void) {
    c_log(C_LOG_LIB_AUDIO_OUT, C_LOG_LEVEL_ERROR, "sceAudioOutGetHandleStatusInfo",
          "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceAudioOutGetInfo(void) {
    c_log(C_LOG_LIB_AUDIO_OUT, C_LOG_LEVEL_ERROR, "sceAudioOutGetInfo", "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceAudioOutGetInfoOpenNum(void) {
    c_log(C_LOG_LIB_AUDIO_OUT, C_LOG_LEVEL_ERROR, "sceAudioOutGetInfoOpenNum", "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceAudioOutOpenEx(void) {
    c_log(C_LOG_LIB_AUDIO_OUT, C_LOG_LEVEL_ERROR, "sceAudioOutOpenEx", "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceAudioOutGetSimulatedBusUsableStatusByBusType(void) {
    c_log(C_LOG_LIB_AUDIO_OUT, C_LOG_LEVEL_ERROR, "sceAudioOutGetSimulatedBusUsableStatusByBusType",
          "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceAudioOutGetSimulatedHandleStatusInfo(void) {
    c_log(C_LOG_LIB_AUDIO_OUT, C_LOG_LEVEL_ERROR, "sceAudioOutGetSimulatedHandleStatusInfo",
          "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceAudioOutGetSimulatedHandleStatusInfo2(void) {
    c_log(C_LOG_LIB_AUDIO_OUT, C_LOG_LEVEL_ERROR, "sceAudioOutGetSimulatedHandleStatusInfo2",
          "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceAudioOutGetSparkVss(void) {
    c_log(C_LOG_LIB_AUDIO_OUT, C_LOG_LEVEL_ERROR, "sceAudioOutGetSparkVss", "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceAudioOutInitIpmiGetSession(void) {
    c_log(C_LOG_LIB_AUDIO_OUT, C_LOG_LEVEL_ERROR, "sceAudioOutInitIpmiGetSession",
          "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceAudioOutMasteringGetState(void) {
    c_log(C_LOG_LIB_AUDIO_OUT, C_LOG_LEVEL_ERROR, "sceAudioOutMasteringGetState",
          "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceAudioOutMasteringSetParam(void) {
    c_log(C_LOG_LIB_AUDIO_OUT, C_LOG_LEVEL_ERROR, "sceAudioOutMasteringSetParam",
          "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceAudioOutMasteringTerm(void) {
    c_log(C_LOG_LIB_AUDIO_OUT, C_LOG_LEVEL_ERROR, "sceAudioOutMasteringTerm", "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceAudioOutMbusInit(void) {
    c_log(C_LOG_LIB_AUDIO_OUT, C_LOG_LEVEL_ERROR, "sceAudioOutMbusInit", "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceAudioOutPtClose(void) {
    c_log(C_LOG_LIB_AUDIO_OUT, C_LOG_LEVEL_ERROR, "sceAudioOutPtClose", "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceAudioOutPtGetLastOutputTime(void) {
    c_log(C_LOG_LIB_AUDIO_OUT, C_LOG_LEVEL_ERROR, "sceAudioOutPtGetLastOutputTime",
          "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceAudioOutPtOpen(void) {
    c_log(C_LOG_LIB_AUDIO_OUT, C_LOG_LEVEL_ERROR, "sceAudioOutPtOpen", "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceAudioOutSetConnections(void) {
    c_log(C_LOG_LIB_AUDIO_OUT, C_LOG_LEVEL_ERROR, "sceAudioOutSetConnections", "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceAudioOutSetConnectionsForUser(void) {
    c_log(C_LOG_LIB_AUDIO_OUT, C_LOG_LEVEL_ERROR, "sceAudioOutSetConnectionsForUser",
          "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceAudioOutSetDevConnection(void) {
    c_log(C_LOG_LIB_AUDIO_OUT, C_LOG_LEVEL_ERROR, "sceAudioOutSetDevConnection",
          "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceAudioOutSetHeadphoneOutMode(void) {
    c_log(C_LOG_LIB_AUDIO_OUT, C_LOG_LEVEL_ERROR, "sceAudioOutSetHeadphoneOutMode",
          "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceAudioOutSetJediJackVolume(void) {
    c_log(C_LOG_LIB_AUDIO_OUT, C_LOG_LEVEL_ERROR, "sceAudioOutSetJediJackVolume",
          "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceAudioOutSetJediSpkVolume(void) {
    c_log(C_LOG_LIB_AUDIO_OUT, C_LOG_LEVEL_ERROR, "sceAudioOutSetJediSpkVolume",
          "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceAudioOutSetMainOutput(void) {
    c_log(C_LOG_LIB_AUDIO_OUT, C_LOG_LEVEL_ERROR, "sceAudioOutSetMainOutput", "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceAudioOutSetMorpheusParam(void) {
    c_log(C_LOG_LIB_AUDIO_OUT, C_LOG_LEVEL_ERROR, "sceAudioOutSetMorpheusParam",
          "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceAudioOutSetMorpheusWorkingMode(void) {
    c_log(C_LOG_LIB_AUDIO_OUT, C_LOG_LEVEL_ERROR, "sceAudioOutSetMorpheusWorkingMode",
          "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceAudioOutSetPortConnections(void) {
    c_log(C_LOG_LIB_AUDIO_OUT, C_LOG_LEVEL_ERROR, "sceAudioOutSetPortConnections",
          "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceAudioOutSetPortStatuses(void) {
    c_log(C_LOG_LIB_AUDIO_OUT, C_LOG_LEVEL_ERROR, "sceAudioOutSetPortStatuses", "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceAudioOutSetRecMode(void) {
    c_log(C_LOG_LIB_AUDIO_OUT, C_LOG_LEVEL_ERROR, "sceAudioOutSetRecMode", "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceAudioOutSetSparkParam(void) {
    c_log(C_LOG_LIB_AUDIO_OUT, C_LOG_LEVEL_ERROR, "sceAudioOutSetSparkParam", "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceAudioOutSetUsbVolume(void) {
    c_log(C_LOG_LIB_AUDIO_OUT, C_LOG_LEVEL_ERROR, "sceAudioOutSetUsbVolume", "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceAudioOutSetVolumeDown(void) {
    c_log(C_LOG_LIB_AUDIO_OUT, C_LOG_LEVEL_ERROR, "sceAudioOutSetVolumeDown", "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceAudioOutStartAuxBroadcast(void) {
    c_log(C_LOG_LIB_AUDIO_OUT, C_LOG_LEVEL_ERROR, "sceAudioOutStartAuxBroadcast",
          "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceAudioOutStartSharePlay(void) {
    c_log(C_LOG_LIB_AUDIO_OUT, C_LOG_LEVEL_ERROR, "sceAudioOutStartSharePlay", "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceAudioOutStopAuxBroadcast(void) {
    c_log(C_LOG_LIB_AUDIO_OUT, C_LOG_LEVEL_ERROR, "sceAudioOutStopAuxBroadcast",
          "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceAudioOutStopSharePlay(void) {
    c_log(C_LOG_LIB_AUDIO_OUT, C_LOG_LEVEL_ERROR, "sceAudioOutStopSharePlay", "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceAudioOutSuspendResume(void) {
    c_log(C_LOG_LIB_AUDIO_OUT, C_LOG_LEVEL_ERROR, "sceAudioOutSuspendResume", "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceAudioOutSysConfigureOutputMode(void) {
    c_log(C_LOG_LIB_AUDIO_OUT, C_LOG_LEVEL_ERROR, "sceAudioOutSysConfigureOutputMode",
          "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceAudioOutSysGetHdmiMonitorInfo(void) {
    c_log(C_LOG_LIB_AUDIO_OUT, C_LOG_LEVEL_ERROR, "sceAudioOutSysGetHdmiMonitorInfo",
          "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceAudioOutSysGetSystemInfo(void) {
    c_log(C_LOG_LIB_AUDIO_OUT, C_LOG_LEVEL_ERROR, "sceAudioOutSysGetSystemInfo",
          "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceAudioOutSysHdmiMonitorInfoIsSupportedAudioOutMode(void) {
    c_log(C_LOG_LIB_AUDIO_OUT, C_LOG_LEVEL_ERROR,
          "sceAudioOutSysHdmiMonitorInfoIsSupportedAudioOutMode", "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceAudioOutSystemControlGet(void) {
    c_log(C_LOG_LIB_AUDIO_OUT, C_LOG_LEVEL_ERROR, "sceAudioOutSystemControlGet",
          "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceAudioOutSystemControlSet(void) {
    c_log(C_LOG_LIB_AUDIO_OUT, C_LOG_LEVEL_ERROR, "sceAudioOutSystemControlSet",
          "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceAudioOutSparkControlSetEqCoef(void) {
    c_log(C_LOG_LIB_AUDIO_OUT, C_LOG_LEVEL_ERROR, "sceAudioOutSparkControlSetEqCoef",
          "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceAudioOutSetSystemDebugState(void) {
    c_log(C_LOG_LIB_AUDIO_OUT, C_LOG_LEVEL_ERROR, "sceAudioOutSetSystemDebugState",
          "(STUBBED) called");
    return ORBIS_OK;
}

/* The bridge for the libraries written in C (audioout_c.h) */
typedef char audioout_check_float_stereo
    [AUDIOOUT_FORMAT_FLOAT_STEREO == ORBIS_AUDIO_OUT_PARAM_FORMAT_FLOAT_STEREO ? 1 : -1];
typedef char audioout_check_float_8ch
    [AUDIOOUT_FORMAT_FLOAT_8CH == ORBIS_AUDIO_OUT_PARAM_FORMAT_FLOAT_8CH ? 1 : -1];
typedef char audioout_check_float_8ch_std
    [AUDIOOUT_FORMAT_FLOAT_8CH_STD == ORBIS_AUDIO_OUT_PARAM_FORMAT_FLOAT_8CH_STD ? 1 : -1];
typedef char audioout_check_audio3d[AUDIOOUT_PORT_AUDIO3D == ORBIS_AUDIO_OUT_PORT_AUDIO3D ? 1 : -1];
typedef char audioout_check_already_init
    [AUDIOOUT_ERROR_ALREADY_INIT == ORBIS_AUDIO_OUT_ERROR_ALREADY_INIT ? 1 : -1];
typedef char audioout_check_output_param
    [sizeof(audioout_output_param_t) == sizeof(OrbisAudioOutOutputParam) ? 1 : -1];

s32 audioout_init(void) {
    return sceAudioOutInit();
}

s32 audioout_open(s32 user_id, s32 port_type, s32 index, u32 length, u32 sample_rate, u32 param) {
    return sceAudioOutOpen(user_id, port_type, index, length, sample_rate, param);
}

s32 audioout_output(s32 handle, void* ptr) {
    return sceAudioOutOutput(handle, ptr);
}

s32 audioout_close(s32 handle) {
    return sceAudioOutClose(handle);
}
