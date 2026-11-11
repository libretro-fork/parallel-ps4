// SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

// The SRT walkers on a host that is not x86-64. FlattenExtendedUserdataPass emits them as x86-64
// code with Xbyak on every host, so a pipeline cache holds the same walkers whatever machine
// wrote it; here they are decoded once with Zydis and run by a small interpreter of the few
// instructions the pass emits. A load from a bad guest pointer reads 0, as the x86-64 walker's
// fault handler makes it.

#include "common/arch.h"
#include "common/combiner.h"

#if defined(__aarch64__) && defined(__linux__)

#include <array>
#include <cstring>
#include <mutex>
#include <shared_mutex>
#include <unordered_map>
#include <vector>
#include <ucontext.h>
#include "common/assert.h"
#include "common/decoder.h"
#include "common/logging/log.h"
#include "core/signals.h"
#include "shader_recompiler/ir/passes/srt.h"

// Loads that may fault on a guest pointer: the access violation handler below returns 0 from
// them in place of the fault
extern "C" u32 SrtSafeLoad32(u64 address);
extern "C" u64 SrtSafeLoad64(u64 address);
asm(R"(
    .text
    .p2align 2
    .globl SrtSafeLoad32
    .type SrtSafeLoad32, %function
SrtSafeLoad32:
    ldr w0, [x0]
    ret
    .size SrtSafeLoad32, .-SrtSafeLoad32
    .globl SrtSafeLoad64
    .type SrtSafeLoad64, %function
SrtSafeLoad64:
    ldr x0, [x0]
    ret
    .size SrtSafeLoad64, .-SrtSafeLoad64
)");

namespace Shader {

namespace {

enum class OperandKind : u8 { None, Register, Memory, Immediate };

struct Operand {
    OperandKind kind{};
    u8 size{}; // bits
    s8 reg{-1};
    s8 base{-1};
    s8 index{-1};
    u8 scale{};
    s64 disp{};
    u64 imm{};
};

struct Op {
    ZydisMnemonic mnemonic{};
    u8 count{};
    std::array<Operand, 3> operands{};
};

using Program = std::vector<Op>;

constexpr s8 RegRsp = 4;
constexpr s8 RegRsi = 6;
constexpr s8 RegRdi = 7;

s8 GprIndex(ZydisRegister reg) {
    if (reg == ZYDIS_REGISTER_NONE) {
        return -1;
    }
    const ZydisRegister full = ZydisRegisterGetLargestEnclosing(ZYDIS_MACHINE_MODE_LONG_64, reg);
    ASSERT_MSG(full >= ZYDIS_REGISTER_RAX && full <= ZYDIS_REGISTER_R15,
               "SRT walker uses a register that is not a GPR");
    return static_cast<s8>(full - ZYDIS_REGISTER_RAX);
}

Program Decode(const u8* code) {
    Program program;
    auto* decoder = Common::Decoder::Instance();
    for (const u8* ip = code;;) {
        ZydisDecodedInstruction instruction;
        ZydisDecodedOperand operands[ZYDIS_MAX_OPERAND_COUNT];
        const auto status =
            decoder->decodeInstruction(instruction, operands, const_cast<u8*>(ip));
        ASSERT_MSG(ZYAN_SUCCESS(status), "SRT walker code could not be decoded");
        Op op{};
        op.mnemonic = instruction.mnemonic;
        op.count = instruction.operand_count_visible;
        ASSERT(op.count <= op.operands.size());
        for (u8 i = 0; i < op.count; ++i) {
            const auto& source = operands[i];
            auto& operand = op.operands[i];
            operand.size = static_cast<u8>(source.size);
            switch (source.type) {
            case ZYDIS_OPERAND_TYPE_REGISTER:
                operand.kind = OperandKind::Register;
                operand.reg = GprIndex(source.reg.value);
                break;
            case ZYDIS_OPERAND_TYPE_MEMORY:
                operand.kind = OperandKind::Memory;
                operand.base = GprIndex(source.mem.base);
                operand.index = GprIndex(source.mem.index);
                operand.scale = source.mem.scale;
                operand.disp = source.mem.disp.value;
                break;
            case ZYDIS_OPERAND_TYPE_IMMEDIATE:
                operand.kind = OperandKind::Immediate;
                operand.imm = source.imm.is_signed ? static_cast<u64>(source.imm.value.s)
                                                   : source.imm.value.u;
                break;
            default:
                UNREACHABLE_MSG("SRT walker operand type {}", static_cast<int>(source.type));
            }
        }
        program.push_back(op);
        if (instruction.mnemonic == ZYDIS_MNEMONIC_RET) {
            return program;
        }
        ip += instruction.length;
    }
}

Common::Combiner programs_mutex;
std::unordered_map<const void*, Program> programs;

const Program& GetProgram(const void* code) {
    if (const Program* found = programs_mutex.Run([&]() -> const Program* {
            const auto it = programs.find(code);
            return it != programs.end() ? &it->second : nullptr;
        })) {
        return *found;
    }
    Program program = Decode(static_cast<const u8*>(code));
    return programs_mutex.Run([&]() -> const Program& {
        return programs.try_emplace(code, std::move(program)).first->second;
    });
}

bool SafeLoadFaultHandler(void* context, void*) {
    auto* uc = static_cast<ucontext_t*>(context);
    const auto pc = static_cast<u64>(uc->uc_mcontext.pc);
    if (pc != reinterpret_cast<u64>(&SrtSafeLoad32) &&
        pc != reinterpret_cast<u64>(&SrtSafeLoad64)) {
        return false;
    }
    uc->uc_mcontext.regs[0] = 0;
    uc->uc_mcontext.pc = uc->uc_mcontext.regs[30];
    return true;
}

struct Machine {
    std::array<u64, 16> gpr{};
    alignas(16) std::array<u64, 64> stack{};
    u64 cmp_left{};
    u64 cmp_right{};

    static u64 Mask(u64 value, u8 size) {
        return size >= 64 ? value : value & ((u64{1} << size) - 1);
    }

    u64 Address(const Operand& operand) const {
        u64 address = static_cast<u64>(operand.disp);
        if (operand.base >= 0) {
            address += gpr[operand.base];
        }
        if (operand.index >= 0) {
            address += gpr[operand.index] * operand.scale;
        }
        return address;
    }

    bool IsHostMemory(const Operand& operand) const {
        // The walker's own stack and the flattened buffer it writes are host memory; what it
        // reads through the user data is the guest's, and may be a bad pointer
        return operand.base == RegRsp || operand.base == RegRsi;
    }

    // size: the width of the operation, to which an immediate is sign-extended
    u64 Read(const Operand& operand, u8 size) const {
        switch (operand.kind) {
        case OperandKind::Register:
            return Mask(gpr[operand.reg], operand.size);
        case OperandKind::Immediate:
            return Mask(operand.imm, size);
        case OperandKind::Memory: {
            const u64 address = Address(operand);
            if (IsHostMemory(operand)) {
                if (operand.size == 64) {
                    u64 value;
                    std::memcpy(&value, reinterpret_cast<const void*>(address), sizeof(value));
                    return value;
                }
                u32 value;
                std::memcpy(&value, reinterpret_cast<const void*>(address), sizeof(value));
                return value;
            }
            return operand.size == 64 ? SrtSafeLoad64(address) : SrtSafeLoad32(address);
        }
        default:
            UNREACHABLE();
        }
    }

    void Write(const Operand& operand, u64 value) {
        switch (operand.kind) {
        case OperandKind::Register:
            // A 32-bit write clears the upper half, as on x86-64; the pass writes no narrower
            ASSERT(operand.size == 32 || operand.size == 64);
            gpr[operand.reg] = Mask(value, operand.size);
            return;
        case OperandKind::Memory: {
            const u64 address = Address(operand);
            if (operand.size == 64) {
                std::memcpy(reinterpret_cast<void*>(address), &value, sizeof(value));
            } else {
                const u32 value32 = static_cast<u32>(value);
                std::memcpy(reinterpret_cast<void*>(address), &value32, sizeof(value32));
            }
            return;
        }
        default:
            UNREACHABLE();
        }
    }

    void Run(const Program& program) {
        for (const Op& op : program) {
            const auto& a = op.operands[0];
            const auto& b = op.operands[1];
            const u8 size = a.size;
            switch (op.mnemonic) {
            case ZYDIS_MNEMONIC_MOV:
                Write(a, Read(b, size));
                break;
            case ZYDIS_MNEMONIC_ADD:
                Write(a, Read(a, size) + Read(b, size));
                break;
            case ZYDIS_MNEMONIC_SUB:
                Write(a, Read(a, size) - Read(b, size));
                break;
            case ZYDIS_MNEMONIC_AND:
                Write(a, Read(a, size) & Read(b, size));
                break;
            case ZYDIS_MNEMONIC_OR:
                Write(a, Read(a, size) | Read(b, size));
                break;
            case ZYDIS_MNEMONIC_XOR:
                Write(a, Read(a, size) ^ Read(b, size));
                break;
            case ZYDIS_MNEMONIC_NEG:
                Write(a, u64{0} - Read(a, size));
                break;
            case ZYDIS_MNEMONIC_NOT:
                Write(a, ~Read(a, size));
                break;
            case ZYDIS_MNEMONIC_DEC:
                Write(a, Read(a, size) - 1);
                break;
            case ZYDIS_MNEMONIC_IMUL:
                if (op.count == 3) {
                    Write(a, Read(b, size) * Read(op.operands[2], size));
                } else {
                    Write(a, Read(a, size) * Read(b, size));
                }
                break;
            case ZYDIS_MNEMONIC_SHL:
                Write(a, Read(a, size) << (Read(b, size) & (size == 64 ? 63 : 31)));
                break;
            case ZYDIS_MNEMONIC_SHR:
                Write(a, Read(a, size) >> (Read(b, size) & (size == 64 ? 63 : 31)));
                break;
            case ZYDIS_MNEMONIC_CMP:
                cmp_left = Read(a, size);
                cmp_right = Read(b, size);
                break;
            case ZYDIS_MNEMONIC_CMOVNBE:
                if (cmp_left > cmp_right) {
                    Write(a, Read(b, size));
                }
                break;
            case ZYDIS_MNEMONIC_CMOVB:
                if (cmp_left < cmp_right) {
                    Write(a, Read(b, size));
                }
                break;
            case ZYDIS_MNEMONIC_PUSH: {
                gpr[RegRsp] -= 8;
                const u64 value = Read(a, size);
                std::memcpy(reinterpret_cast<void*>(gpr[RegRsp]), &value, sizeof(value));
                break;
            }
            case ZYDIS_MNEMONIC_POP: {
                u64 value;
                std::memcpy(&value, reinterpret_cast<const void*>(gpr[RegRsp]), sizeof(value));
                gpr[RegRsp] += 8;
                Write(a, value);
                break;
            }
            case ZYDIS_MNEMONIC_RET:
                return;
            default:
                UNREACHABLE_MSG("SRT walker instruction {} not interpreted",
                                ZydisMnemonicGetString(op.mnemonic));
            }
        }
    }
};

} // namespace

void RunSrtWalker(PFN_SrtWalker walker, const u32* user_data, u32* flat_dst) {
    static std::once_flag handler_once;
    std::call_once(handler_once, [] {
        // After the memory invalidation handler, as the x86-64 walker's own
        Core::Signals::Instance()->RegisterAccessViolationHandler(SafeLoadFaultHandler, 1);
    });
    const Program& program = GetProgram(reinterpret_cast<const void*>(walker));
    Machine machine;
    machine.gpr[RegRdi] = reinterpret_cast<u64>(user_data);
    machine.gpr[RegRsi] = reinterpret_cast<u64>(flat_dst);
    machine.gpr[RegRsp] = reinterpret_cast<u64>(machine.stack.data() + machine.stack.size());
    machine.Run(program);
}

} // namespace Shader

#endif
