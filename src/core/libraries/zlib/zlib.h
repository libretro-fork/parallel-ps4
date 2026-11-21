/* SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef SHADPS4_LIBRARIES_ZLIB_H
#define SHADPS4_LIBRARIES_ZLIB_H

/* libSceZlib: whole zlib streams inflated on a thread of the library's, one request at a
 * time, in the order they were asked for. Defined in zlib.c, registered in libs.cpp. */

#include <retro_common_api.h>

#include "common/types.h"

RETRO_BEGIN_DECLS

s32 PS4_SYSV_ABI sceZlibInitialize(const void* buffer, u32 length);
s32 PS4_SYSV_ABI sceZlibInflate(const void* src, u32 src_len, void* dst, u32 dst_len,
                                u64* request_id);
s32 PS4_SYSV_ABI sceZlibWaitForDone(u64* request_id, const u32* timeout);
s32 PS4_SYSV_ABI sceZlibGetResult(u64 request_id, u32* dst_length, s32* status);
s32 PS4_SYSV_ABI sceZlibFinalize(void);

/* Makes the library's state, once, before any call: libs.cpp's registration calls it */
void zlib_library_init(void);

RETRO_END_DECLS

#endif
