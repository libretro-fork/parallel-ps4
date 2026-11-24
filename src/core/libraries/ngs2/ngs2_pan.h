/* SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef SHADPS4_NGS2_PAN_H
#define SHADPS4_NGS2_PAN_H

#include "core/libraries/ngs2/ngs2.h"

#include <retro_common_api.h>

RETRO_BEGIN_DECLS

typedef struct OrbisNgs2PanParam {
    float angle;
    float distance;
    float fbwLevel;
    float lfeLevel;
} OrbisNgs2PanParam;

typedef struct OrbisNgs2PanWork {
    float aSpeakerAngle[ORBIS_NGS2_MAX_VOICE_CHANNELS];
    float unitAngle;
    u32 numSpeakers;
} OrbisNgs2PanWork;

RETRO_END_DECLS

#endif
