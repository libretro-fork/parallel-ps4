/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <boolean.h>

#include "common/c_bridge.h"
#include "core/libraries/content_export/content_export.h"
#include "core/libraries/content_export/content_export_error.h"
#include "core/libraries/error_codes.h"

static bool g_is_initialized = false;

static s32 content_export_init(OrbisContentExportInitParam* init_param, u8 version) {
    if (g_is_initialized) {
        return ORBIS_CONTENT_EXPORT_ERROR_MULTIPLEINIT;
    }
    if (!init_param || !init_param->mallocfunc || !init_param->freefunc) {
        return ORBIS_CONTENT_EXPORT_ERROR_INVALDPARAM;
    }
    if (version == 1 && (init_param->reserved0 != 0 || init_param->reserved1 != 0 ||
                         (init_param->bufsize != 0 && init_param->bufsize < 0x100))) {
        return ORBIS_CONTENT_EXPORT_ERROR_INVALDPARAM;
    }
    g_is_initialized = true;
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceContentExportInit(OrbisContentExportInitParam* init_param) {
    c_log(C_LOG_LIB_CONTENT_EXPORT, C_LOG_LEVEL_ERROR, "sceContentExportInit", "(STUBBED) called");
    return content_export_init(init_param, 0);
}

s32 PS4_SYSV_ABI sceContentExportInit2(OrbisContentExportInitParam* init_param) {
    c_log(C_LOG_LIB_CONTENT_EXPORT, C_LOG_LEVEL_ERROR, "sceContentExportInit2", "(STUBBED) called");
    return content_export_init(init_param, 1);
}

s32 PS4_SYSV_ABI sceContentExportTerm(void) {
    c_log(C_LOG_LIB_CONTENT_EXPORT, C_LOG_LEVEL_ERROR, "sceContentExportTerm", "(STUBBED) called");
    if (!g_is_initialized) {
        return ORBIS_CONTENT_EXPORT_ERROR_NOINIT;
    }
    g_is_initialized = false;
    return ORBIS_OK;
}
