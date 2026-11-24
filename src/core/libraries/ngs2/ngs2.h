/* SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef SHADPS4_NGS2_H
#define SHADPS4_NGS2_H

#include "core/libraries/ngs2/ngs2_impl.h"

#include "common/types.h"

#include <retro_common_api.h>

RETRO_BEGIN_DECLS

typedef s32(PS4_SYSV_ABI* OrbisNgs2ParseReadHandler)(uintptr_t user_data, u32 offset, void* data,
                                                     size_t size);

typedef u32 OrbisNgs2HandleType;
#define ORBIS_NGS2_HANDLE_TYPE_INVALID 0
#define ORBIS_NGS2_HANDLE_TYPE_SYSTEM 1
#define ORBIS_NGS2_HANDLE_TYPE_RACK 2
#define ORBIS_NGS2_HANDLE_TYPE_VOICE 3
#define ORBIS_NGS2_HANDLE_TYPE_VOICE_CONTROL 6

#define ORBIS_NGS2_MAX_VOICE_CHANNELS (8)
#define ORBIS_NGS2_WAVEFORM_INFO_MAX_BLOCKS (4)
#define ORBIS_NGS2_MAX_MATRIX_LEVELS                                                               \
    ((ORBIS_NGS2_MAX_VOICE_CHANNELS * ORBIS_NGS2_MAX_VOICE_CHANNELS))

typedef struct OrbisNgs2WaveformFormat {
    u32 waveformType;
    u32 numChannels;
    u32 sampleRate;
    u32 configData;
    u32 frameOffset;
    u32 frameMargin;
} OrbisNgs2WaveformFormat;

typedef struct OrbisNgs2WaveformBlock {
    u32 dataOffset;
    u32 dataSize;
    u32 numRepeats;
    u32 numSkipSamples;
    u32 numSamples;
    u32 reserved;
    uintptr_t userData;
} OrbisNgs2WaveformBlock;

typedef struct OrbisNgs2WaveformInfo {
    OrbisNgs2WaveformFormat format;

    u32 dataOffset;
    u32 dataSize;

    u32 loopBeginPosition;
    u32 loopEndPosition;
    u32 numSamples;

    u32 audioUnitSize;
    u32 numAudioUnitSamples;
    u32 numAudioUnitPerFrame;

    u32 audioFrameSize;
    u32 numAudioFrameSamples;

    u32 numDelaySamples;

    u32 numBlocks;
    OrbisNgs2WaveformBlock aBlock[ORBIS_NGS2_WAVEFORM_INFO_MAX_BLOCKS];
} OrbisNgs2WaveformInfo;

typedef struct OrbisNgs2EnvelopePoint {
    u32 curve;
    u32 duration;
    float height;
} OrbisNgs2EnvelopePoint;

typedef struct OrbisNgs2UserFxProcessContext {
    float** aChannelData;
    uintptr_t userData0;
    uintptr_t userData1;
    uintptr_t userData2;
    u32 flags;
    u32 numChannels;
    u32 numGrainSamples;
    u32 sampleRate;
} OrbisNgs2UserFxProcessContext;

typedef s32(PS4_SYSV_ABI* OrbisNgs2UserFxProcessHandler)(OrbisNgs2UserFxProcessContext* context);

typedef struct OrbisNgs2UserFx2SetupContext {
    void* common;
    void* param;
    void* work;
    uintptr_t userData;
    u32 maxVoices;
    u32 voiceIndex;
    u64 reserved[4];
} OrbisNgs2UserFx2SetupContext;

typedef s32(PS4_SYSV_ABI* OrbisNgs2UserFx2SetupHandler)(OrbisNgs2UserFx2SetupContext* context);

typedef struct OrbisNgs2UserFx2CleanupContext {
    void* common;
    void* param;
    void* work;
    uintptr_t userData;
    u32 maxVoices;
    u32 voiceIndex;
    u64 reserved[4];
} OrbisNgs2UserFx2CleanupContext;

typedef s32(PS4_SYSV_ABI* OrbisNgs2UserFx2CleanupHandler)(OrbisNgs2UserFx2CleanupContext* context);

typedef struct OrbisNgs2UserFx2ControlContext {
    const void* data;
    size_t dataSize;
    void* common;
    void* param;
    uintptr_t userData;
    u64 reserved[4];
} OrbisNgs2UserFx2ControlContext;

typedef s32(PS4_SYSV_ABI* OrbisNgs2UserFx2ControlHandler)(OrbisNgs2UserFx2ControlContext* context);

typedef struct OrbisNgs2UserFx2ProcessContext {
    float** aChannelData;
    void* common;
    const void* param;
    void* work;
    void* state;
    uintptr_t userData;
    u32 flags;
    u32 numInputChannels;
    u32 numOutputChannels;
    u32 numGrainSamples;
    u32 sampleRate;
    u32 reserved;
    u64 reserved2[4];
} OrbisNgs2UserFx2ProcessContext;

typedef s32(PS4_SYSV_ABI* OrbisNgs2UserFx2ProcessHandler)(OrbisNgs2UserFx2ProcessContext* context);

typedef struct OrbisNgs2BufferAllocator {
    OrbisNgs2BufferAllocHandler allocHandler;
    OrbisNgs2BufferFreeHandler freeHandler;
    uintptr_t userData;
} OrbisNgs2BufferAllocator;

typedef struct OrbisNgs2RenderBufferInfo {
    void* buffer;
    size_t bufferSize;
    u32 waveformType;
    u32 numChannels;
} OrbisNgs2RenderBufferInfo;

struct OrbisNgs2RackOption {
    size_t size;
    char name[ORBIS_NGS2_RACK_NAME_LENGTH];

    u32 flags;
    u32 maxGrainSamples;
    u32 maxVoices;
    u32 maxInputDelayBlocks;
    u32 maxMatrices;
    u32 maxPorts;
    u32 aReserved[20];
};

typedef struct OrbisNgs2VoiceParamHeader {
    u16 size;
    s16 next;
    u32 id;
} OrbisNgs2VoiceParamHeader;

typedef struct OrbisNgs2VoiceMatrixLevelsParam {
    OrbisNgs2VoiceParamHeader header;

    u32 matrixId;
    u32 numLevels;
    const float* aLevel;
} OrbisNgs2VoiceMatrixLevelsParam;

typedef struct OrbisNgs2VoicePortMatrixParam {
    OrbisNgs2VoiceParamHeader header;

    u32 port;
    s32 matrixId;
} OrbisNgs2VoicePortMatrixParam;

typedef struct OrbisNgs2VoicePortVolumeParam {
    OrbisNgs2VoiceParamHeader header;

    u32 port;
    float level;
} OrbisNgs2VoicePortVolumeParam;

typedef struct OrbisNgs2VoicePortDelayParam {
    OrbisNgs2VoiceParamHeader header;

    u32 port;
    u32 numSamples;
} OrbisNgs2VoicePortDelayParam;

typedef struct OrbisNgs2VoicePatchParam {
    OrbisNgs2VoiceParamHeader header;

    u32 port;
    u32 destInputId;
    OrbisNgs2Handle destHandle;
} OrbisNgs2VoicePatchParam;

typedef struct OrbisNgs2VoiceEventParam {
    OrbisNgs2VoiceParamHeader header;

    u32 eventId;
} OrbisNgs2VoiceEventParam;

typedef struct OrbisNgs2VoiceCallbackInfo {
    uintptr_t callbackData;
    OrbisNgs2Handle voiceHandle;
    u32 flag;
    u32 reserved;
    union {
        struct {
            uintptr_t userData;
            const void* data;
            u32 dataSize;
            u32 repeatedCount;
            u32 attributeFlags;
            u32 reserved2;
        } waveformBlock;
    } param;
} OrbisNgs2VoiceCallbackInfo;

typedef void(PS4_SYSV_ABI* OrbisNgs2VoiceCallbackHandler)(const OrbisNgs2VoiceCallbackInfo* info);

typedef struct OrbisNgs2VoiceCallbackParam {
    OrbisNgs2VoiceParamHeader header;
    OrbisNgs2VoiceCallbackHandler callbackHandler;

    uintptr_t callbackData;
    u32 flags;
    u32 reserved;
} OrbisNgs2VoiceCallbackParam;

typedef struct OrbisNgs2VoicePortInfo {
    s32 matrixId;
    float volume;
    u32 numDelaySamples;
    u32 destInputId;
    OrbisNgs2Handle destHandle;
} OrbisNgs2VoicePortInfo;

typedef struct OrbisNgs2VoiceMatrixInfo {
    u32 numLevels;
    float aLevel[ORBIS_NGS2_MAX_MATRIX_LEVELS];
} OrbisNgs2VoiceMatrixInfo;

typedef struct OrbisNgs2VoiceState {
    u32 stateFlags;
} OrbisNgs2VoiceState;

RETRO_END_DECLS

#endif
