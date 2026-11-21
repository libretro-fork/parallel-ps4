/* SPDX-FileCopyrightText: Copyright 2025 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef SHADPS4_LIBRARIES_COMPANION_UTIL_H
#define SHADPS4_LIBRARIES_COMPANION_UTIL_H

/* libSceCompanionUtil. Registered in libs.cpp. */

#include <retro_common_api.h>

#include "common/types.h"

RETRO_BEGIN_DECLS

#define ORBIS_COMPANION_UTIL_OK 0

typedef struct sceCompanionUtilEvent {
    u8 blob[0x104]; /* 0x104 bytes of data, dont know what it is exactly */
} sceCompanionUtilEvent;

typedef struct sceCompanionUtilContext {
    u8 blob[0x27B]; /* 0x27B bytes of data, dont know what it is exactly */
} sceCompanionUtilContext;

s32 PS4_SYSV_ABI sceCompanionUtilGetEvent(sceCompanionUtilEvent* outEvent);
s32 PS4_SYSV_ABI sceCompanionUtilGetRemoteOskEvent(void);
s32 PS4_SYSV_ABI sceCompanionUtilInitialize(void);
s32 PS4_SYSV_ABI sceCompanionUtilOptParamInitialize(void);
s32 PS4_SYSV_ABI sceCompanionUtilTerminate(void);

RETRO_END_DECLS

#endif
