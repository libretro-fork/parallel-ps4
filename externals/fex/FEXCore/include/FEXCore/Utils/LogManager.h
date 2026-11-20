// SPDX-License-Identifier: MIT
#pragma once

#include <FEXCore/Utils/CompilerDefs.h>

#include <cinttypes>
#include <cstdarg>
#include <cstdint>

// printf formats, checked by the compiler
#if defined(__GNUC__) || defined(__clang__)
#define FEX_PRINTF(fmt, first) __attribute__((format(printf, fmt, first)))
#else
#define FEX_PRINTF(fmt, first)
#endif

namespace LogMan {
enum DebugLevels : uint32_t {
  NONE = 0,   ///< Expect zero messages
  ASSERT = 1, ///< Assert throwing
  ERROR = 2,  ///< Only Errors printed
  DEBUG = 3,  ///< Debug messages added
  INFO = 4,   ///< Info messages added
};

static inline const char* DebugLevelStr(uint32_t Level) {
  switch (Level) {
  case NONE: return "NONE";
  case ASSERT: return "A";
  case ERROR: return "E";
  case DEBUG: return "D";
  case INFO: return "I";
  default: return "???"; break;
  }
}

constexpr DebugLevels MSG_LEVEL = INFO;

// All logging functions with the Fmt or _FMT suffix on them take printf formats.

namespace Throw {
  using ThrowHandler = void (*)(const char* Message);
  FEX_DEFAULT_VISIBILITY void InstallHandler(ThrowHandler Handler);
  FEX_DEFAULT_VISIBILITY void UnInstallHandler();

  [[noreturn]]
  void MFmt(const char* fmt, ...) FEX_PRINTF(1, 2);

#if defined(ASSERTIONS_ENABLED) && ASSERTIONS_ENABLED
  [[noreturn]]
  void VMFmt(const char* fmt, va_list args);

  static inline void AFmt(bool Value, const char* fmt, ...) FEX_PRINTF(2, 3);
  static inline void AFmt(bool Value, const char* fmt, ...) {
    if (MSG_LEVEL < ASSERT || Value) {
      return;
    }
    va_list args;
    va_start(args, fmt);
    VMFmt(fmt, args);
  }

#define LOGMAN_THROW_A_FMT(pred, format, ...)                                                                           \
  do {                                                                                                                  \
    if (!(pred)) {                                                                                                      \
      LogMan::Throw::MFmt("%s:%d, %s: " format, __FILE_NAME__, __LINE__, __FUNCTION__ __VA_OPT__(, ) __VA_ARGS__); \
    }                                                                                                                   \
  } while (0)
#else
  static inline void AFmt(bool, const char*, ...) {}
#define LOGMAN_THROW_A_FMT(pred, ...) \
  do {                                \
    (void)(pred);                     \
  } while (0)
#endif

} // namespace Throw

namespace Msg {
  using MsgHandler = void (*)(DebugLevels Level, const char* Message);
  FEX_DEFAULT_VISIBILITY void InstallHandler(MsgHandler Handler);
  FEX_DEFAULT_VISIBILITY void UnInstallHandler();

  FEX_DEFAULT_VISIBILITY void MFmtImpl(DebugLevels level, const char* fmt, va_list args);

  static inline void MFmt(DebugLevels level, const char* fmt, ...) FEX_PRINTF(2, 3);
  static inline void MFmt(DebugLevels level, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    MFmtImpl(level, fmt, args);
    va_end(args);
  }

#define FEX_LOGMAN_LEVEL_FMT(Name, Level)                                    \
  static inline void Name(const char* fmt, ...) FEX_PRINTF(1, 2);           \
  static inline void Name(const char* fmt, ...) {                           \
    if (MSG_LEVEL < Level) {                                                \
      return;                                                               \
    }                                                                       \
    va_list args;                                                           \
    va_start(args, fmt);                                                    \
    MFmtImpl(Level, fmt, args);                                             \
    va_end(args);                                                           \
  }
  FEX_LOGMAN_LEVEL_FMT(EFmt, ERROR)
  FEX_LOGMAN_LEVEL_FMT(DFmt, DEBUG)
  FEX_LOGMAN_LEVEL_FMT(IFmt, INFO)

#if defined(ASSERTIONS_ENABLED) && ASSERTIONS_ENABLED
  static inline void AFmt(const char* fmt, ...) FEX_PRINTF(1, 2);
  static inline void AFmt(const char* fmt, ...) {
    if (MSG_LEVEL < ASSERT) {
      return;
    }
    va_list args;
    va_start(args, fmt);
    MFmtImpl(ASSERT, fmt, args);
    va_end(args);
    FEX_TRAP_EXECUTION;
  }
#define LOGMAN_MSG_A_FMT(...)       \
  do {                              \
    LogMan::Msg::AFmt(__VA_ARGS__); \
  } while (0)
#else
  static inline void AFmt(const char*, ...) {}
#define LOGMAN_MSG_A_FMT(...) \
  do {                        \
  } while (0)
#endif
#undef FEX_LOGMAN_LEVEL_FMT

#define WARN_ONCE_FMT(...)            \
  do {                                \
    static bool Warned {};            \
    if (!Warned) {                    \
      LogMan::Msg::DFmt(__VA_ARGS__); \
      Warned = true;                  \
    }                                 \
  } while (0);

#define ERROR_AND_DIE_FMT(...)                      \
  do {                                              \
    LogMan::Msg::MFmt(LogMan::ASSERT, __VA_ARGS__); \
    FEX_TRAP_EXECUTION;                             \
  } while (0)

} // namespace Msg
} // namespace LogMan
