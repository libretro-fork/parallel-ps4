// SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "common/arch.h"
#include "common/signal_context.h"

#ifdef _WIN32
#include <windows.h>
#elif defined(__FreeBSD__)
#include <machine/npx.h>
#include <sys/ucontext.h>
#else
#include <sys/ucontext.h>
#if defined(__linux__) && defined(ARCH_ARM64)
#include <asm/sigcontext.h>
#include "common/types.h"
#endif
#endif

namespace Common {

#if defined(__linux__) && defined(ARCH_ARM64)
// The fault's syndrome register, which the kernel hands over as one of the
// records in the context's reserved area
static u64 LinuxArm64Esr(void* ctx) {
    const u8* record = reinterpret_cast<const u8*>(((ucontext_t*)ctx)->uc_mcontext.__reserved);
    const u8* end = record + sizeof(((ucontext_t*)ctx)->uc_mcontext.__reserved);
    while (record + sizeof(_aarch64_ctx) <= end) {
        const _aarch64_ctx* head = reinterpret_cast<const _aarch64_ctx*>(record);
        if (head->magic == ESR_MAGIC) {
            return reinterpret_cast<const esr_context*>(record)->esr;
        }
        if (head->magic == 0 || head->size == 0) {
            break;
        }
        record += head->size;
    }
    return 0;
}
#endif

void* GetRip(void* ctx) {
#if defined(_WIN32)
    return (void*)((EXCEPTION_POINTERS*)ctx)->ContextRecord->Rip;
#elif defined(__APPLE__) && defined(ARCH_X86_64)
    return (void*)((ucontext_t*)ctx)->uc_mcontext->__ss.__rip;
#elif defined(__APPLE__) && defined(ARCH_ARM64)
    return (void*)((ucontext_t*)ctx)->uc_mcontext->__ss.__pc;
#elif defined(__FreeBSD__)
    return (void*)((ucontext_t*)ctx)->uc_mcontext.mc_rip;
#elif defined(ARCH_X86_64)
    return (void*)((ucontext_t*)ctx)->uc_mcontext.gregs[REG_RIP];
#elif defined(__linux__) && defined(ARCH_ARM64)
    return (void*)((ucontext_t*)ctx)->uc_mcontext.pc;
#else
#error "Unsupported architecture"
#endif
}

bool IsWriteError(void* ctx) {
#if defined(_WIN32)
    return ((EXCEPTION_POINTERS*)ctx)->ExceptionRecord->ExceptionInformation[0] == 1;
#elif defined(__APPLE__) && defined(ARCH_X86_64)
    return ((ucontext_t*)ctx)->uc_mcontext->__es.__err & 0x2;
#elif defined(__APPLE__) && defined(ARCH_ARM64)
    return ((ucontext_t*)ctx)->uc_mcontext->__es.__esr & 0x40;
#elif defined(__FreeBSD__) && defined(ARCH_X86_64)
    return ((ucontext_t*)ctx)->uc_mcontext.mc_err & 0x2;
#elif defined(ARCH_X86_64)
    return ((ucontext_t*)ctx)->uc_mcontext.gregs[REG_ERR] & 0x2;
#elif defined(__linux__) && defined(ARCH_ARM64)
    // WnR of a data abort
    return LinuxArm64Esr(ctx) & 0x40;
#else
#error "Unsupported architecture"
#endif
}

bool IsExecuteError(void* ctx) {
#if defined(_WIN32)
    return ((EXCEPTION_POINTERS*)ctx)->ExceptionRecord->ExceptionInformation[0] == 0xf;
#elif defined(__APPLE__) && defined(ARCH_X86_64)
    return ((ucontext_t*)ctx)->uc_mcontext->__es.__err & 0x10;
#elif defined(__FreeBSD__) && defined(ARCH_X86_64)
    return ((ucontext_t*)ctx)->uc_mcontext.mc_err & 0x10;
#elif defined(ARCH_X86_64)
    return ((ucontext_t*)ctx)->uc_mcontext.gregs[REG_ERR] & 0x10;
#elif defined(__linux__) && defined(ARCH_ARM64)
    // Exception class: an instruction abort, from a lower or the same level
    const u64 ec = LinuxArm64Esr(ctx) >> 26;
    return ec == 0x20 || ec == 0x21;
#else
#error "Unsupported architecture"
#endif
}

} // namespace Common
