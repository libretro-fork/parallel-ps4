/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef SHADPS4_FILE_FORMAT_RIF_C_H
#define SHADPS4_FILE_FORMAT_RIF_C_H

/* A license (its .idx and .rif files), for code written in C. Defined in rif.cpp. */

#include <stddef.h>

#include <boolean.h>
#include <retro_common_api.h>

#include "common/types.h"

RETRO_BEGIN_DECLS

#define RIF_ENTITLEMENT_KEY_SIZE 16

/* Opens the license at the host paths @ridx_path and @rifa_path (UTF-8): false if it cannot be.
 * Once it is open @opened is called, then each of its license-only entitlements, its label
 * (@length bytes, terminated) and its key, is given to @use. */
bool rif_for_each_license_only_entitlement(
    const char* ridx_path, const char* rifa_path, void (*opened)(void* ctx),
    void (*use)(const char* label, size_t length, const u8* key, void* ctx), void* ctx);

RETRO_END_DECLS

#endif
