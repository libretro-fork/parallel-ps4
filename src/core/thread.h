// SPDX-FileCopyrightText: Copyright 2024-2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "common/host_thread.h"
#include "common/types.h"

namespace Core {

/// A guest thread's host thread: libretro-common's rthreads, through common/host_thread.h,
/// on which the guest's code runs, on its own stack.
class NativeThread {
public:
    using Func = void (*)(void*);

    NativeThread();
    ~NativeThread();
    NativeThread(const NativeThread&) = delete;
    NativeThread& operator=(const NativeThread&) = delete;

    int Create(Func func, void* arg);
    /// Ends the calling thread, from anywhere in its guest code (host_thread_exit)
    [[noreturn]] void Exit();

    void Initialize();

    /// What signals and APCs are sent to: the pthread_t, or on Windows a HANDLE
    uintptr_t GetHandle() {
        return native_handle;
    }

    u64 GetTid() {
        return tid;
    }

private:
    host_thread_t thread{};
    uintptr_t native_handle = 0;
    u64 tid = 0;
};

} // namespace Core
