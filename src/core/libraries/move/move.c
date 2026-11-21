/* SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <boolean.h>
#include <stddef.h>

#include "common/c_bridge.h"
#include "core/libraries/error_codes.h"
#include "core/libraries/move/move.h"
#include "core/libraries/move/move_error.h"

static bool g_library_initialized = false;

s32 PS4_SYSV_ABI sceMoveInit(void) {
    if (g_library_initialized) {
        return ORBIS_MOVE_ERROR_ALREADY_INIT;
    }
    c_log(C_LOG_LIB_MOVE, C_LOG_LEVEL_WARNING, "sceMoveInit",
          "Move controllers are not supported yet");
    g_library_initialized = true;
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceMoveOpen(s32 user_id, s32 type, s32 index) {
    /* Even when no controllers are connected, this returns a proper handle. Internal
     * libSceVrTracker logic requires this handle to be different from other devices. */
    static s32 handle = 0x30b0000;
    c_log(C_LOG_LIB_MOVE, C_LOG_LEVEL_DEBUG, "sceMoveOpen", "called");
    if (!g_library_initialized) {
        return ORBIS_MOVE_ERROR_NOT_INIT;
    }
    return handle += 0x100;
}

s32 PS4_SYSV_ABI sceMoveGetDeviceInfo(s32 handle, OrbisMoveDeviceInfo* info) {
    c_log(C_LOG_LIB_MOVE, C_LOG_LEVEL_TRACE, "sceMoveGetDeviceInfo", "called");
    if (!g_library_initialized) {
        return ORBIS_MOVE_ERROR_NOT_INIT;
    }
    if (info == NULL) {
        return ORBIS_MOVE_ERROR_INVALID_ARG;
    }
    return ORBIS_MOVE_ERROR_NO_CONTROLLER_CONNECTED;
}

s32 PS4_SYSV_ABI sceMoveReadStateLatest(s32 handle, OrbisMoveData* data) {
    c_log(C_LOG_LIB_MOVE, C_LOG_LEVEL_TRACE, "sceMoveReadStateLatest", "called");
    if (!g_library_initialized) {
        return ORBIS_MOVE_ERROR_NOT_INIT;
    }
    if (data == NULL) {
        return ORBIS_MOVE_ERROR_INVALID_ARG;
    }
    return ORBIS_MOVE_ERROR_NO_CONTROLLER_CONNECTED;
}

s32 PS4_SYSV_ABI sceMoveReadStateRecent(s32 handle, s64 timestamp, OrbisMoveData* data,
                                        s32* out_count) {
    c_log(C_LOG_LIB_MOVE, C_LOG_LEVEL_TRACE, "sceMoveReadStateRecent", "called");
    if (!g_library_initialized) {
        return ORBIS_MOVE_ERROR_NOT_INIT;
    }
    if (timestamp < 0 || data == NULL || out_count == NULL) {
        return ORBIS_MOVE_ERROR_INVALID_ARG;
    }
    return ORBIS_MOVE_ERROR_NO_CONTROLLER_CONNECTED;
}

s32 PS4_SYSV_ABI sceMoveGetExtensionPortInfo(s32 handle, void* data) {
    c_log(C_LOG_LIB_MOVE, C_LOG_LEVEL_TRACE, "sceMoveGetExtensionPortInfo", "called");
    if (!g_library_initialized) {
        return ORBIS_MOVE_ERROR_NOT_INIT;
    }
    if (data == NULL) {
        return ORBIS_MOVE_ERROR_INVALID_ARG;
    }
    return ORBIS_MOVE_ERROR_NO_CONTROLLER_CONNECTED;
}

s32 PS4_SYSV_ABI sceMoveSetVibration(s32 handle, u8 intensity) {
    c_log(C_LOG_LIB_MOVE, C_LOG_LEVEL_TRACE, "sceMoveSetVibration", "called");
    if (!g_library_initialized) {
        return ORBIS_MOVE_ERROR_NOT_INIT;
    }
    return ORBIS_MOVE_ERROR_NO_CONTROLLER_CONNECTED;
}

s32 PS4_SYSV_ABI sceMoveSetLightSphere(s32 handle, u8 red, u8 green, u8 blue) {
    c_log(C_LOG_LIB_MOVE, C_LOG_LEVEL_TRACE, "sceMoveSetLightSphere", "called");
    if (!g_library_initialized) {
        return ORBIS_MOVE_ERROR_NOT_INIT;
    }
    return ORBIS_MOVE_ERROR_NO_CONTROLLER_CONNECTED;
}

s32 PS4_SYSV_ABI sceMoveResetLightSphere(s32 handle) {
    c_log(C_LOG_LIB_MOVE, C_LOG_LEVEL_TRACE, "sceMoveResetLightSphere", "called");
    if (!g_library_initialized) {
        return ORBIS_MOVE_ERROR_NOT_INIT;
    }
    /* Returns success, even if no controllers are connected. */
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceMoveClose(s32 handle) {
    c_log(C_LOG_LIB_MOVE, C_LOG_LEVEL_DEBUG, "sceMoveClose", "called");
    if (!g_library_initialized) {
        return ORBIS_MOVE_ERROR_NOT_INIT;
    }
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceMoveTerm(void) {
    c_log(C_LOG_LIB_MOVE, C_LOG_LEVEL_DEBUG, "sceMoveTerm", "called");
    if (!g_library_initialized) {
        return ORBIS_MOVE_ERROR_NOT_INIT;
    }
    g_library_initialized = false;
    return ORBIS_OK;
}
