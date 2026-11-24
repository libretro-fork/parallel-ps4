/* SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef SHADPS4_NGS2_SAMPLER_H
#define SHADPS4_NGS2_SAMPLER_H

#include "core/libraries/ngs2/ngs2.h"

#include <retro_common_api.h>

RETRO_BEGIN_DECLS

typedef struct OrbisNgs2SamplerRackOption {
    OrbisNgs2RackOption rackOption;
    u32 maxChannelWorks;
    u32 maxCodecCaches;
    u32 maxWaveformBlocks;
    u32 maxEnvelopePoints;
    u32 maxFilters;
    u32 maxAtrac9Decoders;
    u32 maxAtrac9ChannelWorks;
    u32 maxAjmAtrac9Decoders;
    u32 numPeakMeterBlocks;
} OrbisNgs2SamplerRackOption;

typedef struct OrbisNgs2SamplerVoiceSetupParam {
    OrbisNgs2VoiceParamHeader header;

    OrbisNgs2WaveformFormat format;
    u32 flags;
    u32 reserved;
} OrbisNgs2SamplerVoiceSetupParam;

typedef struct OrbisNgs2SamplerVoiceWaveformBlocksParam {
    OrbisNgs2VoiceParamHeader header;

    const void* data;
    u32 flags;
    u32 numBlocks;
    const OrbisNgs2WaveformBlock* aBlock;
    /* Blocks */
} OrbisNgs2SamplerVoiceWaveformBlocksParam;

typedef struct OrbisNgs2SamplerVoiceWaveformAddressParam {
    OrbisNgs2VoiceParamHeader header;

    const void* from;
    const void* to;
} OrbisNgs2SamplerVoiceWaveformAddressParam;

typedef struct OrbisNgs2SamplerVoiceWaveformFrameOffsetParam {
    OrbisNgs2VoiceParamHeader header;

    u32 frameOffset;
    u32 reserved;
} OrbisNgs2SamplerVoiceWaveformFrameOffsetParam;

typedef struct OrbisNgs2SamplerVoiceExitLoopParam {
    OrbisNgs2VoiceParamHeader header;
} OrbisNgs2SamplerVoiceExitLoopParam;

typedef struct OrbisNgs2SamplerVoicePitchParam {
    OrbisNgs2VoiceParamHeader header;

    float ratio;
    u32 reserved;
} OrbisNgs2SamplerVoicePitchParam;

typedef struct OrbisNgs2SamplerVoiceEnvelopeParam {
    OrbisNgs2VoiceParamHeader header;

    u32 numForwardPoints;
    u32 numReleasePoints;
    const OrbisNgs2EnvelopePoint* aPoint;
} OrbisNgs2SamplerVoiceEnvelopeParam;

typedef struct OrbisNgs2SamplerVoiceDistortionParam {
    OrbisNgs2VoiceParamHeader header;

    u32 flags;
    float a;
    float b;
    float clip;
    float gate;
    float wetLevel;
    float dryLevel;
    u32 reserved;
} OrbisNgs2SamplerVoiceDistortionParam;

typedef struct OrbisNgs2SamplerVoiceUserFxParam {
    OrbisNgs2VoiceParamHeader header;

    OrbisNgs2UserFxProcessHandler handler;

    uintptr_t userData0;
    uintptr_t userData1;
    uintptr_t userData2;
} OrbisNgs2SamplerVoiceUserFxParam;

typedef struct OrbisNgs2SamplerVoicePeakMeterParam {
    OrbisNgs2VoiceParamHeader header;

    u32 enableFlag;
    u32 reserved;
} OrbisNgs2SamplerVoicePeakMeterParam;

typedef struct OrbisNgs2SamplerVoiceFilterParam {
    OrbisNgs2VoiceParamHeader header;

    u32 index;
    u32 location;
    u32 type;
    u32 channelMask;
    union {
        struct {
            float i0;
            float i1;
            float i2;
            float o1;
            float o2;
        } direct;
        struct {
            float fc;
            float q;
            float level;
            u32 reserved;
            u32 reserved2;
        } fcq;
    } param;
    u32 reserved3;
} OrbisNgs2SamplerVoiceFilterParam;

typedef struct OrbisNgs2SamplerVoiceNumFilters {
    OrbisNgs2VoiceParamHeader header;

    u32 numFilters;
    u32 reserved;
} OrbisNgs2SamplerVoiceNumFilters;

typedef struct OrbisNgs2SamplerVoiceState {
    OrbisNgs2VoiceState voiceState;
    float envelopeHeight;
    float peakHeight;
    u32 reserved;
    u64 numDecodedSamples;
    u64 decodedDataSize;
    u64 userData;
    const void* waveformData;
} OrbisNgs2SamplerVoiceState;

typedef struct OrbisNgs2SamplerRackInfo {
    OrbisNgs2RackInfo rackInfo;
    u32 maxChannelWorks;
    u32 maxCodecCaches;
    u32 maxWaveformBlocks;
    u32 maxEnvelopePoints;
    u32 maxFilters;
    u32 maxAtrac9Decoders;
    u32 maxAtrac9ChannelWorks;
    u32 maxAjmAtrac9Decoders;
} OrbisNgs2SamplerRackInfo;

RETRO_END_DECLS

#endif
