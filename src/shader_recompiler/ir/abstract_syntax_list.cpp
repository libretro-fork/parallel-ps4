// SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "abstract_syntax_list.h"
#include "common/string_util.h"

namespace Shader::IR {

std::string DumpASLNode(const AbstractSyntaxNode& node,
                        const std::map<const Block*, size_t>& block_to_index,
                        const std::map<const Inst*, size_t>& inst_to_index) {
    switch (node.type) {
    case AbstractSyntaxNode::Type::Block:
        return Common::StringFromFormat(
            "Block: $%llu", static_cast<unsigned long long>(block_to_index.at(node.data.block)));
    case AbstractSyntaxNode::Type::If:
        return Common::StringFromFormat(
            "If: cond = %%%llu, body = $%llu, merge = $%llu",
            static_cast<unsigned long long>(inst_to_index.at(node.data.if_node.cond.Inst())),
            static_cast<unsigned long long>(block_to_index.at(node.data.if_node.body)),
            static_cast<unsigned long long>(block_to_index.at(node.data.if_node.merge)));
    case AbstractSyntaxNode::Type::EndIf:
        return Common::StringFromFormat(
            "EndIf: merge = $%llu",
            static_cast<unsigned long long>(block_to_index.at(node.data.end_if.merge)));
    case AbstractSyntaxNode::Type::Loop:
        return Common::StringFromFormat(
            "Loop: body = $%llu, continue = $%llu, merge = $%llu",
            static_cast<unsigned long long>(block_to_index.at(node.data.loop.body)),
            static_cast<unsigned long long>(block_to_index.at(node.data.loop.continue_block)),
            static_cast<unsigned long long>(block_to_index.at(node.data.loop.merge)));
    case AbstractSyntaxNode::Type::Repeat:
        return Common::StringFromFormat(
            "Repeat: cond = %%%llu, header = $%llu, merge = $%llu",
            static_cast<unsigned long long>(inst_to_index.at(node.data.repeat.cond.Inst())),
            static_cast<unsigned long long>(block_to_index.at(node.data.repeat.loop_header)),
            static_cast<unsigned long long>(block_to_index.at(node.data.repeat.merge)));
    case AbstractSyntaxNode::Type::Break:
        return Common::StringFromFormat(
            "Break: cond = %%%llu, merge = $%llu, skip = $%llu",
            static_cast<unsigned long long>(inst_to_index.at(node.data.break_node.cond.Inst())),
            static_cast<unsigned long long>(block_to_index.at(node.data.break_node.merge)),
            static_cast<unsigned long long>(block_to_index.at(node.data.break_node.skip)));
    case AbstractSyntaxNode::Type::Return:
        return "Return";
    case AbstractSyntaxNode::Type::Unreachable:
        return "Unreachable";
    };
    UNREACHABLE();
}

} // namespace Shader::IR