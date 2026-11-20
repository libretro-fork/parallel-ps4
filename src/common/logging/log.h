// SPDX-FileCopyrightText: Copyright 2025-2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <array>
#include <cinttypes>
#include <cstdarg>
#include <cstdint>
#include <string_view>

#include "common/logging/classes.h"

namespace Common::Log {

enum class Level : std::uint8_t {
    Trace = 0,
    Debug,
    Info,
    Warning,
    Error,
    Critical,
    Off,
};
extern std::array<Level, NUM_LOG_CLASSES> g_class_levels;

void Setup(std::string_view shadps4_filename);
void Switch(std::string_view game_filename, bool append_log);
void Shutdown();
void Flush();
void Terminate();
void UpdateSinks();
void UpdateLogLevels(std::string_view log_filter);
void UpdateLogFlushLevel(std::string_view log_flush_level);

[[nodiscard]] inline bool ShouldLog(Class log_class, Level level) {
    // A critical line is written whatever the filter says, and with logging off: it is what
    // a crash or a stop leaves behind
    return level == Level::Critical || level >= g_class_levels[static_cast<std::size_t>(log_class)];
}

// A printf format: the compiler checks each argument against it
#if defined(__MINGW32__)
#define SHAD_PRINTF(format_index, first_arg)                                                       \
    __attribute__((format(gnu_printf, format_index, first_arg)))
#elif defined(__GNUC__) || defined(__clang__)
#define SHAD_PRINTF(format_index, first_arg)                                                       \
    __attribute__((format(printf, format_index, first_arg)))
#else
#define SHAD_PRINTF(format_index, first_arg)
#endif

void VLog(Class log_class, Level level, const char* file, int line, const char* func,
          const char* format, va_list args);

void Log(Class log_class, Level level, const char* file, int line, const char* func,
         const char* format, ...) SHAD_PRINTF(6, 7);

} // namespace Common::Log

// The log macros: a printf format and its arguments
#define LOG_GENERIC_AT(log_class, log_level, func, ...)                                            \
    do {                                                                                           \
        if (Common::Log::ShouldLog(Common::Log::Class::log_class, log_level)) {                    \
            Common::Log::Log(Common::Log::Class::log_class, log_level, __FILE__, __LINE__, func,   \
                             __VA_ARGS__);                                                         \
        }                                                                                          \
    } while (false)

#define LOG_GENERIC(log_class, log_level, ...)                                                     \
    LOG_GENERIC_AT(log_class, log_level, __func__, __VA_ARGS__)

#ifdef NDEBUG
#define LOG_TRACE(log_class, ...) (void(0))
#else
#define LOG_TRACE(log_class, ...) LOG_GENERIC(log_class, Common::Log::Level::Trace, __VA_ARGS__)
#endif

#define LOG_DEBUG(log_class, ...) LOG_GENERIC(log_class, Common::Log::Level::Debug, __VA_ARGS__)
#define LOG_INFO(log_class, ...) LOG_GENERIC(log_class, Common::Log::Level::Info, __VA_ARGS__)
#define LOG_WARNING(log_class, ...) LOG_GENERIC(log_class, Common::Log::Level::Warning, __VA_ARGS__)
#define LOG_ERROR(log_class, ...) LOG_GENERIC(log_class, Common::Log::Level::Error, __VA_ARGS__)
#define LOG_CRITICAL(log_class, ...)                                                               \
    LOG_GENERIC(log_class, Common::Log::Level::Critical, __VA_ARGS__)
