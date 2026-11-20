// SPDX-FileCopyrightText: Copyright 2021 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <stdlib.h>
#include "common/arch.h"
#include "common/assert.h"
#include "core/signals.h"
#include "emulator.h"

#if defined(ARCH_X86_64)
#define Crash() __asm__ __volatile__("int $3")
#elif defined(ARCH_ARM64)
#define Crash() __asm__ __volatile__("brk 0")
#else
#error "Missing Crash() implementation for target CPU architecture."
#endif

void assert_fail_impl() {
    Core::Signals::Instance()->RemoveHandlers();
    Common::Singleton<Core::Emulator>::Instance()->Shutdown();
    Crash();
}

[[noreturn]] void unreachable_impl() {
    assert_fail_impl();
    abort();
}

void assert_fail_debug_msg(const char* msg) {
    LOG_CRITICAL(Debug, "Assertion Failed!\n%s", msg);
    assert_fail_impl();
}

namespace Common::Detail {

void AssertFail(const char* file, int line, const char* func, const char* format, ...) {
    va_list args;
    va_start(args, format);
    Common::Log::VLog(Common::Log::Class::Debug, Common::Log::Level::Critical, file, line, func,
                      format, args);
    va_end(args);
    assert_fail_impl();
}

void UnreachableFail(const char* file, int line, const char* func, const char* format, ...) {
    va_list args;
    va_start(args, format);
    Common::Log::VLog(Common::Log::Class::Debug, Common::Log::Level::Critical, file, line, func,
                      format, args);
    va_end(args);
    unreachable_impl();
}

} // namespace Common::Detail
