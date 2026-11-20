// SPDX-FileCopyrightText: Copyright 2025 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <retro_atomic.h>
#include "common/assert.h"
#include "common/line_buffer.h"
#include "common/logging/log.h"
#include "core/file_sys/devices/logger.h"
#include "core/libraries/kernel/file_system.h"

namespace Core::Devices {

static constexpr int MaxLoggers = 4;
static retro_atomic_int_t g_loggers{0};
// Each thread's lines, one buffer a device
static thread_local line_buffer_t t_lines[MaxLoggers];

Logger::Logger(std::string prefix, bool is_err)
    : prefix(std::move(prefix)), is_err(is_err), slot(retro_atomic_fetch_add_int(&g_loggers, 1)) {
    ASSERT_MSG(slot < MaxLoggers, "More logger devices than line buffers");
}

Logger::~Logger() = default;

s64 Logger::write(const void* buf, u64 nbytes) {
    log(static_cast<const char*>(buf), nbytes);
    return nbytes;
}

s64 Logger::writev(const Libraries::Kernel::OrbisKernelIovec* iov, s32 iovcnt) {
    u64 total_written = 0;
    for (int i = 0; i < iovcnt; i++) {
        log(static_cast<const char*>(iov[i].iov_base), iov[i].iov_len);
        total_written += iov[i].iov_len;
    }
    return total_written;
}

s64 Logger::pwrite(const void* buf, u64 nbytes, s64 offset) {
    log(static_cast<const char*>(buf), nbytes);
    return nbytes;
}

s32 Logger::fsync() {
    log_flush();
    return 0;
}

static void EmitLine(void* ctx, const char* text, size_t len) {
    const auto* const self = static_cast<const Logger*>(ctx);
    self->Emit(std::string_view{text, len});
}

void Logger::Emit(std::string_view line) const {
    if (is_err) {
        LOG_ERROR(Tty, "[%s] %.*s", (prefix).c_str(), static_cast<int>((line).size()),
                  (line).data());
    } else {
        LOG_INFO(Tty, "[%s] %.*s", (prefix).c_str(), static_cast<int>((line).size()),
                 (line).data());
    }
}

void Logger::log(const char* buf, u64 nbytes) {
    line_buffer_feed(&t_lines[slot], buf, static_cast<size_t>(nbytes), EmitLine, this);
}

void Logger::log_flush() {
    line_buffer_flush(&t_lines[slot], EmitLine, this);
}

} // namespace Core::Devices