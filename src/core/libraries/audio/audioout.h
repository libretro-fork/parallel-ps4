/* SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef SHADPS4_AUDIO_AUDIOOUT_H
#define SHADPS4_AUDIO_AUDIOOUT_H

/* libSceAudioOut. Defined in audioout.c, registered in core/libraries/libs.cpp. */

#include <boolean.h>
#include <retro_atomic.h>
#include <retro_common_api.h>
#include <stdint.h>

#include "common/combine.h"
#include "common/types.h"
#include "core/libraries/audio/audioout_backend.h"

RETRO_BEGIN_DECLS

/* Main up to 8 ports, BGM 1 port, voice up to 4 ports,
 * personal up to 4 ports, padspk up to 5 ports, aux 1 port */
#define ORBIS_AUDIO_OUT_NUM_PORTS 25
#define ORBIS_AUDIO_OUT_VOLUME_0DB 32768              /* max volume value */
#define ORBIS_AUDIO_OUT_MIXLEVEL_PADSPK_DEFAULT 11626 /* default -9db */
#define ORBIS_AUDIO_OUT_MIXLEVEL_PADSPK_0DB 32768     /* max volume */

#define ORBIS_AUDIO_OUT_PARAM_ATTR_RESTRICTED 0x00010000
#define ORBIS_AUDIO_OUT_PARAM_ATTR_MIX_TO_MAIN 0x00020000

/* Volume flags */
#define ORBIS_AUDIO_VOLUME_FLAG_L_CH (1u << 0)
#define ORBIS_AUDIO_VOLUME_FLAG_R_CH (1u << 1)
#define ORBIS_AUDIO_VOLUME_FLAG_C_CH (1u << 2)
#define ORBIS_AUDIO_VOLUME_FLAG_LFE_CH (1u << 3)
#define ORBIS_AUDIO_VOLUME_FLAG_LS_CH (1u << 4)
#define ORBIS_AUDIO_VOLUME_FLAG_RS_CH (1u << 5)
#define ORBIS_AUDIO_VOLUME_FLAG_LE_CH (1u << 6)
#define ORBIS_AUDIO_VOLUME_FLAG_RE_CH (1u << 7)

/* Port state constants */
#define ORBIS_AUDIO_OUT_STATE_OUTPUT_UNKNOWN 0x00
#define ORBIS_AUDIO_OUT_STATE_OUTPUT_CONNECTED_PRIMARY 0x01
#define ORBIS_AUDIO_OUT_STATE_OUTPUT_CONNECTED_SECONDARY 0x02
#define ORBIS_AUDIO_OUT_STATE_OUTPUT_CONNECTED_TERTIARY 0x04
#define ORBIS_AUDIO_OUT_STATE_OUTPUT_CONNECTED_HEADPHONE 0x40
#define ORBIS_AUDIO_OUT_STATE_OUTPUT_CONNECTED_EXTERNAL 0x80

/* The enums: the values the game passes. Parameters and fields that hold them are s32 or u32. */
enum OrbisAudioOutPort {
    ORBIS_AUDIO_OUT_PORT_MAIN = 0,
    ORBIS_AUDIO_OUT_PORT_BGM = 1,
    ORBIS_AUDIO_OUT_PORT_VOICE = 2,
    ORBIS_AUDIO_OUT_PORT_PERSONAL = 3,
    ORBIS_AUDIO_OUT_PORT_PAD_SPK = 4,
    ORBIS_AUDIO_OUT_PORT_AUDIO3D = 126,
    ORBIS_AUDIO_OUT_PORT_AUX = 127
};

enum OrbisAudioOutParamFormat {
    ORBIS_AUDIO_OUT_PARAM_FORMAT_S16_MONO = 0,
    ORBIS_AUDIO_OUT_PARAM_FORMAT_S16_STEREO = 1,
    ORBIS_AUDIO_OUT_PARAM_FORMAT_S16_8CH = 2,
    ORBIS_AUDIO_OUT_PARAM_FORMAT_FLOAT_MONO = 3,
    ORBIS_AUDIO_OUT_PARAM_FORMAT_FLOAT_STEREO = 4,
    ORBIS_AUDIO_OUT_PARAM_FORMAT_FLOAT_8CH = 5,
    ORBIS_AUDIO_OUT_PARAM_FORMAT_S16_8CH_STD = 6,
    ORBIS_AUDIO_OUT_PARAM_FORMAT_FLOAT_8CH_STD = 7
};

enum OrbisAudioOutParamAttr {
    ORBIS_AUDIO_OUT_PARAM_ATTR_NONE = 0,
    ORBIS_AUDIO_OUT_PARAM_ATTR_VALUE_RESTRICTED = 1,
    ORBIS_AUDIO_OUT_PARAM_ATTR_VALUE_MIX_TO_MAIN = 2
};

/* OrbisAudioOutParamExtendedInformation, a u32: the format in bits 0-7, the attributes in
 * bits 16-19 */
#define ORBIS_AUDIO_OUT_PARAM_DATA_FORMAT(param) ((u32)(param) & 0xFFu)
#define ORBIS_AUDIO_OUT_PARAM_ATTRIBUTES(param) (((u32)(param) >> 16) & 0xFu)

typedef struct OrbisAudioOutOutputParam {
    s32 handle;
    void* ptr;
} OrbisAudioOutOutputParam;

typedef struct OrbisAudioOutPortState {
    u16 output;
    u8 channel;
    u8 reserved8_1[1];
    s16 volume;
    u16 rerouteCounter;
    u64 flag;
    u64 reserved64[2];
} OrbisAudioOutPortState;

typedef struct OrbisAudioOutSystemState {
    float loudness;
    u8 reserved8[4];
    u64 reserved64[3];
} OrbisAudioOutSystemState;

typedef struct PortOut {
    /* The game's buffer is handed to the frontend's mix in the game's own call, straight
     * from where the game wrote it: no copy of it, no thread, no timer. The call returns
     * once a buffer's time on the game's time, as the device paces it (PaceOutput), and the
     * mix holds it back besides while more than a frame is queued (frontend_audio_out.c).
     * The game's calls on a port run one at a time in writers; volume and mix level changes,
     * and what reads the port's shape, in control. Opening sets the port up under both,
     * closing takes its backend down under writers. */
    combiner_t writers;
    combiner_t control;
    /* The open's number while open, 0 while closed: a call that found the port open does
     * nothing once it has been closed (or opened again) meanwhile */
    retro_atomic_int_t open;
    /* The open whose backend is in impl (under writers and control), 0 for none */
    int impl_open;
    audioout_port_backend_t impl;
    s32 type;
    AudioFormatInfo format_info;
    u32 sample_rate;
    u32 buffer_frames;
    retro_atomic_64_t last_output_time;
    /* The call's pace, on the game's time (PaceOutput): the buffers returned since
     * pace_start, which is when the cadence (re)started. The port's writers only. */
    uint64_t pace_start;
    uint64_t pace_buffers;
    s32 volume[8];
    s32 userId;
    s32 mixLevelPadSpk;
    bool is_restricted;
    bool is_mix_to_main;
} PortOut;

/* Once, when the library is registered */
void audio_out_library_init(void);

s32 PS4_SYSV_ABI sceAudioOutDeviceIdOpen(void);
s32 PS4_SYSV_ABI sceAudioDeviceControlGet(void);
s32 PS4_SYSV_ABI sceAudioDeviceControlSet(void);
s32 PS4_SYSV_ABI sceAudioOutA3dControl(void);
s32 PS4_SYSV_ABI sceAudioOutA3dExit(void);
s32 PS4_SYSV_ABI sceAudioOutA3dInit(void);
s32 PS4_SYSV_ABI sceAudioOutAttachToApplicationByPid(void);
s32 PS4_SYSV_ABI sceAudioOutChangeAppModuleState(void);
s32 PS4_SYSV_ABI sceAudioOutClose(s32 handle);
s32 PS4_SYSV_ABI sceAudioOutDetachFromApplicationByPid(void);
s32 PS4_SYSV_ABI sceAudioOutExConfigureOutputMode(void);
s32 PS4_SYSV_ABI sceAudioOutExGetSystemInfo(void);
s32 PS4_SYSV_ABI sceAudioOutExPtClose(void);
s32 PS4_SYSV_ABI sceAudioOutExPtGetLastOutputTime(void);
s32 PS4_SYSV_ABI sceAudioOutExPtOpen(void);
s32 PS4_SYSV_ABI sceAudioOutExSystemInfoIsSupportedAudioOutExMode(void);
s32 PS4_SYSV_ABI sceAudioOutGetFocusEnablePid(void);
s32 PS4_SYSV_ABI sceAudioOutGetHandleStatusInfo(void);
s32 PS4_SYSV_ABI sceAudioOutGetInfo(void);
s32 PS4_SYSV_ABI sceAudioOutGetInfoOpenNum(void);
s32 PS4_SYSV_ABI sceAudioOutGetLastOutputTime(s32 handle, u64* output_time);
s32 PS4_SYSV_ABI sceAudioOutGetPortState(s32 handle, OrbisAudioOutPortState* state);
s32 PS4_SYSV_ABI sceAudioOutGetSimulatedBusUsableStatusByBusType(void);
s32 PS4_SYSV_ABI sceAudioOutGetSimulatedHandleStatusInfo(void);
s32 PS4_SYSV_ABI sceAudioOutGetSimulatedHandleStatusInfo2(void);
s32 PS4_SYSV_ABI sceAudioOutGetSparkVss(void);
s32 PS4_SYSV_ABI sceAudioOutGetSystemState(OrbisAudioOutSystemState* state);
s32 PS4_SYSV_ABI sceAudioOutInit(void);
s32 PS4_SYSV_ABI sceAudioOutInitIpmiGetSession(void);
s32 PS4_SYSV_ABI sceAudioOutMasteringGetState(void);
s32 PS4_SYSV_ABI sceAudioOutMasteringInit(u32 flags);
s32 PS4_SYSV_ABI sceAudioOutMasteringSetParam(void);
s32 PS4_SYSV_ABI sceAudioOutMasteringTerm(void);
s32 PS4_SYSV_ABI sceAudioOutMbusInit(void);
s32 PS4_SYSV_ABI sceAudioOutOpen(s32 user_id, s32 port_type, s32 index, u32 length, u32 sample_rate,
                                 u32 param_type);
s32 PS4_SYSV_ABI sceAudioOutOpenEx(void);
s32 PS4_SYSV_ABI sceAudioOutOutput(s32 handle, void* ptr);
s32 PS4_SYSV_ABI sceAudioOutOutputs(OrbisAudioOutOutputParam* param, u32 num);
s32 PS4_SYSV_ABI sceAudioOutPtClose(void);
s32 PS4_SYSV_ABI sceAudioOutPtGetLastOutputTime(void);
s32 PS4_SYSV_ABI sceAudioOutPtOpen(void);
s32 PS4_SYSV_ABI sceAudioOutSetConnections(void);
s32 PS4_SYSV_ABI sceAudioOutSetConnectionsForUser(void);
s32 PS4_SYSV_ABI sceAudioOutSetDevConnection(void);
s32 PS4_SYSV_ABI sceAudioOutSetHeadphoneOutMode(void);
s32 PS4_SYSV_ABI sceAudioOutSetJediJackVolume(void);
s32 PS4_SYSV_ABI sceAudioOutSetJediSpkVolume(void);
s32 PS4_SYSV_ABI sceAudioOutSetMainOutput(void);
s32 PS4_SYSV_ABI sceAudioOutSetMixLevelPadSpk(s32 handle, s32 mixLevel);
s32 PS4_SYSV_ABI sceAudioOutSetMorpheusParam(void);
s32 PS4_SYSV_ABI sceAudioOutSetMorpheusWorkingMode(void);
s32 PS4_SYSV_ABI sceAudioOutSetPortConnections(void);
s32 PS4_SYSV_ABI sceAudioOutSetPortStatuses(void);
s32 PS4_SYSV_ABI sceAudioOutSetRecMode(void);
s32 PS4_SYSV_ABI sceAudioOutSetSparkParam(void);
s32 PS4_SYSV_ABI sceAudioOutSetUsbVolume(void);
s32 PS4_SYSV_ABI sceAudioOutSetVolume(s32 handle, s32 flag, s32* vol);
s32 PS4_SYSV_ABI sceAudioOutSetVolumeDown(void);
s32 PS4_SYSV_ABI sceAudioOutStartAuxBroadcast(void);
s32 PS4_SYSV_ABI sceAudioOutStartSharePlay(void);
s32 PS4_SYSV_ABI sceAudioOutStopAuxBroadcast(void);
s32 PS4_SYSV_ABI sceAudioOutStopSharePlay(void);
s32 PS4_SYSV_ABI sceAudioOutSuspendResume(void);
s32 PS4_SYSV_ABI sceAudioOutSysConfigureOutputMode(void);
s32 PS4_SYSV_ABI sceAudioOutSysGetHdmiMonitorInfo(void);
s32 PS4_SYSV_ABI sceAudioOutSysGetSystemInfo(void);
s32 PS4_SYSV_ABI sceAudioOutSysHdmiMonitorInfoIsSupportedAudioOutMode(void);
s32 PS4_SYSV_ABI sceAudioOutSystemControlGet(void);
s32 PS4_SYSV_ABI sceAudioOutSystemControlSet(void);
s32 PS4_SYSV_ABI sceAudioOutSparkControlSetEqCoef(void);
s32 PS4_SYSV_ABI sceAudioOutSetSystemDebugState(void);

RETRO_END_DECLS

#endif
