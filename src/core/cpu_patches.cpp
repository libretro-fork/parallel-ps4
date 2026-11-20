// SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <algorithm>
#include <array>
#include <atomic>
#include <bitset>
#include <climits>
#include <cstring>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <set>
#include <unordered_set>
#include <vector>
#include <encodings/rzstd.h>
#include <retro_atomic.h>
#if defined(_MSC_VER)
#include <intrin.h>
#else
#include <cpuid.h>
#endif
#include "common/alignment.h"
#include "common/arch.h"
#include "common/assert.h"
#include "common/combiner.h"
#include "common/embedded_resources.h"
#include "common/run_once.h"
#include "common/signal_context.h"
#include "common/types.h"
#include "common/x64_decode.h"
#include "common/x64_emit.h"
#include "core/signals.h"
#include "core/tls.h"
#include "cpu_patches.h"

#ifdef _WIN32
#include <windows.h>
#else
#if defined(__FreeBSD__)
#include <machine/npx.h>
#endif
#include <pthread.h>
#include <sys/ucontext.h>
#endif

namespace Core {

constexpr static u64 rcp_index_table_size = 1u << 21;
static bool rcp_index_tables_initialized = false;
// VGATHER used in our RCP packing reads 32-bit elements, as there's no 8-bit read alternative
// To ensure that the page right after rcp_index_table is readable and won't fault, we add 3 bytes
// to the array size
static std::array<u8, rcp_index_table_size + 3> rcp_index_table{};
static std::array<u8, rcp_index_table_size + 3> rsqrt_index_table{};
constexpr static std::array rcp_xor_values = {
    0x00000000, 0x00001000, 0x00000800, 0x00001800, 0x00003000, 0x00007000,
    0x00003800, 0x00007800, 0x0000f800, 0x0000f000, 0x0007f800, 0x0003f800,
    0x000ff000, 0x0001f000, 0x001ff800, 0x0001f800, 0x000ff800};
constexpr static std::array rsqrt_xor_values = {
    0x00000000, 0x00000800, 0x00001800, 0x00001000, 0x00007000, 0x00003000, 0x0000f000, 0x00003800,
    0x00007800, 0x0003f800, 0x0000f800, 0x0001f800, 0x0001f000, 0x0007f000, 0x0003f000, 0x000ff000,
    0x000ff800, 0x0007f800, 0x001ff800, 0x003ff800, 0x001ff000, 0x007ff800};

static bool InitializeIndexTables() {
    if (rcp_index_tables_initialized) {
        return true;
    }
    const auto rcp_file = Common::GetEmbeddedResource("src/resources/amd_rcp_index_table.bin.zstd");
    size_t rcp_size = 0;
    if (rzstd_decode(reinterpret_cast<u8*>(rcp_index_table.data()), rcp_index_table_size,
                     rcp_file.data(), rcp_file.size(), &rcp_size) != RZSTD_PROCESS_END ||
        rcp_size != rcp_index_table_size) {
        LOG_WARNING(Core, "Failed to decompress AMD RCP index table");
        return false;
    }
    const auto rsqrt_file =
        Common::GetEmbeddedResource("src/resources/amd_rsqrt_index_table.bin.zstd");
    size_t rsqrt_size = 0;
    if (rzstd_decode(reinterpret_cast<u8*>(rsqrt_index_table.data()), rcp_index_table_size,
                     rsqrt_file.data(), rsqrt_file.size(), &rsqrt_size) != RZSTD_PROCESS_END ||
        rsqrt_size != rcp_index_table_size) {
        LOG_WARNING(Core, "Failed to decompress AMD RSQRT index table");
        return false;
    }
    rcp_index_tables_initialized = true;
    return true;
}

/// The instruction at @code, of the @size bytes there, and its operands: false when there is none
static bool DecodeInstruction(const void* code, size_t size, x64_insn_t& instruction,
                              x64_operand_t* operands) {
    if (!x64_decode(static_cast<const u8*>(code), size, &instruction)) {
        return false;
    }
    x64_operands(&instruction, operands);
    return true;
}

/// A register of a guest instruction, as the encoder numbers it, and its width in bits (32, 64;
/// 128 for an xmm, 256 for a ymm)
struct PatchReg {
    int idx;
    int bits;
    bool IsVector() const {
        return bits >= 128;
    }
    int L() const {
        return bits == 256 ? 1 : 0;
    }
};

static PatchReg ToPatchRegister(const unsigned reg) {
    if (reg >= X64_REG_EAX && reg <= X64_REG_R15D) {
        return {static_cast<int>(reg - X64_REG_EAX), 32};
    }
    if (reg >= X64_REG_RAX && reg <= X64_REG_R15) {
        return {static_cast<int>(reg - X64_REG_RAX), 64};
    }
    if (reg >= X64_REG_XMM0 && reg <= X64_REG_XMM31) {
        return {static_cast<int>(reg - X64_REG_XMM0), 128};
    }
    if (reg >= X64_REG_YMM0 && reg <= X64_REG_YMM31) {
        return {static_cast<int>(reg - X64_REG_YMM0), 256};
    }
    UNREACHABLE_MSG("Unsupported register: %u", static_cast<u32>(reg));
}

static PatchReg ToPatchRegisterOperand(const x64_operand_t& operand) {
    ASSERT_MSG(operand.type == X64_OPERAND_REGISTER, "Expected register operand, got type: %u",
               static_cast<u32>(operand.type));

    return ToPatchRegister(operand.reg);
}

static x64_mem_t ToPatchMemoryOperand(void* address, const x64_insn_t& instruction,
                                      const x64_operand_t& operand) {
    ASSERT_MSG(operand.type == X64_OPERAND_MEMORY, "Expected memory operand, got type: %u",
               static_cast<u32>(operand.type));

    if (operand.mem.base == X64_REG_RIP || operand.mem.base == X64_REG_EIP) {
        u64 target{};
        ASSERT(
            x64_operand_address(&instruction, &operand, reinterpret_cast<u64>(address), &target));
        return x64_mrip(target);
    }

    const int base = operand.mem.base != X64_REG_NONE ? ToPatchRegister(operand.mem.base).idx : -1;
    const int index =
        operand.mem.index != X64_REG_NONE ? ToPatchRegister(operand.mem.index).idx : -1;
    const int scale = operand.mem.scale != 0 ? operand.mem.scale : 1;
    const s32 disp = operand.mem.disp_size != 0 ? static_cast<s32>(operand.mem.disp) : 0;
    return x64_mi(base, index, scale, disp);
}

/// @m through gs
static x64_mem_t Gs(x64_mem_t m) {
    m.seg = X64_SEG_GS;
    return m;
}

static bool FilterTcbAccess(const x64_operand_t* operands) {
    const auto& dst_op = operands[0];
    const auto& src_op = operands[1];

    // Patch only 'mov (64-bit register), fs:[0]'
    return src_op.type == X64_OPERAND_MEMORY && src_op.mem.segment == X64_REG_FS &&
           src_op.mem.base == X64_REG_NONE && src_op.mem.index == X64_REG_NONE &&
           src_op.mem.disp < sizeof(Core::Tcb) && dst_op.reg >= X64_REG_RAX &&
           dst_op.reg <= X64_REG_R15;
}

#if defined(_WIN32)
static void RetrieveTcbPointer(int dst, x64_code_t* c, s64 offset) {
    // The following logic is based on the Kernel32.dll asm of TlsGetValue
    static constexpr u32 TlsSlotsOffset = 0x1480;
    static constexpr u32 TlsExpansionSlotsOffset = 0x1780;
    static constexpr u32 TlsMinimumAvailable = 64;

    const auto slot = GetTcbKey();

    // Load the pointer to the table of TLS slots (through gs).
    if (slot < TlsMinimumAvailable) {
        // Load the pointer to TLS slots.
        x64_mov_rm(c, 64, dst,
                   Gs(x64_mabs(static_cast<s32>(TlsSlotsOffset + slot * sizeof(LPVOID)))));
    } else {
        const u32 tls_index = slot - TlsMinimumAvailable;

        // Load the pointer to the table of TLS expansion slots.
        x64_mov_rm(c, 64, dst, Gs(x64_mabs(TlsExpansionSlotsOffset)));
        // Load the pointer to our buffer.
        x64_mov_rm(c, 64, dst, x64_m(dst, static_cast<s32>(tls_index * sizeof(LPVOID))));
    }
    if (offset > 0) {
        // TCB starts with a pointer to self. Dereference this to get the correct data.
        x64_mov_rm(c, 64, dst, x64_m(dst, static_cast<s32>(offset)));
    }
}
#endif

static void GenerateTcbAccess(void* address, const x64_insn_t& instruction,
                              const x64_operand_t* operands, x64_code_t* c) {
    const auto dst = ToPatchRegisterOperand(operands[0]);

#if defined(_WIN32)
    RetrieveTcbPointer(dst.idx, c, operands[1].mem.disp);
#else
    const auto src = ToPatchMemoryOperand(address, instruction, operands[1]);

    // Replace fs read with gs read.
    x64_mov_rm(c, dst.bits, dst.idx, Gs(src));
#endif
}

static void GenerateTcbCompare(void* address, const x64_insn_t& instruction,
                               const x64_operand_t* operands, x64_code_t* c) {
    const auto dst = ToPatchRegisterOperand(operands[0]);

#if defined(_WIN32)
    // Prepare a scratch register. Use rax where possible, rbx if rax is in use.
    const int scratch = operands[0].reg == X64_REG_RAX ? X64_RBX : X64_RAX;

    // Set rsp to before red zone and save scratch register
    x64_lea(c, X64_RSP, x64_m(X64_RSP, -128));
    x64_pushfq(c);
    x64_push(c, scratch);

    // Retrieve value from TCB and store it in the scratch register
    RetrieveTcbPointer(scratch, c, operands[1].mem.disp);

    // Perform compare op
    x64_alu_rr(c, X64_CMP, dst.bits, dst.idx, scratch);

    // Restore registers
    x64_pop(c, scratch);
    x64_popfq(c);
    x64_lea(c, X64_RSP, x64_m(X64_RSP, 128));
#else
    const auto src = ToPatchMemoryOperand(address, instruction, operands[1]);

    // Replace fs compare with gs compare.
    x64_alu_rm(c, X64_CMP, dst.bits, dst.idx, Gs(src));
#endif
}

static void GenerateTcbExclusiveOr(void* address, const x64_insn_t& instruction,
                                   const x64_operand_t* operands, x64_code_t* c) {
    const auto dst = ToPatchRegisterOperand(operands[0]);

#if defined(_WIN32)
    // Prepare a scratch register. Use rax where possible, rbx if rax is in use.
    const int scratch = operands[0].reg == X64_REG_RAX ? X64_RBX : X64_RAX;

    // Set rsp to before red zone and save scratch register
    x64_lea(c, X64_RSP, x64_m(X64_RSP, -128));
    x64_pushfq(c);
    x64_push(c, scratch);

    // Retrieve value from TCB and store it in the scratch register
    RetrieveTcbPointer(scratch, c, operands[1].mem.disp);

    // Perform xor
    x64_alu_rr(c, X64_XOR, dst.bits, dst.idx, scratch);

    // Restore registers
    x64_pop(c, scratch);
    x64_popfq(c);
    x64_lea(c, X64_RSP, x64_m(X64_RSP, 128));
#else
    const auto src = ToPatchMemoryOperand(address, instruction, operands[1]);

    // Replace fs xor with gs xor.
    x64_alu_rm(c, X64_XOR, dst.bits, dst.idx, Gs(src));
#endif
}

/// CPUID @leaf: eax, ebx, ecx, edx
static std::array<u32, 4> Cpuid(u32 leaf) {
    std::array<u32, 4> r{};
#if defined(_MSC_VER)
    int regs[4];
    __cpuid(regs, static_cast<int>(leaf));
    std::memcpy(r.data(), regs, sizeof(regs));
#else
    __cpuid(leaf, r[0], r[1], r[2], r[3]);
#endif
    return r;
}

static bool FilterNoSSE4a(const x64_operand_t*) {
    // CPUID 0x80000001: ECX bit 6, where the extended leaves go that far
    if (Cpuid(0x80000000)[0] < 0x80000001) {
        return true;
    }
    return (Cpuid(0x80000001)[2] & (1u << 6)) == 0;
}

static bool FilterIntelCPU(const x64_operand_t*) {
#if defined(__APPLE__)
    // Rosetta seems to emulate the reciprocal instructions with exact Intel results
    // so we enable our reciprocal translation there too
    return true;
#endif

    // "GenuineIntel" in ebx, edx, ecx
    const auto id = Cpuid(0);
    return id[1] == 0x756e6547 && id[3] == 0x49656e69 && id[2] == 0x6c65746e;
}

static void GenerateReciprocalInstruction(void* address, const x64_insn_t& instruction,
                                          const x64_operand_t* operands, x64_code_t* c, bool rsqrt,
                                          bool vex) {
    bool table_loaded = InitializeIndexTables();
    ASSERT_MSG(table_loaded, "Failed to load RCP index table");

    ASSERT_MSG(operands[0].type == X64_OPERAND_REGISTER, "operand 0 must be a register");

    const auto dst = ToPatchRegisterOperand(operands[0]);

    ASSERT_MSG(dst.IsVector(), "operand 0 must be an XMM or YMM register");
    bool is_src_mem = operands[1].type == X64_OPERAND_MEMORY;

    // The registers' width is the destination's: xmm or ymm
    const int l = dst.L();
    std::array<bool, 16> taken_vecs = {};
    int dst_reg;
    int src_reg = 0;

    if (!is_src_mem) {
        src_reg = ToPatchRegisterOperand(operands[1]).idx;
        taken_vecs[src_reg] = true;
    }

    dst_reg = dst.idx;
    taken_vecs[dst_reg] = true;

    const int scratch1 = X64_RAX;
    const int scratch2 = X64_RCX;
    auto it1 = std::find(taken_vecs.begin(), taken_vecs.end(), false);
    auto it2 = std::find(std::next(it1), taken_vecs.end(), false);
    auto it3 = std::find(std::next(it2), taken_vecs.end(), false);
    auto it4 = std::find(std::next(it3), taken_vecs.end(), false);
    auto it5 = std::find(std::next(it4), taken_vecs.end(), false);
    const int xmm_scratch1 = static_cast<int>(it1 - taken_vecs.begin());
    const int xmm_scratch2 = static_cast<int>(it2 - taken_vecs.begin());
    const int xmm_scratch3 = static_cast<int>(it3 - taken_vecs.begin());
    const int src_storage = static_cast<int>(it4 - taken_vecs.begin());
    const int nan_mask = static_cast<int>(it5 - taken_vecs.begin());
    const int ymm_storage = is_src_mem ? 32 * 5 : 32 * 4;

    // Set rsp to before red zone and save scratch registers (whole ymm registers)
    const int rsp_disp = 128 + ymm_storage;
    x64_lea(c, X64_RSP, x64_m(X64_RSP, -rsp_disp));
    x64_vmovups_mr(c, 1, x64_m(X64_RSP, 0), xmm_scratch1);
    x64_vmovups_mr(c, 1, x64_m(X64_RSP, 32), xmm_scratch2);
    x64_vmovups_mr(c, 1, x64_m(X64_RSP, 64), xmm_scratch3);
    x64_vmovups_mr(c, 1, x64_m(X64_RSP, 96), nan_mask);
    if (is_src_mem) {
        x64_vmovups_mr(c, 1, x64_m(X64_RSP, 128), src_storage);
    }
    x64_pushfq(c);
    x64_push(c, scratch1);
    x64_push(c, scratch2);

    if (is_src_mem) {
        if (operands[1].mem.base == X64_REG_RIP) {
            const u64 target = (u64)address + instruction.len + operands[1].mem.disp;
            x64_mov_ri(c, 64, X64_RAX, target);
            x64_vmovups_rm(c, l, src_storage, x64_m(X64_RAX, 0));
        } else {
            x64_operand_t operand = operands[1];
            if (operands[1].mem.base == X64_REG_RSP) { // rsp can't be index
                operand.mem.disp_size = 4;
                operand.mem.disp += rsp_disp + 8 * 3; // Account for what we pushed
            }
            x64_vmovups_rm(c, l, src_storage, ToPatchMemoryOperand(address, instruction, operand));
        }
        src_reg = src_storage;
    }
    void* index_table = rsqrt ? rsqrt_index_table.data() : rcp_index_table.data();
    const int* xor_table = rsqrt ? rsqrt_xor_values.data() : rcp_xor_values.data();
    x64_mov_ri(c, 64, scratch1, reinterpret_cast<u64>(index_table));
    x64_mov_ri(c, 64, scratch2, reinterpret_cast<u64>(xor_table));
    // Find NaNs in source
    // In non-VEX forms this will zero the top elements which will make the vblendvps
    // pick from dst_reg, thus preserving the top bits
    x64_vcmpps(c, l, nan_mask, src_reg, src_reg, 3); // unord
    // Set mask to all ones for the elements we'll load
    x64_vpcmpeqd(c, l, xmm_scratch2, xmm_scratch2, xmm_scratch2);
    // Load indices for active elements from table
    x64_vpsrld_ri(c, l, xmm_scratch3, src_reg, 11);
    x64_vgatherdps(c, l, xmm_scratch1, x64_mi(scratch1, xmm_scratch3, 1, 0), xmm_scratch2);
    x64_vpslld_ri(c, l, xmm_scratch1, xmm_scratch1, 24);
    x64_vpsrld_ri(c, l, xmm_scratch1, xmm_scratch1, 24);
    // Load XOR values using those indices
    x64_vpcmpeqd(c, l, xmm_scratch2, xmm_scratch2, xmm_scratch2); // vgather sets to zero
    x64_vgatherdps(c, l, xmm_scratch3, x64_mi(scratch2, xmm_scratch1, 4, 0), xmm_scratch2);
    if (dst_reg == src_reg) {
        // The RCP would modify our source reg so we wouldn't be able to use it for NaN merging
        x64_vmovaps_rr(c, l, xmm_scratch1, src_reg);
        src_reg = xmm_scratch1;
    }
    if (vex) {
        if (rsqrt) {
            x64_vrsqrtps(c, l, dst_reg, src_reg);
        } else {
            x64_vrcpps(c, l, dst_reg, src_reg);
        }
        x64_vxorps_rrr(c, l, dst_reg, dst_reg, xmm_scratch3);
    } else {
        // Preserve top bits
        if (rsqrt) {
            x64_rsqrtps(c, dst_reg, src_reg);
        } else {
            x64_rcpps(c, dst_reg, src_reg);
        }
        x64_xorps(c, dst_reg, xmm_scratch3);
    }
    // Merge NaNs back into dst
    x64_vblendvps(c, 1, dst_reg, dst_reg, src_reg, nan_mask);

    x64_pop(c, scratch2);
    x64_pop(c, scratch1);
    x64_popfq(c);
    if (is_src_mem) {
        x64_vmovups_rm(c, 1, src_storage, x64_m(X64_RSP, 128));
    }
    x64_vmovups_rm(c, 1, nan_mask, x64_m(X64_RSP, 96));
    x64_vmovups_rm(c, 1, xmm_scratch3, x64_m(X64_RSP, 64));
    x64_vmovups_rm(c, 1, xmm_scratch2, x64_m(X64_RSP, 32));
    x64_vmovups_rm(c, 1, xmm_scratch1, x64_m(X64_RSP, 0));
    x64_lea(c, X64_RSP, x64_m(X64_RSP, rsp_disp));
}

static void GenerateRSQRTPS(void* address, const x64_insn_t& instruction,
                            const x64_operand_t* operands, x64_code_t* c) {
    GenerateReciprocalInstruction(address, instruction, operands, c, true, false);
}

static void GenerateVRSQRTPS(void* address, const x64_insn_t& instruction,
                             const x64_operand_t* operands, x64_code_t* c) {
    GenerateReciprocalInstruction(address, instruction, operands, c, true, true);
}

static void GenerateRCPPS(void* address, const x64_insn_t& instruction,
                          const x64_operand_t* operands, x64_code_t* c) {
    GenerateReciprocalInstruction(address, instruction, operands, c, false, false);
}

static void GenerateVRCPPS(void* address, const x64_insn_t& instruction,
                           const x64_operand_t* operands, x64_code_t* c) {
    GenerateReciprocalInstruction(address, instruction, operands, c, false, true);
}

static void GenerateEXTRQ(void* /* address */, const x64_insn_t&, const x64_operand_t* operands,
                          x64_code_t* c) {
    bool immediateForm =
        operands[1].type == X64_OPERAND_IMMEDIATE && operands[2].type == X64_OPERAND_IMMEDIATE;

    ASSERT_MSG(operands[0].type == X64_OPERAND_REGISTER, "operand 0 must be a register");

    const auto dst = ToPatchRegisterOperand(operands[0]);

    ASSERT_MSG(dst.bits == 128, "operand 0 must be an XMM register");

    const int xmm_dst = dst.idx;

    if (immediateForm) {
        u8 length = operands[1].imm.value & 0x3F;
        u8 index = operands[2].imm.value & 0x3F;

        LOG_DEBUG(Core, "Patching immediate form EXTRQ, length: %u, index: %u", length, index);

        const int scratch1 = X64_RAX;
        const int scratch2 = X64_RCX;

        // Set rsp to before red zone and save scratch registers
        x64_lea(c, X64_RSP, x64_m(X64_RSP, -128));
        x64_pushfq(c);
        x64_push(c, scratch1);
        x64_push(c, scratch2);

        u64 mask;
        if (length == 0) {
            length = 64; // for the check below
            mask = 0xFFFF'FFFF'FFFF'FFFF;
        } else {
            mask = (1ULL << length) - 1;
        }

        if (length + index > 64) {
            mask = 0xFFFF'FFFF'FFFF'FFFF;
        }

        // Get lower qword from xmm register
        x64_vmovq_rx(c, scratch1, xmm_dst);

        if (index != 0) {
            x64_shift_ri(c, X64_SHR, 64, scratch1, index);
        }

        // We need to move mask to a register because we can't use all the possible
        // immediate values with `and reg, imm32`
        x64_mov_ri(c, 64, scratch2, mask);
        x64_alu_rr(c, X64_AND, 64, scratch1, scratch2);

        // Writeback to xmm register, extrq instruction says top 64-bits are undefined but zeroed on
        // AMD CPUs
        x64_vmovq_xr(c, xmm_dst, scratch1);

        x64_pop(c, scratch2);
        x64_pop(c, scratch1);
        x64_popfq(c);
        x64_lea(c, X64_RSP, x64_m(X64_RSP, 128));
    } else {
        ASSERT_MSG(operands[0].type == X64_OPERAND_REGISTER &&
                       operands[1].type == X64_OPERAND_REGISTER &&
                       operands[0].reg >= X64_REG_XMM0 && operands[0].reg <= X64_REG_XMM15 &&
                       operands[1].reg >= X64_REG_XMM0 && operands[1].reg <= X64_REG_XMM15,
                   "Unexpected operand types for EXTRQ instruction");

        const auto src = ToPatchRegisterOperand(operands[1]);

        ASSERT_MSG(src.bits == 128, "operand 1 must be an XMM register");

        const int xmm_src = src.idx;

        const int scratch1 = X64_RAX;
        const int scratch2 = X64_RCX;
        const int mask = X64_RDX;

        size_t length_zero, end;

        x64_lea(c, X64_RSP, x64_m(X64_RSP, -128));
        x64_pushfq(c);
        x64_push(c, scratch1);
        x64_push(c, scratch2);
        x64_push(c, mask);

        // Construct the mask out of the length that resides in bottom 6 bits of source xmm
        x64_vmovq_rx(c, scratch1, xmm_src);
        x64_mov_rr(c, 64, scratch2, scratch1);
        x64_alu_ri(c, X64_AND, 64, scratch2, 0x3F);
        length_zero = x64_jcc_fwd(c, X64_CC_Z);

        // mask = (1ULL << length) - 1
        x64_mov_ri(c, 64, mask, 1);
        x64_shift_rcl(c, X64_SHL, 64, mask);
        x64_dec(c, 64, mask);
        end = x64_jmp_fwd(c);

        x64_bind(c, length_zero);
        x64_mov_ri(c, 64, mask, 0xFFFF'FFFF'FFFF'FFFF);

        x64_bind(c, end);

        // Get the shift amount and store it in scratch2
        x64_shift_ri(c, X64_SHR, 64, scratch1, 8);
        x64_alu_ri(c, X64_AND, 64, scratch1, 0x3F);
        x64_mov_rr(c, 64, scratch2, scratch1); // cl now contains the shift amount

        x64_vmovq_rx(c, scratch1, xmm_dst);
        x64_shift_rcl(c, X64_SHR, 64, scratch1);
        x64_alu_rr(c, X64_AND, 64, scratch1, mask);
        x64_vmovq_xr(c, xmm_dst, scratch1);

        x64_pop(c, mask);
        x64_pop(c, scratch2);
        x64_pop(c, scratch1);
        x64_popfq(c);
        x64_lea(c, X64_RSP, x64_m(X64_RSP, 128));
    }
}

static void GenerateINSERTQ(void* /* address */, const x64_insn_t&, const x64_operand_t* operands,
                            x64_code_t* c) {
    bool immediateForm =
        operands[2].type == X64_OPERAND_IMMEDIATE && operands[3].type == X64_OPERAND_IMMEDIATE;

    ASSERT_MSG(operands[0].type == X64_OPERAND_REGISTER && operands[1].type == X64_OPERAND_REGISTER,
               "operands 0 and 1 must be registers.");

    const auto dst = ToPatchRegisterOperand(operands[0]);
    const auto src = ToPatchRegisterOperand(operands[1]);

    ASSERT_MSG(dst.bits == 128 && src.bits == 128, "operands 0 and 1 must be xmm registers.");

    const int xmm_dst = dst.idx;
    const int xmm_src = src.idx;

    if (immediateForm) {
        u8 length = operands[2].imm.value & 0x3F;
        u8 index = operands[3].imm.value & 0x3F;

        const int scratch1 = X64_RAX;
        const int scratch2 = X64_RCX;
        const int mask = X64_RDX;

        // Set rsp to before red zone and save scratch registers
        x64_lea(c, X64_RSP, x64_m(X64_RSP, -128));
        x64_pushfq(c);
        x64_push(c, scratch1);
        x64_push(c, scratch2);
        x64_push(c, mask);

        u64 mask_value;
        if (length == 0) {
            length = 64; // for the check below
            mask_value = 0xFFFF'FFFF'FFFF'FFFF;
        } else {
            mask_value = (1ULL << length) - 1;
        }

        if (length + index > 64) {
            mask_value = 0xFFFF'FFFF'FFFF'FFFF;
        }

        x64_vmovq_rx(c, scratch1, xmm_src);
        x64_vmovq_rx(c, scratch2, xmm_dst);
        x64_mov_ri(c, 64, mask, mask_value);

        // src &= mask
        x64_alu_rr(c, X64_AND, 64, scratch1, mask);

        // src <<= index
        x64_shift_ri(c, X64_SHL, 64, scratch1, index);

        // dst &= ~(mask << index)
        mask_value = ~(mask_value << index);
        x64_mov_ri(c, 64, mask, mask_value);
        x64_alu_rr(c, X64_AND, 64, scratch2, mask);

        // dst |= src
        x64_alu_rr(c, X64_OR, 64, scratch2, scratch1);

        // Insert scratch2 into low 64 bits of dst, upper 64 bits are undefined but zeroed on AMD
        // CPUs
        x64_vmovq_xr(c, xmm_dst, scratch2);

        x64_pop(c, mask);
        x64_pop(c, scratch2);
        x64_pop(c, scratch1);
        x64_popfq(c);
        x64_lea(c, X64_RSP, x64_m(X64_RSP, 128));
    } else {
        ASSERT_MSG(operands[2].type == X64_OPERAND_UNUSED && operands[3].type == X64_OPERAND_UNUSED,
                   "operands 2 and 3 must be unused for register form.");

        const int scratch1 = X64_RAX;
        const int scratch2 = X64_RCX;
        const int index = X64_RDX;
        const int mask = X64_RBX;

        size_t length_zero, end;

        x64_lea(c, X64_RSP, x64_m(X64_RSP, -128));
        x64_pushfq(c);
        x64_push(c, scratch1);
        x64_push(c, scratch2);
        x64_push(c, index);
        x64_push(c, mask);

        // Get upper 64 bits of src and copy it to mask and index
        x64_vpextrq(c, index, xmm_src, 1);
        x64_mov_rr(c, 64, mask, index);

        // When length is 0, set it to 64
        x64_alu_ri(c, X64_AND, 64, mask, 0x3F); // mask now holds the length
        length_zero =
            x64_jcc_fwd(c, X64_CC_Z); // Check if length is 0 and set mask to all 1s if it is

        // Create a mask out of the length
        x64_mov_rr(c, 8, X64_RCX, mask);
        x64_mov_ri(c, 64, mask, 1);
        x64_shift_rcl(c, X64_SHL, 64, mask);
        x64_dec(c, 64, mask);
        end = x64_jmp_fwd(c);

        x64_bind(c, length_zero);
        x64_mov_ri(c, 64, mask, 0xFFFF'FFFF'FFFF'FFFF);

        x64_bind(c, end);
        // Get index to insert at
        x64_shift_ri(c, X64_SHR, 64, index, 8);
        x64_alu_ri(c, X64_AND, 64, index, 0x3F);

        // src &= mask
        x64_vmovq_rx(c, scratch1, xmm_src);
        x64_alu_rr(c, X64_AND, 64, scratch1, mask);

        // mask = ~(mask << index)
        x64_mov_rr(c, 8, X64_RCX, index);
        x64_shift_rcl(c, X64_SHL, 64, mask);
        x64_not(c, 64, mask);

        // src <<= index
        x64_shift_rcl(c, X64_SHL, 64, scratch1);

        // dst = (dst & mask) | src
        x64_vmovq_rx(c, scratch2, xmm_dst);
        x64_alu_rr(c, X64_AND, 64, scratch2, mask);
        x64_alu_rr(c, X64_OR, 64, scratch2, scratch1);

        // Upper 64 bits are undefined in insertq but AMD CPUs zero them
        x64_vmovq_xr(c, xmm_dst, scratch2);

        x64_pop(c, mask);
        x64_pop(c, index);
        x64_pop(c, scratch2);
        x64_pop(c, scratch1);
        x64_popfq(c);
        x64_lea(c, X64_RSP, x64_m(X64_RSP, 128));
    }
}

static void ReplaceMOVNT(void* address, u8 rep_prefix) {
    // Find the opcode byte
    // There can be any amount of prefixes but the instruction can't be more than 15 bytes
    // And we know for sure this is a MOVNTSS/MOVNTSD
    bool found = false;
    bool rep_prefix_found = false;
    int index = 0;
    u8* ptr = reinterpret_cast<u8*>(address);
    for (int i = 0; i < 15; i++) {
        if (ptr[i] == rep_prefix) {
            rep_prefix_found = true;
        } else if (ptr[i] == 0x2B) {
            index = i;
            found = true;
            break;
        }
    }

    // Some sanity checks
    ASSERT(found);
    ASSERT(index >= 2);
    ASSERT(ptr[index - 1] == 0x0F);
    ASSERT(rep_prefix_found);

    // This turns the MOVNTSS/MOVNTSD to a MOVSS/MOVSD m, xmm
    ptr[index] = 0x11;
}

static void ReplaceMOVNTSS(void* address, const x64_insn_t&, const x64_operand_t*, x64_code_t*) {
    ReplaceMOVNT(address, 0xF3);
}

static void ReplaceMOVNTSD(void* address, const x64_insn_t&, const x64_operand_t*, x64_code_t*) {
    ReplaceMOVNT(address, 0xF2);
}

#if defined(__APPLE__)
// Rosetta 2 mistranslates VCMPSS with NEQ, NLT or NLE: if the destination is then reloaded by a
// full width VEX load, later scalar reads of it still return the compare mask.
static bool FilterNegatedScalarCompare(const x64_operand_t* operands) {
    // NEQ, NLT and NLE are 4 to 6, and 20 to 22 are the same predicates with bit 4 set.
    const u64 predicate = operands[3].imm.value & ~0x10ULL;
    return predicate >= 4 && predicate <= 6;
}

// Compares with the non-negated predicate and flips lane 0, which gives the same bits.
static void GenerateNegatedScalarCompare(void* address, const x64_insn_t& instruction,
                                         const x64_operand_t* operands, x64_code_t* c) {
    const int xmm_dst = ToPatchRegisterOperand(operands[0]).idx;
    const int xmm_src1 = ToPatchRegisterOperand(operands[1]).idx;
    const u8 predicate = operands[3].imm.value & 3;
    if (operands[2].type == X64_OPERAND_REGISTER) {
        x64_vcmpss_rrr(c, xmm_dst, xmm_src1, ToPatchRegisterOperand(operands[2]).idx, predicate);
    } else {
        x64_vcmpss_rrm(c, xmm_dst, xmm_src1,
                       ToPatchMemoryOperand(address, instruction, operands[2]), predicate);
    }
    // The lane 0 mask lies after the vxorps and the 2-byte jump over the mask. A rip-relative
    // vxorps of xmm registers always takes the 2-byte VEX: 8 bytes in all
    const u8* vx = x64_curr(c);
    x64_vxorps_rrm(c, 0, xmm_dst, xmm_dst, x64_mrip(reinterpret_cast<u64>(vx + 8 + 2)));
    ASSERT(c->error || x64_curr(c) == vx + 8);
    const size_t done = x64_jmp_fwd(c);
    x64_dq(c, 0xFFFFFFFF);
    x64_dq(c, 0);
    x64_bind(c, done);
}
#endif

using PatchFilter = bool (*)(const x64_operand_t*);
using InstructionGenerator = void (*)(void*, const x64_insn_t&, const x64_operand_t*, x64_code_t*);
struct PatchInfo {
    /// Filter for more granular patch conditions past just the instruction mnemonic.
    PatchFilter filter;

    /// Generator for the patch/trampoline.
    InstructionGenerator generator;

    /// Whether to use a trampoline for this patch.
    bool trampoline;
};

constexpr size_t NearJumpSize = 5;

#if defined(_WIN32)
static const bool need_tcb_trampoline = true;
#else
static const bool need_tcb_trampoline = false;
#endif

static const std::unordered_map<u16, std::vector<PatchInfo>> Patches = {
    // SSE4a
    {X64_MN_EXTRQ, {{FilterNoSSE4a, GenerateEXTRQ, true}}},
    {X64_MN_INSERTQ, {{FilterNoSSE4a, GenerateINSERTQ, true}}},
    {X64_MN_MOVNTSS, {{FilterNoSSE4a, ReplaceMOVNTSS, false}}},
    {X64_MN_MOVNTSD, {{FilterNoSSE4a, ReplaceMOVNTSD, false}}},
    {X64_MN_RSQRTPS, {{FilterIntelCPU, GenerateRSQRTPS, true}}},
    {X64_MN_VRSQRTPS, {{FilterIntelCPU, GenerateVRSQRTPS, true}}},
    {X64_MN_RCPPS, {{FilterIntelCPU, GenerateRCPPS, true}}},
    {X64_MN_VRCPPS, {{FilterIntelCPU, GenerateVRCPPS, true}}},

#if defined(__APPLE__)
    // Rosetta 2
    {X64_MN_VCMPSS, {{FilterNegatedScalarCompare, GenerateNegatedScalarCompare, true}}},
#else
    // FS segment patches
    // For most of these, Windows needs a trampoline while other platforms do not.
    {X64_MN_XOR, {{FilterTcbAccess, GenerateTcbExclusiveOr, need_tcb_trampoline}}},
    {X64_MN_MOV, {{FilterTcbAccess, GenerateTcbAccess, need_tcb_trampoline}}},
    {X64_MN_CMP, {{FilterTcbAccess, GenerateTcbCompare, need_tcb_trampoline}}}
#endif
};

static run_once_t init_flag = RUN_ONCE_INIT;

struct PatchModule {
    /// Mutex controlling access to module code regions.
    Common::Combiner mutex{};

    /// Start of the module.
    u8* start;

    /// End of the module.
    u8* end;

    /// Tracker for patched code locations.
    std::set<u8*> patched;

    /// Code written over the module's own, where it is.
    x64_code_t patch_gen{};

    /// Code written into the module's trampoline area.
    x64_code_t trampoline_gen{};

    /// Prevents repeated generation attempts after the fixed trampoline area is exhausted.
    bool trampoline_exhausted{};

    PatchModule(u8* module_ptr, const u64 module_size, u8* trampoline_ptr,
                const u64 trampoline_size)
        : start(module_ptr), end(module_ptr + module_size) {
        x64_init(&patch_gen, module_ptr, module_size);
        x64_init(&trampoline_gen, trampoline_ptr, trampoline_size);
    }
};
static std::map<u64, PatchModule> modules;

/// A trampoline that did not fit, its code dropped from @at on: the area is full
static void TrampolineFull(PatchModule* module, size_t at) {
    module->trampoline_gen.size = at;
    module->trampoline_gen.error = 0;
    if (!module->trampoline_exhausted) {
        LOG_WARNING(Core, "Patch trampoline space exhausted for module at 0x%llx",
                    static_cast<unsigned long long>(reinterpret_cast<uintptr_t>(module->start)));
        module->trampoline_exhausted = true;
    }
}

static PatchModule* GetModule(const void* ptr) {
    auto upper_bound = modules.upper_bound(reinterpret_cast<u64>(ptr));
    if (upper_bound == modules.begin()) {
        return nullptr;
    }
    return &(std::prev(upper_bound)->second);
}

// Windows static guest red-zone protection
static PatchModule* GetContainingModule(const void* ptr) {
    auto* module = GetModule(ptr);
    const auto* address = static_cast<const u8*>(ptr);
    return module != nullptr && address < module->end ? module : nullptr;
}

/// Returns a boolean indicating whether the instruction was patched, and the offset to advance past
/// whatever is at the current code pointer.
static std::pair<bool, u64> TryPatch(u8* code, PatchModule* module, bool allow_trampoline = true) {
    x64_insn_t instruction;
    x64_operand_t operands[X64_MAX_OPERANDS];
    if (!DecodeInstruction(code, module->end - code, instruction, operands)) {
        return std::make_pair(false, 1);
    }

    if (Patches.contains(instruction.mnemonic)) {
        const auto& patches = Patches.at(instruction.mnemonic);
        for (const auto& patch_info : patches) {
            bool needs_trampoline = patch_info.trampoline;
            if (patch_info.filter(operands)) {
                auto& patch_gen = module->patch_gen;

                if (needs_trampoline && instruction.len < NearJumpSize) {
                    // Trampoline is needed but instruction is too short to patch.
                    // Return false and length to signal to AOT compilation that this instruction
                    // should be skipped and handled at runtime.
                    return std::make_pair(false, instruction.len);
                }

                if (needs_trampoline && (!allow_trampoline || module->trampoline_exhausted)) {
                    return std::make_pair(false, instruction.len);
                }

                // Move to the current code position.
                patch_gen.error = 0;
                patch_gen.size = code - patch_gen.p;

                if (needs_trampoline) {
                    auto& trampoline_gen = module->trampoline_gen;
                    const size_t trampoline_offset = trampoline_gen.size;
                    const auto trampoline_ptr = x64_curr(&trampoline_gen);
                    patch_info.generator(code, instruction, operands, &trampoline_gen);

                    // Return to the following instruction at the end of the trampoline.
                    x64_jmp(&trampoline_gen, reinterpret_cast<u64>(code + instruction.len),
                            X64_JMP_AUTO);
                    if (trampoline_gen.error) {
                        TrampolineFull(module, trampoline_offset);
                        return std::make_pair(false, instruction.len);
                    }

                    // Replace instruction with near jump to the trampoline.
                    x64_jmp(&patch_gen, reinterpret_cast<u64>(trampoline_ptr), X64_JMP_NEAR);
                } else {
                    patch_info.generator(code, instruction, operands, &patch_gen);
                }
                ASSERT_MSG(!patch_gen.error, "Patch past the end of the module at 0x%llx",
                           static_cast<unsigned long long>(reinterpret_cast<uintptr_t>(code)));

                const auto patch_size = x64_curr(&patch_gen) - code;
                if (patch_size > 0) {
                    ASSERT_MSG(instruction.len >= patch_size,
                               "Instruction %s with length %u is too short to replace at: 0x%llx",
                               x64_mnemonic_name(instruction.mnemonic), instruction.len,
                               static_cast<unsigned long long>(reinterpret_cast<uintptr_t>(code)));

                    // Fill remaining space with nops.
                    x64_nop(&patch_gen, instruction.len - patch_size);

                    module->patched.insert(code);
                    LOG_DEBUG(Core, "Patched instruction '%s' at: 0x%llx",
                              x64_mnemonic_name(instruction.mnemonic),
                              static_cast<unsigned long long>(reinterpret_cast<uintptr_t>(code)));
                    return std::make_pair(true, instruction.len);
                }
            }
        }
    }

    return std::make_pair(false, instruction.len);
}

static bool Is4ByteExtrqOrInsertq(void* code_address) {
    u8* bytes = (u8*)code_address;
    if (bytes[0] == 0x66 && bytes[1] == 0x0F && bytes[2] == 0x79) {
        return true; // extrq
    } else if (bytes[0] == 0xF2 && bytes[1] == 0x0F && bytes[2] == 0x79) {
        return true; // insertq
    } else {
        return false;
    }
}

static void* GetXmmPointer(void* ctx, u8 index) {
#if defined(_WIN32)
#define CASE(index)                                                                                \
    case index:                                                                                    \
        return (void*)(&((EXCEPTION_POINTERS*)ctx)->ContextRecord->Xmm##index.Low)
#elif defined(__APPLE__)
#define CASE(index)                                                                                \
    case index:                                                                                    \
        return (void*)(&((ucontext_t*)ctx)->uc_mcontext->__fs.__fpu_xmm##index);
#elif defined(__FreeBSD__)
    // In mc_fpstate
    // See <machine/npx.h> for the internals of mc_fpstate[].
#define CASE(index)                                                                                \
    case index: {                                                                                  \
        auto& mctx = ((ucontext_t*)ctx)->uc_mcontext;                                              \
        ASSERT(mctx.mc_fpformat == _MC_FPFMT_XMM);                                                 \
        auto* s_fpu = (struct savefpu*)(&mctx.mc_fpstate[0]);                                      \
        return (void*)(&(s_fpu->sv_xmm[0]));                                                       \
    }
#else
#define CASE(index)                                                                                \
    case index:                                                                                    \
        return (void*)(&((ucontext_t*)ctx)->uc_mcontext.fpregs->_xmm[index].element[0])
#endif
    switch (index) {
        CASE(0);
        CASE(1);
        CASE(2);
        CASE(3);
        CASE(4);
        CASE(5);
        CASE(6);
        CASE(7);
        CASE(8);
        CASE(9);
        CASE(10);
        CASE(11);
        CASE(12);
        CASE(13);
        CASE(14);
        CASE(15);
    default: {
        UNREACHABLE_MSG("Invalid XMM register index: %u", index);
        return nullptr;
    }
    }
#undef CASE
}

static void IncrementRip(void* ctx, u64 length) {
#if defined(_WIN32)
    ((EXCEPTION_POINTERS*)ctx)->ContextRecord->Rip += length;
#elif defined(__APPLE__)
    ((ucontext_t*)ctx)->uc_mcontext->__ss.__rip += length;
#elif defined(__FreeBSD__)
    ((ucontext_t*)ctx)->uc_mcontext.mc_rip += length;
#else
    ((ucontext_t*)ctx)->uc_mcontext.gregs[REG_RIP] += length;
#endif
}

static bool TryExecuteIllegalInstruction(void* ctx, void* code_address) {
    // We need to decode the instruction to find out what it is. It is definitely an extrq or an
    // insertq, so a mini decoder here tells them apart faster than the full one does.
    u16 mnemonic;
    u8* bytes = (u8*)code_address;
    if (bytes[0] == 0x66) {
        mnemonic = X64_MN_EXTRQ;
    } else if (bytes[0] == 0xF2) {
        mnemonic = X64_MN_INSERTQ;
    } else {
        x64_insn_t instruction;
        x64_operand_t operands[X64_MAX_OPERANDS];
        const bool decoded = DecodeInstruction(code_address, 15, instruction, operands);
        LOG_ERROR(Core, "Unhandled illegal instruction at code address 0x%llx: %s",
                  static_cast<unsigned long long>(reinterpret_cast<uintptr_t>(code_address)),
                  decoded ? x64_mnemonic_name(instruction.mnemonic) : "Failed to decode");
        return false;
    }

    ASSERT(bytes[1] == 0x0F && bytes[2] == 0x79);

    // Note: It's guaranteed that there's no REX prefix in these instructions checked by
    // Is4ByteExtrqOrInsertq
    u8 modrm = bytes[3];
    u8 rm = modrm & 0b111;
    u8 reg = (modrm >> 3) & 0b111;
    u8 mod = (modrm >> 6) & 0b11;

    ASSERT(mod == 0b11); // Any instruction we interpret here uses reg/reg addressing only

    int dstIndex = reg;
    int srcIndex = rm;

    switch (mnemonic) {
    case X64_MN_EXTRQ: {
        const auto dst = GetXmmPointer(ctx, dstIndex);
        const auto src = GetXmmPointer(ctx, srcIndex);

        u64 lowQWordSrc;
        memcpy(&lowQWordSrc, src, sizeof(lowQWordSrc));

        u64 lowQWordDst;
        memcpy(&lowQWordDst, dst, sizeof(lowQWordDst));

        u64 length = lowQWordSrc & 0x3F;
        u64 mask;
        if (length == 0) {
            length = 64; // for the check below
            mask = 0xFFFF'FFFF'FFFF'FFFF;
        } else {
            mask = (1ULL << length) - 1;
        }

        u64 index = (lowQWordSrc >> 8) & 0x3F;
        if (length + index > 64) {
            // Undefined behavior if length + index is bigger than 64 according to the spec,
            // we'll warn and continue execution.
            LOG_TRACE(Core,
                      "extrq at 0x%llx with length %" PRIu64 " and index %" PRIu64
                      " is bigger than 64, "
                      "undefined behavior",
                      static_cast<unsigned long long>(reinterpret_cast<uintptr_t>(code_address)),
                      length, index);
        }

        lowQWordDst >>= index;
        lowQWordDst &= mask;

        memset((u8*)dst + sizeof(u64), 0, sizeof(u64));
        memcpy(dst, &lowQWordDst, sizeof(lowQWordDst));

        IncrementRip(ctx, 4);

        return true;
    }
    case X64_MN_INSERTQ: {
        const auto dst = GetXmmPointer(ctx, dstIndex);
        const auto src = GetXmmPointer(ctx, srcIndex);

        u64 lowQWordSrc, highQWordSrc;
        memcpy(&lowQWordSrc, src, sizeof(lowQWordSrc));
        memcpy(&highQWordSrc, (u8*)src + 8, sizeof(highQWordSrc));

        u64 lowQWordDst;
        memcpy(&lowQWordDst, dst, sizeof(lowQWordDst));

        u64 length = highQWordSrc & 0x3F;
        u64 mask;
        if (length == 0) {
            length = 64; // for the check below
            mask = 0xFFFF'FFFF'FFFF'FFFF;
        } else {
            mask = (1ULL << length) - 1;
        }

        u64 index = (highQWordSrc >> 8) & 0x3F;
        if (length + index > 64) {
            // Undefined behavior if length + index is bigger than 64 according to the spec,
            // we'll warn and continue execution.
            LOG_TRACE(Core,
                      "insertq at 0x%llx with length %" PRIu64 " and index %" PRIu64
                      " is bigger than 64, "
                      "undefined behavior",
                      static_cast<unsigned long long>(reinterpret_cast<uintptr_t>(code_address)),
                      length, index);
        }

        lowQWordSrc &= mask;
        lowQWordDst &= ~(mask << index);
        lowQWordDst |= lowQWordSrc << index;

        memset((u8*)dst + sizeof(u64), 0, sizeof(u64));
        memcpy(dst, &lowQWordDst, sizeof(lowQWordDst));

        IncrementRip(ctx, 4);

        return true;
    }
    default: {
        UNREACHABLE();
    }
    }

    UNREACHABLE();
}

static bool TryPatchJit(void* code_address) {
    auto* code = static_cast<u8*>(code_address);
    auto* module = GetContainingModule(code);
    if (module == nullptr) {
        return false;
    }

    return module->mutex.Run([&]() -> bool {
        // Return early if already patched, in case multiple threads signaled at the same time.
        if (std::ranges::find(module->patched, code) != module->patched.end()) {
            return true;
        }

        return TryPatch(code, module).first;
    });
}

static void TryPatchAot(void* code_address, u64 code_size) {
    auto* code = static_cast<u8*>(code_address);
    auto* module = GetModule(code);
    if (module == nullptr) {
        return;
    }

    module->mutex.Run([&] {
        const auto* end = code + code_size;
        while (code < end) {
            code += TryPatch(code, module, false).second;
        }
    });
}

// ============================================================================
// Windows static guest red-zone protection
// ============================================================================

namespace WindowsGuestRedZoneProtection {
namespace {

retro_atomic_int_t active_mode{static_cast<int>(WindowsGuestRedZoneProtectionMode::Disabled)};

} // namespace

void SetActiveMode(WindowsGuestRedZoneProtectionMode mode) noexcept {
    retro_atomic_store_release_int(&active_mode, mode);
}

WindowsGuestRedZoneProtectionMode GetActiveMode() noexcept {
    return static_cast<WindowsGuestRedZoneProtectionMode>(
        retro_atomic_load_acquire_int(&active_mode));
}

bool IsStaticPatchingEnabled() noexcept {
    return GetActiveMode() == WindowsGuestRedZoneProtectionMode::StaticPatching;
}

} // namespace WindowsGuestRedZoneProtection

namespace {

constexpr size_t GuestRedZoneSize = 128;
constexpr size_t ShortJumpSize = 2;
using RedZoneMask = std::bitset<GuestRedZoneSize>;

struct DecodedCodeInstruction {
    uintptr_t address{};
    x64_insn_t instruction{};
    std::array<x64_operand_t, X64_MAX_OPERANDS> operands{};
    RedZoneMask red_zone_use{};
    RedZoneMask red_zone_def{};
    RedZoneMask red_zone_live{};
    bool accesses_memory{};
    bool uses_stack_pointer{};
    bool has_red_zone_operand{};
    bool has_unmodeled_red_zone_operand{};
    bool changes_stack_pointer{};
    bool replaces_stack_pointer{};
    std::optional<s64> stack_pointer_delta;
};

struct DecodedFunction {
    std::map<uintptr_t, DecodedCodeInstruction> instructions;
    std::set<uintptr_t> branch_targets;
    bool uses_red_zone{};
    bool has_indirect_branch{};
    bool requires_conservative_red_zone_tracking{};
};

struct InstructionRewrite {
    const PatchInfo* cpu_patch{};
    bool protect_red_zone{};
    bool protected_indirect_call{};
};

bool IsStackPointerRegister(unsigned reg) {
    return reg != X64_REG_NONE && x64_register_largest(reg) == X64_REG_RSP;
}

bool IsControlFlowTerminator(const x64_insn_t& instruction) {
    return instruction.category == X64_CAT_UNCOND_BR || instruction.category == X64_CAT_RET ||
           instruction.category == X64_CAT_INTERRUPT || instruction.category == X64_CAT_SYSRET ||
           instruction.mnemonic == X64_MN_UD2;
}

uintptr_t GetRelativeTarget(const DecodedCodeInstruction& decoded) {
    for (u8 index = 0; index < decoded.instruction.operand_count_visible; ++index) {
        const auto& operand = decoded.operands[index];
        if (operand.type != X64_OPERAND_IMMEDIATE || !operand.imm.is_relative) {
            continue;
        }

        u64 target{};
        if (x64_operand_address(&decoded.instruction, &operand, decoded.address, &target)) {
            return target;
        }
    }
    return 0;
}

DecodedCodeInstruction DecodeCodeInstruction(uintptr_t address, uintptr_t end) {
    DecodedCodeInstruction decoded{.address = address};
    if (!DecodeInstruction(reinterpret_cast<void*>(address), end - address, decoded.instruction,
                           decoded.operands.data())) {
        decoded.instruction.len = 0;
        return decoded;
    }

    for (u8 index = 0; index < decoded.instruction.operand_count; ++index) {
        const auto& operand = decoded.operands[index];
        if (operand.type == X64_OPERAND_REGISTER) {
            decoded.uses_stack_pointer |= IsStackPointerRegister(operand.reg);
            if (IsStackPointerRegister(operand.reg) &&
                (operand.actions & X64_ACTION_MASK_WRITE) != 0 &&
                decoded.instruction.category != X64_CAT_CALL &&
                decoded.instruction.category != X64_CAT_RET) {
                decoded.changes_stack_pointer = true;
            }
            continue;
        }
        if (operand.type != X64_OPERAND_MEMORY) {
            continue;
        }

        const bool stack_relative =
            IsStackPointerRegister(operand.mem.base) || IsStackPointerRegister(operand.mem.index);
        decoded.uses_stack_pointer |= stack_relative;
        constexpr u8 MemoryAccessMask = X64_ACTION_MASK_READ | X64_ACTION_MASK_WRITE;
        if (decoded.instruction.mnemonic != X64_MN_LEA &&
            decoded.instruction.mnemonic != X64_MN_NOP && !stack_relative &&
            (operand.actions & MemoryAccessMask) != 0) {
            decoded.accesses_memory = true;
        }
    }

    if (decoded.changes_stack_pointer) {
        const auto& operands = decoded.operands;
        if (decoded.instruction.mnemonic == X64_MN_MOV &&
            operands[0].type == X64_OPERAND_REGISTER && IsStackPointerRegister(operands[0].reg) &&
            operands[1].type == X64_OPERAND_MEMORY) {
            decoded.replaces_stack_pointer = true;
        } else if ((decoded.instruction.mnemonic == X64_MN_ADD ||
                    decoded.instruction.mnemonic == X64_MN_SUB) &&
                   operands[0].type == X64_OPERAND_REGISTER &&
                   IsStackPointerRegister(operands[0].reg) &&
                   operands[1].type == X64_OPERAND_IMMEDIATE) {
            const s64 immediate = static_cast<s64>(operands[1].imm.value); // sign extended
            decoded.stack_pointer_delta =
                decoded.instruction.mnemonic == X64_MN_ADD ? immediate : -immediate;
        } else if (decoded.instruction.mnemonic == X64_MN_LEA &&
                   operands[0].type == X64_OPERAND_REGISTER &&
                   IsStackPointerRegister(operands[0].reg) &&
                   operands[1].type == X64_OPERAND_MEMORY &&
                   IsStackPointerRegister(operands[1].mem.base) &&
                   operands[1].mem.index == X64_REG_NONE) {
            decoded.stack_pointer_delta = operands[1].mem.disp;
        } else if (decoded.instruction.mnemonic == X64_MN_PUSH ||
                   decoded.instruction.mnemonic == X64_MN_PUSHF ||
                   decoded.instruction.mnemonic == X64_MN_PUSHFQ) { // no PUSHFD in long mode
            decoded.stack_pointer_delta = -static_cast<s64>(sizeof(u64));
        } else if (decoded.instruction.mnemonic == X64_MN_POP ||
                   decoded.instruction.mnemonic == X64_MN_POPF ||
                   decoded.instruction.mnemonic == X64_MN_POPFQ) { // no POPFD in long mode
            decoded.stack_pointer_delta = sizeof(u64);
        }
    }

    for (u8 index = 0; index < decoded.instruction.operand_count_visible; ++index) {
        const auto& operand = decoded.operands[index];
        if (operand.type != X64_OPERAND_MEMORY || !IsStackPointerRegister(operand.mem.base) ||
            operand.mem.disp_size == 0 || operand.mem.disp >= 0) {
            continue;
        }

        decoded.has_red_zone_operand = true;
        if (decoded.instruction.mnemonic == X64_MN_LEA) {
            if (decoded.stack_pointer_delta.has_value()) {
                decoded.has_red_zone_operand = false;
                continue;
            }
            decoded.has_unmodeled_red_zone_operand = true;
            continue;
        }

        const s64 access_start = operand.mem.disp;
        const s64 access_size = std::max<s64>(operand.size / 8, 1);
        const s64 range_start = std::max(access_start, -static_cast<s64>(GuestRedZoneSize));
        const s64 range_end = std::min(access_start + access_size, static_cast<s64>(0));
        for (s64 offset = range_start; offset < range_end; ++offset) {
            const size_t bit = static_cast<size_t>(offset + static_cast<s64>(GuestRedZoneSize));
            if ((operand.actions & X64_ACTION_MASK_READ) != 0) {
                decoded.red_zone_use.set(bit);
            }
            if ((operand.actions & X64_ACTION_WRITE) != 0) {
                decoded.red_zone_def.set(bit);
            }
        }
    }
    return decoded;
}

bool IsSameRegister(unsigned lhs, unsigned rhs) {
    return lhs != X64_REG_NONE && rhs != X64_REG_NONE &&
           x64_register_largest(lhs) == x64_register_largest(rhs);
}

bool WritesRegister(const DecodedCodeInstruction& decoded, unsigned reg) {
    return std::ranges::any_of(std::span{decoded.operands}.first(decoded.instruction.operand_count),
                               [reg](const x64_operand_t& operand) {
                                   return operand.type == X64_OPERAND_REGISTER &&
                                          (operand.actions & X64_ACTION_MASK_WRITE) != 0 &&
                                          IsSameRegister(operand.reg, reg);
                               });
}

std::optional<std::vector<uintptr_t>> ResolveBoundedJumpTable(
    const DecodedFunction& function, uintptr_t branch_address, uintptr_t function_start,
    uintptr_t function_end, uintptr_t segment_start, uintptr_t segment_end) {
    const auto branch = function.instructions.find(branch_address);
    if (branch == function.instructions.end() ||
        branch->second.instruction.mnemonic != X64_MN_JMP ||
        branch->second.operands[0].type != X64_OPERAND_REGISTER) {
        return std::nullopt;
    }
    const unsigned target_reg = branch->second.operands[0].reg;

    const auto previous_contiguous = [&function](auto instruction) {
        if (instruction == function.instructions.begin()) {
            return function.instructions.end();
        }
        const auto previous = std::prev(instruction);
        return previous->first + previous->second.instruction.len == instruction->first
                   ? previous
                   : function.instructions.end();
    };

    constexpr size_t MaxInterveningInstructions = 4;
    auto add = function.instructions.end();
    auto pattern_cursor = branch;
    for (size_t count = 0; count <= MaxInterveningInstructions; ++count) {
        const auto candidate = previous_contiguous(pattern_cursor);
        if (candidate == function.instructions.end()) {
            break;
        }
        const auto& decoded = candidate->second;
        if (decoded.instruction.mnemonic == X64_MN_ADD &&
            decoded.operands[0].type == X64_OPERAND_REGISTER &&
            IsSameRegister(decoded.operands[0].reg, target_reg) &&
            decoded.operands[1].type == X64_OPERAND_REGISTER) {
            add = candidate;
            break;
        }
        if (WritesRegister(decoded, target_reg) || IsControlFlowTerminator(decoded.instruction)) {
            return std::nullopt;
        }
        pattern_cursor = candidate;
    }
    if (add == function.instructions.end()) {
        return std::nullopt;
    }
    const unsigned table_reg = add->second.operands[1].reg;

    const auto load = previous_contiguous(add);
    if (load == function.instructions.end() || load->second.instruction.mnemonic != X64_MN_MOVSXD ||
        load->second.operands[0].type != X64_OPERAND_REGISTER ||
        !IsSameRegister(load->second.operands[0].reg, target_reg) ||
        load->second.operands[1].type != X64_OPERAND_MEMORY ||
        !IsSameRegister(load->second.operands[1].mem.base, table_reg) ||
        load->second.operands[1].mem.index == X64_REG_NONE ||
        load->second.operands[1].mem.scale != sizeof(s32)) {
        return std::nullopt;
    }
    const unsigned index_reg = load->second.operands[1].mem.index;

    constexpr size_t MaxPatternInstructions = 64;
    std::optional<size_t> table_size;
    std::optional<uintptr_t> guarded_path_start;
    auto cursor = load;
    for (size_t count = 0; count < MaxPatternInstructions; ++count) {
        cursor = previous_contiguous(cursor);
        if (cursor == function.instructions.end()) {
            break;
        }
        const auto& decoded = cursor->second;
        if (decoded.instruction.mnemonic == X64_MN_CMP &&
            decoded.operands[0].type == X64_OPERAND_REGISTER &&
            IsSameRegister(decoded.operands[0].reg, index_reg) &&
            decoded.operands[1].type == X64_OPERAND_IMMEDIATE) {
            const uintptr_t next_address = decoded.address + decoded.instruction.len;
            const auto bounds_branch = function.instructions.find(next_address);
            if (bounds_branch == function.instructions.end()) {
                return std::nullopt;
            }
            const s64 bound = static_cast<s64>(decoded.operands[1].imm.value); // sign extended
            if (bound < 0) {
                return std::nullopt;
            }
            if (bounds_branch->second.instruction.mnemonic == X64_MN_JNBE) {
                table_size = static_cast<size_t>(bound) + 1;
            } else if (bounds_branch->second.instruction.mnemonic == X64_MN_JNB) {
                table_size = static_cast<size_t>(bound);
            } else {
                return std::nullopt;
            }
            guarded_path_start = next_address + bounds_branch->second.instruction.len;
            break;
        }
        if (WritesRegister(decoded, index_reg)) {
            return std::nullopt;
        }
    }
    if (!table_size) {
        return std::nullopt;
    }
    if (std::ranges::any_of(function.branch_targets, [&](uintptr_t target) {
            return target >= *guarded_path_start && target <= branch_address;
        })) {
        return std::nullopt;
    }

    const auto decode_table_address =
        [table_reg](const DecodedCodeInstruction& decoded) -> std::optional<uintptr_t> {
        if (decoded.instruction.mnemonic != X64_MN_LEA ||
            decoded.operands[0].type != X64_OPERAND_REGISTER ||
            !IsSameRegister(decoded.operands[0].reg, table_reg) ||
            decoded.operands[1].type != X64_OPERAND_MEMORY) {
            return std::nullopt;
        }
        u64 absolute_address{};
        if (!x64_operand_address(&decoded.instruction, &decoded.operands[1], decoded.address,
                                 &absolute_address)) {
            return std::nullopt;
        }
        return absolute_address;
    };

    std::set<uintptr_t> table_candidates;
    cursor = load;
    for (size_t count = 0; count < MaxPatternInstructions; ++count) {
        cursor = previous_contiguous(cursor);
        if (cursor == function.instructions.end()) {
            break;
        }
        if (!WritesRegister(cursor->second, table_reg)) {
            continue;
        }
        if (const auto address = decode_table_address(cursor->second)) {
            table_candidates.insert(*address);
        }
        break;
    }
    if (table_candidates.empty()) {
        for (const auto& [address, decoded] : function.instructions) {
            if (address >= branch_address) {
                break;
            }
            if (const auto table_address = decode_table_address(decoded)) {
                table_candidates.insert(*table_address);
            }
        }
    }
    constexpr size_t MaxJumpTableEntries = 4096;
    if (*table_size == 0 || *table_size > MaxJumpTableEntries) {
        return std::nullopt;
    }

    std::optional<std::vector<uintptr_t>> resolved_targets;
    for (const uintptr_t table_address : table_candidates) {
        if (table_address < segment_start || table_address > segment_end ||
            *table_size > (segment_end - table_address) / sizeof(s32)) {
            continue;
        }

        std::vector<uintptr_t> targets;
        targets.reserve(*table_size);
        bool valid = true;
        for (size_t index = 0; index < *table_size; ++index) {
            s32 offset;
            std::memcpy(&offset,
                        reinterpret_cast<const void*>(table_address + index * sizeof(offset)),
                        sizeof(offset));
            const s64 target = static_cast<s64>(table_address) + offset;
            if (target < static_cast<s64>(function_start) ||
                target >= static_cast<s64>(function_end)) {
                valid = false;
                break;
            }
            targets.push_back(static_cast<uintptr_t>(target));
        }
        if (!valid) {
            continue;
        }
        std::ranges::sort(targets);
        targets.erase(std::ranges::unique(targets).begin(), targets.end());
        if (resolved_targets) {
            return std::nullopt;
        }
        resolved_targets = std::move(targets);
    }
    return resolved_targets;
}

DecodedFunction DecodeFunction(uintptr_t function_start, uintptr_t function_end,
                               uintptr_t segment_start, uintptr_t segment_end) {
    DecodedFunction function;
    std::vector<uintptr_t> blocks{function_start};
    std::unordered_set<uintptr_t> visited;
    std::set<uintptr_t> indirect_branches;
    std::set<uintptr_t> resolved_indirect_branches;

    while (true) {
        while (!blocks.empty()) {
            uintptr_t address = blocks.back();
            blocks.pop_back();

            while (address >= function_start && address < function_end &&
                   !visited.contains(address)) {
                visited.insert(address);
                auto decoded = DecodeCodeInstruction(address, function_end);
                if (decoded.instruction.len == 0) {
                    break;
                }

                function.uses_red_zone |= decoded.has_red_zone_operand;
                function.requires_conservative_red_zone_tracking |=
                    decoded.has_unmodeled_red_zone_operand ||
                    (decoded.changes_stack_pointer && !decoded.stack_pointer_delta.has_value());

                const uintptr_t next_address = address + decoded.instruction.len;
                const uintptr_t branch_target = GetRelativeTarget(decoded);
                if (branch_target >= function_start && branch_target < function_end) {
                    function.branch_targets.insert(branch_target);
                }
                function.instructions.emplace(address, decoded);

                if (decoded.instruction.category == X64_CAT_COND_BR) {
                    if (branch_target >= function_start && branch_target < function_end) {
                        blocks.push_back(branch_target);
                    }
                } else if (IsControlFlowTerminator(decoded.instruction)) {
                    if (decoded.instruction.category == X64_CAT_UNCOND_BR &&
                        branch_target >= function_start && branch_target < function_end) {
                        blocks.push_back(branch_target);
                    } else if (decoded.instruction.category == X64_CAT_UNCOND_BR &&
                               branch_target == 0) {
                        indirect_branches.insert(address);
                    }
                    break;
                }
                address = next_address;
            }
        }

        bool discovered_block = false;
        for (const uintptr_t branch_address : indirect_branches) {
            if (resolved_indirect_branches.contains(branch_address)) {
                continue;
            }
            const auto targets = ResolveBoundedJumpTable(function, branch_address, function_start,
                                                         function_end, segment_start, segment_end);
            if (!targets) {
                continue;
            }
            resolved_indirect_branches.insert(branch_address);
            for (const uintptr_t target : *targets) {
                function.branch_targets.insert(target);
                if (!visited.contains(target)) {
                    blocks.push_back(target);
                    discovered_block = true;
                }
            }
        }
        if (!discovered_block) {
            break;
        }
    }
    function.has_indirect_branch = indirect_branches.size() != resolved_indirect_branches.size();
    return function;
}

RedZoneMask TranslateRedZoneMask(const RedZoneMask& mask, s64 stack_pointer_delta) {
    RedZoneMask translated;
    for (size_t bit = 0; bit < GuestRedZoneSize; ++bit) {
        if (!mask.test(bit)) {
            continue;
        }
        const s64 after_offset = static_cast<s64>(bit) - static_cast<s64>(GuestRedZoneSize);
        const s64 before_offset = after_offset + stack_pointer_delta;
        if (before_offset >= -static_cast<s64>(GuestRedZoneSize) && before_offset < 0) {
            translated.set(static_cast<size_t>(before_offset + static_cast<s64>(GuestRedZoneSize)));
        }
    }
    return translated;
}

void AnalyzeRedZoneLiveness(DecodedFunction& function) {
    if (!function.uses_red_zone) {
        return;
    }

    if (function.has_indirect_branch || function.requires_conservative_red_zone_tracking ||
        std::ranges::any_of(function.branch_targets, [&function](uintptr_t target) {
            return !function.instructions.contains(target);
        })) {
        for (auto& [_, decoded] : function.instructions) {
            decoded.red_zone_live.set();
        }
        return;
    }

    std::vector<DecodedCodeInstruction*> instructions;
    instructions.reserve(function.instructions.size());
    std::map<uintptr_t, size_t> indices;
    for (auto& [address, decoded] : function.instructions) {
        indices.emplace(address, instructions.size());
        instructions.push_back(&decoded);
    }

    std::vector<RedZoneMask> live_in(instructions.size());
    bool changed;
    do {
        changed = false;
        for (size_t reverse_index = instructions.size(); reverse_index-- > 0;) {
            const auto& decoded = *instructions[reverse_index];
            RedZoneMask live_out;

            const auto add_successor = [&](uintptr_t address) {
                if (const auto successor = indices.find(address); successor != indices.end()) {
                    live_out |= live_in[successor->second];
                }
            };

            const uintptr_t next_address = decoded.address + decoded.instruction.len;
            const uintptr_t branch_target = GetRelativeTarget(decoded);
            if (decoded.instruction.category == X64_CAT_COND_BR) {
                add_successor(next_address);
                add_successor(branch_target);
            } else if (decoded.instruction.category == X64_CAT_UNCOND_BR) {
                add_successor(branch_target);
            } else if (!IsControlFlowTerminator(decoded.instruction)) {
                add_successor(next_address);
            }

            const RedZoneMask translated_live_out =
                decoded.stack_pointer_delta.has_value()
                    ? TranslateRedZoneMask(live_out, *decoded.stack_pointer_delta)
                    : live_out;
            const RedZoneMask new_live_in =
                decoded.red_zone_use | (translated_live_out & ~decoded.red_zone_def);
            if (new_live_in != live_in[reverse_index]) {
                live_in[reverse_index] = new_live_in;
                changed = true;
            }
        }
    } while (changed);

    for (size_t index = 0; index < instructions.size(); ++index) {
        instructions[index]->red_zone_live = live_in[index];
    }
}

bool EncodeRelocatedInstruction(const DecodedCodeInstruction& decoded, x64_code_t* generator) {
    // The instruction's own bytes; a relative branch or rip-relative operand still reaching its
    // target from the trampoline
    return x64_relocate(generator, reinterpret_cast<const u8*>(decoded.address),
                        &decoded.instruction, decoded.address) != 0;
}

bool GenerateProtectedIndirectCall(const DecodedCodeInstruction& decoded, x64_code_t* generator) {
    ASSERT(decoded.instruction.category == X64_CAT_CALL);
    const auto& target = decoded.operands[0];
    ASSERT(target.type == X64_OPERAND_MEMORY && target.size == sizeof(uintptr_t) * CHAR_BIT);

    // mov r11, the call's memory operand (its segment, address size and target kept)
    x64_mem_t source;
    if (target.mem.base == X64_REG_RIP || target.mem.base == X64_REG_EIP) {
        u64 absolute_address{};
        if (target.mem.base == X64_REG_EIP ||
            !x64_operand_address(&decoded.instruction, &target, decoded.address,
                                 &absolute_address)) {
            return false;
        }
        source = x64_mrip(absolute_address);
    } else {
        const auto gpr = [](unsigned reg) {
            return reg == X64_REG_NONE ? -1
                                       : static_cast<int>(x64_register_largest(reg) - X64_REG_RAX);
        };
        source = x64_mi(gpr(target.mem.base), gpr(target.mem.index),
                        target.mem.scale ? target.mem.scale : 1,
                        target.mem.disp_size ? static_cast<s32>(target.mem.disp) : 0);
        if (target.mem.base == X64_REG_NONE && target.mem.index == X64_REG_NONE) {
            source = x64_mabs(static_cast<s32>(target.mem.disp));
        }
    }
    if (target.mem.segment == X64_REG_FS || target.mem.segment == X64_REG_GS) {
        source.seg = target.mem.segment == X64_REG_FS ? X64_SEG_FS : X64_SEG_GS;
    }

    x64_lea(generator, X64_RSP, x64_m(X64_RSP, -static_cast<s32>(GuestRedZoneSize)));
    x64_push(generator, X64_R11);

    if (decoded.instruction.address_width == 32) {
        static const u8 address_size = 0x67;
        x64_db(generator, &address_size, 1);
    }
    x64_mov_rm(generator, 64, X64_R11, source);

    x64_xchg_rm(generator, X64_R11, x64_m(X64_RSP, 0));
    x64_lea(generator, X64_RSP,
            x64_m(X64_RSP, static_cast<s32>(GuestRedZoneSize + sizeof(uintptr_t))));
    x64_call_m(generator, x64_m(X64_RSP, -static_cast<s32>(GuestRedZoneSize + sizeof(uintptr_t))));
    return true;
}

bool IsInPlaceMemoryPatch(u16 mnemonic) {
    return mnemonic == X64_MN_MOVNTSS || mnemonic == X64_MN_MOVNTSD;
}

const PatchInfo* FindMatchingPatch(const DecodedCodeInstruction& decoded) {
    const auto patches = Patches.find(decoded.instruction.mnemonic);
    if (patches == Patches.end()) {
        return nullptr;
    }
    const auto patch =
        std::ranges::find_if(patches->second, [&decoded](const PatchInfo& candidate) {
            return candidate.filter(decoded.operands.data());
        });
    return patch != patches->second.end() ? &*patch : nullptr;
}

} // namespace

using AddressRanges = std::vector<std::pair<uintptr_t, uintptr_t>>;

/// Applies the CPU patches to every function of the segment. If red_zone_protection is true,
/// the red-zone accesses of those functions are protected as well. If covered_ranges is not
/// null, it receives the sorted address ranges of the decoded instructions.
static RedZonePatchResult PatchSegmentStatically(u64 segment_addr, u64 segment_size,
                                                 std::span<const uintptr_t> function_starts,
                                                 bool red_zone_protection,
                                                 AddressRanges* covered_ranges) {
    RedZonePatchResult result{};
    auto* module = GetContainingModule(reinterpret_cast<void*>(segment_addr));
    if (module == nullptr || function_starts.empty()) {
        return result;
    }

    const uintptr_t segment_end = segment_addr + segment_size;
    std::vector<uintptr_t> starts;
    starts.reserve(function_starts.size());
    for (const uintptr_t start : function_starts) {
        if (start >= segment_addr && start < segment_end) {
            starts.push_back(start);
        }
    }
    std::ranges::sort(starts);
    const auto unique_end = std::ranges::unique(starts).begin();
    starts.erase(unique_end, starts.end());

    return module->mutex.Run([&]() -> RedZonePatchResult {
        for (size_t function_index = 0; function_index < starts.size(); ++function_index) {
            const uintptr_t function_start = starts[function_index];
            const uintptr_t function_end =
                function_index + 1 < starts.size() ? starts[function_index + 1] : segment_end;
            if (function_end <= function_start) {
                continue;
            }

            ++result.function_count;
            auto function = DecodeFunction(function_start, function_end, segment_addr, segment_end);
            if (red_zone_protection) {
                AnalyzeRedZoneLiveness(function);
            }
            result.instruction_count += function.instructions.size();
            if (covered_ranges != nullptr) {
                // Functions and their instructions are visited in address order.
                for (const auto& [address, decoded] : function.instructions) {
                    const uintptr_t end = address + decoded.instruction.len;
                    if (!covered_ranges->empty() && address <= covered_ranges->back().second) {
                        covered_ranges->back().second =
                            std::max(covered_ranges->back().second, end);
                    } else {
                        covered_ranges->emplace_back(address, end);
                    }
                }
            }

            std::map<uintptr_t, InstructionRewrite> rewrite_sites;

            // Apply existing instruction substitutions ahead of time now that only verified
            // instruction boundaries are being visited. Short trampoline patches are combined with
            // the relocation pass below.
            for (auto& [address, decoded] : function.instructions) {
                const PatchInfo* matching_patch = FindMatchingPatch(decoded);
                const auto [patched, _] = TryPatch(reinterpret_cast<u8*>(address), module);
                result.inplace_cpu_patch_instruction_count += patched;
                if (IsInPlaceMemoryPatch(decoded.instruction.mnemonic)) {
                    const RedZoneMask red_zone_live = decoded.red_zone_live;
                    decoded = DecodeCodeInstruction(address, function_end);
                    decoded.red_zone_live = red_zone_live;
                } else if (patched) {
                    decoded = DecodeCodeInstruction(address, function_end);
                    decoded.accesses_memory = false;
                } else if (matching_patch != nullptr && matching_patch->trampoline) {
                    rewrite_sites.emplace(address, InstructionRewrite{.cpu_patch = matching_patch});
                    ++result.cpu_patch_instruction_count;
                }
            }

            if (red_zone_protection && function.uses_red_zone) {
                ++result.red_zone_function_count;
                result.indirect_red_zone_function_count += function.has_indirect_branch;
                for (const auto& [address, decoded] : function.instructions) {
                    if (!decoded.accesses_memory || !decoded.red_zone_live.any() ||
                        rewrite_sites.contains(address)) {
                        continue;
                    }
                    ++result.memory_instruction_count;
                    if (decoded.instruction.len < NearJumpSize) {
                        ++result.short_memory_instruction_count;
                    }
                    if (decoded.instruction.category == X64_CAT_CALL &&
                        decoded.operands[0].type == X64_OPERAND_MEMORY &&
                        decoded.operands[0].size == sizeof(uintptr_t) * CHAR_BIT &&
                        !IsStackPointerRegister(decoded.operands[0].mem.base) &&
                        !IsStackPointerRegister(decoded.operands[0].mem.index)) {
                        rewrite_sites[address] = {
                            .protect_red_zone = true,
                            .protected_indirect_call = true,
                        };
                        continue;
                    }
                    if (decoded.uses_stack_pointer && !decoded.replaces_stack_pointer) {
                        ++result.stack_dependent_memory_instruction_count;
                        continue;
                    }
                    if (decoded.instruction.category == X64_CAT_CALL ||
                        IsControlFlowTerminator(decoded.instruction)) {
                        ++result.control_flow_memory_instruction_count;
                        continue;
                    }
                    rewrite_sites[address].protect_red_zone = true;
                }
            }
            if (rewrite_sites.empty()) {
                continue;
            }

            struct RelocationSpan {
                std::vector<const DecodedCodeInstruction*> instructions;
                uintptr_t patch_start{};
                uintptr_t continuation{};
                size_t patch_size{};
            };

            const auto emit_span = [&](const RelocationSpan& span) -> std::optional<size_t> {
                x64_code_t* tg = &module->trampoline_gen;
                const size_t trampoline_offset = tg->size;
                if (module->trampoline_exhausted) {
                    return std::nullopt;
                }
                {
                    for (const auto* decoded : span.instructions) {
                        const auto rewrite = rewrite_sites.find(decoded->address);
                        const bool protected_indirect_call =
                            rewrite != rewrite_sites.end() &&
                            rewrite->second.protected_indirect_call;
                        const bool protect_red_zone = rewrite != rewrite_sites.end() &&
                                                      rewrite->second.protect_red_zone &&
                                                      !protected_indirect_call;
                        if (protect_red_zone) {
                            x64_lea(tg, X64_RSP,
                                    x64_m(X64_RSP, -static_cast<s32>(GuestRedZoneSize)));
                        }
                        if (protected_indirect_call) {
                            if (!GenerateProtectedIndirectCall(*decoded, tg)) {
                                tg->size = trampoline_offset;
                                tg->error = 0;
                                return std::nullopt;
                            }
                        } else if (rewrite != rewrite_sites.end() &&
                                   rewrite->second.cpu_patch != nullptr) {
                            rewrite->second.cpu_patch->generator(
                                reinterpret_cast<void*>(decoded->address), decoded->instruction,
                                decoded->operands.data(), tg);
                        } else if (!EncodeRelocatedInstruction(*decoded, tg)) {
                            tg->size = trampoline_offset;
                            tg->error = 0;
                            return std::nullopt;
                        }
                        if (protect_red_zone && !decoded->replaces_stack_pointer) {
                            x64_lea(tg, X64_RSP,
                                    x64_m(X64_RSP, static_cast<s32>(GuestRedZoneSize)));
                        }
                    }
                    x64_jmp(tg, span.continuation, X64_JMP_AUTO);
                }
                if (tg->error) {
                    TrampolineFull(module, trampoline_offset);
                    return std::nullopt;
                }
                return trampoline_offset;
            };

            const auto record_rewrites = [&](const RelocationSpan& span) {
                for (const auto* decoded : span.instructions) {
                    const auto rewrite = rewrite_sites.find(decoded->address);
                    if (rewrite == rewrite_sites.end()) {
                        continue;
                    }
                    if (rewrite->second.protect_red_zone) {
                        ++result.patched_memory_instruction_count;
                    }
                    if (rewrite->second.cpu_patch != nullptr) {
                        ++result.patched_cpu_patch_instruction_count;
                    }
                    module->patched.insert(reinterpret_cast<u8*>(decoded->address));
                }
            };

            std::vector<std::pair<uintptr_t, uintptr_t>> patched_spans;
            std::vector<uintptr_t> relay_slots;
            std::vector<uintptr_t> short_relay_slots;
            std::vector<uintptr_t> unresolved_sites;
            uintptr_t covered_until{};
            for (auto site_it = rewrite_sites.begin(); site_it != rewrite_sites.end(); ++site_it) {
                const uintptr_t site = site_it->first;
                if (site < covered_until) {
                    continue;
                }

                const auto collect_forward_span = [&]() -> std::optional<RelocationSpan> {
                    RelocationSpan span{.patch_start = site, .continuation = site};
                    while (span.patch_size < NearJumpSize) {
                        const auto decoded_it = function.instructions.find(span.continuation);
                        if (decoded_it == function.instructions.end() ||
                            (span.continuation != site &&
                             function.branch_targets.contains(span.continuation))) {
                            return std::nullopt;
                        }

                        const auto& decoded = decoded_it->second;
                        span.instructions.push_back(&decoded);
                        span.patch_size += decoded.instruction.len;
                        span.continuation += decoded.instruction.len;
                        if (span.patch_size < NearJumpSize &&
                            IsControlFlowTerminator(decoded.instruction)) {
                            return std::nullopt;
                        }
                    }
                    return span;
                };

                const auto collect_backward_span = [&]() -> std::optional<RelocationSpan> {
                    const auto site_instruction = function.instructions.find(site);
                    ASSERT(site_instruction != function.instructions.end());
                    RelocationSpan span{
                        .instructions = {&site_instruction->second},
                        .patch_start = site,
                        .continuation = site + site_instruction->second.instruction.len,
                        .patch_size = site_instruction->second.instruction.len,
                    };

                    while (span.patch_size < NearJumpSize) {
                        const auto previous_end =
                            function.instructions.lower_bound(span.patch_start);
                        if (previous_end == function.instructions.begin() ||
                            function.branch_targets.contains(span.patch_start)) {
                            return std::nullopt;
                        }

                        const auto previous = std::prev(previous_end);
                        const auto& decoded = previous->second;
                        if (previous->first + decoded.instruction.len != span.patch_start ||
                            previous->first < covered_until ||
                            IsControlFlowTerminator(decoded.instruction)) {
                            return std::nullopt;
                        }

                        span.instructions.insert(span.instructions.begin(), &decoded);
                        span.patch_start = previous->first;
                        span.patch_size += decoded.instruction.len;
                    }
                    return span;
                };

                std::optional<RelocationSpan> selected_span;
                std::optional<size_t> trampoline_offset;
                const auto& site_instruction = function.instructions.at(site);
                const bool can_relocate_neighbors = !function.has_indirect_branch;
                if (can_relocate_neighbors || site_instruction.instruction.len >= NearJumpSize) {
                    auto forward_span = collect_forward_span();
                    if (forward_span) {
                        trampoline_offset = emit_span(*forward_span);
                        if (trampoline_offset) {
                            selected_span = std::move(forward_span);
                        }
                    }
                }
                if (!selected_span && can_relocate_neighbors) {
                    if (auto backward_span = collect_backward_span()) {
                        trampoline_offset = emit_span(*backward_span);
                        if (trampoline_offset) {
                            selected_span = std::move(backward_span);
                        }
                    }
                }
                if (!selected_span) {
                    unresolved_sites.push_back(site);
                    continue;
                }

                const auto& span = *selected_span;
                const auto* trampoline = module->trampoline_gen.p + *trampoline_offset;

                auto& patch_gen = module->patch_gen;
                patch_gen.error = 0;
                patch_gen.size = span.patch_start - reinterpret_cast<uintptr_t>(patch_gen.p);
                x64_jmp(&patch_gen, reinterpret_cast<u64>(trampoline), X64_JMP_NEAR);
                x64_nop(&patch_gen, span.patch_size - NearJumpSize);

                record_rewrites(span);
                patched_spans.emplace_back(span.patch_start, span.continuation);
                if (span.patch_size >= NearJumpSize * 2) {
                    relay_slots.push_back(span.patch_start + NearJumpSize);
                }
                if (span.patch_size >= NearJumpSize + ShortJumpSize) {
                    short_relay_slots.push_back(span.patch_start + NearJumpSize);
                }
                covered_until = span.continuation;
            }

            const auto record_unsupported = [&](uintptr_t site) {
                const auto& rewrite = rewrite_sites.at(site);
                if (rewrite.cpu_patch != nullptr) {
                    ++result.unsupported_cpu_patch_instruction_count;
                }
                if (rewrite.protect_red_zone) {
                    ++result.unrelocatable_memory_instruction_count;
                }
            };
            const auto overlaps_patched_span = [&patched_spans](uintptr_t start, uintptr_t end) {
                return std::ranges::any_of(patched_spans, [start, end](const auto& patched) {
                    return start < patched.second && patched.first < end;
                });
            };

            for (const uintptr_t site : unresolved_sites) {
                if (module->patched.contains(reinterpret_cast<u8*>(site))) {
                    continue;
                }

                const auto site_instruction = function.instructions.find(site);
                ASSERT(site_instruction != function.instructions.end());
                if (function.has_indirect_branch ||
                    site_instruction->second.instruction.len < ShortJumpSize) {
                    record_unsupported(site);
                    continue;
                }

                const RelocationSpan site_span{
                    .instructions = {&site_instruction->second},
                    .patch_start = site,
                    .continuation = site + site_instruction->second.instruction.len,
                    .patch_size = site_instruction->second.instruction.len,
                };
                const size_t trampoline_start = module->trampoline_gen.size;
                const auto site_trampoline_offset = emit_span(site_span);
                if (!site_trampoline_offset) {
                    record_unsupported(site);
                    continue;
                }

                constexpr s64 ShortJumpMin = std::numeric_limits<s8>::min();
                constexpr s64 ShortJumpMax = std::numeric_limits<s8>::max();
                const auto relay_slot =
                    std::ranges::find_if(relay_slots, [site](uintptr_t address) {
                        const s64 displacement =
                            static_cast<s64>(address) - static_cast<s64>(site + ShortJumpSize);
                        return displacement >= ShortJumpMin && displacement <= ShortJumpMax;
                    });
                if (relay_slot != relay_slots.end()) {
                    const auto* site_trampoline =
                        module->trampoline_gen.p + *site_trampoline_offset;

                    auto& patch_gen = module->patch_gen;
                    patch_gen.error = 0;
                    patch_gen.size = *relay_slot - reinterpret_cast<uintptr_t>(patch_gen.p);
                    x64_jmp(&patch_gen, reinterpret_cast<u64>(site_trampoline), X64_JMP_NEAR);

                    patch_gen.error = 0;
                    patch_gen.size = site - reinterpret_cast<uintptr_t>(patch_gen.p);
                    x64_jmp(&patch_gen, static_cast<u64>(*relay_slot), X64_JMP_SHORT);
                    ASSERT_MSG(!patch_gen.error, "Short jump out of reach");
                    x64_nop(&patch_gen, site_span.patch_size - ShortJumpSize);

                    record_rewrites(site_span);
                    patched_spans.emplace_back(site_span.patch_start, site_span.continuation);
                    std::erase(short_relay_slots, *relay_slot);
                    relay_slots.erase(relay_slot);
                    continue;
                }

                const auto find_host = [&](uintptr_t short_jump_address) {
                    std::pair<std::optional<RelocationSpan>, std::optional<size_t>> result;
                    const s64 minimum_host = static_cast<s64>(short_jump_address) +
                                             static_cast<s64>(ShortJumpSize) -
                                             static_cast<s64>(NearJumpSize) + ShortJumpMin;
                    const s64 maximum_host = static_cast<s64>(short_jump_address) +
                                             static_cast<s64>(ShortJumpSize) -
                                             static_cast<s64>(NearJumpSize) + ShortJumpMax;

                    auto candidate = function.instructions.lower_bound(
                        static_cast<uintptr_t>(std::max<s64>(minimum_host, 0)));
                    for (; candidate != function.instructions.end() &&
                           static_cast<s64>(candidate->first) <= maximum_host;
                         ++candidate) {
                        RelocationSpan span{.patch_start = candidate->first,
                                            .continuation = candidate->first};
                        while (span.patch_size < NearJumpSize * 2) {
                            const auto instruction = function.instructions.find(span.continuation);
                            if (instruction == function.instructions.end() ||
                                (span.continuation != span.patch_start &&
                                 function.branch_targets.contains(span.continuation))) {
                                span.instructions.clear();
                                break;
                            }
                            span.instructions.push_back(&instruction->second);
                            span.patch_size += instruction->second.instruction.len;
                            span.continuation += instruction->second.instruction.len;
                            if (span.patch_size < NearJumpSize * 2 &&
                                IsControlFlowTerminator(instruction->second.instruction)) {
                                span.instructions.clear();
                                break;
                            }
                        }
                        if (span.instructions.empty() ||
                            !(span.continuation <= site ||
                              span.patch_start >= site_span.continuation) ||
                            overlaps_patched_span(span.patch_start, span.continuation)) {
                            continue;
                        }

                        const uintptr_t relay_address = span.patch_start + NearJumpSize;
                        const s64 displacement =
                            static_cast<s64>(relay_address) -
                            static_cast<s64>(short_jump_address + ShortJumpSize);
                        if (displacement < ShortJumpMin || displacement > ShortJumpMax) {
                            continue;
                        }

                        const auto offset = emit_span(span);
                        if (!offset) {
                            continue;
                        }
                        result.first = std::move(span);
                        result.second = offset;
                        break;
                    }
                    return result;
                };

                auto [host_span, host_trampoline_offset] = find_host(site);
                std::optional<uintptr_t> final_relay_slot;
                uintptr_t final_short_jump = site;
                std::map<uintptr_t, uintptr_t> relay_parent{{site, site}};
                std::vector<uintptr_t> relay_queue{site};
                for (size_t queue_index = 0;
                     !host_span && !final_relay_slot && queue_index < relay_queue.size();
                     ++queue_index) {
                    const uintptr_t current = relay_queue[queue_index];
                    if (current != site) {
                        if (std::ranges::find(relay_slots, current) != relay_slots.end()) {
                            final_relay_slot = current;
                            final_short_jump = relay_parent.at(current);
                            break;
                        }
                        const auto near_slot =
                            std::ranges::find_if(relay_slots, [current](uintptr_t address) {
                                const s64 displacement = static_cast<s64>(address) -
                                                         static_cast<s64>(current + ShortJumpSize);
                                return address != current && displacement >= ShortJumpMin &&
                                       displacement <= ShortJumpMax;
                            });
                        if (near_slot != relay_slots.end()) {
                            final_relay_slot = *near_slot;
                            final_short_jump = current;
                            break;
                        }

                        auto bridge_host = find_host(current);
                        if (bridge_host.first) {
                            host_span = std::move(bridge_host.first);
                            host_trampoline_offset = bridge_host.second;
                            final_short_jump = current;
                            break;
                        }
                    }

                    for (const uintptr_t slot : short_relay_slots) {
                        if (relay_parent.contains(slot)) {
                            continue;
                        }
                        const s64 displacement =
                            static_cast<s64>(slot) - static_cast<s64>(current + ShortJumpSize);
                        if (displacement < ShortJumpMin || displacement > ShortJumpMax) {
                            continue;
                        }
                        relay_parent.emplace(slot, current);
                        relay_queue.push_back(slot);
                    }
                }

                if (!host_span && !final_relay_slot) {
                    module->trampoline_gen.size = trampoline_start;
                    record_unsupported(site);
                    continue;
                }

                const auto* site_trampoline = module->trampoline_gen.p + *site_trampoline_offset;
                auto& patch_gen = module->patch_gen;
                uintptr_t relay_address{};
                if (final_relay_slot) {
                    relay_address = *final_relay_slot;
                    patch_gen.error = 0;
                    patch_gen.size = relay_address - reinterpret_cast<uintptr_t>(patch_gen.p);
                    x64_jmp(&patch_gen, reinterpret_cast<u64>(site_trampoline), X64_JMP_NEAR);
                    std::erase(relay_slots, relay_address);
                    std::erase(short_relay_slots, relay_address);
                } else {
                    ASSERT(host_span && host_trampoline_offset);
                    const auto* host_trampoline =
                        module->trampoline_gen.p + *host_trampoline_offset;
                    relay_address = host_span->patch_start + NearJumpSize;

                    patch_gen.error = 0;
                    patch_gen.size =
                        host_span->patch_start - reinterpret_cast<uintptr_t>(patch_gen.p);
                    x64_jmp(&patch_gen, reinterpret_cast<u64>(host_trampoline), X64_JMP_NEAR);
                    x64_jmp(&patch_gen, reinterpret_cast<u64>(site_trampoline), X64_JMP_NEAR);
                    x64_nop(&patch_gen, host_span->patch_size - NearJumpSize * 2);
                }

                uintptr_t jump_target = relay_address;
                while (final_short_jump != site) {
                    patch_gen.error = 0;
                    patch_gen.size = final_short_jump - reinterpret_cast<uintptr_t>(patch_gen.p);
                    x64_jmp(&patch_gen, static_cast<u64>(jump_target), X64_JMP_SHORT);
                    ASSERT_MSG(!patch_gen.error, "Short jump out of reach");
                    jump_target = final_short_jump;
                    const auto parent = relay_parent.find(final_short_jump);
                    ASSERT(parent != relay_parent.end());
                    final_short_jump = parent->second;
                    std::erase(relay_slots, jump_target);
                    std::erase(short_relay_slots, jump_target);
                }

                patch_gen.error = 0;
                patch_gen.size = site - reinterpret_cast<uintptr_t>(patch_gen.p);
                x64_jmp(&patch_gen, static_cast<u64>(jump_target), X64_JMP_SHORT);
                ASSERT_MSG(!patch_gen.error, "Short jump out of reach");
                x64_nop(&patch_gen, site_span.patch_size - ShortJumpSize);

                if (host_span) {
                    record_rewrites(*host_span);
                    patched_spans.emplace_back(host_span->patch_start, host_span->continuation);
                    if (host_span->patch_size >= NearJumpSize * 3) {
                        relay_slots.push_back(host_span->patch_start + NearJumpSize * 2);
                    }
                    if (host_span->patch_size >= NearJumpSize * 2 + ShortJumpSize) {
                        short_relay_slots.push_back(host_span->patch_start + NearJumpSize * 2);
                    }
                }
                record_rewrites(site_span);
                patched_spans.emplace_back(site_span.patch_start, site_span.continuation);
            }
        }
        return result;
    });
}

/// Returns the start of the SSE4a instruction whose opcode is at the given address, or null if
/// the bytes before it are not the prefixes of one.
static u8* FindSSE4aInstructionStart(u8* opcode, const u8* lower_bound) {
    if (opcode[0] != 0x0F || (opcode[1] != 0x78 && opcode[1] != 0x79 && opcode[1] != 0x2B)) {
        return nullptr;
    }
    u8* start = opcode - 1;
    if (start >= lower_bound && (*start & 0xF0) == 0x40) {
        // REX prefix
        --start;
    }
    if (start < lower_bound || (*start != 0x66 && *start != 0xF2 && *start != 0xF3)) {
        return nullptr;
    }
    return start;
}

#if defined(__APPLE__)
/// Returns the start of the VEX encoded VCMPSS whose opcode is at the given address, or null if
/// the bytes before it are not its VEX prefix.
static u8* FindScalarCompareInstructionStart(u8* opcode, const u8* lower_bound) {
    if (opcode[0] != 0xC2) {
        return nullptr;
    }
    // Two byte VEX with L = 0 and the F3 implied prefix.
    if (opcode - 2 >= lower_bound && opcode[-2] == 0xC5 && (opcode[-1] & 0x7) == 0x2) {
        return opcode - 2;
    }
    // Three byte VEX in the 0F map with L = 0 and the F3 implied prefix.
    if (opcode - 3 >= lower_bound && opcode[-3] == 0xC4 && (opcode[-2] & 0x1F) == 0x1 &&
        (opcode[-1] & 0x7) == 0x2) {
        return opcode - 3;
    }
    return nullptr;
}
#endif

/// Patches the CPU instructions between covered_ranges in place, without relocating any.
/// The bytes after the last range are skipped, as they hold read-only data.
static void PatchUncoveredCpuInstructions(u64 segment_addr, const AddressRanges& covered_ranges,
                                          RedZonePatchResult& result) {
    auto* module = GetContainingModule(reinterpret_cast<void*>(segment_addr));
    if (module == nullptr) {
        return;
    }
    module->mutex.Run([&] {
        const auto patch_gap = [&](uintptr_t gap_start, uintptr_t gap_end) {
            auto* const lower_bound = reinterpret_cast<u8*>(gap_start);
            for (uintptr_t address = gap_start; address + 2 <= gap_end; ++address) {
                u8* start = FindSSE4aInstructionStart(reinterpret_cast<u8*>(address), lower_bound);
#if defined(__APPLE__)
                if (start == nullptr) {
                    start = FindScalarCompareInstructionStart(reinterpret_cast<u8*>(address),
                                                              lower_bound);
                }
#endif
                if (start == nullptr) {
                    continue;
                }
                const auto decoded =
                    DecodeCodeInstruction(reinterpret_cast<uintptr_t>(start), gap_end);
                if (decoded.instruction.len == 0 || FindMatchingPatch(decoded) == nullptr) {
                    continue;
                }
                if (TryPatch(start, module).first) {
                    ++result.uncovered_inplace_cpu_patch_instruction_count;
                } else if (!IsInPlaceMemoryPatch(decoded.instruction.mnemonic)) {
                    ++result.uncovered_unsupported_cpu_patch_instruction_count;
                }
                address = decoded.address + decoded.instruction.len - 1;
            }
        };

        uintptr_t cursor = segment_addr;
        for (const auto& [range_start, range_end] : covered_ranges) {
            if (range_start > cursor) {
                patch_gap(cursor, range_start);
            }
            cursor = std::max(cursor, range_end);
        }
    });
}

RedZonePatchResult PatchRedZoneMemoryInstructions(u64 segment_addr, u64 segment_size,
                                                  std::span<const uintptr_t> function_starts) {
    return PatchSegmentStatically(segment_addr, segment_size, function_starts, true, nullptr);
}

RedZonePatchResult PatchCpuInstructionsStatically(u64 segment_addr, u64 segment_size,
                                                  std::span<const uintptr_t> function_starts) {
    AddressRanges covered_ranges;
    auto result =
        PatchSegmentStatically(segment_addr, segment_size, function_starts, false, &covered_ranges);
    // The EH frame search table does not list every function of every module.
    PatchUncoveredCpuInstructions(segment_addr, covered_ranges, result);
    return result;
}

// ============================================================================
// End Windows static guest red-zone protection
// ============================================================================

static bool PatchesAccessViolationHandler(void* context, void* /* fault_address */) {
    return TryPatchJit(Common::GetRip(context));
}

static bool PatchesIllegalInstructionHandler(void* context) {
    void* code_address = Common::GetRip(context);
#if defined(_WIN32)
    // Windows static guest red-zone protection
    const bool inspect_short_cpu_patch =
        !WindowsGuestRedZoneProtection::IsStaticPatchingEnabled() ||
        GetContainingModule(code_address) != nullptr;
#else
    constexpr bool inspect_short_cpu_patch = true;
#endif
    if (inspect_short_cpu_patch && // Windows static guest red-zone protection
        Is4ByteExtrqOrInsertq(code_address)) {
        // The instruction is not big enough for a relative jump, don't try to patch it and pass it
        // to our illegal instruction interpreter directly
        return TryExecuteIllegalInstruction(context, code_address);
    } else {
        if (!TryPatchJit(code_address)) {
            x64_insn_t instruction;
            x64_operand_t operands[X64_MAX_OPERANDS];
            if (DecodeInstruction(code_address, 15, instruction, operands)) {
                char disassembled[256];
                x64_format(&instruction, operands, std::bit_cast<u64>(code_address), disassembled,
                           sizeof(disassembled));
                LOG_ERROR(Core,
                          "Failed to patch address %" PRIx64 " -- mnemonic: %s, instruction: %s",
                          reinterpret_cast<u64>(code_address),
                          x64_mnemonic_name(instruction.mnemonic), disassembled);
            } else {
                LOG_ERROR(Core,
                          "Failed to patch address %" PRIx64 " -- mnemonic: (failed to decode)",
                          reinterpret_cast<u64>(code_address));
            }
            return false;
        }
    }

    return true;
}

static void PatchesInit() {
    if (!Patches.empty()) {
        auto* signals = Signals::Instance();
        // Should be called last.
        constexpr auto priority = std::numeric_limits<u32>::max();
        signals->RegisterAccessViolationHandler(PatchesAccessViolationHandler, priority);
        signals->RegisterIllegalInstructionHandler(PatchesIllegalInstructionHandler, priority);
    }
}

void RegisterPatchModule(void* module_ptr, u64 module_size, void* trampoline_area_ptr,
                         u64 trampoline_area_size) {
    run_once(&init_flag, [](void*) { PatchesInit(); }, nullptr);

    const auto module_addr = reinterpret_cast<u64>(module_ptr);
    modules.emplace(std::piecewise_construct, std::forward_as_tuple(module_addr),
                    std::forward_as_tuple(static_cast<u8*>(module_ptr), module_size,
                                          static_cast<u8*>(trampoline_area_ptr),
                                          trampoline_area_size));
}

void PrePatchInstructions(u64 segment_addr, u64 segment_size) {
#if !defined(_WIN32) && !defined(__APPLE__)
    // Linux and others have an FS segment pointing to valid memory, so continue to do full
    // ahead-of-time patching for now until a better solution is worked out.
    if (!Patches.empty()) {
        TryPatchAot(reinterpret_cast<void*>(segment_addr), segment_size);
    }
#endif
}

} // namespace Core
