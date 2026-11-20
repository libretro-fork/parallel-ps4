// SPDX-FileCopyrightText: Copyright 2024-2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <array>
#include <cstring>
#include <unordered_map>
#include <encodings/xxh3.h>
#include <memmap.h>
#include <queue>
#include "common/arch.h"
#include "common/flat_map.h"
#include "common/io_file.h"
#include "common/logging/log.h"
#include "common/path_util.h"
#include "common/run_once.h"
#include "common/signal_context.h"
#include "common/string_util.h"
#include "common/x64_decode.h"
#include "common/x64_emit.h"
#include "core/emulator_settings.h"
#include "core/signals.h"
#include "shader_recompiler/info.h"
#include "shader_recompiler/ir/ir_emitter.h"
#include "shader_recompiler/ir/opcodes.h"
#include "shader_recompiler/ir/passes/srt.h"
#include "shader_recompiler/ir/program.h"
#include "shader_recompiler/ir/reg.h"
#include "shader_recompiler/ir/srt_gvn_table.h"
#include "shader_recompiler/ir/value.h"

// The libretro core on aarch64 emits the same x86-64 walkers, which srt_walker_interpreter.cpp
// runs there (RunSrtWalker)
#if defined(ARCH_X86_64) || (defined(__aarch64__) && defined(__linux__))

// The walkers' code, x86-64 on every host (an aarch64 one interprets it): 32 MB, made
// executable once, at the first walker
static x64_code_t g_srt_code{};
static run_once_t g_srt_code_once = RUN_ONCE_INIT;
static const u8* g_srt_codegen_start = nullptr;

static x64_code_t* SrtCode() {
    run_once(
        &g_srt_code_once,
        [](void*) {
            constexpr size_t size = 32_MB;
            void* mem = mmap(nullptr, size, PROT_READ | PROT_WRITE | PROT_EXEC,
                             MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
            ASSERT_MSG(mem != MAP_FAILED, "No executable memory for the SRT walkers");
            x64_init(&g_srt_code, mem, size);
        },
        nullptr);
    return &g_srt_code;
}

namespace Shader {

// The walkers kept, by their code. The permutations of a program have the same walker, and
// each kept a copy of its own: with the hundreds of permutations a material shader gets, the
// 32 MB of code could run out in a long session, and the game stopped ("code is too big";
// doax3-scarlet-pc, from The Order). A walker is straight-line code with nothing in it that
// depends on where it is, so one copy serves them all. A fixed table: nothing is allocated.
struct KeptWalker {
    u64 hash;
    const u8* code;
    u64 size;
};
static std::array<KeptWalker, 16384> g_srt_walkers{};

static u64 WalkerHash(const u8* code, size_t size) {
    return xxh3_64(code, size, 0) | 1; // 0 is an empty slot
}

/// The kept walker with this code, or null after keeping this one (when there is room).
static const u8* FindOrKeepWalker(const u8* code, size_t size) {
    const u64 hash = WalkerHash(code, size);
    for (u64 probe = 0; probe < 32; ++probe) {
        KeptWalker& kept = g_srt_walkers[(hash + probe) % g_srt_walkers.size()];
        if (kept.hash == 0) {
            kept = {hash, code, size};
            return nullptr;
        }
        // One the fault handler has patched no longer has its code: not reused
        if (kept.hash == hash && kept.size == size && kept.code != code &&
            std::memcmp(kept.code, code, size) == 0) {
            return kept.code;
        }
    }
    return nullptr;
}

PFN_SrtWalker RegisterWalkerCode(const u8* ptr, size_t size) {
    x64_code_t* c = SrtCode();
    const size_t start = c->size;
    const auto func_addr = (PFN_SrtWalker)x64_curr(c);
    x64_db(c, ptr, size);
    ASSERT_MSG(!c->error, "SRT walker code is too big");
    if (const u8* kept = FindOrKeepWalker(reinterpret_cast<const u8*>(func_addr), size)) {
        c->size = start;
        return reinterpret_cast<PFN_SrtWalker>(const_cast<u8*>(kept));
    }
    return func_addr;
}

} // namespace Shader

namespace {

static void DumpSrtProgram(const Shader::Info& info, const u8* code, size_t codesize) {
    using namespace Common::FS;

    const auto dump_dir = GetUserPath(PathType::ShaderDir) / "dumps";
    if (!Common::FS::Exists(dump_dir)) {
        Common::FS::CreateDirectories(dump_dir);
    }
    const auto filename = Common::StringFromFormat("%s_0x%016" PRIx64 ".srtprogram.txt",
                                                   Shader::NameOf(info.hw_stage), info.pgm_hash);
    const auto file = IOFile{dump_dir / filename, FileAccessMode::Create};

    const u8* const end = code + codesize;
    for (const u8* ip = code; ip < end;) {
        x64_insn_t instruction;
        x64_operand_t operands[X64_MAX_OPERANDS];
        char text[256];
        const unsigned length = x64_decode(ip, static_cast<size_t>(end - ip), &instruction);
        if (!length) {
            break;
        }
        x64_operands(&instruction, operands);
        // The text and its line end, cut short as the buffer is
        size_t n =
            x64_format(&instruction, operands, reinterpret_cast<u64>(ip), text, sizeof(text) - 1);
        n = n < sizeof(text) - 2 ? n : sizeof(text) - 2;
        text[n++] = '\n';
        file.WriteString(std::span<const char>{text, n});
        ip += length;
    }
}

static bool SrtWalkerSignalHandler(void* context, void* fault_address) {
    // Only handle if the fault address is within the SRT code range
    const u8* code_start = g_srt_codegen_start;
    const u8* code_end = g_srt_code.p + g_srt_code.size;
    const void* code = Common::GetRip(context);
    if (code < code_start || code >= code_end) {
        return false; // Not in SRT code range
    }

    // Patch instruction to zero register
    x64_insn_t instruction;
    x64_operand_t operands[X64_MAX_OPERANDS];
    const size_t len = x64_decode(static_cast<const u8*>(code), 15, &instruction);
    if (len) {
        x64_operands(&instruction, operands);
    }

    ASSERT(len != 0 && instruction.mnemonic == X64_MN_MOV &&
           operands[0].type == X64_OPERAND_REGISTER && operands[1].type == X64_OPERAND_MEMORY);

    const size_t patch_size = 3;
    u8* code_patch = const_cast<u8*>(reinterpret_cast<const u8*>(code));

    // We can only encounter rdi or r10d as the first operand in a
    // fault memory access for SRT walker.
    switch (operands[0].reg) {
    case X64_REG_RDI:
        // mov rdi, [rdi + (off_dw << 2)] -> xor rdi, rdi
        code_patch[0] = 0x48;
        code_patch[1] = 0x31;
        code_patch[2] = 0xFF;
        break;
    case X64_REG_R10D:
        // mov r10d, [rdi + (off_dw << 2)] -> xor r10d, r10d
        code_patch[0] = 0x45;
        code_patch[1] = 0x31;
        code_patch[2] = 0xD2;
        break;
    default:
        UNREACHABLE_MSG("Unsupported register for SRT walker patch");
        return false;
    }

    // Fill nops
    memset(code_patch + patch_size, 0x90, len - patch_size);

    LOG_WARNING(Render_Recompiler, "Patched SRT walker at 0x%llx, fault address 0x%llx",
                static_cast<unsigned long long>(reinterpret_cast<uintptr_t>(code)),
                static_cast<unsigned long long>(reinterpret_cast<uintptr_t>(fault_address)));

    return true;
}

using namespace Shader;

struct PassInfo {
    struct PointerListCompare {
        bool operator()(const IR::Value& a, const IR::Value& b) const {
            // Keep order with the following criteria:
            // 1. If immediate, sort by immediate value
            // 2. If not immediate, sort by instruction order
            // 3. Immediates come first, then instructions
            if (a.IsImmediate() && b.IsImmediate()) {
                return a.U32() < b.U32();
            } else if (!a.IsImmediate() && !b.IsImmediate()) {
                auto a_inst = a.Inst();
                auto b_inst = b.Inst();
                auto a_block = a_inst->GetParent();
                auto b_block = b_inst->GetParent();
                if (a_block != b_block) {
                    return a_block < b_block; // Sort by block address
                }

                auto it = a_block->Instructions().iterator_to(*a_inst);
                auto end = a_block->Instructions().end();
                for (; it != end; ++it) {
                    if (&*it == b_inst) {
                        return true;
                    }
                }
                return false;
            } else {
                return a.IsImmediate(); // Immediates come first
            }
        }
    };

    // map offset to inst
    using PtrUserList = Common::FlatMap<IR::Value, Shader::IR::Inst*, 8, PointerListCompare>;

    Optimization::SrtGvnTable gvn_table;
    // keys are GetUserData, ReadConst or ReadConstBuffer instructions that are used as pointers
    std::unordered_map<IR::Inst*, PtrUserList> pointer_uses;
    // GetUserData instructions corresponding to sgpr_base of SRT roots
    Common::FlatMap<IR::ScalarReg, IR::Inst*, 1> srt_roots;

    // pick a single inst for a given value number
    std::unordered_map<u32, IR::Inst*> vn_to_inst;

    // Bumped during codegen to assign offsets to readconsts
    u16 dst_off_dw;

    PtrUserList* GetUsesAsPointer(IR::Inst* inst) {
        auto it = pointer_uses.find(inst);
        if (it != pointer_uses.end()) {
            return &it->second;
        }
        return nullptr;
    }

    // Return a single instruction that this instruction is identical to, according
    // to value number
    // The "original" is arbitrary. Here it's the first instruction found for a given value number
    IR::Inst* DeduplicateInstruction(IR::Inst* inst) {
        auto it = vn_to_inst.try_emplace(gvn_table.GetValueNumber(inst), inst);
        return it.first->second;
    }
};
} // namespace

namespace Shader::Optimization {

namespace {

static inline u16 GetFlatbufOffset(const IR::Inst* inst) {
    if (inst->GetOpcode() == IR::Opcode::ReadConstBuffer) {
        auto inst_info = inst->Flags<IR::BufferInstInfo>();
        return inst_info.flatbuf_off_dw;
    }
    if (inst->GetOpcode() == IR::Opcode::ReadConst) {
        return inst->Flags<u16>();
    }
    UNREACHABLE_MSG("Instruction not supported");
}

static inline void SetFlatbufOffset(IR::Inst* inst, u16 offset) {
    if (inst->GetOpcode() == IR::Opcode::ReadConstBuffer) {
        auto inst_info = inst->Flags<IR::BufferInstInfo>();
        inst_info.flatbuf_off_dw.Assign(offset);
        inst->SetFlags(inst_info);
        return;
    }
    if (inst->GetOpcode() == IR::Opcode::ReadConst) {
        inst->SetFlags(offset);
        return;
    }
    UNREACHABLE_MSG("Instruction not supported");
}

static bool ComputeOffset(x64_code_t* c, int reg, PassInfo& pass_info, const IR::Value& off_dw);

#define ABORT_ON_FAILURE(expr)                                                                     \
    if (!(expr)) {                                                                                 \
        return false;                                                                              \
    }

#define POP_ABORT_ON_FAILURE(expr)                                                                 \
    if (!(expr)) {                                                                                 \
        x64_alu_ri(c, X64_ADD, 64, X64_RSP, 8);                                                    \
        return false;                                                                              \
    }

static bool EmitComputeOffsetIAdd32(x64_code_t* c, int reg, PassInfo& pass_info, IR::Inst* inst) {
    ASSERT(!inst->AreAllArgsImmediates());
    if (inst->Arg(0).IsImmediate()) {
        ABORT_ON_FAILURE(ComputeOffset(c, reg, pass_info, inst->Arg(1)));
        x64_alu_ri(c, X64_ADD, 32, reg, inst->Arg(0).U32());
    } else if (inst->Arg(1).IsImmediate()) {
        ABORT_ON_FAILURE(ComputeOffset(c, reg, pass_info, inst->Arg(0)));
        x64_alu_ri(c, X64_ADD, 32, reg, inst->Arg(1).U32());
    } else {
        ABORT_ON_FAILURE(ComputeOffset(c, reg, pass_info, inst->Arg(0)));
        x64_push(c, reg);
        POP_ABORT_ON_FAILURE(ComputeOffset(c, reg, pass_info, inst->Arg(1)));
        x64_alu_rm(c, X64_ADD, 32, reg, x64_m(X64_RSP, 0));
        x64_alu_ri(c, X64_ADD, 64, X64_RSP, 8);
    }
    return true;
}

static bool EmitComputeOffsetISub32(x64_code_t* c, int reg, PassInfo& pass_info, IR::Inst* inst) {
    ASSERT(!inst->AreAllArgsImmediates());
    if (inst->Arg(0).IsImmediate()) {
        ABORT_ON_FAILURE(ComputeOffset(c, reg, pass_info, inst->Arg(1)));
        x64_neg(c, 32, reg);
        x64_alu_ri(c, X64_ADD, 32, reg, inst->Arg(0).U32());
    } else if (inst->Arg(1).IsImmediate()) {
        ABORT_ON_FAILURE(ComputeOffset(c, reg, pass_info, inst->Arg(0)));
        x64_alu_ri(c, X64_SUB, 32, reg, inst->Arg(1).U32());
    } else {
        ABORT_ON_FAILURE(ComputeOffset(c, reg, pass_info, inst->Arg(0)));
        x64_push(c, reg);
        POP_ABORT_ON_FAILURE(ComputeOffset(c, reg, pass_info, inst->Arg(1)));
        x64_neg(c, 32, reg);
        x64_alu_rm(c, X64_ADD, 32, reg, x64_m(X64_RSP, 0));
        x64_alu_ri(c, X64_ADD, 64, X64_RSP, 8);
    }
    return true;
}

static bool EmitComputeOffsetIMul32(x64_code_t* c, int reg, PassInfo& pass_info, IR::Inst* inst) {
    ASSERT(!inst->AreAllArgsImmediates());
    if (inst->Arg(0).IsImmediate()) {
        ABORT_ON_FAILURE(ComputeOffset(c, reg, pass_info, inst->Arg(1)));
        x64_imul_rri(c, 32, reg, reg, inst->Arg(0).U32());
    } else if (inst->Arg(1).IsImmediate()) {
        ABORT_ON_FAILURE(ComputeOffset(c, reg, pass_info, inst->Arg(0)));
        x64_imul_rri(c, 32, reg, reg, inst->Arg(1).U32());
    } else {
        ABORT_ON_FAILURE(ComputeOffset(c, reg, pass_info, inst->Arg(0)));
        x64_push(c, reg);
        POP_ABORT_ON_FAILURE(ComputeOffset(c, reg, pass_info, inst->Arg(1)));
        x64_imul_rm(c, 32, reg, x64_m(X64_RSP, 0));
        x64_alu_ri(c, X64_ADD, 64, X64_RSP, 8);
    }
    return true;
}

static bool EmitComputeOffsetShiftLeftLogical32(x64_code_t* c, int reg, PassInfo& pass_info,
                                                IR::Inst* inst) {
    ASSERT(!inst->AreAllArgsImmediates());
    if (inst->Arg(0).IsImmediate()) {
        ABORT_ON_FAILURE(ComputeOffset(c, reg, pass_info, inst->Arg(1)));
        x64_shift_ri(c, X64_SHL, 32, reg, static_cast<u8>(inst->Arg(0).U32()));
        x64_mov_rr(c, 32, X64_RCX, reg);
        x64_mov_ri(c, 32, reg, inst->Arg(0).U32());
        x64_shift_rcl(c, X64_SHL, 32, reg);
    } else if (inst->Arg(1).IsImmediate()) {
        ABORT_ON_FAILURE(ComputeOffset(c, reg, pass_info, inst->Arg(0)));
        x64_shift_ri(c, X64_SHL, 32, reg, static_cast<u8>(inst->Arg(1).U32()));
    } else {
        ABORT_ON_FAILURE(ComputeOffset(c, reg, pass_info, inst->Arg(1)));
        x64_push(c, reg);
        POP_ABORT_ON_FAILURE(ComputeOffset(c, reg, pass_info, inst->Arg(0)));
        x64_pop(c, X64_RCX);
        x64_shift_rcl(c, X64_SHL, 32, reg);
    }
    return true;
}

static bool EmitComputeOffsetShiftRightLogical32(x64_code_t* c, int reg, PassInfo& pass_info,
                                                 IR::Inst* inst) {
    ASSERT(!inst->AreAllArgsImmediates());
    if (inst->Arg(0).IsImmediate()) {
        ABORT_ON_FAILURE(ComputeOffset(c, reg, pass_info, inst->Arg(1)));
        x64_mov_rr(c, 32, X64_RCX, reg);
        x64_mov_ri(c, 32, reg, inst->Arg(0).U32());
        x64_shift_rcl(c, X64_SHR, 32, reg);
    } else if (inst->Arg(1).IsImmediate()) {
        ABORT_ON_FAILURE(ComputeOffset(c, reg, pass_info, inst->Arg(0)));
        x64_shift_ri(c, X64_SHR, 32, reg, static_cast<u8>(inst->Arg(1).U32()));
    } else {
        ABORT_ON_FAILURE(ComputeOffset(c, reg, pass_info, inst->Arg(1)));
        x64_push(c, reg);
        POP_ABORT_ON_FAILURE(ComputeOffset(c, reg, pass_info, inst->Arg(0)));
        x64_pop(c, X64_RCX);
        x64_shift_rcl(c, X64_SHR, 32, reg);
    }
    return true;
}

static bool EmitComputeOffsetBitwiseAnd32(x64_code_t* c, int reg, PassInfo& pass_info,
                                          IR::Inst* inst) {
    ASSERT(!inst->AreAllArgsImmediates());
    if (inst->Arg(0).IsImmediate()) {
        ABORT_ON_FAILURE(ComputeOffset(c, reg, pass_info, inst->Arg(1)));
        x64_alu_ri(c, X64_AND, 32, reg, inst->Arg(0).U32());
    } else if (inst->Arg(1).IsImmediate()) {
        ABORT_ON_FAILURE(ComputeOffset(c, reg, pass_info, inst->Arg(0)));
        x64_alu_ri(c, X64_AND, 32, reg, inst->Arg(1).U32());
    } else {
        ABORT_ON_FAILURE(ComputeOffset(c, reg, pass_info, inst->Arg(0)));
        x64_push(c, reg);
        POP_ABORT_ON_FAILURE(ComputeOffset(c, reg, pass_info, inst->Arg(1)));
        x64_alu_rm(c, X64_AND, 32, reg, x64_m(X64_RSP, 0));
        x64_alu_ri(c, X64_ADD, 64, X64_RSP, 8);
    }
    return true;
}

static bool EmitComputeOffsetBitwiseOr32(x64_code_t* c, int reg, PassInfo& pass_info,
                                         IR::Inst* inst) {
    ASSERT(!inst->AreAllArgsImmediates());
    if (inst->Arg(0).IsImmediate()) {
        ABORT_ON_FAILURE(ComputeOffset(c, reg, pass_info, inst->Arg(1)));
        x64_alu_ri(c, X64_OR, 32, reg, inst->Arg(0).U32());
    } else if (inst->Arg(1).IsImmediate()) {
        ABORT_ON_FAILURE(ComputeOffset(c, reg, pass_info, inst->Arg(0)));
        x64_alu_ri(c, X64_OR, 32, reg, inst->Arg(1).U32());
    } else {
        ABORT_ON_FAILURE(ComputeOffset(c, reg, pass_info, inst->Arg(0)));
        x64_push(c, reg);
        POP_ABORT_ON_FAILURE(ComputeOffset(c, reg, pass_info, inst->Arg(1)));
        x64_alu_rm(c, X64_OR, 32, reg, x64_m(X64_RSP, 0));
        x64_alu_ri(c, X64_ADD, 64, X64_RSP, 8);
    }
    return true;
}

static bool EmitComputeOffsetBitwiseXor32(x64_code_t* c, int reg, PassInfo& pass_info,
                                          IR::Inst* inst) {
    ASSERT(!inst->AreAllArgsImmediates());
    if (inst->Arg(0).IsImmediate()) {
        ABORT_ON_FAILURE(ComputeOffset(c, reg, pass_info, inst->Arg(1)));
        x64_alu_ri(c, X64_XOR, 32, reg, inst->Arg(0).U32());
    } else if (inst->Arg(1).IsImmediate()) {
        ABORT_ON_FAILURE(ComputeOffset(c, reg, pass_info, inst->Arg(0)));
        x64_alu_ri(c, X64_XOR, 32, reg, inst->Arg(1).U32());
    } else {
        ABORT_ON_FAILURE(ComputeOffset(c, reg, pass_info, inst->Arg(0)));
        x64_push(c, reg);
        POP_ABORT_ON_FAILURE(ComputeOffset(c, reg, pass_info, inst->Arg(1)));
        x64_alu_rm(c, X64_XOR, 32, reg, x64_m(X64_RSP, 0));
        x64_alu_ri(c, X64_ADD, 64, X64_RSP, 8);
    }
    return true;
}

static bool EmitComputeOffsetBitwiseNot32(x64_code_t* c, int reg, PassInfo& pass_info,
                                          IR::Inst* inst) {
    ASSERT(!inst->AreAllArgsImmediates());
    ABORT_ON_FAILURE(ComputeOffset(c, reg, pass_info, inst->Arg(0)));
    x64_not(c, 32, reg);
    return true;
}

static bool EmitComputeOffsetUMin32(x64_code_t* c, int reg, PassInfo& pass_info, IR::Inst* inst) {
    ASSERT(!inst->AreAllArgsImmediates());
    if (inst->Arg(0).IsImmediate()) {
        ABORT_ON_FAILURE(ComputeOffset(c, reg, pass_info, inst->Arg(1)));
        x64_mov_ri(c, 32, X64_RCX, inst->Arg(0).U32());
        x64_alu_rr(c, X64_CMP, 32, reg, X64_RCX);
        x64_cmov_rr(c, X64_CC_A, 32, reg, X64_RCX);
    } else if (inst->Arg(1).IsImmediate()) {
        ABORT_ON_FAILURE(ComputeOffset(c, reg, pass_info, inst->Arg(0)));
        x64_mov_ri(c, 32, X64_RCX, inst->Arg(1).U32());
        x64_alu_rr(c, X64_CMP, 32, reg, X64_RCX);
        x64_cmov_rr(c, X64_CC_A, 32, reg, X64_RCX);
    } else {
        ABORT_ON_FAILURE(ComputeOffset(c, reg, pass_info, inst->Arg(0)));
        x64_push(c, reg);
        POP_ABORT_ON_FAILURE(ComputeOffset(c, reg, pass_info, inst->Arg(1)));
        x64_alu_rm(c, X64_CMP, 32, reg, x64_m(X64_RSP, 0));
        x64_cmov_rm(c, X64_CC_A, 32, reg, x64_m(X64_RSP, 0));
        x64_alu_ri(c, X64_ADD, 64, X64_RSP, 8);
    }
    return true;
}

static bool EmitComputeOffsetUMax32(x64_code_t* c, int reg, PassInfo& pass_info, IR::Inst* inst) {
    ASSERT(!inst->AreAllArgsImmediates());
    if (inst->Arg(0).IsImmediate()) {
        ABORT_ON_FAILURE(ComputeOffset(c, reg, pass_info, inst->Arg(1)));
        x64_mov_ri(c, 32, X64_RCX, inst->Arg(0).U32());
        x64_alu_rr(c, X64_CMP, 32, reg, X64_RCX);
        x64_cmov_rr(c, X64_CC_B, 32, reg, X64_RCX);
    } else if (inst->Arg(1).IsImmediate()) {
        ABORT_ON_FAILURE(ComputeOffset(c, reg, pass_info, inst->Arg(0)));
        x64_mov_ri(c, 32, X64_RCX, inst->Arg(1).U32());
        x64_alu_rr(c, X64_CMP, 32, reg, X64_RCX);
        x64_cmov_rr(c, X64_CC_B, 32, reg, X64_RCX);
    } else {
        ABORT_ON_FAILURE(ComputeOffset(c, reg, pass_info, inst->Arg(0)));
        x64_push(c, reg);
        POP_ABORT_ON_FAILURE(ComputeOffset(c, reg, pass_info, inst->Arg(1)));
        x64_alu_rm(c, X64_CMP, 32, reg, x64_m(X64_RSP, 0));
        x64_cmov_rm(c, X64_CC_B, 32, reg, x64_m(X64_RSP, 0));
        x64_alu_ri(c, X64_ADD, 64, X64_RSP, 8);
    }
    return true;
}

static bool EmitComputeOffsetBitFieldUExtract(x64_code_t* c, int reg, PassInfo& pass_info,
                                              IR::Inst* inst) {
    // We asume that the count is always less than 32, Is this correct?
    ASSERT(!inst->AreAllArgsImmediates());
    if (inst->Arg(0).IsImmediate()) {
        x64_mov_ri(c, 32, reg, inst->Arg(0).U32());
    } else {
        ABORT_ON_FAILURE(ComputeOffset(c, reg, pass_info, inst->Arg(0)));
    }
    bool in_stack = false;
    if (inst->Arg(1).IsImmediate()) {
        x64_shift_ri(c, X64_SHR, 32, reg, static_cast<u8>(inst->Arg(1).U32()));
    } else {
        x64_push(c, reg);
        in_stack = true;
        POP_ABORT_ON_FAILURE(ComputeOffset(c, reg, pass_info, inst->Arg(1)));
        x64_mov_rr(c, 32, X64_RCX, reg);
        x64_shift_mcl(c, X64_SHR, 32, x64_m(X64_RSP, 0));
    }
    if (inst->Arg(2).IsImmediate()) {
        if (in_stack) {
            x64_pop(c, reg);
        }
        x64_alu_ri(c, X64_AND, 32, reg, (1U << inst->Arg(2).U32()) - 1);
    } else {
        if (!in_stack) {
            x64_push(c, reg);
        }
        POP_ABORT_ON_FAILURE(ComputeOffset(c, reg, pass_info, inst->Arg(2)));
        x64_mov_rr(c, 32, X64_RCX, reg);
        x64_mov_ri(c, 32, X64_RDX, 1);
        x64_shift_rcl(c, X64_SHL, 32, X64_RDX);
        x64_dec(c, 32, X64_RDX);
        x64_alu_mr(c, X64_AND, 32, x64_m(X64_RSP, 0), X64_RDX);
        x64_pop(c, reg);
    }
    return true;
}

static bool IsAllowedOffsetInstruction(const IR::Inst* inst) {
    switch (inst->GetOpcode()) {
    case IR::Opcode::GetUserData:
    case IR::Opcode::ReadConst:
    case IR::Opcode::ReadConstBuffer:
    case IR::Opcode::IAdd32:
    case IR::Opcode::ISub32:
    case IR::Opcode::IMul32:
    case IR::Opcode::ShiftLeftLogical32:
    case IR::Opcode::ShiftRightLogical32:
    case IR::Opcode::BitwiseAnd32:
    case IR::Opcode::BitwiseOr32:
    case IR::Opcode::BitwiseXor32:
    case IR::Opcode::BitwiseNot32:
    case IR::Opcode::UMin32:
    case IR::Opcode::UMax32:
    case IR::Opcode::BitFieldUExtract:
        return true;
    default:
        return false;
    }
}

static bool ComputeOffset(x64_code_t* c, int reg, PassInfo& pass_info, const IR::Value& off_dw) {
    ASSERT(reg != X64_RCX && reg != X64_RDX);
    auto inst = off_dw.Inst();
    switch (inst->GetOpcode()) {
    case IR::Opcode::GetUserData:
        x64_mov_rm(c, 32, reg, x64_m(X64_RSI, (static_cast<u32>(inst->Arg(0).ScalarReg()) << 2)));
        return true;
    case IR::Opcode::ReadConst:
    case IR::Opcode::ReadConstBuffer:
        if (u16 offset = GetFlatbufOffset(pass_info.DeduplicateInstruction(inst)); offset != 0) {
            x64_mov_rm(c, 32, reg, x64_m(X64_RSI, (offset << 2)));
            return true;
        }
        return false;
    case IR::Opcode::IAdd32:
        ABORT_ON_FAILURE(EmitComputeOffsetIAdd32(c, reg, pass_info, inst));
        return true;
    case IR::Opcode::ISub32:
        ABORT_ON_FAILURE(EmitComputeOffsetISub32(c, reg, pass_info, inst));
        return true;
    case IR::Opcode::IMul32:
        ABORT_ON_FAILURE(EmitComputeOffsetIMul32(c, reg, pass_info, inst));
        return true;
    case IR::Opcode::ShiftLeftLogical32:
        ABORT_ON_FAILURE(EmitComputeOffsetShiftLeftLogical32(c, reg, pass_info, inst));
        return true;
    case IR::Opcode::ShiftRightLogical32:
        ABORT_ON_FAILURE(EmitComputeOffsetShiftRightLogical32(c, reg, pass_info, inst));
        return true;
    case IR::Opcode::BitwiseAnd32:
        ABORT_ON_FAILURE(EmitComputeOffsetBitwiseAnd32(c, reg, pass_info, inst));
        return true;
    case IR::Opcode::BitwiseOr32:
        ABORT_ON_FAILURE(EmitComputeOffsetBitwiseOr32(c, reg, pass_info, inst));
        return true;
    case IR::Opcode::BitwiseXor32:
        ABORT_ON_FAILURE(EmitComputeOffsetBitwiseXor32(c, reg, pass_info, inst));
        return true;
    case IR::Opcode::BitwiseNot32:
        ABORT_ON_FAILURE(EmitComputeOffsetBitwiseNot32(c, reg, pass_info, inst));
        return true;
    case IR::Opcode::UMin32:
        ABORT_ON_FAILURE(EmitComputeOffsetUMin32(c, reg, pass_info, inst));
        return true;
    case IR::Opcode::UMax32:
        ABORT_ON_FAILURE(EmitComputeOffsetUMax32(c, reg, pass_info, inst));
        return true;
    case IR::Opcode::BitFieldUExtract:
        ABORT_ON_FAILURE(EmitComputeOffsetBitFieldUExtract(c, reg, pass_info, inst));
        return true;
    default:
        LOG_ERROR(Render_Recompiler, "Unexpected instruction for offset computation, %.*s",
                  static_cast<int>((Shader::IR::NameOf(inst->GetOpcode())).size()),
                  (Shader::IR::NameOf(inst->GetOpcode())).data());
        return false;
    }
}

static inline bool PushPtr(x64_code_t* c, PassInfo& pass_info, const IR::Value& off_dw) {
    x64_push(c, X64_RDI);
    if (off_dw.IsImmediate()) {
        x64_mov_rm(c, 64, X64_RDI, x64_m(X64_RDI, (off_dw.U32() << 2)));
    } else {
        ABORT_ON_FAILURE(ComputeOffset(c, X64_R10, pass_info, off_dw));
        x64_shift_ri(c, X64_SHL, 32, X64_R10, static_cast<u8>(2));
        x64_mov_rr(c, 32, X64_R10, X64_R10);
        x64_mov_rm(c, 64, X64_RDI, x64_mi(X64_RDI, X64_R10, 1, 0));
    }
    x64_mov_ri(c, 64, X64_R10, 0xFFFFFFFFFFFFULL);
    x64_alu_rr(c, X64_AND, 64, X64_RDI, X64_R10);
    return true;
}

static inline void PopPtr(x64_code_t* c) {
    x64_pop(c, X64_RDI);
}

static void VisitPointer(const IR::Value& off_dw, IR::Inst* subtree, PassInfo& pass_info,
                         x64_code_t* c) {
    if (subtree->GetOpcode() == IR::Opcode::ReadConst && subtree->Flags<u16>() == 0 ||
        subtree->GetOpcode() == IR::Opcode::ReadConstBuffer &&
            subtree->Flags<IR::BufferInstInfo>().flatbuf_off_dw == 0) {
        return;
    }

    if (!PushPtr(c, pass_info, off_dw)) {
        LOG_ERROR(Render_Recompiler, "Failed to compute offset for SRT walker");
        return;
    }
    PassInfo::PtrUserList* use_list = pass_info.GetUsesAsPointer(subtree);
    ASSERT(use_list);

    // First copy all the src data from this tree level
    // That way, all data that was contiguous in the guest SRT is also contiguous in the
    // flattened buffer.
    // TODO src and dst are contiguous. Optimize with wider loads/stores
    // TODO if this subtree is dynamically indexed, don't compact it (keep it sparse)
    for (auto [src_off_dw, use] : *use_list) {
        if (src_off_dw.IsImmediate()) {
            x64_mov_rm(c, 32, X64_R10, x64_m(X64_RDI, (src_off_dw.U32() << 2)));
        } else {
            if (!ComputeOffset(c, X64_R10, pass_info, src_off_dw)) {
                LOG_ERROR(Render_Recompiler, "Failed to compute offset for SRT walker");
                continue;
            }
            x64_shift_ri(c, X64_SHL, 32, X64_R10, static_cast<u8>(2));
            x64_mov_rr(c, 32, X64_R10, X64_R10);
            x64_mov_rm(c, 32, X64_R10, x64_mi(X64_RDI, X64_R10, 1, 0));
        }
        x64_mov_mr(c, 32, x64_m(X64_RSI, (pass_info.dst_off_dw << 2)), X64_R10);

        SetFlatbufOffset(use, pass_info.dst_off_dw);
        pass_info.dst_off_dw++;
    }

    // Then visit any children used as pointers
    for (const auto [src_off_dw, use] : *use_list) {
        if (pass_info.GetUsesAsPointer(use)) {
            VisitPointer(src_off_dw, use, pass_info, c);
        }
    }

    PopPtr(c);
}

static void GenerateSrtProgram(Info& info, PassInfo& pass_info) {
    x64_code_t* c = SrtCode();

    if (pass_info.srt_roots.empty()) {
        return;
    }

    // Register the signal handler for SRT walker, if not already registered
#ifdef ARCH_X86_64
    if (g_srt_codegen_start == nullptr) {
#else
    // The interpreter reads a bad guest pointer as 0 itself; the x86-64 code is not run
    if (false) {
#endif
        g_srt_codegen_start = x64_curr(c);
        auto* signals = Core::Signals::Instance();
        // Call after the memory invalidation handler
        constexpr u32 priority = 1;
        signals->RegisterAccessViolationHandler(SrtWalkerSignalHandler, priority);
    }

    const size_t walker_start = c->size;
    info.srt_info.walker_func = reinterpret_cast<PFN_SrtWalker>(x64_curr(c));
    pass_info.dst_off_dw = NUM_USER_DATA_REGS;
    ASSERT(pass_info.dst_off_dw == info.srt_info.flattened_bufsize_dw);

    for (const auto& [sgpr_base, root] : pass_info.srt_roots) {
        VisitPointer(IR::Value(static_cast<u32>(sgpr_base)), root, pass_info, c);
    }

    x64_ret(c);
    ASSERT_MSG(!c->error, "SRT walker code is too big");

    info.srt_info.walker_func_size =
        x64_curr(c) - reinterpret_cast<const u8*>(info.srt_info.walker_func);
    // The walker as made, for the pipeline cache: the fault handler rewrites the code it runs
    // where a pointer was bad at that moment, and that rewrite must not be kept for later starts
    {
        const u8* const code = reinterpret_cast<const u8*>(info.srt_info.walker_func);
        info.srt_walker_code.assign(code, code + info.srt_info.walker_func_size);
    }
    // Another permutation's identical walker is used, and this copy is dropped
    if (const u8* kept = FindOrKeepWalker(reinterpret_cast<const u8*>(info.srt_info.walker_func),
                                          info.srt_info.walker_func_size)) {
        c->size = walker_start;
        info.srt_info.walker_func = reinterpret_cast<PFN_SrtWalker>(const_cast<u8*>(kept));
    }

    if (emu_settings_get_dump_shaders()) {
        DumpSrtProgram(info, reinterpret_cast<const u8*>(info.srt_info.walker_func),
                       info.srt_info.walker_func_size);
    }

    info.srt_info.flattened_bufsize_dw = pass_info.dst_off_dw;
}

static bool IsReadConstSource(const IR::Value base) {
    auto* inst = base.TryInst();
    if (!inst) {
        return false;
    }
    return inst->GetOpcode() == IR::Opcode::GetUserData ||
           inst->GetOpcode() == IR::Opcode::ReadConst ||
           inst->GetOpcode() == IR::Opcode::ReadConstBuffer;
}

void SimplifyReadConstAddressAdd(IR::Inst& inst) {
    // This handles the following pattern by combining the addition with the offset
    // s_add_u32    s12, s2, <offset>
    // s_addc_u32   s13, s3, 0
    // s_load_dword s16, s[12:13], 0x0

    if (inst.GetOpcode() != IR::Opcode::ReadConst) {
        return;
    }
    IR::Inst* addr = inst.Arg(0).Inst();
    IR::Inst* hi = addr->Arg(1).TryInst();
    IR::Inst* lo = addr->Arg(0).TryInst();
    if (!hi || !lo) {
        return;
    }

    if (lo->GetOpcode() != IR::Opcode::IAdd32 || hi->GetOpcode() != IR::Opcode::IAdd32) {
        return;
    }

    IR::Value add_offset;
    IR::Value base;
    if (base = lo->Arg(0); IsReadConstSource(base)) {
        add_offset = lo->Arg(1);
    } else if (base = lo->Arg(1); IsReadConstSource(base)) {
        add_offset = lo->Arg(0);
    } else {
        return;
    }

    IR::Inst* sel = hi->Arg(1).TryInst();
    if (!sel || sel->GetOpcode() != IR::Opcode::SelectU32) {
        return;
    }

    IR::Value sel_true = sel->Arg(1);
    IR::Value sel_false = sel->Arg(2);
    if (!sel_true.IsImmediate() || !sel_false.IsImmediate() || sel_true.U32() != 1 ||
        sel_false.U32() != 0) {
        return;
    }

    addr->SetArg(0, base);
    addr->SetArg(1, hi->Arg(0));
    for (auto [user, operand] : addr->Uses()) {
        ASSERT(user->GetOpcode() == IR::Opcode::ReadConst && operand == 0);
        IR::IREmitter ir{*user->GetParent(), IR::Block::InstructionList::s_iterator_to(*user)};
        IR::U32 offset = IR::U32{user->Arg(1)};
        IR::U32 dw_add_offset =
            add_offset.IsImmediate()
                ? ir.Imm32(add_offset.U32() >> 2u)
                : IR::U32{ir.ShiftRightLogical(IR::U32{add_offset}, ir.Imm32(2u))};
        if (offset.IsImmediate() && dw_add_offset.IsImmediate()) {
            user->SetArg(1, ir.Imm32(offset.U32() + dw_add_offset.U32()));
        } else {
            user->SetArg(1, ir.IAdd(offset, dw_add_offset));
        }
    }
}

} // Anonymous namespace

void FlattenExtendedUserdataPass(IR::Program& program) {
    auto& post_order = program.post_order_blocks;
    PassInfo pass_info;

    // traverse at end and assign offsets to duplicate readconsts, using
    // vn_to_inst as the source
    Common::SmallVector<IR::Inst*, 32> all_readconsts, visited;
    std::queue<IR::Inst*> queue;

    for (auto it = post_order.rbegin(); it != post_order.rend(); it++) {
        IR::Block* block{*it};
        for (IR::Inst& inst : *block) {
            if (inst.GetOpcode() != IR::Opcode::ReadConst &&
                inst.GetOpcode() != IR::Opcode::ReadConstBuffer) {
                continue;
            }

            if (inst.GetOpcode() == IR::Opcode::ReadConstBuffer) {
                // Only flatten ReadConstBuffer if it was marked as a sharp source in the
                // resource discovery pass
                auto inst_info = inst.Flags<IR::BufferInstInfo>();
                if (!inst_info.sharp_source) {
                    continue;
                }
            }

            SimplifyReadConstAddressAdd(inst);

            all_readconsts.push_back(&inst);

            auto offset = inst.Arg(1);
            if (offset.IsImmediate()) {
                continue;
            }

            queue.push(offset.Inst());
            while (!queue.empty()) {
                IR::Inst* inst{queue.front()};
                queue.pop();

                if (inst->GetOpcode() == IR::Opcode::ReadConstBuffer) {
                    auto buffer_inst_info = inst->Flags<IR::BufferInstInfo>();
                    if (!buffer_inst_info.sharp_source.Value()) {
                        all_readconsts.push_back(inst);
                        auto offset = inst->Arg(1);
                        if (!offset.IsImmediate()) {
                            auto arg_inst = offset.Inst();
                            if (std::ranges::find(visited, arg_inst) == visited.end()) {
                                visited.push_back(arg_inst);
                                queue.push(arg_inst);
                            }
                        }
                    }
                    continue;
                }
                if (inst->GetOpcode() == IR::Opcode::GetUserData ||
                    inst->GetOpcode() == IR::Opcode::ReadConst ||
                    !IsAllowedOffsetInstruction(inst)) {
                    continue;
                }
                for (size_t arg = inst->NumArgs(); arg--;) {
                    const IR::Value arg_value = inst->Arg(arg);
                    if (arg_value.IsImmediate()) {
                        continue;
                    }
                    IR::Inst* arg_inst = arg_value.Inst();
                    if (std::ranges::find(visited, arg_inst) == visited.end()) {
                        visited.push_back(arg_inst);
                        queue.push(arg_inst);
                    }
                }
            }
        }
    }

    for (auto inst : all_readconsts) {
        if (pass_info.DeduplicateInstruction(inst) != inst) {
            // This is a duplicate of a readconst we've already visited
            continue;
        }

        IR::Inst* base = inst->Arg(0).Inst();
        if (auto* inst = base->Arg(0).TryInst();
            inst && inst->GetOpcode() == IR::Opcode::ReadFirstLane) {
            continue;
        }
        ASSERT_MSG(IsReadConstSource(base->Arg(0)), "ReadConst base low not from constant memory");
        ASSERT_MSG(IsReadConstSource(base->Arg(1)), "ReadConst base high not from constant memory");

        IR::Inst* ptr_lo = base->Arg(0).Inst();
        ptr_lo = pass_info.DeduplicateInstruction(ptr_lo);

        auto ptr_uses_kv = pass_info.pointer_uses.try_emplace(ptr_lo, PassInfo::PtrUserList{});
        PassInfo::PtrUserList& user_list = ptr_uses_kv.first->second;

        user_list[inst->Arg(1)] = inst;

        if (ptr_lo->GetOpcode() == IR::Opcode::GetUserData) {
            IR::ScalarReg ud_reg = ptr_lo->Arg(0).ScalarReg();
            pass_info.srt_roots[ud_reg] = ptr_lo;
        }
    }

    GenerateSrtProgram(program.info, pass_info);

    // Assign offsets to duplicate readconsts
    for (IR::Inst* readconst : all_readconsts) {
        ASSERT(pass_info.vn_to_inst.contains(pass_info.gvn_table.GetValueNumber(readconst)));
        IR::Inst* original = pass_info.DeduplicateInstruction(readconst);
        SetFlatbufOffset(readconst, GetFlatbufOffset(original));
    }

    program.info.RefreshFlatBuf();
}

} // namespace Shader::Optimization

#else

namespace Shader {

PFN_SrtWalker RegisterWalkerCode(const u8* ptr, size_t size) {
    UNREACHABLE_MSG("RegisterWalkerCode unimplemented for target architecture.");
}

namespace Optimization {

void FlattenExtendedUserdataPass(IR::Program& program) {
    UNREACHABLE_MSG("FlattenExtendedUserdataPass unimplemented for target architecture.");
}

} // namespace Optimization

} // namespace Shader

#endif
