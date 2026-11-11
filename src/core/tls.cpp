// SPDX-FileCopyrightText: Copyright 2025-2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <mutex>
#include <retro_atomic.h>
#include "common/arch.h"
#include "common/assert.h"
#include "core/libraries/kernel/threads/pthread.h"
#include "core/tls.h"

#ifdef _WIN32
#include <windows.h>
#elif defined(__FreeBSD__)
#include <machine/sysarch.h>
#elif defined(__APPLE__) && defined(ARCH_X86_64)
#include <atomic>
#include <bit>
#include <architecture/i386/table.h>
#include <i386/user_ldt.h>
#include <sys/mman.h>
#elif !defined(ARCH_X86_64)
#include <pthread.h>
#endif
#if defined(__linux__) && defined(ARCH_X86_64)
#include <asm/prctl.h>
#include <sys/prctl.h>
#include <sys/syscall.h>
#include <unistd.h>
#endif

namespace Core {

#ifdef _WIN32

// Windows

static DWORD slot = 0;
static std::once_flag slot_alloc_flag;

static void AllocTcbKey() {
    slot = TlsAlloc();
}

u32 GetTcbKey() {
    std::call_once(slot_alloc_flag, &AllocTcbKey);
    return slot;
}

void SetTcbBase(void* image_address) {
    const BOOL result = TlsSetValue(GetTcbKey(), image_address);
    ASSERT(result != 0);
}

Tcb* GetTcbBase() {
    return reinterpret_cast<Tcb*>(TlsGetValue(GetTcbKey()));
}

#elif defined(__APPLE__) && defined(ARCH_X86_64)

// Apple x86_64

// Reserve space in the 32-bit address range for allocating TCB pages.
asm(".zerofill TCB_SPACE,TCB_SPACE,__tcb_space,0x3FC000");

struct LdtPage {
    Tcb tcb;
    u16 index;
};

static constexpr uintptr_t ldt_region_base = 0x4000;
static constexpr size_t ldt_region_size = 0x3FC000;
static constexpr u16 ldt_block_size = 0x1000;
static constexpr u16 ldt_index_base = 8;
static constexpr u16 ldt_index_total = (ldt_region_size - ldt_region_base) / ldt_block_size;

// One bit per LDT slot, set while it is free: a thread takes the lowest free one with a
// compare-and-swap and gives it back with an atomic or, so no lock is held.
static constexpr u32 ldt_free_words = (ldt_index_total + 63) / 64;
static retro_atomic_64_t free_ldts[ldt_free_words];

static void ReleaseLdtIndex(u16 index) {
    const u32 slot = index - ldt_index_base;
    retro_atomic_fetch_or_64(&free_ldts[slot / 64], u64{1} << (slot % 64));
}

static bool AcquireLdtIndex(u16* out) {
    for (u32 w = 0; w < ldt_free_words; ++w) {
        u64 bits = static_cast<u64>(retro_atomic_load_relaxed_64(&free_ldts[w]));
        while (bits != 0) {
            const u64 lowest = bits & (~bits + 1);
            if ((retro_atomic_cas_64(&free_ldts[w], bits, bits & ~lowest) ||
                 ((bits = static_cast<u64>(retro_atomic_load_acquire_64(&free_ldts[w]))), false))) {
                *out = static_cast<u16>(ldt_index_base + w * 64 + std::countr_zero(lowest));
                return true;
            }
        }
    }
    return false;
}
static std::once_flag ldt_region_init_flag;
static pthread_key_t ldt_page_slot = 0;

static void FreeLdtPage(void* raw) {
    const auto* ldt_page = static_cast<LdtPage*>(raw);

    ReleaseLdtIndex(ldt_page->index);
}

static void InitLdtRegion() {
    const void* result =
        mmap(reinterpret_cast<void*>(ldt_region_base), ldt_region_size, PROT_READ | PROT_WRITE,
             MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0);
    ASSERT_MSG(result != MAP_FAILED, "Failed to map memory region for LDT entries.");

    for (u32 slot = 0; slot < ldt_index_total; ++slot) {
        ReleaseLdtIndex(static_cast<u16>(ldt_index_base + slot));
    }
    ASSERT_MSG(pthread_key_create(&ldt_page_slot, FreeLdtPage) == 0,
               "Failed to create thread LDT page key: {}", errno);
}

void SetTcbBase(void* image_address) {
    std::call_once(ldt_region_init_flag, InitLdtRegion);

    auto* ldt_page = static_cast<LdtPage*>(pthread_getspecific(ldt_page_slot));
    if (ldt_page != nullptr) {
        // Update TCB in existing page.
        if (image_address != nullptr) {
            std::memcpy(&ldt_page->tcb, image_address, sizeof(Tcb));
        } else {
            std::memset(&ldt_page->tcb, 0, sizeof(Tcb));
        }
        return;
    }

    // Allocate a new LDT index for the current thread.
    u16 ldt_index;
    const bool acquired = AcquireLdtIndex(&ldt_index);
    ASSERT_MSG(acquired, "Out of LDT space.");

    const uintptr_t addr = ldt_region_base + (ldt_index - ldt_index_base) * ldt_block_size;

    // Create an LDT entry for the TCB.
    ldt_entry ldt{};
    ldt.data = {
        .limit00 = static_cast<u16>(ldt_block_size - 1),
        .base00 = static_cast<u16>(addr),
        .base16 = static_cast<u8>(addr >> 16),
        .type = DESC_DATA_WRITE,
        .dpl = 3,     // User accessible
        .present = 1, // Segment present
        .limit16 = 0,
        .stksz = DESC_DATA_32B,
        .granular = DESC_GRAN_BYTE,
        .base24 = static_cast<u8>(addr >> 24),
    };
    int ret = i386_set_ldt(ldt_index, &ldt, 1);
    ASSERT_MSG(ret == ldt_index,
               "Failed to set LDT {} at {:#x} for TLS area: syscall returned {}, errno {}",
               ldt_index, addr, ret, errno);

    // Set the FS segment to the created LDT.
    const sel_t new_selector{
        .rpl = USER_PRIV,
        .ti = SEL_LDT,
        .index = ldt_index,
    };
    asm volatile("mov %0, %%fs" ::"r"(new_selector));

    // Store the TCB structure and index in the created LDT area.
    ldt_page = reinterpret_cast<LdtPage*>(addr);
    if (image_address != nullptr) {
        std::memcpy(&ldt_page->tcb, image_address, sizeof(Tcb));
    } else {
        std::memset(&ldt_page->tcb, 0, sizeof(Tcb));
    }
    ldt_page->index = ldt_index;

    ASSERT_MSG(pthread_setspecific(ldt_page_slot, ldt_page) == 0,
               "Failed to store thread LDT page pointer: {}", errno);
}

Tcb* GetTcbBase() {
    Tcb* tcb;
    asm volatile("mov %%fs:0x0, %0" : "=r"(tcb));
    return tcb;
}

#elif defined(ARCH_X86_64)

// Linux x86_64
#if defined(__FreeBSD__)
void SetTcbBase(void* image_address) {
    amd64_set_gsbase(image_address);
}
#else
void SetTcbBase(void* image_address) {
    const int ret = syscall(SYS_arch_prctl, ARCH_SET_GS, (unsigned long)image_address);
    ASSERT_MSG(ret == 0, "Failed to set GS base: errno {}", errno);
}
#endif

Tcb* GetTcbBase() {
    return Libraries::Kernel::g_curthread->tcb;
}

#else

// POSIX non-x86_64
// Just sets up a simple thread-local variable to store it, then instruction translation can point
// code to it.

static pthread_key_t slot = 0;
static std::once_flag slot_alloc_flag;

static void AllocTcbKey() {
    ASSERT(pthread_key_create(&slot, nullptr) == 0);
}

pthread_key_t GetTcbKey() {
    std::call_once(slot_alloc_flag, &AllocTcbKey);
    return slot;
}

void SetTcbBase(void* image_address) {
    ASSERT(pthread_setspecific(GetTcbKey(), image_address) == 0);
}

Tcb* GetTcbBase() {
    return static_cast<Tcb*>(pthread_getspecific(GetTcbKey()));
}

#endif

thread_local std::once_flag init_tls_flag;

void InitializeTLS() {
    std::call_once(init_tls_flag, [] { SetTcbBase(Libraries::Kernel::g_curthread->tcb); });
}

} // namespace Core
