/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef SHADPS4_CORE_TLS_C_H
#define SHADPS4_CORE_TLS_C_H

/* The guest thread's TCB, for code written in C. Defined in tls.cpp. */

#include <retro_common_api.h>

RETRO_BEGIN_DECLS

struct OrbisFiberContext;

/* Where the calling thread's TCB keeps its fiber context (Tcb::tcb_fiber) */
struct OrbisFiberContext** core_tcb_fiber(void);

RETRO_END_DECLS

#endif
