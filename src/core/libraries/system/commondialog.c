/* SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "common/c_bridge.h"
#include "core/libraries/error_codes.h"
#include "core/libraries/system/commondialog_c.h"

bool g_common_dialog_initialized = false;
bool g_common_dialog_used = false;

int PS4_SYSV_ABI CommonDialog_ZN3sce16CommonDialogUtil12getSelfAppIdEv(void) {
    c_log(C_LOG_LIB_COMMON_DLG, C_LOG_LEVEL_ERROR, "_ZN3sce16CommonDialogUtil12getSelfAppIdEv",
          "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI CommonDialog_ZN3sce16CommonDialogUtil6Client11closeModuleEv(void) {
    c_log(C_LOG_LIB_COMMON_DLG, C_LOG_LEVEL_ERROR,
          "_ZN3sce16CommonDialogUtil6Client11closeModuleEv", "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI CommonDialog_ZN3sce16CommonDialogUtil6Client11updateStateEv(void) {
    c_log(C_LOG_LIB_COMMON_DLG, C_LOG_LEVEL_ERROR,
          "_ZN3sce16CommonDialogUtil6Client11updateStateEv", "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI CommonDialog_ZN3sce16CommonDialogUtil6Client15launchCmnDialogEv(void) {
    c_log(C_LOG_LIB_COMMON_DLG, C_LOG_LEVEL_ERROR,
          "_ZN3sce16CommonDialogUtil6Client15launchCmnDialogEv", "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI CommonDialog_ZN3sce16CommonDialogUtil6ClientD0Ev(void) {
    c_log(C_LOG_LIB_COMMON_DLG, C_LOG_LEVEL_ERROR, "_ZN3sce16CommonDialogUtil6ClientD0Ev",
          "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI CommonDialog_ZN3sce16CommonDialogUtil6ClientD1Ev(void) {
    c_log(C_LOG_LIB_COMMON_DLG, C_LOG_LEVEL_ERROR, "_ZN3sce16CommonDialogUtil6ClientD1Ev",
          "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI CommonDialog_ZN3sce16CommonDialogUtil6ClientD2Ev(void) {
    c_log(C_LOG_LIB_COMMON_DLG, C_LOG_LEVEL_ERROR, "_ZN3sce16CommonDialogUtil6ClientD2Ev",
          "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI CommonDialog_ZNK3sce16CommonDialogUtil6Client10isCloseReqEv(void) {
    c_log(C_LOG_LIB_COMMON_DLG, C_LOG_LEVEL_ERROR,
          "_ZNK3sce16CommonDialogUtil6Client10isCloseReqEv", "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI CommonDialog_ZNK3sce16CommonDialogUtil6Client13getFinishDataEPvm(void) {
    c_log(C_LOG_LIB_COMMON_DLG, C_LOG_LEVEL_ERROR,
          "_ZNK3sce16CommonDialogUtil6Client13getFinishDataEPvm", "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI CommonDialog_ZNK3sce16CommonDialogUtil6Client14getClientStateEv(void) {
    c_log(C_LOG_LIB_COMMON_DLG, C_LOG_LEVEL_ERROR,
          "_ZNK3sce16CommonDialogUtil6Client14getClientStateEv", "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI CommonDialog_ZNK3sce16CommonDialogUtil6Client19isInitializedStatusEv(void) {
    c_log(C_LOG_LIB_COMMON_DLG, C_LOG_LEVEL_ERROR,
          "_ZNK3sce16CommonDialogUtil6Client19isInitializedStatusEv", "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI CommonDialog_ZNK3sce16CommonDialogUtil6Client8getAppIdEv(void) {
    c_log(C_LOG_LIB_COMMON_DLG, C_LOG_LEVEL_ERROR, "_ZNK3sce16CommonDialogUtil6Client8getAppIdEv",
          "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI CommonDialog_ZNK3sce16CommonDialogUtil6Client8isFinishEv(void) {
    c_log(C_LOG_LIB_COMMON_DLG, C_LOG_LEVEL_ERROR, "_ZNK3sce16CommonDialogUtil6Client8isFinishEv",
          "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI CommonDialog_ZNK3sce16CommonDialogUtil6Client9getResultEv(void) {
    c_log(C_LOG_LIB_COMMON_DLG, C_LOG_LEVEL_ERROR, "_ZNK3sce16CommonDialogUtil6Client9getResultEv",
          "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI CommonDialog_ZTVN3sce16CommonDialogUtil6ClientE(void) {
    c_log(C_LOG_LIB_COMMON_DLG, C_LOG_LEVEL_ERROR, "_ZTVN3sce16CommonDialogUtil6ClientE",
          "(STUBBED) called");
    return ORBIS_OK;
}

u32 PS4_SYSV_ABI sceCommonDialogInitialize(void) {
    if (g_common_dialog_initialized) {
        c_log(C_LOG_LIB_COMMON_DLG, C_LOG_LEVEL_INFO, "sceCommonDialogInitialize",
              "already initialized");
        return COMMON_DIALOG_ERROR_ALREADY_SYSTEM_INITIALIZED;
    }
    c_log(C_LOG_LIB_COMMON_DLG, C_LOG_LEVEL_DEBUG, "sceCommonDialogInitialize", "initialized");
    g_common_dialog_initialized = true;
    return COMMON_DIALOG_ERROR_OK;
}

bool PS4_SYSV_ABI sceCommonDialogIsUsed(void) {
    c_log(C_LOG_LIB_COMMON_DLG, C_LOG_LEVEL_TRACE, "sceCommonDialogIsUsed", "called");
    return g_common_dialog_used;
}

int PS4_SYSV_ABI CommonDialog_Func_0FF577E4E8457883(void) {
    c_log(C_LOG_LIB_COMMON_DLG, C_LOG_LEVEL_ERROR, "Func_0FF577E4E8457883", "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI CommonDialog_Func_41716C2CE379416C(void) {
    c_log(C_LOG_LIB_COMMON_DLG, C_LOG_LEVEL_ERROR, "Func_41716C2CE379416C", "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI CommonDialog_Func_483A427D8F6E0748(void) {
    c_log(C_LOG_LIB_COMMON_DLG, C_LOG_LEVEL_ERROR, "Func_483A427D8F6E0748", "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI CommonDialog_Func_6944B83E02727BDF(void) {
    c_log(C_LOG_LIB_COMMON_DLG, C_LOG_LEVEL_ERROR, "Func_6944B83E02727BDF", "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI CommonDialog_Func_69F2DD23A8B4950C(void) {
    c_log(C_LOG_LIB_COMMON_DLG, C_LOG_LEVEL_ERROR, "Func_69F2DD23A8B4950C", "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI CommonDialog_Func_9954673DEAC170AD(void) {
    c_log(C_LOG_LIB_COMMON_DLG, C_LOG_LEVEL_ERROR, "Func_9954673DEAC170AD", "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI CommonDialog_Func_A7D4D3AB86CB7455(void) {
    c_log(C_LOG_LIB_COMMON_DLG, C_LOG_LEVEL_ERROR, "Func_A7D4D3AB86CB7455", "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI CommonDialog_Func_ADE4C51256B8350C(void) {
    c_log(C_LOG_LIB_COMMON_DLG, C_LOG_LEVEL_ERROR, "Func_ADE4C51256B8350C", "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI CommonDialog_Func_B71349CF15FACAB0(void) {
    c_log(C_LOG_LIB_COMMON_DLG, C_LOG_LEVEL_ERROR, "Func_B71349CF15FACAB0", "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI CommonDialog_Func_CB18E00EFA946C64(void) {
    c_log(C_LOG_LIB_COMMON_DLG, C_LOG_LEVEL_ERROR, "Func_CB18E00EFA946C64", "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI CommonDialog_Func_F2AEE270605622B0(void) {
    c_log(C_LOG_LIB_COMMON_DLG, C_LOG_LEVEL_ERROR, "Func_F2AEE270605622B0", "(STUBBED) called");
    return ORBIS_OK;
}
