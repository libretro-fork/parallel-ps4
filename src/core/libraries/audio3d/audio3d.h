/* SPDX-FileCopyrightText: Copyright 2025-2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef SHADPS4_AUDIO3D_H
#define SHADPS4_AUDIO3D_H

/* libSceAudio3d: ports whose beds and objects are mixed (audio3d_port.c) and go out through
 * AudioOut, and AudioOut ports a game opens through it. MSVC C89. */

#include <boolean.h>
#include <retro_atomic.h>
#include <retro_common_api.h>

#include "common/pin_gate.h"
#include "common/types.h"
#include "core/libraries/audio/audioout_c.h"
#include "core/libraries/audio3d/audio3d_port.h"

RETRO_BEGIN_DECLS

typedef u32 OrbisAudio3dPortId;
typedef u32 OrbisAudio3dObjectId;
typedef u32 OrbisAudio3dAmbisonics;

#define ORBIS_AUDIO3D_OBJECT_INVALID 0xFFFFFFFFu
#define ORBIS_AUDIO3D_PORT_INVALID 0xFFFFFFFFu
#define AUDIO3D_MAX_PORTS 4

/* The enums: the values the game passes. Fields and parameters that hold them are u32. */
enum OrbisAudio3dRate { ORBIS_AUDIO3D_RATE_48000 = 0 };

enum OrbisAudio3dBufferMode {
    ORBIS_AUDIO3D_BUFFER_NO_ADVANCE = 0,
    ORBIS_AUDIO3D_BUFFER_ADVANCE_NO_PUSH = 1,
    ORBIS_AUDIO3D_BUFFER_ADVANCE_AND_PUSH = 2
};

enum OrbisAudio3dFormat { ORBIS_AUDIO3D_FORMAT_S16 = 0, ORBIS_AUDIO3D_FORMAT_FLOAT = 1 };

enum OrbisAudio3dOutputRoute {
    ORBIS_AUDIO3D_OUTPUT_BOTH = 0,
    ORBIS_AUDIO3D_OUTPUT_HMU_ONLY = 1,
    ORBIS_AUDIO3D_OUTPUT_TV_ONLY = 2
};

enum OrbisAudio3dBlocking { ORBIS_AUDIO3D_BLOCKING_ASYNC = 0, ORBIS_AUDIO3D_BLOCKING_SYNC = 1 };

enum OrbisAudio3dAttributeId {
    ORBIS_AUDIO3D_ATTRIBUTE_PCM = 1,
    ORBIS_AUDIO3D_ATTRIBUTE_PRIORITY = 2,
    ORBIS_AUDIO3D_ATTRIBUTE_POSITION = 3,
    ORBIS_AUDIO3D_ATTRIBUTE_SPREAD = 4,
    ORBIS_AUDIO3D_ATTRIBUTE_GAIN = 5,
    ORBIS_AUDIO3D_ATTRIBUTE_PASSTHROUGH = 6,
    ORBIS_AUDIO3D_ATTRIBUTE_RESET_STATE = 7,
    ORBIS_AUDIO3D_ATTRIBUTE_APPLICATION_SPECIFIC = 8,
    ORBIS_AUDIO3D_ATTRIBUTE_AMBISONICS = 9,
    ORBIS_AUDIO3D_ATTRIBUTE_RESTRICTED = 10,
    ORBIS_AUDIO3D_ATTRIBUTE_OUTPUT_ROUTE = 11
};

typedef struct OrbisAudio3dOpenParameters {
    u64 size_this;
    u32 granularity;
    u32 rate; /* OrbisAudio3dRate */
    u32 max_objects;
    u32 queue_depth;
    u32 buffer_mode; /* OrbisAudio3dBufferMode */
    u32 _pad;
    u32 num_beds;
} OrbisAudio3dOpenParameters;

typedef struct OrbisAudio3dPcm {
    u32 format; /* OrbisAudio3dFormat */
    void* sample_buffer;
    u32 num_samples;
} OrbisAudio3dPcm;

typedef struct OrbisAudio3dAttribute {
    u32 attribute_id; /* OrbisAudio3dAttributeId */
    u32 _pad;
    void* value;
    u64 value_size;
} OrbisAudio3dAttribute;

/* An AudioOut port opened by the game through sceAudio3dAudioOutOpen: what the game outputs
 * to it waits in a ring until the port's push sends it on. Free: state 0, handle -1. */
typedef struct audio3d_associated_port {
    retro_atomic_int_t state; /* free, opening, live */
    s32 handle;
    u32 samples_per_buffer;
    bool is_float;
    audio3d_ring_t pending;
} audio3d_associated_port_t;

#define AUDIO3D_MAX_ASSOCIATED_PORTS 8

/* A port: its sound and objects in audio3d_port.c, made when it opens. Calls pin it through
 * its gate; closing it waits for those in to leave. */
typedef struct audio3d_lib_port {
    retro_atomic_int_t state; /* free, opening, open, closing */
    pin_gate_t gate;
    OrbisAudio3dOpenParameters parameters;
    /* The AudioOut port the mix goes out through, opened on the first push; -1 until then */
    retro_atomic_int_t audio_out_handle;
    audio3d_port_t core;
    audio3d_associated_port_t audioout_ports[AUDIO3D_MAX_ASSOCIATED_PORTS];
} audio3d_lib_port_t;

s32 PS4_SYSV_ABI sceAudio3dAudioOutClose(s32 handle);
s32 PS4_SYSV_ABI sceAudio3dAudioOutOpen(OrbisAudio3dPortId port_id, s32 user_id, s32 type,
                                        s32 index, u32 len, u32 freq, u32 param);
s32 PS4_SYSV_ABI sceAudio3dAudioOutOutput(s32 handle, void* ptr);
s32 PS4_SYSV_ABI sceAudio3dAudioOutOutputs(audioout_output_param_t* param, u32 num);
s32 PS4_SYSV_ABI sceAudio3dBedWrite(OrbisAudio3dPortId port_id, u32 num_channels, u32 format,
                                    void* buffer, u32 num_samples);
s32 PS4_SYSV_ABI sceAudio3dBedWrite2(OrbisAudio3dPortId port_id, u32 num_channels, u32 format,
                                     void* buffer, u32 num_samples, u32 output_route,
                                     bool restricted);
s32 PS4_SYSV_ABI sceAudio3dCreateSpeakerArray(void);
s32 PS4_SYSV_ABI sceAudio3dDeleteSpeakerArray(void);
s32 PS4_SYSV_ABI sceAudio3dGetDefaultOpenParameters(OrbisAudio3dOpenParameters* params);
s32 PS4_SYSV_ABI sceAudio3dGetSpeakerArrayMemorySize(void);
s32 PS4_SYSV_ABI sceAudio3dGetSpeakerArrayMixCoefficients(void);
s32 PS4_SYSV_ABI sceAudio3dGetSpeakerArrayMixCoefficients2(void);
s32 PS4_SYSV_ABI sceAudio3dInitialize(s64 reserved);
s32 PS4_SYSV_ABI sceAudio3dObjectReserve(OrbisAudio3dPortId port_id,
                                         OrbisAudio3dObjectId* object_id);
s32 PS4_SYSV_ABI sceAudio3dObjectSetAttribute(OrbisAudio3dPortId port_id,
                                              OrbisAudio3dObjectId object_id, u32 attribute_id,
                                              const void* attribute, u64 attribute_size);
s32 PS4_SYSV_ABI sceAudio3dObjectSetAttributes(OrbisAudio3dPortId port_id,
                                               OrbisAudio3dObjectId object_id, u64 num_attributes,
                                               const OrbisAudio3dAttribute* attribute_array);
s32 PS4_SYSV_ABI sceAudio3dObjectUnreserve(OrbisAudio3dPortId port_id,
                                           OrbisAudio3dObjectId object_id);
s32 PS4_SYSV_ABI sceAudio3dPortAdvance(OrbisAudio3dPortId port_id);
s32 PS4_SYSV_ABI sceAudio3dPortClose(OrbisAudio3dPortId port_id);
s32 PS4_SYSV_ABI sceAudio3dPortCreate(u32 granularity, u32 rate, s64 reserved,
                                      OrbisAudio3dPortId* port_id);
s32 PS4_SYSV_ABI sceAudio3dPortDestroy(void);
s32 PS4_SYSV_ABI sceAudio3dPortFlush(OrbisAudio3dPortId port_id);
s32 PS4_SYSV_ABI sceAudio3dPortFreeState(void);
s32 PS4_SYSV_ABI sceAudio3dPortGetAttributesSupported(OrbisAudio3dPortId port_id, u32* capabilities,
                                                      u32* num_capabilities);
s32 PS4_SYSV_ABI sceAudio3dPortGetList(void);
s32 PS4_SYSV_ABI sceAudio3dPortGetParameters(void);
s32 PS4_SYSV_ABI sceAudio3dPortGetQueueLevel(OrbisAudio3dPortId port_id, u32* queue_level,
                                             u32* queue_available);
s32 PS4_SYSV_ABI sceAudio3dPortGetState(void);
s32 PS4_SYSV_ABI sceAudio3dPortGetStatus(void);
s32 PS4_SYSV_ABI sceAudio3dPortOpen(s32 user_id, const OrbisAudio3dOpenParameters* parameters,
                                    OrbisAudio3dPortId* port_id);
s32 PS4_SYSV_ABI sceAudio3dPortPush(OrbisAudio3dPortId port_id, u32 blocking);
s32 PS4_SYSV_ABI sceAudio3dPortQueryDebug(void);
s32 PS4_SYSV_ABI sceAudio3dPortSetAttribute(OrbisAudio3dPortId port_id, u32 attribute_id,
                                            void* attribute, u64 attribute_size);
s32 PS4_SYSV_ABI sceAudio3dReportRegisterHandler(void);
s32 PS4_SYSV_ABI sceAudio3dReportUnregisterHandler(void);
s32 PS4_SYSV_ABI sceAudio3dSetGpuRenderer(void);
s32 PS4_SYSV_ABI sceAudio3dStrError(void);
s32 PS4_SYSV_ABI sceAudio3dTerminate(void);

RETRO_END_DECLS

#endif
