/* SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef SHADPS4_CORE_LIBRARIES_FIBER_FIBER_H
#define SHADPS4_CORE_LIBRARIES_FIBER_FIBER_H

/* libSceFiber. Defined in fiber.c, registered in core/libraries/libs.cpp. */

#include <retro_atomic.h>
#include <retro_common_api.h>

#include "common/types.h"

RETRO_BEGIN_DECLS

#define ORBIS_FIBER_MAX_NAME_LENGTH (31)
#define ORBIS_FIBER_CONTEXT_MINIMUM_SIZE (512)

typedef void PS4_SYSV_ABI (*OrbisFiberEntry)(u64 arg_on_initialize, u64 arg_on_run);

/* A fiber's state */
#define ORBIS_FIBER_STATE_RUN 1u
#define ORBIS_FIBER_STATE_IDLE 2u
#define ORBIS_FIBER_STATE_TERMINATED 3u

/* A fiber's flags */
#define ORBIS_FIBER_FLAG_NONE 0x0u
#define ORBIS_FIBER_FLAG_NO_ULOBJMGR 0x1u
#define ORBIS_FIBER_FLAG_CONTEXT_SIZE_CHECK 0x10u
#define ORBIS_FIBER_FLAG_SET_FPU_REGS 0x100u

typedef struct OrbisFiber OrbisFiber;

/* What _sceFiberSetJmp saves (fiber_context.c reads and writes it at these offsets) */
typedef struct OrbisFiberContext {
    u64 rax, rcx, rdx, rbx, rsp, rbp, r8, r9, r10, r11, r12, r13, r14, r15;
    u16 fpucw;
    u32 mxcsr;
    OrbisFiber* current_fiber;
    OrbisFiber* prev_fiber;
    u64 arg_on_run_to;
    u64 arg_on_return;
    u64 return_val;
} OrbisFiberContext;

typedef struct OrbisFiberData {
    OrbisFiberEntry entry;
    u64 arg_on_initialize;
    u64 arg_on_run_to;
    void* stack_addr;
    u32* state;
    u16 fpucw;
    s8 pad[2];
    u32 mxcsr;
} OrbisFiberData;

struct OrbisFiber {
    u32 magic_start;
    retro_atomic_int_t state;
    OrbisFiberEntry entry;
    u64 arg_on_initialize;
    void* addr_context;
    u64 size_context;
    char name[ORBIS_FIBER_MAX_NAME_LENGTH + 1];
    OrbisFiberContext* context;
    u32 flags;
    void* context_start;
    void* context_end;
    u32 magic_end;
};
typedef char orbis_fiber_size_check[sizeof(OrbisFiber) <= 256 ? 1 : -1];

typedef struct OrbisFiberInfo {
    u64 size;
    OrbisFiberEntry entry;
    u64 arg_on_initialize;
    void* addr_context;
    u64 size_context;
    char name[ORBIS_FIBER_MAX_NAME_LENGTH + 1];
    u64 size_context_margin;
    u8 pad[48];
} OrbisFiberInfo;
typedef char orbis_fiber_info_size_check[sizeof(OrbisFiberInfo) == 128 ? 1 : -1];

typedef struct OrbisFiberOptParam {
    u32 magic;
} OrbisFiberOptParam;
typedef char orbis_fiber_opt_param_size_check[sizeof(OrbisFiberOptParam) <= 128 ? 1 : -1];

s32 PS4_SYSV_ABI sceFiberInitialize(OrbisFiber* fiber, const char* name, OrbisFiberEntry entry,
                                    u64 arg_on_initialize, void* addr_context, u64 size_context,
                                    const OrbisFiberOptParam* opt_param, u32 build_version);

/* _sceFiberInitializeWithInternalOptionImpl */
s32 PS4_SYSV_ABI sceFiberInitializeImpl(OrbisFiber* fiber, const char* name, OrbisFiberEntry entry,
                                        u64 arg_on_initialize, void* addr_context, u64 size_context,
                                        const OrbisFiberOptParam* opt_param, u32 flags,
                                        u32 build_ver);

s32 PS4_SYSV_ABI sceFiberOptParamInitialize(OrbisFiberOptParam* opt_param);

s32 PS4_SYSV_ABI sceFiberFinalize(OrbisFiber* fiber);

s32 PS4_SYSV_ABI sceFiberRun(OrbisFiber* fiber, u64 arg_on_run_to, u64* arg_on_return);

s32 PS4_SYSV_ABI sceFiberSwitch(OrbisFiber* fiber, u64 arg_on_run_to, u64* arg_on_run);

/* _sceFiberAttachContextAndRun */
s32 PS4_SYSV_ABI sceFiberRunImpl(OrbisFiber* fiber, void* addr_context, u64 size_context,
                                 u64 arg_on_run_to, u64* arg_on_return);

/* _sceFiberAttachContextAndSwitch */
s32 PS4_SYSV_ABI sceFiberSwitchImpl(OrbisFiber* fiber, void* addr_context, u64 size_context,
                                    u64 arg_on_run_to, u64* arg_on_run);

s32 PS4_SYSV_ABI sceFiberGetSelf(OrbisFiber** fiber);

s32 PS4_SYSV_ABI sceFiberReturnToThread(u64 arg_on_return, u64* arg_on_run);

s32 PS4_SYSV_ABI sceFiberGetInfo(OrbisFiber* fiber, OrbisFiberInfo* fiber_info);

s32 PS4_SYSV_ABI sceFiberStartContextSizeCheck(u32 flags);

s32 PS4_SYSV_ABI sceFiberStopContextSizeCheck(void);

s32 PS4_SYSV_ABI sceFiberRename(OrbisFiber* fiber, const char* name);

s32 PS4_SYSV_ABI sceFiberGetThreadFramePointerAddress(u64* addr_frame_pointer);

#ifdef SHADPS4_ENABLE_FEX_GUEST_CPU
/* Where a fiber's entry returns to through FEX (its veneer made by
 * core_hle_fiber_exit_address): the thread's sceFiberRun call is returned from */
void PS4_SYSV_ABI fiber_entry_returned(void);
#endif

RETRO_END_DECLS

#endif
