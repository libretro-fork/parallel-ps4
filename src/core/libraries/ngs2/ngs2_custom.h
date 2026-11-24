/* SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef SHADPS4_NGS2_CUSTOM_H
#define SHADPS4_NGS2_CUSTOM_H

#include "core/libraries/ngs2/ngs2.h"
#include "core/libraries/ngs2/ngs2_reverb.h"

#include <retro_common_api.h>

RETRO_BEGIN_DECLS

#define ORBIS_NGS2_CUSTOM_MAX_MODULES (24)
#define ORBIS_NGS2_CUSTOM_MAX_PORTS (16)
#define ORBIS_NGS2_CUSTOM_DELAY_MAX_TAPS (8)

typedef struct OrbisNgs2CustomModuleOption {
    u32 size;
} OrbisNgs2CustomModuleOption;

typedef struct OrbisNgs2CustomEnvelopeModuleOption {
    OrbisNgs2CustomModuleOption customModuleOption;

    u32 maxPoints;
    u32 reserved;
} OrbisNgs2CustomEnvelopeModuleOption;

typedef struct OrbisNgs2CustomReverbModuleOption {
    OrbisNgs2CustomModuleOption customModuleOption;

    u32 reverbSize;
    u32 reserved;
} OrbisNgs2CustomReverbModuleOption;

typedef struct OrbisNgs2CustomChorusModuleOption {
    OrbisNgs2CustomModuleOption customModuleOption;

    u32 maxPhases;
    u32 reserved;
} OrbisNgs2CustomChorusModuleOption;

typedef struct OrbisNgs2CustomPeakMeterModuleOption {
    OrbisNgs2CustomModuleOption customModuleOption;
    u32 numBlocks;
    u32 reserved;
} OrbisNgs2CustomPeakMeterModuleOption;

typedef struct OrbisNgs2CustomDelayModuleOption {
    OrbisNgs2CustomModuleOption customModuleOption;

    u32 type;
    u32 maxTaps;
    float maxLength;
    u32 reserved;
} OrbisNgs2CustomDelayModuleOption;

typedef struct OrbisNgs2CustomPitchShiftModuleOption {
    OrbisNgs2CustomModuleOption customModuleOption;

    u32 quality;
} OrbisNgs2CustomPitchShiftModuleOption;

typedef struct OrbisNgs2CustomUserFx2ModuleOption {
    OrbisNgs2CustomModuleOption customModuleOption;

    OrbisNgs2UserFx2SetupHandler setupHandler;
    OrbisNgs2UserFx2CleanupHandler cleanupHandler;
    OrbisNgs2UserFx2ControlHandler controlHandler;
    OrbisNgs2UserFx2ProcessHandler processHandler;

    size_t commonSize;
    size_t paramSize;
    size_t workSize;
    uintptr_t userData;
} OrbisNgs2CustomUserFx2ModuleOption;

typedef struct OrbisNgs2CustomRackModuleInfo {
    const OrbisNgs2CustomModuleOption* option;

    u32 moduleId;
    u32 sourceBufferId;
    u32 extraBufferId;
    u32 destBufferId;
    u32 stateOffset;
    u32 stateSize;
    u32 reserved;
    u32 reserved2;
} OrbisNgs2CustomRackModuleInfo;

typedef struct OrbisNgs2CustomRackPortInfo {
    u32 sourceBufferId;
    u32 reserved;
} OrbisNgs2CustomRackPortInfo;

typedef struct OrbisNgs2CustomRackOption {
    OrbisNgs2RackOption rackOption;
    u32 stateSize;
    u32 numBuffers;
    u32 numModules;
    u32 reserved;
    OrbisNgs2CustomRackModuleInfo aModule[ORBIS_NGS2_CUSTOM_MAX_MODULES];
    OrbisNgs2CustomRackPortInfo aPort[ORBIS_NGS2_CUSTOM_MAX_PORTS];
} OrbisNgs2CustomRackOption;

typedef struct OrbisNgs2CustomSamplerRackOption {
    OrbisNgs2CustomRackOption customRackOption;

    u32 maxChannelWorks;
    u32 maxWaveformBlocks;
    u32 maxAtrac9Decoders;
    u32 maxAtrac9ChannelWorks;
    u32 maxAjmAtrac9Decoders;
    u32 maxCodecCaches;
} OrbisNgs2CustomSamplerRackOption;

typedef struct OrbisNgs2CustomSubmixerRackOption {
    OrbisNgs2CustomRackOption customRackOption;

    u32 maxChannels;
    u32 maxInputs;
} OrbisNgs2CustomSubmixerRackOption;

typedef struct OrbisNgs2CustomMasteringRackOption {
    OrbisNgs2CustomRackOption customRackOption;

    u32 maxChannels;
    u32 maxInputs;
} OrbisNgs2CustomMasteringRackOption;

typedef struct OrbisNgs2CustomSamplerVoiceSetupParam {
    OrbisNgs2VoiceParamHeader header;
    OrbisNgs2WaveformFormat format;
    u32 flags;
    u32 reserved;
} OrbisNgs2CustomSamplerVoiceSetupParam;

typedef struct OrbisNgs2CustomSamplerVoiceWaveformBlocksParam {
    OrbisNgs2VoiceParamHeader header;
    const void* data;
    u32 flags;
    u32 numBlocks;
    const OrbisNgs2WaveformBlock* aBlock;
} OrbisNgs2CustomSamplerVoiceWaveformBlocksParam;

typedef struct OrbisNgs2CustomSamplerVoiceWaveformAddressParam {
    OrbisNgs2VoiceParamHeader header;
    const void* from;
    const void* to;
} OrbisNgs2CustomSamplerVoiceWaveformAddressParam;

typedef struct OrbisNgs2CustomSamplerVoiceWaveformFrameOffsetParam {
    OrbisNgs2VoiceParamHeader header;
    u32 frameOffset;
    u32 reserved;
} OrbisNgs2CustomSamplerVoiceWaveformFrameOffsetParam;

typedef struct OrbisNgs2CustomSamplerVoiceExitLoopParam {
    OrbisNgs2VoiceParamHeader header;
} OrbisNgs2CustomSamplerVoiceExitLoopParam;

typedef struct OrbisNgs2CustomSamplerVoicePitchParam {
    OrbisNgs2VoiceParamHeader header;
    float ratio;
    u32 reserved;
} OrbisNgs2CustomSamplerVoicePitchParam;

typedef struct OrbisNgs2CustomSamplerVoiceState {
    OrbisNgs2VoiceState voiceState;
    char padding[32];
    const void* waveformData;
    u64 numDecodedSamples;
    u64 decodedDataSize;
    u64 userData;
    u32 reserved;
    u32 reserved2;
} OrbisNgs2CustomSamplerVoiceState;

typedef struct OrbisNgs2CustomSubmixerVoiceSetupParam {
    OrbisNgs2VoiceParamHeader header;
    u32 numInputChannels;
    u32 numOutputChannels;
    u32 flags;
    u32 reserved;
} OrbisNgs2CustomSubmixerVoiceSetupParam;

typedef struct OrbisNgs2CustomSubmixerVoiceState {
    OrbisNgs2VoiceState voiceState; /* Voice state */
    u32 reserved;
    u32 reserved2;
} OrbisNgs2CustomSubmixerVoiceState;

typedef struct OrbisNgs2CustomMasteringVoiceSetupParam {
    OrbisNgs2VoiceParamHeader header;
    u32 numInputChannels;
    u32 flags;
} OrbisNgs2CustomMasteringVoiceSetupParam;

typedef struct OrbisNgs2CustomMasteringVoiceOutputParam {
    OrbisNgs2VoiceParamHeader header;
    u32 outputId;
    u32 reserved;
} OrbisNgs2CustomMasteringVoiceOutputParam;

typedef struct OrbisNgs2CustomMasteringVoiceState {
    OrbisNgs2VoiceState voiceState;
    u32 reserved;
    u32 reserved2;
} OrbisNgs2CustomMasteringVoiceState;

typedef struct OrbisNgs2CustomVoiceEnvelopeParam {
    OrbisNgs2VoiceParamHeader header;
    u32 numForwardPoints;
    u32 numReleasePoints;
    const OrbisNgs2EnvelopePoint* aPoint;
} OrbisNgs2CustomVoiceEnvelopeParam;

typedef struct OrbisNgs2CustomVoiceDistortionParam {
    OrbisNgs2VoiceParamHeader header;
    u32 flags;
    float a;
    float b;
    float clip;
    float gate;
    float wetLevel;
    float dryLevel;
    u32 reserved;
} OrbisNgs2CustomVoiceDistortionParam;

typedef struct OrbisNgs2CustomVoiceCompressorParam {
    OrbisNgs2VoiceParamHeader header;
    u32 flags;
    float threshold;
    float ratio;
    float knee;
    float attackTime;
    float releaseTime;
    float level;
    u32 reserved;
} OrbisNgs2CustomVoiceCompressorParam;

typedef struct OrbisNgs2CustomVoiceFilterParam {
    OrbisNgs2VoiceParamHeader header;
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
} OrbisNgs2CustomVoiceFilterParam;

typedef struct OrbisNgs2CustomVoiceLfeFilterParam {
    OrbisNgs2VoiceParamHeader header;
    u32 enableFlag;
    u32 fc;
} OrbisNgs2CustomVoiceLfeFilterParam;

typedef struct OrbisNgs2CustomVoiceGainParam {
    OrbisNgs2VoiceParamHeader header;
    float aLevel[ORBIS_NGS2_MAX_VOICE_CHANNELS];
} OrbisNgs2CustomVoiceGainParam;

typedef struct OrbisNgs2CustomVoiceMixerParam {
    OrbisNgs2VoiceParamHeader header;
    float aSourceLevel[ORBIS_NGS2_MAX_VOICE_CHANNELS];
    float aDestLevel[ORBIS_NGS2_MAX_VOICE_CHANNELS];
} OrbisNgs2CustomVoiceMixerParam;

typedef struct OrbisNgs2CustomVoiceChannelMixerParam {
    OrbisNgs2VoiceParamHeader header;
    float aLevel[ORBIS_NGS2_MAX_VOICE_CHANNELS][ORBIS_NGS2_MAX_VOICE_CHANNELS];
} OrbisNgs2CustomVoiceChannelMixerParam;

typedef struct OrbisNgs2CustomVoiceUserFxParam {
    OrbisNgs2VoiceParamHeader header;
    OrbisNgs2UserFxProcessHandler handler;

    uintptr_t userData0;
    uintptr_t userData1;
    uintptr_t userData2;
} OrbisNgs2CustomVoiceUserFxParam;

typedef struct OrbisNgs2CustomVoiceUserFx2Param {
    OrbisNgs2VoiceParamHeader header;
    const void* data;
    size_t dataSize;
} OrbisNgs2CustomVoiceUserFx2Param;

typedef struct OrbisNgs2CustomVoiceOutputParam {
    OrbisNgs2VoiceParamHeader header;
    u32 outputId;
    u32 reserved;
} OrbisNgs2CustomVoiceOutputParam;

typedef struct OrbisNgs2CustomVoicePeakMeterParam {
    OrbisNgs2VoiceParamHeader header;
    u32 enableFlag;
    u32 reserved;
} OrbisNgs2CustomVoicePeakMeterParam;

typedef struct OrbisNgs2CustomVoiceReverbParam {
    OrbisNgs2VoiceParamHeader header;
    OrbisNgs2ReverbI3DL2Param i3dl2;
} OrbisNgs2CustomVoiceReverbParam;

typedef struct OrbisNgs2CustomVoiceChorusParam {
    OrbisNgs2VoiceParamHeader header;
    u32 flags;
    u32 numPhases;
    u32 channelMask;
    float inputLevel;
    float delayTime;
    float modulationRatio;
    float modulationDepth;
    float feedbackLevel;
    float wetLevel;
    float dryLevel;
} OrbisNgs2CustomVoiceChorusParam;

typedef struct OrbisNgs2DelayTapInfo {
    float tapLevel;
    float delayTime;
} OrbisNgs2DelayTapInfo;

typedef struct OrbisNgs2CustomVoiceDelayParam {
    OrbisNgs2VoiceParamHeader header;
    float dryLevel;
    float wetLevel;
    float inputLevel;
    float feedbackLevel;
    float lowpassFc;
    u32 numTaps;
    OrbisNgs2DelayTapInfo aTap[ORBIS_NGS2_CUSTOM_DELAY_MAX_TAPS];
    float aInputMixLevel[ORBIS_NGS2_MAX_VOICE_CHANNELS];
    u32 channelMask;
    u32 flags;
} OrbisNgs2CustomVoiceDelayParam;

typedef struct OrbisNgs2CustomVoiceNoiseGateParam {
    OrbisNgs2VoiceParamHeader header;
    u32 flags;
    float threshold;
    float attackTime;
    float releaseTime;
} OrbisNgs2CustomVoiceNoiseGateParam;

typedef struct OrbisNgs2CustomVoicePitchShiftParam {
    OrbisNgs2VoiceParamHeader header;
    s32 cent;
} OrbisNgs2CustomVoicePitchShiftParam;

typedef struct OrbisNgs2CustomEnvelopeModuleState {
    float height;
    u32 reserved;
} OrbisNgs2CustomEnvelopeModuleState;

typedef struct OrbisNgs2CustomCompressorModuleState {
    float peakHeight;
    float compressorHeight;
} OrbisNgs2CustomCompressorModuleState;

typedef struct OrbisNgs2CustomPeakMeterModuleState {
    float peak;
    float aChannelPeak[ORBIS_NGS2_MAX_VOICE_CHANNELS];
    u32 reserved;
} OrbisNgs2CustomPeakMeterModuleState;

typedef struct OrbisNgs2CustomNoiseGateModuleState {
    float gateHeight;
} OrbisNgs2CustomNoiseGateModuleState;

typedef struct OrbisNgs2CustomRackInfo {
    OrbisNgs2RackInfo rackInfo;
    u32 stateSize;
    u32 numBuffers;
    u32 numModules;
    u32 reserved;
    OrbisNgs2CustomRackModuleInfo aModule[ORBIS_NGS2_CUSTOM_MAX_MODULES];
    OrbisNgs2CustomRackPortInfo aPort[ORBIS_NGS2_CUSTOM_MAX_PORTS];
} OrbisNgs2CustomRackInfo;

typedef struct OrbisNgs2CustomSamplerRackInfo {
    OrbisNgs2CustomRackInfo customRackInfo;

    u32 maxChannelWorks;
    u32 maxWaveformBlocks;
    u32 maxAtrac9Decoders;
    u32 maxAtrac9ChannelWorks;
    u32 maxAjmAtrac9Decoders;
    u32 maxCodecCaches;
} OrbisNgs2CustomSamplerRackInfo;

typedef struct OrbisNgs2CustomSubmixerRackInfo {
    OrbisNgs2CustomRackInfo customRackInfo;

    u32 maxChannels;
    u32 maxInputs;
} OrbisNgs2CustomSubmixerRackInfo;

typedef struct OrbisNgs2CustomMasteringRackInfo {
    OrbisNgs2CustomRackInfo customRackInfo;

    u32 maxChannels;
    u32 maxInputs;
} OrbisNgs2CustomMasteringRackInfo;

typedef struct OrbisNgs2CustomModuleInfo {
    u32 moduleId;
    u32 sourceBufferId;
    u32 extraBufferId;
    u32 destBufferId;
    u32 stateOffset;
    u32 stateSize;
    u32 reserved;
    u32 reserved2;
} OrbisNgs2CustomModuleInfo;

typedef struct OrbisNgs2CustomEnvelopeModuleInfo {
    OrbisNgs2CustomModuleInfo moduleInfo;

    u32 maxPoints;
    u32 reserved;
} OrbisNgs2CustomEnvelopeModuleInfo;

typedef struct OrbisNgs2CustomReverbModuleInfo {
    OrbisNgs2CustomModuleInfo moduleInfo;

    u32 reverbSize;
    u32 reserved;
} OrbisNgs2CustomReverbModuleInfo;

RETRO_END_DECLS

#endif
