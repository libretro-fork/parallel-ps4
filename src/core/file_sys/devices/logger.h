// SPDX-FileCopyrightText: Copyright 2025 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "core/file_sys/devices/base_device.h"

#include <string>
#include <string_view>

namespace Core::Devices {

/// The game's stdin, stdout and stderr: what it writes goes to the log a line at a time. Each
/// thread cuts its own writes into lines in a buffer of its own (line_buffer.c), so threads
/// never wait for each other and a line is never another thread's half.
class Logger final : BaseDevice {
    std::string prefix;
    bool is_err;
    int slot; // which of each thread's line buffers is this device's

public:
    explicit Logger(std::string prefix, bool is_err);

    ~Logger() override;

    s64 write(const void* buf, u64 nbytes) override;
    s64 writev(const Libraries::Kernel::OrbisKernelIovec* iov, s32 iovcnt) override;
    s64 pwrite(const void* buf, u64 nbytes, s64 offset) override;

    s32 fsync() override;

    void Emit(std::string_view line) const;

private:
    void log(const char* buf, u64 nbytes);
    void log_flush();
};

} // namespace Core::Devices
