/* SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef SHADPS4_NGS2_MASTERING_H
#define SHADPS4_NGS2_MASTERING_H

#include "core/libraries/ngs2/ngs2.h"

#include <retro_common_api.h>

RETRO_BEGIN_DECLS

typedef struct OrbisNgs2MasteringRackOption {
    OrbisNgs2RackOption rackOption;
    u32 maxChannels;
    u32 numPeakMeterBlocks;
} OrbisNgs2MasteringRackOption;

typedef struct OrbisNgs2MasteringVoiceSetupParam {
    OrbisNgs2VoiceParamHeader header;

    u32 numInputChannels;
    u32 flags;
} OrbisNgs2MasteringVoiceSetupParam;

typedef struct OrbisNgs2MasteringVoiceMatrixParam {
    OrbisNgs2VoiceParamHeader header;

    u32 type;
    u32 numLevels;
    const float* aLevel;
} OrbisNgs2MasteringVoiceMatrixParam;

typedef struct OrbisNgs2MasteringVoiceLfeParam {
    OrbisNgs2VoiceParamHeader header;

    u32 enableFlag;
    u32 fc;
} OrbisNgs2MasteringVoiceLfeParam;

typedef struct OrbisNgs2MasteringVoiceLimiterParam {
    OrbisNgs2VoiceParamHeader header;

    u32 enableFlag;
    float threshold;
} OrbisNgs2MasteringVoiceLimiterParam;

typedef struct OrbisNgs2MasteringVoiceGainParam {
    OrbisNgs2VoiceParamHeader header;

    float fbwLevel;
    float lfeLevel;
} OrbisNgs2MasteringVoiceGainParam;

typedef struct OrbisNgs2MasteringVoiceOutputParam {
    OrbisNgs2VoiceParamHeader header;

    u32 outputId;
    u32 reserved;
} OrbisNgs2MasteringVoiceOutputParam;

typedef struct OrbisNgs2MasteringVoicePeakMeterParam {
    OrbisNgs2VoiceParamHeader header;
    u32 enableFlag;
    u32 reserved;
} OrbisNgs2MasteringVoicePeakMeterParam;

typedef struct OrbisNgs2MasteringVoiceState {
    OrbisNgs2VoiceState voiceState;
    float limiterPeakLevel;
    float limiterPressLevel;
    float aInputPeakHeight[ORBIS_NGS2_MAX_VOICE_CHANNELS];
    float aOutputPeakHeight[ORBIS_NGS2_MAX_VOICE_CHANNELS];
} OrbisNgs2MasteringVoiceState;

typedef struct OrbisNgs2MasteringRackInfo {
    OrbisNgs2RackInfo rackInfo;
    u32 maxChannels;
    u32 reserved;
} OrbisNgs2MasteringRackInfo;

RETRO_END_DECLS

#endif
