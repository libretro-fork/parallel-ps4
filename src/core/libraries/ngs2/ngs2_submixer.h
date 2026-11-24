/* SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef SHADPS4_NGS2_SUBMIXER_H
#define SHADPS4_NGS2_SUBMIXER_H

#include "core/libraries/ngs2/ngs2.h"

#include <retro_common_api.h>

RETRO_BEGIN_DECLS

typedef struct OrbisNgs2SubmixerRackOption {
    OrbisNgs2RackOption rackOption;
    u32 maxChannels;
    u32 maxEnvelopePoints;
    u32 maxFilters;
    u32 maxInputs;
    u32 numPeakMeterBlocks;
} OrbisNgs2SubmixerRackOption;

typedef struct OrbisNgs2SubmixerVoiceSetupParam {
    OrbisNgs2VoiceParamHeader header;
    u32 numIoChannels;
    u32 flags;
} OrbisNgs2SubmixerVoiceSetupParam;

typedef struct OrbisNgs2SubmixerVoiceEnvelopeParam {
    OrbisNgs2VoiceParamHeader header;

    u32 numForwardPoints;
    u32 numReleasePoints;
    const OrbisNgs2EnvelopePoint* aPoint;
} OrbisNgs2SubmixerVoiceEnvelopeParam;

typedef struct OrbisNgs2SubmixerVoiceCompressorParam {
    OrbisNgs2VoiceParamHeader header;

    u32 flags;
    float threshold;
    float ratio;
    float knee;
    float attackTime;
    float releaseTime;
    float level;
    u32 reserved;
} OrbisNgs2SubmixerVoiceCompressorParam;

typedef struct OrbisNgs2SubmixerVoiceDistortionParam {
    OrbisNgs2VoiceParamHeader header;

    u32 flags;
    float a;
    float b;
    float clip;
    float gate;
    float wetLevel;
    float dryLevel;
    u32 reserved;
} OrbisNgs2SubmixerVoiceDistortionParam;

typedef struct OrbisNgs2SubmixerVoiceUserFxParam {
    OrbisNgs2VoiceParamHeader header;

    OrbisNgs2UserFxProcessHandler handler;

    uintptr_t userData0;
    uintptr_t userData1;
    uintptr_t userData2;
} OrbisNgs2SubmixerVoiceUserFxParam;

typedef struct OrbisNgs2SubmixerVoicePeakMeterParam {
    OrbisNgs2VoiceParamHeader header;

    u32 enableFlag;
    u32 reserved;
} OrbisNgs2SubmixerVoicePeakMeterParam;

typedef struct OrbisNgs2SubmixerVoiceFilterParam {
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
} OrbisNgs2SubmixerVoiceFilterParam;

typedef struct OrbisNgs2SubmixerVoiceNumFilters {
    OrbisNgs2VoiceParamHeader header;

    u32 numFilters;
    u32 reserved;
} OrbisNgs2SubmixerVoiceNumFilters;

typedef struct OrbisNgs2SubmixerVoiceState {
    OrbisNgs2VoiceState voiceState;
    float envelopeHeight;
    float peakHeight;
    float compressorHeight;
} OrbisNgs2SubmixerVoiceState;

typedef struct OrbisNgs2SubmixerRackInfo {
    OrbisNgs2RackInfo rackInfo;
    u32 maxChannels;
    u32 maxEnvelopePoints;
    u32 maxFilters;
    u32 maxInputs;
} OrbisNgs2SubmixerRackInfo;

RETRO_END_DECLS

#endif
