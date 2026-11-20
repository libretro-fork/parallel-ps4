// SPDX-License-Identifier: MIT
/*
$info$
tags: glue|log-manager
$end_info$
*/

#include <FEXCore/Utils/CompilerDefs.h>
#include <FEXCore/Utils/LogManager.h>
#include <FEXCore/fextl/fmt.h>

namespace LogMan {

namespace Throw {
  ThrowHandler Handler {};
  void InstallHandler(ThrowHandler _Handler) {
    Handler = _Handler;
  }
  void UnInstallHandler() {
    Handler = nullptr;
  }

  [[noreturn]]
  static void Throw(const char* fmt, va_list args) {
    if (Handler) {
      const auto msg = fextl::fmt::vformat(fmt, args);
      Handler(msg.c_str());
    }

    FEX_TRAP_EXECUTION;
  }

  void MFmt(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    Throw(fmt, args);
  }

#if defined(ASSERTIONS_ENABLED) && ASSERTIONS_ENABLED
  void VMFmt(const char* fmt, va_list args) {
    Throw(fmt, args);
  }
#endif
} // namespace Throw

namespace Msg {
  MsgHandler Handler {};
  void InstallHandler(MsgHandler _Handler) {
    Handler = _Handler;
  }
  void UnInstallHandler() {
    Handler = nullptr;
  }

  void MFmtImpl(DebugLevels level, const char* fmt, va_list args) {
    if (Handler) {
      const auto msg = fextl::fmt::vformat(fmt, args);
      Handler(level, msg.c_str());
    }
  }

} // namespace Msg
} // namespace LogMan
