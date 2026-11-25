// SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "common/assert.h"
#include "common/c_bridge.h"
#include "common/logging/log.h"
#include "common/path_util.h"
#include "common/scm_rev.h"

static_assert(static_cast<int>(Common::Log::Level::Critical) == C_LOG_LEVEL_CRITICAL);
static_assert(static_cast<int>(Common::FS::PathType::LicensesDir) == C_USER_PATH_LICENSES);

static Common::Log::Class ToClass(c_log_class log_class) {
    switch (log_class) {
    case C_LOG_CONFIG:
        return Common::Log::Class::Config;
    case C_LOG_KEY_MANAGER:
        return Common::Log::Class::KeyManager;
    case C_LOG_LIB_AUDIODEC:
        return Common::Log::Class::Lib_Audiodec;
    case C_LOG_LIB_HTTP:
        return Common::Log::Class::Lib_Http;
    case C_LOG_LIB_ULT:
        return Common::Log::Class::Lib_Ult;
    case C_LOG_LIB_ZLIB:
        return Common::Log::Class::Lib_Zlib;
    case C_LOG_LIB_PLAYGO:
        return Common::Log::Class::Lib_PlayGo;
    case C_LOG_LIB_PLAYGO_DIALOG:
        return Common::Log::Class::Lib_PlayGoDialog;
    case C_LOG_LIB_COMMON_DLG:
        return Common::Log::Class::Lib_CommonDlg;
    case C_LOG_LIB_WEB_BROWSER_DIALOG:
        return Common::Log::Class::Lib_WebBrowserDialog;
    case C_LOG_LIB_NP_PROFILE_DIALOG:
        return Common::Log::Class::Lib_NpProfileDialog;
    case C_LOG_LIB_INVITATION_DIALOG:
        return Common::Log::Class::Lib_InvitationDialog;
    case C_LOG_LIB_LIBC_INTERNAL:
        return Common::Log::Class::Lib_LibcInternal;
    case C_LOG_LIB_APP_CONTENT:
        return Common::Log::Class::Lib_AppContent;
    case C_LOG_LIB_PAD:
        return Common::Log::Class::Lib_Pad;
    case C_LOG_LIB_SYS_MODULE:
        return Common::Log::Class::Lib_SysModule;
    case C_LOG_LIB_AUDIO3D:
        return Common::Log::Class::Lib_Audio3d;
    case C_LOG_LIB_AUDIO_OUT:
        return Common::Log::Class::Lib_AudioOut;
    case C_LOG_LIB_HMD:
        return Common::Log::Class::Lib_Hmd;
    case C_LOG_LIB_HMD_SETUP_DIALOG:
        return Common::Log::Class::Lib_HmdSetupDialog;
    case C_LOG_LIB_NGS2:
        return Common::Log::Class::Lib_Ngs2;
    case C_LOG_LIB_VIDEODEC:
        return Common::Log::Class::Lib_Videodec;
    case C_LOG_LIB_VDEC2:
        return Common::Log::Class::Lib_Vdec2;
    case C_LOG_LIB_VDECSW:
        return Common::Log::Class::Lib_Vdecsw;
    case C_LOG_LIB_FIBER:
        return Common::Log::Class::Lib_Fiber;
    case C_LOG_LIB_USBD:
        return Common::Log::Class::Lib_Usbd;
    case C_LOG_LIB_VIDEO_OUT:
        return Common::Log::Class::Lib_VideoOut;
    case C_LOG_LIB_AUDIO_IN:
        return Common::Log::Class::Lib_AudioIn;
    case C_LOG_LIB_VOICE:
        return Common::Log::Class::Lib_Voice;
    case C_LOG_LIB_RTC:
        return Common::Log::Class::Lib_Rtc;
    case C_LOG_LIB_CAMERA:
        return Common::Log::Class::Lib_Camera;
    case C_LOG_LIB_VR_TRACKER:
        return Common::Log::Class::Lib_VrTracker;
    case C_LOG_LIB_JPEG:
        return Common::Log::Class::Lib_Jpeg;
    case C_LOG_LIB_SYSTEM_GESTURE:
        return Common::Log::Class::Lib_SystemGesture;
    case C_LOG_LIB_COMPANION_HTTPD:
        return Common::Log::Class::Lib_CompanionHttpd;
    case C_LOG_LIB_COMPANION_UTIL:
        return Common::Log::Class::Lib_CompanionUtil;
    case C_LOG_LIB_GAME_LIVE_STREAMING:
        return Common::Log::Class::Lib_GameLiveStreaming;
    case C_LOG_LIB_PNG:
        return Common::Log::Class::Lib_Png;
    case C_LOG_LIB_RAZOR_CPU:
        return Common::Log::Class::Lib_RazorCpu;
    case C_LOG_LIB_REMOTEPLAY:
        return Common::Log::Class::Lib_Remoteplay;
    case C_LOG_LIB_RUDP:
        return Common::Log::Class::Lib_Rudp;
    case C_LOG_LIB_CONTENT_EXPORT:
        return Common::Log::Class::Lib_ContentExport;
    case C_LOG_LIB_KEYBOARD:
        return Common::Log::Class::Lib_Keyboard;
    case C_LOG_LIB_MOUSE:
        return Common::Log::Class::Lib_Mouse;
    case C_LOG_LIB_MOVE:
        return Common::Log::Class::Lib_Move;
    case C_LOG_LIB_RANDOM:
        return Common::Log::Class::Lib_Random;
    case C_LOG_LIB_SCREENSHOT:
        return Common::Log::Class::Lib_Screenshot;
    case C_LOG_LIB_SHARE_PLAY:
        return Common::Log::Class::Lib_SharePlay;
    case C_LOG_LIB_SIGNIN_DIALOG:
        return Common::Log::Class::Lib_SigninDialog;
    case C_LOG_LIB_VIDEO_RECORDING:
        return Common::Log::Class::Lib_VideoRecording;
    case C_LOG_LOADER:
        return Common::Log::Class::Loader;
    case C_LOG_NP_HANDLER:
        return Common::Log::Class::NpHandler;
    case C_LOG_RENDER_VULKAN:
        return Common::Log::Class::Render_Vulkan;
    }
    return Common::Log::Class::Common;
}

extern "C" void c_log(c_log_class log_class, c_log_level level, const char* func, const char* fmt,
                      ...) {
    const auto cls = ToClass(log_class);
    const auto lvl = static_cast<Common::Log::Level>(level);
    if (!Common::Log::ShouldLog(cls, lvl)) {
        return;
    }
    char buf[1024];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    Common::Log::Log(cls, lvl, "c", 0, func, "%s", buf);
}

extern "C" void c_assert_fail(const char* file, int line, const char* func, const char* msg) {
    Common::Detail::AssertFail(file, line, func, "Assertion Failed!\n%s", msg);
}

extern "C" void c_unreachable(const char* file, int line, const char* func, const char* msg) {
    Common::Detail::UnreachableFail(file, line, func, "Unreachable code!\n%s", msg);
}

extern "C" size_t c_get_user_path(c_user_path type, char* s, size_t len) {
    // The table's own UTF-8 copy: no string made
    const char* path = Common::FS::GetUserPathUTF8(static_cast<Common::FS::PathType>(type));
    const size_t size = strlen(path);
    if (len) {
        const size_t n = size < len ? size : len - 1;
        memcpy(s, path, n);
        s[n] = '\0';
    }
    return size;
}

extern "C" const char* c_get_scm_rev(void) {
    return Common::g_scm_rev;
}
