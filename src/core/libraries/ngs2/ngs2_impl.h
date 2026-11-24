/* SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef SHADPS4_NGS2_IMPL_H
#define SHADPS4_NGS2_IMPL_H

#include <stddef.h>
#include "common/types.h"

#include <retro_common_api.h>

RETRO_BEGIN_DECLS

typedef struct OrbisNgs2RackOption OrbisNgs2RackOption;

#define ORBIS_NGS2_SYSTEM_NAME_LENGTH (16)
#define ORBIS_NGS2_RACK_NAME_LENGTH (16)

typedef uintptr_t OrbisNgs2Handle;

typedef struct OrbisNgs2ContextBufferInfo {
    void* hostBuffer;
    size_t hostBufferSize;
    uintptr_t reserved[5];
    uintptr_t userData;
} OrbisNgs2ContextBufferInfo;

typedef struct OrbisNgs2SystemOption {
    size_t size;
    char name[ORBIS_NGS2_SYSTEM_NAME_LENGTH];

    u32 flags;
    u32 maxGrainSamples;
    u32 numGrainSamples;
    u32 sampleRate;
    u32 aReserved[6];
} OrbisNgs2SystemOption;

typedef s32(PS4_SYSV_ABI* OrbisNgs2BufferAllocHandler)(OrbisNgs2ContextBufferInfo* io_buffer_info);
typedef s32(PS4_SYSV_ABI* OrbisNgs2BufferFreeHandler)(OrbisNgs2ContextBufferInfo* io_buffer_info);

typedef struct OrbisNgs2SystemInfo {
    char name[ORBIS_NGS2_SYSTEM_NAME_LENGTH]; /* 0 */

    OrbisNgs2Handle systemHandle;          /* 16 */
    OrbisNgs2ContextBufferInfo bufferInfo; /* 24 */

    u32 uid;             /* 88 */
    u32 minGrainSamples; /* 92 */
    u32 maxGrainSamples; /* 96 */

    u32 stateFlags;        /* 100 */
    u32 rackCount;         /* 104 */
    float lastRenderRatio; /* 108 */
    s64 lastRenderTick;    /* 112 */
    s64 renderCount;       /* 120 */
    u32 sampleRate;        /* 128 */
    u32 numGrainSamples;   /* 132 */
} OrbisNgs2SystemInfo;

typedef struct OrbisNgs2RackInfo {
    char name[ORBIS_NGS2_RACK_NAME_LENGTH]; /* 0 */

    OrbisNgs2Handle rackHandle;            /* 16 */
    OrbisNgs2ContextBufferInfo bufferInfo; /* 24 */

    OrbisNgs2Handle ownerSystemHandle; /* 88 */

    u32 type;            /* 96 */
    u32 rackId;          /* 100 */
    u32 uid;             /* 104 */
    u32 minGrainSamples; /* 108 */
    u32 maxGrainSamples; /* 112 */
    u32 maxVoices;       /* 116 */
    u32 maxChannelWorks; /* 120 */
    u32 maxInputs;       /* 124 */
    u32 maxMatrices;     /* 128 */
    u32 maxPorts;        /* 132 */

    u32 stateFlags;             /* 136 */
    float lastProcessRatio;     /* 140 */
    u64 lastProcessTick;        /* 144 */
    u64 renderCount;            /* 152 */
    u32 activeVoiceCount;       /* 160 */
    u32 activeChannelWorkCount; /* 164 */
} OrbisNgs2RackInfo;

RETRO_END_DECLS

#endif
