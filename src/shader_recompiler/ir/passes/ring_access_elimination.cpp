// SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "common/assert.h"
#include "shader_recompiler/ir/ir_emitter.h"
#include "shader_recompiler/ir/opcodes.h"
#include "shader_recompiler/ir/operand_helper.h"
#include "shader_recompiler/ir/position.h"
#include "shader_recompiler/ir/program.h"
#include "shader_recompiler/ir/reg.h"
#include "shader_recompiler/recompiler.h"
#include "shader_recompiler/runtime_info.h"

namespace Shader::Optimization {

void RingAccessElimination(const IR::Program& program, const RuntimeInfo& runtime_info) {
    Shader::Info& info = program.info;

    const auto& ForEachInstruction = [&](auto func) {
        for (IR::Block* block : program.blocks) {
            for (IR::Inst& inst : block->Instructions()) {
                IR::IREmitter ir{*block, IR::Block::InstructionList::s_iterator_to(inst)};
                func(ir, inst);
            }
        }
    };

    switch (program.info.hw_stage) {
    case HwStage::Local: {
        ForEachInstruction([=](IR::IREmitter& ir, IR::Inst& inst) {
            const IR::Opcode opcode = inst.GetOpcode();
            switch (opcode) {
            case IR::Opcode::WriteSharedU64:
            case IR::Opcode::WriteSharedU32: {
                bool is_composite = opcode == IR::Opcode::WriteSharedU64;
                u32 num_components = opcode == IR::Opcode::WriteSharedU32 ? 1 : 2;

                ASSERT(inst.Arg(0).IsImmediate());

                u32 offset = inst.Arg(0).U32();
                IR::Value data =
                    is_composite ? ir.UnpackUint2x32(IR::U64{inst.Arg(1)}) : inst.Arg(1);
                for (s32 i = 0; i < num_components; i++) {
                    const Shader::IR::Attribute attrib = IR::Attribute::Param0 + (offset / 16);
                    const u32 comp = (offset / 4) % 4;
                    const IR::U32 value =
                        IR::U32{is_composite ? ir.CompositeExtract(data, i) : data};
                    ir.SetAttribute(attrib, ir.BitCast<IR::F32, IR::U32>(value), comp);
                    offset += 4;
                }
                inst.Invalidate();
                break;
            }
            default:
                break;
            }
        });
        break;
    }
    case HwStage::Export: {
        ForEachInstruction([=](IR::IREmitter& ir, IR::Inst& inst) {
            const IR::Opcode opcode = inst.GetOpcode();
            switch (opcode) {
            case IR::Opcode::StoreBufferU32: {
                Shader::IR::BufferInstInfo const info = inst.Flags<IR::BufferInstInfo>();
                if (!info.system_coherent || !info.globally_coherent) {
                    break;
                }

                const u64 offset = inst.Flags<IR::BufferInstInfo>().inst_offset.Value();
                ASSERT(offset < runtime_info.hw.es.vertex_data_size * 4);
                IR::F32 const data = ir.BitCast<IR::F32>(IR::U32{inst.Arg(2)});
                const IR::Value attrib =
                    IR::Value{offset < 16 ? IR::Attribute::Position0
                                          : IR::Attribute::Param0 + (offset / 16 - 1)};
                const u32 comp = (offset / 4) % 4;

                inst.ReplaceOpcode(IR::Opcode::SetAttribute);
                inst.ClearArgs();
                inst.SetArg(0, attrib);
                inst.SetArg(1, data);
                inst.SetArg(2, ir.Imm32(comp));
                break;
            }
            default:
                break;
            }
        });
        break;
    }
    case HwStage::Geometry: {
        const Shader::HwGeometryRuntimeInfo& gs_info = runtime_info.hw.gs;
        info.gs_copy_data = Shader::ParseCopyShader(gs_info.vs_copy);

        u32 dwords_per_vertex = gs_info.out_vertex_data_size;
        if (info.gs_copy_data.num_comps && info.gs_copy_data.num_comps > dwords_per_vertex) {
            LOG_WARNING(Render_Vulkan,
                        "VERT_ITEMSIZE %u is different than actual number of dwords per vertex %u",
                        dwords_per_vertex, info.gs_copy_data.num_comps);
            dwords_per_vertex = info.gs_copy_data.num_comps;
        }

        ForEachInstruction([&](IR::IREmitter& ir, IR::Inst& inst) {
            const IR::Opcode opcode = inst.GetOpcode();
            switch (opcode) {
            case IR::Opcode::LoadBufferU32: {
                Shader::IR::BufferInstInfo const info = inst.Flags<IR::BufferInstInfo>();
                if (!info.system_coherent || !info.globally_coherent) {
                    break;
                }

                const u32 vertex_id = (info.index_enable ? IR::GetBufferIndexArg(&inst)
                                                         : IR::GetBufferVOffsetArg(&inst))
                                          .U32() >>
                                      2;
                const IR::U32 soffset = IR::GetBufferSOffsetArg(&inst);
                const unsigned int bucket = soffset.U32() / 256u;
                const Shader::IR::Attribute attrib = bucket < 4
                                                         ? IR::Attribute::Position0
                                                         : IR::Attribute::Param0 + (bucket / 4 - 1);
                const unsigned int comp = bucket % 4;

                IR::F32 attr_value = ir.GetAttribute(attrib, comp, vertex_id);
                inst.ReplaceOpcode(IR::Opcode::BitCastU32F32);
                inst.ClearArgs();
                inst.SetArg(0, attr_value);
                break;
            }
            case IR::Opcode::StoreBufferU32: {
                Shader::IR::BufferInstInfo const buffer_info = inst.Flags<IR::BufferInstInfo>();
                if (!buffer_info.system_coherent || !buffer_info.globally_coherent) {
                    break;
                }

                const u64 offset = inst.Flags<IR::BufferInstInfo>().inst_offset.Value();
                IR::F32 const data = ir.BitCast<IR::F32>(IR::U32{inst.Arg(2)});
                const size_t comp_ofs = info.gs_copy_data.output_vertices * sizeof(u32);
                const size_t output_size =
                    gs_info.output_vertices * dwords_per_vertex * sizeof(u32);

                const u64 vc_read_ofs = (((offset / comp_ofs) * comp_ofs) % output_size) * 16u;
                const auto& it = info.gs_copy_data.attr_map.find(vc_read_ofs);
                ASSERT(it != info.gs_copy_data.attr_map.cend());
                const auto& [attr, comp] = it->second;

                inst.Invalidate();
                if (IsPosition(attr)) {
                    ExportPosition(ir, gs_info, false, attr, comp, data);
                } else {
                    ir.SetAttribute(attr, data, comp);
                }
                break;
            }
            default:
                break;
            }
        });
        break;
    }
    default:
        break;
    }
}

} // namespace Shader::Optimization
