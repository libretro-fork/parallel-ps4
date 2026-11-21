/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef SHADPS4_LIBC_INTERNAL_THREADS_H
#define SHADPS4_LIBC_INTERNAL_THREADS_H

/* libSceLibcInternal's mutexes (_Mtxinit and the rest): the game's recursive pthread mutexes,
 * returning 1 on any failure. Defined in libc_internal_threads.c, registered in
 * libs.cpp. */

#include <retro_common_api.h>

#include "common/types.h"
#include "core/libraries/kernel/kernel_c.h"

RETRO_BEGIN_DECLS

s32 PS4_SYSV_ABI internal__Mtxinit(kernel_pthread_mutex_t* mtx, const char* name);
s32 PS4_SYSV_ABI internal__Mtxlock(kernel_pthread_mutex_t* mtx);
s32 PS4_SYSV_ABI internal__Mtxunlock(kernel_pthread_mutex_t* mtx);
s32 PS4_SYSV_ABI internal__Mtxdst(kernel_pthread_mutex_t* mtx);

RETRO_END_DECLS

#endif
