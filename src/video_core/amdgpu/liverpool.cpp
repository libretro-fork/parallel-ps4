// SPDX-FileCopyrightText: Copyright 2024-2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <retro_atomic.h>
#include "common/assert.h"
#include "common/enum_names.h"
#include "common/scope_exit.h"
#include "common/thread_name.h"
#include "core/emulator_settings.h"
#include "core/libraries/kernel/process.h"
#include "core/libraries/videoout/driver.h"
#include "core/memory.h"
#include "core/platform.h"
#include "video_core/amdgpu/liverpool.h"
#include "video_core/amdgpu/pm4_cmds.h"
#include "video_core/renderer_vulkan/vk_rasterizer.h"

namespace AmdGpu {

#define YIELD_CE()                                                                                 \
    co_yield {}
#define YIELD_GFX()                                                                                \
    co_yield {}
#define YIELD_ASC(id)                                                                              \
    co_yield {}

#define RESUME_CE(task) task.handle.resume()
#define RESUME_GFX(task) task.handle.resume()
#define RESUME_ASC(task, id) task.handle.resume()

u8 Liverpool::ConstantEngine::constants_heap[48_KB];

static Common::Span<const u32> NextPacket(Common::Span<const u32> span, size_t offset) {
    if (offset > span.size()) {
        LOG_ERROR(
            Lib_GnmDriver,
            ": packet length exceeds remaining submission size. Packet dword count=%zu, remaining "
            "submission dwords=%zu",
            offset, span.size());
        // Return empty subspan so check for next packet bails out
        return {};
    }

    return span.subspan(offset);
}

Liverpool::Liverpool() : guest_markers_enabled{emu_settings_get_vkguest_markers()} {
    num_counter_pairs = Libraries::Kernel::sceKernelIsNeoMode() ? 16 : 8;
    mpsc_stack_init(&commands);
    retro_eventcount_init(&work_ec);
    retro_eventcount_init(&idle_ec);
    retro_eventcount_init(&command_done_ec);
    stop_thread_start(
        &process_thread, [](void* self) { static_cast<Liverpool*>(self)->Process(); }, this);
}

Liverpool::~Liverpool() {
    // Asked to stop, then woken wherever it is parked on work_ec
    stop_thread_request(&process_thread);
    retro_eventcount_notify(&work_ec);
    stop_thread_join(&process_thread);
    // ce_submits, then each queue's
    for (u32 q = 0; q <= NumTotalQueues; ++q) {
        TaskQueue& queue = q == 0 ? ce_submits : mapped_queues[q - 1].submits;
        queue.Take();
        while (!queue.Empty()) {
            const Task::Handle task = queue.Front();
            queue.Pop();
            task.destroy();
        }
    }
    cmd_copy_ring_free(copy_ring);
    // Commands nobody will run: only the allocated ones are left, as a sender that waits
    // for its command does so before the GPU thread is stopped
    for (mpsc_stack_node_t* node = mpsc_stack_drain(&commands); node;) {
        AmdGpu::Liverpool::Command* command = reinterpret_cast<Command*>(node);
        node = node->next;
        if (!command->done) {
            delete command;
        }
    }
    retro_eventcount_free(&command_done_ec);
    retro_eventcount_free(&idle_ec);
    retro_eventcount_free(&work_ec);
}

void Liverpool::SubmitFinished() {
    if (static_cast<u32>(retro_atomic_fetch_sub_int(&num_submits, 1)) == 1) {
        retro_eventcount_notify(&idle_ec);
    }
}

void Liverpool::ProcessCeQueue() {
    ce_submits.Take();
    if (ce_submits.Empty()) {
        return;
    }
    const Task::Handle task = ce_submits.Front();
    task.resume();
    if (task.done()) {
        ce_submits.Pop();
        task.destroy();
        if (copy_ring) {
            cmd_copy_ring_ce_finished(copy_ring);
        }
        SubmitFinished();
    }
}

void Liverpool::ProcessCommands() {
    // Process incoming commands with high priority
    while (static_cast<u32>(retro_atomic_load_relaxed_int(&num_commands))) {
        mpsc_stack_node_t* node = mpsc_stack_reverse(mpsc_stack_drain(&commands));
        while (node) {
            AmdGpu::Liverpool::Command* command = reinterpret_cast<Command*>(node);
            node = node->next;
            command->func();
            retro_atomic_fetch_sub_int(&num_commands, 1);
            if (retro_atomic_int_t* done = command->done) {
                // The sender's: it may be gone as soon as done is set
                retro_atomic_store_release_int(done, 1);
                retro_eventcount_notify(&command_done_ec);
            } else {
                delete command;
            }
        }
    }
}

bool Liverpool::HasWork() {
    return static_cast<u32>(retro_atomic_load_seq_cst_int(&num_commands)) ||
           static_cast<u32>(retro_atomic_load_seq_cst_int(&num_submits)) ||
           (retro_atomic_load_acquire_int(&submit_done) != 0) ||
           stop_thread_stopping(&process_thread);
}

void Liverpool::Process() {
    thread_name_set("shadPS4:GpuCommandProcessor");
    gpu_id = sthread_get_current_thread_id();
#ifdef __linux__
    gpu_tid = gettid();
#endif

    while (!stop_thread_stopping(&process_thread)) {
        while (!HasWork()) {
            const int key = retro_eventcount_prepare_wait(&work_ec);
            if (HasWork()) {
                retro_eventcount_cancel_wait(&work_ec);
                break;
            }
            retro_eventcount_commit_wait(&work_ec, key);
        }
        if (stop_thread_stopping(&process_thread)) {
            break;
        }

        curr_qid = -1;

        while ((static_cast<u32>(retro_atomic_load_seq_cst_int(&num_submits)) ||
                static_cast<u32>(retro_atomic_load_seq_cst_int(&num_commands))) &&
               !stop_thread_stopping(&process_thread)) {
            ProcessCommands();
            ProcessCeQueue();

            curr_qid = (curr_qid + 1) %
                       static_cast<u32>(retro_atomic_load_acquire_int(&num_mapped_queues));

            AmdGpu::Liverpool::GpuQueue& queue = mapped_queues[curr_qid];

            queue.submits.Take();
            if (queue.submits.Empty()) {
                continue;
            }
            const Task::Handle task = queue.submits.Front();
            task.resume();

            if (task.done()) {
                queue.submits.Pop();
                task.destroy();
                // Its copies, if any, may be written over from here on
                if (curr_qid == GfxQueueId && copy_ring) {
                    cmd_copy_ring_gfx_finished(copy_ring);
                }
                SubmitFinished();
            }
        }

        if (retro_atomic_exchange_int(&submit_done, 0) != 0) {
            if (rasterizer) {
                rasterizer->OnSubmit();
                rasterizer->Flush();
            }
        }

        Platform::IrqC::Instance()->Signal(Platform::InterruptId::GpuIdle);
    }
}

Liverpool::Task Liverpool::ProcessCeUpdate(Common::Span<const u32> ccb) {

    while (!ccb.empty()) {
        ProcessCommands();

        const AmdGpu::PM4Header* header = reinterpret_cast<const PM4Header*>(ccb.data());
        const u32 type = header->type;
        if (type != 3) {
            // No other types of packets were spotted so far
            UNREACHABLE_MSG("Invalid PM4 type %u", type);
        }

        const PM4ItOpcode opcode = header->type3.opcode;
        const u32* it_body = reinterpret_cast<const u32*>(header) + 1;
        switch (opcode) {
        case PM4ItOpcode::Nop: {
            // const auto* nop = reinterpret_cast<const PM4CmdNop*>(header);
            break;
        }
        case PM4ItOpcode::WriteConstRam: {
            const AmdGpu::PM4WriteConstRam* write_const =
                reinterpret_cast<const PM4WriteConstRam*>(header);
            memcpy(cblock.constants_heap + write_const->Offset(), &write_const->data,
                   write_const->Size());
            break;
        }
        case PM4ItOpcode::DumpConstRam: {
            const AmdGpu::PM4DumpConstRam* dump_const =
                reinterpret_cast<const PM4DumpConstRam*>(header);
            memcpy(dump_const->Address<void*>(), cblock.constants_heap + dump_const->Offset(),
                   dump_const->Size());
            break;
        }
        case PM4ItOpcode::IncrementCeCounter: {
            ++cblock.ce_count;
            break;
        }
        case PM4ItOpcode::WaitOnDeCounterDiff: {
            const u32 diff = it_body[0];
            while (cblock.ce_count >= cblock.de_count &&
                   (cblock.ce_count - cblock.de_count) >= diff) {
                YIELD_CE();
            }
            break;
        }
        case PM4ItOpcode::IndirectBufferConst: {
            const AmdGpu::PM4CmdIndirectBuffer* indirect_buffer =
                reinterpret_cast<const PM4CmdIndirectBuffer*>(header);
            Liverpool::Task task =
                ProcessCeUpdate({indirect_buffer->Address<const u32>(), indirect_buffer->ib_size});
            SCOPE_EXIT {
                task.handle.destroy();
            };
            RESUME_CE(task);

            while (!task.handle.done()) {
                YIELD_CE();
                RESUME_CE(task);
            }
            break;
        }
        default:
            const u32 count = header->type3.NumWords();
            UNREACHABLE_MSG("Unknown PM4 type 3 opcode 0x%x with count %u",
                            static_cast<u32>(opcode), count);
        }
        ccb = NextPacket(ccb, header->type3.NumWords() + 1);
    }
}

Liverpool::Task Liverpool::ProcessGraphics(Common::Span<const u32> dcb) {

    const uintptr_t base_addr = reinterpret_cast<uintptr_t>(dcb.data());
    while (!dcb.empty()) {
        ProcessCommands();

        const AmdGpu::PM4Header* header = reinterpret_cast<const PM4Header*>(dcb.data());
        const u32 type = header->type;

        switch (type) {
        default:
            UNREACHABLE_MSG("Wrong PM4 type %u", type);
            break;
        case 0:
            UNREACHABLE_MSG("Unimplemented PM4 type 0, base reg: %u, size: %u",
                            header->type0.base.Value(), header->type0.NumWords());
            break;
        case 2:
            // Type-2 packet are used for padding purposes
            dcb = NextPacket(dcb, 1);
            continue;
        case 3:
            const u32 count = header->type3.NumWords();
            const PM4ItOpcode opcode = header->type3.opcode;
            switch (opcode) {
            case PM4ItOpcode::Nop: {
                const AmdGpu::PM4CmdNop* nop = reinterpret_cast<const PM4CmdNop*>(header);
                if (nop->header.count.Value() == 0) {
                    break;
                }

                switch (nop->data_block[0]) {
                case PM4CmdNop::PayloadType::PatchedFlip: {
                    // There is no evidence that GPU CP drives flip events by parsing
                    // special NOP packets. For convenience lets assume that it does.
                    Platform::IrqC::Instance()->Signal(Platform::InterruptId::GfxFlip);
                    break;
                }
                case PM4CmdNop::PayloadType::DebugMarkerPush: {
                    if (guest_markers_enabled) {
                        // The guest's label, ended by a NUL within its words
                        rasterizer->ScopeMarkerBegin(
                            reinterpret_cast<const char*>(&nop->data_block[1]), true);
                    }
                    break;
                }
                case PM4CmdNop::PayloadType::DebugColorMarkerPush: {
                    if (guest_markers_enabled) {
                        const u32 marker_sz = nop->header.count.Value() * 2;
                        const char* const label =
                            reinterpret_cast<const char*>(&nop->data_block[1]);
                        const u32 color = *reinterpret_cast<const u32*>(
                            reinterpret_cast<const u8*>(&nop->data_block[1]) + marker_sz);
                        rasterizer->ScopedMarkerInsertColor(label, color, true);
                    }
                    break;
                }
                case PM4CmdNop::PayloadType::DebugMarkerPop: {
                    if (guest_markers_enabled) {
                        rasterizer->ScopeMarkerEnd(true);
                    }
                    break;
                }
                default:
                    break;
                }
                break;
            }
            case PM4ItOpcode::ContextControl: {
                break;
            }
            case PM4ItOpcode::ClearState: {
                regs.SetDefaults();
                break;
            }
            case PM4ItOpcode::SetConfigReg: {
                const AmdGpu::PM4CmdSetData* set_data =
                    reinterpret_cast<const PM4CmdSetData*>(header);
                const unsigned int reg_addr = Regs::ConfigRegWordOffset + set_data->reg_offset;
                const u32* payload = reinterpret_cast<const u32*>(header + 2);
                memcpy(&regs.reg_array[reg_addr], payload, (count - 1) * sizeof(u32));
                break;
            }
            case PM4ItOpcode::SetContextReg: {
                const AmdGpu::PM4CmdSetData* set_data =
                    reinterpret_cast<const PM4CmdSetData*>(header);
                const unsigned int reg_addr = Regs::ContextRegWordOffset + set_data->reg_offset;
                const u32* payload = reinterpret_cast<const u32*>(header + 2);

                memcpy(&regs.reg_array[reg_addr], payload, (count - 1) * sizeof(u32));

                // In the case of HW, render target memory has alignment as color block operates on
                // tiles. There is no information of actual resource extents stored in CB context
                // regs, so any deduction of it from slices/pitch will lead to a larger surface
                // created. The same applies to the depth targets. Fortunately, the guest always
                // sends a trailing NOP packet right after the context regs setup, so we can use the
                // heuristic below and extract the hint to determine actual resource dims.

                switch (reg_addr) {
                case ContextRegs::CbColor0Base:
                case ContextRegs::CbColor1Base:
                case ContextRegs::CbColor2Base:
                case ContextRegs::CbColor3Base:
                case ContextRegs::CbColor4Base:
                case ContextRegs::CbColor5Base:
                case ContextRegs::CbColor6Base:
                case ContextRegs::CbColor7Base: {
                    const unsigned int col_buf_id =
                        (reg_addr - ContextRegs::CbColor0Base) /
                        (ContextRegs::CbColor1Base - ContextRegs::CbColor0Base);
                    ASSERT(col_buf_id < NUM_COLOR_BUFFERS);

                    const BitField<16, 14, unsigned int> nop_offset = header->type3.count;
                    if (nop_offset == 0x0e || nop_offset == 0x0d || nop_offset == 0x0b) {
                        ASSERT_MSG(payload[nop_offset] == 0xc0001000,
                                   "NOP hint is missing in CB setup sequence");
                        last_cb_extent[col_buf_id].raw = payload[nop_offset + 1];
                    } else {
                        last_cb_extent[col_buf_id].raw = 0;
                    }
                    break;
                }
                case ContextRegs::CbColor0Cmask:
                case ContextRegs::CbColor1Cmask:
                case ContextRegs::CbColor2Cmask:
                case ContextRegs::CbColor3Cmask:
                case ContextRegs::CbColor4Cmask:
                case ContextRegs::CbColor5Cmask:
                case ContextRegs::CbColor6Cmask:
                case ContextRegs::CbColor7Cmask: {
                    const unsigned int col_buf_id =
                        (reg_addr - ContextRegs::CbColor0Cmask) /
                        (ContextRegs::CbColor1Cmask - ContextRegs::CbColor0Cmask);
                    ASSERT(col_buf_id < NUM_COLOR_BUFFERS);

                    const BitField<16, 14, unsigned int> nop_offset = header->type3.count;
                    if (nop_offset == 0x04) {
                        ASSERT_MSG(payload[nop_offset] == 0xc0001000,
                                   "NOP hint is missing in CB setup sequence");
                        last_cb_extent[col_buf_id].raw = payload[nop_offset + 1];
                    }
                    break;
                }
                case ContextRegs::DbZInfo: {
                    if (header->type3.count == 8) {
                        ASSERT_MSG(payload[20] == 0xc0001000,
                                   "NOP hint is missing in DB setup sequence");
                        last_db_extent.raw = payload[21];
                    } else {
                        last_db_extent.raw = 0;
                    }
                    break;
                }
                default:
                    break;
                }
                break;
            }
            case PM4ItOpcode::SetShReg: {
                const AmdGpu::PM4CmdSetData* set_data =
                    reinterpret_cast<const PM4CmdSetData*>(header);
                const size_t set_size = (count - 1) * sizeof(u32);

                if (set_data->reg_offset >= 0x200 &&
                    set_data->reg_offset <= (0x200 + sizeof(ComputeProgram) / 4)) {
                    ASSERT(set_size <= sizeof(ComputeProgram));
                    u32* addr = reinterpret_cast<u32*>(&mapped_queues[GfxQueueId].cs_state) +
                                (set_data->reg_offset - 0x200);
                    memcpy(addr, header + 2, set_size);
                } else {
                    memcpy(&regs.reg_array[Regs::ShRegWordOffset + set_data->reg_offset],
                           header + 2, set_size);
                }
                break;
            }
            case PM4ItOpcode::SetUconfigReg: {
                const AmdGpu::PM4CmdSetData* set_data =
                    reinterpret_cast<const PM4CmdSetData*>(header);
                memcpy(&regs.reg_array[Regs::UconfigRegWordOffset + set_data->reg_offset],
                       header + 2, (count - 1) * sizeof(u32));
                break;
            }
            case PM4ItOpcode::SetPredication: {
                LOG_WARNING(Render, "Unimplemented IT_SET_PREDICATION");
                break;
            }
            case PM4ItOpcode::IndexType: {
                const AmdGpu::PM4CmdDrawIndexType* index_type =
                    reinterpret_cast<const PM4CmdDrawIndexType*>(header);
                regs.index_buffer_type.raw = index_type->raw;
                break;
            }
            case PM4ItOpcode::DrawIndex2: {
                const AmdGpu::PM4CmdDrawIndex2* draw_index =
                    reinterpret_cast<const PM4CmdDrawIndex2*>(header);
                regs.max_index_size = draw_index->max_size;
                regs.index_base_address.base_addr_lo = draw_index->index_base_lo;
                regs.index_base_address.base_addr_hi = draw_index->index_base_hi;
                regs.num_indices = draw_index->index_count;
                regs.draw_initiator = draw_index->draw_initiator;
                if (!rasterizer) {
                    break;
                }
                const void* const cmd_address = reinterpret_cast<const void*>(header);
                rasterizer->ScopeMarker("gfx:0x%llx:DrawIndex2", cmd_address,
                                        [&] { rasterizer->Draw(true); });
                break;
            }
            case PM4ItOpcode::DrawIndexOffset2: {
                const AmdGpu::PM4CmdDrawIndexOffset2* draw_index_off =
                    reinterpret_cast<const PM4CmdDrawIndexOffset2*>(header);
                regs.max_index_size = draw_index_off->max_size;
                regs.num_indices = draw_index_off->index_count;
                regs.draw_initiator = draw_index_off->draw_initiator;
                if (!rasterizer) {
                    break;
                }
                const void* const cmd_address = reinterpret_cast<const void*>(header);
                rasterizer->ScopeMarker("gfx:0x%llx:DrawIndexOffset2", cmd_address, [&] {
                    rasterizer->Draw(true, draw_index_off->index_offset);
                });
                break;
            }
            case PM4ItOpcode::DrawIndexAuto: {
                const AmdGpu::PM4CmdDrawIndexAuto* draw_index =
                    reinterpret_cast<const PM4CmdDrawIndexAuto*>(header);
                regs.num_indices = draw_index->index_count;
                regs.draw_initiator = draw_index->draw_initiator;
                if (!rasterizer) {
                    break;
                }
                const void* const cmd_address = reinterpret_cast<const void*>(header);
                rasterizer->ScopeMarker("gfx:0x%llx:DrawIndexAuto", cmd_address,
                                        [&] { rasterizer->Draw(false); });
                break;
            }
            case PM4ItOpcode::DrawIndirect: {
                const AmdGpu::PM4CmdDrawIndirect* draw_indirect =
                    reinterpret_cast<const PM4CmdDrawIndirect*>(header);
                const u32 offset = draw_indirect->data_offset;
                const size_t stride = sizeof(DrawIndirectArgs);
                if (!rasterizer) {
                    break;
                }
                const void* const cmd_address = reinterpret_cast<const void*>(header);
                rasterizer->ScopeMarker(
                    "gfx:0x%llx:DrawIndirect", cmd_address, [&] {
                        rasterizer->DrawIndirect(false, indirect_args_addr, offset, stride, 1, 0,
                                                 draw_indirect->base_vtx_loc,
                                                 draw_indirect->start_inst_loc);
                    });
                break;
            }
            case PM4ItOpcode::DrawIndirectCountMulti: {
                const AmdGpu::PM4CmdDrawIndirectCountMulti* draw_indirect =
                    reinterpret_cast<const PM4CmdDrawIndirectCountMulti*>(header);
                const u32 offset = draw_indirect->data_offset;
                const size_t stride = sizeof(DrawIndirectArgs);
                if (!rasterizer) {
                    break;
                }
                const void* const cmd_address = reinterpret_cast<const void*>(header);
                rasterizer->ScopeMarker(
                    "gfx:0x%llx:DrawIndirectCountMulti", cmd_address, [&] {
                        rasterizer->DrawIndirect(
                            false, indirect_args_addr, offset, draw_indirect->stride,
                            draw_indirect->count, draw_indirect->count_addr,
                            draw_indirect->base_vtx_loc, draw_indirect->start_inst_loc);
                    });
                break;
            }
            case PM4ItOpcode::DrawIndirectMulti: {
                const AmdGpu::PM4CmdDrawIndirectMulti* draw_indirect =
                    reinterpret_cast<const PM4CmdDrawIndirectMulti*>(header);
                const u32 offset = draw_indirect->data_offset;
                if (!rasterizer) {
                    break;
                }
                const void* const cmd_address = reinterpret_cast<const void*>(header);
                rasterizer->ScopeMarker(
                    "gfx:0x%llx:DrawIndirectMulti", cmd_address, [&] {
                        rasterizer->DrawIndirect(false, indirect_args_addr, offset,
                                                 draw_indirect->stride, draw_indirect->count, 0,
                                                 draw_indirect->base_vtx_loc,
                                                 draw_indirect->start_inst_loc);
                    });
                break;
            }
            case PM4ItOpcode::DrawIndexIndirect: {
                const AmdGpu::PM4CmdDrawIndexIndirect* draw_index_indirect =
                    reinterpret_cast<const PM4CmdDrawIndexIndirect*>(header);
                const u32 offset = draw_index_indirect->data_offset;
                const size_t stride = sizeof(DrawIndexedIndirectArgs);
                regs.max_index_size = index_buffer_num_indices;
                if (!rasterizer) {
                    break;
                }
                const void* const cmd_address = reinterpret_cast<const void*>(header);
                rasterizer->ScopeMarker(
                    "gfx:0x%llx:DrawIndexIndirect", cmd_address, [&] {
                        rasterizer->DrawIndirect(true, indirect_args_addr, offset, stride, 1, 0,
                                                 draw_index_indirect->base_vtx_loc,
                                                 draw_index_indirect->start_inst_loc);
                    });
                break;
            }
            case PM4ItOpcode::DrawIndexIndirectMulti: {
                const AmdGpu::PM4CmdDrawIndexIndirectMulti* draw_index_indirect =
                    reinterpret_cast<const PM4CmdDrawIndexIndirectMulti*>(header);
                const u32 offset = draw_index_indirect->data_offset;
                regs.max_index_size = index_buffer_num_indices;
                if (!rasterizer) {
                    break;
                }
                const void* const cmd_address = reinterpret_cast<const void*>(header);
                rasterizer->ScopeMarker(
                    "gfx:0x%llx:DrawIndexIndirectMulti", cmd_address, [&] {
                        rasterizer->DrawIndirect(
                            true, indirect_args_addr, offset, draw_index_indirect->stride,
                            draw_index_indirect->count, 0, draw_index_indirect->base_vtx_loc,
                            draw_index_indirect->start_inst_loc);
                    });
                break;
            }
            case PM4ItOpcode::DrawIndexIndirectCountMulti: {
                const AmdGpu::PM4CmdDrawIndexIndirectCountMulti* draw_index_indirect =
                    reinterpret_cast<const PM4CmdDrawIndexIndirectCountMulti*>(header);
                const u32 offset = draw_index_indirect->data_offset;
                regs.max_index_size = index_buffer_num_indices;
                if (!rasterizer) {
                    break;
                }
                const void* const cmd_address = reinterpret_cast<const void*>(header);
                rasterizer->ScopeMarker(
                    "gfx:0x%llx:DrawIndexIndirectCountMulti", cmd_address, [&] {
                        rasterizer->DrawIndirect(
                            true, indirect_args_addr, offset, draw_index_indirect->stride,
                            draw_index_indirect->count,
                            draw_index_indirect->count_indirect_enable.Value()
                                ? draw_index_indirect->count_addr
                                : 0,
                            draw_index_indirect->base_vtx_loc, draw_index_indirect->start_inst_loc);
                    });
                break;
            }
            case PM4ItOpcode::DispatchDirect: {
                const AmdGpu::PM4CmdDispatchDirect* dispatch_direct =
                    reinterpret_cast<const PM4CmdDispatchDirect*>(header);
                AmdGpu::ComputeProgram& cs_program = GetCsRegs();
                cs_program.dim_x = dispatch_direct->dim_x;
                cs_program.dim_y = dispatch_direct->dim_y;
                cs_program.dim_z = dispatch_direct->dim_z;
                cs_program.dispatch_initiator = dispatch_direct->dispatch_initiator;
                if (!rasterizer || (cs_program.dispatch_initiator & 1) == 0) {
                    break;
                }
                const void* const cmd_address = reinterpret_cast<const void*>(header);
                rasterizer->ScopeMarker("gfx:0x%llx:DispatchDirect", cmd_address,
                                        [&] { rasterizer->DispatchDirect(); });
                break;
            }
            case PM4ItOpcode::DispatchIndirect: {
                const AmdGpu::PM4CmdDispatchIndirect* dispatch_indirect =
                    reinterpret_cast<const PM4CmdDispatchIndirect*>(header);
                AmdGpu::ComputeProgram& cs_program = GetCsRegs();
                const u32 offset = dispatch_indirect->data_offset;
                const size_t size = sizeof(PM4CmdDispatchIndirect::GroupDimensions);
                if (!rasterizer || (cs_program.dispatch_initiator & 1) == 0) {
                    break;
                }
                const void* const cmd_address = reinterpret_cast<const void*>(header);
                rasterizer->ScopeMarker("gfx:0x%llx:DispatchIndirect", cmd_address, [&] {
                    rasterizer->DispatchIndirect(indirect_args_addr, offset, size);
                });
                break;
            }
            case PM4ItOpcode::NumInstances: {
                const AmdGpu::PM4CmdDrawNumInstances* num_instances =
                    reinterpret_cast<const PM4CmdDrawNumInstances*>(header);
                regs.num_instances.num_instances = num_instances->num_instances;
                break;
            }
            case PM4ItOpcode::IndexBase: {
                const AmdGpu::PM4CmdDrawIndexBase* index_base =
                    reinterpret_cast<const PM4CmdDrawIndexBase*>(header);
                regs.index_base_address.base_addr_lo = index_base->addr_lo;
                regs.index_base_address.base_addr_hi = index_base->addr_hi;
                break;
            }
            case PM4ItOpcode::IndexBufferSize: {
                const AmdGpu::PM4CmdDrawIndexBufferSize* index_size =
                    reinterpret_cast<const PM4CmdDrawIndexBufferSize*>(header);
                index_buffer_num_indices = index_size->num_indices;
                break;
            }
            case PM4ItOpcode::SetBase: {
                const AmdGpu::PM4CmdSetBase* set_base =
                    reinterpret_cast<const PM4CmdSetBase*>(header);
                ASSERT(set_base->base_index == PM4CmdSetBase::BaseIndex::DrawIndexIndirPatchTable);
                indirect_args_addr = set_base->Address<u64>();
                break;
            }
            case PM4ItOpcode::EventWrite: {
                const AmdGpu::PM4CmdEventWrite* event =
                    reinterpret_cast<const PM4CmdEventWrite*>(header);
                LOG_DEBUG(Render, "Encountered EventWrite: event_type = %s, event_index = %s",
                          enum_name(&enum_names_AmdGpu_EventType,
                                    static_cast<s64>(event->event_type.Value())),
                          enum_name(&enum_names_AmdGpu_EventIndex,
                                    static_cast<s64>(event->event_index.Value())));
                if (event->event_type.Value() == EventType::SoVgtStreamoutFlush) {
                    // TODO: handle proper synchronization, for now signal that update is done
                    // immediately
                    regs.cp_strmout_cntl.offset_update_done = 1;
                } else if (event->event_index.Value() == EventIndex::ZpassDone) {
                    if (event->event_type.Value() == EventType::PixelPipeStatDump) {
                        static constexpr u64 OcclusionCounterValidMask = 0x8000000000000000ULL;
                        static constexpr u64 OcclusionCounterStep = 0x2FFFFFFULL;
                        u64* results = event->Address<u64*>();
                        for (s32 i = 0; i < num_counter_pairs; ++i, results += 2) {
                            *results = pixel_counter | OcclusionCounterValidMask;
                        }
                        pixel_counter += OcclusionCounterStep;
                    }
                }
                break;
            }
            case PM4ItOpcode::EventWriteEos: {
                const AmdGpu::PM4CmdEventWriteEos* event_eos =
                    reinterpret_cast<const PM4CmdEventWriteEos*>(header);
                if (rasterizer) {
                    rasterizer->OnFence();
                }
                event_eos->SignalFence([](void* address, u64 data, u32 num_bytes) {
                    Core::MemoryManager* memory = Core::Memory::Instance();
                    ASSERT(memory->TryWriteBacking(address, &data, num_bytes));
                });
                if (event_eos->command == PM4CmdEventWriteEos::Command::GdsStore) {
                    ASSERT(event_eos->size == 1);
                    if (rasterizer) {
                        rasterizer->Finish();
                        const u32 value = rasterizer->ReadDataFromGds(event_eos->gds_index);
                        *event_eos->Address() = value;
                    }
                }
                break;
            }
            case PM4ItOpcode::EventWriteEop: {
                const AmdGpu::PM4CmdEventWriteEop* event_eop =
                    reinterpret_cast<const PM4CmdEventWriteEop*>(header);
                if (rasterizer) {
                    rasterizer->OnFence();
                }
                event_eop->SignalFence(
                    [](void* address, u64 data, u32 num_bytes) {
                        Core::MemoryManager* memory = Core::Memory::Instance();
                        ASSERT(memory->TryWriteBacking(address, &data, num_bytes));
                    },
                    [] { Platform::IrqC::Instance()->Signal(Platform::InterruptId::GfxEop); });
                break;
            }
            case PM4ItOpcode::DmaData: {
                const AmdGpu::PM4DmaData* dma_data = reinterpret_cast<const PM4DmaData*>(header);
                if (dma_data->dst_addr_lo == 0x3022C || !rasterizer) {
                    break;
                }
                ASSERT(dma_data->command.das == 0);
                if (dma_data->src_sel == DmaDataSrc::Data && dma_data->dst_sel == DmaDataDst::Gds) {
                    rasterizer->FillBuffer(dma_data->dst_addr_lo, dma_data->NumBytes(),
                                           dma_data->data, true);
                } else if ((dma_data->src_sel == DmaDataSrc::Memory ||
                            dma_data->src_sel == DmaDataSrc::MemoryUsingL2) &&
                           dma_data->dst_sel == DmaDataDst::Gds) {
                    rasterizer->CopyBuffer(dma_data->dst_addr_lo, dma_data->SrcAddress<VAddr>(),
                                           dma_data->NumBytes(), true, false);
                } else if (dma_data->src_sel == DmaDataSrc::Data &&
                           (dma_data->dst_sel == DmaDataDst::Memory ||
                            dma_data->dst_sel == DmaDataDst::MemoryUsingL2)) {
                    rasterizer->FillBuffer(dma_data->DstAddress<VAddr>(), dma_data->NumBytes(),
                                           dma_data->data, false);
                } else if (dma_data->src_sel == DmaDataSrc::Gds &&
                           (dma_data->dst_sel == DmaDataDst::Memory ||
                            dma_data->dst_sel == DmaDataDst::MemoryUsingL2)) {
                    rasterizer->CopyBuffer(dma_data->DstAddress<VAddr>(), dma_data->src_addr_lo,
                                           dma_data->NumBytes(), false, true);
                } else if ((dma_data->src_sel == DmaDataSrc::Memory ||
                            dma_data->src_sel == DmaDataSrc::MemoryUsingL2) &&
                           (dma_data->dst_sel == DmaDataDst::Memory ||
                            dma_data->dst_sel == DmaDataDst::MemoryUsingL2)) {
                    rasterizer->CopyBuffer(dma_data->DstAddress<VAddr>(),
                                           dma_data->SrcAddress<VAddr>(), dma_data->NumBytes(),
                                           false, false);
                } else {
                    UNREACHABLE_MSG("WriteData src_sel = %u, dst_sel = %u", u32(dma_data->src_sel),
                                    u32(dma_data->dst_sel));
                }
                break;
            }
            case PM4ItOpcode::WriteData: {
                const AmdGpu::PM4CmdWriteData* write_data =
                    reinterpret_cast<const PM4CmdWriteData*>(header);
                ASSERT(write_data->dst_sel.Value() == 2 || write_data->dst_sel.Value() == 5);
                const u32 data_size = (header->type3.count.Value() - 2) * 4;
                u64* address = write_data->Address<u64*>();
                if (!write_data->wr_one_addr.Value()) {
                    if (rasterizer) {
                        rasterizer->OnFence();
                    }
                    memcpy(address, write_data->data, data_size);
                } else {
                    UNREACHABLE();
                }
                break;
            }
            case PM4ItOpcode::CopyData: {
                const AmdGpu::PM4CmdCopyData* copy_data =
                    reinterpret_cast<const PM4CmdCopyData*>(header);
                LOG_WARNING(Render,
                            "unhandled IT_COPY_DATA src_sel = %u, dst_sel = %u, "
                            "count_sel = %u, wr_confirm = %u, engine_sel = %u",
                            u32(copy_data->src_sel.Value()), u32(copy_data->dst_sel.Value()),
                            copy_data->count_sel.Value(), copy_data->wr_confirm.Value(),
                            u32(copy_data->engine_sel.Value()));
                break;
            }
            case PM4ItOpcode::MemSemaphore: {
                const AmdGpu::PM4CmdMemSemaphore* mem_semaphore =
                    reinterpret_cast<const PM4CmdMemSemaphore*>(header);
                if (mem_semaphore->IsSignaling()) {
                    mem_semaphore->Signal();
                } else {
                    while (!mem_semaphore->Signaled()) {
                        YIELD_GFX();
                    }
                    mem_semaphore->Decrement();
                }
                break;
            }
            case PM4ItOpcode::AcquireMem: {
                // const auto* acquire_mem = reinterpret_cast<PM4CmdAcquireMem*>(header);
                break;
            }
            case PM4ItOpcode::Rewind: {
                if (!rasterizer) {
                    break;
                }
                const PM4CmdRewind* rewind = reinterpret_cast<const PM4CmdRewind*>(header);
                while (!rewind->Valid()) {
                    YIELD_GFX();
                }
                break;
            }
            case PM4ItOpcode::WaitRegMem: {
                const AmdGpu::PM4CmdWaitRegMem* wait_reg_mem =
                    reinterpret_cast<const PM4CmdWaitRegMem*>(header);
                // ASSERT(wait_reg_mem->engine.Value() == PM4CmdWaitRegMem::Engine::Me);
                // Optimization: VO label waits are special because the emulator
                // will write to the label when presentation is finished. So if
                // there are no other submits to yield to we can sleep the thread
                // instead and allow other tasks to run.
                const u64* wait_addr = wait_reg_mem->Address<u64*>();
                if (videoout_port_is_vo_label(vo_port, wait_addr) && [this] {
                        // Everything submitted so far is in the GFX queue
                        TaskQueue& gfx_submits = mapped_queues[GfxQueueId].submits;
                        gfx_submits.Take();
                        return static_cast<u32>(retro_atomic_load_seq_cst_int(&num_submits)) ==
                               gfx_submits.size;
                    }()) {
                    struct LabelWait {
                        const PM4CmdWaitRegMem* wait_reg_mem;
                        const u32* reg_array;
                    } label_wait{wait_reg_mem, regs.reg_array};
                    videoout_port_wait_vo_label(
                        vo_port,
                        [](void* ctx) -> int {
                            const auto* w = static_cast<const LabelWait*>(ctx);
                            return w->wait_reg_mem->Test(w->reg_array) ? 1 : 0;
                        },
                        &label_wait);
                    break;
                }
                while (!wait_reg_mem->Test(regs.reg_array)) {
                    YIELD_GFX();
                }
                break;
            }
            case PM4ItOpcode::IndirectBuffer: {
                const AmdGpu::PM4CmdIndirectBuffer* indirect_buffer =
                    reinterpret_cast<const PM4CmdIndirectBuffer*>(header);
                Liverpool::Task task = ProcessGraphics(
                    {indirect_buffer->Address<const u32>(), indirect_buffer->ib_size});
                SCOPE_EXIT {
                    task.handle.destroy();
                };
                RESUME_GFX(task);

                while (!task.handle.done()) {
                    YIELD_GFX();
                    RESUME_GFX(task);
                }
                break;
            }
            case PM4ItOpcode::IncrementDeCounter: {
                ++cblock.de_count;
                break;
            }
            case PM4ItOpcode::WaitOnCeCounter: {
                while (cblock.ce_count <= cblock.de_count) {
                    YIELD_GFX();
                }
                break;
            }
            case PM4ItOpcode::PfpSyncMe: {
                break;
            }
            case PM4ItOpcode::StrmoutBufferUpdate: {
                const AmdGpu::PM4CmdStrmoutBufferUpdate* strmout =
                    reinterpret_cast<const PM4CmdStrmoutBufferUpdate*>(header);
                LOG_WARNING(Render_Vulkan,
                            "Unimplemented IT_STRMOUT_BUFFER_UPDATE, update_memory = %u, "
                            "source_select = %s, buffer_select = %u",
                            strmout->update_memory.Value(),
                            enum_name(&enum_names_AmdGpu_SourceSelect,
                                      static_cast<s64>(strmout->source_select.Value())),
                            strmout->buffer_select.Value());
                break;
            }
            case PM4ItOpcode::GetLodStats: {
                LOG_WARNING(Render_Vulkan, "Unimplemented IT_GET_LOD_STATS");
                break;
            }
            case PM4ItOpcode::CondExec: {
                const AmdGpu::PM4CmdCondExec* cond_exec =
                    reinterpret_cast<const PM4CmdCondExec*>(header);
                if (cond_exec->command.Value() != 0) {
                    LOG_WARNING(Render, "IT_COND_EXEC used a reserved command");
                }
                const bool skip = *cond_exec->Address() == false;
                if (skip) {
                    dcb = NextPacket(dcb,
                                     header->type3.NumWords() + 1 + cond_exec->exec_count.Value());
                    continue;
                }
                break;
            }
            default:
                UNREACHABLE_MSG("Unknown PM4 type 3 opcode 0x%x with count %u",
                                static_cast<u32>(opcode), count);
            }
            dcb = NextPacket(dcb, header->type3.NumWords() + 1);
            break;
        }
    }
}

template <bool is_indirect>
Liverpool::Task Liverpool::ProcessCompute(Common::Span<const u32> acb, u32 vqid) {
    AmdGpu::Liverpool::AscQueueInfo& queue = asc_queues[{vqid}];

    struct IndirectPatch {
        const PM4Header* header;
        VAddr indirect_addr;
    };
    Common::SmallVector<IndirectPatch, 4> indirect_patches;

    VAddr base_addr = reinterpret_cast<VAddr>(acb.data());
    size_t acb_size = acb.size_bytes();
    while (!acb.empty()) {
        ProcessCommands();

        const AmdGpu::PM4Header* header = reinterpret_cast<const PM4Header*>(acb.data());
        u32 next_dw_off = header->type3.NumWords() + 1;

        // If we have a buffered packet, use it.
        if (queue.tmp_dwords > 0) [[unlikely]] {
            header = reinterpret_cast<const PM4Header*>(queue.tmp_packet);
            next_dw_off = header->type3.NumWords() + 1 - queue.tmp_dwords;
            memcpy(queue.tmp_packet + queue.tmp_dwords, acb.data(), next_dw_off * sizeof(u32));
            queue.tmp_dwords = 0;
        }

        // If the packet is split across ring boundary, buffer until next submission
        if (next_dw_off > acb.size()) [[unlikely]] {
            memcpy(queue.tmp_packet, acb.data(), acb.size_bytes());
            queue.tmp_dwords = acb.size();
            if constexpr (!is_indirect) {
                *queue.read_addr += acb.size();
                *queue.read_addr %= queue.ring_size_dw;
            }
            break;
        }

        if (header->type == 2) {
            // Type-2 packet are used for padding purposes
            next_dw_off = 1;
            acb = NextPacket(acb, next_dw_off);
            if constexpr (!is_indirect) {
                *queue.read_addr += next_dw_off;
                *queue.read_addr %= queue.ring_size_dw;
            }
            continue;
        }

        if (header->type != 3) {
            // No other types of packets were spotted so far
            UNREACHABLE_MSG("Invalid PM4 type %u", header->type.Value());
        }

        const PM4ItOpcode opcode = header->type3.opcode;

        const u32* it_body = reinterpret_cast<const u32*>(header) + 1;
        switch (opcode) {
        case PM4ItOpcode::Nop: {
            const AmdGpu::PM4CmdNop* nop = reinterpret_cast<const PM4CmdNop*>(header);
            break;
        }
        case PM4ItOpcode::IndirectBuffer: {
            const AmdGpu::PM4CmdIndirectBuffer* indirect_buffer =
                reinterpret_cast<const PM4CmdIndirectBuffer*>(header);
            Liverpool::Task task = ProcessCompute<true>(
                {indirect_buffer->Address<const u32>(), indirect_buffer->ib_size}, vqid);
            SCOPE_EXIT {
                task.handle.destroy();
            };
            RESUME_ASC(task, vqid);

            while (!task.handle.done()) {
                YIELD_ASC(vqid);
                RESUME_ASC(task, vqid);
            }
            break;
        }
        case PM4ItOpcode::DmaData: {
            const AmdGpu::PM4DmaData* dma_data = reinterpret_cast<const PM4DmaData*>(header);
            if (dma_data->dst_addr_lo == 0x3022C || !rasterizer) {
                break;
            }
            ASSERT(dma_data->command.das == 0);
            if (dma_data->src_sel == DmaDataSrc::Data && dma_data->dst_sel == DmaDataDst::Gds) {
                rasterizer->FillBuffer(dma_data->dst_addr_lo, dma_data->NumBytes(), dma_data->data,
                                       true);
            } else if ((dma_data->src_sel == DmaDataSrc::Memory ||
                        dma_data->src_sel == DmaDataSrc::MemoryUsingL2) &&
                       dma_data->dst_sel == DmaDataDst::Gds) {
                rasterizer->CopyBuffer(dma_data->dst_addr_lo, dma_data->SrcAddress<VAddr>(),
                                       dma_data->NumBytes(), true, false);
            } else if (dma_data->src_sel == DmaDataSrc::Data &&
                       (dma_data->dst_sel == DmaDataDst::Memory ||
                        dma_data->dst_sel == DmaDataDst::MemoryUsingL2)) {
                rasterizer->FillBuffer(dma_data->DstAddress<VAddr>(), dma_data->NumBytes(),
                                       dma_data->data, false);
            } else if (dma_data->src_sel == DmaDataSrc::Gds &&
                       (dma_data->dst_sel == DmaDataDst::Memory ||
                        dma_data->dst_sel == DmaDataDst::MemoryUsingL2)) {
                rasterizer->CopyBuffer(dma_data->DstAddress<VAddr>(), dma_data->src_addr_lo,
                                       dma_data->NumBytes(), false, true);
            } else if ((dma_data->src_sel == DmaDataSrc::Memory ||
                        dma_data->src_sel == DmaDataSrc::MemoryUsingL2) &&
                       (dma_data->dst_sel == DmaDataDst::Memory ||
                        dma_data->dst_sel == DmaDataDst::MemoryUsingL2)) {
                const u32 num_bytes = dma_data->NumBytes();
                const VAddr src_addr = dma_data->SrcAddress<VAddr>();
                const VAddr dst_addr = dma_data->DstAddress<VAddr>();
                const PM4Header* header =
                    reinterpret_cast<const PM4Header*>(dst_addr - sizeof(PM4Header));
                if (dst_addr >= base_addr && dst_addr < base_addr + acb_size &&
                    num_bytes == sizeof(PM4CmdDispatchIndirect::GroupDimensions) &&
                    header->type == 3 && header->type3.opcode == PM4ItOpcode::DispatchDirect) {
                    indirect_patches.emplace_back(header, src_addr);
                } else {
                    rasterizer->CopyBuffer(dst_addr, src_addr, num_bytes, false, false);
                }
            } else {
                UNREACHABLE_MSG("WriteData src_sel = %u, dst_sel = %u", u32(dma_data->src_sel),
                                u32(dma_data->dst_sel));
            }
            break;
        }
        case PM4ItOpcode::AcquireMem: {
            break;
        }
        case PM4ItOpcode::Rewind: {
            if (!rasterizer) {
                break;
            }
            const PM4CmdRewind* rewind = reinterpret_cast<const PM4CmdRewind*>(header);
            while (!rewind->Valid()) {
                YIELD_ASC(vqid);
            }
            break;
        }
        case PM4ItOpcode::SetShReg: {
            const AmdGpu::PM4CmdSetData* set_data = reinterpret_cast<const PM4CmdSetData*>(header);
            const size_t set_size = (header->type3.NumWords() - 1) * sizeof(u32);

            if (set_data->reg_offset >= 0x200 &&
                set_data->reg_offset <= (0x200 + sizeof(ComputeProgram) / 4)) {
                ASSERT(set_size <= sizeof(ComputeProgram));
                u32* addr = reinterpret_cast<u32*>(&mapped_queues[vqid + 1].cs_state) +
                            (set_data->reg_offset - 0x200);
                memcpy(addr, header + 2, set_size);
            } else {
                memcpy(&regs.reg_array[Regs::ShRegWordOffset + set_data->reg_offset], header + 2,
                       set_size);
            }
            break;
        }
        case PM4ItOpcode::SetQueueReg: {
            const AmdGpu::PM4CmdSetQueueReg* set_data =
                reinterpret_cast<const PM4CmdSetQueueReg*>(header);
            LOG_WARNING(Render, "Encountered compute SetQueueReg: vqid = %u, reg_offset = 0x%x",
                        set_data->vqid.Value(), set_data->reg_offset.Value());
            break;
        }
        case PM4ItOpcode::DispatchDirect: {
            const AmdGpu::PM4CmdDispatchDirect* dispatch_direct =
                reinterpret_cast<const PM4CmdDispatchDirect*>(header);
            const IndirectPatch* patch = nullptr;
            for (const IndirectPatch& candidate : indirect_patches) {
                if (candidate.header == header) {
                    patch = &candidate;
                    break;
                }
            }
            if (patch) {
                const size_t size = sizeof(PM4CmdDispatchIndirect::GroupDimensions);
                rasterizer->DispatchIndirect(patch->indirect_addr, 0, size);
                break;
            }
            AmdGpu::ComputeProgram& cs_program = GetCsRegs();
            cs_program.dim_x = dispatch_direct->dim_x;
            cs_program.dim_y = dispatch_direct->dim_y;
            cs_program.dim_z = dispatch_direct->dim_z;
            cs_program.dispatch_initiator = dispatch_direct->dispatch_initiator;
            if (!rasterizer || (cs_program.dispatch_initiator & 1) == 0) {
                break;
            }
            const void* const cmd_address = reinterpret_cast<const void*>(header);
            rasterizer->ScopeMarker("asc[%u]:0x%llx:DispatchDirect", vqid, cmd_address,
                                    [&] { rasterizer->DispatchDirect(); });
            break;
        }
        case PM4ItOpcode::DispatchIndirect: {
            const AmdGpu::PM4CmdDispatchIndirectMec* dispatch_indirect =
                reinterpret_cast<const PM4CmdDispatchIndirectMec*>(header);
            AmdGpu::ComputeProgram& cs_program = GetCsRegs();
            const VAddr ib_address = dispatch_indirect->Address<VAddr>();
            const size_t size = sizeof(PM4CmdDispatchIndirect::GroupDimensions);
            if (!rasterizer || (cs_program.dispatch_initiator & 1) == 0) {
                break;
            }
            const void* const cmd_address = reinterpret_cast<const void*>(header);
            rasterizer->ScopeMarker("asc[%u]:0x%llx:DispatchIndirect", vqid, cmd_address,
                                    [&] { rasterizer->DispatchIndirect(ib_address, 0, size); });
            break;
        }
        case PM4ItOpcode::WriteData: {
            const AmdGpu::PM4CmdWriteData* write_data =
                reinterpret_cast<const PM4CmdWriteData*>(header);
            ASSERT(write_data->dst_sel.Value() == 2 || write_data->dst_sel.Value() == 5);
            const u32 data_size = (header->type3.count.Value() - 2) * 4;
            if (!write_data->wr_one_addr.Value()) {
                if (rasterizer) {
                    rasterizer->OnFence();
                }
                memcpy(write_data->Address<void*>(), write_data->data, data_size);
            } else {
                UNREACHABLE();
            }
            break;
        }
        case PM4ItOpcode::MemSemaphore: {
            const AmdGpu::PM4CmdMemSemaphore* mem_semaphore =
                reinterpret_cast<const PM4CmdMemSemaphore*>(header);
            if (mem_semaphore->IsSignaling()) {
                mem_semaphore->Signal();
            } else {
                while (!mem_semaphore->Signaled()) {
                    YIELD_ASC(vqid);
                }
                mem_semaphore->Decrement();
            }
            break;
        }
        case PM4ItOpcode::WaitRegMem: {
            const AmdGpu::PM4CmdWaitRegMem* wait_reg_mem =
                reinterpret_cast<const PM4CmdWaitRegMem*>(header);
            ASSERT(wait_reg_mem->engine.Value() == PM4CmdWaitRegMem::Engine::Me);
            while (!wait_reg_mem->Test(regs.reg_array)) {
                YIELD_ASC(vqid);
            }
            break;
        }
        case PM4ItOpcode::ReleaseMem: {
            const AmdGpu::PM4CmdReleaseMem* release_mem =
                reinterpret_cast<const PM4CmdReleaseMem*>(header);
            if (rasterizer) {
                rasterizer->OnFence();
            }
            release_mem->SignalFence(
                [pipe_id = queue.pipe_id] {
                    Platform::IrqC::Instance()->Signal(static_cast<Platform::InterruptId>(pipe_id));
                },
                [this](VAddr dst, u16 gds_index, u16 num_dwords) {
                    rasterizer->CopyBuffer(dst, gds_index, num_dwords * sizeof(u32), false, true);
                });
            break;
        }
        case PM4ItOpcode::EventWrite: {
            // const auto* event = reinterpret_cast<const PM4CmdEventWrite*>(header);
            break;
        }
        default:
            UNREACHABLE_MSG("Unknown PM4 type 3 opcode 0x%x with count %u",
                            static_cast<u32>(opcode), header->type3.NumWords());
        }

        acb = NextPacket(acb, next_dw_off);

        if constexpr (!is_indirect) {
            *queue.read_addr += next_dw_off;
            *queue.read_addr %= queue.ring_size_dw;
        }
    }
}

Liverpool::CmdBuffer Liverpool::CopyCmdBuffers(Common::Span<const u32> dcb,
                                               Common::Span<const u32> ccb) {
    // Into room the GPU thread is done with; with none free, read where the game has them, as
    // with the copying off (cmd_copy_ring.h)
    const u32* dcb_copy = nullptr;
    const u32* ccb_copy = nullptr;
    if (!copy_ring || !cmd_copy_ring_copy(copy_ring, dcb.data(), dcb.size(), ccb.data(), ccb.size(),
                                          &dcb_copy, &ccb_copy)) {
        return CmdBuffer{dcb, ccb};
    }
    return CmdBuffer{Common::Span<const u32>{dcb_copy, dcb.size()},
                     Common::Span<const u32>{ccb_copy, ccb.size()}};
}

void Liverpool::SubmitGfx(Common::Span<const u32> dcb, Common::Span<const u32> ccb) {
    AmdGpu::Liverpool::GpuQueue& queue = mapped_queues[GfxQueueId];

    if (emu_settings_get_copy_gpu_buffers()) {
        const CmdBuffer copied = CopyCmdBuffers(dcb, ccb);
        dcb = copied.dcb;
        ccb = copied.ccb;
    }

    Liverpool::Task task = ProcessGraphics(dcb);
    // Counted before the GPU thread can finish it, as everything below is
    if (copy_ring) {
        cmd_copy_ring_submitted(copy_ring, !ccb.empty());
    }
    // Counted before it is visible, so the GPU thread never takes num_submits below zero
    if (!ccb.empty()) {
        retro_atomic_fetch_add_int(&num_submits, 1);
        ce_submits.Push(ProcessCeUpdate(ccb).handle);
    }
    retro_atomic_fetch_add_int(&num_submits, 1);
    queue.submits.Push(task.handle);
    retro_eventcount_notify(&work_ec);
}

void Liverpool::SubmitAsc(u32 gnm_vqid, Common::Span<const u32> acb) {
    ASSERT_MSG(gnm_vqid > 0 && gnm_vqid < NumTotalQueues, "Invalid virtual ASC queue index");
    AmdGpu::Liverpool::GpuQueue& queue = mapped_queues[gnm_vqid];

    const u32 vqid = gnm_vqid - 1;
    const Liverpool::Task& task = ProcessCompute(acb, vqid);
    u32 mapped = static_cast<u32>(retro_atomic_load_relaxed_int(&num_mapped_queues));
    while (mapped < gnm_vqid + 1 &&
           !(retro_atomic_cas_int(&num_mapped_queues, mapped, gnm_vqid + 1) ||
             ((mapped = static_cast<u32>(retro_atomic_load_acquire_int(&num_mapped_queues))),
              false))) {
    }
    retro_atomic_fetch_add_int(&num_submits, 1);
    queue.submits.Push(task.handle);
    retro_eventcount_notify(&work_ec);
}

} // namespace AmdGpu
