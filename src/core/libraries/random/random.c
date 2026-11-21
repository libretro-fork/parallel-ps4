/* SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <stdlib.h>

#include "common/c_bridge.h"
#include "core/libraries/error_codes.h"
#include "core/libraries/random/random.h"
#include "core/libraries/random/random_error.h"

s32 PS4_SYSV_ABI sceRandomGetRandomNumber(u8* buf, size_t size) {
    size_t i;
    c_log(C_LOG_LIB_RANDOM, C_LOG_LEVEL_TRACE, "sceRandomGetRandomNumber", "called");
    if (size > SCE_RANDOM_MAX_SIZE) {
        return SCE_RANDOM_ERROR_INVALID;
    }

    for (i = 0; i < size; ++i) {
        buf[i] = (u8)(rand() & 0xFF);
    }
    return ORBIS_OK;
}
