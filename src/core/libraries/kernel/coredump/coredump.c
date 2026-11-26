/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <stddef.h>

#include "common/c_bridge.h"
#include "core/libraries/error_codes.h"
#include "core/libraries/kernel/coredump/coredump.h"
#include "core/libraries/kernel/coredump/coredump_error.h"

/* The game's handler and what it gives it. The real library starts a thread of its own for
 * them; here they are only kept. Set and read by the game's threads as the game calls. */
static OrbisCoredumpHandler g_coredump_handler = NULL;
static void* g_coredump_common = NULL;

s32 PS4_SYSV_ABI sceCoredumpRegisterCoredumpHandler(OrbisCoredumpHandler handler, u64 stack_size,
                                                    void* common) {
    c_log(C_LOG_LIB_KERNEL, C_LOG_LEVEL_WARNING, "sceCoredumpRegisterCoredumpHandler",
          "(STUBBED) called, handler = %p, stack_size = 0x%llx, common = %p", (void*)handler,
          (unsigned long long)stack_size, common);
    if (g_coredump_handler) {
        c_log(C_LOG_LIB_KERNEL, C_LOG_LEVEL_ERROR, "sceCoredumpRegisterCoredumpHandler",
              "Coredump handler is already registered");
        return ORBIS_COREDUMP_ERROR_ALREADY_REGISTERED;
    }
    if (!handler || stack_size >= 0x20000000) {
        c_log(C_LOG_LIB_KERNEL, C_LOG_LEVEL_ERROR, "sceCoredumpRegisterCoredumpHandler",
              "Invalid coredump handler parameters");
        return ORBIS_COREDUMP_ERROR_PARAM;
    }
    g_coredump_handler = handler;
    g_coredump_common = common;
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceCoredumpUnregisterCoredumpHandler(void) {
    c_log(C_LOG_LIB_KERNEL, C_LOG_LEVEL_INFO, "sceCoredumpUnregisterCoredumpHandler", "called");
    if (!g_coredump_handler) {
        c_log(C_LOG_LIB_KERNEL, C_LOG_LEVEL_ERROR, "sceCoredumpUnregisterCoredumpHandler",
              "No coredump handler to unregister");
        return ORBIS_COREDUMP_ERROR_NOT_REGISTERED;
    }
    g_coredump_handler = NULL;
    g_coredump_common = NULL;
    return ORBIS_OK;
}
