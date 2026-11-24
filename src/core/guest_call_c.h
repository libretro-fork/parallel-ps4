/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef SHADPS4_CORE_GUEST_CALL_C_H
#define SHADPS4_CORE_GUEST_CALL_C_H

/* Core::CallGuest for the code written in C. Defined in core/libraries/libs.cpp. */

#include <retro_common_api.h>

#include "common/types.h"

RETRO_BEGIN_DECLS

/* Calls the game's function at @function (a callback it registered) with up to four integer or
 * pointer arguments, those it does not take ignored, as Core::CallGuest does: through FEX for
 * the game's code on the aarch64 core, directly elsewhere. Its result register: an int result
 * is the low 32 bits. */
u64 core_call_guest4(const void* function, u64 a0, u64 a1, u64 a2, u64 a3);

RETRO_END_DECLS

#endif
