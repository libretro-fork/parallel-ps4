/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef SHADPS4_CORE_GUEST_CPU_HLE_FRAME_C_H
#define SHADPS4_CORE_GUEST_CPU_HLE_FRAME_C_H

/* The guest's registers of an HLE call through FEX, for code written in C. */

#include <retro_common_api.h>

#include "common/types.h"

RETRO_BEGIN_DECLS

/* The 16 general registers (rax, rcx, rdx, rbx, rsp, rbp, rsi, rdi, r8..r15) of the HLE call
 * this thread is in, or NULL outside one: CurrentHleCallFrame()->gpr. A function that
 * switches guest contexts (the fibers) reads and replaces them: the veneer's ret then
 * continues wherever the new registers say. Defined in fex_hle_bridge.cpp. */
u64* core_hle_call_frame_gpr(void);

/* The guest address of the veneer that calls fiber_entry_returned, made on first use and the
 * same after. Defined in core/libraries/libs.cpp. */
u64 core_hle_fiber_exit_address(void);

RETRO_END_DECLS

#endif
