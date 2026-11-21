/* SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef SHADPS4_LIBRARIES_RANDOM_H
#define SHADPS4_LIBRARIES_RANDOM_H

/* libSceRandom. Registered in libs.cpp. */

#include <stddef.h>

#include <retro_common_api.h>

#include "common/types.h"

RETRO_BEGIN_DECLS

#define SCE_RANDOM_MAX_SIZE 64

s32 PS4_SYSV_ABI sceRandomGetRandomNumber(u8* buf, size_t size);

RETRO_END_DECLS

#endif
