/* SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef SHADPS4_AUDIO_AUDIOIN_H
#define SHADPS4_AUDIO_AUDIOIN_H

/* libSceAudioIn. Defined in audioin.c, registered in core/libraries/libs.cpp. */

#include <retro_atomic.h>
#include <retro_common_api.h>

#include "common/combine.h"
#include "common/types.h"
#include "core/libraries/audio/audioin_backend.h"

RETRO_BEGIN_DECLS

#define ORBIS_AUDIO_IN_NUM_PORTS 7

/* OrbisAudioInParamFormat */
#define ORBIS_AUDIO_IN_PARAM_FORMAT_S16_MONO 0u
#define ORBIS_AUDIO_IN_PARAM_FORMAT_S16_STEREO 2u

/* OrbisAudioInType */
#define ORBIS_AUDIO_IN_TYPE_VOICE_CHAT 0u
#define ORBIS_AUDIO_IN_TYPE_GENERAL 1u
#define ORBIS_AUDIO_IN_TYPE_VOICE_RECOGNITION 5u

#define ORBIS_AUDIO_IN_SILENT_STATE_DEVICE_NONE 0x00000001
#define ORBIS_AUDIO_IN_SILENT_STATE_PRIORITY_LOW 0x00000002
#define ORBIS_AUDIO_IN_SILENT_STATE_USER_SETTING 0x0000000
#define ORBIS_AUDIO_IN_SILENT_STATE_UNABLE_FORMAT 0x00000008

typedef struct PortIn {
    /* The calls that use the backend run one at a time here, and so does Close's reset of it */
    combiner_t access;
    /* The open's number while open, 0 while closed: a call that found the port open does
     * nothing once it has been closed (or opened again) meanwhile */
    retro_atomic_int_t open;
    /* The open whose backend is in impl (under access), 0 for none */
    int impl_open;
    audioin_port_backend_t impl;
    u32 type;
    u32 format;
    u32 samples_num;
    u32 freq;
    u32 channels_num;
    u32 sample_size;
} PortIn;

/* Once, when the library is registered */
void audio_in_library_init(void);

int PS4_SYSV_ABI sceAudioInOpen(s32 userId, u32 type, u32 index, u32 len, u32 freq, u32 param);
int PS4_SYSV_ABI sceAudioInHqOpen(s32 userId, u32 type, u32 index, u32 len, u32 freq, u32 param);
int PS4_SYSV_ABI sceAudioInClose(s32 handle);
int PS4_SYSV_ABI sceAudioInInput(s32 handle, void* dest);
int PS4_SYSV_ABI sceAudioInGetSilentState(s32 handle);
int PS4_SYSV_ABI sceAudioInChangeAppModuleState(void);
int PS4_SYSV_ABI sceAudioInCountPorts(void);
int PS4_SYSV_ABI sceAudioInDeviceHqOpen(void);
int PS4_SYSV_ABI sceAudioInDeviceIdHqOpen(void);
int PS4_SYSV_ABI sceAudioInDeviceIdOpen(void);
int PS4_SYSV_ABI sceAudioInDeviceOpen(void);
int PS4_SYSV_ABI sceAudioInDeviceOpenEx(void);
int PS4_SYSV_ABI sceAudioInExtClose(void);
int PS4_SYSV_ABI sceAudioInExtCtrl(void);
int PS4_SYSV_ABI sceAudioInExtInput(void);
int PS4_SYSV_ABI sceAudioInExtOpen(void);
int PS4_SYSV_ABI sceAudioInExtSetAecMode(void);
int PS4_SYSV_ABI sceAudioInGetGain(void);
int PS4_SYSV_ABI sceAudioInGetHandleStatusInfo(void);
int PS4_SYSV_ABI sceAudioInGetRerouteCount(void);
int PS4_SYSV_ABI sceAudioInHqOpenEx(void);
int PS4_SYSV_ABI sceAudioInInit(void);
int PS4_SYSV_ABI sceAudioInInputs(void);
int PS4_SYSV_ABI sceAudioInIsSharedDevice(void);
int PS4_SYSV_ABI sceAudioInOpenEx(void);
int PS4_SYSV_ABI sceAudioInSetAllMute(void);
int PS4_SYSV_ABI sceAudioInSetCompressorPreGain(void);
int PS4_SYSV_ABI sceAudioInSetConnections(void);
int PS4_SYSV_ABI sceAudioInSetConnectionsForUser(void);
int PS4_SYSV_ABI sceAudioInSetDevConnection(void);
int PS4_SYSV_ABI sceAudioInSetFocusForUser(void);
int PS4_SYSV_ABI sceAudioInSetMode(void);
int PS4_SYSV_ABI sceAudioInSetMode2(void);
int PS4_SYSV_ABI sceAudioInSetPortConnections(void);
int PS4_SYSV_ABI sceAudioInSetPortStatuses(void);
int PS4_SYSV_ABI sceAudioInSetSparkParam(void);
int PS4_SYSV_ABI sceAudioInSetSparkSideTone(void);
int PS4_SYSV_ABI sceAudioInSetUsbGain(void);
int PS4_SYSV_ABI sceAudioInSetUserMute(void);
int PS4_SYSV_ABI sceAudioInVmicCreate(void);
int PS4_SYSV_ABI sceAudioInVmicDestroy(void);
int PS4_SYSV_ABI sceAudioInVmicWrite(void);

RETRO_END_DECLS

#endif
