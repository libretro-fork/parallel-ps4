/* SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef SHADPS4_NGS2_GEOM_H
#define SHADPS4_NGS2_GEOM_H

#include "core/libraries/ngs2/ngs2.h"

#include <retro_common_api.h>

RETRO_BEGIN_DECLS

typedef struct OrbisNgs2GeomVector {
    float x;
    float y;
    float z;
} OrbisNgs2GeomVector;

typedef struct OrbisNgs2GeomCone {
    float innerLevel;
    float innerAngle;
    float outerLevel;
    float outerAngle;
} OrbisNgs2GeomCone;

typedef struct OrbisNgs2GeomRolloff {
    u32 model;
    float maxDistance;
    float rolloffFactor;
    float referenceDistance;
} OrbisNgs2GeomRolloff;

typedef struct OrbisNgs2GeomListenerParam {
    OrbisNgs2GeomVector position;
    OrbisNgs2GeomVector orientFront;
    OrbisNgs2GeomVector orientUp;
    OrbisNgs2GeomVector velocity;
    float soundSpeed;
    u32 reserved[2];
} OrbisNgs2GeomListenerParam;

typedef struct OrbisNgs2GeomListenerWork {
    float matrix[4][4];
    OrbisNgs2GeomVector velocity;
    float soundSpeed;
    u32 coordinate;
    u32 reserved[3];
} OrbisNgs2GeomListenerWork;

typedef struct OrbisNgs2GeomSourceParam {
    OrbisNgs2GeomVector position;
    OrbisNgs2GeomVector velocity;
    OrbisNgs2GeomVector direction;
    OrbisNgs2GeomCone cone;
    OrbisNgs2GeomRolloff rolloff;
    float dopplerFactor;
    float fbwLevel;
    float lfeLevel;
    float maxLevel;
    float minLevel;
    float radius;
    u32 numSpeakers;
    u32 matrixFormat;
    u32 reserved[2];
} OrbisNgs2GeomSourceParam;

typedef struct OrbisNgs2GeomA3dAttribute {
    OrbisNgs2GeomVector position;
    float volume;
    u32 reserved[4];
} OrbisNgs2GeomA3dAttribute;

typedef struct OrbisNgs2GeomAttribute {
    float pitchRatio;
    float aLevel[ORBIS_NGS2_MAX_VOICE_CHANNELS * ORBIS_NGS2_MAX_VOICE_CHANNELS];

    OrbisNgs2GeomA3dAttribute a3dAttrib;
    u32 reserved[4];
} OrbisNgs2GeomAttribute;

RETRO_END_DECLS

#endif
