/* SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include <boolean.h>
#include <compat/strl.h>
#include <file/file_path.h>
#include <retro_miscellaneous.h>

#include "common/c_bridge.h"
#include "core/emulator_settings.h"
#include "core/file_format/psf.h"
#include "core/file_format/rif_c.h"
#include "core/file_sys/fs_c.h"
#include "core/libraries/app_content/app_content.h"
#include "core/libraries/app_content/app_content_error.h"
#include "core/libraries/kernel/kernel_c.h"
#include "core/libraries/system/systemservice_c.h"

typedef struct AddContInfo {
    u32 service_label;
    char entitlement_label[ORBIS_NP_UNIFIED_ENTITLEMENT_LABEL_SIZE];
    u32 status; /* ORBIS_APP_CONTENT_ADDCONT_DOWNLOAD_STATUS_* */
    OrbisAppContentGetEntitlementKey key;
} AddContInfo;

typedef char app_content_key_fits[ORBIS_APP_CONTENT_ENTITLEMENT_KEY_SIZE == FS_ADDCONT_KEY_SIZE &&
                                          FS_ADDCONT_KEY_SIZE == RIF_ENTITLEMENT_KEY_SIZE
                                      ? 1
                                      : -1];

static AddContInfo addcont_info[ORBIS_APP_CONTENT_INFO_LIST_MAX_SIZE];

static s32 sdk_ver = 0;
static s32 addcont_count = 0;

/* The game's own service and the seven SERVICE_ID_ADDCONT_ADD_ ones, at most */
#define APP_CONTENT_SERVICES 8
/* A param.sfo string: TITLE_ID, CONTENT_ID or a service ID */
#define APP_CONTENT_ID_SIZE 128

static char title_id[APP_CONTENT_ID_SIZE];
static bool is_initialized = false;
/* Folders searched for additional content: the game's own title ID plus the title IDs of any
 * services linked through SERVICE_ID_ADDCONT_ADD_1..7 in the game's param.sfo. The first is the
 * game's own, which also has the PKGs beside the game's PKG. */
static char addon_paths[APP_CONTENT_SERVICES][PATH_MAX_LENGTH];
static s32 addon_path_count = 0;

/* What a std::string_view compare of a label held in 17 bytes and @id, @id_len long, gave */
static bool LabelIs(const char* label, const char* id, size_t id_len) {
    size_t n = 0;
    while (n < ORBIS_NP_UNIFIED_ENTITLEMENT_LABEL_SIZE && label[n] != '\0') {
        n++;
    }
    return n == id_len && memcmp(label, id, n) == 0;
}

int PS4_SYSV_ABI AppContent_Z5dummyv(void) {
    c_log(C_LOG_LIB_APP_CONTENT, C_LOG_LEVEL_ERROR, "_Z5dummyv", "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI sceAppContentAddcontDelete(void) {
    c_log(C_LOG_LIB_APP_CONTENT, C_LOG_LEVEL_ERROR, "sceAppContentAddcontDelete",
          "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI sceAppContentAddcontEnqueueDownload(void) {
    c_log(C_LOG_LIB_APP_CONTENT, C_LOG_LEVEL_ERROR, "sceAppContentAddcontEnqueueDownload",
          "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI sceAppContentAddcontEnqueueDownloadSp(void) {
    c_log(C_LOG_LIB_APP_CONTENT, C_LOG_LEVEL_ERROR, "sceAppContentAddcontEnqueueDownloadSp",
          "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI sceAppContentAddcontShrink(void) {
    c_log(C_LOG_LIB_APP_CONTENT, C_LOG_LEVEL_ERROR, "sceAppContentAddcontShrink",
          "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI sceAppContentAppParamGetString(void) {
    c_log(C_LOG_LIB_APP_CONTENT, C_LOG_LEVEL_ERROR, "sceAppContentAppParamGetString",
          "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI sceAppContentDownload0Expand(void) {
    c_log(C_LOG_LIB_APP_CONTENT, C_LOG_LEVEL_ERROR, "sceAppContentDownload0Expand",
          "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI sceAppContentDownload0Shrink(void) {
    c_log(C_LOG_LIB_APP_CONTENT, C_LOG_LEVEL_ERROR, "sceAppContentDownload0Shrink",
          "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI sceAppContentDownload1Expand(void) {
    c_log(C_LOG_LIB_APP_CONTENT, C_LOG_LEVEL_ERROR, "sceAppContentDownload1Expand",
          "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI sceAppContentDownload1Shrink(void) {
    c_log(C_LOG_LIB_APP_CONTENT, C_LOG_LEVEL_ERROR, "sceAppContentDownload1Shrink",
          "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI sceAppContentDownloadDataFormat(void) {
    c_log(C_LOG_LIB_APP_CONTENT, C_LOG_LEVEL_ERROR, "sceAppContentDownloadDataFormat",
          "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI sceAppContentGetAddcontDownloadProgress(void) {
    c_log(C_LOG_LIB_APP_CONTENT, C_LOG_LEVEL_ERROR, "sceAppContentGetAddcontDownloadProgress",
          "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI sceAppContentGetRegion(void) {
    c_log(C_LOG_LIB_APP_CONTENT, C_LOG_LEVEL_ERROR, "sceAppContentGetRegion", "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI sceAppContentRequestPatchInstall(void) {
    c_log(C_LOG_LIB_APP_CONTENT, C_LOG_LEVEL_ERROR, "sceAppContentRequestPatchInstall",
          "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI sceAppContentSmallSharedDataFormat(void) {
    c_log(C_LOG_LIB_APP_CONTENT, C_LOG_LEVEL_ERROR, "sceAppContentSmallSharedDataFormat",
          "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI sceAppContentSmallSharedDataGetAvailableSpaceKb(void) {
    c_log(C_LOG_LIB_APP_CONTENT, C_LOG_LEVEL_ERROR,
          "sceAppContentSmallSharedDataGetAvailableSpaceKb", "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI sceAppContentSmallSharedDataMount(void) {
    c_log(C_LOG_LIB_APP_CONTENT, C_LOG_LEVEL_ERROR, "sceAppContentSmallSharedDataMount",
          "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI sceAppContentSmallSharedDataUnmount(void) {
    c_log(C_LOG_LIB_APP_CONTENT, C_LOG_LEVEL_ERROR, "sceAppContentSmallSharedDataUnmount",
          "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI sceAppContentTemporaryDataFormat(void) {
    c_log(C_LOG_LIB_APP_CONTENT, C_LOG_LEVEL_ERROR, "sceAppContentTemporaryDataFormat",
          "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI sceAppContentGetPftFlag(void) {
    c_log(C_LOG_LIB_APP_CONTENT, C_LOG_LEVEL_ERROR, "sceAppContentGetPftFlag", "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI AppContent_Func_C59A36FF8D7C59DA(void) {
    c_log(C_LOG_LIB_APP_CONTENT, C_LOG_LEVEL_ERROR, "Func_C59A36FF8D7C59DA", "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI sceAppContentAddcontEnqueueDownloadByEntitlementId(void) {
    c_log(C_LOG_LIB_APP_CONTENT, C_LOG_LEVEL_ERROR,
          "sceAppContentAddcontEnqueueDownloadByEntitlementId", "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI sceAppContentAddcontMountByEntitlementId(void) {
    c_log(C_LOG_LIB_APP_CONTENT, C_LOG_LEVEL_ERROR, "sceAppContentAddcontMountByEntitlementId",
          "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI sceAppContentGetAddcontInfoByEntitlementId(void) {
    c_log(C_LOG_LIB_APP_CONTENT, C_LOG_LEVEL_ERROR, "sceAppContentGetAddcontInfoByEntitlementId",
          "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI sceAppContentGetAddcontInfoListByIroTag(void) {
    c_log(C_LOG_LIB_APP_CONTENT, C_LOG_LEVEL_ERROR, "sceAppContentGetAddcontInfoListByIroTag",
          "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI sceAppContentGetDownloadedStoreCountry(void) {
    c_log(C_LOG_LIB_APP_CONTENT, C_LOG_LEVEL_ERROR, "sceAppContentGetDownloadedStoreCountry",
          "(STUBBED) called");
    return ORBIS_OK;
}

/* sceAppContentAddcontMount: the content of the entitlement asked for, looked for among the
 * add-on folders' */
typedef struct addcont_mount_find {
    const OrbisNpUnifiedEntitlementLabel* entitlement_label;
    OrbisAppContentMountPoint* mount_point;
    bool mounted;
} addcont_mount_find;

static bool MountIfEntitlement(const char* path, void* ctx) {
    addcont_mount_find* f = (addcont_mount_find*)ctx;
    fs_addcont_t content;
    const char* entitlement_id;
    size_t entitlement_len;
    /* A temporary backend to view the param.sfo; a PKG that cannot be read (no keys, cut
     * short) has none. Or this content has no param.sfo, or the param.sfo is invalid. */
    if (fs_read_addcont(path, false, &content) != FS_ADDCONT_OK) {
        return true;
    }
    /* Validate the available params */
    if (!content.has_category || strncmp(content.category, "ac", 2) != 0 ||
        !content.has_content_id ||
        content.content_id_length <= ORBIS_APP_CONTENT_ENTITLEMENT_LABEL_OFFSET) {
        /* This content fails the error checks performed in sceAppContentInitialize. */
        return true;
    }
    entitlement_id = content.content_id + ORBIS_APP_CONTENT_ENTITLEMENT_LABEL_OFFSET;
    entitlement_len = strlen(entitlement_id);
    if (strncmp(entitlement_id, f->entitlement_label->data, entitlement_len) != 0) {
        /* The entitlement ID is incorrect. */
        return true;
    }
    /* We've located the correct folder. */
    fs_mount(path, f->mount_point->data, true);
    f->mounted = true;
    return false;
}

int PS4_SYSV_ABI sceAppContentAddcontMount(u32 service_label,
                                           const OrbisNpUnifiedEntitlementLabel* entitlement_label,
                                           OrbisAppContentMountPoint* mount_point) {
    addcont_mount_find find;
    s32 i;
    c_log(C_LOG_LIB_APP_CONTENT, C_LOG_LEVEL_INFO, "sceAppContentAddcontMount", "called");

    if (!is_initialized) {
        return ORBIS_APP_CONTENT_ERROR_NOT_INITIALIZED;
    }

    if (service_label > 7 || entitlement_label == NULL || mount_point == NULL) {
        return ORBIS_APP_CONTENT_ERROR_PARAMETER;
    }

    /* Determine which loaded additional content this entitlement label is for. */
    i = 0;
    while (i < addcont_count) {
        if (addcont_info[i].service_label == service_label &&
            strncmp(entitlement_label->data, addcont_info[i].entitlement_label,
                    ORBIS_NP_UNIFIED_ENTITLEMENT_LABEL_SIZE - 1) == 0) {
            snprintf(mount_point->data, ORBIS_APP_CONTENT_MOUNTPOINT_DATA_MAXSIZE, "/addcont%d",
                     (int)i);
            break;
        }
        ++i;
    }

    if (i == addcont_count) {
        /* None of the loaded additional content match the entitlement label requested. */
        return ORBIS_APP_CONTENT_ERROR_NOT_FOUND;
    }

    /* Find which directory corresponds to this entitlement */
    find.entitlement_label = entitlement_label;
    find.mount_point = mount_point;
    find.mounted = false;
    for (i = 0; i < addon_path_count && !find.mounted; i++) {
        fs_for_each_addcont(addon_paths[i], i == 0, MountIfEntitlement, &find);
    }
    if (find.mounted) {
        return ORBIS_OK;
    }

    /* Hitting this shouldn't be possible, as it would mean the entitlement was loaded,
     * but the folder it was loaded from doesn't exist. */
    c_unreachable(__FILE__, __LINE__, "sceAppContentAddcontMount",
                  "Folder for loaded entitlement label doesn't exist.");
    return ORBIS_APP_CONTENT_ERROR_NOT_FOUND;
}

/* The mount point's text: up to its 16 bytes */
static int MountPointLength(const OrbisAppContentMountPoint* mount_point) {
    int n = 0;
    while (n < (int)sizeof(mount_point->data) && mount_point->data[n] != '\0') {
        n++;
    }
    return n;
}

/* Unmount takes a terminated string: the mount point's text, cut to its 16 bytes */
static void UnmountPoint(const OrbisAppContentMountPoint* mount_point) {
    char mp[ORBIS_APP_CONTENT_MOUNTPOINT_DATA_MAXSIZE + 1];
    const int n = MountPointLength(mount_point);
    memcpy(mp, mount_point->data, (size_t)n);
    mp[n] = '\0';
    fs_unmount(mp);
}

int PS4_SYSV_ABI sceAppContentAddcontUnmount(OrbisAppContentMountPoint* mount_point) {
    if (!is_initialized) {
        c_log(C_LOG_LIB_APP_CONTENT, C_LOG_LEVEL_ERROR, "sceAppContentAddcontUnmount",
              "called before initialization");
        return ORBIS_APP_CONTENT_ERROR_NOT_INITIALIZED;
    }

    if (!mount_point) {
        c_log(C_LOG_LIB_APP_CONTENT, C_LOG_LEVEL_ERROR, "sceAppContentAddcontUnmount",
              "mount_point is null");
        return ORBIS_APP_CONTENT_ERROR_PARAMETER;
    }

    c_log(C_LOG_LIB_APP_CONTENT, C_LOG_LEVEL_INFO, "sceAppContentAddcontUnmount",
          "called, mount_point = %.*s", MountPointLength(mount_point), mount_point->data);
    UnmountPoint(mount_point);
    return ORBIS_OK;
}

int PS4_SYSV_ABI sceAppContentAppParamGetInt(OrbisAppContentAppParamId paramId, s32* out_value) {
    s32 value = 0;
    if (out_value == NULL) {
        return ORBIS_APP_CONTENT_ERROR_PARAMETER;
    }
    switch (paramId) {
    case ORBIS_APP_CONTENT_APPPARAM_ID_SKU_FLAG:
        value = ORBIS_APP_CONTENT_APPPARAM_SKU_FLAG_FULL;
        break;
    case ORBIS_APP_CONTENT_APPPARAM_ID_USER_DEFINED_PARAM_1:
        psf_game_get_integer("USER_DEFINED_PARAM_1", &value);
        break;
    case ORBIS_APP_CONTENT_APPPARAM_ID_USER_DEFINED_PARAM_2:
        psf_game_get_integer("USER_DEFINED_PARAM_2", &value);
        break;
    case ORBIS_APP_CONTENT_APPPARAM_ID_USER_DEFINED_PARAM_3:
        psf_game_get_integer("USER_DEFINED_PARAM_3", &value);
        break;
    case ORBIS_APP_CONTENT_APPPARAM_ID_USER_DEFINED_PARAM_4:
        psf_game_get_integer("USER_DEFINED_PARAM_4", &value);
        break;
    default:
        c_log(C_LOG_LIB_APP_CONTENT, C_LOG_LEVEL_ERROR, "sceAppContentAppParamGetInt",
              " paramId = %u paramId is not valid", (unsigned)paramId);
        return ORBIS_APP_CONTENT_ERROR_PARAMETER;
    }
    *out_value = value;
    return ORBIS_OK;
}

int PS4_SYSV_ABI sceAppContentDownloadDataGetAvailableSpaceKb(OrbisAppContentMountPoint* mountPoint,
                                                              u64* availableSpaceKb) {
    (void)mountPoint;
    c_log(C_LOG_LIB_APP_CONTENT, C_LOG_LEVEL_ERROR, "sceAppContentDownloadDataGetAvailableSpaceKb",
          "(STUBBED) called");
    *availableSpaceKb = 1048576;
    return ORBIS_OK;
}

int PS4_SYSV_ABI sceAppContentGetAddcontInfo(u32 service_label,
                                             const OrbisNpUnifiedEntitlementLabel* entitlementLabel,
                                             OrbisAppContentAddcontInfo* info) {
    s32 i;
    c_log(C_LOG_LIB_APP_CONTENT, C_LOG_LEVEL_INFO, "sceAppContentGetAddcontInfo", "called");

    if (!is_initialized) {
        return ORBIS_APP_CONTENT_ERROR_NOT_INITIALIZED;
    }

    if (service_label > 7 || entitlementLabel == NULL || info == NULL) {
        return ORBIS_APP_CONTENT_ERROR_PARAMETER;
    }

    for (i = 0; i < addcont_count; i++) {
        if (addcont_info[i].service_label != service_label ||
            strncmp(entitlementLabel->data, addcont_info[i].entitlement_label,
                    ORBIS_NP_UNIFIED_ENTITLEMENT_LABEL_SIZE - 1) != 0) {
            continue;
        }

        c_log(C_LOG_LIB_APP_CONTENT, C_LOG_LEVEL_INFO, "sceAppContentGetAddcontInfo",
              "found DLC %s", entitlementLabel->data);

        strncpy(info->entitlement_label.data, addcont_info[i].entitlement_label,
                ORBIS_NP_UNIFIED_ENTITLEMENT_LABEL_SIZE);
        info->status = addcont_info[i].status;
        return ORBIS_OK;
    }

    return ORBIS_APP_CONTENT_ERROR_DRM_NO_ENTITLEMENT;
}

int PS4_SYSV_ABI sceAppContentGetAddcontInfoList(u32 service_label,
                                                 OrbisAppContentAddcontInfo* list, u32 list_num,
                                                 u32* hit_num) {
    u32 matching_count = 0;
    u32 dlcs_to_list;
    u32 written = 0;
    s32 i;
    c_log(C_LOG_LIB_APP_CONTENT, C_LOG_LEVEL_INFO, "sceAppContentGetAddcontInfoList", "called");

    if (!is_initialized) {
        return ORBIS_APP_CONTENT_ERROR_NOT_INITIALIZED;
    }

    if (service_label > 7) {
        return ORBIS_APP_CONTENT_ERROR_PARAMETER;
    }

    for (i = 0; i < addcont_count; i++) {
        if (addcont_info[i].service_label == service_label) {
            matching_count++;
        }
    }

    if (list_num == 0 || list == NULL) {
        if (hit_num == NULL) {
            return ORBIS_APP_CONTENT_ERROR_PARAMETER;
        }

        *hit_num = matching_count;
        return ORBIS_OK;
    }

    dlcs_to_list = matching_count < list_num ? matching_count : list_num;
    for (i = 0; i < addcont_count && written < dlcs_to_list; i++) {
        if (addcont_info[i].service_label == service_label) {
            strncpy(list[written].entitlement_label.data, addcont_info[i].entitlement_label,
                    ORBIS_NP_UNIFIED_ENTITLEMENT_LABEL_SIZE);
            list[written].status = addcont_info[i].status;
            written++;
        }
    }

    if (hit_num != NULL) {
        *hit_num = dlcs_to_list;
    }

    return ORBIS_OK;
}

int PS4_SYSV_ABI sceAppContentGetEntitlementKey(
    u32 service_label, const OrbisNpUnifiedEntitlementLabel* entitlement_label,
    OrbisAppContentGetEntitlementKey* key) {
    s32 i;
    c_log(C_LOG_LIB_APP_CONTENT, C_LOG_LEVEL_INFO, "sceAppContentGetEntitlementKey", "called");

    if (!is_initialized) {
        return ORBIS_APP_CONTENT_ERROR_NOT_INITIALIZED;
    }

    if (service_label > 7) {
        return ORBIS_APP_CONTENT_ERROR_PARAMETER;
    }

    if (entitlement_label == NULL || key == NULL) {
        return ORBIS_APP_CONTENT_ERROR_PARAMETER;
    }

    for (i = 0; i < addcont_count; i++) {
        if (addcont_info[i].service_label != service_label ||
            strncmp(entitlement_label->data, addcont_info[i].entitlement_label,
                    ORBIS_NP_UNIFIED_ENTITLEMENT_LABEL_SIZE - 1) != 0) {
            continue;
        }

        memcpy(key->data, addcont_info[i].key.data, ORBIS_APP_CONTENT_ENTITLEMENT_KEY_SIZE);
        return ORBIS_OK;
    }

    return ORBIS_APP_CONTENT_ERROR_DRM_NO_ENTITLEMENT;
}

static bool CanAddEntitlement(u32 service_label, const char* entitlement_id, size_t id_len) {
    s32 i;
    if (addcont_count >= ORBIS_APP_CONTENT_INFO_LIST_MAX_SIZE) {
        c_log(C_LOG_LIB_APP_CONTENT, C_LOG_LEVEL_ERROR, "CanAddEntitlement",
              "Too many entitlements, ignoring %.*s", (int)id_len, entitlement_id);
        return false;
    }
    for (i = 0; i < addcont_count; i++) {
        if (addcont_info[i].service_label != service_label) {
            continue;
        }
        if (LabelIs(addcont_info[i].entitlement_label, entitlement_id, id_len)) {
            return false;
        }
    }
    return true;
}

/* A new entitlement's entry: the first @id_len bytes of its label, as many as the entry holds,
 * and the rest zero */
static AddContInfo* AddEntitlement(u32 service_label, const char* entitlement_id, size_t id_len,
                                   u32 status) {
    AddContInfo* info = &addcont_info[addcont_count++];
    memset(info, 0, sizeof(*info));
    info->service_label = service_label;
    if (id_len > sizeof(info->entitlement_label)) {
        id_len = sizeof(info->entitlement_label);
    }
    memcpy(info->entitlement_label, entitlement_id, id_len);
    info->status = status;
    return info;
}

/* LoadAddonFolder: each piece of additional content in an add-on folder */
static bool LoadAddcont(const char* path, void* ctx) {
    const u32 service_label = *(const u32*)ctx;
    fs_addcont_t content;
    const char* entitlement_id;
    size_t id_len;
    AddContInfo* info;
    const s32 read = fs_read_addcont(path, true, &content);

    /* A PKG that cannot be read (no keys, cut short) has no backend. */
    if (read == FS_ADDCONT_NO_BACKEND) {
        return true;
    }
    if (read == FS_ADDCONT_NO_PARAM_SFO) {
        /* This content doesn't have a param.sfo */
        c_log(C_LOG_LIB_APP_CONTENT, C_LOG_LEVEL_WARNING, "LoadAddonFolder",
              "Additional content %s has no param.sfo", path_basename(path));
        return true;
    }
    if (read == FS_ADDCONT_BAD_PARAM_SFO) {
        /* The param.sfo is invalid */
        c_log(C_LOG_LIB_APP_CONTENT, C_LOG_LEVEL_WARNING, "LoadAddonFolder",
              "Additional content %s has an invalid param.sfo", path_basename(path));
        return true;
    }

    if (content.has_category && strncmp(content.category, "ac", 2) != 0) {
        c_log(C_LOG_LIB_APP_CONTENT, C_LOG_LEVEL_WARNING, "LoadAddonFolder",
              "Additonal content %s is not additional content", path_basename(path));
        return true;
    }

    /* Find the entitlement id from the content id. */
    if (!content.has_content_id) {
        c_log(C_LOG_LIB_APP_CONTENT, C_LOG_LEVEL_WARNING, "LoadAddonFolder",
              "Additonal content %s param.sfo is missing CONTENT_ID", path_basename(path));
        return true;
    }

    /* Content IDs have consistent formatting, so this will always work.
     * They follow the format UPXXXX-CUSAXXXXX_XX-entitlement */
    if (content.content_id_length <= ORBIS_APP_CONTENT_ENTITLEMENT_LABEL_OFFSET) {
        c_log(C_LOG_LIB_APP_CONTENT, C_LOG_LEVEL_WARNING, "LoadAddonFolder",
              "Additonal content %s param.sfo has malformed CONTENT_ID", path_basename(path));
        return true;
    }

    entitlement_id = content.content_id + ORBIS_APP_CONTENT_ENTITLEMENT_LABEL_OFFSET;
    id_len = strlen(entitlement_id);
    c_log(C_LOG_LIB_APP_CONTENT, C_LOG_LEVEL_INFO, "LoadAddonFolder",
          "Entitlement %.*s found for service_label %u", (int)id_len, entitlement_id,
          (unsigned)service_label);

    /* Save the additional content info in addcont_info. */
    if (!CanAddEntitlement(service_label, entitlement_id, id_len)) {
        return true;
    }
    info = AddEntitlement(service_label, entitlement_id, id_len,
                          ORBIS_APP_CONTENT_ADDCONT_DOWNLOAD_STATUS_INSTALLED);

    /* The entitlement key, from license.info or else license.dat, if available */
    memcpy(info->key.data, content.key, sizeof(info->key.data));
    if (!content.has_key) {
        c_log(C_LOG_LIB_APP_CONTENT, C_LOG_LEVEL_WARNING, "LoadAddonFolder",
              "Could not load entitlement key for %.*s", (int)id_len, entitlement_id);
    }
    return true;
}

static void LoadAddonFolder(const char* addon_path, u32 service_label) {
    /* The game's own folder may not exist and still have PKGs beside the game. */
    fs_for_each_addcont(addon_path, service_label == 0, LoadAddcont, &service_label);
}

/* LoadLicenses: each license-only entitlement */
static void LoadLicenseEntitlement(const char* label, size_t length, const u8* key, void* ctx) {
    const u32 service_label = *(const u32*)ctx;
    AddContInfo* info;
    c_log(C_LOG_LIB_APP_CONTENT, C_LOG_LEVEL_INFO, "LoadLicenses",
          "License-only entitlement %s found for service_label %u", label, (unsigned)service_label);
    if (!CanAddEntitlement(service_label, label, length)) {
        return;
    }
    info = AddEntitlement(service_label, label, length,
                          ORBIS_APP_CONTENT_ADDCONT_DOWNLOAD_STATUS_NO_EXTRA_DATA);
    memcpy(info->key.data, key, sizeof(info->key.data));
}

/* Which license LoadLicenses opened, for its log */
typedef struct app_content_license {
    u32 service_label; /* first: LoadLicenseEntitlement reads it */
    const char* service_id;
} app_content_license;

static void LoadLicensesOpened(void* ctx) {
    const app_content_license* l = (const app_content_license*)ctx;
    c_log(C_LOG_LIB_APP_CONTENT, C_LOG_LEVEL_INFO, "LoadLicenses",
          "Checking license %s for additional entitlements", l->service_id);
}

static void LoadLicenses(const char* service_id, u32 service_label) {
    app_content_license license;
    char licenses_dir[PATH_MAX_LENGTH];
    char name[APP_CONTENT_ID_SIZE + 8];
    char ridx_path[PATH_MAX_LENGTH];
    char rifa_path[PATH_MAX_LENGTH];

    c_get_user_path(C_USER_PATH_LICENSES, licenses_dir, sizeof(licenses_dir));
    snprintf(name, sizeof(name), "%s.idx", service_id);
    fill_pathname_join_special(ridx_path, licenses_dir, name, sizeof(ridx_path));
    snprintf(name, sizeof(name), "%s.rif", service_id);
    fill_pathname_join_special(rifa_path, licenses_dir, name, sizeof(rifa_path));

    if (!path_is_valid(ridx_path) || !path_is_valid(rifa_path)) {
        return;
    }

    license.service_label = service_label;
    license.service_id = service_id;
    if (!rif_for_each_license_only_entitlement(ridx_path, rifa_path, LoadLicensesOpened,
                                               LoadLicenseEntitlement, &license)) {
        c_log(C_LOG_LIB_APP_CONTENT, C_LOG_LEVEL_ERROR, "LoadLicenses",
              "Could not open license file for %s", service_id);
    }
}

typedef struct app_content_service {
    u32 service_label;
    char service_id[APP_CONTENT_ID_SIZE];
} app_content_service;

int PS4_SYSV_ABI sceAppContentInitialize(const OrbisAppContentInitParam* initParam,
                                         OrbisAppContentBootParam* bootParam) {
    char addons_dir[PATH_MAX_LENGTH];
    char content_id[APP_CONTENT_ID_SIZE];
    app_content_service services[APP_CONTENT_SERVICES];
    s32 service_count = 0;
    u32 i;
    s32 s;
    (void)initParam;
    (void)bootParam;

    if (sdk_ver >= KERNEL_FW_150 && is_initialized) {
        c_log(C_LOG_LIB_APP_CONTENT, C_LOG_LEVEL_ERROR, "sceAppContentInitialize",
              "Already initialized");
        return ORBIS_APP_CONTENT_ERROR_BUSY;
    }

    c_log(C_LOG_LIB_APP_CONTENT, C_LOG_LEVEL_WARNING, "sceAppContentInitialize", "(DUMMY) called");
    is_initialized = true;

    emu_settings_get_addon_install_dir(addons_dir, sizeof(addons_dir));
    if (!psf_game_get_string("TITLE_ID", title_id, sizeof(title_id), NULL)) {
        c_unreachable(__FILE__, __LINE__, "sceAppContentInitialize", "Failed to get TITLE_ID");
    }

    /* The game's own service: its content ID up to its last '-' (all of it, with none) */
    if (psf_game_get_string("CONTENT_ID", content_id, sizeof(content_id), NULL)) {
        char* last_dash = strrchr(content_id, '-');
        if (last_dash != NULL) {
            *last_dash = '\0';
        }
        services[service_count].service_label = 0;
        strlcpy(services[service_count].service_id, content_id,
                sizeof(services[service_count].service_id));
        service_count++;
    }

    for (i = 1; i <= 7; i++) {
        char key[32];
        char service_id[APP_CONTENT_ID_SIZE];
        size_t length = 0;
        snprintf(key, sizeof(key), "SERVICE_ID_ADDCONT_ADD_%u", (unsigned)i);
        if (psf_game_get_string(key, service_id, sizeof(service_id), &length) && length != 0) {
            c_log(C_LOG_LIB_APP_CONTENT, C_LOG_LEVEL_INFO, "sceAppContentInitialize",
                  "Linked additional content service %s at slot %u", service_id, (unsigned)i);
            services[service_count].service_label = i;
            strlcpy(services[service_count].service_id, service_id,
                    sizeof(services[service_count].service_id));
            service_count++;
        }
    }

    /* Additional content folders: the game's own title ID first, then the title ID of each
     * linked service (service IDs look like UPXXXX-CUSAXXXXX_XX). */
    addon_path_count = 0;
    addcont_count = 0;
    for (s = 0; s < service_count; s++) {
        const app_content_service* srv = &services[s];
        char path[PATH_MAX_LENGTH];
        path[0] = '\0';
        if (srv->service_label == 0) {
            fill_pathname_join_special(path, addons_dir, title_id, sizeof(path));
        } else if (strlen(srv->service_id) >= 16) {
            char service_title[10];
            memcpy(service_title, srv->service_id + 7, 9);
            service_title[9] = '\0';
            fill_pathname_join_special(path, addons_dir, service_title, sizeof(path));
        }
        /* The game's own folder is looked at even when it does not exist: the PKGs
         * beside the game's PKG count as its additional content. */
        if (path[0] != '\0' && (srv->service_label == 0 || path_is_valid(path))) {
            s32 p;
            bool known = false;
            for (p = 0; p < addon_path_count; p++) {
                if (strcmp(addon_paths[p], path) == 0) {
                    known = true;
                    break;
                }
            }
            if (!known) {
                strlcpy(addon_paths[addon_path_count++], path, sizeof(addon_paths[0]));
            }
            LoadAddonFolder(path, srv->service_label);
        }
        LoadLicenses(srv->service_id, srv->service_label);
    }

    if (addcont_count > 0) {
        system_service_push_entitlement_update();
    }

    return ORBIS_OK;
}

int PS4_SYSV_ABI sceAppContentTemporaryDataGetAvailableSpaceKb(
    const OrbisAppContentMountPoint* mountPoint, u64* availableSpaceKb) {
    (void)mountPoint;
    c_log(C_LOG_LIB_APP_CONTENT, C_LOG_LEVEL_ERROR, "sceAppContentTemporaryDataGetAvailableSpaceKb",
          "(STUBBED) called");
    *availableSpaceKb = 1048576;
    return ORBIS_OK;
}

int PS4_SYSV_ABI sceAppContentTemporaryDataMount(OrbisAppContentMountPoint* mountPoint) {
    return sceAppContentTemporaryDataMount2(ORBIS_APP_CONTENT_TEMPORARY_DATA_OPTION_FORMAT,
                                            mountPoint);
}

int PS4_SYSV_ABI sceAppContentTemporaryDataMount2(OrbisAppContentTemporaryDataOption option,
                                                  OrbisAppContentMountPoint* mountPoint) {
    static const char TmpMount[] = "/temp0";
    char tid[APP_CONTENT_ID_SIZE];
    char temp_dir[PATH_MAX_LENGTH];
    char mount_temp_dir[PATH_MAX_LENGTH];
    if (!is_initialized) {
        return ORBIS_APP_CONTENT_ERROR_NOT_INITIALIZED;
    }

    if (mountPoint == NULL) {
        return ORBIS_APP_CONTENT_ERROR_PARAMETER;
    }

    if (title_id[0] != '\0') {
        strlcpy(tid, title_id, sizeof(tid));
    } else if (!psf_game_get_string("TITLE_ID", tid, sizeof(tid), NULL)) {
        tid[0] = '\0';
    }
    c_get_user_path(C_USER_PATH_TEMP_DATA, temp_dir, sizeof(temp_dir));
    fill_pathname_join_special(mount_temp_dir, temp_dir, tid, sizeof(mount_temp_dir));

    memcpy(mountPoint->data, TmpMount, sizeof(TmpMount));

    fs_mount_made_folder(mount_temp_dir, option == ORBIS_APP_CONTENT_TEMPORARY_DATA_OPTION_FORMAT,
                         TmpMount);

    c_log(C_LOG_LIB_APP_CONTENT, C_LOG_LEVEL_INFO, "sceAppContentTemporaryDataMount2",
          "option = %u, mountPoint = %s", (unsigned)option, mountPoint->data);
    return ORBIS_OK;
}

int PS4_SYSV_ABI sceAppContentTemporaryDataUnmount(const OrbisAppContentMountPoint* mountPoint) {
    if (!is_initialized) {
        return ORBIS_APP_CONTENT_ERROR_NOT_INITIALIZED;
    }

    if (mountPoint == NULL) {
        return ORBIS_APP_CONTENT_ERROR_PARAMETER;
    }
    c_log(C_LOG_LIB_APP_CONTENT, C_LOG_LEVEL_INFO, "sceAppContentTemporaryDataUnmount",
          "mountPoint = %.*s", MountPointLength(mountPoint), mountPoint->data);
    UnmountPoint(mountPoint);
    return ORBIS_OK;
}

void app_content_library_init(void) {
    C_ASSERT_MSG(kernel_get_compiled_sdk_version(&sdk_ver) == 0, "app_content_library_init",
                 "Failed to get SDK version");
}
