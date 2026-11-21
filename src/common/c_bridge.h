/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef SHADPS4_COMMON_C_BRIDGE_H
#define SHADPS4_COMMON_C_BRIDGE_H

/* What the core's C89 modules need from the C++ side: logging, the user
 * directories and the build revision. Implemented in c_bridge.cpp. */

#include <stddef.h>

#include <retro_common_api.h>

RETRO_BEGIN_DECLS

/* Log classes used from C. Mapped to Common::Log::Class in c_bridge.cpp. */
enum c_log_class {
    C_LOG_CONFIG = 0,
    C_LOG_KEY_MANAGER,
    C_LOG_LIB_AUDIODEC,
    C_LOG_LIB_HTTP,
    C_LOG_LIB_ULT,
    C_LOG_LIB_ZLIB,
    C_LOG_LIB_PLAYGO,
    C_LOG_LIB_PLAYGO_DIALOG,
    C_LOG_LIB_COMMON_DLG,
    C_LOG_LIB_WEB_BROWSER_DIALOG,
    C_LOG_LIB_NP_PROFILE_DIALOG,
    C_LOG_LIB_INVITATION_DIALOG,
    C_LOG_LIB_VOICE,
    C_LOG_LIB_RTC,
    C_LOG_LIB_CAMERA,
    C_LOG_LIB_VR_TRACKER,
    C_LOG_LIB_JPEG,
    C_LOG_LIB_SYSTEM_GESTURE,
    C_LOG_LIB_COMPANION_HTTPD,
    C_LOG_LIB_COMPANION_UTIL,
    C_LOG_LIB_GAME_LIVE_STREAMING,
    C_LOG_LIB_PNG,
    C_LOG_LIB_RAZOR_CPU,
    C_LOG_LIB_REMOTEPLAY,
    C_LOG_LIB_RUDP,
    C_LOG_LIB_CONTENT_EXPORT,
    C_LOG_LIB_KEYBOARD,
    C_LOG_LIB_MOUSE,
    C_LOG_LIB_MOVE,
    C_LOG_LIB_RANDOM,
    C_LOG_LIB_SCREENSHOT,
    C_LOG_LIB_SHARE_PLAY,
    C_LOG_LIB_SIGNIN_DIALOG,
    C_LOG_LIB_VIDEO_RECORDING,
    C_LOG_LOADER,
    C_LOG_NP_HANDLER,
    C_LOG_RENDER_VULKAN
};

/* Same order as Common::Log::Level. */
enum c_log_level {
    C_LOG_LEVEL_TRACE = 0,
    C_LOG_LEVEL_DEBUG,
    C_LOG_LEVEL_INFO,
    C_LOG_LEVEL_WARNING,
    C_LOG_LEVEL_ERROR,
    C_LOG_LEVEL_CRITICAL
};

/* printf-style logging. func names the calling function. */
void c_log(enum c_log_class log_class, enum c_log_level level, const char* func, const char* fmt,
           ...);

/* Logs msg as unreachable code was reached, and stops the emulator, as UNREACHABLE_MSG
 * does. Does not return. */
void c_unreachable(const char* file, int line, const char* func, const char* msg);

/* Logs that the assertion @msg failed, and stops the emulator, as ASSERT_MSG does. */
void c_assert_fail(const char* file, int line, const char* func, const char* msg);

/* ASSERT for C: @func names the calling function (C89 has no __func__) */
#define C_ASSERT(cond, func)                                                                       \
    do {                                                                                           \
        if (!(cond)) {                                                                             \
            c_assert_fail(__FILE__, __LINE__, func, #cond);                                        \
        }                                                                                          \
    } while (0)

/* ASSERT_MSG for C: @msg logged when @cond does not hold */
#define C_ASSERT_MSG(cond, func, msg)                                                              \
    do {                                                                                           \
        if (!(cond)) {                                                                             \
            c_assert_fail(__FILE__, __LINE__, func, msg);                                          \
        }                                                                                          \
    } while (0)

/* Same order as Common::FS::PathType. */
enum c_user_path {
    C_USER_PATH_USER = 0,
    C_USER_PATH_LOG,
    C_USER_PATH_SHADER,
    C_USER_PATH_TEMP_DATA,
    C_USER_PATH_GAME_DATA,
    C_USER_PATH_SYS_MODULE,
    C_USER_PATH_DOWNLOAD,
    C_USER_PATH_CHEATS,
    C_USER_PATH_PATCHES,
    C_USER_PATH_METADATA,
    C_USER_PATH_CUSTOM_TROPHY,
    C_USER_PATH_CUSTOM_CONFIGS,
    C_USER_PATH_CACHE,
    C_USER_PATH_FONTS,
    C_USER_PATH_TROPHY,
    C_USER_PATH_HOME,
    C_USER_PATH_CUSTOM_MODULES,
    C_USER_PATH_LICENSES
};

/* Writes the UTF-8 directory into s. Returns its length; a result of len
 * or more means it didn't fit. */
size_t c_get_user_path(enum c_user_path type, char* s, size_t len);

/* The git revision this build was made from. */
const char* c_get_scm_rev(void);

RETRO_END_DECLS

#endif
