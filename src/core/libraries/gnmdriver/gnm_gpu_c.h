/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef SHADPS4_LIBRARIES_GNMDRIVER_GNM_GPU_C_H
#define SHADPS4_LIBRARIES_GNMDRIVER_GNM_GPU_C_H

/* The GPU libSceGnmDriver submits to (AmdGpu::Liverpool) and the presenter, both made and held
 * in video_core/amdgpu/liverpool.cpp, and the event queues a game has the GPU's interrupts told
 * to. */

#include <boolean.h>
#include <retro_common_api.h>

#include "common/types.h"

RETRO_BEGIN_DECLS

/* AmdGpu::Liverpool::NumComputePipes, NumQueuesPerPipe and NumComputeRings */
#define GNM_GPU_NUM_COMPUTE_PIPES 7u
#define GNM_GPU_NUM_QUEUES_PER_PIPE 8u
#define GNM_GPU_NUM_COMPUTE_RINGS (GNM_GPU_NUM_COMPUTE_PIPES * GNM_GPU_NUM_QUEUES_PER_PIPE)

/* Makes the GPU and the presenter anew */
void gnm_gpu_create(void);
/* Liverpool::ReserveCopyBufferSpace */
void gnm_gpu_reserve_copy_buffer_space(void);
/* @on_idle runs at each GPU idle interrupt (Platform::InterruptId::GpuIdle) */
void gnm_gpu_on_idle(void (*on_idle)(void));
/* Liverpool::IsGpuIdle */
bool gnm_gpu_is_idle(void);
/* Liverpool::SubmitGfx of @dcb_dw words at @dcb and @ccb_dw at @ccb, borrowed as there */
void gnm_gpu_submit_gfx(const u32* dcb, u32 dcb_dw, const u32* ccb, u32 ccb_dw);
/* Liverpool::SubmitAsc to the queue the game knows as @gnm_vqid */
void gnm_gpu_submit_asc(u32 gnm_vqid, const u32* acb, u32 acb_dw);
/* Liverpool::SubmitDone */
void gnm_gpu_submit_done(void);
/* A compute queue mapped (Liverpool::asc_queues): its index, one less than the game's id */
u32 gnm_gpu_asc_map(uintptr_t ring_base_addr, u32* read_ptr_addr, u32 ring_size_dw, u32 pipe_id);
void gnm_gpu_asc_unmap(u32 index);
/* The ring of the queue at @index and its size in words */
void gnm_gpu_asc_queue(u32 index, const u32** map_addr, u32* ring_size_dw);
/* The registers VGT_ESGS_RING_SIZE and VGT_GSVS_RING_SIZE, if there is a GPU */
void gnm_gpu_set_gs_ring_sizes(u32 esgs, u32 gsvs);
/* Where the tessellation factor ring is: the last 256 MiB of the system reserved area */
uintptr_t gnm_gpu_tessellation_ring_base(void);

/* sceGnmAddEqEvent: the event queue @eq is told of the interrupt @id from now on. */
s32 gnm_eq_event_add(s64 eq, u64 id, void* udata);
/* sceGnmDeleteEqEvent */
s32 gnm_eq_event_delete(s64 eq, u64 id);

RETRO_END_DECLS

#endif
