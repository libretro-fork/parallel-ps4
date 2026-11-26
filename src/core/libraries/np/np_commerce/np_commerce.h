/* SPDX-FileCopyrightText: Copyright 2025-2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

/* libSceNpCommerce: the store's dialog and icon, which the overlay shows
 * (video_core/overlay/ovl_commerce.c). The status and error values are CommonDialog's
 * (system/commondialog_c.h). Registered in libs.cpp. */

#ifndef SHADPS4_LIBRARIES_NP_COMMERCE_H
#define SHADPS4_LIBRARIES_NP_COMMERCE_H

#include "common/types.h"
#include "core/libraries/system/commondialog_c.h"

#ifdef __cplusplus
extern "C" {
#endif

#define NP_COMMERCE_MAX_LABEL_LEN 128
#define ORBIS_NP_COMMERCE_DIALOG_NUM_TARGETS_MAX 10u

typedef struct OrbisNpCommerceDialogParam {
    common_dialog_base_param baseParam;
    s32 size;
    s32 userId;
    s32 mode;
    u32 serviceLabel;
    const char* const* targets;
    u32 numTargets;
    u32 padding;
    u64 features;
    void* userData;
    u8 reserved[32];
} OrbisNpCommerceDialogParam;

typedef char
    np_commerce_dialog_param_size_check[sizeof(OrbisNpCommerceDialogParam) == 0x80 ? 1 : -1];

typedef struct OrbisNpCommerceDialogResult {
    s32 result;
    u8 authorized; /* bool */
    u8 padding[3];
    void* userData;
    u8 reserved[32];
} OrbisNpCommerceDialogResult;

typedef char
    np_commerce_dialog_result_size_check[sizeof(OrbisNpCommerceDialogResult) == 0x30 ? 1 : -1];

/* OrbisNpCommerceDialogResult's result */
#define ORBIS_NP_COMMERCE_DIALOG_RESULT_OK 0
#define ORBIS_NP_COMMERCE_DIALOG_RESULT_USER_CANCELED 1
#define ORBIS_NP_COMMERCE_DIALOG_RESULT_PURCHASED 2

/* OrbisNpCommerceDialogMode */
#define ORBIS_NP_COMMERCE_DIALOG_MODE_CATEGORY 0     /* browse a category */
#define ORBIS_NP_COMMERCE_DIALOG_MODE_PRODUCT 1      /* browse a product */
#define ORBIS_NP_COMMERCE_DIALOG_MODE_PRODUCT_CODE 2 /* a promotion code */
#define ORBIS_NP_COMMERCE_DIALOG_MODE_CHECKOUT 3
#define ORBIS_NP_COMMERCE_DIALOG_MODE_DOWNLOADLIST 4
#define ORBIS_NP_COMMERCE_DIALOG_MODE_PLUS 5 /* PlayStation Plus */

s32 PS4_SYSV_ABI sceNpCommerceDialogClose(void);
s32 PS4_SYSV_ABI sceNpCommerceDialogGetResult(OrbisNpCommerceDialogResult* result);
s8 PS4_SYSV_ABI sceNpCommerceDialogGetStatus(void);
s32 PS4_SYSV_ABI sceNpCommerceDialogInitialize(void);
s32 PS4_SYSV_ABI sceNpCommerceDialogInitializeInternal(void);
s32 PS4_SYSV_ABI sceNpCommerceDialogOpen(const OrbisNpCommerceDialogParam* param);
s32 PS4_SYSV_ABI sceNpCommerceDialogTerminate(void);
s32 PS4_SYSV_ABI sceNpCommerceDialogUpdateStatus(void);
s32 PS4_SYSV_ABI sceNpCommerceHidePsStoreIcon(void);
s32 PS4_SYSV_ABI sceNpCommerceSetPsStoreIconLayout(s32 layout);
s32 PS4_SYSV_ABI sceNpCommerceShowPsStoreIcon(s32 pos);

#ifdef __cplusplus
}
#endif

#endif
