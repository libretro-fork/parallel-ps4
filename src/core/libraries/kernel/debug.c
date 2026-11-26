/* SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <string.h>

#include "core/libraries/kernel/debug.h"
#include "core/libraries/kernel/kernel_c.h"

void PS4_SYSV_ABI sceKernelDebugOutText(void* unk, char* text) {
    (void)unk;
    kernel_write(1, text, strlen(text));
}
