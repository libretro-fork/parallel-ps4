/* SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef SHADPS4_NGS2_REVERB_H
#define SHADPS4_NGS2_REVERB_H

#include "core/libraries/ngs2/ngs2.h"

#include <retro_common_api.h>

RETRO_BEGIN_DECLS

typedef struct OrbisNgs2ReverbRackOption {
    OrbisNgs2RackOption rackOption;
    u32 maxChannels;
    u32 reverbSize;
} OrbisNgs2ReverbRackOption;

typedef struct OrbisNgs2ReverbI3DL2Param {
    float wet;
    float dry;
    s32 room;
    s32 roomHF;
    u32 reflectionPattern;
    float decayTime;
    float decayHFRatio;
    s32 reflections;
    float reflectionsDelay;
    s32 reverb;
    float reverbDelay;
    float diffusion;
    float density;
    float HFReference;
    u32 reserve[8];
} OrbisNgs2ReverbI3DL2Param;

typedef struct OrbisNgs2ReverbVoiceSetupParam {
    OrbisNgs2VoiceParamHeader header;

    u32 numInputChannels;
    u32 numOutputChannels;
    u32 flags;
    u32 reserved;
} OrbisNgs2ReverbVoiceSetupParam;

typedef struct OrbisNgs2ReverbVoiceI3DL2Param {
    OrbisNgs2VoiceParamHeader header;

    OrbisNgs2ReverbI3DL2Param i3dl2;
} OrbisNgs2ReverbVoiceI3DL2Param;

typedef struct OrbisNgs2ReverbVoiceState {
    OrbisNgs2VoiceState voiceState;
} OrbisNgs2ReverbVoiceState;

typedef struct OrbisNgs2ReverbRackInfo {
    OrbisNgs2RackInfo rackInfo;
    u32 maxChannels;
    u32 reverbSize;
} OrbisNgs2ReverbRackInfo;

RETRO_END_DECLS

#endif
