/* SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

/* libkernel's debug text output (debug.c), registered in kernel.cpp */

#ifndef SHADPS4_LIBRARIES_KERNEL_DEBUG_H
#define SHADPS4_LIBRARIES_KERNEL_DEBUG_H

#include "common/types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Writes @text to the game's standard output */
void PS4_SYSV_ABI sceKernelDebugOutText(void* unk, char* text);

#ifdef __cplusplus
}
#endif

#endif
