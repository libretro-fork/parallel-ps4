/* SPDX-FileCopyrightText: Copyright 2024-2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef SHADPS4_LIBRARIES_SAVE_DATA_SAVEDATA_H
#define SHADPS4_LIBRARIES_SAVE_DATA_SAVEDATA_H

/* libSceSaveData: the game's saves, each a folder mounted at /savedataN, and the save data
 * memory. Defined in savedata.c, registered in libs.cpp. */

#include <stddef.h>

#include <boolean.h>
#include <retro_common_api.h>

#include "common/types.h"

RETRO_BEGIN_DECLS

#define SAVE_DATA_TITLE_MAXSIZE 128
#define SAVE_DATA_SUBTITLE_MAXSIZE 128
#define SAVE_DATA_DETAIL_MAXSIZE 1024
/* A title ID: 4 uppercase letters and 5 digits */
#define SAVE_DATA_TITLE_ID_DATA_SIZE 10
#define SAVE_DATA_DIRNAME_DATA_MAXSIZE 32
/* "/savedataN" */
#define SAVE_DATA_MOUNT_POINT_DATA_MAXSIZE 16
#define SAVE_DATA_FINGERPRINT_DATA_SIZE 65

#define SAVE_DATA_BLOCK_SIZE 32768u /* 32 KiB */
#define SAVE_DATA_BLOCKS_MIN2 96u   /* 3 MiB */
#define SAVE_DATA_BLOCKS_MAX 32768u /* 1 GiB */

/* What the calls return */
#define SAVE_DATA_OK 0u
#define SAVE_DATA_ERROR_USER_SERVICE_NOT_INITIALIZED 0x80960002u
#define SAVE_DATA_ERROR_PARAMETER 0x809F0000u
#define SAVE_DATA_ERROR_NOT_INITIALIZED 0x809F0001u
#define SAVE_DATA_ERROR_OUT_OF_MEMORY 0x809F0002u
#define SAVE_DATA_ERROR_BUSY 0x809F0003u
#define SAVE_DATA_ERROR_NOT_MOUNTED 0x809F0004u
#define SAVE_DATA_ERROR_EXISTS 0x809F0007u
#define SAVE_DATA_ERROR_NOT_FOUND 0x809F0008u
#define SAVE_DATA_ERROR_NO_SPACE_FS 0x809F000Au
#define SAVE_DATA_ERROR_INTERNAL 0x809F000Bu
#define SAVE_DATA_ERROR_MOUNT_FULL 0x809F000Cu
#define SAVE_DATA_ERROR_BAD_MOUNTED 0x809F000Du
#define SAVE_DATA_ERROR_BROKEN 0x809F000Fu
#define SAVE_DATA_ERROR_INVALID_LOGIN_USER 0x809F0011u
#define SAVE_DATA_ERROR_MEMORY_NOT_READY 0x809F0012u
#define SAVE_DATA_ERROR_BACKUP_BUSY 0x809F0013u
#define SAVE_DATA_ERROR_BUSY_FOR_SAVING 0x809F0016u

typedef enum OrbisSaveDataParamType {
    ORBIS_SAVE_DATA_PARAM_TYPE_ALL = 0,
    ORBIS_SAVE_DATA_PARAM_TYPE_TITLE = 1,
    ORBIS_SAVE_DATA_PARAM_TYPE_SUB_TITLE = 2,
    ORBIS_SAVE_DATA_PARAM_TYPE_DETAIL = 3,
    ORBIS_SAVE_DATA_PARAM_TYPE_USER_PARAM = 4,
    ORBIS_SAVE_DATA_PARAM_TYPE_MTIME = 5
} OrbisSaveDataParamType;

typedef struct OrbisSaveDataTitleId {
    char data[SAVE_DATA_TITLE_ID_DATA_SIZE];
    u8 _pad[6];
} OrbisSaveDataTitleId;

typedef struct OrbisSaveDataDirName {
    char data[SAVE_DATA_DIRNAME_DATA_MAXSIZE];
} OrbisSaveDataDirName;

typedef struct OrbisSaveDataParam {
    char title[SAVE_DATA_TITLE_MAXSIZE];
    char subTitle[SAVE_DATA_SUBTITLE_MAXSIZE];
    char detail[SAVE_DATA_DETAIL_MAXSIZE];
    u32 userParam;
    u8 _pad[4];
    s64 mtime; /* the game's time_t */
    u8 _reserved[32];
} OrbisSaveDataParam;

struct OrbisSaveDataBackup;
struct OrbisSaveDataCheckBackupData;
struct OrbisSaveDataDelete;
struct OrbisSaveDataDirNameSearchCond;
struct OrbisSaveDataDirNameSearchResult;
struct OrbisSaveDataEvent;
struct OrbisSaveDataIcon;
struct OrbisSaveDataMemoryGet2;
struct OrbisSaveDataMemorySet2;
struct OrbisSaveDataMemorySetup2;
struct OrbisSaveDataMemorySetupResult;
struct OrbisSaveDataMemorySync;
struct OrbisSaveDataMount2;
struct OrbisSaveDataMount;
struct OrbisSaveDataMountInfo;
struct OrbisSaveDataMountPoint;
struct OrbisSaveDataMountResult;
struct OrbisSaveDataRestoreBackupData;
struct OrbisSaveDataTransferringMount;

int PS4_SYSV_ABI sceSaveDataAbort(void);
u32 PS4_SYSV_ABI sceSaveDataBackup(const struct OrbisSaveDataBackup* backup);
int PS4_SYSV_ABI sceSaveDataBindPsnAccount(void);
int PS4_SYSV_ABI sceSaveDataBindPsnAccountForSystemBackup(void);
int PS4_SYSV_ABI sceSaveDataChangeDatabase(void);
int PS4_SYSV_ABI sceSaveDataChangeInternal(void);
u32 PS4_SYSV_ABI sceSaveDataCheckBackupData(const struct OrbisSaveDataCheckBackupData* check);
int PS4_SYSV_ABI sceSaveDataCheckBackupDataForCdlg(void);
int PS4_SYSV_ABI sceSaveDataCheckBackupDataInternal(void);
int PS4_SYSV_ABI sceSaveDataCheckCloudData(void);
int PS4_SYSV_ABI sceSaveDataCheckIpmiIfSize(void);
int PS4_SYSV_ABI sceSaveDataCheckSaveDataBroken(void);
int PS4_SYSV_ABI sceSaveDataCheckSaveDataVersion(void);
int PS4_SYSV_ABI sceSaveDataCheckSaveDataVersionLatest(void);
u32 PS4_SYSV_ABI sceSaveDataClearProgress(void);
int PS4_SYSV_ABI sceSaveDataCopy5(void);
int PS4_SYSV_ABI sceSaveDataCreateUploadData(void);
int PS4_SYSV_ABI sceSaveDataDebug(void);
int PS4_SYSV_ABI sceSaveDataDebugCleanMount(void);
int PS4_SYSV_ABI sceSaveDataDebugCompiledSdkVersion(void);
int PS4_SYSV_ABI sceSaveDataDebugCreateSaveDataRoot(void);
int PS4_SYSV_ABI sceSaveDataDebugGetThreadId(void);
int PS4_SYSV_ABI sceSaveDataDebugRemoveSaveDataRoot(void);
int PS4_SYSV_ABI sceSaveDataDebugTarget(void);
u32 PS4_SYSV_ABI sceSaveDataDelete(const struct OrbisSaveDataDelete* del);
int PS4_SYSV_ABI sceSaveDataDelete5(void);
int PS4_SYSV_ABI sceSaveDataDeleteAllUser(void);
int PS4_SYSV_ABI sceSaveDataDeleteCloudData(void);
int PS4_SYSV_ABI sceSaveDataDeleteUser(void);
u32 PS4_SYSV_ABI sceSaveDataDirNameSearch(const struct OrbisSaveDataDirNameSearchCond* cond,
                                          struct OrbisSaveDataDirNameSearchResult* result);
int PS4_SYSV_ABI sceSaveDataDirNameSearchInternal(void);
int PS4_SYSV_ABI sceSaveDataDownload(void);
int PS4_SYSV_ABI sceSaveDataGetAllSize(void);
int PS4_SYSV_ABI sceSaveDataGetAppLaunchedUser(void);
int PS4_SYSV_ABI sceSaveDataGetAutoUploadConditions(void);
int PS4_SYSV_ABI sceSaveDataGetAutoUploadRequestInfo(void);
int PS4_SYSV_ABI sceSaveDataGetAutoUploadSetting(void);
int PS4_SYSV_ABI sceSaveDataGetBoundPsnAccountCount(void);
int PS4_SYSV_ABI sceSaveDataGetClientThreadPriority(void);
int PS4_SYSV_ABI sceSaveDataGetCloudQuotaInfo(void);
int PS4_SYSV_ABI sceSaveDataGetDataBaseFilePath(void);
int PS4_SYSV_ABI sceSaveDataGetEventInfo(void);
u32 PS4_SYSV_ABI sceSaveDataGetEventResult(const void* eventParam,
                                           struct OrbisSaveDataEvent* event);
int PS4_SYSV_ABI sceSaveDataGetFormat(void);
int PS4_SYSV_ABI sceSaveDataGetMountedSaveDataCount(void);
u32 PS4_SYSV_ABI sceSaveDataGetMountInfo(const struct OrbisSaveDataMountPoint* mountPoint,
                                         struct OrbisSaveDataMountInfo* info);
u32 PS4_SYSV_ABI sceSaveDataGetParam(const struct OrbisSaveDataMountPoint* mountPoint,
                                     u32 paramType, void* paramBuf, size_t paramBufSize,
                                     size_t* gotSize);
u32 PS4_SYSV_ABI sceSaveDataGetProgress(float* progress);
int PS4_SYSV_ABI sceSaveDataGetSaveDataCount(void);
u32 PS4_SYSV_ABI sceSaveDataGetSaveDataMemory(s32 userId, void* buf, size_t bufSize, s64 offset);
u32 PS4_SYSV_ABI sceSaveDataGetSaveDataMemory2(struct OrbisSaveDataMemoryGet2* getParam);
int PS4_SYSV_ABI sceSaveDataGetSaveDataRootDir(void);
int PS4_SYSV_ABI sceSaveDataGetSaveDataRootPath(void);
int PS4_SYSV_ABI sceSaveDataGetSaveDataRootUsbPath(void);
int PS4_SYSV_ABI sceSaveDataGetSavePoint(void);
int PS4_SYSV_ABI sceSaveDataGetUpdatedDataCount(void);
u32 PS4_SYSV_ABI sceSaveDataInitialize(void* init);
u32 PS4_SYSV_ABI sceSaveDataInitialize2(void* init);
u32 PS4_SYSV_ABI sceSaveDataInitialize3(void* init);
int PS4_SYSV_ABI sceSaveDataInitializeForCdlg(void);
int PS4_SYSV_ABI sceSaveDataIsDeletingUsbDb(void);
int PS4_SYSV_ABI sceSaveDataIsMounted(void);
u32 PS4_SYSV_ABI sceSaveDataLoadIcon(const struct OrbisSaveDataMountPoint* mountPoint,
                                     struct OrbisSaveDataIcon* icon);
u32 PS4_SYSV_ABI sceSaveDataMount(const struct OrbisSaveDataMount* mount,
                                  struct OrbisSaveDataMountResult* mount_result);
u32 PS4_SYSV_ABI sceSaveDataMount2(const struct OrbisSaveDataMount2* mount,
                                   struct OrbisSaveDataMountResult* mount_result);
int PS4_SYSV_ABI sceSaveDataMount5(void);
int PS4_SYSV_ABI sceSaveDataMountInternal(void);
int PS4_SYSV_ABI sceSaveDataMountSys(void);
int PS4_SYSV_ABI sceSaveDataPromote5(void);
int PS4_SYSV_ABI sceSaveDataRebuildDatabase(void);
int PS4_SYSV_ABI sceSaveDataRegisterEventCallback(void);
u32 PS4_SYSV_ABI sceSaveDataRestoreBackupData(const struct OrbisSaveDataRestoreBackupData* restore);
int PS4_SYSV_ABI sceSaveDataRestoreBackupDataForCdlg(void);
int PS4_SYSV_ABI sceSaveDataRestoreLoadSaveDataMemory(void);
u32 PS4_SYSV_ABI sceSaveDataSaveIcon(const struct OrbisSaveDataMountPoint* mountPoint,
                                     const struct OrbisSaveDataIcon* icon);
int PS4_SYSV_ABI sceSaveDataSetAutoUploadSetting(void);
int PS4_SYSV_ABI sceSaveDataSetEventInfo(void);
u32 PS4_SYSV_ABI sceSaveDataSetParam(const struct OrbisSaveDataMountPoint* mountPoint,
                                     u32 paramType, const void* paramBuf, size_t paramBufSize);
int PS4_SYSV_ABI sceSaveDataSetSaveDataLibraryUser(void);
u32 PS4_SYSV_ABI sceSaveDataSetSaveDataMemory(s32 userId, void* buf, size_t bufSize, s64 offset);
u32 PS4_SYSV_ABI sceSaveDataSetSaveDataMemory2(const struct OrbisSaveDataMemorySet2* setParam);
u32 PS4_SYSV_ABI sceSaveDataSetupSaveDataMemory(s32 userId, size_t memorySize,
                                                OrbisSaveDataParam* param);
u32 PS4_SYSV_ABI sceSaveDataSetupSaveDataMemory2(const struct OrbisSaveDataMemorySetup2* setupParam,
                                                 struct OrbisSaveDataMemorySetupResult* result);
int PS4_SYSV_ABI sceSaveDataShutdownStart(void);
int PS4_SYSV_ABI sceSaveDataSupportedFakeBrokenStatus(void);
int PS4_SYSV_ABI sceSaveDataSyncCloudList(void);
u32 PS4_SYSV_ABI sceSaveDataSyncSaveDataMemory(struct OrbisSaveDataMemorySync* syncParam);
u32 PS4_SYSV_ABI sceSaveDataTerminate(void);
u32 PS4_SYSV_ABI sceSaveDataTransferringMount(const struct OrbisSaveDataTransferringMount* mount,
                                              struct OrbisSaveDataMountResult* mountResult);
u32 PS4_SYSV_ABI sceSaveDataUmount(const struct OrbisSaveDataMountPoint* mountPoint);
int PS4_SYSV_ABI sceSaveDataUmountSys(void);
u32 PS4_SYSV_ABI sceSaveDataUmountWithBackup(const struct OrbisSaveDataMountPoint* mountPoint);
int PS4_SYSV_ABI sceSaveDataUnregisterEventCallback(void);
int PS4_SYSV_ABI sceSaveDataUpload(void);
int PS4_SYSV_ABI SaveData_Func_02E4C4D201716422(void);

RETRO_END_DECLS

#endif
