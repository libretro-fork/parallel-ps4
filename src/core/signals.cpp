// SPDX-FileCopyrightText: Copyright 2024-2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <cstring>
#include <iterator>
#include <string>

#include <retro_atomic.h>
#include "common/arch.h"
#include "common/assert.h"
#include "common/hex_dump.h"
#include "common/logging/log.h"
#include "common/quick_exit.h"
#include "common/signal_context.h"
#include "common/string_util.h"
#include "common/x64_decode.h"
#include "core/cpu_patches.h" // Windows static guest red-zone protection
#include "core/libraries/kernel/kernel.h"
#include "core/libraries/kernel/threads/exception.h"
#include "core/libraries/kernel/threads/pthread.h"
#include "core/libraries/kernel/threads/thread_state.h"
#include "core/linker.h"
#include "core/module.h"
#include "core/signals.h"
#include "core/thread.h"
#ifdef SHADPS4_ENABLE_FEX_GUEST_CPU
#include "core/fex/fex_guest_engine.h"
#include "core/libraries/kernel/threads/pthread.h"
#include "core/libraries/kernel/threads/thread_state.h"
#endif
#include "emulator.h"

#ifdef _WIN32
#include <windows.h>
static constexpr DWORD MS_VC_EXCEPTION = 0x406D1388;
#else
#include <csignal>
#include <pthread.h>
#endif

namespace Core {

#if defined(_WIN32)

static LONG WINAPI CrashReport(EXCEPTION_POINTERS* info) noexcept;

static LONG WINAPI SignalHandler(EXCEPTION_POINTERS* pExp) noexcept {
    using namespace Libraries::Kernel;
    const auto* signals = Signals::Instance();

    const bool use_static_windows_guest_red_zone_protection =
        WindowsGuestRedZoneProtection::IsStaticPatchingEnabled();
    DWORD code = 0;
    PVOID address = nullptr;

    if (pExp != nullptr && pExp->ExceptionRecord != nullptr) {
        code = pExp->ExceptionRecord->ExceptionCode;
        address = pExp->ExceptionRecord->ExceptionAddress;
    }

    Ucontext guest_context{pExp->ContextRecord};
    Siginfo guest_info{
        ._si_signo = 0,
        ._si_errno = 0,
        ._si_code = POSIX_SI_NOINFO,
        ._si_addr = (void*)guest_context.uc_mcontext.mc_rip,
    };

    bool handled = false;
    bool static_protection_exception = false; // Windows static guest red-zone protection
    switch (code) {
    case EXCEPTION_ACCESS_VIOLATION:
        guest_info._si_signo = POSIX_SIGSEGV;
        guest_info._si_code = POSIX_SEGV_MAPERR;
        static_protection_exception = true; // Windows static guest red-zone protection
        handled = signals->DispatchAccessViolation(
            pExp, reinterpret_cast<void*>(pExp->ExceptionRecord->ExceptionInformation[1]));
        break;
    case EXCEPTION_ILLEGAL_INSTRUCTION:
        guest_info._si_signo = POSIX_SIGILL;
        guest_info._si_code = POSIX_ILL_ILLOPC;
        static_protection_exception = true; // Windows static guest red-zone protection
        handled = signals->DispatchIllegalInstruction(pExp);
        break;
    case EXCEPTION_PRIV_INSTRUCTION: // Windows static guest red-zone protection
        if (use_static_windows_guest_red_zone_protection) {
            static_protection_exception = true;
            handled = signals->DispatchIllegalInstruction(pExp);
        }
        break;
    case EXCEPTION_IN_PAGE_ERROR:
        guest_info._si_signo = POSIX_SIGBUS;
        guest_info._si_code = POSIX_BUS_ADRALN;
        break;
    case EXCEPTION_INT_DIVIDE_BY_ZERO:
        guest_info._si_signo = POSIX_SIGFPE;
        guest_info._si_code = POSIX_FPE_INTDIV;
        break;
    case EXCEPTION_INT_OVERFLOW:
        guest_info._si_signo = POSIX_SIGFPE;
        guest_info._si_code = POSIX_FPE_INTOVF;
        break;
    case EXCEPTION_FLT_DIVIDE_BY_ZERO:
        guest_info._si_signo = POSIX_SIGFPE;
        guest_info._si_code = POSIX_FPE_FLTDIV;
        break;
    case EXCEPTION_FLT_INVALID_OPERATION:
        guest_info._si_signo = POSIX_SIGFPE;
        guest_info._si_code = POSIX_FPE_FLTINV;
        break;
    case EXCEPTION_FLT_OVERFLOW:
        guest_info._si_signo = POSIX_SIGFPE;
        guest_info._si_code = POSIX_FPE_FLTOVF;
        break;
    case EXCEPTION_FLT_UNDERFLOW:
        guest_info._si_signo = POSIX_SIGFPE;
        guest_info._si_code = POSIX_FPE_FLTUND;
        break;
    case EXCEPTION_FLT_DENORMAL_OPERAND:
        guest_info._si_signo = POSIX_SIGFPE;
        guest_info._si_code = POSIX_FPE_FLTSUB; // i am not sure about this one
        break;
    case EXCEPTION_FLT_INEXACT_RESULT:
        guest_info._si_signo = POSIX_SIGFPE;
        guest_info._si_code = POSIX_FPE_FLTRES;
        break;
    case EXCEPTION_FLT_STACK_CHECK:
        guest_info._si_signo = POSIX_SIGILL;
        guest_info._si_code = POSIX_ILL_BADSTK; // i am not sure about this one either
        break;
    case EXCEPTION_BREAKPOINT:
    case EXCEPTION_SINGLE_STEP:
        guest_info._si_signo = POSIX_SIGTRAP;
        guest_info._si_code = POSIX_TRAP_BRKPT;
        break;
    case DBG_PRINTEXCEPTION_C:
    case DBG_PRINTEXCEPTION_WIDE_C:
        // Used by OutputDebugString functions.
        return EXCEPTION_CONTINUE_EXECUTION;
    case MS_VC_EXCEPTION:
        LOG_DEBUG(Debug, "Pass MS_VC_EXCEPTION at 0x%llx to handler",
                  static_cast<unsigned long long>(reinterpret_cast<uintptr_t>(address)));
        return EXCEPTION_EXECUTE_HANDLER;
    default:
        break;
    }

    if (handled) {
        return EXCEPTION_CONTINUE_EXECUTION;
    }

    if (guest_info._si_signo != 0) {
        if (g_curthread &&
            g_curthread->DispatchSignal(guest_info._si_signo, &guest_info, &guest_context)) {
            return EXCEPTION_CONTINUE_EXECUTION;
        }
    }

    const bool report_unhandled =
        use_static_windows_guest_red_zone_protection ? static_protection_exception : true;
    if (report_unhandled) {
        // The ones nothing after this handler can make good: reported in full here. The
        // filter of last resort is reached through the thread's frames, and from the game's
        // code, which has no unwind tables, it is not reached at all.
        switch (code) {
        case EXCEPTION_ACCESS_VIOLATION:
        case EXCEPTION_ILLEGAL_INSTRUCTION:
        case EXCEPTION_PRIV_INSTRUCTION:
        case EXCEPTION_IN_PAGE_ERROR:
        case EXCEPTION_STACK_OVERFLOW:
        case EXCEPTION_INT_DIVIDE_BY_ZERO:
            CrashReport(pExp);
            break;
        default:
            LOG_CRITICAL(Debug, "Unhandled Exception code 0x%llx at 0x%llx",
                         static_cast<unsigned long long>(code),
                         static_cast<unsigned long long>(reinterpret_cast<uintptr_t>(address)));
            break;
        }
        Common::Singleton<Core::Emulator>::Instance()->Shutdown();
    }

    return EXCEPTION_CONTINUE_SEARCH;
}

#else

static std::string DisassembleInstruction(void* code_address) {
    char buffer[256] = "<unable to decode>";

#ifdef ARCH_X86_64
    x64_insn_t instruction;
    x64_operand_t operands[X64_MAX_OPERANDS];
    if (x64_decode(static_cast<const u8*>(code_address), 15, &instruction)) {
        x64_operands(&instruction, operands);
        x64_format(&instruction, operands, reinterpret_cast<u64>(code_address), buffer,
                   sizeof(buffer));
    }
#endif

    return buffer;
}

static s32 NativeSiCodeToGuest(s32 sig, s32 code) {
    using namespace Libraries::Kernel;
    switch (sig) {
    case SIGUSR1:
        return POSIX_SI_LWP;
    case SIGSEGV:
        switch (code) {
        case SEGV_MAPERR:
            return POSIX_SEGV_MAPERR;
        case SEGV_ACCERR:
            return POSIX_SEGV_ACCERR;
        }
    case SIGBUS:
        switch (code) {
        case BUS_ADRALN:
            return POSIX_BUS_ADRALN;
        case BUS_ADRERR:
            return POSIX_BUS_ADRERR;
        case BUS_OBJERR:
            return POSIX_BUS_OBJERR;
        }
    case SIGILL:
        switch (code) {
        case ILL_ILLOPC:
            return POSIX_ILL_ILLOPC;
        case ILL_ILLOPN:
            return POSIX_ILL_ILLOPN;
        case ILL_ILLADR:
            return POSIX_ILL_ILLADR;
        case ILL_ILLTRP:
            return POSIX_ILL_ILLTRP;
        case ILL_PRVOPC:
            return POSIX_ILL_PRVOPC;
        case ILL_PRVREG:
            return POSIX_ILL_PRVREG;
        case ILL_COPROC:
            return POSIX_ILL_COPROC;
        case ILL_BADSTK:
            return POSIX_ILL_BADSTK;
        }
    case SIGFPE:
        switch (code) {
        case FPE_INTOVF:
            return POSIX_FPE_INTOVF;
        case FPE_INTDIV:
            return POSIX_FPE_INTDIV;
        case FPE_FLTDIV:
            return POSIX_FPE_FLTDIV;
        case FPE_FLTOVF:
            return POSIX_FPE_FLTOVF;
        case FPE_FLTUND:
            return POSIX_FPE_FLTUND;
        case FPE_FLTRES:
            return POSIX_FPE_FLTRES;
        case FPE_FLTINV:
            return POSIX_FPE_FLTINV;
        case FPE_FLTSUB:
            return POSIX_FPE_FLTSUB;
        }
    case SIGTRAP:
        switch (code) {
        case TRAP_BRKPT:
            return POSIX_TRAP_BRKPT;
        case TRAP_TRACE:
            return POSIX_TRAP_TRACE;
#ifdef __FreeBSD__
        case TRAP_DTRACE:
            return POSIX_TRAP_DTRACE;
#endif
        }

    default:
        return POSIX_SI_NOINFO;
    }
}

// The signals SignalDispatch takes over, and what was installed for them
// before: put back by RemoveHandlers (the libretro frontend's process outlives
// the core), and handed a signal on a thread that is not the game's, which is
// the frontend's or a library's to report, not the emulator's (sco8487)
static constexpr int s_handled_signals[] = {SIGSEGV, SIGBUS,  SIGILL,  SIGABRT, SIGTRAP,
                                            SIGSYS,  SIGUSR1, SIGFPE,  SIGSLEEP};
static struct sigaction s_previous_actions[std::size(s_handled_signals)];
static bool s_handlers_installed = false;

// The previous action back for this signal, and the signal to it: a fault runs
// its instruction again under it, a sent signal (abort, raise, kill) is sent
// again, delivered once this handler returns
static void PassToPrevious(int sig, siginfo_t* info) {
    for (size_t i = 0; i < std::size(s_handled_signals); i++) {
        if (s_handled_signals[i] == sig) {
            sigaction(sig, &s_previous_actions[i], nullptr);
            break;
        }
    }
    if (!info || info->si_code <= 0 || sig == SIGTRAP) {
        raise(sig);
    }
}

void SignalHandler(int sig, siginfo_t* info, void* raw_context) {
    using namespace Libraries::Kernel;
    auto* thread = g_curthread;
    const auto* signals = Signals::Instance();

    auto* code_address = Common::GetRip(raw_context);

    Ucontext context{info, reinterpret_cast<ucontext_t*>(raw_context)};
    Siginfo guest_info{};
    if (info) {
        guest_info = *reinterpret_cast<Siginfo*>(info);
        guest_info._si_signo = sig == SIGUSR1 ? 0 : NativeToOrbisSignal(info->si_signo);
        guest_info._si_errno = NativeToPosixErrno(info->si_errno);
        guest_info._si_code = NativeSiCodeToGuest(sig, info->si_code);
        guest_info._si_addr = (void*)context.uc_mcontext.mc_rip;
    }
    Siginfo* info_p = info ? &guest_info : nullptr;
    Ucontext* context_p = raw_context ? &context : nullptr;

    switch (sig) {
    case SIGSEGV:
    case SIGBUS: {
#ifdef SHADPS4_ENABLE_FEX_GUEST_CPU
        // An unaligned atomic in FEX's translated code, which it fixes up itself
        if (sig == SIGBUS && ::Core::Fex::HandleGuestSignal(sig, info, raw_context)) {
            return;
        }
#endif
        const bool is_write = Common::IsWriteError(raw_context);
        const bool is_exec = Common::IsExecuteError(raw_context);
        if (!signals->DispatchAccessViolation(raw_context, info->si_addr)) {
            if (thread && thread->DispatchSignal(NativeToOrbisSignal(sig), info_p, context_p)) {
                return;
            }
            if (!thread) {
                PassToPrevious(sig, info);
                return;
            }
#ifdef SHADPS4_ENABLE_FEX_GUEST_CPU
            uint64_t guest_rip = 0;
            uint64_t guest_rax = 0;
            if (::Core::Fex::BachataQueryGuestRipSyscall(&guest_rip, &guest_rax)) {
                LOG_CRITICAL(Debug, "FEX guest state at fault: rip=0x%llx rax=0x%llx",
                             static_cast<unsigned long long>(guest_rip),
                             static_cast<unsigned long long>(guest_rax));
            }
            // The game's threads and their stacks, for a fault in a scan of them (a GC)
            for (const auto* t = Libraries::Kernel::ThrState::Instance()->threads; t != nullptr;
                 t = t->thread_next) {
                const auto base = reinterpret_cast<u64>(t->attr.stackaddr_attr);
                char thread_name[WORD_NAME_MAX + 1];
                word_name_get(&t->name, thread_name);
                LOG_CRITICAL(Debug, "thread '%s': stack 0x%llx-0x%llx", thread_name,
                             static_cast<unsigned long long>(base),
                             static_cast<unsigned long long>(base + t->attr.stacksize_attr));
            }
#endif
            UNREACHABLE_MSG(
                "Unhandled access violation at code address 0x%llx: %s address 0x%llx",
                static_cast<unsigned long long>(reinterpret_cast<uintptr_t>(code_address)),
                is_write  ? "Write to"
                : is_exec ? "Executed from"
                          : "Read from",
                static_cast<unsigned long long>(reinterpret_cast<uintptr_t>(info->si_addr)));
        }
        break;
    }
    case SIGILL:
        if (signals->DispatchIllegalInstruction(raw_context)) {
            return;
        }
    case SIGFPE:
    case SIGTRAP:
    case SIGABRT:
    case SIGSYS: {
        if (thread && thread->DispatchSignal(NativeToOrbisSignal(sig), info_p, context_p)) {
            return;
        }
        if (!thread) {
            PassToPrevious(sig, info);
            return;
        }

        UNREACHABLE_MSG("Unhandled signal %d at code address 0x%llx", sig,
                        static_cast<unsigned long long>(reinterpret_cast<uintptr_t>(code_address)));
    }
    case SIGSLEEP: {
        if (thread) {
            thread->suspended_context = context.uc_mcontext;
            (void)retro_atomic_exchange_int(&thread->is_suspended_in_signal, 1);
        }
        // Sleep thread until signal is received again
        sigset_t sigset;
        sigemptyset(&sigset);
        sigaddset(&sigset, SIGSLEEP);
        sigwait(&sigset, &sig);
        if (thread) {
            (void)retro_atomic_exchange_int(&thread->is_suspended_in_signal, 0);
        }
        break;
    }
    case SIGUSR1:
        if (thread) {
            thread->DispatchPendingSignals(info_p, context_p);
        }
        break;
    default:
        UNREACHABLE_MSG("Unhandled signal %d at code address 0x%llx", sig,
                        static_cast<unsigned long long>(reinterpret_cast<uintptr_t>(code_address)));
    }
}

#endif

#if defined(_WIN32)
// Where an address is, for the crash report: its module and the offset into it, or that it is
// in none (the game's code, or code generated for it).
static std::string DescribeAddress(u64 address) {
    HMODULE module = nullptr;
    char path[MAX_PATH];
    if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                               GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                           reinterpret_cast<LPCSTR>(address), &module) &&
        module && GetModuleFileNameA(module, path, sizeof(path))) {
        const char* name = std::strrchr(path, '\\');
        return Common::StringFromFormat("0x%" PRIx64 " (%s+0x%" PRIx64 ")", address,
                                        name ? name + 1 : path,
                                        address - reinterpret_cast<u64>(module));
    }
    // One of the game's own modules, by the address it was loaded at
    if (const auto* guest = Common::Singleton<Core::Linker>::Instance()->FindByAddress(address)) {
        return Common::StringFromFormat(
            "0x%" PRIx64 " (the game's %s+0x%llx)", address, guest->name.c_str(),
            static_cast<unsigned long long>(address - guest->GetBaseAddress()));
    }
    return Common::StringFromFormat("0x%" PRIx64 " (in no module: generated code or data)",
                                    address);
}

// Memory read for the crash report: false where it cannot be read, never a fault
static bool ReadMemory(u64 address, void* out, size_t size) {
    SIZE_T read = 0;
    return ReadProcessMemory(GetCurrentProcess(), reinterpret_cast<LPCVOID>(address), out, size,
                             &read) &&
           read == size;
}

static bool InHostModule(u64 address) {
    HMODULE module = nullptr;
    return GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                                  GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                              reinterpret_cast<LPCSTR>(address), &module) &&
           module;
}

#if defined(_M_X64) || defined(__x86_64__)
// The game's callers above a frame of its code, by its frame pointers (it is built with them):
// the unwind tables end where the game's code starts. @shown says whether any was found.
static int LogGameCallers(u64 rbp, const char* indent, int depth, int limit,
                          u64* callers = nullptr) {
    auto* linker = Common::Singleton<Core::Linker>::Instance();
    int shown = 0;
    for (; shown < limit; ++shown) {
        u64 frame[2]; // the caller's frame pointer, then the return address
        if (rbp == 0 || (rbp & 7) != 0 || !ReadMemory(rbp, frame, sizeof(frame)) ||
            !linker->FindByAddress(frame[1])) {
            break;
        }
        LOG_CRITICAL(Debug, "%s#%d %s", indent, depth + shown, (DescribeAddress(frame[1])).c_str());
        if (callers) {
            callers[shown] = frame[1];
        }
        if (frame[0] <= rbp) {
            break; // a caller's frame is above its callee's
        }
        rbp = frame[0];
    }
    return shown;
}

// What is in memory around each address the registers hold: what the code was looking at when
// it crashed (the object a garbage pointer came out of, and what lies before and after it).
// Addresses in the emulator's own modules, and what cannot be read, are left out.
static void LogMemoryAroundRegisters(const CONTEXT& ctx) {
    static constexpr struct {
        const char* name;
        DWORD64 CONTEXT::* value;
    } registers[] = {
        {"rdi", &CONTEXT::Rdi}, {"rsi", &CONTEXT::Rsi}, {"rax", &CONTEXT::Rax},
        {"rbx", &CONTEXT::Rbx}, {"rcx", &CONTEXT::Rcx}, {"rdx", &CONTEXT::Rdx},
        {"r8", &CONTEXT::R8},   {"r9", &CONTEXT::R9},   {"r10", &CONTEXT::R10},
        {"r11", &CONTEXT::R11}, {"r12", &CONTEXT::R12}, {"r13", &CONTEXT::R13},
        {"r14", &CONTEXT::R14}, {"r15", &CONTEXT::R15},
    };
    // More before the first two, a call's arguments: where what was written over an object
    // starts is as telling as the object
    static constexpr u64 Before = 0x100, BeforeArgument = 0x400, After = 0x100;
    u64 shown[std::size(registers)][2];
    size_t shown_count = 0;
    for (const auto& reg : registers) {
        const u64 address = ctx.*(reg.value);
        u64 start = 0, length = 0;
        hex_dump_window(address, &reg < &registers[2] ? BeforeArgument : Before, After, &start,
                        &length);
        bool covered = address < 0x10000 || InHostModule(address);
        for (size_t i = 0; i < shown_count && !covered; i++) {
            covered = address >= shown[i][0] && address < shown[i][0] + shown[i][1];
        }
        u8 bytes[HEX_DUMP_LINE_BYTES];
        if (covered || !ReadMemory(address & ~u64{HEX_DUMP_LINE_BYTES - 1}, bytes, sizeof(bytes))) {
            continue;
        }
        shown[shown_count][0] = start;
        shown[shown_count][1] = length;
        ++shown_count;
        LOG_CRITICAL(Debug, "  memory around %s = 0x%" PRIx64 ":", reg.name, address);
        for (u64 at = start; at < start + length; at += HEX_DUMP_LINE_BYTES) {
            char line[HEX_DUMP_LINE_CHARS];
            if (!ReadMemory(at, bytes, sizeof(bytes))) {
                continue; // a page that is not there, before or after what is
            }
            hex_dump_line(line, sizeof(line), bytes, sizeof(bytes));
            LOG_CRITICAL(Debug, "    0x%" PRIx64 ": %s", at, line);
        }
    }
}

// A dump of @length bytes from @start, the lines that can be read
static void LogMemory(u64 start, u64 length) {
    for (u64 at = start; at < start + length; at += HEX_DUMP_LINE_BYTES) {
        u8 bytes[HEX_DUMP_LINE_BYTES];
        char line[HEX_DUMP_LINE_CHARS];
        if (ReadMemory(at, bytes, sizeof(bytes))) {
            hex_dump_line(line, sizeof(line), bytes, sizeof(bytes));
            LOG_CRITICAL(Debug, "    0x%" PRIx64 ": %s", at, line);
        }
    }
}

// The game's instructions up to and after @address: where a register's value came from is in
// the code before the one that crashed, and before each call above it. More before than after,
// and from a line's start, so two runs line up.
static void LogCode(const char* what, u64 address) {
    static constexpr u64 Before = 0x100, After = 0x40;
    const u64 start = (address - Before) & ~u64{HEX_DUMP_LINE_BYTES - 1};
    LOG_CRITICAL(Debug, "  code around %s = 0x%" PRIx64 ":", what, address);
    LogMemory(start, address + After - start);
}

// When an argument of the call that crashed points into packed data (a compressed or encoded
// stream, where an object of the game's should be): how far that data runs either way, and what
// is at its two ends. Where it starts is the buffer it was written into, or the one it ran out
// of; its first bytes are often a header that names what it is.
static void LogPackedRun(const char* name, u64 address) {
    static constexpr u64 Line = HEX_DUMP_LINE_BYTES, Page = 0x1000, Limit = u64{64} << 20;
    static constexpr int Gap = 4; // lines that are not packed, in a row, end the run
    const u64 origin = address & ~(Line - 1);
    u8 page[Page];
    u64 page_at = ~u64{0};
    const auto packed = [&](u64 at) {
        if ((at & ~(Page - 1)) != page_at) {
            page_at = at & ~(Page - 1);
            if (!ReadMemory(page_at, page, Page)) {
                page_at = ~u64{0};
                return -1;
            }
        }
        return hex_dump_packed(page + (at - page_at), Line);
    };
    if (address < 0x10000 || InHostModule(address) || packed(origin) != 1) {
        return;
    }
    u64 start = origin, end = origin + Line;
    int gap = 0;
    for (u64 at = origin; at >= Line && origin - at < Limit && gap < Gap;) {
        at -= Line;
        const int is = packed(at);
        if (is < 0) {
            break;
        }
        gap = is ? 0 : gap + 1;
        if (is) {
            start = at;
        }
    }
    gap = 0;
    for (u64 at = origin + Line; at - origin < Limit && gap < Gap; at += Line) {
        const int is = packed(at);
        if (is < 0) {
            break;
        }
        gap = is ? 0 : gap + 1;
        if (is) {
            end = at + Line;
        }
    }
    LOG_CRITICAL(Debug,
                 "  %s = 0x%" PRIx64 " is inside packed data from 0x%" PRIx64 " to 0x%" PRIx64
                 " (%" PRIu64 " bytes)",
                 name, address, start, end, end - start);
    LOG_CRITICAL(Debug, "  where it starts:");
    LogMemory(start >= 0x100 ? start - 0x100 : 0, 0x200);
    LOG_CRITICAL(Debug, "  where it ends:");
    LogMemory(end - 0x80, 0x180);
}

// Where each of the game's other threads was: one of them may be the one that wrote what the
// crashed thread tripped over. Each is stopped only for as long as reading its registers takes,
// so that none is left holding a lock the report itself needs.
static void LogOtherGameThreads() {
    using namespace Libraries::Kernel;
    auto* state = ThrState::Peek();
    if (!state) {
        return;
    }
    auto* linker = Common::Singleton<Core::Linker>::Instance();
    int count = 0;
    for (Pthread* thread = state->threads; thread != nullptr && count < 256;
         thread = thread->thread_next, ++count) {
        if (!state->IsThreadRecord(thread)) {
            break; // the list changed under the walk
        }
        if (thread == g_curthread || !thread->native_thr) {
            continue;
        }
        const HANDLE handle = reinterpret_cast<HANDLE>(thread->native_thr->GetHandle());
        CONTEXT ctx{};
        ctx.ContextFlags = CONTEXT_CONTROL | CONTEXT_INTEGER;
        if (!handle || SuspendThread(handle) == static_cast<DWORD>(-1)) {
            continue;
        }
        const bool got = GetThreadContext(handle, &ctx) != 0;
        ResumeThread(handle);
        if (!got) {
            continue;
        }
        char thread_name[WORD_NAME_MAX + 1];
        word_name_get(&thread->name, thread_name);
        LOG_CRITICAL(
            Debug, "  thread '%s' at %s: rdi=0x%llx rsi=0x%llx rdx=0x%llx rcx=0x%llx r8=0x%llx",
            thread_name, (DescribeAddress(ctx.Rip)).c_str(),
            static_cast<unsigned long long>(ctx.Rdi), static_cast<unsigned long long>(ctx.Rsi),
            static_cast<unsigned long long>(ctx.Rdx), static_cast<unsigned long long>(ctx.Rcx),
            static_cast<unsigned long long>(ctx.R8));
        if (LogGameCallers(ctx.Rbp, "    ", 1, 6) != 0) {
            continue;
        }
        // In the emulator's code, which keeps no frame pointer: the game's code addresses on
        // its stack instead, its callers among them
        int found = 0;
        for (u64 at = ctx.Rsp & ~u64{7}; at < ctx.Rsp + 0x600 && found < 4; at += 8) {
            u64 value = 0;
            if (!ReadMemory(at, &value, sizeof(value))) {
                break;
            }
            if (linker->FindByAddress(value)) {
                LOG_CRITICAL(Debug, "    on its stack: %s", (DescribeAddress(value)).c_str());
                ++found;
            }
        }
    }
}
#endif

// The last word on an exception nothing handled, just before Windows ends the process: what
// happened, where, and how it got there, written and flushed to the log. Without it a crash
// left a log with nothing about it, or with nothing at all: the lines before it were still
// in the writer's buffers.
static LONG WINAPI CrashReport(EXCEPTION_POINTERS* info) noexcept {
    static retro_atomic_int_t reported{0};
    if (!info || !info->ExceptionRecord || !info->ContextRecord ||
        retro_atomic_exchange_int(&reported, 1) != 0) {
        return EXCEPTION_CONTINUE_SEARCH;
    }
    const EXCEPTION_RECORD& record = *info->ExceptionRecord;
    const CONTEXT& ctx = *info->ContextRecord;
    LOG_CRITICAL(Debug, "The emulator crashed: exception 0x%llx at %s",
                 static_cast<unsigned long long>(record.ExceptionCode),
                 (DescribeAddress(reinterpret_cast<u64>(record.ExceptionAddress))).c_str());
    if (record.ExceptionCode == EXCEPTION_ACCESS_VIOLATION && record.NumberParameters >= 2) {
        static constexpr const char* kinds[] = {"reading", "writing"};
        const u64 kind = record.ExceptionInformation[0];
        LOG_CRITICAL(Debug, "  %s address 0x%llx", kind == 8 ? "executing" : kinds[kind & 1],
                     static_cast<unsigned long long>(record.ExceptionInformation[1]));
    }
    char current_name[WORD_NAME_MAX + 1] = "(not a game thread)";
    if (Libraries::Kernel::g_curthread) {
        word_name_get(&Libraries::Kernel::g_curthread->name, current_name);
    }
    LOG_CRITICAL(Debug, "  thread %llu '%s'", static_cast<unsigned long long>(GetCurrentThreadId()),
                 current_name);
#if defined(_M_X64) || defined(__x86_64__)
    LOG_CRITICAL(Debug, "  rax=0x%llx rbx=0x%llx rcx=0x%llx rdx=0x%llx rsi=0x%llx rdi=0x%llx",
                 static_cast<unsigned long long>(ctx.Rax), static_cast<unsigned long long>(ctx.Rbx),
                 static_cast<unsigned long long>(ctx.Rcx), static_cast<unsigned long long>(ctx.Rdx),
                 static_cast<unsigned long long>(ctx.Rsi),
                 static_cast<unsigned long long>(ctx.Rdi));
    LOG_CRITICAL(Debug, "  rbp=0x%llx rsp=0x%llx r8=0x%llx r9=0x%llx r10=0x%llx r11=0x%llx",
                 static_cast<unsigned long long>(ctx.Rbp), static_cast<unsigned long long>(ctx.Rsp),
                 static_cast<unsigned long long>(ctx.R8), static_cast<unsigned long long>(ctx.R9),
                 static_cast<unsigned long long>(ctx.R10),
                 static_cast<unsigned long long>(ctx.R11));
    LOG_CRITICAL(Debug, "  r12=0x%llx r13=0x%llx r14=0x%llx r15=0x%llx rip=0x%llx",
                 static_cast<unsigned long long>(ctx.R12), static_cast<unsigned long long>(ctx.R13),
                 static_cast<unsigned long long>(ctx.R14), static_cast<unsigned long long>(ctx.R15),
                 static_cast<unsigned long long>(ctx.Rip));
    // The callers, as far as the unwind tables go: they end at the game's code
    CONTEXT frame = ctx;
    int depth = 0;
    for (; depth < 24 && frame.Rip != 0; ++depth) {
        LOG_CRITICAL(Debug, "  #%d %s", depth, (DescribeAddress(frame.Rip)).c_str());
        DWORD64 image_base = 0;
        PRUNTIME_FUNCTION function = RtlLookupFunctionEntry(frame.Rip, &image_base, nullptr);
        if (!function) {
            // A leaf function has no entry: its return address is at the top of its stack.
            // Outside the modules (the game's code) there are no tables to go by: the end.
            HMODULE module = nullptr;
            SIZE_T read = 0;
            u64 return_address = 0;
            if (!GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                                        GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                                    reinterpret_cast<LPCSTR>(frame.Rip), &module) ||
                !ReadProcessMemory(GetCurrentProcess(), reinterpret_cast<LPCVOID>(frame.Rsp),
                                   &return_address, sizeof(return_address), &read) ||
                read != sizeof(return_address)) {
                break;
            }
            frame.Rip = return_address;
            frame.Rsp += 8;
            continue;
        }
        void* handler_data = nullptr;
        DWORD64 establisher = 0;
        RtlVirtualUnwind(UNW_FLAG_NHANDLER, image_base, frame.Rip, function, &frame, &handler_data,
                         &establisher, nullptr);
    }
    // What is written so far is in the log whatever becomes of the rest
    Common::Log::Flush();
    // The game's own callers above that, and what the registers point at
    static constexpr int CodeCallers = 4;
    u64 callers[16] = {};
    int caller_count = 0;
    if (!InHostModule(frame.Rip)) {
        caller_count = LogGameCallers(frame.Rbp, "  ", depth + 1, 16, callers);
    }
    LogMemoryAroundRegisters(ctx);
    // The game's code where it crashed and where it was called from
    if (!InHostModule(ctx.Rip)) {
        LogCode("rip", ctx.Rip);
        for (int i = 0; i < caller_count && i < CodeCallers; i++) {
            LogCode("a caller's return", callers[i]);
        }
    }
    LogPackedRun("rdi", ctx.Rdi);
    LogPackedRun("rsi", ctx.Rsi);
    Common::Log::Flush();
    LogOtherGameThreads();
#endif
    Common::Log::Flush();
    return EXCEPTION_CONTINUE_SEARCH;
}
#endif

void InstallCrashReport() {
#if defined(_WIN32)
    SetUnhandledExceptionFilter(CrashReport);
#endif
}

SignalDispatch::SignalDispatch() {
#if defined(_WIN32)
    ASSERT_MSG(handle = AddVectoredExceptionHandler(0, SignalHandler),
               "Failed to register exception handler.");
    InstallCrashReport();
#else
    struct sigaction action{};
    action.sa_sigaction = SignalHandler;
    action.sa_flags = SA_SIGINFO | SA_ONSTACK;
    sigemptyset(&action.sa_mask);

    bool ok = true;
    for (size_t i = 0; i < std::size(s_handled_signals); i++) {
        ok = sigaction(s_handled_signals[i], &action, &s_previous_actions[i]) == 0 && ok;
    }
    s_handlers_installed = true;
    ASSERT_MSG(ok, "Failed to register signal handlers.");
#endif
}

void SignalDispatch::RemoveHandlers() {
    // asserting here would get into an infinite loop until too
    // many nested exceptions makes the OS kill the process
#if defined(_WIN32)
    if (!(RemoveVectoredExceptionHandler(handle))) {
        LOG_CRITICAL(Core, "Failed to remove exception handler.");
        Common::QuickExit(1);
    }
#else
    // the ones there were before, not the default: those are the frontend's,
    // or the system's crash reporter (Android's tombstone)
    if (!s_handlers_installed) {
        return;
    }
    s_handlers_installed = false;
    bool ok = true;
    for (size_t i = 0; i < std::size(s_handled_signals); i++) {
        ok = sigaction(s_handled_signals[i], &s_previous_actions[i], nullptr) == 0 && ok;
    }
    if (!ok) {
        LOG_CRITICAL(Core, "Failed to remove signal handlers.");
    }
#endif
}

SignalDispatch::~SignalDispatch() {
    RemoveHandlers();
}

bool SignalDispatch::DispatchAccessViolation(void* context, void* fault_address) const {
    for (const auto& [handler, _] : access_violation_handlers) {
        if (handler(context, fault_address)) {
            return true;
        }
    }
    return false;
}

bool SignalDispatch::DispatchIllegalInstruction(void* context) const {
    for (const auto& [handler, _] : illegal_instruction_handlers) {
        if (handler(context)) {
            return true;
        }
    }
    return false;
}

} // namespace Core
