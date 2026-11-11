// SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <string_view>

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
    case C_LOG_LOADER:
        return Common::Log::Class::Loader;
    case C_LOG_NP_HANDLER:
        return Common::Log::Class::NpHandler;
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
    std::vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    Common::Log::Log(cls, lvl, "c", 0, func, "{}", std::string_view(buf));
}

extern "C" size_t c_get_user_path(c_user_path type, char* s, size_t len) {
    const auto& path = Common::FS::GetUserPath(static_cast<Common::FS::PathType>(type));
    const auto u8 = path.u8string();
    if (len) {
        const size_t n = u8.size() < len ? u8.size() : len - 1;
        std::memcpy(s, u8.data(), n);
        s[n] = '\0';
    }
    return u8.size();
}

extern "C" const char* c_get_scm_rev(void) {
    return Common::g_scm_rev;
}
