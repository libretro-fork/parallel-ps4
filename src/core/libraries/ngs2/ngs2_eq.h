/* SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef SHADPS4_NGS2_EQ_H
#define SHADPS4_NGS2_EQ_H

#include "core/libraries/ngs2/ngs2.h"

#include <retro_common_api.h>

RETRO_BEGIN_DECLS

typedef struct OrbisNgs2EqVoiceSetupParam {
    u32 numChannels;
} OrbisNgs2EqVoiceSetupParam;

typedef struct OrbisNgs2EqVoiceFilterParam {
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
} OrbisNgs2EqVoiceFilterParam;

typedef struct OrbisNgs2EqVoiceState {
    u32 stateFlags;
} OrbisNgs2EqVoiceState;

RETRO_END_DECLS

#endif
