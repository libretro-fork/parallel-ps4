/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef SHADPS4_LIBRARIES_CONTENT_EXPORT_H
#define SHADPS4_LIBRARIES_CONTENT_EXPORT_H

/* libSceContentExport. Registered in libs.cpp. */

#include <retro_common_api.h>

#include "common/types.h"

RETRO_BEGIN_DECLS

typedef void*(PS4_SYSV_ABI* OrbisContentExportMalloc)(u64 datasize, void* userdata);
typedef void(PS4_SYSV_ABI* OrbisContentExportFree)(void* p, void* userdata);
typedef s32(PS4_SYSV_ABI* OrbisContentExportDataProvideFunction)(void** data, u64* datasize,
                                                                 void* userdata);

typedef struct OrbisContentExportInitParam {
    OrbisContentExportMalloc mallocfunc;
    OrbisContentExportFree freefunc;
    void* userdata;
    u64 bufsize;
    s64 reserved0;
    s64 reserved1;
} OrbisContentExportInitParam;

typedef struct OrbisContentExportParam {
    char title[257];
    char comment[257];
    char contenttype[65];
} OrbisContentExportParam;

typedef struct OrbisContentExportCallbackParam {
    u64 contentlength;
    OrbisContentExportDataProvideFunction func;
    void* userdata;
} OrbisContentExportCallbackParam;

s32 PS4_SYSV_ABI sceContentExportInit(OrbisContentExportInitParam* init_param);
s32 PS4_SYSV_ABI sceContentExportInit2(OrbisContentExportInitParam* init_param);
s32 PS4_SYSV_ABI sceContentExportTerm(void);

RETRO_END_DECLS

#endif
