// SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <algorithm>
#include <initializer_list>
#include <map>
#include "common/string_util.h"
#include "shader_recompiler/ir/basic_block.h"
#include "shader_recompiler/ir/value.h"

namespace Shader::IR {

Block::Block(Common::ObjectPool<Inst>& inst_pool_) : inst_pool{&inst_pool_} {}

Block::~Block() = default;

void Block::AppendNewInst(Opcode op, std::initializer_list<Value> args) {
    PrependNewInst(end(), op, args);
}

Block::iterator Block::PrependNewInst(iterator insertion_point, const Inst& base_inst) {
    Inst* const inst{inst_pool->Create(base_inst)};
    inst->SetParent(this);
    return instructions.insert(insertion_point, *inst);
}

Block::iterator Block::PrependNewInst(iterator insertion_point, Opcode op,
                                      std::initializer_list<Value> args, u32 flags) {
    Inst* const inst{inst_pool->Create(op, flags)};
    inst->SetParent(this);
    const auto result_it{instructions.insert(insertion_point, *inst)};

    if (inst->NumArgs() != args.size()) {
        UNREACHABLE_MSG("Invalid number of arguments %zu in %.*s", args.size(),
                        static_cast<int>(Shader::IR::NameOf(op).size()),
                        Shader::IR::NameOf(op).data());
    }
    std::ranges::for_each(args, [inst, index = size_t{0}](const Value& arg) mutable {
        inst->SetArg(index, arg);
        ++index;
    });
    return result_it;
}

void Block::AddBranch(Block* block) {
    if (std::ranges::find(imm_successors, block) != imm_successors.end()) {
        UNREACHABLE_MSG("Successor already inserted");
    }
    if (std::ranges::find(block->imm_predecessors, this) != block->imm_predecessors.end()) {
        UNREACHABLE_MSG("Predecessor already inserted");
    }
    imm_successors.push_back(block);
    block->imm_predecessors.push_back(this);
}

static std::string BlockToIndex(const std::map<const Block*, size_t>& block_to_index,
                                Block* block) {
    if (const auto it{block_to_index.find(block)}; it != block_to_index.end()) {
        return Common::StringFromFormat("{Block $%llu}",
                                        static_cast<unsigned long long>(it->second));
    }
    return Common::StringFromFormat("$<unknown block %016" PRIx64 ">",
                                    reinterpret_cast<u64>(block));
}

static size_t InstIndex(std::map<const Inst*, size_t>& inst_to_index, size_t& inst_index,
                        const Inst* inst) {
    const auto [it, is_inserted]{inst_to_index.emplace(inst, inst_index + 1)};
    if (is_inserted) {
        ++inst_index;
    }
    return it->second;
}

static std::string ArgToIndex(std::map<const Inst*, size_t>& inst_to_index, size_t& inst_index,
                              const Value& arg) {
    if (arg.IsEmpty()) {
        return "<null>";
    }
    if (!arg.IsImmediate()) {
        return Common::StringFromFormat("%%%zu", InstIndex(inst_to_index, inst_index, arg.Inst()));
    }
    switch (arg.Type()) {
    case Type::U1:
        return Common::StringFromFormat("#%s", arg.U1() ? "true" : "false");
    case Type::U8:
        return Common::StringFromFormat("#%u", arg.U8());
    case Type::U16:
        return Common::StringFromFormat("#%u", arg.U16());
    case Type::U32:
        return Common::StringFromFormat("#%u", arg.U32());
    case Type::U64:
        return Common::StringFromFormat("#%" PRIu64, arg.U64());
    case Type::F32:
        return Common::StringFromFormat("#%g", arg.F32());
    case Type::ScalarReg:
        return Common::StringFromFormat("SGPR%u", static_cast<u32>(arg.ScalarReg()));
    case Type::VectorReg:
        return Common::StringFromFormat("VGPR%u", static_cast<u32>(arg.VectorReg()));
    case Type::VirtualReg:
        return Common::StringFromFormat("REG%u_%s", (arg.VirtualReg()).index,
                                        Shader::IR::NameOf((arg.VirtualReg()).type).c_str());
    case Type::Attribute:
        return Common::StringFromFormat("%s", Shader::IR::NameOf(arg.Attribute()).c_str());
    case Type::Patch:
        return Common::StringFromFormat("%s", Shader::IR::NameOf(arg.Patch()).c_str());
    default:
        return "<unknown immediate type>";
    }
}

static std::string RegTagInfo(const RegTag tag) {
    switch (tag.type) {
    case RegType::ScalarReg:
        return Common::StringFromFormat("SGPR%u", static_cast<u32>(tag.sreg));
    case RegType::VectorReg:
        return Common::StringFromFormat("VGPR%u", static_cast<u32>(tag.vreg));
    case RegType::VirtualReg:
        return Common::StringFromFormat("REG%u_%s", (tag.reg).index,
                                        Shader::IR::NameOf((tag.reg).type).c_str());
    case RegType::GotoVariable:
        return Common::StringFromFormat("GOTO%u", tag.index);
    case RegType::VccLo:
        return "VCC_LO";
    case RegType::VccHi:
        return "VCC_HI";
    case RegType::Exec:
        return "EXEC";
    case RegType::Scc:
        return "SCC";
    case RegType::M0:
        return "M0";
    default:
        UNREACHABLE();
    }
}

std::string DumpBlock(const Block& block) {
    size_t inst_index{0};
    std::map<const Inst*, size_t> inst_to_index;
    return DumpBlock(block, {}, inst_to_index, inst_index);
}

std::string DumpBlock(const Block& block, const std::map<const Block*, size_t>& block_to_index,
                      std::map<const Inst*, size_t>& inst_to_index, size_t& inst_index) {
    std::string ret{"Block"};
    if (const auto it{block_to_index.find(&block)}; it != block_to_index.end()) {
        ret += Common::StringFromFormat(" $%llu", static_cast<unsigned long long>(it->second));
    }
    ret += " predecessors [";
    for (Shader::IR::Block* block : block.ImmPredecessors()) {
        const auto it{block_to_index.find(block)};
        ASSERT(it != block_to_index.end());
        ret += Common::StringFromFormat("$%llu, ", static_cast<unsigned long long>(it->second));
    }
    if (!block.ImmPredecessors().empty()) {
        ret.pop_back();
        ret.pop_back();
    }
    ret += "]\n";
    for (const Inst& inst : block) {
        const Opcode op{inst.GetOpcode()};
        ret += Common::StringFromFormat("[%016" PRIx64 "] ", reinterpret_cast<u64>(&inst));
        if (TypeOf(op) != Type::Void) {
            ret += Common::StringFromFormat(
                "%%%-5zu = %.*s", InstIndex(inst_to_index, inst_index, &inst),
                static_cast<int>(Shader::IR::NameOf(op).size()), Shader::IR::NameOf(op).data());
        } else {
            ret += Common::StringFromFormat(
                "         %.*s", static_cast<int>(Shader::IR::NameOf(op).size()),
                Shader::IR::NameOf(op).data()); // '%00000 = ' -> 1 + 5 + 3 = 9 spaces
        }

        if (op == Opcode::ReadConst || op == Opcode::ImageSampleRaw) {
            ret += Common::StringFromFormat(" (flags=0x%x) ", inst.Flags<u32>());
        } else if (op == Opcode::ReadConstBuffer) {
            Shader::IR::BufferInstInfo const info = inst.Flags<BufferInstInfo>();
            ret += Common::StringFromFormat(
                " (sharp_source=%s, flatbuf_off_dw=0x%llx) ",
                (bool(info.sharp_source)) ? "true" : "false",
                static_cast<unsigned long long>(info.flatbuf_off_dw.Value()));
        }
        const size_t arg_count{inst.NumArgs()};
        for (size_t arg_index = 0; arg_index < arg_count; ++arg_index) {
            const Value arg{inst.Arg(arg_index)};
            const std::string arg_str{ArgToIndex(inst_to_index, inst_index, arg)};
            ret += arg_index != 0 ? ", " : " ";
            if (op == Opcode::Phi) {
                ret += Common::StringFromFormat(
                    "[ %s, %s ]", (arg_str).c_str(),
                    (BlockToIndex(block_to_index, inst.PhiBlock(arg_index))).c_str());
            } else {
                ret += arg_str;
            }
            if (op != Opcode::Phi) {
                const Type actual_type{arg.Type()};
                const Type expected_type{ArgTypeOf(op, arg_index)};
                if (!AreTypesCompatible(actual_type, expected_type)) {
                    ret += Common::StringFromFormat("<type error: %s != %s>",
                                                    Shader::IR::NameOf(actual_type).c_str(),
                                                    Shader::IR::NameOf(expected_type).c_str());
                }
            }
        }
        if (TypeOf(op) != Type::Void) {
            ret += Common::StringFromFormat(" (uses: %d)", inst.UseCount());
            if (Shader::IR::RegTag tag = inst.GetRegTag()) {
                ret += Common::StringFromFormat(" (tag: %s)", (RegTagInfo(tag)).c_str());
            }
            if (op == Opcode::Phi) {
                ret += Common::StringFromFormat(" (scc%u)", inst.scc_index);
            }
            ret += '\n';
        } else {
            ret += '\n';
        }
    }
    return ret;
}

} // namespace Shader::IR
