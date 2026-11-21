/* SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <math.h>
#include <stddef.h>
#include <string.h>

#include <boolean.h>

#include "common/c_bridge.h"
#include "common/combine.h"
#include "core/libraries/error_codes.h"
#include "core/libraries/voice/voice.h"
#include "core/libraries/voice/voice_error.h"

/* ORBIS_AUDIO_IN_SILENT_STATE_DEVICE_NONE (audioin.h) */
#define VOICE_AUDIO_IN_SILENT_STATE_DEVICE_NONE 0x00000001

typedef struct voice_port {
    bool allocated;
    s32 type;
    bool muted;
    float volume;
    u32 bitrate;
} voice_port;

/* A port as it is before it is created, and after it is deleted */
static const voice_port voice_port_default = {false, 0, false, 1.0f, 48000};

/* The ports, and the combiner every call runs under */
static struct {
    bool initialized;
    voice_port ports[ORBIS_VOICE_MAX_PORT];
    combiner_t mutex;
} g_state;

static void reset_ports(void) {
    u32 i;
    for (i = 0; i < ORBIS_VOICE_MAX_PORT; i++) {
        g_state.ports[i] = voice_port_default;
    }
}

void voice_library_init(void) {
    static bool done = false;
    if (!done) {
        done = true;
        combiner_init(&g_state.mutex);
        g_state.initialized = false;
        reset_ports();
    }
}

static s32 CheckPort(u32 port_id) {
    if (!g_state.initialized) {
        return ORBIS_VOICE_ERROR_NOT_INIT;
    }
    if (port_id >= ORBIS_VOICE_MAX_PORT) {
        return ORBIS_VOICE_ERROR_PORT_INVALID;
    }
    return ORBIS_OK;
}

static voice_port* FindPort(u32 port_id) {
    voice_port* port = &g_state.ports[port_id];
    return port->allocated ? port : NULL;
}

/* std::isfinite: not NaN, not infinite */
static bool voice_is_finite(float v) {
    return v == v && v - v == 0.0f;
}

typedef struct InitInternal_call {
    const OrbisVoiceInitParam* param;
    s32 ret;
} InitInternal_call;

static void InitInternal_locked(void* user) {
    InitInternal_call* c = (InitInternal_call*)user;

    if (g_state.initialized) {
        c_log(C_LOG_LIB_VOICE, C_LOG_LEVEL_ERROR, "InitInternal", "Voice is already initialized");
        {
            c->ret = ORBIS_VOICE_ERROR_ALREADY_INIT;
            return;
        }
    }
    if (c->param == NULL) {
        c_log(C_LOG_LIB_VOICE, C_LOG_LEVEL_ERROR, "InitInternal",
              "Invalid argument: param is null");
        {
            c->ret = ORBIS_VOICE_ERROR_ARGUMENT_INVALID;
            return;
        }
    }
    reset_ports();
    g_state.initialized = true;
    {
        c->ret = ORBIS_OK;
        return;
    }
}

static s32 InitInternal(const OrbisVoiceInitParam* param) {
    InitInternal_call c;
    c.param = param;
    combiner_run(&g_state.mutex, InitInternal_locked, &c);
    return c.ret;
}

typedef struct sceVoiceConnectIPortToOPort_call {
    u32 in_port_id;
    u32 out_port_id;
    s32 ret;
} sceVoiceConnectIPortToOPort_call;

static void sceVoiceConnectIPortToOPort_locked(void* user) {
    sceVoiceConnectIPortToOPort_call* c = (sceVoiceConnectIPortToOPort_call*)user;

    if (!g_state.initialized) {
        c_log(C_LOG_LIB_VOICE, C_LOG_LEVEL_ERROR, "sceVoiceConnectIPortToOPort",
              "Voice is not initialized");
        {
            c->ret = ORBIS_VOICE_ERROR_NOT_INIT;
            return;
        }
    }
    if (c->in_port_id >= ORBIS_VOICE_MAX_PORT || c->out_port_id >= ORBIS_VOICE_MAX_PORT) {
        c_log(C_LOG_LIB_VOICE, C_LOG_LEVEL_ERROR, "sceVoiceConnectIPortToOPort",
              "Invalid port: in_port_id = %u, out_port_id = %u", c->in_port_id, c->out_port_id);
        {
            c->ret = ORBIS_VOICE_ERROR_PORT_INVALID;
            return;
        }
    }
    if (!FindPort(c->in_port_id) || !FindPort(c->out_port_id)) {
        c_log(C_LOG_LIB_VOICE, C_LOG_LEVEL_ERROR, "sceVoiceConnectIPortToOPort",
              "Port not allocated: in_port_id = %u, out_port_id = %u", c->in_port_id,
              c->out_port_id);
        {
            c->ret = ORBIS_VOICE_ERROR_TOPOLOGY;
            return;
        }
    }
    {
        c->ret = ORBIS_OK;
        return;
    }
}

s32 PS4_SYSV_ABI sceVoiceConnectIPortToOPort(u32 in_port_id, u32 out_port_id) {
    sceVoiceConnectIPortToOPort_call c;
    c_log(C_LOG_LIB_VOICE, C_LOG_LEVEL_INFO, "sceVoiceConnectIPortToOPort",
          "in_port_id = %u, out_port_id = %u", in_port_id, out_port_id);
    c.in_port_id = in_port_id;
    c.out_port_id = out_port_id;
    combiner_run(&g_state.mutex, sceVoiceConnectIPortToOPort_locked, &c);
    return c.ret;
}

typedef struct sceVoiceCreatePort_call {
    s32* port_id;
    const OrbisVoicePortParam* param;
    s32 ret;
} sceVoiceCreatePort_call;

static void sceVoiceCreatePort_locked(void* user) {
    u32 i;
    sceVoiceCreatePort_call* c = (sceVoiceCreatePort_call*)user;

    if (!g_state.initialized) {
        c_log(C_LOG_LIB_VOICE, C_LOG_LEVEL_ERROR, "sceVoiceCreatePort", "Voice is not initialized");
        {
            c->ret = ORBIS_VOICE_ERROR_NOT_INIT;
            return;
        }
    }
    if (c->port_id == NULL || c->param == NULL) {
        c_log(C_LOG_LIB_VOICE, C_LOG_LEVEL_ERROR, "sceVoiceCreatePort",
              "Invalid argument: port_id is null or param is null");
        {
            c->ret = ORBIS_VOICE_ERROR_ARGUMENT_INVALID;
            return;
        }
    }
    if (c->param->type < ORBIS_VOICE_PORTTYPE_IN_DEVICE ||
        c->param->type > ORBIS_VOICE_PORTTYPE_OUT_DEVICE) {
        c_log(C_LOG_LIB_VOICE, C_LOG_LEVEL_ERROR, "sceVoiceCreatePort", "Invalid port type: %d",
              c->param->type);
        {
            c->ret = ORBIS_VOICE_ERROR_ARGUMENT_INVALID;
            return;
        }
    }
    for (i = 0; i < ORBIS_VOICE_MAX_PORT && g_state.ports[i].allocated; i++) {
    }
    if (i == ORBIS_VOICE_MAX_PORT) {
        c_log(C_LOG_LIB_VOICE, C_LOG_LEVEL_ERROR, "sceVoiceCreatePort", "No free port available");
        c->ret = ORBIS_VOICE_ERROR_RESOURCE_INSUFFICIENT;
        return;
    }
    g_state.ports[i] = voice_port_default;
    g_state.ports[i].allocated = true;
    g_state.ports[i].type = c->param->type;
    *c->port_id = (s32)i;
    {
        c->ret = ORBIS_OK;
        return;
    }
}

s32 PS4_SYSV_ABI sceVoiceCreatePort(s32* port_id, const OrbisVoicePortParam* param) {
    sceVoiceCreatePort_call c;
    c_log(C_LOG_LIB_VOICE, C_LOG_LEVEL_INFO, "sceVoiceCreatePort", "called");
    c.port_id = port_id;
    c.param = param;
    combiner_run(&g_state.mutex, sceVoiceCreatePort_locked, &c);
    return c.ret;
}

typedef struct sceVoiceDeletePort_call {
    u32 port_id;
    s32 ret;
} sceVoiceDeletePort_call;

static void sceVoiceDeletePort_locked(void* user) {
    s32 ret;
    sceVoiceDeletePort_call* c = (sceVoiceDeletePort_call*)user;

    ret = CheckPort(c->port_id);
    if (ret != ORBIS_OK) {
        c_log(C_LOG_LIB_VOICE, C_LOG_LEVEL_ERROR, "sceVoiceDeletePort",
              "Port check failed: port_id = %u, error = 0x%x", c->port_id, ((u32)(ret)));
        {
            c->ret = ret;
            return;
        }
    }
    if (!FindPort(c->port_id)) {
        c_log(C_LOG_LIB_VOICE, C_LOG_LEVEL_ERROR, "sceVoiceDeletePort", "Port %u is not allocated",
              c->port_id);
        {
            c->ret = ORBIS_VOICE_ERROR_PORT_INVALID;
            return;
        }
    }
    g_state.ports[c->port_id] = voice_port_default;
    {
        c->ret = ORBIS_OK;
        return;
    }
}

s32 PS4_SYSV_ABI sceVoiceDeletePort(u32 port_id) {
    sceVoiceDeletePort_call c;
    c_log(C_LOG_LIB_VOICE, C_LOG_LEVEL_INFO, "sceVoiceDeletePort", "port_id = %u", port_id);
    c.port_id = port_id;
    combiner_run(&g_state.mutex, sceVoiceDeletePort_locked, &c);
    return c.ret;
}

typedef struct sceVoiceDisconnectIPortFromOPort_call {
    u32 in_port_id;
    u32 out_port_id;
    s32 ret;
} sceVoiceDisconnectIPortFromOPort_call;

static void sceVoiceDisconnectIPortFromOPort_locked(void* user) {
    sceVoiceDisconnectIPortFromOPort_call* c = (sceVoiceDisconnectIPortFromOPort_call*)user;

    if (!g_state.initialized) {
        c_log(C_LOG_LIB_VOICE, C_LOG_LEVEL_ERROR, "sceVoiceDisconnectIPortFromOPort",
              "Voice is not initialized");
        {
            c->ret = ORBIS_VOICE_ERROR_NOT_INIT;
            return;
        }
    }
    if (c->in_port_id >= ORBIS_VOICE_MAX_PORT || c->out_port_id >= ORBIS_VOICE_MAX_PORT) {
        c_log(C_LOG_LIB_VOICE, C_LOG_LEVEL_ERROR, "sceVoiceDisconnectIPortFromOPort",
              "Invalid port: in_port_id = %u, out_port_id = %u", c->in_port_id, c->out_port_id);
        {
            c->ret = ORBIS_VOICE_ERROR_PORT_INVALID;
            return;
        }
    }
    if (!FindPort(c->in_port_id) || !FindPort(c->out_port_id)) {
        c_log(C_LOG_LIB_VOICE, C_LOG_LEVEL_ERROR, "sceVoiceDisconnectIPortFromOPort",
              "Port not allocated: in_port_id = %u, out_port_id = %u", c->in_port_id,
              c->out_port_id);
        {
            c->ret = ORBIS_VOICE_ERROR_TOPOLOGY;
            return;
        }
    }
    {
        c->ret = ORBIS_OK;
        return;
    }
}

s32 PS4_SYSV_ABI sceVoiceDisconnectIPortFromOPort(u32 in_port_id, u32 out_port_id) {
    sceVoiceDisconnectIPortFromOPort_call c;
    c_log(C_LOG_LIB_VOICE, C_LOG_LEVEL_INFO, "sceVoiceDisconnectIPortFromOPort",
          "in_port_id = %u, out_port_id = %u", in_port_id, out_port_id);
    c.in_port_id = in_port_id;
    c.out_port_id = out_port_id;
    combiner_run(&g_state.mutex, sceVoiceDisconnectIPortFromOPort_locked, &c);
    return c.ret;
}

typedef struct sceVoiceEnd_call {
    s32 ret;
} sceVoiceEnd_call;

static void sceVoiceEnd_locked(void* user) {
    sceVoiceEnd_call* c = (sceVoiceEnd_call*)user;

    if (!g_state.initialized) {
        c_log(C_LOG_LIB_VOICE, C_LOG_LEVEL_ERROR, "sceVoiceEnd", "Voice is not initialized");
        {
            c->ret = ORBIS_VOICE_ERROR_NOT_INIT;
            return;
        }
    }
    g_state.initialized = false;
    reset_ports();
    {
        c->ret = ORBIS_OK;
        return;
    }
}

s32 PS4_SYSV_ABI sceVoiceEnd(void) {
    sceVoiceEnd_call c;
    c_log(C_LOG_LIB_VOICE, C_LOG_LEVEL_INFO, "sceVoiceEnd", "called");
    combiner_run(&g_state.mutex, sceVoiceEnd_locked, &c);
    return c.ret;
}

typedef struct sceVoiceGetBitRate_call {
    u32 port_id;
    u32* bitrate;
    s32 ret;
} sceVoiceGetBitRate_call;

static void sceVoiceGetBitRate_locked(void* user) {
    s32 ret;
    voice_port* port;
    sceVoiceGetBitRate_call* c = (sceVoiceGetBitRate_call*)user;

    ret = CheckPort(c->port_id);
    if (ret != ORBIS_OK) {
        c_log(C_LOG_LIB_VOICE, C_LOG_LEVEL_ERROR, "sceVoiceGetBitRate",
              "Port check failed: port_id = %u, error = 0x%x", c->port_id, ((u32)(ret)));
        {
            c->ret = ret;
            return;
        }
    }
    if (c->bitrate == NULL) {
        c_log(C_LOG_LIB_VOICE, C_LOG_LEVEL_ERROR, "sceVoiceGetBitRate",
              "Invalid argument: bitrate is null");
        {
            c->ret = ORBIS_VOICE_ERROR_ARGUMENT_INVALID;
            return;
        }
    }
    port = FindPort(c->port_id);
    if (!port) {
        c_log(C_LOG_LIB_VOICE, C_LOG_LEVEL_ERROR, "sceVoiceGetBitRate", "Port %u is not allocated",
              c->port_id);
        {
            c->ret = ORBIS_VOICE_ERROR_PORT_INVALID;
            return;
        }
    }
    *c->bitrate = port->bitrate;
    {
        c->ret = ORBIS_OK;
        return;
    }
}

s32 PS4_SYSV_ABI sceVoiceGetBitRate(u32 port_id, u32* bitrate) {
    sceVoiceGetBitRate_call c;
    c_log(C_LOG_LIB_VOICE, C_LOG_LEVEL_TRACE, "sceVoiceGetBitRate", "port_id = %u", port_id);
    c.port_id = port_id;
    c.bitrate = bitrate;
    combiner_run(&g_state.mutex, sceVoiceGetBitRate_locked, &c);
    return c.ret;
}

typedef struct sceVoiceGetMuteFlag_call {
    u32 port_id;
    bool* mute;
    s32 ret;
} sceVoiceGetMuteFlag_call;

static void sceVoiceGetMuteFlag_locked(void* user) {
    s32 ret;
    voice_port* port;
    sceVoiceGetMuteFlag_call* c = (sceVoiceGetMuteFlag_call*)user;

    ret = CheckPort(c->port_id);
    if (ret != ORBIS_OK) {
        c_log(C_LOG_LIB_VOICE, C_LOG_LEVEL_ERROR, "sceVoiceGetMuteFlag",
              "Port check failed: port_id = %u, error = 0x%x", c->port_id, ((u32)(ret)));
        {
            c->ret = ret;
            return;
        }
    }
    if (c->mute == NULL) {
        c_log(C_LOG_LIB_VOICE, C_LOG_LEVEL_ERROR, "sceVoiceGetMuteFlag",
              "Invalid argument: mute is null");
        {
            c->ret = ORBIS_VOICE_ERROR_ARGUMENT_INVALID;
            return;
        }
    }
    port = FindPort(c->port_id);
    if (!port) {
        c_log(C_LOG_LIB_VOICE, C_LOG_LEVEL_ERROR, "sceVoiceGetMuteFlag", "Port %u is not allocated",
              c->port_id);
        {
            c->ret = ORBIS_VOICE_ERROR_PORT_INVALID;
            return;
        }
    }
    *c->mute = port->muted;
    {
        c->ret = ORBIS_OK;
        return;
    }
}

s32 PS4_SYSV_ABI sceVoiceGetMuteFlag(u32 port_id, bool* mute) {
    sceVoiceGetMuteFlag_call c;
    c_log(C_LOG_LIB_VOICE, C_LOG_LEVEL_TRACE, "sceVoiceGetMuteFlag", "port_id = %u", port_id);
    c.port_id = port_id;
    c.mute = mute;
    combiner_run(&g_state.mutex, sceVoiceGetMuteFlag_locked, &c);
    return c.ret;
}

typedef struct sceVoiceGetPortAttr_call {
    u32 port_id;
    s32 attr_id;
    u32 size;
    s32 ret;
    void* value;
} sceVoiceGetPortAttr_call;

static void sceVoiceGetPortAttr_locked(void* user) {
    s32 ret;
    u32 expected_size;
    s32 required_type;
    voice_port* port;
    u32 result;
    sceVoiceGetPortAttr_call* c = (sceVoiceGetPortAttr_call*)user;

    ret = CheckPort(c->port_id);
    if (ret != ORBIS_OK) {
        c_log(C_LOG_LIB_VOICE, C_LOG_LEVEL_ERROR, "sceVoiceGetPortAttr",
              "Port check failed: port_id = %u, error = 0x%x", c->port_id, ((u32)(ret)));
        {
            c->ret = ret;
            return;
        }
    }
    if (c->value == NULL) {
        c_log(C_LOG_LIB_VOICE, C_LOG_LEVEL_ERROR, "sceVoiceGetPortAttr",
              "Invalid argument: value is null");
        {
            c->ret = ORBIS_VOICE_ERROR_ARGUMENT_INVALID;
            return;
        }
    }
    switch (c->attr_id) {
    case ORBIS_VOICE_ATTR_1000:
    case ORBIS_VOICE_ATTR_1001:
    case ORBIS_VOICE_ATTR_2000:
    case ORBIS_VOICE_ATTR_2002:
        expected_size = 1;
        required_type = ORBIS_VOICE_PORTTYPE_IN_DEVICE;
        break;
    case ORBIS_VOICE_ATTR_1002:
    case ORBIS_VOICE_ATTR_2001:
        expected_size = 4;
        required_type = ORBIS_VOICE_PORTTYPE_IN_DEVICE;
        break;
    case ORBIS_VOICE_ATTR_1003:
        expected_size = 2;
        required_type = ORBIS_VOICE_PORTTYPE_OUT_DEVICE;
        break;
    default:
        c_log(C_LOG_LIB_VOICE, C_LOG_LEVEL_ERROR, "sceVoiceGetPortAttr", "Invalid attr_id: %d",
              c->attr_id);
        {
            c->ret = ORBIS_VOICE_ERROR_ARGUMENT_INVALID;
            return;
        }
    }
    if (c->size != expected_size) {
        c_log(C_LOG_LIB_VOICE, C_LOG_LEVEL_ERROR, "sceVoiceGetPortAttr",
              "Invalid size %u for attr_id %d (expected %u)", c->size, c->attr_id, expected_size);
        {
            c->ret = ORBIS_VOICE_ERROR_ARGUMENT_INVALID;
            return;
        }
    }
    port = FindPort(c->port_id);
    if (!port) {
        c_log(C_LOG_LIB_VOICE, C_LOG_LEVEL_ERROR, "sceVoiceGetPortAttr", "Port %u is not allocated",
              c->port_id);
        {
            c->ret = ORBIS_VOICE_ERROR_PORT_INVALID;
            return;
        }
    }
    if (port->type != required_type) {
        c_log(C_LOG_LIB_VOICE, C_LOG_LEVEL_ERROR, "sceVoiceGetPortAttr",
              "Port %u has the wrong type for attr_id %d", c->port_id, c->attr_id);
        {
            c->ret = ORBIS_VOICE_ERROR_ARGUMENT_INVALID;
            return;
        }
    }
    result = c->attr_id == ORBIS_VOICE_ATTR_1002 ? VOICE_AUDIO_IN_SILENT_STATE_DEVICE_NONE : 0;
    switch (expected_size) {
    case 1:
        *((u8*)(c->value)) = ((u8)(result));
        break;
    case 2:
        *((u16*)(c->value)) = ((u16)(result));
        break;
    case 4:
        *((u32*)(c->value)) = result;
        break;
    }
    {
        c->ret = ORBIS_OK;
        return;
    }
}

s32 PS4_SYSV_ABI sceVoiceGetPortAttr(u32 port_id, s32 attr_id, void* value, u32 size) {
    sceVoiceGetPortAttr_call c;
    c_log(C_LOG_LIB_VOICE, C_LOG_LEVEL_TRACE, "sceVoiceGetPortAttr",
          "port_id = %u, attr_id = %d, size = %u", port_id, attr_id, size);
    c.port_id = port_id;
    c.attr_id = attr_id;
    c.value = value;
    c.size = size;
    combiner_run(&g_state.mutex, sceVoiceGetPortAttr_locked, &c);
    return c.ret;
}

typedef struct sceVoiceGetPortInfo_call {
    u32 port_id;
    OrbisVoicePortInfo* info;
    s32 ret;
} sceVoiceGetPortInfo_call;

static void sceVoiceGetPortInfo_locked(void* user) {
    s32 ret;
    voice_port* port;
    sceVoiceGetPortInfo_call* c = (sceVoiceGetPortInfo_call*)user;

    ret = CheckPort(c->port_id);
    if (ret != ORBIS_OK) {
        c_log(C_LOG_LIB_VOICE, C_LOG_LEVEL_ERROR, "sceVoiceGetPortInfo",
              "Port check failed: port_id = %u, error = 0x%x", c->port_id, ((u32)(ret)));
        {
            c->ret = ret;
            return;
        }
    }
    if (c->info == NULL) {
        c_log(C_LOG_LIB_VOICE, C_LOG_LEVEL_ERROR, "sceVoiceGetPortInfo",
              "Invalid argument: info is null");
        {
            c->ret = ORBIS_VOICE_ERROR_ARGUMENT_INVALID;
            return;
        }
    }
    port = FindPort(c->port_id);
    if (!port) {
        c_log(C_LOG_LIB_VOICE, C_LOG_LEVEL_ERROR, "sceVoiceGetPortInfo", "Port %u is not allocated",
              c->port_id);
        {
            c->ret = ORBIS_VOICE_ERROR_TOPOLOGY;
            return;
        }
    }
    c->info->port_type = port->type;
    c->info->state = 0;
    c->info->byte_count = 0;
    c->info->frame_size = 1;
    c->info->edge_count = 0;
    c->info->reserved = 0;
    {
        c->ret = ORBIS_OK;
        return;
    }
}

s32 PS4_SYSV_ABI sceVoiceGetPortInfo(u32 port_id, OrbisVoicePortInfo* info) {
    sceVoiceGetPortInfo_call c;
    c_log(C_LOG_LIB_VOICE, C_LOG_LEVEL_TRACE, "sceVoiceGetPortInfo", "port_id = %u", port_id);
    c.port_id = port_id;
    c.info = info;
    combiner_run(&g_state.mutex, sceVoiceGetPortInfo_locked, &c);
    return c.ret;
}

typedef struct sceVoiceGetResourceInfo_call {
    OrbisVoiceResourceInfo* info;
    s32 ret;
} sceVoiceGetResourceInfo_call;

static void sceVoiceGetResourceInfo_locked(void* user) {
    sceVoiceGetResourceInfo_call* c = (sceVoiceGetResourceInfo_call*)user;

    if (!g_state.initialized) {
        c_log(C_LOG_LIB_VOICE, C_LOG_LEVEL_ERROR, "sceVoiceGetResourceInfo",
              "Voice is not initialized");
        {
            c->ret = ORBIS_VOICE_ERROR_NOT_INIT;
            return;
        }
    }
    if (c->info == NULL) {
        c_log(C_LOG_LIB_VOICE, C_LOG_LEVEL_ERROR, "sceVoiceGetResourceInfo",
              "Invalid argument: info is null");
        {
            c->ret = ORBIS_VOICE_ERROR_ARGUMENT_INVALID;
            return;
        }
    }
    memset(c->info, 0, sizeof(*c->info));
    {
        c->ret = ORBIS_OK;
        return;
    }
}

s32 PS4_SYSV_ABI sceVoiceGetResourceInfo(OrbisVoiceResourceInfo* info) {
    sceVoiceGetResourceInfo_call c;
    c_log(C_LOG_LIB_VOICE, C_LOG_LEVEL_INFO, "sceVoiceGetResourceInfo", "called");
    c.info = info;
    combiner_run(&g_state.mutex, sceVoiceGetResourceInfo_locked, &c);
    return c.ret;
}

typedef struct sceVoiceGetVolume_call {
    u32 port_id;
    float* volume;
    s32 ret;
} sceVoiceGetVolume_call;

static void sceVoiceGetVolume_locked(void* user) {
    s32 ret;
    voice_port* port;
    sceVoiceGetVolume_call* c = (sceVoiceGetVolume_call*)user;

    ret = CheckPort(c->port_id);
    if (ret != ORBIS_OK) {
        c_log(C_LOG_LIB_VOICE, C_LOG_LEVEL_ERROR, "sceVoiceGetVolume",
              "Port check failed: port_id = %u, error = 0x%x", c->port_id, ((u32)(ret)));
        {
            c->ret = ret;
            return;
        }
    }
    if (c->volume == NULL) {
        c_log(C_LOG_LIB_VOICE, C_LOG_LEVEL_ERROR, "sceVoiceGetVolume",
              "Invalid argument: volume is null");
        {
            c->ret = ORBIS_VOICE_ERROR_ARGUMENT_INVALID;
            return;
        }
    }
    port = FindPort(c->port_id);
    if (!port) {
        c_log(C_LOG_LIB_VOICE, C_LOG_LEVEL_ERROR, "sceVoiceGetVolume", "Port %u is not allocated",
              c->port_id);
        {
            c->ret = ORBIS_VOICE_ERROR_PORT_INVALID;
            return;
        }
    }
    *c->volume = port->volume;
    {
        c->ret = ORBIS_OK;
        return;
    }
}

s32 PS4_SYSV_ABI sceVoiceGetVolume(u32 port_id, float* volume) {
    sceVoiceGetVolume_call c;
    c_log(C_LOG_LIB_VOICE, C_LOG_LEVEL_TRACE, "sceVoiceGetVolume", "port_id = %u", port_id);
    c.port_id = port_id;
    c.volume = volume;
    combiner_run(&g_state.mutex, sceVoiceGetVolume_locked, &c);
    return c.ret;
}

s32 PS4_SYSV_ABI sceVoiceInit(const OrbisVoiceInitParam* param, s32 container) {
    c_log(C_LOG_LIB_VOICE, C_LOG_LEVEL_INFO, "sceVoiceInit", "called, container = %d", container);
    return InitInternal(param);
}

s32 PS4_SYSV_ABI sceVoiceInitHQ(const OrbisVoiceInitParam* param, s32 container) {
    c_log(C_LOG_LIB_VOICE, C_LOG_LEVEL_INFO, "sceVoiceInitHQ", "called, container = %d", container);
    return InitInternal(param);
}

typedef struct sceVoicePausePort_call {
    u32 port_id;
    s32 ret;
} sceVoicePausePort_call;

static void sceVoicePausePort_locked(void* user) {
    s32 ret;
    sceVoicePausePort_call* c = (sceVoicePausePort_call*)user;

    ret = CheckPort(c->port_id);
    if (ret != ORBIS_OK) {
        c_log(C_LOG_LIB_VOICE, C_LOG_LEVEL_ERROR, "sceVoicePausePort",
              "Port check failed: port_id = %u, error = 0x%x", c->port_id, ((u32)(ret)));
        {
            c->ret = ret;
            return;
        }
    }
    if (!FindPort(c->port_id)) {
        c_log(C_LOG_LIB_VOICE, C_LOG_LEVEL_ERROR, "sceVoicePausePort", "Port %u is not allocated",
              c->port_id);
        {
            c->ret = ORBIS_VOICE_ERROR_PORT_INVALID;
            return;
        }
    }
    {
        c->ret = ORBIS_OK;
        return;
    }
}

s32 PS4_SYSV_ABI sceVoicePausePort(u32 port_id) {
    sceVoicePausePort_call c;
    c_log(C_LOG_LIB_VOICE, C_LOG_LEVEL_INFO, "sceVoicePausePort", "port_id = %u", port_id);
    c.port_id = port_id;
    combiner_run(&g_state.mutex, sceVoicePausePort_locked, &c);
    return c.ret;
}

typedef struct sceVoicePausePortAll_call {
    s32 ret;
} sceVoicePausePortAll_call;

static void sceVoicePausePortAll_locked(void* user) {
    sceVoicePausePortAll_call* c = (sceVoicePausePortAll_call*)user;

    if (!g_state.initialized) {
        c_log(C_LOG_LIB_VOICE, C_LOG_LEVEL_ERROR, "sceVoicePausePortAll",
              "Voice is not initialized");
        {
            c->ret = ORBIS_VOICE_ERROR_NOT_INIT;
            return;
        }
    }
    {
        c->ret = ORBIS_OK;
        return;
    }
}

s32 PS4_SYSV_ABI sceVoicePausePortAll(void) {
    sceVoicePausePortAll_call c;
    c_log(C_LOG_LIB_VOICE, C_LOG_LEVEL_INFO, "sceVoicePausePortAll", "called");
    combiner_run(&g_state.mutex, sceVoicePausePortAll_locked, &c);
    return c.ret;
}

typedef struct sceVoiceReadFromOPort_call {
    u32 port_id;
    u32* size;
    s32 ret;
    void* data;
} sceVoiceReadFromOPort_call;

static void sceVoiceReadFromOPort_locked(void* user) {
    s32 ret;
    voice_port* port;
    sceVoiceReadFromOPort_call* c = (sceVoiceReadFromOPort_call*)user;

    ret = CheckPort(c->port_id);
    if (ret != ORBIS_OK) {
        c_log(C_LOG_LIB_VOICE, C_LOG_LEVEL_ERROR, "sceVoiceReadFromOPort",
              "Port check failed: port_id = %u, error = 0x%x", c->port_id, ((u32)(ret)));
        {
            c->ret = ret;
            return;
        }
    }
    if (c->data == NULL || c->size == NULL) {
        c_log(C_LOG_LIB_VOICE, C_LOG_LEVEL_ERROR, "sceVoiceReadFromOPort",
              "Invalid argument: data is null or size is null");
        {
            c->ret = ORBIS_VOICE_ERROR_ARGUMENT_INVALID;
            return;
        }
    }
    *c->size = 0;
    port = FindPort(c->port_id);
    if (!port) {
        c_log(C_LOG_LIB_VOICE, C_LOG_LEVEL_ERROR, "sceVoiceReadFromOPort",
              "Port %u is not allocated", c->port_id);
        {
            c->ret = ORBIS_VOICE_ERROR_TOPOLOGY;
            return;
        }
    }
    switch (port->type) {
    case ORBIS_VOICE_PORTTYPE_OUT_PCMAUDIO:
    case ORBIS_VOICE_PORTTYPE_OUT_VOICE: {
        c->ret = ORBIS_OK;
        return;
    }
    case ORBIS_VOICE_PORTTYPE_OUT_DEVICE:
        c_log(C_LOG_LIB_VOICE, C_LOG_LEVEL_ERROR, "sceVoiceReadFromOPort",
              "Cannot read from a device output port: port_id = %u", c->port_id);
        {
            c->ret = ORBIS_VOICE_ERROR_ARGUMENT_INVALID;
            return;
        }
    default:
        c_log(C_LOG_LIB_VOICE, C_LOG_LEVEL_ERROR, "sceVoiceReadFromOPort",
              "Port %u is not an output port (type = %d)", c->port_id, port->type);
        {
            c->ret = ORBIS_VOICE_ERROR_TOPOLOGY;
            return;
        }
    }
}

s32 PS4_SYSV_ABI sceVoiceReadFromOPort(u32 port_id, void* data, u32* size) {
    sceVoiceReadFromOPort_call c;
    c_log(C_LOG_LIB_VOICE, C_LOG_LEVEL_TRACE, "sceVoiceReadFromOPort", "port_id = %u", port_id);
    c.port_id = port_id;
    c.data = data;
    c.size = size;
    combiner_run(&g_state.mutex, sceVoiceReadFromOPort_locked, &c);
    return c.ret;
}

typedef struct sceVoiceResetPort_call {
    u32 port_id;
    s32 ret;
} sceVoiceResetPort_call;

static void sceVoiceResetPort_locked(void* user) {
    s32 ret;
    voice_port* port;
    sceVoiceResetPort_call* c = (sceVoiceResetPort_call*)user;

    ret = CheckPort(c->port_id);
    if (ret != ORBIS_OK) {
        c_log(C_LOG_LIB_VOICE, C_LOG_LEVEL_ERROR, "sceVoiceResetPort",
              "Port check failed: port_id = %u, error = 0x%x", c->port_id, ((u32)(ret)));
        {
            c->ret = ret;
            return;
        }
    }
    port = FindPort(c->port_id);
    if (!port) {
        c_log(C_LOG_LIB_VOICE, C_LOG_LEVEL_ERROR, "sceVoiceResetPort", "Port %u is not allocated",
              c->port_id);
        {
            c->ret = ORBIS_VOICE_ERROR_PORT_INVALID;
            return;
        }
    }
    {
        const s32 type = port->type;
        *port = voice_port_default;
        port->allocated = true;
        port->type = type;
    }
    {
        c->ret = ORBIS_OK;
        return;
    }
}

s32 PS4_SYSV_ABI sceVoiceResetPort(u32 port_id) {
    sceVoiceResetPort_call c;
    c_log(C_LOG_LIB_VOICE, C_LOG_LEVEL_INFO, "sceVoiceResetPort", "port_id = %u", port_id);
    c.port_id = port_id;
    combiner_run(&g_state.mutex, sceVoiceResetPort_locked, &c);
    return c.ret;
}

typedef struct sceVoiceResumePort_call {
    u32 port_id;
    s32 ret;
} sceVoiceResumePort_call;

static void sceVoiceResumePort_locked(void* user) {
    s32 ret;
    sceVoiceResumePort_call* c = (sceVoiceResumePort_call*)user;

    ret = CheckPort(c->port_id);
    if (ret != ORBIS_OK) {
        c_log(C_LOG_LIB_VOICE, C_LOG_LEVEL_ERROR, "sceVoiceResumePort",
              "Port check failed: port_id = %u, error = 0x%x", c->port_id, ((u32)(ret)));
        {
            c->ret = ret;
            return;
        }
    }
    if (!FindPort(c->port_id)) {
        c_log(C_LOG_LIB_VOICE, C_LOG_LEVEL_ERROR, "sceVoiceResumePort", "Port %u is not allocated",
              c->port_id);
        {
            c->ret = ORBIS_VOICE_ERROR_PORT_INVALID;
            return;
        }
    }
    {
        c->ret = ORBIS_OK;
        return;
    }
}

s32 PS4_SYSV_ABI sceVoiceResumePort(u32 port_id) {
    sceVoiceResumePort_call c;
    c_log(C_LOG_LIB_VOICE, C_LOG_LEVEL_INFO, "sceVoiceResumePort", "port_id = %u", port_id);
    c.port_id = port_id;
    combiner_run(&g_state.mutex, sceVoiceResumePort_locked, &c);
    return c.ret;
}

typedef struct sceVoiceResumePortAll_call {
    s32 ret;
} sceVoiceResumePortAll_call;

static void sceVoiceResumePortAll_locked(void* user) {
    sceVoiceResumePortAll_call* c = (sceVoiceResumePortAll_call*)user;

    if (!g_state.initialized) {
        c_log(C_LOG_LIB_VOICE, C_LOG_LEVEL_ERROR, "sceVoiceResumePortAll",
              "Voice is not initialized");
        {
            c->ret = ORBIS_VOICE_ERROR_NOT_INIT;
            return;
        }
    }
    {
        c->ret = ORBIS_OK;
        return;
    }
}

s32 PS4_SYSV_ABI sceVoiceResumePortAll(void) {
    sceVoiceResumePortAll_call c;
    c_log(C_LOG_LIB_VOICE, C_LOG_LEVEL_INFO, "sceVoiceResumePortAll", "called");
    combiner_run(&g_state.mutex, sceVoiceResumePortAll_locked, &c);
    return c.ret;
}

typedef struct sceVoiceSetBitRate_call {
    u32 port_id;
    u32 bitrate;
    s32 ret;
} sceVoiceSetBitRate_call;

static void sceVoiceSetBitRate_locked(void* user) {
    s32 ret;
    voice_port* port;
    sceVoiceSetBitRate_call* c = (sceVoiceSetBitRate_call*)user;

    ret = CheckPort(c->port_id);
    if (ret != ORBIS_OK) {
        c_log(C_LOG_LIB_VOICE, C_LOG_LEVEL_ERROR, "sceVoiceSetBitRate",
              "Port check failed: port_id = %u, error = 0x%x", c->port_id, ((u32)(ret)));
        {
            c->ret = ret;
            return;
        }
    }
    port = FindPort(c->port_id);
    if (!port) {
        c_log(C_LOG_LIB_VOICE, C_LOG_LEVEL_ERROR, "sceVoiceSetBitRate", "Port %u is not allocated",
              c->port_id);
        {
            c->ret = ORBIS_VOICE_ERROR_PORT_INVALID;
            return;
        }
    }
    port->bitrate = c->bitrate;
    {
        c->ret = ORBIS_OK;
        return;
    }
}

s32 PS4_SYSV_ABI sceVoiceSetBitRate(u32 port_id, u32 bitrate) {
    sceVoiceSetBitRate_call c;
    c_log(C_LOG_LIB_VOICE, C_LOG_LEVEL_INFO, "sceVoiceSetBitRate", "port_id = %u, bitrate = %u",
          port_id, bitrate);
    c.port_id = port_id;
    c.bitrate = bitrate;
    combiner_run(&g_state.mutex, sceVoiceSetBitRate_locked, &c);
    return c.ret;
}

typedef struct sceVoiceSetMuteFlag_call {
    u32 port_id;
    bool mute;
    s32 ret;
} sceVoiceSetMuteFlag_call;

static void sceVoiceSetMuteFlag_locked(void* user) {
    s32 ret;
    voice_port* port;
    sceVoiceSetMuteFlag_call* c = (sceVoiceSetMuteFlag_call*)user;

    ret = CheckPort(c->port_id);
    if (ret != ORBIS_OK) {
        c_log(C_LOG_LIB_VOICE, C_LOG_LEVEL_ERROR, "sceVoiceSetMuteFlag",
              "Port check failed: port_id = %u, error = 0x%x", c->port_id, ((u32)(ret)));
        {
            c->ret = ret;
            return;
        }
    }
    port = FindPort(c->port_id);
    if (!port) {
        c_log(C_LOG_LIB_VOICE, C_LOG_LEVEL_ERROR, "sceVoiceSetMuteFlag", "Port %u is not allocated",
              c->port_id);
        {
            c->ret = ORBIS_VOICE_ERROR_PORT_INVALID;
            return;
        }
    }
    port->muted = c->mute;
    {
        c->ret = ORBIS_OK;
        return;
    }
}

s32 PS4_SYSV_ABI sceVoiceSetMuteFlag(u32 port_id, bool mute) {
    sceVoiceSetMuteFlag_call c;
    c_log(C_LOG_LIB_VOICE, C_LOG_LEVEL_INFO, "sceVoiceSetMuteFlag", "port_id = %u, mute = %s",
          port_id, (mute) ? "true" : "false");
    c.port_id = port_id;
    c.mute = mute;
    combiner_run(&g_state.mutex, sceVoiceSetMuteFlag_locked, &c);
    return c.ret;
}

typedef struct sceVoiceSetMuteFlagAll_call {
    bool mute;
    s32 ret;
} sceVoiceSetMuteFlagAll_call;

static void sceVoiceSetMuteFlagAll_locked(void* user) {
    u32 i;
    sceVoiceSetMuteFlagAll_call* c = (sceVoiceSetMuteFlagAll_call*)user;

    if (!g_state.initialized) {
        c_log(C_LOG_LIB_VOICE, C_LOG_LEVEL_ERROR, "sceVoiceSetMuteFlagAll",
              "Voice is not initialized");
        {
            c->ret = ORBIS_VOICE_ERROR_NOT_INIT;
            return;
        }
    }
    for (i = 0; i < ORBIS_VOICE_MAX_PORT; i++) {
        g_state.ports[i].muted = c->mute;
    }
    {
        c->ret = ORBIS_OK;
        return;
    }
}

s32 PS4_SYSV_ABI sceVoiceSetMuteFlagAll(bool mute) {
    sceVoiceSetMuteFlagAll_call c;
    c_log(C_LOG_LIB_VOICE, C_LOG_LEVEL_INFO, "sceVoiceSetMuteFlagAll", "mute = %s",
          (mute) ? "true" : "false");
    c.mute = mute;
    combiner_run(&g_state.mutex, sceVoiceSetMuteFlagAll_locked, &c);
    return c.ret;
}

s32 PS4_SYSV_ABI sceVoiceSetThreadsParams(void) {
    c_log(C_LOG_LIB_VOICE, C_LOG_LEVEL_ERROR, "sceVoiceSetThreadsParams", "(STUBBED) called");
    return ORBIS_OK;
}

typedef struct sceVoiceSetVolume_call {
    u32 port_id;
    float volume;
    s32 ret;
} sceVoiceSetVolume_call;

static void sceVoiceSetVolume_locked(void* user) {
    s32 ret;
    voice_port* port;
    sceVoiceSetVolume_call* c = (sceVoiceSetVolume_call*)user;

    ret = CheckPort(c->port_id);
    if (ret != ORBIS_OK) {
        c_log(C_LOG_LIB_VOICE, C_LOG_LEVEL_ERROR, "sceVoiceSetVolume",
              "Port check failed: port_id = %u, error = 0x%x", c->port_id, ((u32)(ret)));
        {
            c->ret = ret;
            return;
        }
    }
    port = FindPort(c->port_id);
    if (!port) {
        c_log(C_LOG_LIB_VOICE, C_LOG_LEVEL_ERROR, "sceVoiceSetVolume", "Port %u is not allocated",
              c->port_id);
        {
            c->ret = ORBIS_VOICE_ERROR_PORT_INVALID;
            return;
        }
    }
    port->volume = voice_is_finite(c->volume) ? c->volume : 0.0f;
    {
        c->ret = ORBIS_OK;
        return;
    }
}

s32 PS4_SYSV_ABI sceVoiceSetVolume(u32 port_id, float volume) {
    sceVoiceSetVolume_call c;
    c_log(C_LOG_LIB_VOICE, C_LOG_LEVEL_INFO, "sceVoiceSetVolume", "port_id = %u, volume = %g",
          port_id, volume);
    c.port_id = port_id;
    c.volume = volume;
    combiner_run(&g_state.mutex, sceVoiceSetVolume_locked, &c);
    return c.ret;
}

typedef struct sceVoiceStart_call {
    const OrbisVoiceStartParam* param;
    s32 ret;
} sceVoiceStart_call;

static void sceVoiceStart_locked(void* user) {
    sceVoiceStart_call* c = (sceVoiceStart_call*)user;

    if (!g_state.initialized) {
        c_log(C_LOG_LIB_VOICE, C_LOG_LEVEL_ERROR, "sceVoiceStart", "Voice is not initialized");
        {
            c->ret = ORBIS_VOICE_ERROR_NOT_INIT;
            return;
        }
    }
    if (c->param == NULL || c->param->memory == NULL) {
        c_log(C_LOG_LIB_VOICE, C_LOG_LEVEL_ERROR, "sceVoiceStart",
              "Invalid argument: param is null or param->memory is null");
        {
            c->ret = ORBIS_VOICE_ERROR_ARGUMENT_INVALID;
            return;
        }
    }
    {
        c->ret = ORBIS_OK;
        return;
    }
}

s32 PS4_SYSV_ABI sceVoiceStart(const OrbisVoiceStartParam* param) {
    sceVoiceStart_call c;
    c_log(C_LOG_LIB_VOICE, C_LOG_LEVEL_INFO, "sceVoiceStart", "called");
    c.param = param;
    combiner_run(&g_state.mutex, sceVoiceStart_locked, &c);
    return c.ret;
}

typedef struct sceVoiceStop_call {
    s32 ret;
} sceVoiceStop_call;

static void sceVoiceStop_locked(void* user) {
    sceVoiceStop_call* c = (sceVoiceStop_call*)user;

    if (!g_state.initialized) {
        c_log(C_LOG_LIB_VOICE, C_LOG_LEVEL_ERROR, "sceVoiceStop", "Voice is not initialized");
        {
            c->ret = ORBIS_VOICE_ERROR_NOT_INIT;
            return;
        }
    }
    {
        c->ret = ORBIS_OK;
        return;
    }
}

s32 PS4_SYSV_ABI sceVoiceStop(void) {
    sceVoiceStop_call c;
    c_log(C_LOG_LIB_VOICE, C_LOG_LEVEL_INFO, "sceVoiceStop", "called");
    combiner_run(&g_state.mutex, sceVoiceStop_locked, &c);
    return c.ret;
}

typedef struct sceVoiceUpdatePort_call {
    u32 port_id;
    const OrbisVoicePortParam* param;
    s32 ret;
} sceVoiceUpdatePort_call;

static void sceVoiceUpdatePort_locked(void* user) {
    s32 ret;
    sceVoiceUpdatePort_call* c = (sceVoiceUpdatePort_call*)user;

    ret = CheckPort(c->port_id);
    if (ret != ORBIS_OK) {
        c_log(C_LOG_LIB_VOICE, C_LOG_LEVEL_ERROR, "sceVoiceUpdatePort",
              "Port check failed: port_id = %u, error = 0x%x", c->port_id, ((u32)(ret)));
        {
            c->ret = ret;
            return;
        }
    }
    if (c->param == NULL) {
        c_log(C_LOG_LIB_VOICE, C_LOG_LEVEL_ERROR, "sceVoiceUpdatePort",
              "Invalid argument: param is null");
        {
            c->ret = ORBIS_VOICE_ERROR_ARGUMENT_INVALID;
            return;
        }
    }
    if (!FindPort(c->port_id)) {
        c_log(C_LOG_LIB_VOICE, C_LOG_LEVEL_ERROR, "sceVoiceUpdatePort", "Port %u is not allocated",
              c->port_id);
        {
            c->ret = ORBIS_VOICE_ERROR_PORT_INVALID;
            return;
        }
    }
    {
        c->ret = ORBIS_OK;
        return;
    }
}

s32 PS4_SYSV_ABI sceVoiceUpdatePort(u32 port_id, const OrbisVoicePortParam* param) {
    sceVoiceUpdatePort_call c;
    c_log(C_LOG_LIB_VOICE, C_LOG_LEVEL_INFO, "sceVoiceUpdatePort", "port_id = %u", port_id);
    c.port_id = port_id;
    c.param = param;
    combiner_run(&g_state.mutex, sceVoiceUpdatePort_locked, &c);
    return c.ret;
}

s32 PS4_SYSV_ABI sceVoiceVADAdjustment(float param1, float param2) {
    c_log(C_LOG_LIB_VOICE, C_LOG_LEVEL_INFO, "sceVoiceVADAdjustment", "param1 = %g, param2 = %g",
          param1, param2);
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceVoiceVADSetVersion(s32 version) {
    c_log(C_LOG_LIB_VOICE, C_LOG_LEVEL_INFO, "sceVoiceVADSetVersion", "version = %d", version);
    if (version != 0 && version != 1) {
        c_log(C_LOG_LIB_VOICE, C_LOG_LEVEL_ERROR, "sceVoiceVADSetVersion", "Invalid version: %d",
              version);
        return ORBIS_VOICE_ERROR_ARGUMENT_INVALID;
    }
    return ORBIS_OK;
}

typedef struct sceVoiceWriteToIPort_call {
    u32 port_id;
    u32* size;
    s16 unk;
    s32 ret;
    const void* data;
} sceVoiceWriteToIPort_call;

static void sceVoiceWriteToIPort_locked(void* user) {
    s32 ret;
    voice_port* port;
    sceVoiceWriteToIPort_call* c = (sceVoiceWriteToIPort_call*)user;

    ret = CheckPort(c->port_id);
    if (ret != ORBIS_OK) {
        c_log(C_LOG_LIB_VOICE, C_LOG_LEVEL_ERROR, "sceVoiceWriteToIPort",
              "Port check failed: port_id = %u, error = 0x%x", c->port_id, ((u32)(ret)));
        {
            c->ret = ret;
            return;
        }
    }
    if (c->data == NULL || c->size == NULL) {
        c_log(C_LOG_LIB_VOICE, C_LOG_LEVEL_ERROR, "sceVoiceWriteToIPort",
              "Invalid argument: data is null or size is null");
        {
            c->ret = ORBIS_VOICE_ERROR_ARGUMENT_INVALID;
            return;
        }
    }
    port = FindPort(c->port_id);
    if (!port) {
        c_log(C_LOG_LIB_VOICE, C_LOG_LEVEL_ERROR, "sceVoiceWriteToIPort",
              "Port %u is not allocated", c->port_id);
        *c->size = 0;
        {
            c->ret = ORBIS_VOICE_ERROR_TOPOLOGY;
            return;
        }
    }
    if (port->type != ORBIS_VOICE_PORTTYPE_IN_PCMAUDIO &&
        port->type != ORBIS_VOICE_PORTTYPE_IN_VOICE) {
        c_log(C_LOG_LIB_VOICE, C_LOG_LEVEL_ERROR, "sceVoiceWriteToIPort",
              "Port %u is not an input port (type = %d)", c->port_id, port->type);
        *c->size = 0;
        {
            c->ret = ORBIS_VOICE_ERROR_ARGUMENT_INVALID;
            return;
        }
    }
    {
        c->ret = ORBIS_OK;
        return;
    }
}

s32 PS4_SYSV_ABI sceVoiceWriteToIPort(u32 port_id, const void* data, u32* size, s16 unk) {
    sceVoiceWriteToIPort_call c;
    c_log(C_LOG_LIB_VOICE, C_LOG_LEVEL_TRACE, "sceVoiceWriteToIPort", "port_id = %u, unk = %d",
          port_id, unk);
    c.port_id = port_id;
    c.data = data;
    c.size = size;
    c.unk = unk;
    combiner_run(&g_state.mutex, sceVoiceWriteToIPort_locked, &c);
    return c.ret;
}
