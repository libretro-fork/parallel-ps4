/* SPDX-FileCopyrightText: Copyright 2024-2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

/* libSceGnmDriver: the command packets a game has written into its buffers, and its submissions.
 * The GPU they go to (AmdGpu::Liverpool) and the presenter are reached through gnm_gpu_c.h
 * (video_core/amdgpu/liverpool.cpp). Registered in libs.cpp. */

#include <string.h>

#include <retro_atomic.h>
#include <retro_inline.h>
#include <rthreads/retro_eventcount.h>

#include "common/c_bridge.h"
#include "common/combine.h"
#include "core/emulator_settings.h"
#include "core/libraries/error_codes.h"
#include "core/libraries/gnmdriver/gnm_error.h"
#include "core/libraries/gnmdriver/gnm_gpu_c.h"
#include "core/libraries/gnmdriver/gnmdriver.h"
#include "core/libraries/gnmdriver/gnmdriver_init.h"
#include "core/libraries/kernel/kernel_c.h"
#include "core/libraries/kernel/orbis_error.h"
#include "core/libraries/videoout/video_out.h"

/* A type 3 packet's header: @count is one less than the words that follow it */
#define PM4_HDR_N(op, count, type, pred)                                                           \
    ((u32)(3u << 30 | (((u32)(count)) & 0x3fffu) << 16 | ((u32)(op)) << 8 | ((u32)(type)) << 1 |   \
           ((u32)(pred) & 1u)))
/* The same of a packet of @size words after the header */
#define PM4_HDR(op, size, type, pred) PM4_HDR_N(op, (u32)(size) - 1u, type, pred)

#define PM4_GFX 0u
#define PM4_COMPUTE 1u

/* AmdGpu::PM4ItOpcode */
#define PM4_IT_NOP 0x10
#define PM4_IT_DISPATCH_DIRECT 0x15
#define PM4_IT_DISPATCH_INDIRECT 0x16
#define PM4_IT_DRAW_INDIRECT 0x24
#define PM4_IT_DRAW_INDEX_INDIRECT 0x25
#define PM4_IT_DRAW_INDEX2 0x27
#define PM4_IT_DRAW_INDIRECT_MULTI 0x2c
#define PM4_IT_DRAW_INDEX_AUTO 0x2d
#define PM4_IT_DRAW_INDEX_OFFSET2 0x35
#define PM4_IT_WRITE_DATA 0x37
#define PM4_IT_DRAW_INDEX_INDIRECT_MULTI 0x38
#define PM4_IT_MEM_SEMAPHORE 0x39
#define PM4_IT_WAIT_REG_MEM 0x3c
#define PM4_IT_EVENT_WRITE_EOP 0x47
#define PM4_IT_ACQUIRE_MEM 0x58
#define PM4_IT_SET_CONTEXT_REG 0x69
#define PM4_IT_SET_SH_REG 0x76
#define PM4_IT_SET_UCONFIG_REG 0x79
#define PM4_IT_DRAW_INDIRECT_COUNT_MULTI 0x9c
#define PM4_IT_DRAW_INDEX_INDIRECT_COUNT_MULTI 0x9d

/* What a NOP packet's first word says it is (AmdGpu::PM4CmdNop::PayloadType) */
#define PM4_NOP_DEBUG_MARKER_PUSH 0x68750001u
#define PM4_NOP_DEBUG_MARKER_POP 0x68750002u
#define PM4_NOP_DEBUG_SET_MARKER 0x68750003u
#define PM4_NOP_DEBUG_COLOR_MARKER_PUSH 0x6875000eu
#define PM4_NOP_PATCHED_FLIP 0x68750776u
#define PM4_NOP_PREPARE_FLIP 0x68750777u
#define PM4_NOP_PREPARE_FLIP_LABEL 0x68750778u
#define PM4_NOP_PREPARE_FLIP_INTERRUPT 0x68750780u
#define PM4_NOP_PREPARE_FLIP_INTERRUPT_LABEL 0x68750781u

/* The sizes of AmdGpu::DrawIndirectArgs and DrawIndexedIndirectArgs */
#define DRAW_INDIRECT_ARGS_SIZE 0x10u
#define DRAW_INDEXED_INDIRECT_ARGS_SIZE 0x14u

/* What an event queue is told of (the interrupts of the same numbers) */
#define GNM_EVENT_COMPUTE6_REL_MEM 0x06u
#define GNM_EVENT_GFX_EOP 0x40u

enum {
    GNM_STAGE_CS,
    GNM_STAGE_PS,
    GNM_STAGE_VS,
    GNM_STAGE_GS,
    GNM_STAGE_ES,
    GNM_STAGE_HS,
    GNM_STAGE_LS,
    GNM_STAGE_MAX
};

static const u32 indirect_sgpr_offsets[GNM_STAGE_MAX] = {0u, 0u, 0x4cu, 0u, 0xccu, 0u, 0x14cu};

/* Gates use of what appear to be the neo-mode init sequences but with the older
 * IA_MULTI_VGT_PARAM register address. No idea what this is for as the ioctl
 * that controls it is still a mystery, but leaving the sequences in gated behind
 * this flag in case we need it in the future. */
#define USE_NEO_COMPAT_SEQUENCES 0

/* In case if `submitDone` is issued we need to block submissions until GPU idle. Set by the
 * submitter, cleared by it or by the GPU idle interrupt; wait_gpu_idle parks on idle_ec. */
static retro_atomic_int_t submission_lock;
static retro_eventcount_t idle_ec;
/* The submissions run one at a time, in order, and so does all the state below */
static combiner_t submit_combiner;
static bool submit_combiner_made;
static bool send_init_packet = true; /* initialize HW state before first game's submit in a frame */
static s32 sdk_version;

static u32 asc_next_offs_dw[GNM_GPU_NUM_COMPUTE_RINGS];

/* This address is initialized in sceGnmGetTheTessellationFactorRingBufferBaseAddress */
static uintptr_t tessellation_factors_ring_addr = (uintptr_t)-1;
#define TESSELLATION_OFFCHIP_BUFFER_SIZE 0x800000u

static void release_submission_lock(void) {
    (void)retro_atomic_exchange_int(&submission_lock, 0);
    retro_eventcount_notify(&idle_ec);
}

/* The GPU idle interrupt */
static void reset_submission_lock(void) {
    /* The GPU thread signals idle after it has seen its queues empty, and the game can submit
     * in between: the signal for the init sequence then released the lock while the frame's
     * command buffer was still waiting, the game went on to its next frame and wrote over that
     * buffer ("Out of order flip IRQ"). With something submitted the GPU thread is not idle; it
     * signals again once that is done. */
    if (!gnm_gpu_is_idle()) {
        return;
    }
    release_submission_lock();
}

/* After the submit, before the submit done: blocks submissions until the GPU is idle. The lock
 * is taken first and the GPU looked at after, and the interrupt looks at the GPU before it
 * releases, so whichever sees the GPU idle last releases it: none is left held. */
static void take_submission_lock(void) {
    (void)retro_atomic_exchange_int(&submission_lock, 1);
    if (gnm_gpu_is_idle()) {
        release_submission_lock();
    }
}

static int submission_unlocked(void* ctx) {
    (void)ctx;
    return (u32)retro_atomic_load_seq_cst_int(&submission_lock) == 0;
}

static void wait_gpu_idle(void) {
    /* Not counted running meanwhile (kernel_game_wait) */
    kernel_game_wait(&idle_ec, 0, submission_unlocked, NULL);
}

/* The words of a packet, one after the other; the word after them */

static INLINE u32* pm4_w1(u32* p, u32 a) {
    p[0] = a;
    return p + 1;
}

static INLINE u32* pm4_w3(u32* p, u32 a, u32 b, u32 c) {
    p[0] = a;
    p[1] = b;
    p[2] = c;
    return p + 3;
}

static INLINE u32* pm4_w4(u32* p, u32 a, u32 b, u32 c, u32 d) {
    p[0] = a;
    p[1] = b;
    p[2] = c;
    p[3] = d;
    return p + 4;
}

static INLINE u32* pm4_w5(u32* p, u32 a, u32 b, u32 c, u32 d, u32 e) {
    p[0] = a;
    p[1] = b;
    p[2] = c;
    p[3] = d;
    p[4] = e;
    return p + 5;
}

static INLINE u32* pm4_w6(u32* p, u32 a, u32 b, u32 c, u32 d, u32 e, u32 f) {
    p[0] = a;
    p[1] = b;
    p[2] = c;
    p[3] = d;
    p[4] = e;
    p[5] = f;
    return p + 6;
}

/* A NOP packet with @data_block_size words after its header, the first of them zero (only one
 * of them is initialized) */
static INLINE u32* pm4_nop(u32* cmdbuf, u32 data_block_size) {
    cmdbuf[0] = PM4_HDR(PM4_IT_NOP, data_block_size, PM4_GFX, 0);
    cmdbuf[1] = 0u;
    return cmdbuf + data_block_size + 1 /* header */;
}

static INLINE u32* clear_context_state(u32* cmdbuf) {
    static const u32 clear_state_sequence[0xc] = {
        0xc0012800u, 0x80000000u, 0x80000000u, 0xc0001200u, 0u, 0xc0055800u,
        0x2ec47fc0u, 0xffffffffu, 0u,          0u,          0u, 10u,
    };
    memcpy(cmdbuf, clear_state_sequence, sizeof(clear_state_sequence));
    return cmdbuf + 0xc;
}

s32 PS4_SYSV_ABI sceGnmAddEqEvent(s64 eq, u64 id, void* udata) {
    return gnm_eq_event_add(eq, id, udata);
}

int PS4_SYSV_ABI sceGnmAreSubmitsAllowed(void) {
    return (u32)(retro_atomic_load_seq_cst_int(&submission_lock)) == 0;
}

int PS4_SYSV_ABI sceGnmBeginWorkload(u32 workload_stream, u64* workload) {
    if (workload) {
        *workload = (-(u32)(workload_stream < 0x10) & 1);
        return 0xf < workload_stream;
    }
    return 3;
}

s32 PS4_SYSV_ABI sceGnmComputeWaitOnAddress(u32* cmdbuf, u32 size, uintptr_t addr, u32 mask,
                                            u32 cmp_func, u32 ref) {
    if (cmdbuf && (size == 0xe)) {
        const u32 is_mem = addr > 0xffffu;
        const u32 addr_mask = is_mem ? 0xfffffffcu : 0xffffu;
        cmdbuf = pm4_nop(cmdbuf, 3);
        /* A WAIT_REG_MEM: what to compare and where, the address, the reference, the mask, the
         * poll interval */
        cmdbuf[0] = PM4_HDR_N(PM4_IT_WAIT_REG_MEM, 5, PM4_GFX, 0);
        cmdbuf[1] = (is_mem << 4u) | (cmp_func & 7u);
        cmdbuf[2] = (u32)(addr & addr_mask);
        cmdbuf[3] = (u32)((u64)addr >> 32u);
        cmdbuf[4] = ref;
        cmdbuf[5] = mask;
        cmdbuf[6] = 10u;
        pm4_nop(cmdbuf + 7, 2);
        return ORBIS_OK;
    }
    return -1;
}

s32 PS4_SYSV_ABI sceGnmComputeWaitSemaphore(u32* cmdbuf, u32 size, u64 sem_addr, u32 flags) {
    if (cmdbuf && size == 0xa) {
        cmdbuf = pm4_nop(cmdbuf, 3);
        cmdbuf = pm4_w1(cmdbuf, PM4_HDR(PM4_IT_MEM_SEMAPHORE, 2, PM4_GFX, 0));
        cmdbuf[0] = (u32)(sem_addr);
        cmdbuf[1] = (flags == 1 ? 0x100000 : 0) - 0x20000000 + (u32)(sem_addr >> 0x20);
        cmdbuf += 2;

        pm4_nop(cmdbuf, 2);
        return ORBIS_OK;
    }
    return -1;
}

int PS4_SYSV_ABI sceGnmCreateWorkloadStream(u64 param1, u32* workload_stream) {
    if (param1 != 0 && workload_stream) {
        *workload_stream = 1;
        return 0;
    }
    return 3;
}

int PS4_SYSV_ABI sceGnmDebuggerGetAddressWatch(void) {
    /* Not available in retail firmware */
    return ORBIS_GNM_ERROR_FAILURE;
}

int PS4_SYSV_ABI sceGnmDebuggerHaltWavefront(void) {
    /* Not available in retail firmware */
    return ORBIS_GNM_ERROR_FAILURE;
}

int PS4_SYSV_ABI sceGnmDebuggerReadGds(void) {
    /* Not available in retail firmware */
    return ORBIS_GNM_ERROR_FAILURE;
}

int PS4_SYSV_ABI sceGnmDebuggerReadSqIndirectRegister(void) {
    /* Not available in retail firmware */
    return ORBIS_GNM_ERROR_FAILURE;
}

int PS4_SYSV_ABI sceGnmDebuggerResumeWavefront(void) {
    /* Not available in retail firmware */
    return ORBIS_GNM_ERROR_FAILURE;
}

int PS4_SYSV_ABI sceGnmDebuggerResumeWavefrontCreation(void) {
    /* Not available in retail firmware */
    return ORBIS_GNM_ERROR_FAILURE;
}

int PS4_SYSV_ABI sceGnmDebuggerSetAddressWatch(void) {
    /* Not available in retail firmware */
    return ORBIS_GNM_ERROR_FAILURE;
}

int PS4_SYSV_ABI sceGnmDebuggerWriteGds(void) {
    /* Not available in retail firmware */
    return ORBIS_GNM_ERROR_FAILURE;
}

int PS4_SYSV_ABI sceGnmDebuggerWriteSqIndirectRegister(void) {
    /* Not available in retail firmware */
    return ORBIS_GNM_ERROR_FAILURE;
}

int PS4_SYSV_ABI sceGnmDebugHardwareStatus(void) {
    c_log(C_LOG_LIB_GNM_DRIVER, C_LOG_LEVEL_ERROR, "sceGnmDebugHardwareStatus", "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceGnmDeleteEqEvent(s64 eq, u64 id) {
    return gnm_eq_event_delete(eq, id);
}

int PS4_SYSV_ABI sceGnmDestroyWorkloadStream(void) {
    c_log(C_LOG_LIB_GNM_DRIVER, C_LOG_LEVEL_ERROR, "sceGnmDestroyWorkloadStream",
          "(STUBBED) called");
    return ORBIS_OK;
}

typedef struct ding_dong_args {
    u32 gnm_vqid;
    u32 next_offs_dw;
} ding_dong_args;

static void ding_dong_run(void* ctx) {
    const ding_dong_args* a = (const ding_dong_args*)ctx;
    const u32 vqid = a->gnm_vqid - 1;
    const u32 next_offs_dw = a->next_offs_dw;
    const u32* map_addr;
    u32 ring_size_dw;
    u32* offs_dw;
    wait_gpu_idle();
    gnm_gpu_asc_queue(vqid, &map_addr, &ring_size_dw);
    offs_dw = &asc_next_offs_dw[vqid];
    if (next_offs_dw == *offs_dw) {
        return;
    }
    if (next_offs_dw < *offs_dw && next_offs_dw != 0) {
        /* For cases if a submission is split at the end of the ring buffer, we need to submit
         * it in two parts to handle the wrap */
        gnm_gpu_submit_asc(a->gnm_vqid, map_addr + *offs_dw, ring_size_dw - *offs_dw);
        *offs_dw = 0;
    }
    {
        const u32* acb_ptr = map_addr + *offs_dw;
        const u32 acb_size_dw = (next_offs_dw ? next_offs_dw : ring_size_dw) - *offs_dw;
        asc_next_offs_dw[vqid] = next_offs_dw;
        gnm_gpu_submit_asc(a->gnm_vqid, acb_ptr, acb_size_dw);
    }
}

void PS4_SYSV_ABI sceGnmDingDong(u32 gnm_vqid, u32 next_offs_dw) {
    ding_dong_args a;
    c_log(C_LOG_LIB_GNM_DRIVER, C_LOG_LEVEL_DEBUG, "sceGnmDingDong", "vqid %u, offset_dw %u",
          gnm_vqid, next_offs_dw);
    if (gnm_vqid == 0) {
        return;
    }
    a.gnm_vqid = gnm_vqid;
    a.next_offs_dw = next_offs_dw;
    combiner_run(&submit_combiner, ding_dong_run, &a);
}

void PS4_SYSV_ABI sceGnmDingDongForWorkload(u32 gnm_vqid, u32 next_offs_dw, u64 workload_id) {
    c_log(C_LOG_LIB_GNM_DRIVER, C_LOG_LEVEL_DEBUG, "sceGnmDingDongForWorkload",
          "called, redirecting to sceGnmDingDong");
    sceGnmDingDong(gnm_vqid, next_offs_dw);
}

int PS4_SYSV_ABI sceGnmDisableMipStatsReport(void) {
    c_log(C_LOG_LIB_GNM_DRIVER, C_LOG_LEVEL_ERROR, "sceGnmDisableMipStatsReport",
          "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceGnmDispatchDirect(u32* cmdbuf, u32 size, u32 threads_x, u32 threads_y,
                                      u32 threads_z, u32 flags) {
    if (cmdbuf && (size == 9) && ((s32)(threads_x | threads_y | threads_z) > -1)) {
        const u32 predicate = flags & 1 ? 1u : 0u;
        cmdbuf = pm4_w1(cmdbuf, PM4_HDR(PM4_IT_DISPATCH_DIRECT, 4, PM4_COMPUTE, predicate));
        cmdbuf = pm4_w3(cmdbuf, threads_x, threads_y, threads_z);
        cmdbuf[0] = (flags & 0x18) + 1; /* ordered append mode */

        pm4_nop(cmdbuf + 1, 3);
        return ORBIS_OK;
    }
    return -1;
}

s32 PS4_SYSV_ABI sceGnmDispatchIndirect(u32* cmdbuf, u32 size, u32 data_offset, u32 flags) {
    if (cmdbuf && (size == 7)) {
        const u32 predicate = flags & 1 ? 1u : 0u;
        cmdbuf = pm4_w1(cmdbuf, PM4_HDR(PM4_IT_DISPATCH_INDIRECT, 2, PM4_COMPUTE, predicate));
        cmdbuf[0] = data_offset;
        cmdbuf[1] = (flags & 0x18) + 1; /* ordered append mode */

        pm4_nop(cmdbuf + 2, 3);
        return ORBIS_OK;
    }
    return -1;
}

s32 PS4_SYSV_ABI sceGnmDispatchIndirectOnMec(u32* cmdbuf, u32 size, uintptr_t args, u32 modifier) {
    if (cmdbuf != NULL && size == 8 && args != 0 && ((args & 3u) == 0)) {
        cmdbuf[0] = 0xc0021602 | (modifier & 1u);
        memcpy(&cmdbuf[1], &args, sizeof(args));
        cmdbuf[3] = (modifier & 0x18) | 1u;
        cmdbuf[4] = 0xc0021000;
        cmdbuf[5] = 0;
        return ORBIS_OK;
    }
    return ORBIS_FAIL;
}

u32 PS4_SYSV_ABI sceGnmDispatchInitDefaultHardwareState(u32* cmdbuf, u32 size) {
    if (size < HW_INIT_PACKET_SIZE) {
        return 0;
    }

    cmdbuf = pm4_w3(cmdbuf, PM4_HDR(PM4_IT_SET_SH_REG, 2, PM4_COMPUTE, 0), 0x216u,
                    0xffffffffu); /* COMPUTE_STATIC_THREAD_MGMT_SE0 */
    cmdbuf = pm4_w3(cmdbuf, PM4_HDR(PM4_IT_SET_SH_REG, 2, PM4_COMPUTE, 0), 0x217u,
                    0xffffffffu); /* COMPUTE_STATIC_THREAD_MGMT_SE1 */

    if (kernel_is_neo_mode()) {
        cmdbuf = pm4_w3(cmdbuf, PM4_HDR(PM4_IT_SET_SH_REG, 2, PM4_COMPUTE, 0), 0x219u,
                        0xffffffffu); /* COMPUTE_STATIC_THREAD_MGMT_SE2 */
        cmdbuf = pm4_w3(cmdbuf, PM4_HDR(PM4_IT_SET_SH_REG, 2, PM4_COMPUTE, 0), 0x21au,
                        0xffffffffu); /* COMPUTE_STATIC_THREAD_MGMT_SE3 */
    }

    cmdbuf = pm4_w3(cmdbuf, PM4_HDR(PM4_IT_SET_SH_REG, 2, PM4_COMPUTE, 0), 0x215u,
                    0x170u); /* COMPUTE_RESOURCE_LIMITS */

    cmdbuf = pm4_w1(cmdbuf, PM4_HDR(PM4_IT_ACQUIRE_MEM, 6, PM4_GFX, 0));
    cmdbuf = pm4_w6(cmdbuf, 0x28000000u, 0u, 0u, 0u, 0u, 0xau);

    cmdbuf = pm4_w1(cmdbuf, PM4_HDR(PM4_IT_NOP, kernel_is_neo_mode() ? 0xe9 : 0xef, PM4_GFX, 0));
    cmdbuf = pm4_w1(cmdbuf, 0u);
    return HW_INIT_PACKET_SIZE;
}

s32 PS4_SYSV_ABI sceGnmDrawIndex(u32* cmdbuf, u32 size, u32 index_count, uintptr_t index_addr,
                                 u32 flags, u32 type) {
    (void)type;
    if (cmdbuf && (size == 10) && (index_addr != 0) && (index_addr & 1) == 0 &&
        (flags & 0x1ffffffe) == 0) { /* no predication will be set in the packet */
        /* A DRAW_INDEX_2: the most indices, the index buffer's address, their count, the draw
         * initiator */
        cmdbuf[0] = PM4_HDR_N(PM4_IT_DRAW_INDEX2, 4, PM4_GFX, 0);
        cmdbuf[1] = index_count;
        cmdbuf[2] = (u32)(index_addr);
        cmdbuf[3] = (u32)((u64)index_addr >> 32);
        cmdbuf[4] = index_count;
        cmdbuf[5] = kernel_is_neo_mode() ? flags & 0xe0000000u : 0;
        pm4_nop(cmdbuf + 6, 3);
        return ORBIS_OK;
    }
    return -1;
}

s32 PS4_SYSV_ABI sceGnmDrawIndexAuto(u32* cmdbuf, u32 size, u32 index_count, u32 flags) {
    if (cmdbuf && (size == 7) &&
        (flags & 0x1ffffffe) == 0) { /* no predication will be set in the packet */
        cmdbuf = pm4_w3(cmdbuf, PM4_HDR(PM4_IT_DRAW_INDEX_AUTO, 2, PM4_GFX, 0), index_count,
                        kernel_is_neo_mode() ? (flags & 0xe0000000u) | 2u : 2u);
        pm4_nop(cmdbuf, 3);
        return ORBIS_OK;
    }
    return -1;
}

s32 PS4_SYSV_ABI sceGnmDrawIndexIndirect(u32* cmdbuf, u32 size, u32 data_offset, u32 shader_stage,
                                         u32 vertex_sgpr_offset, u32 instance_sgpr_offset,
                                         u32 flags) {
    u32 sgpr_offset;

    if (cmdbuf && (size == 9) && (shader_stage < GNM_STAGE_MAX) && (vertex_sgpr_offset < 0x10u) &&
        (instance_sgpr_offset < 0x10u)) {
        const u32 predicate = flags & 1 ? 1u : 0u;
        cmdbuf = pm4_w1(cmdbuf, PM4_HDR(PM4_IT_DRAW_INDEX_INDIRECT, 4, PM4_GFX, predicate));

        sgpr_offset = indirect_sgpr_offsets[shader_stage];

        cmdbuf[0] = data_offset;
        cmdbuf[1] = vertex_sgpr_offset == 0 ? 0 : (vertex_sgpr_offset & 0xffffu) + sgpr_offset;
        cmdbuf[2] = instance_sgpr_offset == 0 ? 0 : (instance_sgpr_offset & 0xffffu) + sgpr_offset;
        cmdbuf[3] = kernel_is_neo_mode() ? flags & 0xe0000000u : 0u;

        cmdbuf += 4;
        pm4_nop(cmdbuf, 3);
        return ORBIS_OK;
    }
    return -1;
}

s32 PS4_SYSV_ABI sceGnmDrawIndexIndirectCountMulti(u32* cmdbuf, u32 size, u32 data_offset,
                                                   u32 max_count, u64 count_addr, u32 shader_stage,
                                                   u32 vertex_sgpr_offset, u32 instance_sgpr_offset,
                                                   u32 flags) {
    u32 predicate, sgpr_offset;

    if ((!kernel_is_neo_mode() || !USE_NEO_COMPAT_SEQUENCES) && cmdbuf && (size == 16) &&
        (vertex_sgpr_offset < 0x10u) && (instance_sgpr_offset < 0x10u) &&
        (shader_stage == GNM_STAGE_VS || shader_stage == GNM_STAGE_ES ||
         shader_stage == GNM_STAGE_LS)) {
        cmdbuf = pm4_nop(cmdbuf, 2);

        predicate = flags & 1 ? 1u : 0u;
        cmdbuf =
            pm4_w1(cmdbuf, PM4_HDR(PM4_IT_DRAW_INDEX_INDIRECT_COUNT_MULTI, 9, PM4_GFX, predicate));

        sgpr_offset = indirect_sgpr_offsets[shader_stage];

        cmdbuf[0] = data_offset;
        cmdbuf[1] = vertex_sgpr_offset == 0 ? 0 : (vertex_sgpr_offset & 0xffffu) + sgpr_offset;
        cmdbuf[2] = instance_sgpr_offset == 0 ? 0 : (instance_sgpr_offset & 0xffffu) + sgpr_offset;
        cmdbuf[3] = (count_addr != 0 ? 1u : 0u) << 0x1e;
        cmdbuf[4] = max_count;
        memcpy(&cmdbuf[5], &count_addr, sizeof(count_addr));
        cmdbuf[7] = DRAW_INDEXED_INDIRECT_ARGS_SIZE;
        cmdbuf[8] = kernel_is_neo_mode() ? flags & 0xe0000000u : 0;

        cmdbuf += 9;
        pm4_nop(cmdbuf, 2);
        return ORBIS_OK;
    }
    return -1;
}

int PS4_SYSV_ABI sceGnmDrawIndexIndirectMulti(u32* cmdbuf, u32 size, u32 data_offset, u32 max_count,
                                              u32 shader_stage, u32 vertex_sgpr_offset,
                                              u32 instance_sgpr_offset, u32 flags) {
    u32 sgpr_offset;

    if (cmdbuf && (size == 11) && (vertex_sgpr_offset < 0x10u) && (instance_sgpr_offset < 0x10u) &&
        (shader_stage == GNM_STAGE_VS || shader_stage == GNM_STAGE_ES ||
         shader_stage == GNM_STAGE_LS)) {
        const u32 predicate = flags & 1 ? 1u : 0u;
        cmdbuf = pm4_w1(cmdbuf, PM4_HDR(PM4_IT_DRAW_INDEX_INDIRECT_MULTI, 6, PM4_GFX, predicate));

        sgpr_offset = indirect_sgpr_offsets[shader_stage];

        cmdbuf[0] = data_offset;
        cmdbuf[1] = vertex_sgpr_offset == 0 ? 0 : (vertex_sgpr_offset & 0xffffu) + sgpr_offset;
        cmdbuf[2] = instance_sgpr_offset == 0 ? 0 : (instance_sgpr_offset & 0xffffu) + sgpr_offset;
        cmdbuf[3] = max_count;
        cmdbuf[4] = DRAW_INDEXED_INDIRECT_ARGS_SIZE;
        cmdbuf[5] = kernel_is_neo_mode() ? flags & 0xe0000000u : 0;

        cmdbuf += 6;
        pm4_nop(cmdbuf, 3);
        return ORBIS_OK;
    }
    return -1;
}

int PS4_SYSV_ABI sceGnmDrawIndexMultiInstanced(void) {
    c_log(C_LOG_LIB_GNM_DRIVER, C_LOG_LEVEL_ERROR, "sceGnmDrawIndexMultiInstanced",
          "(STUBBED) called");
    c_unreachable(__FILE__, __LINE__, "sceGnmDrawIndexMultiInstanced", "Unreachable code!");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceGnmDrawIndexOffset(u32* cmdbuf, u32 size, u32 index_offset, u32 index_count,
                                       u32 flags) {
    if (cmdbuf && (size == 9)) {
        const u32 predicate = flags & 1 ? 1u : 0u;
        cmdbuf = pm4_w1(cmdbuf, PM4_HDR(PM4_IT_DRAW_INDEX_OFFSET2, 4, PM4_GFX, predicate));
        cmdbuf = pm4_w4(cmdbuf, index_count, index_offset, index_count,
                        kernel_is_neo_mode() ? flags & 0xe0000000u : 0u);

        pm4_nop(cmdbuf, 3);
        return ORBIS_OK;
    }
    return -1;
}

s32 PS4_SYSV_ABI sceGnmDrawIndirect(u32* cmdbuf, u32 size, u32 data_offset, u32 shader_stage,
                                    u32 vertex_sgpr_offset, u32 instance_sgpr_offset, u32 flags) {
    u32 sgpr_offset;

    if (cmdbuf && (size == 9) && (shader_stage < GNM_STAGE_MAX) && (vertex_sgpr_offset < 0x10u) &&
        (instance_sgpr_offset < 0x10u)) {
        const u32 predicate = flags & 1 ? 1u : 0u;
        cmdbuf = pm4_w1(cmdbuf, PM4_HDR(PM4_IT_DRAW_INDIRECT, 4, PM4_GFX, predicate));

        sgpr_offset = indirect_sgpr_offsets[shader_stage];

        cmdbuf[0] = data_offset;
        cmdbuf[1] = vertex_sgpr_offset == 0 ? 0 : (vertex_sgpr_offset & 0xffffu) + sgpr_offset;
        cmdbuf[2] = instance_sgpr_offset == 0 ? 0 : (instance_sgpr_offset & 0xffffu) + sgpr_offset;
        cmdbuf[3] = kernel_is_neo_mode() ? (flags & 0xe0000000u) | 2u : 2u; /* auto index */

        cmdbuf += 4;
        pm4_nop(cmdbuf, 3);
        return ORBIS_OK;
    }
    return -1;
}

s32 PS4_SYSV_ABI sceGnmDrawIndirectCountMulti(u32* cmdbuf, u32 size, u32 data_offset, u32 max_count,
                                              u64 count_addr, u32 shader_stage,
                                              u32 vertex_sgpr_offset, u32 instance_sgpr_offset,
                                              u32 flags) {
    u32 predicate, sgpr_offset;

    if (cmdbuf && (size == 0x10) && (shader_stage < GNM_STAGE_MAX) &&
        (vertex_sgpr_offset < 0x10u) && (instance_sgpr_offset < 0x10u)) {
        cmdbuf = pm4_nop(cmdbuf, 2);

        predicate = flags & 1 ? 1u : 0u;
        cmdbuf = pm4_w1(cmdbuf, PM4_HDR(PM4_IT_DRAW_INDIRECT_COUNT_MULTI, 9, PM4_GFX, predicate));

        sgpr_offset = indirect_sgpr_offsets[shader_stage];

        cmdbuf[0] = data_offset;
        cmdbuf[1] = vertex_sgpr_offset == 0 ? 0 : (vertex_sgpr_offset & 0xffffu) + sgpr_offset;
        cmdbuf[2] = instance_sgpr_offset == 0 ? 0 : (instance_sgpr_offset & 0xffffu) + sgpr_offset;
        cmdbuf[3] = (count_addr != 0 ? 1u : 0u) << 0x1e;
        cmdbuf[4] = max_count;
        memcpy(&cmdbuf[5], &count_addr, sizeof(count_addr));
        cmdbuf[7] = DRAW_INDIRECT_ARGS_SIZE;
        cmdbuf[8] = kernel_is_neo_mode() ? (flags & 0xe0000000u) | 2u : 2u; /* auto index */

        cmdbuf += 9;
        pm4_nop(cmdbuf, 2);
        return ORBIS_OK;
    }
    return -1;
}

s32 PS4_SYSV_ABI sceGnmDrawIndirectMulti(u32* cmdbuf, u32 size, u32 data_offset, u32 max_count,
                                         u32 shader_stage, u32 vertex_sgpr_offset,
                                         u32 instance_sgpr_offset, u32 flags) {
    u32 sgpr_offset;

    if (cmdbuf && size == 11 && shader_stage < GNM_STAGE_MAX && vertex_sgpr_offset < 0x10 &&
        instance_sgpr_offset < 0x10) {
        const u32 predicate = flags & 1 ? 1u : 0u;
        cmdbuf = pm4_w1(cmdbuf, PM4_HDR(PM4_IT_DRAW_INDIRECT_MULTI, 6, PM4_GFX, predicate));

        sgpr_offset = indirect_sgpr_offsets[shader_stage];
        cmdbuf[0] = data_offset;
        cmdbuf[1] = vertex_sgpr_offset == 0 ? 0 : (vertex_sgpr_offset & 0xffffu) + sgpr_offset;
        cmdbuf[2] = instance_sgpr_offset == 0 ? 0 : (instance_sgpr_offset & 0xffffu) + sgpr_offset;
        cmdbuf[3] = max_count;
        cmdbuf[4] = DRAW_INDIRECT_ARGS_SIZE;
        cmdbuf[5] = kernel_is_neo_mode() ? (flags & 0xe0000000u) | 2u : 2u; /* auto index */

        cmdbuf += 6;
        pm4_nop(cmdbuf, 3);
        return ORBIS_OK;
    }
    return -1;
}

/* The context cleared, the init sequence past its first two words, and a NOP over the rest of
 * the packet */
static u32 hw_init_packet(u32* cmdbuf, const u32* sequence, size_t sequence_words) {
    const u32* cmdbuf_end = cmdbuf + HW_INIT_PACKET_SIZE;
    cmdbuf = clear_context_state(cmdbuf);
    memcpy(cmdbuf, &sequence[2], (sequence_words - 2) * 4);
    cmdbuf += sequence_words - 2;
    pm4_nop(cmdbuf, (u32)(cmdbuf_end - cmdbuf - 1));
    return HW_INIT_PACKET_SIZE;
}

#define HW_INIT_PACKET(cmdbuf, sequence)                                                           \
    hw_init_packet(cmdbuf, sequence, sizeof(sequence) / sizeof((sequence)[0]))

u32 PS4_SYSV_ABI sceGnmDrawInitDefaultHardwareState(u32* cmdbuf, u32 size) {
    if (size < HW_INIT_PACKET_SIZE) {
        return 0;
    }
    return HW_INIT_PACKET(cmdbuf, gnm_init_sequence);
}

u32 PS4_SYSV_ABI sceGnmDrawInitDefaultHardwareState175(u32* cmdbuf, u32 size) {
    if (size < HW_INIT_PACKET_SIZE) {
        return 0;
    }
    return HW_INIT_PACKET(cmdbuf, gnm_init_sequence175);
}

u32 PS4_SYSV_ABI sceGnmDrawInitDefaultHardwareState200(u32* cmdbuf, u32 size) {
    if (size < HW_INIT_PACKET_SIZE) {
        return 0;
    }
    if (kernel_is_neo_mode()) {
#if !USE_NEO_COMPAT_SEQUENCES
        return HW_INIT_PACKET(cmdbuf, gnm_init_sequence200_neo);
#else
        return HW_INIT_PACKET(cmdbuf, gnm_init_sequence200_neo_compat);
#endif
    }
    return HW_INIT_PACKET(cmdbuf, gnm_init_sequence200);
}

u32 PS4_SYSV_ABI sceGnmDrawInitDefaultHardwareState350(u32* cmdbuf, u32 size) {
    if (size < HW_INIT_PACKET_SIZE) {
        return 0;
    }
    if (kernel_is_neo_mode()) {
#if !USE_NEO_COMPAT_SEQUENCES
        return HW_INIT_PACKET(cmdbuf, gnm_init_sequence350_neo);
#else
        return HW_INIT_PACKET(cmdbuf, gnm_init_sequence350_neo_compat);
#endif
    }
    return HW_INIT_PACKET(cmdbuf, gnm_init_sequence350);
}

u32 PS4_SYSV_ABI sceGnmDrawInitToDefaultContextState(u32* cmdbuf, u32 size) {
    if (size != 0x20u) {
        return 0;
    }
    if (kernel_is_neo_mode()) {
        memcpy(cmdbuf, gnm_ctx_init_sequence_neo, sizeof(gnm_ctx_init_sequence_neo));
    } else {
        memcpy(cmdbuf, gnm_ctx_init_sequence, sizeof(gnm_ctx_init_sequence));
    }
    return 0x20u;
}

u32 PS4_SYSV_ABI sceGnmDrawInitToDefaultContextState400(u32* cmdbuf, u32 size) {
    if (size != 0x100u) {
        return 0;
    }
    if (kernel_is_neo_mode()) {
#if !USE_NEO_COMPAT_SEQUENCES
        memcpy(cmdbuf, gnm_ctx_init_sequence400_neo, sizeof(gnm_ctx_init_sequence400_neo));
#else
        memcpy(cmdbuf, gnm_ctx_init_sequence400_neo_compat,
               sizeof(gnm_ctx_init_sequence400_neo_compat));
#endif
    } else {
        memcpy(cmdbuf, gnm_ctx_init_sequence400, sizeof(gnm_ctx_init_sequence400));
    }
    return 0x100u;
}

int PS4_SYSV_ABI sceGnmDrawOpaqueAuto(void) {
    c_log(C_LOG_LIB_GNM_DRIVER, C_LOG_LEVEL_ERROR, "sceGnmDrawOpaqueAuto", "(STUBBED) called");
    return ORBIS_OK;
}

bool PS4_SYSV_ABI sceGnmDriverCaptureInProgress(void) {
    /* Not available in retail firmware */
    return false;
}

u32 PS4_SYSV_ABI sceGnmDriverInternalRetrieveGnmInterface(void) {
    /* Not available in retail firmware */
    return 0x80000000;
}

u32 PS4_SYSV_ABI sceGnmDriverInternalRetrieveGnmInterfaceForGpuDebugger(void) {
    /* Not available in retail firmware */
    return 0x80000000;
}

u32 PS4_SYSV_ABI sceGnmDriverInternalRetrieveGnmInterfaceForGpuException(void) {
    /* Not available in retail firmware */
    return 0x80000000;
}

u32 PS4_SYSV_ABI sceGnmDriverInternalRetrieveGnmInterfaceForHDRScopes(void) {
    /* Not available in retail firmware */
    return 0x80000000;
}

u32 PS4_SYSV_ABI sceGnmDriverInternalRetrieveGnmInterfaceForReplay(void) {
    /* Not available in retail firmware */
    return 0x80000000;
}

u32 PS4_SYSV_ABI sceGnmDriverInternalRetrieveGnmInterfaceForResourceRegistration(void) {
    /* Not available in retail firmware */
    return 0x80000000;
}

u32 PS4_SYSV_ABI sceGnmDriverInternalRetrieveGnmInterfaceForValidation(void) {
    /* Not available in retail firmware */
    return 0x80000000;
}

int PS4_SYSV_ABI sceGnmDriverInternalVirtualQuery(void) {
    /* Not available in retail firmware */
    return ORBIS_OK;
}

bool PS4_SYSV_ABI sceGnmDriverTraceInProgress(void) {
    /* Not available in retail firmware */
    return false;
}

int PS4_SYSV_ABI sceGnmDriverTriggerCapture(void) {
    /* Not available in retail firmware */
    return ORBIS_GNM_ERROR_CAPTURE_RAZOR_NOT_LOADED;
}

int PS4_SYSV_ABI sceGnmEndWorkload(u64 workload) {
    if (workload != 0) {
        return (0xf < ((workload >> 0x38) & 0xff)) * 2;
    }
    return 2;
}

s32 PS4_SYSV_ABI sceGnmFindResourcesPublic(void) {
    /* Not available in retail firmware */
    return ORBIS_GNM_ERROR_FAILURE;
}

void PS4_SYSV_ABI sceGnmFlushGarlic(void) {}

int PS4_SYSV_ABI sceGnmGetCoredumpAddress(void) {
    c_log(C_LOG_LIB_GNM_DRIVER, C_LOG_LEVEL_ERROR, "sceGnmGetCoredumpAddress", "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI sceGnmGetCoredumpMode(void) {
    c_log(C_LOG_LIB_GNM_DRIVER, C_LOG_LEVEL_ERROR, "sceGnmGetCoredumpMode", "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI sceGnmGetCoredumpProtectionFaultTimestamp(void) {
    c_log(C_LOG_LIB_GNM_DRIVER, C_LOG_LEVEL_ERROR, "sceGnmGetCoredumpProtectionFaultTimestamp",
          "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI sceGnmGetDbgGcHandle(void) {
    /* Not available in retail firmware */
    return -1;
}

int PS4_SYSV_ABI sceGnmGetDebugTimestamp(void) {
    c_log(C_LOG_LIB_GNM_DRIVER, C_LOG_LEVEL_ERROR, "sceGnmGetDebugTimestamp", "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI sceGnmGetEqEventType(const kernel_event_t* ev) {
    C_ASSERT_MSG(ev != NULL, "sceGnmGetEqEventType", "ev");
    return (int)ev->data;
}

int PS4_SYSV_ABI sceGnmGetEqTimeStamp(void) {
    c_log(C_LOG_LIB_GNM_DRIVER, C_LOG_LEVEL_ERROR, "sceGnmGetEqTimeStamp", "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI sceGnmGetGpuBlockStatus(void) {
    /* Not available in retail firmware */
    return ORBIS_OK;
}

u32 PS4_SYSV_ABI sceGnmGetGpuCoreClockFrequency(void) {
    /* On console this uses an ioctl check, but we assume it is equal to just checking for neo mode.
     */
    return kernel_is_neo_mode() ? 911000000 : 800000000;
}

int PS4_SYSV_ABI sceGnmGetGpuInfoStatus(void) {
    /* Not available in retail firmware */
    return ORBIS_OK;
}

int PS4_SYSV_ABI sceGnmGetLastWaitedAddress(void) {
    /* Not available in retail firmware */
    return ORBIS_OK;
}

int PS4_SYSV_ABI sceGnmGetNumTcaUnits(void) {
    c_log(C_LOG_LIB_GNM_DRIVER, C_LOG_LEVEL_ERROR, "sceGnmGetNumTcaUnits", "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI sceGnmGetOffChipTessellationBufferSize(void) {
    return TESSELLATION_OFFCHIP_BUFFER_SIZE;
}

int PS4_SYSV_ABI sceGnmGetOwnerName(void) {
    /* Not available in retail firmware */
    return ORBIS_GNM_ERROR_FAILURE;
}

int PS4_SYSV_ABI sceGnmGetPhysicalCounterFromVirtualized(void) {
    /* Not available in retail firmware */
    return ORBIS_GNM_ERROR_FAILURE;
}

u32 PS4_SYSV_ABI sceGnmGetProtectionFaultTimeStamp(void) {
    /* Not available in retail firmware */
    return 0;
}

int PS4_SYSV_ABI sceGnmGetResourceBaseAddressAndSizeInBytes(void) {
    /* Not available in retail firmware */
    return ORBIS_GNM_ERROR_FAILURE;
}

int PS4_SYSV_ABI sceGnmGetResourceName(void) {
    /* Not available in retail firmware */
    return ORBIS_GNM_ERROR_FAILURE;
}

int PS4_SYSV_ABI sceGnmGetResourceShaderGuid(void) {
    /* Not available in retail firmware */
    return ORBIS_GNM_ERROR_FAILURE;
}

int PS4_SYSV_ABI sceGnmGetResourceType(void) {
    /* Not available in retail firmware */
    return ORBIS_GNM_ERROR_FAILURE;
}

int PS4_SYSV_ABI sceGnmGetResourceUserData(void) {
    /* Not available in retail firmware */
    return ORBIS_GNM_ERROR_FAILURE;
}

int PS4_SYSV_ABI sceGnmGetShaderProgramBaseAddress(void) {
    /* Not available in retail firmware */
    return ORBIS_OK;
}

int PS4_SYSV_ABI sceGnmGetShaderStatus(void) {
    /* Not available in retail firmware */
    return ORBIS_OK;
}

uintptr_t PS4_SYSV_ABI sceGnmGetTheTessellationFactorRingBufferBaseAddress(void) {
    if (tessellation_factors_ring_addr == (uintptr_t)-1) {
        tessellation_factors_ring_addr = gnm_gpu_tessellation_ring_base();
    }
    return tessellation_factors_ring_addr;
}

void PS4_SYSV_ABI sceGnmGpuPaDebugEnter(void) {
    /* Not available in retail firmware */
}

void PS4_SYSV_ABI sceGnmGpuPaDebugLeave(void) {
    /* Not available in retail firmware */
}

s32 PS4_SYSV_ABI sceGnmInsertDingDongMarker(u32* cmdbuf, u32 size) {
    if (cmdbuf == NULL || size != 4) {
        return -1;
    }
    pm4_w4(cmdbuf, PM4_HDR(PM4_IT_NOP, 3, PM4_GFX, 0), 0u, 0u, 0u);
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceGnmInsertPopMarker(u32* cmdbuf, u32 size) {
    if (cmdbuf && (size == 6)) {
        cmdbuf = pm4_w6(cmdbuf, PM4_HDR(PM4_IT_NOP, 5, PM4_GFX, 0), PM4_NOP_DEBUG_MARKER_POP, 0u,
                        0u, 0u, 0u);
        return ORBIS_OK;
    }
    return -1;
}

/* A NOP that holds a marker: its kind, the string and its end, zeroes to the packet's end, and
 * @color (if @has_color) eight bytes after the string's end */
static void marker_packet(u32* cmdbuf, u32 packet_size, u32 kind, const char* marker, size_t len,
                          bool has_color, u32 color) {
    u8* text = (u8*)&cmdbuf[2];
    const size_t marker_len = len + 1;
    cmdbuf[0] = PM4_HDR_N(PM4_IT_NOP, packet_size, PM4_GFX, 0);
    cmdbuf[1] = kind;
    memcpy(text, marker, marker_len);
    if (has_color) {
        memcpy(text + marker_len + 8, &color, sizeof(color));
        memset(text + marker_len + 8 + sizeof(u32), 0,
               packet_size * 4 - marker_len - 8 - sizeof(u32));
    } else {
        memset(text + marker_len, 0, packet_size * 4 - marker_len);
    }
}

s32 PS4_SYSV_ABI sceGnmInsertPushColorMarker(u32* cmdbuf, u32 size, const char* marker, u32 color) {
    if (cmdbuf && marker) {
        const size_t len = strlen(marker);
        const u32 packet_size = (u32)(((len + 0xc) >> 2) + ((len + 0x10) >> 3) * 2);
        if (packet_size + 2 == size) {
            marker_packet(cmdbuf, packet_size, PM4_NOP_DEBUG_COLOR_MARKER_PUSH, marker, len, true,
                          color);
            return ORBIS_OK;
        }
    }
    return -1;
}

s32 PS4_SYSV_ABI sceGnmInsertPushMarker(u32* cmdbuf, u32 size, const char* marker) {
    if (cmdbuf && marker) {
        const size_t len = strlen(marker);
        const u32 packet_size = (u32)(((len + 8) >> 2) + ((len + 0xc) >> 3) * 2);
        if (packet_size + 2 == size) {
            marker_packet(cmdbuf, packet_size, PM4_NOP_DEBUG_MARKER_PUSH, marker, len, false, 0);
            return ORBIS_OK;
        }
    }
    return -1;
}

int PS4_SYSV_ABI sceGnmInsertSetColorMarker(void) {
    c_log(C_LOG_LIB_GNM_DRIVER, C_LOG_LEVEL_ERROR, "sceGnmInsertSetColorMarker",
          "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceGnmInsertSetMarker(u32* cmdbuf, u32 size, const char* marker) {
    if (cmdbuf && marker) {
        const size_t len = strlen(marker);
        const u32 packet_size = (u32)(((len + 8) >> 2) + ((len + 0xc) >> 3) * 2);
        if (packet_size + 2 == size) {
            marker_packet(cmdbuf, packet_size, PM4_NOP_DEBUG_SET_MARKER, marker, len, false, 0);
            return ORBIS_OK;
        }
    }
    return -1;
}

int PS4_SYSV_ABI sceGnmInsertThreadTraceMarker(void) {
    /* Not available in retail firmware */
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceGnmInsertWaitFlipDone(u32* cmdbuf, u32 size, s32 vo_handle, u32 buf_idx) {
    uintptr_t label_addr = 0;
    u64 poll_addr;
    if (size != 7) {
        return -1;
    }
    C_ASSERT_MSG(sceVideoOutGetBufferLabelAddress(vo_handle, &label_addr) == 16,
                 "sceGnmInsertWaitFlipDone", "sceVideoOutGetBufferLabelAddress call failed");
    /* A WAIT_REG_MEM on the buffer's label, until it is zero */
    poll_addr = ((u64)label_addr + buf_idx * sizeof(uintptr_t)) & ~0x3ull;
    cmdbuf[0] = PM4_HDR_N(PM4_IT_WAIT_REG_MEM, 5, PM4_GFX, 0);
    cmdbuf[1] = 0x13u;
    memcpy(&cmdbuf[2], &poll_addr, sizeof(poll_addr));
    cmdbuf[4] = 0u;
    cmdbuf[5] = 0xffffffffu;
    cmdbuf[6] = 10u;
    return ORBIS_OK;
}

int PS4_SYSV_ABI sceGnmIsCoredumpValid(void) {
    c_log(C_LOG_LIB_GNM_DRIVER, C_LOG_LEVEL_ERROR, "sceGnmIsCoredumpValid", "(STUBBED) called");
    return ORBIS_OK;
}

bool PS4_SYSV_ABI sceGnmIsUserPaEnabled(void) {
    /* Not available in retail firmware */
    return false;
}

int PS4_SYSV_ABI sceGnmLogicalCuIndexToPhysicalCuIndex(void) {
    /* Not available in retail firmware */
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceGnmLogicalCuMaskToPhysicalCuMask(s64 unused, s32 logical_cu_mask) {
    c_log(C_LOG_LIB_GNM_DRIVER, C_LOG_LEVEL_INFO, "sceGnmLogicalCuMaskToPhysicalCuMask",
          "called, logical_cu_mask: %d", logical_cu_mask);
    return logical_cu_mask;
}

int PS4_SYSV_ABI sceGnmLogicalTcaUnitToPhysical(void) {
    c_log(C_LOG_LIB_GNM_DRIVER, C_LOG_LEVEL_ERROR, "sceGnmLogicalTcaUnitToPhysical",
          "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI sceGnmMapComputeQueue(u32 pipe_id, u32 queue_id, uintptr_t ring_base_addr,
                                       u32 ring_size_dw, u32* read_ptr_addr) {
    u32 index, gnm_vqid;
    if (pipe_id >= GNM_GPU_NUM_COMPUTE_PIPES) {
        return ORBIS_GNM_ERROR_COMPUTEQUEUE_INVALID_PIPE_ID;
    }
    if (queue_id >= GNM_GPU_NUM_QUEUES_PER_PIPE) {
        return ORBIS_GNM_ERROR_COMPUTEQUEUE_INVALID_QUEUE_ID;
    }
    if (ring_base_addr % 256 != 0) { /* alignment check */
        return ORBIS_GNM_ERROR_COMPUTEQUEUE_INVALID_RING_BASE_ADDR;
    }
    if (ring_size_dw == 0 || (ring_size_dw & (ring_size_dw - 1)) != 0) { /* a power of two */
        return ORBIS_GNM_ERROR_COMPUTEQUEUE_INVALID_RING_SIZE;
    }
    if ((uintptr_t)read_ptr_addr % 4 != 0) { /* alignment check */
        return ORBIS_GNM_ERROR_COMPUTEQUEUE_INVALID_READ_PTR_ADDR;
    }
    index = gnm_gpu_asc_map(ring_base_addr, read_ptr_addr, ring_size_dw, pipe_id);
    /* We need to offset index as `dingDong` assumes it to be from the range [1..64] */
    gnm_vqid = index + 1;
    c_log(C_LOG_LIB_GNM_DRIVER, C_LOG_LEVEL_INFO, "sceGnmMapComputeQueue",
          "ASC pipe %u queue %u mapped to vqueue %u", pipe_id, queue_id, gnm_vqid);
    *read_ptr_addr = 0u;
    asc_next_offs_dw[index] = 0;
    return (int)gnm_vqid;
}

int PS4_SYSV_ABI sceGnmMapComputeQueueWithPriority(u32 pipe_id, u32 queue_id, VAddr ring_base_addr,
                                                   u32 ring_size_dw, u32* read_ptr_addr,
                                                   u32 pipePriority) {
    (void)pipePriority;
    return sceGnmMapComputeQueue(pipe_id, queue_id, ring_base_addr, ring_size_dw, read_ptr_addr);
}

int PS4_SYSV_ABI sceGnmPaDisableFlipCallbacks(void) {
    /* Not available in retail firmware */
    return ORBIS_OK;
}

int PS4_SYSV_ABI sceGnmPaEnableFlipCallbacks(void) {
    /* Not available in retail firmware */
    return ORBIS_OK;
}

int PS4_SYSV_ABI sceGnmPaHeartbeat(void) {
    /* Not available in retail firmware */
    return ORBIS_OK;
}

int PS4_SYSV_ABI sceGnmQueryResourceRegistrationUserMemoryRequirements(void) {
    /* Not available in retail firmware */
    return ORBIS_GNM_ERROR_FAILURE;
}

int PS4_SYSV_ABI sceGnmRaiseUserExceptionEvent(void) {
    /* Not available in retail firmware */
    return ORBIS_OK;
}

int PS4_SYSV_ABI sceGnmRegisterGdsResource(void) {
    /* Not available in retail firmware */
    return ORBIS_GNM_ERROR_FAILURE;
}

void PS4_SYSV_ABI sceGnmRegisterGnmLiveCallbackConfig(void) {
    /* Not available in retail firmware */
}

s32 PS4_SYSV_ABI sceGnmRegisterOwner(void* handle, const char* name) {
    /* Not available in retail firmware */
    return ORBIS_GNM_ERROR_FAILURE;
}

s32 PS4_SYSV_ABI sceGnmRegisterResource(void* res_handle, void* owner_handle, const void* addr,
                                        size_t size, const char* name, int res_type,
                                        u64 user_data) {
    /* Not available in retail firmware */
    return ORBIS_GNM_ERROR_FAILURE;
}

static s32 patch_flip_request(u32* cmdbuf, u32 size, u32 vo_handle, u32 buf_idx, u32 flip_mode,
                              s64 flip_arg) {
    u32 backup[7];
    s32 flip_result;
    uintptr_t label_addr = 0;
    u64 addr;
    u32* nop;
    /* check for `prepareFlip` packet */
    cmdbuf += size - 64;
    C_ASSERT_MSG(cmdbuf[0] == 0xc03e1000, "PatchFlipRequest", "Can't find `prepareFlip` packet");
    memcpy(backup, cmdbuf, sizeof(backup));
    C_ASSERT_MSG(((backup[2] & 3) == 0u) || (backup[1] != PM4_NOP_PREPARE_FLIP_LABEL),
                 "PatchFlipRequest", "Invalid flip packet");
    C_ASSERT_MSG(buf_idx != 0xffffffffu, "PatchFlipRequest", "Invalid VO buffer index");
    flip_result = sceVideoOutSubmitEopFlip((s32)vo_handle, buf_idx, flip_mode, flip_arg, NULL);
    if (flip_result != 0) {
        if ((u32)flip_result == 0x80290012u) {
            c_log(C_LOG_LIB_GNM_DRIVER, C_LOG_LEVEL_ERROR, "PatchFlipRequest",
                  "Flip queue is full");
            return (s32)0x80d11081;
        }
        c_log(C_LOG_LIB_GNM_DRIVER, C_LOG_LEVEL_ERROR, "PatchFlipRequest", "Flip request failed");
        return flip_result;
    }
    C_ASSERT_MSG(sceVideoOutGetBufferLabelAddress((s32)vo_handle, &label_addr) == 16,
                 "PatchFlipRequest", "sceVideoOutGetBufferLabelAddress call failed");
    /* Write event to lock the VO surface: a WRITE_DATA of 1 to the buffer's label */
    addr = ((u64)label_addr + buf_idx * sizeof(label_addr)) & ~0x3ull;
    cmdbuf[0] = PM4_HDR_N(PM4_IT_WRITE_DATA, 3, PM4_GFX, 0);
    cmdbuf[1] = 0x500u;
    memcpy(&cmdbuf[2], &addr, sizeof(addr));
    cmdbuf[4] = 1;
    nop = cmdbuf + 5;
    if (backup[1] == PM4_NOP_PREPARE_FLIP) {
        nop[0] = PM4_HDR_N(PM4_IT_NOP, 0x39, PM4_GFX, 0);
        nop[1] = PM4_NOP_PATCHED_FLIP;
    } else {
        if (backup[1] == PM4_NOP_PREPARE_FLIP_LABEL) {
            u32* write_label = cmdbuf + 0x3b;
            nop[0] = PM4_HDR_N(PM4_IT_NOP, 0x34, PM4_GFX, 0);
            nop[1] = PM4_NOP_PATCHED_FLIP;
            /* Write event to update label */
            write_label[0] = PM4_HDR_N(PM4_IT_WRITE_DATA, 3, PM4_GFX, 0);
            write_label[1] = 0x500u;
            write_label[2] = backup[2] & 0xfffffffcu;
            write_label[3] = backup[3];
            write_label[4] = backup[4];
        }
        if (backup[1] == PM4_NOP_PREPARE_FLIP_INTERRUPT_LABEL) {
            u32* write_eop = cmdbuf + 0x3a;
            nop[0] = PM4_HDR_N(PM4_IT_NOP, 0x33, PM4_GFX, 0);
            nop[1] = PM4_NOP_PATCHED_FLIP;
            write_eop[0] = PM4_HDR_N(PM4_IT_EVENT_WRITE_EOP, 4, PM4_GFX, 0);
            write_eop[1] = (backup[5] & 0x3f) + 0x500u + (backup[6] & 0x3f) * 0x1000;
            write_eop[2] = backup[2] & 0xfffffffcu;
            write_eop[3] = (backup[3] & 0xffffu) | 0x22000000u;
            write_eop[4] = backup[4];
            write_eop[5] = 0u;
        }
        if (backup[1] == PM4_NOP_PREPARE_FLIP_INTERRUPT) {
            u32* write_eop = cmdbuf + 0x3a;
            nop[0] = PM4_HDR_N(PM4_IT_NOP, 0x33, PM4_GFX, 0);
            nop[1] = PM4_NOP_PATCHED_FLIP;
            write_eop[0] = PM4_HDR_N(PM4_IT_EVENT_WRITE_EOP, 4, PM4_GFX, 0);
            write_eop[1] = (backup[5] & 0x3f) + 0x500u + (backup[6] & 0x3f) * 0x1000;
            write_eop[2] = 0u;
            write_eop[3] = 0x1000000u;
            write_eop[4] = 0u;
            write_eop[5] = 0u;
        }
    }
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceGnmRequestFlipAndSubmitDone(u32* gpu_addr, u32 gpu_addr_size_in_bytes,
                                                u32 video_out_handle, u32 display_buffer_index,
                                                u32 flip_mode, s64 flip_arg) {
    return sceGnmRequestFlipAndSubmitDoneForWorkload((u64)(gpu_addr), gpu_addr,
                                                     gpu_addr_size_in_bytes, video_out_handle,
                                                     display_buffer_index, flip_mode, flip_arg);
}

typedef struct flip_done_args {
    u32* gpu_addr;
    u32 vo_handle;
    u32 buf_idx;
    u32 flip_mode;
    s64 flip_arg;
    s32 result;
} flip_done_args;

static void flip_done_run(void* ctx) {
    flip_done_args* a = (flip_done_args*)ctx;
    wait_gpu_idle();
    /* Write a PrepareFlip, then patch the flip request */
    a->gpu_addr[0] = PM4_HDR_N(PM4_IT_NOP, 0x3e, PM4_GFX, 0);
    a->gpu_addr[1] = PM4_NOP_PREPARE_FLIP;
    a->result =
        patch_flip_request(a->gpu_addr, 0x40, a->vo_handle, a->buf_idx, a->flip_mode, a->flip_arg);
    if (a->result != ORBIS_OK) {
        return;
    }
    /* Submit */
    gnm_gpu_submit_gfx(a->gpu_addr, 0x40, NULL, 0);
    take_submission_lock();
    gnm_gpu_submit_done();
    send_init_packet = true;
}

s32 PS4_SYSV_ABI sceGnmRequestFlipAndSubmitDoneForWorkload(u64 workload, u32* gpu_addr,
                                                           u32 gpu_addr_size_in_bytes,
                                                           u32 vo_handle, u32 buf_idx,
                                                           u32 flip_mode, s64 flip_arg) {
    flip_done_args a;
    (void)workload;
    c_log(C_LOG_LIB_GNM_DRIVER, C_LOG_LEVEL_DEBUG, "sceGnmRequestFlipAndSubmitDoneForWorkload",
          "called [buf = %u]", buf_idx);
    if (gpu_addr_size_in_bytes < 0x100) {
        return ORBIS_GNM_ERROR_SUBMISSION_NOT_ENOUGH_RESOURCES;
    }
    a.gpu_addr = gpu_addr;
    a.vo_handle = vo_handle;
    a.buf_idx = buf_idx;
    a.flip_mode = flip_mode;
    a.flip_arg = flip_arg;
    a.result = ORBIS_OK;
    combiner_run(&submit_combiner, flip_done_run, &a);
    return a.result;
}

int PS4_SYSV_ABI sceGnmRequestMipStatsReportAndReset(void) {
    c_log(C_LOG_LIB_GNM_DRIVER, C_LOG_LEVEL_ERROR, "sceGnmRequestMipStatsReportAndReset",
          "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceGnmResetVgtControl(u32* cmdbuf, u32 size) {
    if (cmdbuf == NULL || size != 3) {
        return -1;
    }
    if (kernel_is_neo_mode()) {
        if (!USE_NEO_COMPAT_SEQUENCES) {
            pm4_w3(cmdbuf, PM4_HDR(PM4_IT_SET_UCONFIG_REG, 2, PM4_GFX, 0), 0x40000258u,
                   0x6d007fu); /* IA_MULTI_VGT_PARAM */
        } else {
            pm4_w3(cmdbuf, PM4_HDR(PM4_IT_SET_CONTEXT_REG, 2, PM4_GFX, 0), 0x100002aau,
                   0xd00ffu); /* IA_MULTI_VGT_PARAM */
        }
    } else {
        pm4_w3(cmdbuf, PM4_HDR(PM4_IT_SET_CONTEXT_REG, 2, PM4_GFX, 0), 0x2aau,
               0xffu); /* IA_MULTI_VGT_PARAM */
    }
    return ORBIS_OK;
}

int PS4_SYSV_ABI sceGnmSdmaClose(void) {
    /* Not available in retail firmware */
    return ORBIS_GNM_ERROR_FAILURE;
}

int PS4_SYSV_ABI sceGnmSdmaConstFill(void) {
    /* Not available in retail firmware */
    return ORBIS_GNM_ERROR_FAILURE;
}

int PS4_SYSV_ABI sceGnmSdmaCopyLinear(void) {
    /* Not available in retail firmware */
    return ORBIS_GNM_ERROR_FAILURE;
}

int PS4_SYSV_ABI sceGnmSdmaCopyTiled(void) {
    /* Not available in retail firmware */
    return ORBIS_GNM_ERROR_FAILURE;
}

int PS4_SYSV_ABI sceGnmSdmaCopyWindow(void) {
    /* Not available in retail firmware */
    return ORBIS_GNM_ERROR_FAILURE;
}

int PS4_SYSV_ABI sceGnmSdmaFlush(void) {
    /* Not available in retail firmware */
    return ORBIS_GNM_ERROR_FAILURE;
}

int PS4_SYSV_ABI sceGnmSdmaGetMinCmdSize(void) {
    /* Not available in retail firmware */
    return ORBIS_GNM_ERROR_FAILURE;
}

int PS4_SYSV_ABI sceGnmSdmaOpen(void) {
    /* Not available in retail firmware */
    return ORBIS_GNM_ERROR_FAILURE;
}

s32 PS4_SYSV_ABI sceGnmSetCsShader(u32* cmdbuf, u32 size, const u32* cs_regs) {
    if (!cmdbuf || size <= 0x18) {
        return -1;
    }
    if (!cs_regs) {
        c_log(C_LOG_LIB_GNM_DRIVER, C_LOG_LEVEL_ERROR, "sceGnmSetCsShader",
              "Null pointer in shader registers.");
        return -1;
    }
    if (cs_regs[1] != 0) {
        c_log(C_LOG_LIB_GNM_DRIVER, C_LOG_LEVEL_ERROR, "sceGnmSetCsShader",
              "Invalid shader address.");
        return -1;
    }

    cmdbuf = pm4_w4(cmdbuf, PM4_HDR(PM4_IT_SET_SH_REG, 3, PM4_GFX, 0), 0x20cu, cs_regs[0],
                    0u); /* COMPUTE_PGM_LO/COMPUTE_PGM_HI */
    cmdbuf = pm4_w4(cmdbuf, PM4_HDR(PM4_IT_SET_SH_REG, 3, PM4_GFX, 0), 0x212u, cs_regs[2],
                    cs_regs[3]); /* COMPUTE_PGM_RSRC1/COMPUTE_PGM_RSRC2 */
    cmdbuf =
        pm4_w5(cmdbuf, PM4_HDR(PM4_IT_SET_SH_REG, 4, PM4_GFX, 0), 0x207u, cs_regs[4], cs_regs[5],
               cs_regs[6]); /* COMPUTE_NUM_THREAD_X/COMPUTE_NUM_THREAD_Y/COMPUTE_NUM_THREAD_Z */

    pm4_nop(cmdbuf, 11);
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceGnmSetCsShaderWithModifier(u32* cmdbuf, u32 size, const u32* cs_regs,
                                               u32 modifier) {
    u32 rsrc1;

    if (!cmdbuf || size <= 0x18) {
        return -1;
    }
    if (!cs_regs) {
        c_log(C_LOG_LIB_GNM_DRIVER, C_LOG_LEVEL_ERROR, "sceGnmSetCsShaderWithModifier",
              "Null pointer in shader registers.");
        return -1;
    }
    if ((modifier & 0xfffffc3fu) != 0) {
        c_log(C_LOG_LIB_GNM_DRIVER, C_LOG_LEVEL_ERROR, "sceGnmSetCsShaderWithModifier",
              "Invalid modifier mask.");
        return -1;
    }
    if (cs_regs[1] != 0) {
        c_log(C_LOG_LIB_GNM_DRIVER, C_LOG_LEVEL_ERROR, "sceGnmSetCsShaderWithModifier",
              "Invalid shader address.");
        return -1;
    }

    cmdbuf = pm4_w4(cmdbuf, PM4_HDR(PM4_IT_SET_SH_REG, 3, PM4_GFX, 0), 0x20cu, cs_regs[0],
                    0u); /* COMPUTE_PGM_LO/COMPUTE_PGM_HI */
    rsrc1 = modifier == 0 ? cs_regs[2] : (cs_regs[2] & 0xfffffc3fu) | modifier;
    cmdbuf = pm4_w4(cmdbuf, PM4_HDR(PM4_IT_SET_SH_REG, 3, PM4_GFX, 0), 0x212u, rsrc1,
                    cs_regs[3]); /* COMPUTE_PGM_RSRC1/COMPUTE_PGM_RSRC2 */
    cmdbuf =
        pm4_w5(cmdbuf, PM4_HDR(PM4_IT_SET_SH_REG, 4, PM4_GFX, 0), 0x207u, cs_regs[4], cs_regs[5],
               cs_regs[6]); /* COMPUTE_NUM_THREAD_X/COMPUTE_NUM_THREAD_Y/COMPUTE_NUM_THREAD_Z */

    pm4_nop(cmdbuf, 11);
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceGnmSetEmbeddedPsShader(u32* cmdbuf, u32 size, u32 shader_id,
                                           u32 shader_modifier) {
    /* clang-format off */
    static GNM_ALIGN_256(const u32 ps0_code[]) = {
        0xbeeb03ffu, 0x00000003u, /* s_mov_b32     vcc_hi, $0x00000003 */
        0x7e000280u,              /* v_mov_b32     v0, 0 */
        0x5e000100u,              /* v_cvt_pkrtz_f16_f32 v0, v0, v0 */
        0xbf800000u,              /* s_nop */
        0xf8001c0fu, 0x00000000u, /* exp           mrt0, v0, v0 compr vm done */
        0xbf810000u,              /* s_endpgm */
        /* Binary header */
        0x5362724fu, 0x07726468u, 0x00002043u, 0u, 0xb0a45b2bu, 0x1d39766du, 0x72044b7bu, 0x0000000fu,
        /* PS regs */
        0x0fe000f0u, 0u, 0xc0000u, 4u, 0u, 4u, 2u, 2u, 0u, 0u, 0x10u, 0xfu, 0xcu, 0u, 0u, 0u,
    };
    static GNM_ALIGN_256(const u32 ps1_code[]) = {
        0xbeeb03ffu, 0x00000003u, /* s_mov_b32     vcc_hi, $0x00000003 */
        0x7e040280u,              /* v_mov_b32     v2, 0 */
        0xf8001803u, 0x02020202u, /* exp           mrt0, v2, v2, off, off vm done */
        0xbf810000u,              /* s_endpgm */
        /* Binary header */
        0x5362724fu, 0x07726468u, 0x00001841u, 0x04080002u, 0x98b9cb94u, 0u, 0x6f130734u, 0x0000000fu,
        /* PS regs */
        0x0fe000f2u, 0u, 0x2000u, 0u, 0u, 2u, 2u, 2u, 0u, 0u, 0x10u, 3u, 0xcu,
    };
    /* The registers of each after its address (originally 0xfe000f00 and 0xfe000f20) */
    static const u32 ps0_regs[] = {0xc0000u, 4u, 0u, 4u, 2u, 2u, 0u, 0u, 0x10u, 0xfu, 0xcu};
    static const u32 ps1_regs[] = {0x2000u, 0u, 0u, 2u, 2u, 2u, 0u, 0u, 0x10u, 3u, 0xcu};
    /* clang-format on */
    const u64 shader_addr = (u64)(uintptr_t)(shader_id == 0 ? ps0_code : ps1_code);
    const u32* ps_regs = shader_id == 0 ? ps0_regs : ps1_regs;
    (void)size;
    (void)shader_modifier;
    if (shader_id > 1) {
        c_log(C_LOG_LIB_GNM_DRIVER, C_LOG_LEVEL_ERROR, "sceGnmSetEmbeddedPsShader",
              "Unknown shader id %u", shader_id);
        return ORBIS_GNM_ERROR_FAILURE;
    }
    /* Normally the driver will do a call to `sceGnmSetPsShader350()`, but this function has
     * a check for zero in the upper part of shader address. In our case, the address is a
     * pointer to the emulator's own memory, so the check will likely fail. To workaround it we
     * will repeat set shader functionality here as it is trivial. */
    /* SPI_SHADER_PGM_LO_PS/SPI_SHADER_PGM_HI_PS */
    cmdbuf = pm4_w4(cmdbuf, PM4_HDR(PM4_IT_SET_SH_REG, 3, PM4_GFX, 0), 8u, (u32)(shader_addr >> 8),
                    (u32)(shader_addr >> 40));
    /* SPI_SHADER_PGM_RSRC1_PS/SPI_SHADER_PGM_RSRC2_PS */
    cmdbuf = pm4_w4(cmdbuf, PM4_HDR(PM4_IT_SET_SH_REG, 3, PM4_GFX, 0), 10u, ps_regs[0], ps_regs[1]);
    /* SPI_SHADER_Z_FORMAT/SPI_SHADER_COL_FORMAT */
    cmdbuf = pm4_w4(cmdbuf, PM4_HDR(PM4_IT_SET_CONTEXT_REG, 3, PM4_GFX, 0), 0x1c4u, ps_regs[2],
                    ps_regs[3]);
    /* SPI_PS_INPUT_ENA/SPI_PS_INPUT_ADDR */
    cmdbuf = pm4_w4(cmdbuf, PM4_HDR(PM4_IT_SET_CONTEXT_REG, 3, PM4_GFX, 0), 0x1b3u, ps_regs[4],
                    ps_regs[5]);
    /* SPI_PS_IN_CONTROL */
    cmdbuf = pm4_w3(cmdbuf, PM4_HDR(PM4_IT_SET_CONTEXT_REG, 2, PM4_GFX, 0), 0x1b6u, ps_regs[6]);
    /* SPI_BARYC_CNTL */
    cmdbuf = pm4_w3(cmdbuf, PM4_HDR(PM4_IT_SET_CONTEXT_REG, 2, PM4_GFX, 0), 0x1b8u, ps_regs[7]);
    /* DB_SHADER_CONTROL */
    cmdbuf = pm4_w3(cmdbuf, PM4_HDR(PM4_IT_SET_CONTEXT_REG, 2, PM4_GFX, 0), 0x203u, ps_regs[8]);
    /* CB_SHADER_MASK */
    cmdbuf = pm4_w3(cmdbuf, PM4_HDR(PM4_IT_SET_CONTEXT_REG, 2, PM4_GFX, 0), 0x8fu, ps_regs[9]);
    pm4_nop(cmdbuf, 11);
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceGnmSetEmbeddedVsShader(u32* cmdbuf, u32 size, u32 shader_id,
                                           u32 shader_modifier) {
    /* A fullscreen triangle with one uv set */
    /* clang-format off */
    static GNM_ALIGN_256(const u32 shader_code[]) = {
        0xbeeb03ffu, 0x00000007u, /* s_mov_b32     vcc_hi, $0x00000007 */
        0x36020081u,              /* v_and_b32     v1, 1, v0 */
        0x34020281u,              /* v_lshlrev_b32 v1, 1, v1 */
        0x360000c2u,              /* v_and_b32     v0, -2, v0 */
        0x4a0202c1u,              /* v_add_i32     v1, vcc, -1, v1 */
        0x4a0000c1u,              /* v_add_i32     v0, vcc, -1, v0 */
        0x7e020b01u,              /* v_cvt_f32_i32 v1, v1 */
        0x7e000b00U,              /* v_cvt_f32_i32 v0, v0 */
        0x7e040280u,              /* v_mov_b32     v2, 0 */
        0x7e0602f2u,              /* v_mov_b32     v3, 1.0 */
        0xf80008cfu, 0x03020001u, /* exp           pos0, v1, v0, v2, v3 done */
        0xf800020fu, 0x03030303u, /* exp           param0, v3, v3, v3, v3 */
        0xbf810000u,              /* s_endpgm */
        /* Binary header */
        0x5362724fu, 0x07726468u, 0x00004047u, 0u, 0x47f8c29fu, 0x9b2da5cfu, 0xff7c5b7du, 0x00000017u,
        /* VS regs */
        0x0fe000f1u, 0u, 0x000c0000u, 4u, 0u, 4u, 0u, 7u,
    };
    /* clang-format on */
    /* Its registers after its address (originally 0xfe000f10) */
    static const u32 vs_regs[] = {0xc0000u, 4, 0, 4, 0, 7};
    const u64 shader_addr = (u64)(uintptr_t)shader_code;
    (void)size;
    (void)shader_modifier;
    if (shader_id != 0) {
        c_log(C_LOG_LIB_GNM_DRIVER, C_LOG_LEVEL_ERROR, "sceGnmSetEmbeddedVsShader",
              "Unknown shader id %u", shader_id);
        return ORBIS_GNM_ERROR_FAILURE;
    }
    /* Normally the driver will do a call to `sceGnmSetVsShader()`, but this function has
     * a check for zero in the upper part of shader address. In our case, the address is a
     * pointer to the emulator's own memory, so the check will likely fail. To workaround it we
     * will repeat set shader functionality here as it is trivial. */
    /* SPI_SHADER_PGM_LO_VS */
    cmdbuf = pm4_w4(cmdbuf, PM4_HDR(PM4_IT_SET_SH_REG, 3, PM4_GFX, 0), 0x48u,
                    (u32)(shader_addr >> 8), (u32)(shader_addr >> 40));
    /* SPI_SHADER_PGM_RSRC1_VS/SPI_SHADER_PGM_RSRC2_VS */
    cmdbuf =
        pm4_w4(cmdbuf, PM4_HDR(PM4_IT_SET_SH_REG, 3, PM4_GFX, 0), 0x4au, vs_regs[0], vs_regs[1]);
    /* PA_CL_VS_OUT_CNTL */
    cmdbuf = pm4_w3(cmdbuf, PM4_HDR(PM4_IT_SET_CONTEXT_REG, 2, PM4_GFX, 0), 0x207u, vs_regs[4]);
    /* SPI_VS_OUT_CONFIG */
    cmdbuf = pm4_w3(cmdbuf, PM4_HDR(PM4_IT_SET_CONTEXT_REG, 2, PM4_GFX, 0), 0x1b1u, vs_regs[2]);
    /* SPI_SHADER_POS_FORMAT */
    cmdbuf = pm4_w3(cmdbuf, PM4_HDR(PM4_IT_SET_CONTEXT_REG, 2, PM4_GFX, 0), 0x1c3u, vs_regs[3]);
    pm4_nop(cmdbuf, 11);
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceGnmSetEsShader(u32* cmdbuf, u32 size, const u32* es_regs, u32 shader_modifier) {
    u32 var;

    if (!cmdbuf || size < 0x14) {
        return -1;
    }

    if (!es_regs) {
        c_log(C_LOG_LIB_GNM_DRIVER, C_LOG_LEVEL_ERROR, "sceGnmSetEsShader",
              "Null pointer passed as argument");
        return -1;
    }

    if (shader_modifier & 0xfcfffc3f) {
        c_log(C_LOG_LIB_GNM_DRIVER, C_LOG_LEVEL_ERROR, "sceGnmSetEsShader",
              "Invalid modifier mask");
        return -1;
    }

    if (es_regs[1] != 0) {
        c_log(C_LOG_LIB_GNM_DRIVER, C_LOG_LEVEL_ERROR, "sceGnmSetEsShader",
              "Invalid shader address");
        return -1;
    }

    var = shader_modifier == 0 ? es_regs[2] : ((es_regs[2] & 0xfcfffc3f) | shader_modifier);
    cmdbuf = pm4_w4(cmdbuf, PM4_HDR(PM4_IT_SET_SH_REG, 3, PM4_GFX, 0), 0xc8u, es_regs[0],
                    0u); /* SPI_SHADER_PGM_LO_ES */
    cmdbuf = pm4_w4(cmdbuf, PM4_HDR(PM4_IT_SET_SH_REG, 3, PM4_GFX, 0), 0xcau, var,
                    es_regs[3]); /* SPI_SHADER_PGM_RSRC1_ES */

    pm4_nop(cmdbuf, 11);
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceGnmSetGsRingSizes(u32 esgs_ring_size, u32 gsvs_ring_size) {
    /* Sony libSceGnmDriver validates 1 MB alignment and [4 MB, 8 MB] range. */
    const u32 min_ring_size = 0x400000;  /* 4 MB */
    const u32 max_ring_size = 0x800000;  /* 8 MB */
    const u32 ring_alignment = 0x100000; /* 1 MB */
#ifndef NDEBUG
    c_log(C_LOG_LIB_GNM_DRIVER, C_LOG_LEVEL_TRACE, "sceGnmSetGsRingSizes",
          "esgs_ring_size = 0x%x, gsvs_ring_size = 0x%x", esgs_ring_size, gsvs_ring_size);
#endif
    if (esgs_ring_size < min_ring_size || esgs_ring_size > max_ring_size ||
        ((esgs_ring_size & (ring_alignment - 1)) != 0) || gsvs_ring_size < min_ring_size ||
        gsvs_ring_size > max_ring_size || ((gsvs_ring_size & (ring_alignment - 1)) != 0)) {
        return 0x08000000;
    }
    gnm_gpu_set_gs_ring_sizes(esgs_ring_size >> 8, gsvs_ring_size >> 8);
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceGnmSetGsShader(u32* cmdbuf, u32 size, const u32* gs_regs) {
    if (!cmdbuf || size < 0x1d) {
        return -1;
    }

    if (!gs_regs) {
        c_log(C_LOG_LIB_GNM_DRIVER, C_LOG_LEVEL_ERROR, "sceGnmSetGsShader",
              "Null pointer passed as argument");
        return -1;
    }

    if (gs_regs[1] != 0) {
        c_log(C_LOG_LIB_GNM_DRIVER, C_LOG_LEVEL_ERROR, "sceGnmSetGsShader",
              "Invalid shader address");
        return -1;
    }

    cmdbuf = pm4_w4(cmdbuf, PM4_HDR(PM4_IT_SET_SH_REG, 3, PM4_GFX, 0), 0x88u, gs_regs[0],
                    0u); /* SPI_SHADER_PGM_LO_GS */
    cmdbuf = pm4_w4(cmdbuf, PM4_HDR(PM4_IT_SET_SH_REG, 3, PM4_GFX, 0), 0x8au, gs_regs[2],
                    gs_regs[3]); /* SPI_SHADER_PGM_RSRC1_GS/SPI_SHADER_PGM_RSRC2_GS */
    cmdbuf = pm4_w3(cmdbuf, PM4_HDR(PM4_IT_SET_CONTEXT_REG, 2, PM4_GFX, 0), 0x2e5u,
                    gs_regs[4]); /* VGT_STRMOUT_CONFIG */
    cmdbuf = pm4_w3(cmdbuf, PM4_HDR(PM4_IT_SET_CONTEXT_REG, 2, PM4_GFX, 0), 0x29bu,
                    gs_regs[5]); /* VGT_GS_OUT_PRIM_TYPE */
    cmdbuf = pm4_w3(cmdbuf, PM4_HDR(PM4_IT_SET_CONTEXT_REG, 2, PM4_GFX, 0), 0x2e4u,
                    gs_regs[6]); /* VGT_GS_INSTANCE_CNT */

    pm4_nop(cmdbuf, 11);
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceGnmSetHsShader(u32* cmdbuf, u32 size, const u32* hs_regs, u32 param4) {
    if (!cmdbuf || size < 0x1E) {
        return -1;
    }

    if (!hs_regs) {
        c_log(C_LOG_LIB_GNM_DRIVER, C_LOG_LEVEL_ERROR, "sceGnmSetHsShader",
              "Null pointer passed as argument");
        return -1;
    }

    if (hs_regs[1] != 0) {
        c_log(C_LOG_LIB_GNM_DRIVER, C_LOG_LEVEL_ERROR, "sceGnmSetHsShader",
              "Invalid shader address");
        return -1;
    }

    cmdbuf = pm4_w4(cmdbuf, PM4_HDR(PM4_IT_SET_SH_REG, 3, PM4_GFX, 0), 0x108u, hs_regs[0],
                    0u); /* SPI_SHADER_PGM_LO_HS */
    cmdbuf = pm4_w4(cmdbuf, PM4_HDR(PM4_IT_SET_SH_REG, 3, PM4_GFX, 0), 0x10au, hs_regs[2],
                    hs_regs[3]); /* SPI_SHADER_PGM_RSRC1_HS/SPI_SHADER_PGM_RSRC2_HS */
    cmdbuf = pm4_w4(cmdbuf, PM4_HDR(PM4_IT_SET_CONTEXT_REG, 3, PM4_GFX, 0), 0x286u, hs_regs[5],
                    hs_regs[6]) /* VGT_HOS_MAX_TESS_LEVEL */; /* VGT_HOS_MIN_TESS_LEVEL */
    cmdbuf = pm4_w3(cmdbuf, PM4_HDR(PM4_IT_SET_CONTEXT_REG, 2, PM4_GFX, 0), 0x2dbu,
                    hs_regs[4]); /* VGT_TF_PARAM */
    cmdbuf = pm4_w3(cmdbuf, PM4_HDR(PM4_IT_SET_CONTEXT_REG, 2, PM4_GFX, 0), 0x2d6u,
                    param4); /* VGT_LS_HS_CONFIG */

    /* right padding? */
    pm4_nop(cmdbuf, 11);
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceGnmSetLsShader(u32* cmdbuf, u32 size, const u32* ls_regs, u32 shader_modifier) {
    u32 modifier_mask, var;

    if (!cmdbuf || size < 0x17) {
        return -1;
    }

    if (!ls_regs) {
        c_log(C_LOG_LIB_GNM_DRIVER, C_LOG_LEVEL_ERROR, "sceGnmSetLsShader",
              "Null pointer passed as argument");
        return -1;
    }

    modifier_mask = ((shader_modifier & 0xfffffc3f) == 0) ? 0xfffffc3f : 0xfcfffc3f;
    if (shader_modifier & modifier_mask) {
        c_log(C_LOG_LIB_GNM_DRIVER, C_LOG_LEVEL_ERROR, "sceGnmSetLsShader",
              "Invalid modifier mask");
        return -1;
    }

    if (ls_regs[1] != 0) {
        c_log(C_LOG_LIB_GNM_DRIVER, C_LOG_LEVEL_ERROR, "sceGnmSetLsShader",
              "Invalid shader address");
        return -1;
    }

    var = shader_modifier == 0 ? ls_regs[2] : ((ls_regs[2] & modifier_mask) | shader_modifier);
    cmdbuf = pm4_w4(cmdbuf, PM4_HDR(PM4_IT_SET_SH_REG, 3, PM4_GFX, 0), 0x148u, ls_regs[0],
                    0u); /* SPI_SHADER_PGM_LO_LS */
    cmdbuf = pm4_w3(cmdbuf, PM4_HDR(PM4_IT_SET_SH_REG, 2, PM4_GFX, 0), 0x14bu,
                    ls_regs[3]); /* SPI_SHADER_PGM_RSRC2_LS */
    cmdbuf = pm4_w4(cmdbuf, PM4_HDR(PM4_IT_SET_SH_REG, 3, PM4_GFX, 0), 0x14au, var,
                    ls_regs[3]); /* SPI_SHADER_PGM_RSRC1_LS */

    pm4_nop(cmdbuf, 11);
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceGnmSetPsShader(u32* cmdbuf, u32 size, const u32* ps_regs) {
    if (!cmdbuf || size <= 0x27) {
        return -1;
    }
    if (!ps_regs) {
        cmdbuf = pm4_w4(cmdbuf, PM4_HDR(PM4_IT_SET_SH_REG, 3, PM4_GFX, 0), 8u, 0u,
                        0u); /* SPI_SHADER_PGM_LO_PS/SPI_SHADER_PGM_HI_PS */
        cmdbuf = pm4_w3(cmdbuf, PM4_HDR(PM4_IT_SET_CONTEXT_REG, 2, PM4_GFX, 0), 0x203u,
                        0u); /* DB_SHADER_CONTROL */

        pm4_nop(cmdbuf, 0x20);
    } else {
        if (ps_regs[1] != 0) {
            c_log(C_LOG_LIB_GNM_DRIVER, C_LOG_LEVEL_ERROR, "sceGnmSetPsShader",
                  "Invalid shader address.");
            return -1;
        }

        cmdbuf = pm4_w4(cmdbuf, PM4_HDR(PM4_IT_SET_SH_REG, 3, PM4_GFX, 0), 8u, ps_regs[0],
                        0u); /* SPI_SHADER_PGM_LO_PS/SPI_SHADER_PGM_HI_PS */
        cmdbuf = pm4_w4(cmdbuf, PM4_HDR(PM4_IT_SET_SH_REG, 3, PM4_GFX, 0), 10u, ps_regs[2],
                        ps_regs[3]); /* SPI_SHADER_PGM_RSRC1_PS/SPI_SHADER_PGM_RSRC2_PS */
        cmdbuf = pm4_w4(cmdbuf, PM4_HDR(PM4_IT_SET_CONTEXT_REG, 3, PM4_GFX, 0), 0x1c4u, ps_regs[4],
                        ps_regs[5]); /* SPI_SHADER_Z_FORMAT/SPI_SHADER_COL_FORMAT */
        cmdbuf = pm4_w4(cmdbuf, PM4_HDR(PM4_IT_SET_CONTEXT_REG, 3, PM4_GFX, 0), 0x1b3u, ps_regs[6],
                        ps_regs[7]); /* SPI_PS_INPUT_ENA/SPI_PS_INPUT_ADDR */
        cmdbuf = pm4_w3(cmdbuf, PM4_HDR(PM4_IT_SET_CONTEXT_REG, 2, PM4_GFX, 0), 0x1b6u,
                        ps_regs[8]); /* SPI_PS_IN_CONTROL */
        cmdbuf = pm4_w3(cmdbuf, PM4_HDR(PM4_IT_SET_CONTEXT_REG, 2, PM4_GFX, 0), 0x1b8u,
                        ps_regs[9]); /* SPI_BARYC_CNTL */
        cmdbuf = pm4_w3(cmdbuf, PM4_HDR(PM4_IT_SET_CONTEXT_REG, 2, PM4_GFX, 0), 0x203u,
                        ps_regs[10]); /* DB_SHADER_CONTROL */
        cmdbuf = pm4_w3(cmdbuf, PM4_HDR(PM4_IT_SET_CONTEXT_REG, 2, PM4_GFX, 0), 0x8fu,
                        ps_regs[11]); /* CB_SHADER_MASK */

        pm4_nop(cmdbuf, 11);
    }
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceGnmSetPsShader350(u32* cmdbuf, u32 size, const u32* ps_regs) {
    if (!cmdbuf || size <= 0x27) {
        return -1;
    }
    if (!ps_regs) {
        cmdbuf = pm4_w4(cmdbuf, PM4_HDR(PM4_IT_SET_SH_REG, 3, PM4_GFX, 0), 8u, 0u,
                        0u); /* SPI_SHADER_PGM_LO_PS/SPI_SHADER_PGM_HI_PS */
        cmdbuf = pm4_w3(cmdbuf, PM4_HDR(PM4_IT_SET_CONTEXT_REG, 2, PM4_GFX, 0), 0x203u,
                        0u); /* DB_SHADER_CONTROL */
        cmdbuf = pm4_w3(cmdbuf, PM4_HDR(PM4_IT_SET_CONTEXT_REG, 2, PM4_GFX, 0), 0x8fu,
                        0xfu); /* CB_SHADER_MASK */

        pm4_nop(cmdbuf, 0x1d);
    } else {
        if (ps_regs[1] != 0) {
            c_log(C_LOG_LIB_GNM_DRIVER, C_LOG_LEVEL_ERROR, "sceGnmSetPsShader350",
                  "Invalid shader address.");
            return -1;
        }

        cmdbuf = pm4_w4(cmdbuf, PM4_HDR(PM4_IT_SET_SH_REG, 3, PM4_GFX, 0), 8u, ps_regs[0],
                        0u); /* SPI_SHADER_PGM_LO_PS/SPI_SHADER_PGM_HI_PS */
        cmdbuf = pm4_w4(cmdbuf, PM4_HDR(PM4_IT_SET_SH_REG, 3, PM4_GFX, 0), 10u, ps_regs[2],
                        ps_regs[3]); /* SPI_SHADER_PGM_RSRC1_PS/SPI_SHADER_PGM_RSRC2_PS */
        cmdbuf = pm4_w4(cmdbuf, PM4_HDR(PM4_IT_SET_CONTEXT_REG, 3, PM4_GFX, 0), 0x1c4u, ps_regs[4],
                        ps_regs[5]); /* SPI_SHADER_Z_FORMAT/SPI_SHADER_COL_FORMAT */
        cmdbuf = pm4_w4(cmdbuf, PM4_HDR(PM4_IT_SET_CONTEXT_REG, 3, PM4_GFX, 0), 0x1b3u, ps_regs[6],
                        ps_regs[7]); /* SPI_PS_INPUT_ENA/SPI_PS_INPUT_ADDR */
        cmdbuf = pm4_w3(cmdbuf, PM4_HDR(PM4_IT_SET_CONTEXT_REG, 2, PM4_GFX, 0), 0x1b6u,
                        ps_regs[8]); /* SPI_PS_IN_CONTROL */
        cmdbuf = pm4_w3(cmdbuf, PM4_HDR(PM4_IT_SET_CONTEXT_REG, 2, PM4_GFX, 0), 0x1b8u,
                        ps_regs[9]); /* SPI_BARYC_CNTL */
        cmdbuf = pm4_w3(cmdbuf, PM4_HDR(PM4_IT_SET_CONTEXT_REG, 2, PM4_GFX, 0), 0x203u,
                        ps_regs[10]); /* DB_SHADER_CONTROL */
        cmdbuf = pm4_w3(cmdbuf, PM4_HDR(PM4_IT_SET_CONTEXT_REG, 2, PM4_GFX, 0), 0x8fu,
                        ps_regs[11]); /* CB_SHADER_MASK */

        pm4_nop(cmdbuf, 11);
    }
    return ORBIS_OK;
}

int PS4_SYSV_ABI sceGnmSetResourceRegistrationUserMemory(void) {
    /* Not available in retail firmware */
    return ORBIS_GNM_ERROR_FAILURE;
}

int PS4_SYSV_ABI sceGnmSetResourceUserData(void) {
    /* Not available in retail firmware */
    return ORBIS_GNM_ERROR_FAILURE;
}

int PS4_SYSV_ABI sceGnmSetSpiEnableSqCounters(void) {
    /* Not available in retail firmware */
    return ORBIS_GNM_ERROR_FAILURE;
}

int PS4_SYSV_ABI sceGnmSetSpiEnableSqCountersForUnitInstance(void) {
    /* Not available in retail firmware */
    return ORBIS_GNM_ERROR_FAILURE;
}

s32 PS4_SYSV_ABI sceGnmSetupMipStatsReport(void* output_buffer, u32 size_in_bytes,
                                           u8 intervals_between_reports,
                                           u8 num_reports_before_reset, u32 mip_stats_reset_force) {
    const u32 head = 0x40u;
    c_log(C_LOG_LIB_GNM_DRIVER, C_LOG_LEVEL_DEBUG, "sceGnmSetupMipStatsReport",
          "(STUBBED) output_buffer = 0x%llx, size_in_bytes = 0x%x, intervals = %u, num_reports = "
          "%u, force_reset = %u",
          (unsigned long long)(uintptr_t)output_buffer, size_in_bytes, intervals_between_reports,
          num_reports_before_reset, mip_stats_reset_force);
    if (!output_buffer || ((u64)(uintptr_t)output_buffer & 0xffffff000000003fULL) != 0) {
        return (s32)0x80000001;
    }
    if (size_in_bytes < 0x1040 || (size_in_bytes & 0x7ff) != 0x40) {
        return (s32)0x80000002;
    }
    memset(output_buffer, 0, size_in_bytes);
    memcpy(output_buffer, &head, sizeof(head));
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceGnmSetVgtControl(u32* cmdbuf, u32 size, u32 prim_group_sz_minus_one,
                                     u32 partial_vs_wave_mode, u32 wd_switch_only_on_eop_mode) {
    if (!cmdbuf || size != 3 || (prim_group_sz_minus_one >= 0x100) ||
        ((wd_switch_only_on_eop_mode | partial_vs_wave_mode) >= 2)) {
        return -1;
    }

    if (kernel_is_neo_mode()) {
        const u32 wd_switch_on_eop = (u32)(wd_switch_only_on_eop_mode != 0) << 0x14;
        const u32 switch_on_eoi = (u32)(wd_switch_only_on_eop_mode == 0) << 0x13;
        const u32 reg_value = wd_switch_only_on_eop_mode != 0
                                  ? (partial_vs_wave_mode & 1) << 0x10 | prim_group_sz_minus_one |
                                        wd_switch_on_eop | switch_on_eoi | 0x40000u
                                  : (prim_group_sz_minus_one & 0x1cffffu) | wd_switch_on_eop |
                                        switch_on_eoi | 0x50000u;
        if (!USE_NEO_COMPAT_SEQUENCES) {
            pm4_w3(cmdbuf, PM4_HDR(PM4_IT_SET_UCONFIG_REG, 2, PM4_GFX, 0), 0x40000258u,
                   reg_value | 0x600000u); /* IA_MULTI_VGT_PARAM */
        } else {
            pm4_w3(cmdbuf, PM4_HDR(PM4_IT_SET_CONTEXT_REG, 2, PM4_GFX, 0), 0x100002aau,
                   reg_value); /* IA_MULTI_VGT_PARAM */
        }
    } else {
        const u32 reg_value =
            ((partial_vs_wave_mode & 1) << 0x10) | (prim_group_sz_minus_one & 0xffffu);
        pm4_w3(cmdbuf, PM4_HDR(PM4_IT_SET_CONTEXT_REG, 2, PM4_GFX, 0), 0x2aau,
               reg_value); /* IA_MULTI_VGT_PARAM */
    }
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceGnmSetVsShader(u32* cmdbuf, u32 size, const u32* vs_regs, u32 shader_modifier) {
    u32 var;

    if (!cmdbuf || size <= 0x1c) {
        return -1;
    }

    if (!vs_regs) {
        c_log(C_LOG_LIB_GNM_DRIVER, C_LOG_LEVEL_ERROR, "sceGnmSetVsShader",
              "Null pointer passed as argument");
        return -1;
    }

    if (shader_modifier & 0xfcfffc3f) {
        c_log(C_LOG_LIB_GNM_DRIVER, C_LOG_LEVEL_ERROR, "sceGnmSetVsShader",
              "Invalid modifier mask");
        return -1;
    }

    if (vs_regs[1] != 0) {
        c_log(C_LOG_LIB_GNM_DRIVER, C_LOG_LEVEL_ERROR, "sceGnmSetVsShader",
              "Invalid shader address");
        return -1;
    }

    var = shader_modifier == 0 ? vs_regs[2] : (vs_regs[2] & 0xfcfffc3f) | shader_modifier;
    cmdbuf = pm4_w4(cmdbuf, PM4_HDR(PM4_IT_SET_SH_REG, 3, PM4_GFX, 0), 0x48u, vs_regs[0],
                    0u); /* SPI_SHADER_PGM_LO_VS */
    cmdbuf = pm4_w4(cmdbuf, PM4_HDR(PM4_IT_SET_SH_REG, 3, PM4_GFX, 0), 0x4au, var,
                    vs_regs[3]); /* SPI_SHADER_PGM_RSRC1_VS */
    cmdbuf = pm4_w3(cmdbuf, PM4_HDR(PM4_IT_SET_CONTEXT_REG, 2, PM4_GFX, 0), 0x207u,
                    vs_regs[6]); /* PA_CL_VS_OUT_CNTL */
    cmdbuf = pm4_w3(cmdbuf, PM4_HDR(PM4_IT_SET_CONTEXT_REG, 2, PM4_GFX, 0), 0x1b1u,
                    vs_regs[4]); /* SPI_VS_OUT_CONFIG */
    cmdbuf = pm4_w3(cmdbuf, PM4_HDR(PM4_IT_SET_CONTEXT_REG, 2, PM4_GFX, 0), 0x1c3u,
                    vs_regs[5]); /* SPI_SHADER_POS_FORMAT */

    pm4_nop(cmdbuf, 11);
    return ORBIS_OK;
}

int PS4_SYSV_ABI sceGnmSetWaveLimitMultiplier(void) {
    c_log(C_LOG_LIB_GNM_DRIVER, C_LOG_LEVEL_ERROR, "sceGnmSetWaveLimitMultiplier",
          "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI sceGnmSetWaveLimitMultipliers(void) {
    c_log(C_LOG_LIB_GNM_DRIVER, C_LOG_LEVEL_ERROR, "sceGnmSetWaveLimitMultipliers",
          "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI sceGnmSpmEndSpm(void) {
    /* Not available in retail firmware */
    return ORBIS_GNM_ERROR_FAILURE;
}

int PS4_SYSV_ABI sceGnmSpmInit(void) {
    /* Not available in retail firmware */
    return ORBIS_GNM_ERROR_FAILURE;
}

int PS4_SYSV_ABI sceGnmSpmInit2(void) {
    /* Not available in retail firmware */
    return ORBIS_GNM_ERROR_FAILURE;
}

int PS4_SYSV_ABI sceGnmSpmSetDelay(void) {
    /* Not available in retail firmware */
    return ORBIS_GNM_ERROR_FAILURE;
}

int PS4_SYSV_ABI sceGnmSpmSetMuxRam(void) {
    /* Not available in retail firmware */
    return ORBIS_GNM_ERROR_FAILURE;
}

int PS4_SYSV_ABI sceGnmSpmSetMuxRam2(void) {
    /* Not available in retail firmware */
    return ORBIS_GNM_ERROR_FAILURE;
}

int PS4_SYSV_ABI sceGnmSpmSetSelectCounter(void) {
    /* Not available in retail firmware */
    return ORBIS_GNM_ERROR_FAILURE;
}

int PS4_SYSV_ABI sceGnmSpmSetSpmSelects(void) {
    /* Not available in retail firmware */
    return ORBIS_GNM_ERROR_FAILURE;
}

int PS4_SYSV_ABI sceGnmSpmSetSpmSelects2(void) {
    /* Not available in retail firmware */
    return ORBIS_GNM_ERROR_FAILURE;
}

int PS4_SYSV_ABI sceGnmSpmStartSpm(void) {
    /* Not available in retail firmware */
    return ORBIS_GNM_ERROR_FAILURE;
}

int PS4_SYSV_ABI sceGnmSqttFini(void) {
    /* Not available in retail firmware */
    return ORBIS_GNM_ERROR_FAILURE;
}

int PS4_SYSV_ABI sceGnmSqttFinishTrace(void) {
    /* Not available in retail firmware */
    return ORBIS_GNM_ERROR_FAILURE;
}

int PS4_SYSV_ABI sceGnmSqttGetBcInfo(void) {
    /* Not available in retail firmware */
    return ORBIS_GNM_ERROR_FAILURE;
}

int PS4_SYSV_ABI sceGnmSqttGetGpuClocks(void) {
    /* Not available in retail firmware */
    return ORBIS_GNM_ERROR_FAILURE;
}

int PS4_SYSV_ABI sceGnmSqttGetHiWater(void) {
    /* Not available in retail firmware */
    return ORBIS_GNM_ERROR_FAILURE;
}

int PS4_SYSV_ABI sceGnmSqttGetStatus(void) {
    /* Not available in retail firmware */
    return ORBIS_GNM_ERROR_FAILURE;
}

int PS4_SYSV_ABI sceGnmSqttGetTraceCounter(void) {
    /* Not available in retail firmware */
    return ORBIS_GNM_ERROR_FAILURE;
}

int PS4_SYSV_ABI sceGnmSqttGetTraceWptr(void) {
    /* Not available in retail firmware */
    return ORBIS_GNM_ERROR_FAILURE;
}

int PS4_SYSV_ABI sceGnmSqttGetWrapCounts(void) {
    /* Not available in retail firmware */
    return ORBIS_GNM_ERROR_FAILURE;
}

int PS4_SYSV_ABI sceGnmSqttGetWrapCounts2(void) {
    /* Not available in retail firmware */
    return ORBIS_GNM_ERROR_FAILURE;
}

int PS4_SYSV_ABI sceGnmSqttGetWritebackLabels(void) {
    /* Not available in retail firmware */
    return ORBIS_GNM_ERROR_FAILURE;
}

int PS4_SYSV_ABI sceGnmSqttInit(void) {
    /* Not available in retail firmware */
    return ORBIS_GNM_ERROR_FAILURE;
}

int PS4_SYSV_ABI sceGnmSqttSelectMode(void) {
    /* Not available in retail firmware */
    return ORBIS_GNM_ERROR_FAILURE;
}

int PS4_SYSV_ABI sceGnmSqttSelectTarget(void) {
    /* Not available in retail firmware */
    return ORBIS_GNM_ERROR_FAILURE;
}

int PS4_SYSV_ABI sceGnmSqttSelectTokens(void) {
    /* Not available in retail firmware */
    return ORBIS_GNM_ERROR_FAILURE;
}

int PS4_SYSV_ABI sceGnmSqttSetCuPerfMask(void) {
    /* Not available in retail firmware */
    return ORBIS_GNM_ERROR_FAILURE;
}

int PS4_SYSV_ABI sceGnmSqttSetDceEventWrite(void) {
    /* Not available in retail firmware */
    return ORBIS_GNM_ERROR_FAILURE;
}

int PS4_SYSV_ABI sceGnmSqttSetHiWater(void) {
    /* Not available in retail firmware */
    return ORBIS_GNM_ERROR_FAILURE;
}

int PS4_SYSV_ABI sceGnmSqttSetTraceBuffer2(void) {
    /* Not available in retail firmware */
    return ORBIS_GNM_ERROR_FAILURE;
}

int PS4_SYSV_ABI sceGnmSqttSetTraceBuffers(void) {
    /* Not available in retail firmware */
    return ORBIS_GNM_ERROR_FAILURE;
}

int PS4_SYSV_ABI sceGnmSqttSetUserData(void) {
    /* Not available in retail firmware */
    return ORBIS_GNM_ERROR_FAILURE;
}

int PS4_SYSV_ABI sceGnmSqttSetUserdataTimer(void) {
    /* Not available in retail firmware */
    return ORBIS_GNM_ERROR_FAILURE;
}

int PS4_SYSV_ABI sceGnmSqttStartTrace(void) {
    /* Not available in retail firmware */
    return ORBIS_GNM_ERROR_FAILURE;
}

int PS4_SYSV_ABI sceGnmSqttStopTrace(void) {
    /* Not available in retail firmware */
    return ORBIS_GNM_ERROR_FAILURE;
}

int PS4_SYSV_ABI sceGnmSqttSwitchTraceBuffer(void) {
    /* Not available in retail firmware */
    return ORBIS_GNM_ERROR_FAILURE;
}

int PS4_SYSV_ABI sceGnmSqttSwitchTraceBuffer2(void) {
    /* Not available in retail firmware */
    return ORBIS_GNM_ERROR_FAILURE;
}

int PS4_SYSV_ABI sceGnmSqttWaitForEvent(void) {
    /* Not available in retail firmware */
    return ORBIS_GNM_ERROR_FAILURE;
}

/* The init sequence of the game's SDK, if a frame's first submit, then each pair of buffers */
static s32 perform_submit(u32 count, const u32** dcb_gpu_addrs, const u32* dcb_sizes_in_bytes,
                          const u32** ccb_gpu_addrs, const u32* ccb_sizes_in_bytes) {
    u32 cbpair;
    if (send_init_packet) {
        if ((u32)sdk_version < KERNEL_FW_200) {
            gnm_gpu_submit_gfx(gnm_init_sequence, GNM_WORDS(gnm_init_sequence), NULL, 0);
        } else if ((u32)sdk_version < KERNEL_FW_400) {
            if (kernel_is_neo_mode()) {
#if !USE_NEO_COMPAT_SEQUENCES
                gnm_gpu_submit_gfx(gnm_init_sequence200_neo, GNM_WORDS(gnm_init_sequence200_neo),
                                   NULL, 0);
#else
                gnm_gpu_submit_gfx(gnm_init_sequence200_neo_compat,
                                   GNM_WORDS(gnm_init_sequence200_neo_compat), NULL, 0);
#endif
            } else {
                gnm_gpu_submit_gfx(gnm_init_sequence200, GNM_WORDS(gnm_init_sequence200), NULL, 0);
            }
        } else {
            if (kernel_is_neo_mode()) {
#if !USE_NEO_COMPAT_SEQUENCES
                gnm_gpu_submit_gfx(gnm_init_sequence350_neo, GNM_WORDS(gnm_init_sequence350_neo),
                                   NULL, 0);
#else
                gnm_gpu_submit_gfx(gnm_init_sequence350_neo_compat,
                                   GNM_WORDS(gnm_init_sequence350_neo_compat), NULL, 0);
#endif
            } else {
                gnm_gpu_submit_gfx(gnm_init_sequence350, GNM_WORDS(gnm_init_sequence350), NULL, 0);
            }
        }
        send_init_packet = false;
    }
    for (cbpair = 0u; cbpair < count; ++cbpair) {
        const u32* ccb = ccb_gpu_addrs ? ccb_gpu_addrs[cbpair] : NULL;
        const u32 ccb_size_in_bytes = ccb_sizes_in_bytes ? ccb_sizes_in_bytes[cbpair] : 0;
        gnm_gpu_submit_gfx(dcb_gpu_addrs[cbpair], dcb_sizes_in_bytes[cbpair] >> 2, ccb,
                           ccb_size_in_bytes >> 2);
    }
    return ORBIS_OK;
}

typedef struct submit_args {
    u32 count;
    const u32** dcb_gpu_addrs;
    const u32* dcb_sizes_in_bytes;
    const u32** ccb_gpu_addrs;
    const u32* ccb_sizes_in_bytes;
    /* The flip the last buffer ends with, if @flip */
    bool flip;
    u32 vo_handle;
    u32 buf_idx;
    u32 flip_mode;
    s64 flip_arg;
    s32 result;
} submit_args;

static void submit_run(void* ctx) {
    submit_args* a = (submit_args*)ctx;
    wait_gpu_idle();
    if (a->flip) {
        a->result = patch_flip_request((u32*)a->dcb_gpu_addrs[a->count - 1],
                                       a->dcb_sizes_in_bytes[a->count - 1] / 4, a->vo_handle,
                                       a->buf_idx, a->flip_mode, a->flip_arg);
        if (a->result != ORBIS_OK) {
            return;
        }
    }
    a->result = perform_submit(a->count, a->dcb_gpu_addrs, a->dcb_sizes_in_bytes, a->ccb_gpu_addrs,
                               a->ccb_sizes_in_bytes);
}

/* The sizes a submission may have; 0 if it may go on */
static s32 check_submit_sizes(const char* func, u32 count, const u32* dcb_sizes_in_bytes,
                              const u32* ccb_sizes_in_bytes) {
    u32 i;
    for (i = 0; i < count; i++) {
        if (dcb_sizes_in_bytes[i] == 0) {
            c_log(C_LOG_LIB_GNM_DRIVER, C_LOG_LEVEL_ERROR, func, "Submitting a null DCB %u", i);
            return (s32)0x80d11000;
        }
        if (dcb_sizes_in_bytes[i] > 0x3ffffc) {
            c_log(C_LOG_LIB_GNM_DRIVER, C_LOG_LEVEL_ERROR, func,
                  "dcbSizesInBytes[%u] (%u) is limited to (2*20)-1 DWORDS", i,
                  dcb_sizes_in_bytes[i]);
            return (s32)0x80d11000;
        }
        if (ccb_sizes_in_bytes && ccb_sizes_in_bytes[i] > 0x3ffffc) {
            c_log(C_LOG_LIB_GNM_DRIVER, C_LOG_LEVEL_ERROR, func,
                  "ccbSizesInBytes[%u] (%u) is limited to (2*20)-1 DWORDS", i,
                  ccb_sizes_in_bytes[i]);
            return (s32)0x80d11000;
        }
    }
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceGnmSubmitAndFlipCommandBuffers(u32 count, u32* dcb_gpu_addrs[],
                                                   u32* dcb_sizes_in_bytes, u32* ccb_gpu_addrs[],
                                                   u32* ccb_sizes_in_bytes, u32 vo_handle,
                                                   u32 buf_idx, u32 flip_mode, s64 flip_arg) {
    return sceGnmSubmitAndFlipCommandBuffersForWorkload(
        count, count, dcb_gpu_addrs, dcb_sizes_in_bytes, ccb_gpu_addrs, ccb_sizes_in_bytes,
        vo_handle, buf_idx, flip_mode, flip_arg);
}

s32 PS4_SYSV_ABI sceGnmSubmitAndFlipCommandBuffersForWorkload(
    u32 workload, u32 count, u32* dcb_gpu_addrs[], u32* dcb_sizes_in_bytes, u32* ccb_gpu_addrs[],
    u32* ccb_sizes_in_bytes, u32 vo_handle, u32 buf_idx, u32 flip_mode, s64 flip_arg) {
    static const char func[] = "sceGnmSubmitAndFlipCommandBuffersForWorkload";
    submit_args a;
    s32 result;
    (void)workload;
    c_log(C_LOG_LIB_GNM_DRIVER, C_LOG_LEVEL_DEBUG, func, "called [buf = %u]", buf_idx);
    if (count != 0) {
        if (!dcb_gpu_addrs) {
            c_log(C_LOG_LIB_GNM_DRIVER, C_LOG_LEVEL_ERROR, func, "dcbGpuAddrs must not be NULL");
            return (s32)0x80d11000;
        }
        if (!dcb_sizes_in_bytes) {
            c_log(C_LOG_LIB_GNM_DRIVER, C_LOG_LEVEL_ERROR, func,
                  "dcbSizesInBytes must not be NULL");
            return (s32)0x80d11000;
        }
        if (ccb_sizes_in_bytes && !ccb_gpu_addrs) {
            c_log(C_LOG_LIB_GNM_DRIVER, C_LOG_LEVEL_ERROR, func,
                  "ccbGpuAddrs must not be NULL if ccbSizesInBytes is non-NULL");
            return (s32)0x80d11000;
        }
    }
    result = check_submit_sizes(func, count, dcb_sizes_in_bytes, ccb_sizes_in_bytes);
    if (result != ORBIS_OK) {
        return result;
    }
    a.count = count;
    a.dcb_gpu_addrs = (const u32**)dcb_gpu_addrs;
    a.dcb_sizes_in_bytes = dcb_sizes_in_bytes;
    a.ccb_gpu_addrs = (const u32**)ccb_gpu_addrs;
    a.ccb_sizes_in_bytes = ccb_sizes_in_bytes;
    a.flip = true;
    a.vo_handle = vo_handle;
    a.buf_idx = buf_idx;
    a.flip_mode = flip_mode;
    a.flip_arg = flip_arg;
    a.result = ORBIS_OK;
    combiner_run(&submit_combiner, submit_run, &a);
    return a.result;
}

s32 PS4_SYSV_ABI sceGnmSubmitCommandBuffersForWorkload(u32 workload, u32 count,
                                                       const u32* dcb_gpu_addrs[],
                                                       u32* dcb_sizes_in_bytes,
                                                       const u32* ccb_gpu_addrs[],
                                                       u32* ccb_sizes_in_bytes) {
    static const char func[] = "sceGnmSubmitCommandBuffersForWorkload";
    submit_args a;
    s32 result;
    (void)workload;
    c_log(C_LOG_LIB_GNM_DRIVER, C_LOG_LEVEL_DEBUG, func, "called");
    if (!dcb_gpu_addrs || !dcb_sizes_in_bytes) {
        c_log(C_LOG_LIB_GNM_DRIVER, C_LOG_LEVEL_ERROR, func,
              "dcbGpuAddrs and dcbSizesInBytes must not be NULL");
        return (s32)0x80d11000;
    }
    result = check_submit_sizes(func, count, dcb_sizes_in_bytes, ccb_sizes_in_bytes);
    if (result != ORBIS_OK) {
        return result;
    }
    a.count = count;
    a.dcb_gpu_addrs = dcb_gpu_addrs;
    a.dcb_sizes_in_bytes = dcb_sizes_in_bytes;
    a.ccb_gpu_addrs = ccb_gpu_addrs;
    a.ccb_sizes_in_bytes = ccb_sizes_in_bytes;
    a.flip = false;
    a.vo_handle = a.buf_idx = a.flip_mode = 0;
    a.flip_arg = 0;
    a.result = ORBIS_OK;
    combiner_run(&submit_combiner, submit_run, &a);
    return a.result;
}

s32 PS4_SYSV_ABI sceGnmSubmitCommandBuffers(u32 count, const u32* dcb_gpu_addrs[],
                                            u32* dcb_sizes_in_bytes, const u32* ccb_gpu_addrs[],
                                            u32* ccb_sizes_in_bytes) {
    return sceGnmSubmitCommandBuffersForWorkload(count, count, dcb_gpu_addrs, dcb_sizes_in_bytes,
                                                 ccb_gpu_addrs, ccb_sizes_in_bytes);
}

static void submit_done_run(void* ctx) {
    (void)ctx;
    wait_gpu_idle();
    take_submission_lock();
    gnm_gpu_submit_done();
    send_init_packet = true;
}

s32 PS4_SYSV_ABI sceGnmSubmitDone(void) {
    c_log(C_LOG_LIB_GNM_DRIVER, C_LOG_LEVEL_DEBUG, "sceGnmSubmitDone", "called");
    combiner_run(&submit_combiner, submit_done_run, NULL);
    return ORBIS_OK;
}

int PS4_SYSV_ABI sceGnmUnmapComputeQueue(u32 vqid) {
    gnm_gpu_asc_unmap(vqid - 1);
    return ORBIS_OK;
}

int PS4_SYSV_ABI sceGnmUnregisterAllResourcesForOwner(void) {
    /* Not available in retail firmware */
    return ORBIS_GNM_ERROR_FAILURE;
}

int PS4_SYSV_ABI sceGnmUnregisterOwnerAndResources(void) {
    /* Not available in retail firmware */
    return ORBIS_GNM_ERROR_FAILURE;
}

int PS4_SYSV_ABI sceGnmUnregisterResource(void) {
    /* Not available in retail firmware */
    return ORBIS_GNM_ERROR_FAILURE;
}

s32 PS4_SYSV_ABI sceGnmUpdateGsShader(u32* cmdbuf, u32 size, const u32* gs_regs) {
    if (!cmdbuf || size < 0x1d) {
        return -1;
    }

    if (!gs_regs) {
        c_log(C_LOG_LIB_GNM_DRIVER, C_LOG_LEVEL_ERROR, "sceGnmUpdateGsShader",
              "Null pointer passed as argument");
        return -1;
    }

    if (gs_regs[1] != 0) {
        c_log(C_LOG_LIB_GNM_DRIVER, C_LOG_LEVEL_ERROR, "sceGnmUpdateGsShader",
              "Invalid shader address");
        return -1;
    }

    cmdbuf = pm4_w4(cmdbuf, PM4_HDR(PM4_IT_SET_SH_REG, 3, PM4_GFX, 0), 0x88u, gs_regs[0],
                    0u); /* SPI_SHADER_PGM_LO_GS */
    cmdbuf = pm4_w4(cmdbuf, PM4_HDR(PM4_IT_SET_SH_REG, 3, PM4_GFX, 0), 0x8au, gs_regs[2],
                    gs_regs[3]); /* SPI_SHADER_PGM_RSRC1_GS/SPI_SHADER_PGM_RSRC2_GS */
    cmdbuf = pm4_w3(cmdbuf, PM4_HDR(PM4_IT_NOP, 2, PM4_GFX, 0), 0xc01e02e5u, gs_regs[4]);
    cmdbuf = pm4_w3(cmdbuf, PM4_HDR(PM4_IT_NOP, 2, PM4_GFX, 0), 0xc01e029bu, gs_regs[5]);
    cmdbuf = pm4_w3(cmdbuf, PM4_HDR(PM4_IT_NOP, 2, PM4_GFX, 0), 0xc01e02e4u, gs_regs[6]);

    pm4_nop(cmdbuf, 11);
    return ORBIS_OK;
}

int PS4_SYSV_ABI sceGnmUpdateHsShader(u32* cmdbuf, u32 size, const u32* hs_regs, u32 ls_hs_config) {
    if (!cmdbuf || size <= 0x1c) {
        return -1;
    }

    if (!hs_regs) {
        c_log(C_LOG_LIB_GNM_DRIVER, C_LOG_LEVEL_ERROR, "sceGnmUpdateHsShader",
              "Null pointer passed as argument");
        return -1;
    }

    if (hs_regs[1] != 0) {
        c_log(C_LOG_LIB_GNM_DRIVER, C_LOG_LEVEL_ERROR, "sceGnmUpdateHsShader",
              "Invalid shader address");
        return -1;
    }

    cmdbuf = pm4_w4(cmdbuf, PM4_HDR(PM4_IT_SET_SH_REG, 3, PM4_GFX, 0), 0x108u, hs_regs[0],
                    0u); /* SPI_SHADER_PGM_LO_HS/SPI_SHADER_PGM_HI_HS */
    cmdbuf = pm4_w4(cmdbuf, PM4_HDR(PM4_IT_SET_SH_REG, 3, PM4_GFX, 0), 0x10au, hs_regs[2],
                    hs_regs[3]); /* SPI_SHADER_PGM_RSRC1_HS/SPI_SHADER_PGM_RSRC1_LS */
    cmdbuf = pm4_w4(cmdbuf, PM4_HDR(PM4_IT_NOP, 3, PM4_GFX, 0), 0xc01e0286u, hs_regs[5],
                    hs_regs[6]); /* VGT_HOS_MAX_TESS_LEVEL/VGT_HOS_MIN_TESS_LEVEL update */
    cmdbuf = pm4_w3(cmdbuf, PM4_HDR(PM4_IT_NOP, 2, PM4_GFX, 0), 0xc01e02dbu,
                    hs_regs[4]); /* VGT_TF_PARAM update */
    cmdbuf = pm4_w3(cmdbuf, PM4_HDR(PM4_IT_NOP, 2, PM4_GFX, 0), 0xc01e02d6u,
                    ls_hs_config); /* VGT_LS_HS_CONFIG update */

    pm4_nop(cmdbuf, 11);
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceGnmUpdatePsShader(u32* cmdbuf, u32 size, const u32* ps_regs) {
    if (!cmdbuf || size <= 0x27) {
        return -1;
    }
    if (!ps_regs) {
        cmdbuf = pm4_w4(cmdbuf, PM4_HDR(PM4_IT_SET_SH_REG, 3, PM4_GFX, 0), 8u, 0u,
                        0u); /* SPI_SHADER_PGM_LO_PS/SPI_SHADER_PGM_HI_PS */
        cmdbuf = pm4_w3(cmdbuf, PM4_HDR(PM4_IT_NOP, 2, PM4_GFX, 0), 0xc01e0203u,
                        0u); /* DB_SHADER_CONTROL update */
        pm4_nop(cmdbuf, 0x20);
    } else {
        if (ps_regs[1] != 0) {
            c_log(C_LOG_LIB_GNM_DRIVER, C_LOG_LEVEL_ERROR, "sceGnmUpdatePsShader",
                  "Invalid shader address.");
            return -1;
        }

        cmdbuf = pm4_w4(cmdbuf, PM4_HDR(PM4_IT_SET_SH_REG, 3, PM4_GFX, 0), 8u, ps_regs[0],
                        0u); /* SPI_SHADER_PGM_LO_PS/SPI_SHADER_PGM_HI_PS */
        cmdbuf = pm4_w4(cmdbuf, PM4_HDR(PM4_IT_SET_SH_REG, 3, PM4_GFX, 0), 10u, ps_regs[2],
                        ps_regs[3]); /* SPI_SHADER_PGM_RSRC1_PS/SPI_SHADER_PGM_RSRC2_PS */
        cmdbuf = pm4_w4(cmdbuf, PM4_HDR(PM4_IT_NOP, 3, PM4_GFX, 0), 0xc01e01c4u, ps_regs[4],
                        ps_regs[5]); /* SPI_SHADER_Z_FORMAT/SPI_SHADER_COL_FORMAT update */
        cmdbuf = pm4_w4(cmdbuf, PM4_HDR(PM4_IT_NOP, 3, PM4_GFX, 0), 0xc01e01b3u, ps_regs[6],
                        ps_regs[7]); /* SPI_PS_INPUT_ENA/SPI_PS_INPUT_ADDR update */
        cmdbuf = pm4_w3(cmdbuf, PM4_HDR(PM4_IT_NOP, 2, PM4_GFX, 0), 0xc01e01b6u,
                        ps_regs[8]); /* SPI_PS_IN_CONTROL update */
        cmdbuf = pm4_w3(cmdbuf, PM4_HDR(PM4_IT_NOP, 2, PM4_GFX, 0), 0xc01e01b8u,
                        ps_regs[9]); /* SPI_BARYC_CNTL update */
        cmdbuf = pm4_w3(cmdbuf, PM4_HDR(PM4_IT_NOP, 2, PM4_GFX, 0), 0xc01e0203u,
                        ps_regs[10]); /* DB_SHADER_CONTROL update */
        cmdbuf = pm4_w3(cmdbuf, PM4_HDR(PM4_IT_NOP, 2, PM4_GFX, 0), 0xc01e008fu,
                        ps_regs[11]); /* CB_SHADER_MASK update */

        pm4_nop(cmdbuf, 11);
    }
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceGnmUpdatePsShader350(u32* cmdbuf, u32 size, const u32* ps_regs) {
    if (!cmdbuf || size <= 0x27) {
        return -1;
    }
    if (!ps_regs) {
        cmdbuf = pm4_w4(cmdbuf, PM4_HDR(PM4_IT_SET_SH_REG, 3, PM4_GFX, 0), 8u, 0u,
                        0u); /* SPI_SHADER_PGM_LO_PS/SPI_SHADER_PGM_HI_PS */
        cmdbuf = pm4_w3(cmdbuf, PM4_HDR(PM4_IT_NOP, 2, PM4_GFX, 0), 0xc01e0203u,
                        0u); /* DB_SHADER_CONTROL update */
        cmdbuf = pm4_w3(cmdbuf, PM4_HDR(PM4_IT_SET_CONTEXT_REG, 2, PM4_GFX, 0), 0x8fu,
                        0xfu); /* CB_SHADER_MASK */

        pm4_nop(cmdbuf, 0x1d);
    } else {
        if (ps_regs[1] != 0) {
            c_log(C_LOG_LIB_GNM_DRIVER, C_LOG_LEVEL_ERROR, "sceGnmUpdatePsShader350",
                  "Invalid shader address.");
            return -1;
        }

        cmdbuf = pm4_w4(cmdbuf, PM4_HDR(PM4_IT_SET_SH_REG, 3, PM4_GFX, 0), 8u, ps_regs[0],
                        0u); /* SPI_SHADER_PGM_LO_PS/SPI_SHADER_PGM_HI_PS */
        cmdbuf = pm4_w4(cmdbuf, PM4_HDR(PM4_IT_SET_SH_REG, 3, PM4_GFX, 0), 10u, ps_regs[2],
                        ps_regs[3]); /* SPI_SHADER_PGM_RSRC1_PS/SPI_SHADER_PGM_RSRC2_PS */
        cmdbuf = pm4_w4(cmdbuf, PM4_HDR(PM4_IT_NOP, 3, PM4_GFX, 0), 0xc01e01c4u, ps_regs[4],
                        ps_regs[5]); /* SPI_SHADER_Z_FORMAT/SPI_SHADER_COL_FORMAT update */
        cmdbuf = pm4_w4(cmdbuf, PM4_HDR(PM4_IT_NOP, 3, PM4_GFX, 0), 0xc01e01b3u, ps_regs[6],
                        ps_regs[7]); /* SPI_PS_INPUT_ENA/SPI_PS_INPUT_ADDR update */
        cmdbuf = pm4_w3(cmdbuf, PM4_HDR(PM4_IT_NOP, 2, PM4_GFX, 0), 0xc01e01b6u,
                        ps_regs[8]); /* SPI_PS_IN_CONTROL update */
        cmdbuf = pm4_w3(cmdbuf, PM4_HDR(PM4_IT_NOP, 2, PM4_GFX, 0), 0xc01e01b8u,
                        ps_regs[9]); /* SPI_BARYC_CNTL update */
        cmdbuf = pm4_w3(cmdbuf, PM4_HDR(PM4_IT_NOP, 2, PM4_GFX, 0), 0xc01e0203u,
                        ps_regs[10]); /* DB_SHADER_CONTROL update */
        cmdbuf = pm4_w3(cmdbuf, PM4_HDR(PM4_IT_NOP, 2, PM4_GFX, 0), 0xc01e008fu,
                        ps_regs[11]); /* CB_SHADER_MASK update */

        pm4_nop(cmdbuf, 11);
    }
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceGnmUpdateVsShader(u32* cmdbuf, u32 size, const u32* vs_regs,
                                      u32 shader_modifier) {
    u32 var;

    if (!cmdbuf || size <= 0x1c) {
        return -1;
    }

    if (!vs_regs) {
        c_log(C_LOG_LIB_GNM_DRIVER, C_LOG_LEVEL_ERROR, "sceGnmUpdateVsShader",
              "Null pointer passed as argument");
        return -1;
    }

    if (shader_modifier & 0xfcfffc3f) {
        c_log(C_LOG_LIB_GNM_DRIVER, C_LOG_LEVEL_ERROR, "sceGnmUpdateVsShader",
              "Invalid modifier mask");
        return -1;
    }

    if (vs_regs[1] != 0) {
        c_log(C_LOG_LIB_GNM_DRIVER, C_LOG_LEVEL_ERROR, "sceGnmUpdateVsShader",
              "Invalid shader address");
        return -1;
    }

    var = shader_modifier == 0 ? vs_regs[2] : ((vs_regs[2] & 0xfcfffc3f) | shader_modifier);
    cmdbuf = pm4_w4(cmdbuf, PM4_HDR(PM4_IT_SET_SH_REG, 3, PM4_GFX, 0), 0x48u, vs_regs[0],
                    0u); /* SPI_SHADER_PGM_LO_VS */
    cmdbuf = pm4_w4(cmdbuf, PM4_HDR(PM4_IT_SET_SH_REG, 3, PM4_GFX, 0), 0x4au, var,
                    vs_regs[3]); /* SPI_SHADER_PGM_RSRC1_VS */
    cmdbuf = pm4_w3(cmdbuf, PM4_HDR(PM4_IT_NOP, 2, PM4_GFX, 0), 0xc01e0207u,
                    vs_regs[6]); /* PA_CL_VS_OUT_CNTL update */
    cmdbuf = pm4_w3(cmdbuf, PM4_HDR(PM4_IT_NOP, 2, PM4_GFX, 0), 0xc01e01b1u,
                    vs_regs[4]); /* PA_CL_VS_OUT_CNTL update */
    cmdbuf = pm4_w3(cmdbuf, PM4_HDR(PM4_IT_NOP, 2, PM4_GFX, 0), 0xc01e01c3u,
                    vs_regs[5]); /* PA_CL_VS_OUT_CNTL update */

    pm4_nop(cmdbuf, 11);
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceGnmValidateCommandBuffers(void) {
    /* Not available in retail firmware */
    return ORBIS_GNM_ERROR_VALIDATION_NOT_ENABLED;
}

int PS4_SYSV_ABI sceGnmValidateDisableDiagnostics(void) {
    /* Not available in retail firmware */
    return ORBIS_GNM_ERROR_VALIDATION_NOT_ENABLED;
}

int PS4_SYSV_ABI sceGnmValidateDisableDiagnostics2(void) {
    /* Not available in retail firmware */
    return ORBIS_GNM_ERROR_VALIDATION_NOT_ENABLED;
}

int PS4_SYSV_ABI sceGnmValidateDispatchCommandBuffers(void) {
    /* Not available in retail firmware */
    return ORBIS_GNM_ERROR_VALIDATION_NOT_ENABLED;
}

int PS4_SYSV_ABI sceGnmValidateDrawCommandBuffers(void) {
    /* Not available in retail firmware */
    return ORBIS_GNM_ERROR_VALIDATION_NOT_ENABLED;
}

int PS4_SYSV_ABI sceGnmValidateGetDiagnosticInfo(void) {
    /* Not available in retail firmware */
    return ORBIS_GNM_ERROR_VALIDATION_NOT_ENABLED;
}

int PS4_SYSV_ABI sceGnmValidateGetDiagnostics(void) {
    /* Not available in retail firmware */
    return ORBIS_GNM_ERROR_VALIDATION_NOT_ENABLED;
}

int PS4_SYSV_ABI sceGnmValidateGetVersion(void) {
    /* Not available in retail firmware */
    return 0;
}

bool PS4_SYSV_ABI sceGnmValidateOnSubmitEnabled(void) {
    /* Not available in retail firmware */
    return false;
}

int PS4_SYSV_ABI sceGnmValidateResetState(void) {
    /* Not available in retail firmware */
    return ORBIS_GNM_ERROR_VALIDATION_NOT_ENABLED;
}

int PS4_SYSV_ABI sceGnmValidationRegisterMemoryCheckCallback(void) {
    /* Not available in retail firmware */
    return ORBIS_GNM_ERROR_VALIDATION_NOT_ENABLED;
}

int PS4_SYSV_ABI sceRazorCaptureCommandBuffersOnlyImmediate(void) {
    /* Not available in retail firmware */
    return ORBIS_GNM_ERROR_CAPTURE_FAILED_INTERNAL;
}

int PS4_SYSV_ABI sceRazorCaptureCommandBuffersOnlySinceLastFlip(void) {
    /* Not available in retail firmware */
    return ORBIS_GNM_ERROR_CAPTURE_FAILED_INTERNAL;
}

int PS4_SYSV_ABI sceRazorCaptureImmediate(void) {
    /* Not available in retail firmware */
    return ORBIS_GNM_ERROR_CAPTURE_FAILED_INTERNAL;
}

int PS4_SYSV_ABI sceRazorCaptureSinceLastFlip(void) {
    /* Not available in retail firmware */
    return ORBIS_GNM_ERROR_CAPTURE_FAILED_INTERNAL;
}

bool PS4_SYSV_ABI sceRazorIsLoaded(void) {
    /* Not available in retail firmware */
    return false;
}

int PS4_SYSV_ABI Gnm_Func_063D065A2D6359C3(void) {
    c_log(C_LOG_LIB_GNM_DRIVER, C_LOG_LEVEL_ERROR, "Gnm_Func_063D065A2D6359C3", "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI Gnm_Func_0CABACAFB258429D(void) {
    c_log(C_LOG_LIB_GNM_DRIVER, C_LOG_LEVEL_ERROR, "Gnm_Func_0CABACAFB258429D", "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI Gnm_Func_150CF336FC2E99A3(void) {
    c_log(C_LOG_LIB_GNM_DRIVER, C_LOG_LEVEL_ERROR, "Gnm_Func_150CF336FC2E99A3", "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI Gnm_Func_17CA687F9EE52D49(void) {
    c_log(C_LOG_LIB_GNM_DRIVER, C_LOG_LEVEL_ERROR, "Gnm_Func_17CA687F9EE52D49", "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI Gnm_Func_1870B89F759C6B45(void) {
    c_log(C_LOG_LIB_GNM_DRIVER, C_LOG_LEVEL_ERROR, "Gnm_Func_1870B89F759C6B45", "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI Gnm_Func_26F9029EF68A955E(void) {
    c_log(C_LOG_LIB_GNM_DRIVER, C_LOG_LEVEL_ERROR, "Gnm_Func_26F9029EF68A955E", "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI Gnm_Func_301E3DBBAB092DB0(void) {
    c_log(C_LOG_LIB_GNM_DRIVER, C_LOG_LEVEL_ERROR, "Gnm_Func_301E3DBBAB092DB0", "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI Gnm_Func_30BAFE172AF17FEF(void) {
    c_log(C_LOG_LIB_GNM_DRIVER, C_LOG_LEVEL_ERROR, "Gnm_Func_30BAFE172AF17FEF", "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI Gnm_Func_3E6A3E8203D95317(void) {
    c_log(C_LOG_LIB_GNM_DRIVER, C_LOG_LEVEL_ERROR, "Gnm_Func_3E6A3E8203D95317", "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI Gnm_Func_40FEEF0C6534C434(void) {
    c_log(C_LOG_LIB_GNM_DRIVER, C_LOG_LEVEL_ERROR, "Gnm_Func_40FEEF0C6534C434", "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI Gnm_Func_416B9079DE4CBACE(void) {
    c_log(C_LOG_LIB_GNM_DRIVER, C_LOG_LEVEL_ERROR, "Gnm_Func_416B9079DE4CBACE", "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI Gnm_Func_4774D83BB4DDBF9A(void) {
    c_log(C_LOG_LIB_GNM_DRIVER, C_LOG_LEVEL_ERROR, "Gnm_Func_4774D83BB4DDBF9A", "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI Gnm_Func_50678F1CCEEB9A00(void) {
    c_log(C_LOG_LIB_GNM_DRIVER, C_LOG_LEVEL_ERROR, "Gnm_Func_50678F1CCEEB9A00", "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI Gnm_Func_54A2EC5FA4C62413(void) {
    c_log(C_LOG_LIB_GNM_DRIVER, C_LOG_LEVEL_ERROR, "Gnm_Func_54A2EC5FA4C62413", "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI Gnm_Func_5A9C52C83138AE6B(void) {
    c_log(C_LOG_LIB_GNM_DRIVER, C_LOG_LEVEL_ERROR, "Gnm_Func_5A9C52C83138AE6B", "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI Gnm_Func_5D22193A31EA1142(void) {
    c_log(C_LOG_LIB_GNM_DRIVER, C_LOG_LEVEL_ERROR, "Gnm_Func_5D22193A31EA1142", "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI Gnm_Func_725A36DEBB60948D(void) {
    c_log(C_LOG_LIB_GNM_DRIVER, C_LOG_LEVEL_ERROR, "Gnm_Func_725A36DEBB60948D", "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI Gnm_Func_8021A502FA61B9BB(void) {
    c_log(C_LOG_LIB_GNM_DRIVER, C_LOG_LEVEL_ERROR, "Gnm_Func_8021A502FA61B9BB", "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI Gnm_Func_9D002FE0FA40F0E6(void) {
    c_log(C_LOG_LIB_GNM_DRIVER, C_LOG_LEVEL_ERROR, "Gnm_Func_9D002FE0FA40F0E6", "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI Gnm_Func_9D297F36A7028B71(void) {
    c_log(C_LOG_LIB_GNM_DRIVER, C_LOG_LEVEL_ERROR, "Gnm_Func_9D297F36A7028B71", "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI Gnm_Func_A2D7EC7A7BCF79B3(void) {
    c_log(C_LOG_LIB_GNM_DRIVER, C_LOG_LEVEL_ERROR, "Gnm_Func_A2D7EC7A7BCF79B3", "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI Gnm_Func_AA12A3CB8990854A(void) {
    c_log(C_LOG_LIB_GNM_DRIVER, C_LOG_LEVEL_ERROR, "Gnm_Func_AA12A3CB8990854A", "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI Gnm_Func_ADC8DDC005020BC6(void) {
    c_log(C_LOG_LIB_GNM_DRIVER, C_LOG_LEVEL_ERROR, "Gnm_Func_ADC8DDC005020BC6", "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI Gnm_Func_B0A8688B679CB42D(void) {
    c_log(C_LOG_LIB_GNM_DRIVER, C_LOG_LEVEL_ERROR, "Gnm_Func_B0A8688B679CB42D", "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI Gnm_Func_B489020B5157A5FF(void) {
    c_log(C_LOG_LIB_GNM_DRIVER, C_LOG_LEVEL_ERROR, "Gnm_Func_B489020B5157A5FF", "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI Gnm_Func_BADE7B4C199140DD(void) {
    c_log(C_LOG_LIB_GNM_DRIVER, C_LOG_LEVEL_ERROR, "Gnm_Func_BADE7B4C199140DD", "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI Gnm_Func_D1511B9DCFFB3DD9(void) {
    c_log(C_LOG_LIB_GNM_DRIVER, C_LOG_LEVEL_ERROR, "Gnm_Func_D1511B9DCFFB3DD9", "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI Gnm_Func_D53446649B02E58E(void) {
    c_log(C_LOG_LIB_GNM_DRIVER, C_LOG_LEVEL_ERROR, "Gnm_Func_D53446649B02E58E", "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI Gnm_Func_D8B6E8E28E1EF0A3(void) {
    c_log(C_LOG_LIB_GNM_DRIVER, C_LOG_LEVEL_ERROR, "Gnm_Func_D8B6E8E28E1EF0A3", "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI Gnm_Func_D93D733A19DD7454(void) {
    c_log(C_LOG_LIB_GNM_DRIVER, C_LOG_LEVEL_ERROR, "Gnm_Func_D93D733A19DD7454", "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI Gnm_Func_DE995443BC2A8317(void) {
    c_log(C_LOG_LIB_GNM_DRIVER, C_LOG_LEVEL_ERROR, "Gnm_Func_DE995443BC2A8317", "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI Gnm_Func_DF6E9528150C23FF(void) {
    c_log(C_LOG_LIB_GNM_DRIVER, C_LOG_LEVEL_ERROR, "Gnm_Func_DF6E9528150C23FF", "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI Gnm_Func_ECB4C6BA41FE3350(void) {
    c_log(C_LOG_LIB_GNM_DRIVER, C_LOG_LEVEL_ERROR, "Gnm_Func_ECB4C6BA41FE3350", "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI sceGnmDebugModuleReset(void) {
    /* Not available in retail firmware */
    return ORBIS_GNM_ERROR_FAILURE;
}

int PS4_SYSV_ABI sceGnmDebugReset(void) {
    /* Not available in retail firmware */
    return ORBIS_GNM_ERROR_FAILURE;
}

int PS4_SYSV_ABI Gnm_Func_C4C328B7CF3B4171(void) {
    c_log(C_LOG_LIB_GNM_DRIVER, C_LOG_LEVEL_ERROR, "Gnm_Func_C4C328B7CF3B4171", "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI sceGnmDrawInitToDefaultContextStateInternalCommand(u32* cmdbuf, u32 size) {
    if (sdk_version >= KERNEL_FW_400) {
        return sceGnmDrawInitToDefaultContextState400(cmdbuf, size);
    }
    return sceGnmDrawInitToDefaultContextState(cmdbuf, size);
}

int PS4_SYSV_ABI sceGnmDrawInitToDefaultContextStateInternalSize(void) {
    if (sdk_version >= KERNEL_FW_400) {
        return 0x100;
    }
    return 0x20;
}

int PS4_SYSV_ABI sceGnmFindResources(void) {
    /* Not available in retail firmware */
    return ORBIS_GNM_ERROR_FAILURE;
}

int PS4_SYSV_ABI sceGnmGetResourceRegistrationBuffers(void) {
    /* Not available in retail firmware */
    return ORBIS_GNM_ERROR_FAILURE;
}

int PS4_SYSV_ABI sceGnmRegisterOwnerForSystem(void) {
    /* Not available in retail firmware */
    return ORBIS_GNM_ERROR_FAILURE;
}

int PS4_SYSV_ABI Gnm_Func_1C43886B16EE5530(void) {
    c_log(C_LOG_LIB_GNM_DRIVER, C_LOG_LEVEL_ERROR, "Gnm_Func_1C43886B16EE5530", "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI Gnm_Func_81037019ECCD0E01(void) {
    c_log(C_LOG_LIB_GNM_DRIVER, C_LOG_LEVEL_ERROR, "Gnm_Func_81037019ECCD0E01", "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI Gnm_Func_BFB41C057478F0BF(void) {
    c_log(C_LOG_LIB_GNM_DRIVER, C_LOG_LEVEL_ERROR, "Gnm_Func_BFB41C057478F0BF", "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI Gnm_Func_E51D44DB8151238C(void) {
    c_log(C_LOG_LIB_GNM_DRIVER, C_LOG_LEVEL_ERROR, "Gnm_Func_E51D44DB8151238C", "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI Gnm_Func_F916890425496553(void) {
    c_log(C_LOG_LIB_GNM_DRIVER, C_LOG_LEVEL_ERROR, "Gnm_Func_F916890425496553", "(STUBBED) called");
    return ORBIS_OK;
}

/* Before the library's functions are registered (libs.cpp): the GPU and the presenter made, and
 * what the submissions go by */
void gnm_driver_init(void) {
    c_log(C_LOG_LIB_GNM_DRIVER, C_LOG_LEVEL_INFO, "RegisterLib", "Initializing presenter");
    if (!submit_combiner_made) {
        combiner_init(&submit_combiner);
        submit_combiner_made = true;
    }
    gnm_gpu_create();
    if (kernel_get_compiled_sdk_version(&sdk_version) != ORBIS_OK) {
        sdk_version = 0;
    }
    if (emu_settings_get_copy_gpu_buffers()) {
        gnm_gpu_reserve_copy_buffer_space();
    }
    retro_eventcount_init(&idle_ec);
    gnm_gpu_on_idle(reset_submission_lock);
}
