/* SPDX-FileCopyrightText: Copyright 2025-2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

/* libSceNpProfileDialog: the overlay's profile card (video_core/overlay/ovl_profile.c) shows
 * it. The status, result and error values are CommonDialog's (system/commondialog_c.h).
 * Registered in libs.cpp. */

#ifndef SHADPS4_LIBRARIES_NP_PROFILE_DIALOG_H
#define SHADPS4_LIBRARIES_NP_PROFILE_DIALOG_H

#include "common/types.h"
#include "core/libraries/np/np_types_c.h"
#include "core/libraries/system/commondialog_c.h"

#ifdef __cplusplus
extern "C" {
#endif

/* OrbisNpProfileDialogMode */
typedef u32 OrbisNpProfileDialogMode;
#define ORBIS_NP_PROFILE_DIALOG_MODE_INVALID 0u
#define ORBIS_NP_PROFILE_DIALOG_MODE_NORMAL 1u
#define ORBIS_NP_PROFILE_DIALOG_MODE_FRIEND_REQUEST 2u
#define ORBIS_NP_PROFILE_DIALOG_MODE_ADD_TO_BLOCK_LIST 3u
#define ORBIS_NP_PROFILE_DIALOG_MODE_GRIEF_REPORT 4u

#define ORBIS_NP_PROFILE_DIALOG_MENU_GRIEF_REPORT_ITEM_INVALID 0x00000000
#define ORBIS_NP_PROFILE_DIALOG_MENU_GRIEF_REPORT_ITEM_ONLINE_ID 0x00000001
#define ORBIS_NP_PROFILE_DIALOG_MENU_GRIEF_REPORT_ITEM_NAME 0x00000002
#define ORBIS_NP_PROFILE_DIALOG_MENU_GRIEF_REPORT_ITEM_PICTURE 0x00000004
#define ORBIS_NP_PROFILE_DIALOG_MENU_GRIEF_REPORT_ITEM_ABOUT_ME 0x00000008

typedef struct OrbisNpProfileDialogParam {
    common_dialog_base_param baseParam;
    u64 size;
    OrbisNpProfileDialogMode mode;
    s32 userId;
    np_online_id_t targetOnlineId;
    void* userData;
    u8 reserved[32];
} OrbisNpProfileDialogParam;

typedef struct OrbisNpProfileGriefReportParam {
    s32 reportItem;
    u8 reserved[28];
} OrbisNpProfileGriefReportParam;

typedef struct OrbisNpProfileDialogParamA {
    common_dialog_base_param baseParam;
    u64 size;
    OrbisNpProfileDialogMode mode;
    s32 userId;
    np_account_id_t targetAccountId;
    u32 padding;
    void* userData;
    union {
        u8 reserved[32];
        OrbisNpProfileGriefReportParam griefReportParam;
    } u;
} OrbisNpProfileDialogParamA;

typedef struct OrbisNpProfileDialogResult {
    s32 result;
    u32 userAction; /* COMMON_DIALOG_RESULT_* */
    void* userData;
    u8 reserved[32];
} OrbisNpProfileDialogResult;

typedef char np_profile_dialog_param_size_check[sizeof(OrbisNpProfileDialogParam) == 128 ? 1 : -1];
typedef char
    np_profile_dialog_param_a_size_check[sizeof(OrbisNpProfileDialogParamA) == 120 ? 1 : -1];
typedef char np_profile_dialog_result_size_check[sizeof(OrbisNpProfileDialogResult) == 48 ? 1 : -1];

u32 PS4_SYSV_ABI sceNpProfileDialogOpen(OrbisNpProfileDialogParam* param);
u32 PS4_SYSV_ABI sceNpProfileDialogClose(void);
u32 PS4_SYSV_ABI sceNpProfileDialogGetResult(OrbisNpProfileDialogResult* result);
u32 PS4_SYSV_ABI sceNpProfileDialogGetStatus(void);
u32 PS4_SYSV_ABI sceNpProfileDialogInitialize(void);
u32 PS4_SYSV_ABI sceNpProfileDialogOpenA(OrbisNpProfileDialogParamA* param);
u32 PS4_SYSV_ABI sceNpProfileDialogTerminate(void);
u32 PS4_SYSV_ABI sceNpProfileDialogUpdateStatus(void);

#ifdef __cplusplus
}
#endif

#endif
