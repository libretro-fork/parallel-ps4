/* SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef SHADPS4_LIBRARIES_VOICE_H
#define SHADPS4_LIBRARIES_VOICE_H

/* libSceVoice. No microphone or speaker of its own: the ports are kept, nothing is captured or
 * played. Registered in libs.cpp. */

#include <boolean.h>
#include <retro_common_api.h>

#include "common/types.h"

RETRO_BEGIN_DECLS

#define ORBIS_VOICE_MAX_PORT ((uint32_t)64)

typedef enum OrbisVoicePortType {
    ORBIS_VOICE_PORTTYPE_IN_DEVICE = 0,
    ORBIS_VOICE_PORTTYPE_IN_PCMAUDIO = 1,
    ORBIS_VOICE_PORTTYPE_IN_VOICE = 2,
    ORBIS_VOICE_PORTTYPE_OUT_PCMAUDIO = 3,
    ORBIS_VOICE_PORTTYPE_OUT_VOICE = 4,
    ORBIS_VOICE_PORTTYPE_OUT_DEVICE = 5
} OrbisVoicePortType;

typedef enum OrbisVoicePortAttr {
    ORBIS_VOICE_ATTR_1000 = 1000,
    ORBIS_VOICE_ATTR_1001 = 1001,
    ORBIS_VOICE_ATTR_1002 = 1002,
    ORBIS_VOICE_ATTR_1003 = 1003,
    ORBIS_VOICE_ATTR_2000 = 2000,
    ORBIS_VOICE_ATTR_2001 = 2001,
    ORBIS_VOICE_ATTR_2002 = 2002
} OrbisVoicePortAttr;

typedef struct OrbisVoicePortInfo {
    s32 port_type;
    s32 state;
    u32* edge;
    u32 byte_count;
    u32 frame_size;
    u16 edge_count;
    u16 reserved;
} OrbisVoicePortInfo;

typedef struct OrbisVoicePortParam {
    s32 type;
    s32 reserved0;
    u64 reserved1;
    u64 reserved2;
} OrbisVoicePortParam;
typedef char OrbisVoicePortParam_size_check[(sizeof(OrbisVoicePortParam) == 0x18) ? 1 : -1];

typedef struct OrbisVoiceInitParam {
    u64 app_type;
    u64 reserved0;
    u64 reserved1;
    u64 reserved2;
    u64 reserved3;
} OrbisVoiceInitParam;
typedef char OrbisVoiceInitParam_size_check[(sizeof(OrbisVoiceInitParam) == 0x28) ? 1 : -1];

typedef struct OrbisVoiceStartParam {
    void* memory;
    u32 memory_size;
    u32 reserved0;
    u64 reserved1;
    u64 reserved2;
} OrbisVoiceStartParam;
typedef char OrbisVoiceStartParam_size_check[(sizeof(OrbisVoiceStartParam) == 0x20) ? 1 : -1];

typedef struct OrbisVoiceResourceInfo {
    u16 field_0;
    u16 field_2;
    u16 field_4;
    u16 field_6;
} OrbisVoiceResourceInfo;
typedef char OrbisVoiceResourceInfo_size_check[(sizeof(OrbisVoiceResourceInfo) == 8) ? 1 : -1];

s32 PS4_SYSV_ABI sceVoiceConnectIPortToOPort(u32 in_port_id, u32 out_port_id);
s32 PS4_SYSV_ABI sceVoiceCreatePort(s32* port_id, const OrbisVoicePortParam* param);
s32 PS4_SYSV_ABI sceVoiceDeletePort(u32 port_id);
s32 PS4_SYSV_ABI sceVoiceDisconnectIPortFromOPort(u32 in_port_id, u32 out_port_id);
s32 PS4_SYSV_ABI sceVoiceEnd(void);
s32 PS4_SYSV_ABI sceVoiceGetBitRate(u32 port_id, u32* bitrate);
s32 PS4_SYSV_ABI sceVoiceGetMuteFlag(u32 port_id, bool* mute);
s32 PS4_SYSV_ABI sceVoiceGetPortAttr(u32 port_id, s32 attr_id, void* value, u32 size);
s32 PS4_SYSV_ABI sceVoiceGetPortInfo(u32 port_id, OrbisVoicePortInfo* info);
s32 PS4_SYSV_ABI sceVoiceGetResourceInfo(OrbisVoiceResourceInfo* info);
s32 PS4_SYSV_ABI sceVoiceGetVolume(u32 port_id, float* volume);
s32 PS4_SYSV_ABI sceVoiceInit(const OrbisVoiceInitParam* param, s32 container);
s32 PS4_SYSV_ABI sceVoiceInitHQ(const OrbisVoiceInitParam* param, s32 container);
s32 PS4_SYSV_ABI sceVoicePausePort(u32 port_id);
s32 PS4_SYSV_ABI sceVoicePausePortAll(void);
s32 PS4_SYSV_ABI sceVoiceReadFromOPort(u32 port_id, void* data, u32* size);
s32 PS4_SYSV_ABI sceVoiceResetPort(u32 port_id);
s32 PS4_SYSV_ABI sceVoiceResumePort(u32 port_id);
s32 PS4_SYSV_ABI sceVoiceResumePortAll(void);
s32 PS4_SYSV_ABI sceVoiceSetBitRate(u32 port_id, u32 bitrate);
s32 PS4_SYSV_ABI sceVoiceSetMuteFlag(u32 port_id, bool mute);
s32 PS4_SYSV_ABI sceVoiceSetMuteFlagAll(bool mute);
s32 PS4_SYSV_ABI sceVoiceSetThreadsParams(void);
s32 PS4_SYSV_ABI sceVoiceSetVolume(u32 port_id, float volume);
s32 PS4_SYSV_ABI sceVoiceStart(const OrbisVoiceStartParam* param);
s32 PS4_SYSV_ABI sceVoiceStop(void);
s32 PS4_SYSV_ABI sceVoiceUpdatePort(u32 port_id, const OrbisVoicePortParam* param);
s32 PS4_SYSV_ABI sceVoiceVADAdjustment(float param1, float param2);
s32 PS4_SYSV_ABI sceVoiceVADSetVersion(s32 version);
s32 PS4_SYSV_ABI sceVoiceWriteToIPort(u32 port_id, const void* data, u32* size, s16 unk);

/* Makes the library's state, once, before any call: libs.cpp's registration calls it */
void voice_library_init(void);

RETRO_END_DECLS

#endif
