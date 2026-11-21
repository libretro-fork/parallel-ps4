/* SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef SHADPS4_LIBRARIES_COMMONDIALOG_C_H
#define SHADPS4_LIBRARIES_COMMONDIALOG_C_H

/* libSceCommonDialog, and what every dialog library shares: the status, result and error
 * values, the base parameter and the two flags. Defined in commondialog.c, registered in
 * libs.cpp. C++ has the same values as enum classes in commondialog.h. */

#include <stddef.h>

#include <boolean.h>
#include <retro_common_api.h>

#include "common/types.h"

RETRO_BEGIN_DECLS

/* CommonDialog::Status */
#define COMMON_DIALOG_STATUS_NONE 0u
#define COMMON_DIALOG_STATUS_INITIALIZED 1u
#define COMMON_DIALOG_STATUS_RUNNING 2u
#define COMMON_DIALOG_STATUS_FINISHED 3u

/* CommonDialog::Result */
#define COMMON_DIALOG_RESULT_OK 0u
#define COMMON_DIALOG_RESULT_USER_CANCELED 1u

/* CommonDialog::Error */
#define COMMON_DIALOG_ERROR_OK 0u
#define COMMON_DIALOG_ERROR_NOT_SYSTEM_INITIALIZED 0x80B80001u
#define COMMON_DIALOG_ERROR_ALREADY_SYSTEM_INITIALIZED 0x80B80002u
#define COMMON_DIALOG_ERROR_NOT_INITIALIZED 0x80B80003u
#define COMMON_DIALOG_ERROR_ALREADY_INITIALIZED 0x80B80004u
#define COMMON_DIALOG_ERROR_NOT_FINISHED 0x80B80005u
#define COMMON_DIALOG_ERROR_INVALID_STATE 0x80B80006u
#define COMMON_DIALOG_ERROR_RESULT_NONE 0x80B80007u
#define COMMON_DIALOG_ERROR_BUSY 0x80B80008u
#define COMMON_DIALOG_ERROR_OUT_OF_MEMORY 0x80B80009u
#define COMMON_DIALOG_ERROR_PARAM_INVALID 0x80B8000Au
#define COMMON_DIALOG_ERROR_NOT_RUNNING 0x80B8000Bu
#define COMMON_DIALOG_ERROR_ALREADY_CLOSE 0x80B8000Cu
#define COMMON_DIALOG_ERROR_ARG_NULL 0x80B8000Du
#define COMMON_DIALOG_ERROR_UNEXPECTED_FATAL 0x80B8000Eu
#define COMMON_DIALOG_ERROR_NOT_SUPPORTED 0x80B8000Fu
#define COMMON_DIALOG_ERROR_INHIBIT_SHAREPLAY_CLIENT 0x80B80010u

/* CommonDialog::BaseParam */
typedef struct common_dialog_base_param {
    size_t size;
    u8 reserved[36];
    u32 magic;
} common_dialog_base_param;

typedef char common_dialog_base_param_size_check[sizeof(common_dialog_base_param) == 48 ? 1 : -1];

/* sceCommonDialogInitialize has been called; a dialog library is initialized (one at a time) */
extern bool g_common_dialog_initialized;
extern bool g_common_dialog_used;

int PS4_SYSV_ABI CommonDialog_ZN3sce16CommonDialogUtil12getSelfAppIdEv(void);
int PS4_SYSV_ABI CommonDialog_ZN3sce16CommonDialogUtil6Client11closeModuleEv(void);
int PS4_SYSV_ABI CommonDialog_ZN3sce16CommonDialogUtil6Client11updateStateEv(void);
int PS4_SYSV_ABI CommonDialog_ZN3sce16CommonDialogUtil6Client15launchCmnDialogEv(void);
int PS4_SYSV_ABI CommonDialog_ZN3sce16CommonDialogUtil6ClientD0Ev(void);
int PS4_SYSV_ABI CommonDialog_ZN3sce16CommonDialogUtil6ClientD1Ev(void);
int PS4_SYSV_ABI CommonDialog_ZN3sce16CommonDialogUtil6ClientD2Ev(void);
int PS4_SYSV_ABI CommonDialog_ZNK3sce16CommonDialogUtil6Client10isCloseReqEv(void);
int PS4_SYSV_ABI CommonDialog_ZNK3sce16CommonDialogUtil6Client13getFinishDataEPvm(void);
int PS4_SYSV_ABI CommonDialog_ZNK3sce16CommonDialogUtil6Client14getClientStateEv(void);
int PS4_SYSV_ABI CommonDialog_ZNK3sce16CommonDialogUtil6Client19isInitializedStatusEv(void);
int PS4_SYSV_ABI CommonDialog_ZNK3sce16CommonDialogUtil6Client8getAppIdEv(void);
int PS4_SYSV_ABI CommonDialog_ZNK3sce16CommonDialogUtil6Client8isFinishEv(void);
int PS4_SYSV_ABI CommonDialog_ZNK3sce16CommonDialogUtil6Client9getResultEv(void);
int PS4_SYSV_ABI CommonDialog_ZTVN3sce16CommonDialogUtil6ClientE(void);
u32 PS4_SYSV_ABI sceCommonDialogInitialize(void);
bool PS4_SYSV_ABI sceCommonDialogIsUsed(void);
int PS4_SYSV_ABI CommonDialog_Func_0FF577E4E8457883(void);
int PS4_SYSV_ABI CommonDialog_Func_41716C2CE379416C(void);
int PS4_SYSV_ABI CommonDialog_Func_483A427D8F6E0748(void);
int PS4_SYSV_ABI CommonDialog_Func_6944B83E02727BDF(void);
int PS4_SYSV_ABI CommonDialog_Func_69F2DD23A8B4950C(void);
int PS4_SYSV_ABI CommonDialog_Func_9954673DEAC170AD(void);
int PS4_SYSV_ABI CommonDialog_Func_A7D4D3AB86CB7455(void);
int PS4_SYSV_ABI CommonDialog_Func_ADE4C51256B8350C(void);
int PS4_SYSV_ABI CommonDialog_Func_B71349CF15FACAB0(void);
int PS4_SYSV_ABI CommonDialog_Func_CB18E00EFA946C64(void);
int PS4_SYSV_ABI CommonDialog_Func_F2AEE270605622B0(void);

RETRO_END_DECLS

#endif
