/* SPDX-FileCopyrightText: Copyright 2024-2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

/* libSceSaveData: the game's saves, mounted at /savedata0 to /savedata15, their param.sfo,
 * icons, backups and searches, and the save data memory. */

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <compat/strl.h>
#include <file/file_path.h>
#include <retro_dirent.h>
#include <streams/file_stream.h>

#include "common/c_bridge.h"
#include "common/enum_names.h"
#include "core/emulator_settings.h"
#include "core/libraries/kernel/kernel_c.h"
#include "core/libraries/save_data/save_backup.h"
#include "core/libraries/save_data/save_instance.h"
#include "core/libraries/save_data/save_memory.h"
#include "core/libraries/save_data/savedata.h"

#ifndef ORBIS_OK
#define ORBIS_OK 0
#endif

/* The firmware the game asks for (Common::ElfInfo::FW_*) */
#define SAVE_FW_160 0x1600000u
#define SAVE_FW_170 0x1700000u
#define SAVE_FW_200 0x2000000u
#define SAVE_FW_250 0x2500000u
#define SAVE_FW_400 0x4000000u
#define SAVE_FW_450 0x4500000u
#define SAVE_FW_500 0x5000000u

/* OrbisSaveDataMountMode */
#define SAVE_MOUNT_RDONLY (1u << 0)
#define SAVE_MOUNT_RDWR (1u << 1)
#define SAVE_MOUNT_CREATE (1u << 2)
#define SAVE_MOUNT_DESTRUCT_OFF (1u << 3)
#define SAVE_MOUNT_COPY_ICON (1u << 4)
#define SAVE_MOUNT_CREATE2 (1u << 5)

/* OrbisSaveDataMountStatus */
#define SAVE_MOUNT_STATUS_NOTHING 0u
#define SAVE_MOUNT_STATUS_CREATED 1u

/* OrbisSaveDataSortKey */
#define SAVE_SORT_DIRNAME 0u
#define SAVE_SORT_USER_PARAM 1u
#define SAVE_SORT_BLOCKS 2u
#define SAVE_SORT_MTIME 3u
#define SAVE_SORT_FREE_BLOCKS 5u

/* OrbisSaveDataSortOrder */
#define SAVE_SORT_ASCENT 0u
#define SAVE_SORT_DESCENT 1u

#define SAVE_MOUNT_SLOTS 16

typedef struct OrbisSaveDataFingerprint {
    char data[SAVE_DATA_FINGERPRINT_DATA_SIZE];
    u8 _pad[15];
} OrbisSaveDataFingerprint;

typedef struct OrbisSaveDataBackup {
    s32 userId;
    u8 _pad[4];
    const OrbisSaveDataTitleId* titleId;
    const OrbisSaveDataDirName* dirName;
    const OrbisSaveDataFingerprint* param;
    u8 _reserved[32];
} OrbisSaveDataBackup;

typedef struct OrbisSaveDataIcon {
    void* buf;
    size_t bufSize;
    size_t dataSize;
    u8 _reserved[32];
} OrbisSaveDataIcon;

typedef struct OrbisSaveDataCheckBackupData {
    s32 userId;
    u8 _pad[4];
    const OrbisSaveDataTitleId* titleId;
    const OrbisSaveDataDirName* dirName;
    OrbisSaveDataParam* param;
    OrbisSaveDataIcon* icon;
    u8 _reserved[32];
} OrbisSaveDataCheckBackupData;

typedef struct OrbisSaveDataDelete {
    s32 userId;
    u8 _pad[4];
    const OrbisSaveDataTitleId* titleId;
    const OrbisSaveDataDirName* dirName;
    u32 _unused;
    u8 _reserved[32];
    u8 _pad2[4];
} OrbisSaveDataDelete;

typedef struct OrbisSaveDataMemoryData {
    void* buf;
    size_t bufSize;
    s64 offset;
    u8 _reserved[40];
} OrbisSaveDataMemoryData;

typedef struct OrbisSaveDataMemoryGet2 {
    s32 userId;
    u8 _pad[4];
    OrbisSaveDataMemoryData* data;
    OrbisSaveDataParam* param;
    OrbisSaveDataIcon* icon;
    u32 slotId;
    u8 _reserved[28];
} OrbisSaveDataMemoryGet2;

typedef struct OrbisSaveDataMemorySet2 {
    s32 userId;
    u8 _pad[4];
    const OrbisSaveDataMemoryData* data;
    const OrbisSaveDataParam* param;
    const OrbisSaveDataIcon* icon;
    u32 dataNum;
    u32 slotId;
    u8 _reserved[32];
} OrbisSaveDataMemorySet2;

typedef struct OrbisSaveDataMemorySetup2 {
    u32 option; /* OrbisSaveDataSaveDataMemoryOption */
    s32 userId;
    size_t memorySize;
    size_t iconMemorySize;
    const OrbisSaveDataParam* initParam; /* 4.50 on */
    const OrbisSaveDataIcon* initIcon;   /* 4.50 on */
    u32 slotId;
    u8 _reserved[20];
} OrbisSaveDataMemorySetup2;

typedef struct OrbisSaveDataMemorySetupResult {
    size_t existedMemorySize;
    u8 _reserved[16];
} OrbisSaveDataMemorySetupResult;

typedef struct OrbisSaveDataMemorySync {
    s32 userId;
    u32 slotId;
    u32 option; /* 1: blocking */
    u8 _reserved[28];
} OrbisSaveDataMemorySync;

typedef struct OrbisSaveDataMount2 {
    s32 userId;
    u8 _pad[4];
    const OrbisSaveDataDirName* dirName;
    u64 blocks;
    u32 mountMode;
    u8 _reserved[32];
    u8 _pad2[4];
} OrbisSaveDataMount2;

typedef struct OrbisSaveDataMount {
    s32 userId;
    u8 _pad[4];
    const OrbisSaveDataTitleId* titleId;
    const OrbisSaveDataDirName* dirName;
    const OrbisSaveDataFingerprint* fingerprint;
    u64 blocks;
    u32 mountMode;
    u8 _reserved[32];
} OrbisSaveDataMount;

typedef struct OrbisSaveDataMountInfo {
    u64 blocks;
    u64 freeBlocks;
    u8 _reserved[32];
} OrbisSaveDataMountInfo;

typedef struct OrbisSaveDataMountPoint {
    char data[SAVE_DATA_MOUNT_POINT_DATA_MAXSIZE];
} OrbisSaveDataMountPoint;

typedef struct OrbisSaveDataMountResult {
    OrbisSaveDataMountPoint mount_point;
    u64 required_blocks;
    u32 _unused;
    u32 mount_status; /* 4.50 on */
    u8 _reserved[28];
    u8 _pad[4];
} OrbisSaveDataMountResult;

typedef struct OrbisSaveDataRestoreBackupData {
    s32 userId;
    u8 _pad[4];
    const OrbisSaveDataTitleId* titleId;
    const OrbisSaveDataDirName* dirName;
    const OrbisSaveDataFingerprint* fingerprint;
    u32 _unused;
    u8 _reserved[32];
    u8 _pad2[4];
} OrbisSaveDataRestoreBackupData;

typedef struct OrbisSaveDataTransferringMount {
    s32 userId;
    const OrbisSaveDataTitleId* titleId;
    const OrbisSaveDataDirName* dirName;
    const OrbisSaveDataFingerprint* fingerprint;
    u8 _reserved[32];
} OrbisSaveDataTransferringMount;

typedef struct OrbisSaveDataDirNameSearchCond {
    s32 userId;
    u8 _pad[4];
    const OrbisSaveDataTitleId* titleId;
    const OrbisSaveDataDirName* dirName;
    u32 key;   /* SAVE_SORT_* */
    u32 order; /* SAVE_SORT_ASCENT or DESCENT */
    u8 _reserved[32];
} OrbisSaveDataDirNameSearchCond;

typedef struct OrbisSaveDataSearchInfo {
    u64 blocks;
    u64 freeBlocks;
    u8 _reserved[32];
} OrbisSaveDataSearchInfo;

typedef struct OrbisSaveDataDirNameSearchResult {
    u32 hitNum;
    u8 _pad[4];
    OrbisSaveDataDirName* dirNames;
    u32 dirNamesNum;
    u32 setNum;                     /* 1.70 on */
    OrbisSaveDataParam* params;     /* 1.70 on */
    OrbisSaveDataSearchInfo* infos; /* 2.50 on */
    u8 _reserved[12];
    u8 _pad2[4];
} OrbisSaveDataDirNameSearchResult;

typedef struct OrbisSaveDataEvent {
    u32 type; /* SAVE_DATA_EVENT_* */
    s32 errorCode;
    s32 userId;
    u8 _pad[4];
    OrbisSaveDataTitleId titleId;
    OrbisSaveDataDirName dirName;
    u8 _reserved[40];
} OrbisSaveDataEvent;

static bool g_initialized = false;
static char g_game_serial[SAVE_NAME_SIZE];
static u32 g_fw_ver;
static save_instance_t g_mount_slots[SAVE_MOUNT_SLOTS];
static bool g_mount_used[SAVE_MOUNT_SLOTS];

/* The text of a fixed field of @size bytes: up to its terminator, or all of it */
static size_t sd_strnlen(const char* s, size_t size) {
    const char* end = (const char*)memchr(s, 0, size);
    return end != NULL ? (size_t)(end - s) : size;
}

/* CString::FromString: @src cut to @size - 1 bytes, terminated */
static void sd_put(char* out, size_t size, const char* src) {
    const size_t n = strlen(src);
    const size_t k = n < size - 1 ? n : size - 1;
    memcpy(out, src, k);
    out[k] = '\0';
}

/* A fixed field's text, terminated, into @out */
static void sd_get(char* out, size_t out_size, const char* field, size_t field_size) {
    size_t n = sd_strnlen(field, field_size);
    if (n > out_size - 1) {
        n = out_size - 1;
    }
    memcpy(out, field, n);
    out[n] = '\0';
}

static void sd_log(enum c_log_level level, const char* func, const char* msg) {
    c_log(C_LOG_LIB_SAVE_DATA, level, func, "%s", msg);
}

static void initialize(void) {
    g_initialized = true;
    save_game_serial(g_game_serial, sizeof(g_game_serial));
    g_fw_ver = kernel_firmware_version();
    save_backup_start_thread();
}

/* game_00other | game%other: '%' any run, '_' any one character */
static bool match(const char* str, size_t str_len, const char* pat, size_t pat_len) {
    size_t s = 0, p = 0;
    while (s < str_len && p < pat_len) {
        if (pat[p] == '%') {
            size_t w;
            for (w = s; w <= str_len; w++) {
                if (match(str + w, str_len - w, pat + p + 1, pat_len - p - 1)) {
                    return true;
                }
            }
            return false;
        }
        if (pat[p] != '_' && pat[p] != str[s]) {
            return false;
        }
        s++;
        p++;
    }
    return s == str_len && p == pat_len;
}

static void sd_lower(char* s) {
    for (; *s; s++) {
        if (*s >= 'A' && *s <= 'Z') {
            *s = (char)(*s - 'A' + 'a');
        }
    }
}

static u32 setNotInitializedError(void) {
    if (g_fw_ver < SAVE_FW_200) {
        return SAVE_DATA_ERROR_INTERNAL;
    }
    if (g_fw_ver < SAVE_FW_250) {
        return SAVE_DATA_ERROR_USER_SERVICE_NOT_INITIALIZED;
    }
    return SAVE_DATA_ERROR_NOT_INITIALIZED;
}

#define SD_CHECK_INITIALIZED(func)                                                                 \
    do {                                                                                           \
        if (!g_initialized) {                                                                      \
            sd_log(C_LOG_LEVEL_INFO, func, "called without initialize");                           \
            return setNotInitializedError();                                                       \
        }                                                                                          \
    } while (0)

#define SD_INVALID(func)                                                                           \
    do {                                                                                           \
        sd_log(C_LOG_LEVEL_INFO, func, "called with invalid parameter");                           \
        return SAVE_DATA_ERROR_PARAMETER;                                                          \
    } while (0)

/* The mounted save at @mount_point (a game's field), NULL if none */
static save_instance_t* sd_find_mount(const OrbisSaveDataMountPoint* mount_point) {
    char name[SAVE_DATA_MOUNT_POINT_DATA_MAXSIZE + 1];
    int i;
    sd_get(name, sizeof(name), mount_point->data, sizeof(mount_point->data));
    for (i = 0; i < SAVE_MOUNT_SLOTS; i++) {
        if (g_mount_used[i] && strcmp(g_mount_slots[i].mount_point, name) == 0) {
            return &g_mount_slots[i];
        }
    }
    return NULL;
}

/* Whether a mounted save is the one at @save_path */
static bool sd_path_mounted(const char* save_path) {
    int i;
    for (i = 0; i < SAVE_MOUNT_SLOTS; i++) {
        if (g_mount_used[i] && strcmp(g_mount_slots[i].save_path, save_path) == 0) {
            return true;
        }
    }
    return false;
}

/* The title a call names, or the game's */
static void sd_title(char* out, size_t size, const OrbisSaveDataTitleId* title_id) {
    if (title_id != NULL) {
        sd_get(out, size, title_id->data, sizeof(title_id->data));
    } else {
        strlcpy(out, g_game_serial, size);
    }
}

static u32 saveDataMount(const OrbisSaveDataMount2* mount_info,
                         OrbisSaveDataMountResult* mount_result, const char* title_id) {
    char dir_name[SAVE_NAME_SIZE];
    char save_path[PATH_MAX_LENGTH];
    save_instance_t save_instance;
    bool is_ro, create, create_if_not_exist, copy_icon, ignore_corrupt, to_be_created;
    int slot_num = -1;
    int setup;
    int i;

    if (mount_info->userId < 0) {
        return SAVE_DATA_ERROR_INVALID_LOGIN_USER;
    }
    if (mount_info->dirName == NULL) {
        sd_log(C_LOG_LEVEL_INFO, "saveDataMount", "called without dirName");
        return SAVE_DATA_ERROR_PARAMETER;
    }
    sd_get(dir_name, sizeof(dir_name), mount_info->dirName->data,
           sizeof(mount_info->dirName->data));

    /* Being backed up */
    save_make_dir_path(save_path, sizeof(save_path), mount_info->userId, title_id, dir_name);
    if (save_backup_is_executing_for(save_path) && g_fw_ver) {
        return SAVE_DATA_ERROR_BACKUP_BUSY;
    }

    is_ro = (mount_info->mountMode & SAVE_MOUNT_RDONLY) != 0;
    create = (mount_info->mountMode & SAVE_MOUNT_CREATE) != 0;
    create_if_not_exist =
        (mount_info->mountMode & SAVE_MOUNT_CREATE2) != 0 && g_fw_ver >= SAVE_FW_450;
    C_ASSERT_MSG(!create || !create_if_not_exist, "saveDataMount",
                 "!create || !create_if_not_exist"); /* not both */
    copy_icon = (mount_info->mountMode & SAVE_MOUNT_COPY_ICON) != 0;
    ignore_corrupt =
        (mount_info->mountMode & SAVE_MOUNT_DESTRUCT_OFF) != 0 || g_fw_ver < SAVE_FW_160;

    /* The first free mount point; the save must not be mounted before it */
    for (i = 0; i < SAVE_MOUNT_SLOTS; i++) {
        if (g_mount_used[i]) {
            if (strcmp(g_mount_slots[i].dir_name, dir_name) == 0) {
                return SAVE_DATA_ERROR_BUSY;
            }
        } else {
            slot_num = i;
            break;
        }
    }
    if (slot_num == -1) {
        return SAVE_DATA_ERROR_MOUNT_FULL;
    }

    save_instance_init(&save_instance, slot_num, mount_info->userId, title_id, dir_name,
                       (int)mount_info->blocks);
    if (save_instance.mounted) {
        c_unreachable(__FILE__, __LINE__, "saveDataMount", "Save instance should not be mounted");
    }

    if (!create && !create_if_not_exist && !save_instance.exists) {
        save_instance_free(&save_instance);
        return SAVE_DATA_ERROR_NOT_FOUND;
    }
    if (create && save_instance.exists) {
        save_instance_free(&save_instance);
        return SAVE_DATA_ERROR_EXISTS;
    }

    to_be_created = !save_instance.exists;
    if (to_be_created) { /* the size asked for */
        char home[PATH_MAX_LENGTH];
        char user[16];
        char root[PATH_MAX_LENGTH];
        char tmp[PATH_MAX_LENGTH];
        int64_t free_space;
        u64 available, requested;
        if (mount_info->blocks < SAVE_DATA_BLOCKS_MIN2 ||
            mount_info->blocks > SAVE_DATA_BLOCKS_MAX) {
            sd_log(C_LOG_LEVEL_INFO, "saveDataMount", "called with invalid block size");
        }
        emu_settings_get_home_dir(home, sizeof(home));
        snprintf(user, sizeof(user), "%d", (int)mount_info->userId);
        fill_pathname_join(tmp, home, user, sizeof(tmp));
        fill_pathname_join(root, tmp, "savedata", sizeof(root));
        path_mkdir(root);
        free_space = path_get_free_space(root);
        available = free_space < 0 ? ~(u64)0 : (u64)free_space;
        requested = (u64)((u32)save_instance.max_blocks * SAVE_DATA_BLOCK_SIZE);
        if (requested > available) {
            mount_result->required_blocks = (requested - available) / SAVE_DATA_BLOCK_SIZE;
            save_instance_free(&save_instance);
            return SAVE_DATA_ERROR_NO_SPACE_FS;
        }
    }

    setup = save_instance_setup_and_mount(&save_instance, is_ro, copy_icon, ignore_corrupt, false);
    if (setup != SAVE_SETUP_OK) {
        save_instance_free(&save_instance);
        if (setup == SAVE_SETUP_BROKEN) {
            sd_log(C_LOG_LEVEL_ERROR, "saveDataMount", "Corrupted save data");
            return SAVE_DATA_ERROR_BROKEN;
        }
        if (setup == SAVE_SETUP_NO_SPACE) {
            return SAVE_DATA_ERROR_NO_SPACE_FS;
        }
        sd_log(C_LOG_LEVEL_ERROR, "saveDataMount", "Failed to mount save data");
        return SAVE_DATA_ERROR_INTERNAL;
    }

    sd_put(mount_result->mount_point.data, sizeof(mount_result->mount_point.data),
           save_instance.mount_point);
    if (g_fw_ver >= SAVE_FW_450) {
        mount_result->mount_status = create_if_not_exist && to_be_created
                                         ? SAVE_MOUNT_STATUS_CREATED
                                         : SAVE_MOUNT_STATUS_NOTHING;
    }

    /* The slot takes it, its param.sfo with it */
    g_mount_slots[slot_num] = save_instance;
    g_mount_used[slot_num] = true;
    return SAVE_DATA_OK;
}

static u32 sd_umount(const OrbisSaveDataMountPoint* mountPoint, bool call_backup) {
    char name[SAVE_DATA_MOUNT_POINT_DATA_MAXSIZE + 1];
    save_instance_t* instance;
    SD_CHECK_INITIALIZED("Umount");
    if (mountPoint == NULL) {
        SD_INVALID("Umount");
    }
    sd_get(name, sizeof(name), mountPoint->data, sizeof(mountPoint->data));
    c_log(C_LOG_LIB_SAVE_DATA, C_LOG_LEVEL_DEBUG, "Umount", "Umount mountPoint:%s", name);

    instance = sd_find_mount(mountPoint);
    if (instance == NULL) {
        return SAVE_DATA_ERROR_NOT_FOUND;
    }
    /* TODO: check if is busy */
    save_instance_umount(instance);
    if (call_backup) {
        save_backup_start_thread();
        save_backup_new_request(instance->user_id, instance->game_serial, instance->dir_name,
                                SAVE_DATA_EVENT_UMOUNT_BACKUP);
    }
    save_instance_free(instance);
    g_mount_used[instance - g_mount_slots] = false;
    return SAVE_DATA_OK;
}

/* Reads @icon_path into the game's buffer: dataSize is the file's size */
static u32 sd_load_icon(OrbisSaveDataIcon* icon, const char* icon_path) {
    RFILE* file =
        filestream_open(icon_path, RETRO_VFS_FILE_ACCESS_READ, RETRO_VFS_FILE_ACCESS_HINT_NONE);
    int64_t size;
    if (file == NULL) {
        icon->dataSize = 0;
        return SAVE_DATA_OK;
    }
    size = filestream_get_size(file);
    icon->dataSize = size > 0 ? (size_t)size : 0;
    if (icon->dataSize != 0) {
        const size_t n = icon->bufSize < icon->dataSize ? icon->bufSize : icon->dataSize;
        filestream_read(file, icon->buf, (int64_t)n);
    }
    filestream_close(file);
    return SAVE_DATA_OK;
}

#define SD_STUB(name)                                                                              \
    int PS4_SYSV_ABI name(void) {                                                                  \
        c_log(C_LOG_LIB_SAVE_DATA, C_LOG_LEVEL_ERROR, #name, "(STUBBED) called");                  \
        return ORBIS_OK;                                                                           \
    }

SD_STUB(sceSaveDataAbort)
SD_STUB(sceSaveDataBindPsnAccount)
SD_STUB(sceSaveDataBindPsnAccountForSystemBackup)
SD_STUB(sceSaveDataChangeDatabase)
SD_STUB(sceSaveDataChangeInternal)
SD_STUB(sceSaveDataCheckBackupDataForCdlg)
SD_STUB(sceSaveDataCheckBackupDataInternal)
SD_STUB(sceSaveDataCheckCloudData)
SD_STUB(sceSaveDataCheckIpmiIfSize)
SD_STUB(sceSaveDataCheckSaveDataBroken)
SD_STUB(sceSaveDataCheckSaveDataVersion)
SD_STUB(sceSaveDataCheckSaveDataVersionLatest)
SD_STUB(sceSaveDataCopy5)
SD_STUB(sceSaveDataCreateUploadData)
SD_STUB(sceSaveDataDebug)
SD_STUB(sceSaveDataDebugCleanMount)
SD_STUB(sceSaveDataDebugCompiledSdkVersion)
SD_STUB(sceSaveDataDebugCreateSaveDataRoot)
SD_STUB(sceSaveDataDebugGetThreadId)
SD_STUB(sceSaveDataDebugRemoveSaveDataRoot)
SD_STUB(sceSaveDataDebugTarget)
SD_STUB(sceSaveDataDelete5)
SD_STUB(sceSaveDataDeleteAllUser)
SD_STUB(sceSaveDataDeleteCloudData)
SD_STUB(sceSaveDataDeleteUser)
SD_STUB(sceSaveDataDirNameSearchInternal)
SD_STUB(sceSaveDataDownload)
SD_STUB(sceSaveDataGetAllSize)
SD_STUB(sceSaveDataGetAppLaunchedUser)
SD_STUB(sceSaveDataGetAutoUploadConditions)
SD_STUB(sceSaveDataGetAutoUploadRequestInfo)
SD_STUB(sceSaveDataGetAutoUploadSetting)
SD_STUB(sceSaveDataGetBoundPsnAccountCount)
SD_STUB(sceSaveDataGetClientThreadPriority)
SD_STUB(sceSaveDataGetCloudQuotaInfo)
SD_STUB(sceSaveDataGetDataBaseFilePath)
SD_STUB(sceSaveDataGetEventInfo)
SD_STUB(sceSaveDataGetFormat)
SD_STUB(sceSaveDataGetMountedSaveDataCount)
SD_STUB(sceSaveDataGetSaveDataCount)
SD_STUB(sceSaveDataGetSaveDataRootDir)
SD_STUB(sceSaveDataGetSaveDataRootPath)
SD_STUB(sceSaveDataGetSaveDataRootUsbPath)
SD_STUB(sceSaveDataGetSavePoint)
SD_STUB(sceSaveDataGetUpdatedDataCount)
SD_STUB(sceSaveDataInitializeForCdlg)
SD_STUB(sceSaveDataIsDeletingUsbDb)
SD_STUB(sceSaveDataIsMounted)
SD_STUB(sceSaveDataMount5)
SD_STUB(sceSaveDataMountInternal)
SD_STUB(sceSaveDataMountSys)
SD_STUB(sceSaveDataPromote5)
SD_STUB(sceSaveDataRebuildDatabase)
SD_STUB(sceSaveDataRegisterEventCallback)
SD_STUB(sceSaveDataRestoreBackupDataForCdlg)
SD_STUB(sceSaveDataRestoreLoadSaveDataMemory)
SD_STUB(sceSaveDataSetAutoUploadSetting)
SD_STUB(sceSaveDataSetEventInfo)
SD_STUB(sceSaveDataSetSaveDataLibraryUser)
SD_STUB(sceSaveDataShutdownStart)
SD_STUB(sceSaveDataSupportedFakeBrokenStatus)
SD_STUB(sceSaveDataSyncCloudList)
SD_STUB(sceSaveDataUmountSys)
SD_STUB(sceSaveDataUnregisterEventCallback)
SD_STUB(sceSaveDataUpload)
SD_STUB(SaveData_Func_02E4C4D201716422)

u32 PS4_SYSV_ABI sceSaveDataBackup(const OrbisSaveDataBackup* backup) {
    char dir_name[SAVE_NAME_SIZE];
    char title[SAVE_NAME_SIZE];
    int i;
    SD_CHECK_INITIALIZED("sceSaveDataBackup");
    if (backup == NULL || backup->dirName == NULL) {
        SD_INVALID("sceSaveDataBackup");
    }
    sd_get(dir_name, sizeof(dir_name), backup->dirName->data, sizeof(backup->dirName->data));
    c_log(C_LOG_LIB_SAVE_DATA, C_LOG_LEVEL_DEBUG, "sceSaveDataBackup", "called dirName: %s",
          dir_name);
    sd_title(title, sizeof(title), backup->titleId);
    for (i = 0; i < SAVE_MOUNT_SLOTS; i++) {
        const save_instance_t* instance = &g_mount_slots[i];
        if (g_mount_used[i] && instance->user_id == backup->userId &&
            strcmp(instance->game_serial, title) == 0 &&
            strcmp(instance->dir_name, dir_name) == 0) {
            return SAVE_DATA_ERROR_BUSY;
        }
    }
    save_backup_new_request(backup->userId, title, dir_name, SAVE_DATA_EVENT_BACKUP);
    return SAVE_DATA_OK;
}

u32 PS4_SYSV_ABI sceSaveDataCheckBackupData(const OrbisSaveDataCheckBackupData* check) {
    char title[SAVE_NAME_SIZE];
    char dir_name[SAVE_NAME_SIZE];
    char save_path[PATH_MAX_LENGTH];
    char backup_path[PATH_MAX_LENGTH];
    SD_CHECK_INITIALIZED("sceSaveDataCheckBackupData");
    if (check == NULL || check->dirName == NULL) {
        SD_INVALID("sceSaveDataCheckBackupData");
    }
    sd_title(title, sizeof(title), check->titleId);
    c_log(C_LOG_LIB_SAVE_DATA, C_LOG_LEVEL_DEBUG, "sceSaveDataCheckBackupData",
          "called with titleId=%s", title);
    sd_get(dir_name, sizeof(dir_name), check->dirName->data, sizeof(check->dirName->data));
    save_make_dir_path(save_path, sizeof(save_path), check->userId, title, dir_name);
    if (sd_path_mounted(save_path)) {
        return SAVE_DATA_ERROR_BUSY;
    }
    if (save_backup_is_executing_for(save_path)) {
        return SAVE_DATA_ERROR_BACKUP_BUSY;
    }
    save_backup_make_path(save_path, backup_path, sizeof(backup_path));
    if (!path_is_valid(backup_path)) {
        return SAVE_DATA_ERROR_NOT_FOUND;
    }
    if (check->param != NULL) {
        char sfo_path[PATH_MAX_LENGTH];
        psf_t sfo;
        bool ok;
        save_join(sfo_path, sizeof(sfo_path), backup_path, "sce_sys/param.sfo");
        psf_init(&sfo);
        ok = psf_open_path(&sfo, sfo_path);
        if (ok) {
            save_param_from_sfo(check->param, &sfo);
        }
        psf_free(&sfo);
        if (!ok) {
            c_log(C_LOG_LIB_SAVE_DATA, C_LOG_LEVEL_ERROR, "sceSaveDataCheckBackupData",
                  "Failed to read SFO at %s", backup_path);
            return SAVE_DATA_ERROR_INTERNAL;
        }
    }
    if (check->icon != NULL) {
        char icon_path[PATH_MAX_LENGTH];
        save_join(icon_path, sizeof(icon_path), backup_path, "sce_sys/icon0.png");
        if (path_is_valid(icon_path) && sd_load_icon(check->icon, icon_path) != SAVE_DATA_OK) {
            return SAVE_DATA_ERROR_INTERNAL;
        }
    }
    return SAVE_DATA_OK;
}

u32 PS4_SYSV_ABI sceSaveDataClearProgress(void) {
    SD_CHECK_INITIALIZED("sceSaveDataClearProgress");
    sd_log(C_LOG_LEVEL_DEBUG, "sceSaveDataClearProgress", "called");
    save_backup_clear_progress();
    return SAVE_DATA_OK;
}

u32 PS4_SYSV_ABI sceSaveDataDelete(const OrbisSaveDataDelete* del) {
    char dir_name[SAVE_NAME_SIZE];
    char save_path[PATH_MAX_LENGTH];
    int i;
    SD_CHECK_INITIALIZED("sceSaveDataDelete");
    if (del == NULL || del->dirName == NULL) {
        SD_INVALID("sceSaveDataDelete");
    }
    sd_get(dir_name, sizeof(dir_name), del->dirName->data, sizeof(del->dirName->data));
    c_log(C_LOG_LIB_SAVE_DATA, C_LOG_LEVEL_DEBUG, "sceSaveDataDelete", "called dirName: %s",
          dir_name);
    if (dir_name[0] == '\0') {
        return SAVE_DATA_ERROR_PARAMETER;
    }
    for (i = 0; i < SAVE_MOUNT_SLOTS; i++) {
        if (g_mount_used[i] && strcmp(g_mount_slots[i].dir_name, dir_name) == 0) {
            return SAVE_DATA_ERROR_BUSY;
        }
    }
    save_make_dir_path(save_path, sizeof(save_path), del->userId, g_game_serial, dir_name);
    if (!path_remove_tree(save_path)) {
        c_log(C_LOG_LIB_SAVE_DATA, C_LOG_LEVEL_ERROR, "sceSaveDataDelete",
              "Failed to delete save data: %s", save_path);
        return SAVE_DATA_ERROR_INTERNAL;
    }
    return SAVE_DATA_OK;
}

/* One save a search found */
typedef struct sd_found {
    char name[SAVE_NAME_SIZE];
    OrbisSaveDataParam param; /* its param.sfo's, as FromSFO gives it */
    s32 user_param;
    s64 last_write;
    int max_blocks;
    u64 free_blocks;
} sd_found_t;

/* -1, 0 or 1 as @a sorts before, with or after @b by @key */
static int sd_compare(const sd_found_t* a, const sd_found_t* b, u32 key) {
    switch (key) {
    case SAVE_SORT_DIRNAME: {
        const int c = strcmp(a->name, b->name);
        return c < 0 ? -1 : c > 0;
    }
    case SAVE_SORT_USER_PARAM:
        return a->user_param < b->user_param ? -1 : a->user_param > b->user_param;
    case SAVE_SORT_BLOCKS:
        return a->max_blocks < b->max_blocks ? -1 : a->max_blocks > b->max_blocks;
    case SAVE_SORT_FREE_BLOCKS:
        return a->free_blocks < b->free_blocks ? -1 : a->free_blocks > b->free_blocks;
    case SAVE_SORT_MTIME:
        return a->last_write < b->last_write ? -1 : a->last_write > b->last_write;
    default:
        return 0;
    }
}

/* Stable: the ones that compare equal keep their order */
static void sd_sort(sd_found_t** list, size_t count, u32 key) {
    size_t i;
    for (i = 1; i < count; i++) {
        sd_found_t* v = list[i];
        size_t j = i;
        while (j > 0 && sd_compare(v, list[j - 1], key) < 0) {
            list[j] = list[j - 1];
            j--;
        }
        list[j] = v;
    }
}

u32 PS4_SYSV_ABI sceSaveDataDirNameSearch(const OrbisSaveDataDirNameSearchCond* cond,
                                          OrbisSaveDataDirNameSearchResult* result) {
    char title_id[SAVE_NAME_SIZE];
    char save_path[PATH_MAX_LENGTH];
    char pat[SAVE_DATA_DIRNAME_DATA_MAXSIZE + 1];
    struct RDIR* rdir;
    sd_found_t* found = NULL;
    sd_found_t** list = NULL;
    size_t count = 0, capacity = 0, max_count, i;

    SD_CHECK_INITIALIZED("sceSaveDataDirNameSearch");
    if (cond == NULL || result == NULL || cond->key > SAVE_SORT_FREE_BLOCKS ||
        cond->order > SAVE_SORT_DESCENT) {
        SD_INVALID("sceSaveDataDirNameSearch");
    }
    sd_log(C_LOG_LEVEL_DEBUG, "sceSaveDataDirNameSearch", "called");
    sd_title(title_id, sizeof(title_id), cond->titleId);
    save_make_title_path(save_path, sizeof(save_path), cond->userId, title_id);
    if (!path_is_valid(save_path)) {
        result->hitNum = 0;
        if (g_fw_ver >= SAVE_FW_170) {
            result->setNum = 0;
        }
        return SAVE_DATA_OK;
    }

    pat[0] = '\0';
    if (cond->dirName != NULL) {
        sd_get(pat, sizeof(pat), cond->dirName->data, sizeof(cond->dirName->data));
        sd_lower(pat);
    }

    /* Folders with a param.sfo, not sce_*, that the name matches */
    rdir = retro_opendir_include_hidden(save_path, true);
    while (rdir != NULL && retro_readdir(rdir)) {
        const char* name = retro_dirent_get_name(rdir);
        char dir_path[PATH_MAX_LENGTH];
        char sfo_path[PATH_MAX_LENGTH];
        char low[SAVE_NAME_SIZE];
        psf_t sfo;
        sd_found_t* f;
        size_t size;
        u64 total;
        if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0 || !retro_dirent_is_dir(rdir, NULL) ||
            strncmp(name, "sce_", 4) == 0 || strlen(name) >= SAVE_NAME_SIZE) {
            continue;
        }
        fill_pathname_join(dir_path, save_path, name, sizeof(dir_path));
        save_join(sfo_path, sizeof(sfo_path), dir_path, "sce_sys/param.sfo");
        if (!path_is_valid(sfo_path)) {
            continue;
        }
        if (pat[0] != '\0') {
            strlcpy(low, name, sizeof(low));
            sd_lower(low);
            if (!match(low, strlen(low), pat, strlen(pat))) {
                continue;
            }
        }
        psf_init(&sfo);
        if (!psf_open_path(&sfo, sfo_path)) {
            c_log(C_LOG_LIB_SAVE_DATA, C_LOG_LEVEL_ERROR, "sceSaveDataDirNameSearch",
                  "Failed to read SFO: %s", sfo_path);
            psf_free(&sfo);
            continue;
        }
        if (count == capacity) {
            const size_t want = capacity != 0 ? capacity * 2 : 16;
            sd_found_t* grown = (sd_found_t*)realloc(found, want * sizeof(*found));
            if (grown == NULL) {
                psf_free(&sfo);
                break;
            }
            found = grown;
            capacity = want;
        }
        f = &found[count++];
        strlcpy(f->name, name, sizeof(f->name));
        save_param_from_sfo(&f->param, &sfo);
        f->user_param = (s32)f->param.userParam;
        f->last_write = sfo.last_write;
        size = (size_t)save_dir_size(dir_path);
        total = save_max_blocks_from_sfo(&sfo);
        f->max_blocks = (int)total;
        f->free_blocks = total - size / SAVE_DATA_BLOCK_SIZE;
        psf_free(&sfo);
    }
    if (rdir != NULL) {
        retro_closedir(rdir);
    }

    list = (sd_found_t**)malloc((count != 0 ? count : 1) * sizeof(*list));
    if (list == NULL) {
        free(found);
        return SAVE_DATA_ERROR_OUT_OF_MEMORY;
    }
    for (i = 0; i < count; i++) {
        list[i] = &found[i];
    }
    sd_sort(list, count, cond->key);
    if (cond->order == SAVE_SORT_DESCENT) {
        for (i = 0; i < count / 2; i++) {
            sd_found_t* t = list[i];
            list[i] = list[count - 1 - i];
            list[count - 1 - i] = t;
        }
    }

    max_count = result->dirNamesNum < count ? result->dirNamesNum : count;
    if (g_fw_ver >= SAVE_FW_170) {
        result->hitNum = (u32)count;
        result->setNum = (u32)max_count;
    } else {
        result->hitNum = (u32)max_count;
    }
    for (i = 0; i < max_count; i++) {
        sd_put(result->dirNames[i].data, sizeof(result->dirNames[i].data), list[i]->name);
        if (g_fw_ver >= SAVE_FW_170 && result->params != NULL) {
            result->params[i] = list[i]->param;
        }
        if (g_fw_ver >= SAVE_FW_250 && result->infos != NULL) {
            result->infos[i].blocks = (u64)(s64)list[i]->max_blocks;
            result->infos[i].freeBlocks = list[i]->free_blocks;
        }
    }
    free(list);
    free(found);
    return SAVE_DATA_OK;
}

u32 PS4_SYSV_ABI sceSaveDataGetEventResult(const void* eventParam, OrbisSaveDataEvent* event) {
    save_backup_request_t last;
    (void)eventParam;
    SD_CHECK_INITIALIZED("sceSaveDataGetEventResult");
    if (event == NULL) {
        SD_INVALID("sceSaveDataGetEventResult");
    }
#ifndef NDEBUG
    sd_log(C_LOG_LEVEL_TRACE, "sceSaveDataGetEventResult", "called");
#endif
    if (!save_backup_pop_last_event(&last)) {
        return SAVE_DATA_ERROR_NOT_FOUND;
    }
    event->type = last.origin;
    event->errorCode = 0;
    event->userId = last.user_id;
    sd_put(event->titleId.data, sizeof(event->titleId.data), last.title_id);
    sd_put(event->dirName.data, sizeof(event->dirName.data), last.dir_name);
    return SAVE_DATA_OK;
}

u32 PS4_SYSV_ABI sceSaveDataGetMountInfo(const OrbisSaveDataMountPoint* mountPoint,
                                         OrbisSaveDataMountInfo* info) {
    const save_instance_t* instance;
    SD_CHECK_INITIALIZED("sceSaveDataGetMountInfo");
    if (mountPoint == NULL || info == NULL) {
        SD_INVALID("sceSaveDataGetMountInfo");
    }
    sd_log(C_LOG_LEVEL_DEBUG, "sceSaveDataGetMountInfo", "called");
    instance = sd_find_mount(mountPoint);
    if (instance == NULL) {
        return SAVE_DATA_ERROR_NOT_MOUNTED;
    }
    {
        const u32 blocks = (u32)instance->max_blocks;
        const u64 size = save_dir_size(instance->save_path);
        info->blocks = blocks;
        info->freeBlocks = blocks - size / SAVE_DATA_BLOCK_SIZE;
    }
    return SAVE_DATA_OK;
}

u32 PS4_SYSV_ABI sceSaveDataGetParam(const OrbisSaveDataMountPoint* mountPoint, u32 paramType,
                                     void* paramBuf, size_t paramBufSize, size_t* gotSize) {
    const save_instance_t* instance;
    const psf_t* param_sfo;
    SD_CHECK_INITIALIZED("sceSaveDataGetParam");
    if (paramType > ORBIS_SAVE_DATA_PARAM_TYPE_MTIME || paramBuf == NULL || mountPoint == NULL) {
        SD_INVALID("sceSaveDataGetParam");
    }
    c_log(C_LOG_LIB_SAVE_DATA, C_LOG_LEVEL_DEBUG, "sceSaveDataGetParam", "called: paramType = %s",
          enum_name(&enum_names_OrbisSaveDataParamType, (s64)paramType));
    instance = sd_find_mount(mountPoint);
    if (instance == NULL) {
        return SAVE_DATA_ERROR_NOT_MOUNTED;
    }
    param_sfo = &instance->param_sfo;
    switch (paramType) {
    case ORBIS_SAVE_DATA_PARAM_TYPE_ALL:
        C_ASSERT_MSG(paramBufSize == sizeof(OrbisSaveDataParam), "sceSaveDataGetParam",
                     "paramBufSize == sizeof(OrbisSaveDataParam)");
        save_param_from_sfo((OrbisSaveDataParam*)paramBuf, param_sfo);
        if (gotSize != NULL) {
            *gotSize = sizeof(OrbisSaveDataParam);
        }
        break;
    case ORBIS_SAVE_DATA_PARAM_TYPE_TITLE:
    case ORBIS_SAVE_DATA_PARAM_TYPE_SUB_TITLE:
    case ORBIS_SAVE_DATA_PARAM_TYPE_DETAIL: {
        char* param = (char*)paramBuf;
        const char* key = paramType == ORBIS_SAVE_DATA_PARAM_TYPE_TITLE       ? SAVE_PARAM_MAINTITLE
                          : paramType == ORBIS_SAVE_DATA_PARAM_TYPE_SUB_TITLE ? SAVE_PARAM_SUBTITLE
                                                                              : SAVE_PARAM_DETAIL;
        const char* value = "";
        size_t length = 0;
        size_t s;
        if (paramBufSize == 0) {
            return SAVE_DATA_ERROR_PARAMETER;
        }
        psf_get_string(param_sfo, key, &value, &length);
        s = length < paramBufSize - 1 ? length : paramBufSize - 1;
        memcpy(param, value, s);
        param[s] = '\0';
        if (gotSize != NULL) {
            *gotSize = s + 1;
        }
    } break;
    case ORBIS_SAVE_DATA_PARAM_TYPE_USER_PARAM: {
        s32 value = 0;
        psf_get_integer(param_sfo, SAVE_PARAM_SAVEDATA_LIST_PARAM, &value);
        *(u32*)paramBuf = (u32)value;
        if (gotSize != NULL) {
            *gotSize = sizeof(u32);
        }
    } break;
    case ORBIS_SAVE_DATA_PARAM_TYPE_MTIME:
        *(s64*)paramBuf = param_sfo->last_write;
        if (gotSize != NULL) {
            *gotSize = sizeof(s64);
        }
        break;
    default:
        c_unreachable(__FILE__, __LINE__, "sceSaveDataGetParam", "paramType");
    }
    return SAVE_DATA_OK;
}

u32 PS4_SYSV_ABI sceSaveDataGetProgress(float* progress) {
    SD_CHECK_INITIALIZED("sceSaveDataGetProgress");
    if (progress == NULL) {
        SD_INVALID("sceSaveDataGetProgress");
    }
    sd_log(C_LOG_LEVEL_DEBUG, "sceSaveDataGetProgress", "called");
    *progress = save_backup_progress();
    return SAVE_DATA_OK;
}

u32 PS4_SYSV_ABI sceSaveDataGetSaveDataMemory(s32 userId, void* buf, size_t bufSize, s64 offset) {
    OrbisSaveDataMemoryData data;
    OrbisSaveDataMemoryGet2 param;
    sd_log(C_LOG_LEVEL_DEBUG, "sceSaveDataGetSaveDataMemory",
           "Redirecting to sceSaveDataGetSaveDataMemory2");
    memset(&data, 0, sizeof(data));
    data.buf = buf;
    data.bufSize = bufSize;
    data.offset = offset;
    memset(&param, 0, sizeof(param));
    param.userId = userId;
    param.data = &data;
    return sceSaveDataGetSaveDataMemory2(&param);
}

/* The slot a save data memory call is for: slots came with 5.00 */
static u32 sd_memory_slot(u32 slot_id) {
    return g_fw_ver > SAVE_FW_500 ? slot_id : 0;
}

u32 PS4_SYSV_ABI sceSaveDataGetSaveDataMemory2(OrbisSaveDataMemoryGet2* getParam) {
    u32 slot_id;
    SD_CHECK_INITIALIZED("sceSaveDataGetSaveDataMemory2");
    if (getParam == NULL) {
        SD_INVALID("sceSaveDataGetSaveDataMemory2");
    }
    slot_id = sd_memory_slot(getParam->slotId);
    if (!save_memory_is_initialized(slot_id)) {
        sd_log(C_LOG_LEVEL_INFO, "sceSaveDataGetSaveDataMemory2",
               "called without save memory initialized");
        return SAVE_DATA_ERROR_MEMORY_NOT_READY;
    }
    sd_log(C_LOG_LEVEL_DEBUG, "sceSaveDataGetSaveDataMemory2", "called");
    if (getParam->data != NULL) {
        save_memory_read(slot_id, getParam->data->buf, getParam->data->bufSize,
                         getParam->data->offset);
    }
    if (getParam->param != NULL) {
        save_param_from_sfo(getParam->param, save_memory_param_sfo(slot_id));
    }
    if (getParam->icon != NULL) {
        OrbisSaveDataIcon* icon = getParam->icon;
        icon->dataSize = save_memory_copy_icon(slot_id, icon->buf, icon->bufSize);
    }
    return SAVE_DATA_OK;
}

u32 PS4_SYSV_ABI sceSaveDataInitialize(void* init) {
    (void)init;
    sd_log(C_LOG_LEVEL_DEBUG, "sceSaveDataInitialize", "called");
    initialize();
    return SAVE_DATA_OK;
}

u32 PS4_SYSV_ABI sceSaveDataInitialize2(void* init) {
    (void)init;
    sd_log(C_LOG_LEVEL_DEBUG, "sceSaveDataInitialize2", "called");
    initialize();
    return SAVE_DATA_OK;
}

u32 PS4_SYSV_ABI sceSaveDataInitialize3(void* init) {
    (void)init;
    sd_log(C_LOG_LEVEL_DEBUG, "sceSaveDataInitialize3", "called");
    initialize();
    return SAVE_DATA_OK;
}

u32 PS4_SYSV_ABI sceSaveDataLoadIcon(const OrbisSaveDataMountPoint* mountPoint,
                                     OrbisSaveDataIcon* icon) {
    const save_instance_t* instance;
    char path[PATH_MAX_LENGTH];
    SD_CHECK_INITIALIZED("sceSaveDataLoadIcon");
    if (mountPoint == NULL || icon == NULL || icon->buf == NULL) {
        SD_INVALID("sceSaveDataLoadIcon");
    }
    sd_log(C_LOG_LEVEL_DEBUG, "sceSaveDataLoadIcon", "called");
    instance = sd_find_mount(mountPoint);
    if (instance == NULL) {
        return SAVE_DATA_ERROR_NOT_MOUNTED;
    }
    save_instance_icon_path(instance, path, sizeof(path));
    if (!path_is_valid(path)) {
        return SAVE_DATA_ERROR_NOT_FOUND;
    }
    return sd_load_icon(icon, path);
}

/* The mount mode in binary, for the log */
static void sd_binary(char* out, u32 value) {
    char bits[33];
    int n = 0, i;
    do {
        bits[n++] = (char)('0' + (value & 1));
        value >>= 1;
    } while (value != 0);
    for (i = 0; i < n; i++) {
        out[i] = bits[n - 1 - i];
    }
    out[n] = '\0';
}

static void sd_log_mount(const char* func, const OrbisSaveDataDirName* dir_name, u32 mode,
                         u64 blocks) {
    char name[SAVE_NAME_SIZE];
    char bits[33];
    sd_get(name, sizeof(name), dir_name->data, sizeof(dir_name->data));
    sd_binary(bits, mode);
    c_log(C_LOG_LIB_SAVE_DATA, C_LOG_LEVEL_DEBUG, func,
          "called dirName: %s, mode: %s, blocks: %" PRIu64, name, bits, blocks);
}

u32 PS4_SYSV_ABI sceSaveDataMount(const OrbisSaveDataMount* mount,
                                  OrbisSaveDataMountResult* mount_result) {
    OrbisSaveDataMount2 mount_info;
    SD_CHECK_INITIALIZED("sceSaveDataMount");
    if (mount == NULL || mount->dirName == NULL) {
        SD_INVALID("sceSaveDataMount");
    }
    sd_log_mount("sceSaveDataMount", mount->dirName, mount->mountMode, mount->blocks);
    memset(&mount_info, 0, sizeof(mount_info));
    mount_info.userId = mount->userId;
    mount_info.dirName = mount->dirName;
    mount_info.mountMode = mount->mountMode;
    mount_info.blocks = mount->blocks;
    return saveDataMount(&mount_info, mount_result, g_game_serial);
}

u32 PS4_SYSV_ABI sceSaveDataMount2(const OrbisSaveDataMount2* mount,
                                   OrbisSaveDataMountResult* mount_result) {
    SD_CHECK_INITIALIZED("sceSaveDataMount2");
    if (mount == NULL || mount->dirName == NULL) {
        SD_INVALID("sceSaveDataMount2");
    }
    sd_log_mount("sceSaveDataMount2", mount->dirName, mount->mountMode, mount->blocks);
    return saveDataMount(mount, mount_result, g_game_serial);
}

u32 PS4_SYSV_ABI sceSaveDataRestoreBackupData(const OrbisSaveDataRestoreBackupData* restore) {
    char dir_name[SAVE_NAME_SIZE];
    char title[SAVE_NAME_SIZE];
    char save_path[PATH_MAX_LENGTH];
    char backup_path[PATH_MAX_LENGTH];
    SD_CHECK_INITIALIZED("sceSaveDataRestoreBackupData");
    if (restore == NULL || restore->dirName == NULL) {
        SD_INVALID("sceSaveDataRestoreBackupData");
    }
    sd_get(dir_name, sizeof(dir_name), restore->dirName->data, sizeof(restore->dirName->data));
    c_log(C_LOG_LIB_SAVE_DATA, C_LOG_LEVEL_DEBUG, "sceSaveDataRestoreBackupData",
          "called dirName: %s", dir_name);
    sd_title(title, sizeof(title), restore->titleId);
    save_make_dir_path(save_path, sizeof(save_path), restore->userId, title, dir_name);
    if (sd_path_mounted(save_path)) {
        return SAVE_DATA_ERROR_BUSY;
    }
    if (save_backup_is_executing_for(save_path)) {
        return SAVE_DATA_ERROR_BACKUP_BUSY;
    }
    save_backup_make_path(save_path, backup_path, sizeof(backup_path));
    if (!path_is_valid(backup_path)) {
        return SAVE_DATA_ERROR_NOT_FOUND;
    }
    return save_backup_restore(save_path) ? SAVE_DATA_OK : SAVE_DATA_ERROR_INTERNAL;
}

u32 PS4_SYSV_ABI sceSaveDataSaveIcon(const OrbisSaveDataMountPoint* mountPoint,
                                     const OrbisSaveDataIcon* icon) {
    const save_instance_t* instance;
    char path[PATH_MAX_LENGTH];
    SD_CHECK_INITIALIZED("sceSaveDataSaveIcon");
    if (mountPoint == NULL || icon == NULL || icon->buf == NULL) {
        SD_INVALID("sceSaveDataSaveIcon");
    }
    sd_log(C_LOG_LEVEL_DEBUG, "sceSaveDataSaveIcon", "called");
    instance = sd_find_mount(mountPoint);
    if (instance == NULL) {
        return SAVE_DATA_ERROR_NOT_MOUNTED;
    }
    if (instance->read_only) {
        return SAVE_DATA_ERROR_BAD_MOUNTED;
    }
    save_instance_icon_path(instance, path, sizeof(path));
    if (!save_write_file(path, icon->buf,
                         icon->bufSize < icon->dataSize ? icon->bufSize : icon->dataSize)) {
        c_log(C_LOG_LIB_SAVE_DATA, C_LOG_LEVEL_ERROR, "sceSaveDataSaveIcon",
              "Failed to save icon: %s", path);
        return SAVE_DATA_ERROR_INTERNAL;
    }
    return SAVE_DATA_OK;
}

u32 PS4_SYSV_ABI sceSaveDataSetParam(const OrbisSaveDataMountPoint* mountPoint, u32 paramType,
                                     const void* paramBuf, size_t paramBufSize) {
    save_instance_t* instance;
    psf_t* param_sfo;
    SD_CHECK_INITIALIZED("sceSaveDataSetParam");
    if (paramType > ORBIS_SAVE_DATA_PARAM_TYPE_USER_PARAM || mountPoint == NULL ||
        paramBuf == NULL) {
        SD_INVALID("sceSaveDataSetParam");
    }
    c_log(C_LOG_LIB_SAVE_DATA, C_LOG_LEVEL_DEBUG, "sceSaveDataSetParam", "called: paramType = %s",
          enum_name(&enum_names_OrbisSaveDataParamType, (s64)paramType));
    instance = sd_find_mount(mountPoint);
    if (instance == NULL) {
        return SAVE_DATA_ERROR_NOT_MOUNTED;
    }
    param_sfo = &instance->param_sfo;
    switch (paramType) {
    case ORBIS_SAVE_DATA_PARAM_TYPE_ALL:
        C_ASSERT_MSG(paramBufSize == sizeof(OrbisSaveDataParam), "sceSaveDataSetParam",
                     "paramBufSize == sizeof(OrbisSaveDataParam)");
        save_param_to_sfo((const OrbisSaveDataParam*)paramBuf, param_sfo);
        break;
    case ORBIS_SAVE_DATA_PARAM_TYPE_TITLE:
    case ORBIS_SAVE_DATA_PARAM_TYPE_SUB_TITLE:
    case ORBIS_SAVE_DATA_PARAM_TYPE_DETAIL: {
        const char* value = (const char*)paramBuf;
        const char* key = paramType == ORBIS_SAVE_DATA_PARAM_TYPE_TITLE       ? SAVE_PARAM_MAINTITLE
                          : paramType == ORBIS_SAVE_DATA_PARAM_TYPE_SUB_TITLE ? SAVE_PARAM_SUBTITLE
                                                                              : SAVE_PARAM_DETAIL;
        psf_set_string(param_sfo, key, value, strlen(value), true);
    } break;
    case ORBIS_SAVE_DATA_PARAM_TYPE_USER_PARAM: {
        s32 value;
        memcpy(&value, paramBuf, sizeof(value));
        psf_set_integer(param_sfo, SAVE_PARAM_SAVEDATA_LIST_PARAM, value, true);
    } break;
    default:
        c_unreachable(__FILE__, __LINE__, "sceSaveDataSetParam", "paramType");
    }
    return SAVE_DATA_OK;
}

u32 PS4_SYSV_ABI sceSaveDataSetSaveDataMemory(s32 userId, void* buf, size_t bufSize, s64 offset) {
    OrbisSaveDataMemoryData data;
    OrbisSaveDataMemorySet2 setParam;
    sd_log(C_LOG_LEVEL_DEBUG, "sceSaveDataSetSaveDataMemory",
           "Redirecting to sceSaveDataSetSaveDataMemory2");
    memset(&data, 0, sizeof(data));
    data.buf = buf;
    data.bufSize = bufSize;
    data.offset = offset;
    memset(&setParam, 0, sizeof(setParam));
    setParam.userId = userId;
    setParam.data = &data;
    return sceSaveDataSetSaveDataMemory2(&setParam);
}

u32 PS4_SYSV_ABI sceSaveDataSetSaveDataMemory2(const OrbisSaveDataMemorySet2* setParam) {
    u32 slot_id = 0;
    u32 data_num = 1;
    u32 i;
    SD_CHECK_INITIALIZED("sceSaveDataSetSaveDataMemory2");
    if (setParam == NULL) {
        SD_INVALID("sceSaveDataSetSaveDataMemory2");
    }
    if (g_fw_ver > SAVE_FW_500) {
        slot_id = setParam->slotId;
        if (setParam->dataNum > 1) {
            data_num = setParam->dataNum;
        }
    }
    if (!save_memory_is_initialized(slot_id)) {
        sd_log(C_LOG_LEVEL_INFO, "sceSaveDataSetSaveDataMemory2",
               "called without save memory initialized");
        return SAVE_DATA_ERROR_MEMORY_NOT_READY;
    }
    sd_log(C_LOG_LEVEL_DEBUG, "sceSaveDataSetSaveDataMemory2", "called");
    if (setParam->data != NULL) {
        for (i = 0; i < data_num; i++) {
            save_memory_write(slot_id, setParam->data[i].buf, setParam->data[i].bufSize,
                              setParam->data[i].offset);
        }
    }
    if (setParam->param != NULL) {
        save_param_to_sfo(setParam->param, save_memory_param_sfo(slot_id));
        save_memory_save_sfo(slot_id);
    }
    if (setParam->icon != NULL) {
        save_memory_set_icon(slot_id, setParam->icon->buf, setParam->icon->bufSize);
    }
    return SAVE_DATA_OK;
}

u32 PS4_SYSV_ABI sceSaveDataSetupSaveDataMemory(s32 userId, size_t memorySize,
                                                OrbisSaveDataParam* param) {
    OrbisSaveDataMemorySetup2 setupParam;
    OrbisSaveDataMemorySetupResult result;
    u32 res;
    c_log(C_LOG_LIB_SAVE_DATA, C_LOG_LEVEL_DEBUG, "sceSaveDataSetupSaveDataMemory",
          "called: userId = %d, memorySize = %llu", (int)userId, (unsigned long long)memorySize);
    memset(&setupParam, 0, sizeof(setupParam));
    setupParam.userId = userId;
    setupParam.memorySize = memorySize;
    memset(&result, 0, sizeof(result));
    res = sceSaveDataSetupSaveDataMemory2(&setupParam, &result);
    if (res != SAVE_DATA_OK) {
        return res;
    }
    if (param != NULL) {
        OrbisSaveDataMemorySet2 setParam;
        memset(&setParam, 0, sizeof(setParam));
        setParam.userId = userId;
        setParam.param = param;
        sceSaveDataSetSaveDataMemory2(&setParam);
    }
    return SAVE_DATA_OK;
}

u32 PS4_SYSV_ABI sceSaveDataSetupSaveDataMemory2(const OrbisSaveDataMemorySetup2* setupParam,
                                                 OrbisSaveDataMemorySetupResult* result) {
    char save_path[PATH_MAX_LENGTH];
    size_t existed_size = 0;
    u32 slot_id;
    SD_CHECK_INITIALIZED("sceSaveDataSetupSaveDataMemory2");
    if (setupParam == NULL) {
        SD_INVALID("sceSaveDataSetupSaveDataMemory2");
    }
    sd_log(C_LOG_LEVEL_DEBUG, "sceSaveDataSetupSaveDataMemory2", "called");
    slot_id = sd_memory_slot(setupParam->slotId);
    save_memory_save_path(setupParam->userId, slot_id, g_game_serial, save_path, sizeof(save_path));
    if (sd_path_mounted(save_path)) {
        return SAVE_DATA_ERROR_BUSY;
    }
    if (!save_memory_setup(setupParam->userId, slot_id, g_game_serial, setupParam->memorySize,
                           &existed_size)) {
        save_show_message("Failed to create or load save memory");
        return SAVE_DATA_ERROR_INTERNAL;
    }
    if (existed_size == 0) { /* just made */
        const OrbisSaveDataIcon* init_icon = setupParam->initIcon;
        if (g_fw_ver >= SAVE_FW_450 && setupParam->initParam != NULL) {
            save_param_to_sfo(setupParam->initParam, save_memory_param_sfo(slot_id));
        }
        if (!save_memory_save_sfo(slot_id)) {
            sd_log(C_LOG_LEVEL_ERROR, "sceSaveDataSetupSaveDataMemory2",
                   "Failed to create/load save memory: param.sfo not written");
            save_show_message("Failed to create or load save memory:\nparam.sfo not written");
            return SAVE_DATA_ERROR_INTERNAL;
        }
        if (g_fw_ver >= SAVE_FW_450 && init_icon != NULL) {
            save_memory_set_icon(slot_id, init_icon->buf, init_icon->bufSize);
        } else {
            save_memory_set_icon(slot_id, NULL, 0);
        }
    }
    if (g_fw_ver >= SAVE_FW_450 && result != NULL) {
        result->existedMemorySize = existed_size;
    }
    return SAVE_DATA_OK;
}

u32 PS4_SYSV_ABI sceSaveDataSyncSaveDataMemory(OrbisSaveDataMemorySync* syncParam) {
    char dir[SAVE_NAME_SIZE];
    u32 slot_id;
    SD_CHECK_INITIALIZED("sceSaveDataSyncSaveDataMemory");
    if (syncParam == NULL) {
        SD_INVALID("sceSaveDataSyncSaveDataMemory");
    }
    slot_id = sd_memory_slot(syncParam->slotId);
    if (!save_memory_is_initialized(slot_id)) {
        sd_log(C_LOG_LEVEL_INFO, "sceSaveDataSyncSaveDataMemory",
               "called without save memory initialized");
        return SAVE_DATA_ERROR_MEMORY_NOT_READY;
    }
    sd_log(C_LOG_LEVEL_DEBUG, "sceSaveDataSyncSaveDataMemory", "called");
    save_memory_persist(slot_id);
    save_memory_dir_name(slot_id, dir, sizeof(dir));
    save_backup_new_request(syncParam->userId, g_game_serial, dir,
                            SAVE_DATA_EVENT_SAVE_DATA_MEMORY_SYNC);
    return SAVE_DATA_OK;
}

u32 PS4_SYSV_ABI sceSaveDataTerminate(void) {
    int i;
    sd_log(C_LOG_LEVEL_DEBUG, "sceSaveDataTerminate", "called");
    if (!g_initialized) {
        return setNotInitializedError();
    }
    for (i = 0; i < SAVE_MOUNT_SLOTS; i++) {
        if (g_mount_used[i]) {
            if (g_fw_ver >= SAVE_FW_400) {
                return SAVE_DATA_ERROR_BUSY;
            }
            save_instance_umount(&g_mount_slots[i]);
            save_instance_free(&g_mount_slots[i]);
            g_mount_used[i] = false;
        }
    }
    g_initialized = false;
    save_backup_stop_thread();
    return SAVE_DATA_OK;
}

u32 PS4_SYSV_ABI sceSaveDataTransferringMount(const OrbisSaveDataTransferringMount* mount,
                                              OrbisSaveDataMountResult* mountResult) {
    OrbisSaveDataMount2 mount_info;
    char title[SAVE_NAME_SIZE];
    char dir_name[SAVE_NAME_SIZE];
    sd_log(C_LOG_LEVEL_DEBUG, "sceSaveDataTransferringMount", "called");
    SD_CHECK_INITIALIZED("sceSaveDataTransferringMount");
    if (mount == NULL || mount->titleId == NULL || mount->dirName == NULL) {
        SD_INVALID("sceSaveDataTransferringMount");
    }
    sd_get(title, sizeof(title), mount->titleId->data, sizeof(mount->titleId->data));
    sd_get(dir_name, sizeof(dir_name), mount->dirName->data, sizeof(mount->dirName->data));
    c_log(C_LOG_LIB_SAVE_DATA, C_LOG_LEVEL_DEBUG, "sceSaveDataTransferringMount",
          "called titleId: %s, dirName: %s", title, dir_name);
    memset(&mount_info, 0, sizeof(mount_info));
    mount_info.userId = mount->userId;
    mount_info.dirName = mount->dirName;
    mount_info.mountMode = SAVE_MOUNT_RDONLY;
    return saveDataMount(&mount_info, mountResult, title);
}

u32 PS4_SYSV_ABI sceSaveDataUmount(const OrbisSaveDataMountPoint* mountPoint) {
    sd_log(C_LOG_LEVEL_DEBUG, "sceSaveDataUmount", "called");
    return sd_umount(mountPoint, false);
}

u32 PS4_SYSV_ABI sceSaveDataUmountWithBackup(const OrbisSaveDataMountPoint* mountPoint) {
    sd_log(C_LOG_LEVEL_DEBUG, "sceSaveDataUmountWithBackup", "called");
    return sd_umount(mountPoint, true);
}
