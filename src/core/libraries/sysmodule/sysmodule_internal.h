/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef SHADPS4_LIBRARIES_SYSMODULE_INTERNAL_H
#define SHADPS4_LIBRARIES_SYSMODULE_INTERNAL_H

/* What libSceSysmodule does under its calls: the module table (sysmodule_table.h) and the
 * loading of a module and its dependencies. Defined in sysmodule_internal.c; the HLE libraries
 * a system module can fall back on, and their registrations, are in sysmodule_internal.cpp. */

#include <boolean.h>
#include <retro_common_api.h>

#include "common/types.h"

RETRO_BEGIN_DECLS

/* The game used libc (Need_sceLibc) or libSceFios2 (sceFiosInitialize): the linker sets them.
 * A module of the preload that cannot be loaded is then fatal. */
extern bool sysmodule_need_scelibc, sysmodule_need_fios2;

s32 sysmodule_get_module_handle(s32 id, s32* handle);
bool sysmodule_should_hide_name(const char* module_name);
bool sysmodule_is_debug_module(s32 id);
u32 sysmodule_validate_module_id(s32 id);
s32 sysmodule_load_module_internal(s32 index, s32 argc, const void* argv, s32* res_out);
s32 sysmodule_load_module(s32 id, s32 argc, const void* argv, s32* res_out);
s32 sysmodule_unload_module(s32 id, s32 argc, const void* argv, s32* res_out, bool is_internal);
s32 sysmodule_preload_modules_for_libkernel(void);

/* The system modules that may be LLE: the index of @mod_name (with its .sprx) among them, -1
 * for one that is not; whether it has an HLE to fall back on in @has_hle
 * (sysmodule_internal.cpp) */
s32 sysmodule_hle_find(const char* mod_name, bool* has_hle);
/* Registers the HLE of the module at @index, which has one, and relocates every module's
 * imports so the modules loaded later see it (sysmodule_internal.cpp) */
void sysmodule_hle_register(s32 index);

RETRO_END_DECLS

#endif
