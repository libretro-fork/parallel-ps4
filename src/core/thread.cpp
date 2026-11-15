// SPDX-FileCopyrightText: Copyright 2024-2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <algorithm>

#include <rthreads/rthreads.h>

#include "common/alignment.h"
#include "common/arch.h"
#include "common/assert.h"
#include "thread.h"
#ifdef _WIN64
#include <windows.h>
#else
#include <cerrno>
#include <csignal>
#include <cstdlib>
#include <unistd.h>
#ifdef ARCH_X86_64
#include <xmmintrin.h>
#endif
#endif

namespace Core {

static constexpr u32 ORBIS_MXCSR = 0x9fc0;
static constexpr u32 ORBIS_FPUCW = 0x037f;

#ifndef _WIN64
namespace {
/// The alternate signal stack: the thread's own, not the NativeThread's, which the guest
/// thread's joiner may free as soon as the thread is marked ended, before it is gone
thread_local void* sig_stack_ptr = nullptr;
} // namespace
#endif

NativeThread::NativeThread() = default;

NativeThread::~NativeThread() {
#ifdef _WIN64
    if (native_handle) {
        CloseHandle(reinterpret_cast<HANDLE>(native_handle));
    }
#endif
    // Ended by now, or ending: rthreads frees what it holds for it once it has
    host_thread_release(&thread);
}

int NativeThread::Create(Func func, void* arg) {
    if (!host_thread_start(&thread, func, arg)) {
        return -1;
    }
#ifdef _WIN64
    // rthreads gives the id; the APCs and the debugger's suspend take a handle
    native_handle = reinterpret_cast<uintptr_t>(
        OpenThread(THREAD_ALL_ACCESS, FALSE, static_cast<DWORD>(thread.id)));
    if (!native_handle) {
        return static_cast<int>(GetLastError());
    }
#else
    native_handle = thread.id;
#endif
    return 0;
}

void NativeThread::Exit() {
    tid = 0;
#ifndef _WIN64
    // Disable and free the signal stack.
    constexpr stack_t sig_stack = {
        .ss_flags = SS_DISABLE,
    };
    sigaltstack(&sig_stack, nullptr);
    if (sig_stack_ptr) {
        free(sig_stack_ptr);
        sig_stack_ptr = nullptr;
    }
#endif
    host_thread_exit();
    UNREACHABLE();
}

void NativeThread::Initialize() {
#ifdef ARCH_X86_64
    // Set MXCSR and FPUCW registers to the values used by Orbis.
    _mm_setcsr(ORBIS_MXCSR);
    asm volatile("fldcw %0" : : "m"(ORBIS_FPUCW));
#endif
    tid = static_cast<u64>(sthread_get_current_thread_id());
#ifndef _WIN64
    // Set up an alternate signal handler stack to avoid overflowing small thread stacks.
    const size_t page_size = getpagesize();
    const size_t sig_stack_size = Common::AlignUp(std::max<size_t>(64_KB, MINSIGSTKSZ), page_size);
    ASSERT_MSG(posix_memalign(&sig_stack_ptr, page_size, sig_stack_size) == 0,
               "Failed to allocate signal stack: {}", errno);

    stack_t sig_stack;
    sig_stack.ss_sp = sig_stack_ptr;
    sig_stack.ss_size = sig_stack_size;
    sig_stack.ss_flags = 0;
    ASSERT_MSG(sigaltstack(&sig_stack, nullptr) == 0, "Failed to set signal stack: {}", errno);
#endif
}

} // namespace Core
