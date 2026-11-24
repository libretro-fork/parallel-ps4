/* SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef SHADPS4_LIBRARIES_APP_CONTENT_H
#define SHADPS4_LIBRARIES_APP_CONTENT_H

/* libSceAppContent: the game's additional content (from the add-on folders, the PKGs beside the
 * game's and its licenses), its param.sfo values and its temporary data. Defined in
 * app_content.c, registered in libs.cpp. */

#include <retro_common_api.h>

#include "common/types.h"

RETRO_BEGIN_DECLS

#define ORBIS_APP_CONTENT_APPPARAM_ID_SKU_FLAG 0
#define ORBIS_APP_CONTENT_APPPARAM_ID_USER_DEFINED_PARAM_1 1
#define ORBIS_APP_CONTENT_APPPARAM_ID_USER_DEFINED_PARAM_2 2
#define ORBIS_APP_CONTENT_APPPARAM_ID_USER_DEFINED_PARAM_3 3
#define ORBIS_APP_CONTENT_APPPARAM_ID_USER_DEFINED_PARAM_4 4

#define ORBIS_APP_CONTENT_APPPARAM_SKU_FLAG_TRIAL 1
#define ORBIS_APP_CONTENT_APPPARAM_SKU_FLAG_FULL 3

typedef struct OrbisAppContentInitParam {
    char reserved[32];
} OrbisAppContentInitParam;

typedef struct OrbisAppContentBootParam {
    char reserved1[4];
    u32 attr;
    char reserved2[32];
} OrbisAppContentBootParam;

typedef u32 OrbisAppContentTemporaryDataOption;

#define ORBIS_APP_CONTENT_MOUNTPOINT_DATA_MAXSIZE 16

typedef struct OrbisAppContentMountPoint {
    char data[ORBIS_APP_CONTENT_MOUNTPOINT_DATA_MAXSIZE];
} OrbisAppContentMountPoint;

#define ORBIS_APP_CONTENT_TEMPORARY_DATA_OPTION_NONE 0
#define ORBIS_APP_CONTENT_TEMPORARY_DATA_OPTION_FORMAT (1 << 0)
#define ORBIS_NP_UNIFIED_ENTITLEMENT_LABEL_SIZE 17
#define ORBIS_APP_CONTENT_ENTITLEMENT_KEY_SIZE 16
#define ORBIS_APP_CONTENT_ENTITLEMENT_LABEL_OFFSET 20
#define ORBIS_APP_CONTENT_INFO_LIST_MAX_SIZE 2500

/* OrbisAppContentAddcontDownloadStatus: a u32 */
#define ORBIS_APP_CONTENT_ADDCONT_DOWNLOAD_STATUS_NO_EXTRA_DATA 0u
#define ORBIS_APP_CONTENT_ADDCONT_DOWNLOAD_STATUS_NO_IN_QUEUE 1u
#define ORBIS_APP_CONTENT_ADDCONT_DOWNLOAD_STATUS_DOWNLOADING 2u
#define ORBIS_APP_CONTENT_ADDCONT_DOWNLOAD_STATUS_DOWNLOAD_SUSPENDED 3u
#define ORBIS_APP_CONTENT_ADDCONT_DOWNLOAD_STATUS_INSTALLED 4u

typedef struct OrbisNpUnifiedEntitlementLabel {
    char data[ORBIS_NP_UNIFIED_ENTITLEMENT_LABEL_SIZE];
    char padding[3];
} OrbisNpUnifiedEntitlementLabel;

typedef u32 OrbisAppContentAppParamId;

typedef struct OrbisAppContentAddcontInfo {
    OrbisNpUnifiedEntitlementLabel entitlement_label;
    u32 status; /* ORBIS_APP_CONTENT_ADDCONT_DOWNLOAD_STATUS_* */
} OrbisAppContentAddcontInfo;

typedef struct OrbisAppContentGetEntitlementKey {
    char data[ORBIS_APP_CONTENT_ENTITLEMENT_KEY_SIZE];
} OrbisAppContentGetEntitlementKey;

int PS4_SYSV_ABI AppContent_Z5dummyv(void);
int PS4_SYSV_ABI sceAppContentAddcontDelete(void);
int PS4_SYSV_ABI sceAppContentAddcontEnqueueDownload(void);
int PS4_SYSV_ABI sceAppContentAddcontEnqueueDownloadSp(void);
int PS4_SYSV_ABI sceAppContentAddcontMount(u32 service_label,
                                           const OrbisNpUnifiedEntitlementLabel* entitlement_label,
                                           OrbisAppContentMountPoint* mount_point);
int PS4_SYSV_ABI sceAppContentAddcontShrink(void);
int PS4_SYSV_ABI sceAppContentAddcontUnmount(OrbisAppContentMountPoint* mount_point);
int PS4_SYSV_ABI sceAppContentAppParamGetInt(OrbisAppContentAppParamId paramId, s32* value);
int PS4_SYSV_ABI sceAppContentAppParamGetString(void);
int PS4_SYSV_ABI sceAppContentDownload0Expand(void);
int PS4_SYSV_ABI sceAppContentDownload0Shrink(void);
int PS4_SYSV_ABI sceAppContentDownload1Expand(void);
int PS4_SYSV_ABI sceAppContentDownload1Shrink(void);
int PS4_SYSV_ABI sceAppContentDownloadDataFormat(void);
int PS4_SYSV_ABI sceAppContentDownloadDataGetAvailableSpaceKb(OrbisAppContentMountPoint* mountPoint,
                                                              u64* availableSpaceKb);
int PS4_SYSV_ABI sceAppContentGetAddcontDownloadProgress(void);
int PS4_SYSV_ABI sceAppContentGetAddcontInfo(u32 service_label,
                                             const OrbisNpUnifiedEntitlementLabel* entitlementLabel,
                                             OrbisAppContentAddcontInfo* info);
int PS4_SYSV_ABI sceAppContentGetAddcontInfoList(u32 service_label,
                                                 OrbisAppContentAddcontInfo* list, u32 list_num,
                                                 u32* hit_num);
int PS4_SYSV_ABI sceAppContentGetEntitlementKey(
    u32 service_label, const OrbisNpUnifiedEntitlementLabel* entitlement_label,
    OrbisAppContentGetEntitlementKey* key);
int PS4_SYSV_ABI sceAppContentGetRegion(void);
int PS4_SYSV_ABI sceAppContentInitialize(const OrbisAppContentInitParam* initParam,
                                         OrbisAppContentBootParam* bootParam);
int PS4_SYSV_ABI sceAppContentRequestPatchInstall(void);
int PS4_SYSV_ABI sceAppContentSmallSharedDataFormat(void);
int PS4_SYSV_ABI sceAppContentSmallSharedDataGetAvailableSpaceKb(void);
int PS4_SYSV_ABI sceAppContentSmallSharedDataMount(void);
int PS4_SYSV_ABI sceAppContentSmallSharedDataUnmount(void);
int PS4_SYSV_ABI sceAppContentTemporaryDataFormat(void);
int PS4_SYSV_ABI sceAppContentTemporaryDataGetAvailableSpaceKb(
    const OrbisAppContentMountPoint* mountPoint, u64* availableSpaceKb);
int PS4_SYSV_ABI sceAppContentTemporaryDataMount(OrbisAppContentMountPoint* mountPoint);
int PS4_SYSV_ABI sceAppContentTemporaryDataMount2(OrbisAppContentTemporaryDataOption option,
                                                  OrbisAppContentMountPoint* mountPoint);
int PS4_SYSV_ABI sceAppContentTemporaryDataUnmount(const OrbisAppContentMountPoint* mountPoint);
int PS4_SYSV_ABI sceAppContentGetPftFlag(void);
int PS4_SYSV_ABI AppContent_Func_C59A36FF8D7C59DA(void);
int PS4_SYSV_ABI sceAppContentAddcontEnqueueDownloadByEntitlementId(void);
int PS4_SYSV_ABI sceAppContentAddcontMountByEntitlementId(void);
int PS4_SYSV_ABI sceAppContentGetAddcontInfoByEntitlementId(void);
int PS4_SYSV_ABI sceAppContentGetAddcontInfoListByIroTag(void);
int PS4_SYSV_ABI sceAppContentGetDownloadedStoreCountry(void);

/* Reads the game's SDK version, once, before any call: libs.cpp's registration calls it */
void app_content_library_init(void);

RETRO_END_DECLS

#endif
