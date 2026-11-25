/* SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <retro_atomic.h>
#include <string.h>

#include "common/c_bridge.h"
#include "common/combine.h"
#include "core/libraries/audio/audioin.h"
#include "core/libraries/audio/audioin_backend.h"
#include "core/libraries/audio/audioin_error.h"
#include "core/libraries/error_codes.h"

/* The ports, read by any thread with no lock; opening and closing run one at a time. Each
 * port lives for the program: nothing is allocated. */
static PortIn port_table[ORBIS_AUDIO_IN_NUM_PORTS];
static combiner_t port_allocation;
/* The last open's number (0 is none) */
static int open_count;

void audio_in_library_init(void) {
    int i;
    combiner_init(&port_allocation);
    for (i = 0; i < ORBIS_AUDIO_IN_NUM_PORTS; i++) {
        combiner_init(&port_table[i].access);
    }
}

/* The backend in use */
static bool audio_in_backend_open(audioin_port_backend_t* backend, const PortIn* port) {
    return silent_audio_in_open(backend, port);
}

/*
 * Helper functions
 **/
static int GetPortId(const char* func, s32 handle) {
    int port_id = handle & 0xFF;
    (void)func;

    if (port_id >= ORBIS_AUDIO_IN_NUM_PORTS) {
        c_log(C_LOG_LIB_AUDIO_IN, C_LOG_LEVEL_ERROR, "GetPortId", "Invalid port");
        return ORBIS_AUDIO_IN_ERROR_PORT_FULL;
    }

    if ((handle & 0x7f000000) != 0x30000000) {
        c_log(C_LOG_LIB_AUDIO_IN, C_LOG_LEVEL_ERROR, "GetPortId", "Invalid handle format");
        return ORBIS_AUDIO_IN_ERROR_INVALID_HANDLE;
    }

    return port_id;
}

static int AllocatePort(u32 type) {
    /* TODO implement port type ranges if needed */
    int i;
    (void)type;
    for (i = 0; i < ORBIS_AUDIO_IN_NUM_PORTS; i++) {
        if (retro_atomic_load_acquire_int(&port_table[i].open) == 0) {
            return i;
        }
    }
    return -1;
}

typedef struct audio_in_call {
    PortIn* port;
    int open;
    u32 type;
    u32 format;
    u32 len;
    u32 freq;
    void* dest;
    int result;
} audio_in_call_t;

/* Under the port's access: its fields and backend set for the open @call->open */
static void audio_in_setup_locked(void* ctx) {
    audio_in_call_t* call = (audio_in_call_t*)ctx;
    PortIn* port = call->port;
    port->type = call->type;
    port->format = call->format;
    port->samples_num = call->len;
    port->freq = call->freq;

    /* Determine channel count and sample size based on format */
    switch (call->format) {
    case ORBIS_AUDIO_IN_PARAM_FORMAT_S16_MONO:
        port->channels_num = 1;
        port->sample_size = 2;
        break;
    case ORBIS_AUDIO_IN_PARAM_FORMAT_S16_STEREO:
        port->channels_num = 2;
        port->sample_size = 2;
        break;
    default:
        c_log(C_LOG_LIB_AUDIO_IN, C_LOG_LEVEL_ERROR, "sceAudioInOpen",
              "Unsupported audio format: %u", call->format);
        call->result = ORBIS_AUDIO_IN_ERROR_INVALID_PARAM;
        return;
    }

    /* Open backend */
    memset(&port->impl, 0, sizeof(port->impl));
    if (!audio_in_backend_open(&port->impl, port)) {
        c_log(C_LOG_LIB_AUDIO_IN, C_LOG_LEVEL_ERROR, "sceAudioInOpen",
              "Failed to open audio input port: %s", "Failed to create audio backend");
        call->result = ORBIS_AUDIO_IN_ERROR_NOT_OPENED;
        return;
    }
    port->impl_open = call->open;
    call->result = ORBIS_OK;
}

static void audio_in_open_locked(void* ctx) {
    audio_in_call_t* call = (audio_in_call_t*)ctx;
    s32 handle;
    /* Allocate port */
    const int port_id = AllocatePort(call->type);
    if (port_id < 0) {
        c_log(C_LOG_LIB_AUDIO_IN, C_LOG_LEVEL_ERROR, "sceAudioInOpen",
              "No free audio input ports available");
        call->result = ORBIS_AUDIO_IN_ERROR_PORT_FULL;
        return;
    }

    call->port = &port_table[port_id];
    if (++open_count <= 0) {
        open_count = 1;
    }
    call->open = open_count;
    combiner_run(&call->port->access, audio_in_setup_locked, call);
    if (call->result != ORBIS_OK) {
        return;
    }
    retro_atomic_store_release_int(&call->port->open, call->open);

    /* Create handle */
    handle = (s32)((call->type << 16) | (u32)port_id | 0x30000000u);

    c_log(C_LOG_LIB_AUDIO_IN, C_LOG_LEVEL_INFO, "sceAudioInOpen",
          "Opened audio input port %d: type=%u, samples=%u, freq=%u, format=%u", handle, call->type,
          call->len, call->freq, call->format);
    call->result = handle;
}

/*
 * sceAudioIn implementation
 **/
int PS4_SYSV_ABI sceAudioInOpen(s32 userId, u32 type, u32 index, u32 len, u32 freq, u32 param) {
    audio_in_call_t call;
    c_log(C_LOG_LIB_AUDIO_IN, C_LOG_LEVEL_INFO, "sceAudioInOpen",
          "called, userId=%d, type=%u, index=%u, len=%u, freq=%u, param=%u", userId, type, index,
          len, freq, param);

    if (len == 0 || len > 2048) {
        c_log(C_LOG_LIB_AUDIO_IN, C_LOG_LEVEL_ERROR, "sceAudioInOpen", "Invalid size");
        return ORBIS_AUDIO_IN_ERROR_INVALID_SIZE;
    }

    /* Validate parameters */
    if (param != ORBIS_AUDIO_IN_PARAM_FORMAT_S16_MONO &&
        param != ORBIS_AUDIO_IN_PARAM_FORMAT_S16_STEREO) {
        c_log(C_LOG_LIB_AUDIO_IN, C_LOG_LEVEL_ERROR, "sceAudioInOpen", "Invalid format");
        return ORBIS_AUDIO_IN_ERROR_INVALID_PARAM;
    }

    if (freq != 16000 && freq != 48000) {
        c_log(C_LOG_LIB_AUDIO_IN, C_LOG_LEVEL_ERROR, "sceAudioInOpen", "Invalid sample rate");
        return ORBIS_AUDIO_IN_ERROR_INVALID_FREQ;
    }

    memset(&call, 0, sizeof(call));
    call.type = type;
    call.format = param;
    call.len = len;
    call.freq = freq;
    combiner_run(&port_allocation, audio_in_open_locked, &call);
    return call.result;
}

int PS4_SYSV_ABI sceAudioInHqOpen(s32 userId, u32 type, u32 index, u32 len, u32 freq, u32 param) {
    int result;
    c_log(C_LOG_LIB_AUDIO_IN, C_LOG_LEVEL_INFO, "sceAudioInHqOpen",
          "called, userId=%d, type=%u, index=%u, len=%u, freq=%u, param=%u", userId, type, index,
          len, freq, param);
    result = sceAudioInOpen(userId, type, index, len, freq, param);
    if (result < 0) {
        c_log(C_LOG_LIB_AUDIO_IN, C_LOG_LEVEL_ERROR, "sceAudioInHqOpen", "Error returned  0x%x",
              result);
    }
    return result;
}

static void audio_in_take_locked(void* ctx) {
    audio_in_call_t* call = (audio_in_call_t*)ctx;
    call->open = retro_atomic_exchange_int(&call->port->open, 0);
}

static void audio_in_reset_locked(void* ctx) {
    audio_in_call_t* call = (audio_in_call_t*)ctx;
    /* Its own backend only: the port may be another open's by now */
    if (call->port->impl_open == call->open) {
        call->port->impl_open = 0;
        memset(&call->port->impl, 0, sizeof(call->port->impl));
    }
}

int PS4_SYSV_ABI sceAudioInClose(s32 handle) {
    audio_in_call_t call;
    int port_id;
    c_log(C_LOG_LIB_AUDIO_IN, C_LOG_LEVEL_INFO, "sceAudioInClose", "called, handle=0x%x", handle);

    port_id = GetPortId("sceAudioInClose", handle);
    if (port_id < 0) {
        c_log(C_LOG_LIB_AUDIO_IN, C_LOG_LEVEL_ERROR, "sceAudioInClose", "Invalid port id");
        return ORBIS_AUDIO_IN_ERROR_INVALID_HANDLE;
    }

    call.port = &port_table[port_id];
    call.open = 0;
    combiner_run(&port_allocation, audio_in_take_locked, &call);
    if (!call.open) {
        c_log(C_LOG_LIB_AUDIO_IN, C_LOG_LEVEL_ERROR, "sceAudioInClose", "Port wasn't open %d",
              port_id);
        return ORBIS_AUDIO_IN_ERROR_NOT_OPENED;
    }

    /* Free resources */
    combiner_run(&call.port->access, audio_in_reset_locked, &call);

    c_log(C_LOG_LIB_AUDIO_IN, C_LOG_LEVEL_INFO, "sceAudioInClose", "Closed audio input port %d",
          handle);
    return ORBIS_OK;
}

static void audio_in_read_locked(void* ctx) {
    audio_in_call_t* call = (audio_in_call_t*)ctx;
    PortIn* port = call->port;
    /* Closed meanwhile */
    if (port->impl_open != call->open || retro_atomic_load_acquire_int(&port->open) != call->open) {
        call->result = ORBIS_AUDIO_IN_ERROR_NOT_OPENED;
        return;
    }
    call->result = port->impl.ops->read(&port->impl, call->dest);
}

/* The port of @port_id, if open: its open's number into @call; 0 when not */
static int audio_in_find(const char* func, int port_id, s32 handle, audio_in_call_t* call) {
    call->port = &port_table[port_id];
    call->open = retro_atomic_load_acquire_int(&call->port->open);
    if (!call->open) {
        c_log(C_LOG_LIB_AUDIO_IN, C_LOG_LEVEL_ERROR, func, "Audio input port %d is not open",
              handle);
    }
    return call->open;
}

int PS4_SYSV_ABI sceAudioInInput(s32 handle, void* dest) {
    audio_in_call_t call;
    int port_id;
#ifndef NDEBUG
    c_log(C_LOG_LIB_AUDIO_IN, C_LOG_LEVEL_TRACE, "sceAudioInInput",
          "called, handle=0x%x, dest=0x%llx", handle, (unsigned long long)(uintptr_t)dest);
#endif

    port_id = GetPortId("sceAudioInInput", handle);
    if (port_id < 0) {
        c_log(C_LOG_LIB_AUDIO_IN, C_LOG_LEVEL_ERROR, "sceAudioInInput", "Invalid port id");
        return ORBIS_AUDIO_IN_ERROR_INVALID_HANDLE;
    }

    if (!dest) {
        c_log(C_LOG_LIB_AUDIO_IN, C_LOG_LEVEL_ERROR, "sceAudioInInput",
              "Invalid output buffer pointer");
        return ORBIS_AUDIO_IN_ERROR_INVALID_POINTER;
    }

    if (!audio_in_find("sceAudioInInput", port_id, handle, &call)) {
        return ORBIS_AUDIO_IN_ERROR_NOT_OPENED;
    }
    call.dest = dest;
    combiner_run(&call.port->access, audio_in_read_locked, &call);
    return call.result;
}

static void audio_in_silent_locked(void* ctx) {
    audio_in_call_t* call = (audio_in_call_t*)ctx;
    PortIn* port = call->port;
    u32 silent_state = 0;
    /* if no mic exist or is not available */
    if (port->impl_open != call->open || retro_atomic_load_acquire_int(&port->open) != call->open ||
        !port->impl.ops->is_available(&port->impl)) {
        silent_state |= ORBIS_AUDIO_IN_SILENT_STATE_DEVICE_NONE;
    }
    call->result = (int)silent_state;
}

int PS4_SYSV_ABI sceAudioInGetSilentState(s32 handle) {
    audio_in_call_t call;
    int port_id;
#ifndef NDEBUG
    c_log(C_LOG_LIB_AUDIO_IN, C_LOG_LEVEL_TRACE, "sceAudioInGetSilentState", "called, handle=0x%x",
          handle);
#endif

    port_id = GetPortId("sceAudioInGetSilentState", handle);
    if (port_id < 0) {
        c_log(C_LOG_LIB_AUDIO_IN, C_LOG_LEVEL_ERROR, "sceAudioInGetSilentState", "Invalid port id");
        return ORBIS_AUDIO_IN_ERROR_INVALID_HANDLE;
    }

    if (!audio_in_find("sceAudioInGetSilentState", port_id, handle, &call)) {
        return ORBIS_AUDIO_IN_ERROR_NOT_OPENED;
    }
    combiner_run(&call.port->access, audio_in_silent_locked, &call);
    return call.result;
}

/*
 * Stubbed functions
 **/
int PS4_SYSV_ABI sceAudioInChangeAppModuleState(void) {
    c_log(C_LOG_LIB_AUDIO_IN, C_LOG_LEVEL_ERROR, "sceAudioInChangeAppModuleState",
          "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI sceAudioInCountPorts(void) {
    c_log(C_LOG_LIB_AUDIO_IN, C_LOG_LEVEL_ERROR, "sceAudioInCountPorts", "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI sceAudioInDeviceHqOpen(void) {
    c_log(C_LOG_LIB_AUDIO_IN, C_LOG_LEVEL_ERROR, "sceAudioInDeviceHqOpen", "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI sceAudioInDeviceIdHqOpen(void) {
    c_log(C_LOG_LIB_AUDIO_IN, C_LOG_LEVEL_ERROR, "sceAudioInDeviceIdHqOpen", "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI sceAudioInDeviceIdOpen(void) {
    c_log(C_LOG_LIB_AUDIO_IN, C_LOG_LEVEL_ERROR, "sceAudioInDeviceIdOpen", "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI sceAudioInDeviceOpen(void) {
    c_log(C_LOG_LIB_AUDIO_IN, C_LOG_LEVEL_ERROR, "sceAudioInDeviceOpen", "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI sceAudioInDeviceOpenEx(void) {
    c_log(C_LOG_LIB_AUDIO_IN, C_LOG_LEVEL_ERROR, "sceAudioInDeviceOpenEx", "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI sceAudioInExtClose(void) {
    c_log(C_LOG_LIB_AUDIO_IN, C_LOG_LEVEL_ERROR, "sceAudioInExtClose", "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI sceAudioInExtCtrl(void) {
    c_log(C_LOG_LIB_AUDIO_IN, C_LOG_LEVEL_ERROR, "sceAudioInExtCtrl", "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI sceAudioInExtInput(void) {
    c_log(C_LOG_LIB_AUDIO_IN, C_LOG_LEVEL_ERROR, "sceAudioInExtInput", "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI sceAudioInExtOpen(void) {
    c_log(C_LOG_LIB_AUDIO_IN, C_LOG_LEVEL_ERROR, "sceAudioInExtOpen", "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI sceAudioInExtSetAecMode(void) {
    c_log(C_LOG_LIB_AUDIO_IN, C_LOG_LEVEL_ERROR, "sceAudioInExtSetAecMode", "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI sceAudioInGetGain(void) {
    c_log(C_LOG_LIB_AUDIO_IN, C_LOG_LEVEL_ERROR, "sceAudioInGetGain", "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI sceAudioInGetHandleStatusInfo(void) {
    c_log(C_LOG_LIB_AUDIO_IN, C_LOG_LEVEL_ERROR, "sceAudioInGetHandleStatusInfo",
          "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI sceAudioInGetRerouteCount(void) {
    c_log(C_LOG_LIB_AUDIO_IN, C_LOG_LEVEL_ERROR, "sceAudioInGetRerouteCount", "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI sceAudioInHqOpenEx(void) {
    c_log(C_LOG_LIB_AUDIO_IN, C_LOG_LEVEL_ERROR, "sceAudioInHqOpenEx", "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI sceAudioInInit(void) {
    c_log(C_LOG_LIB_AUDIO_IN, C_LOG_LEVEL_ERROR, "sceAudioInInit", "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI sceAudioInInputs(void) {
    c_log(C_LOG_LIB_AUDIO_IN, C_LOG_LEVEL_ERROR, "sceAudioInInputs", "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI sceAudioInIsSharedDevice(void) {
    c_log(C_LOG_LIB_AUDIO_IN, C_LOG_LEVEL_ERROR, "sceAudioInIsSharedDevice", "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI sceAudioInOpenEx(void) {
    c_log(C_LOG_LIB_AUDIO_IN, C_LOG_LEVEL_ERROR, "sceAudioInOpenEx", "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI sceAudioInSetAllMute(void) {
    c_log(C_LOG_LIB_AUDIO_IN, C_LOG_LEVEL_ERROR, "sceAudioInSetAllMute", "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI sceAudioInSetCompressorPreGain(void) {
    c_log(C_LOG_LIB_AUDIO_IN, C_LOG_LEVEL_ERROR, "sceAudioInSetCompressorPreGain",
          "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI sceAudioInSetConnections(void) {
    c_log(C_LOG_LIB_AUDIO_IN, C_LOG_LEVEL_ERROR, "sceAudioInSetConnections", "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI sceAudioInSetConnectionsForUser(void) {
    c_log(C_LOG_LIB_AUDIO_IN, C_LOG_LEVEL_ERROR, "sceAudioInSetConnectionsForUser",
          "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI sceAudioInSetDevConnection(void) {
    c_log(C_LOG_LIB_AUDIO_IN, C_LOG_LEVEL_ERROR, "sceAudioInSetDevConnection", "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI sceAudioInSetFocusForUser(void) {
    c_log(C_LOG_LIB_AUDIO_IN, C_LOG_LEVEL_ERROR, "sceAudioInSetFocusForUser", "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI sceAudioInSetMode(void) {
    c_log(C_LOG_LIB_AUDIO_IN, C_LOG_LEVEL_ERROR, "sceAudioInSetMode", "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI sceAudioInSetMode2(void) {
    c_log(C_LOG_LIB_AUDIO_IN, C_LOG_LEVEL_ERROR, "sceAudioInSetMode2", "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI sceAudioInSetPortConnections(void) {
    c_log(C_LOG_LIB_AUDIO_IN, C_LOG_LEVEL_ERROR, "sceAudioInSetPortConnections",
          "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI sceAudioInSetPortStatuses(void) {
    c_log(C_LOG_LIB_AUDIO_IN, C_LOG_LEVEL_ERROR, "sceAudioInSetPortStatuses", "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI sceAudioInSetSparkParam(void) {
    c_log(C_LOG_LIB_AUDIO_IN, C_LOG_LEVEL_ERROR, "sceAudioInSetSparkParam", "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI sceAudioInSetSparkSideTone(void) {
    c_log(C_LOG_LIB_AUDIO_IN, C_LOG_LEVEL_ERROR, "sceAudioInSetSparkSideTone", "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI sceAudioInSetUsbGain(void) {
    c_log(C_LOG_LIB_AUDIO_IN, C_LOG_LEVEL_ERROR, "sceAudioInSetUsbGain", "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI sceAudioInSetUserMute(void) {
    c_log(C_LOG_LIB_AUDIO_IN, C_LOG_LEVEL_ERROR, "sceAudioInSetUserMute", "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI sceAudioInVmicCreate(void) {
    c_log(C_LOG_LIB_AUDIO_IN, C_LOG_LEVEL_ERROR, "sceAudioInVmicCreate", "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI sceAudioInVmicDestroy(void) {
    c_log(C_LOG_LIB_AUDIO_IN, C_LOG_LEVEL_ERROR, "sceAudioInVmicDestroy", "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI sceAudioInVmicWrite(void) {
    c_log(C_LOG_LIB_AUDIO_IN, C_LOG_LEVEL_ERROR, "sceAudioInVmicWrite", "(STUBBED) called");
    return ORBIS_OK;
}
