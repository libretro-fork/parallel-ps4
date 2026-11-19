// SPDX-FileCopyrightText: Copyright 2024-2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <string>
#include <vector>

#include <memmap.h>

#include "common/assert.h"
#include "common/combiner.h"
#include "common/logging/log.h"
#include "common/x64_emit.h"
#include "core/aerolib/aerolib.h"
#include "core/aerolib/stubs.h"

namespace Core::AeroLib {

// Helper to provide stub implementations for missing functions
//
// This works by constructing a minimal trampoline for each new stub which then jumps to a common
// handler with its index provided as a parameter so nid info can be found

struct StubEntry {
    const NidEntry* nid = nullptr;
    std::string nid_unknown;
    const u8* code = nullptr;
};

static std::vector<StubEntry> g_stub_entries;

// The stubs' code, 17 bytes each at most, in executable blocks made as they fill: a block of 64 KB
// holds some 3,800 (a game imports a few hundred it has nothing for). Each stub was a
// code generator of its own, with an executable allocation of its own.
static constexpr size_t StubBlockSize = 64 << 10;
static x64_code_t g_stub_code{};
static Common::Combiner g_stub_mutex;

static u64 PS4_SYSV_ABI CommonStub(u64 index) {
    const auto& e = g_stub_entries[index];
    if (e.nid) {
        LOG_ERROR(Core, "Stub: {} (nid: {}) called, returning zero to {}", e.nid->name, e.nid->nid,
                  __builtin_return_address(0));
    } else {
        LOG_ERROR(Core, "Stub: Unknown (nid: {}) called, returning zero to {}", e.nid_unknown,
                  __builtin_return_address(0));
    }
    return 0;
}

u64 GetStub(const char* nid) {
    return g_stub_mutex.Run([&]() -> u64 {
        if (g_stub_entries.empty()) {
            g_stub_entries.reserve(500);
        }

        const u64 index = g_stub_entries.size();

        StubEntry e;
        if (const auto* entry = FindByNid(nid)) {
            if (auto const& it = std::ranges::find_if(
                    g_stub_entries,
                    [entry](StubEntry const& en) { return en.nid && en.nid == entry; });
                it != g_stub_entries.end()) {
                return reinterpret_cast<u64>(it->code);
            }
            e.nid = entry;
        } else {
            e.nid_unknown = nid;
        }

        // mov rdi, index; mov rax, &CommonStub; jmp rax: the guest's code, x86-64 on any host
        constexpr size_t StubSize = 17;
        if (!g_stub_code.p || g_stub_code.cap - g_stub_code.size < StubSize) {
            void* block = mmap(nullptr, StubBlockSize, PROT_READ | PROT_WRITE | PROT_EXEC,
                               MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
            ASSERT_MSG(block != MAP_FAILED, "No executable memory for symbol stubs");
            x64_init(&g_stub_code, block, StubBlockSize);
        }
        e.code = x64_curr(&g_stub_code);
        x64_mov_ri(&g_stub_code, 64, X64_RDI, index);
        x64_mov_ri(&g_stub_code, 64, X64_RAX, reinterpret_cast<u64>(&CommonStub));
        x64_jmp_r(&g_stub_code, X64_RAX);
        ASSERT(!g_stub_code.error);

        g_stub_entries.push_back(std::move(e));
        return reinterpret_cast<u64>(g_stub_entries.back().code);
    });
}

} // namespace Core::AeroLib
