/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef SHADPS4_CORE_LINKER_C_H
#define SHADPS4_CORE_LINKER_C_H

/* The linker, for code written in C. Defined in linker.cpp. */

#include <retro_common_api.h>

#include "common/types.h"

RETRO_BEGIN_DECLS

/* Linker::LoadAndStartModule: the module at @path (UTF-8: a host path, or the game's own, as
 * /app0/...), started with @args bytes of arguments at @argp; its start's result in @res. The
 * module's handle, or an error (below 0). */
s32 linker_load_and_start_module(const char* path, u64 args, const void* argp, int* res);

RETRO_END_DECLS

#endif
