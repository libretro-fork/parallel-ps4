/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <compat/strl.h>
#include <file/file_path.h>
#include <retro_dirent.h>

#include "common/c_bridge.h"
#include "common/combine.h"
#include "core/libraries/error_codes.h"
#include "core/libraries/usbd/usbstorage.h"

/* Directory listings open at once: no allocation for the table, so a game with more open
 * than this has the newest refused */
#define USB_GETDENTS_MAX 64

#define USB_PATH_MAX 4096

/* A listing of the stick's folder, as it was when opened: its entries' names one after
 * another, each with its terminator */
typedef struct usb_getdents_state {
    s32 fd; /* 0: a free slot */
    char* names;
    u8* is_directory;
    u32 count;
    u32 current_index;
} usb_getdents_state_t;

static combiner_t g_usb_getdents_mutex;
static usb_getdents_state_t g_usb_getdents_table[USB_GETDENTS_MAX];
static s32 g_usb_next_fd = 1;

void usb_storage_library_init(void) {
    combiner_init(&g_usb_getdents_mutex);
}

void usb_storage_mount_path(u32 device_id, char* out, size_t size) {
    char user[USB_PATH_MAX];
    char mnt[USB_PATH_MAX];
    char usb[32];
    c_get_user_path(C_USER_PATH_USER, user, sizeof(user));
    fill_pathname_join(mnt, user, "mnt", sizeof(mnt));
    snprintf(usb, sizeof(usb), "usb%u", device_id);
    fill_pathname_join(out, mnt, usb, size);
}

s32 PS4_SYSV_ABI sceUsbStorageInit(void* attr) {
    (void)attr;
    c_log(C_LOG_LIB_USBD, C_LOG_LEVEL_DEBUG, "sceUsbStorageInit", "called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceUsbStorageTerm(void) {
    c_log(C_LOG_LIB_USBD, C_LOG_LEVEL_DEBUG, "sceUsbStorageTerm", "called");
    return ORBIS_OK;
}

static int usb_event_type_ok(const char* func, s32 event_type, void* callback) {
    if (!callback) {
        c_log(C_LOG_LIB_USBD, C_LOG_LEVEL_ERROR, func, "callback is nullptr");
        return 0;
    }
    if (event_type != 1 && event_type != 2) {
        c_log(C_LOG_LIB_USBD, C_LOG_LEVEL_ERROR, func, "event_type %d is invalid", event_type);
        return 0;
    }
    return 1;
}

s32 PS4_SYSV_ABI sceUsbStorageRegisterCallback(s32 event_type, void* callback, void* arg) {
    if (!usb_event_type_ok("sceUsbStorageRegisterCallback", event_type, callback)) {
        return ORBIS_AUTO_MOUNTER_ERROR_INVAL;
    }
    c_log(C_LOG_LIB_USBD, C_LOG_LEVEL_ERROR, "sceUsbStorageRegisterCallback",
          "(STUBBED) called: event_type = %d, callback = 0x%llx, arg = 0x%llx", event_type,
          (unsigned long long)(uintptr_t)callback, (unsigned long long)(uintptr_t)arg);
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceUsbStorageUnregisterCallback(s32 event_type, void* callback) {
    if (!usb_event_type_ok("sceUsbStorageUnregisterCallback", event_type, callback)) {
        return ORBIS_AUTO_MOUNTER_ERROR_INVAL;
    }
    c_log(C_LOG_LIB_USBD, C_LOG_LEVEL_ERROR, "sceUsbStorageUnregisterCallback",
          "(STUBBED) called: event_type = %d, callback = 0x%llx", event_type,
          (unsigned long long)(uintptr_t)callback);
    return ORBIS_OK;
}

static s32 usb_map_callback(const char* func, u32 device_id, const char* directory, void* callback,
                            void* arg, int has_arg) {
    if (!directory || !callback) {
        c_log(C_LOG_LIB_USBD, C_LOG_LEVEL_ERROR, func, "directory or callback is null");
        return ORBIS_AUTO_MOUNTER_ERROR_INVAL;
    }
    if (has_arg) {
        c_log(C_LOG_LIB_USBD, C_LOG_LEVEL_ERROR, func,
              "(STUBBED) called: device_id = %u, directory = %s, callback = 0x%llx, arg = 0x%llx",
              device_id, directory, (unsigned long long)(uintptr_t)callback,
              (unsigned long long)(uintptr_t)arg);
    } else {
        c_log(C_LOG_LIB_USBD, C_LOG_LEVEL_ERROR, func,
              "(STUBBED) called: device_id = %u, directory = %s, callback = 0x%llx", device_id,
              directory, (unsigned long long)(uintptr_t)callback);
    }
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceUsbStorageRegisterCallbackForMapAvailable(u32 device_id, const char* directory,
                                                              void* callback, void* arg) {
    return usb_map_callback("sceUsbStorageRegisterCallbackForMapAvailable", device_id, directory,
                            callback, arg, 1);
}

s32 PS4_SYSV_ABI sceUsbStorageUnregisterCallbackForMapAvailable(u32 device_id,
                                                                const char* directory,
                                                                void* callback) {
    return usb_map_callback("sceUsbStorageUnregisterCallbackForMapAvailable", device_id, directory,
                            callback, NULL, 0);
}

s32 PS4_SYSV_ABI sceUsbStorageRegisterCallbackForMapUnavailable(u32 device_id,
                                                                const char* directory,
                                                                void* callback, void* arg) {
    return usb_map_callback("sceUsbStorageRegisterCallbackForMapUnavailable", device_id, directory,
                            callback, arg, 1);
}

s32 PS4_SYSV_ABI sceUsbStorageUnregisterCallbackForMapUnavailable(u32 device_id,
                                                                  const char* directory,
                                                                  void* callback) {
    return usb_map_callback("sceUsbStorageUnregisterCallbackForMapUnavailable", device_id,
                            directory, callback, NULL, 0);
}

static int usb_exists(u32 device_id) {
    char path[USB_PATH_MAX];
    usb_storage_mount_path(device_id, path, sizeof(path));
    return path_is_valid(path);
}

s32 PS4_SYSV_ABI sceUsbStorageGetDeviceList(u32* device_ids, u32* count) {
    if (!device_ids || !count) {
        c_log(C_LOG_LIB_USBD, C_LOG_LEVEL_ERROR, "sceUsbStorageGetDeviceList",
              "device_ids or count pointer is null");
        return ORBIS_AUTO_MOUNTER_ERROR_INVAL;
    }
    if (!usb_exists(0)) {
        *count = 0;
        c_log(C_LOG_LIB_USBD, C_LOG_LEVEL_DEBUG, "sceUsbStorageGetDeviceList",
              "no USB devices found");
        return ORBIS_OK;
    }
    device_ids[0] = 0;
    *count = 1;
    c_log(C_LOG_LIB_USBD, C_LOG_LEVEL_INFO, "sceUsbStorageGetDeviceList",
          "found 1 USB device (id = 0)");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceUsbStorageGetDeviceInfo(u32 device_id, OrbisUsbStorageDeviceInfo* info) {
    if (!info) {
        c_log(C_LOG_LIB_USBD, C_LOG_LEVEL_ERROR, "sceUsbStorageGetDeviceInfo",
              "info pointer is null");
        return ORBIS_AUTO_MOUNTER_ERROR_INVAL;
    }
    if (device_id != 0) {
        c_log(C_LOG_LIB_USBD, C_LOG_LEVEL_ERROR, "sceUsbStorageGetDeviceInfo",
              "invalid device_id %u", device_id);
        return ORBIS_AUTO_MOUNTER_ERROR_NOENT;
    }
    if (!usb_exists(device_id)) {
        c_log(C_LOG_LIB_USBD, C_LOG_LEVEL_ERROR, "sceUsbStorageGetDeviceInfo",
              "USB device %u mount path does not exist", device_id);
        return ORBIS_AUTO_MOUNTER_ERROR_NOENT;
    }

    memset(info, 0, sizeof(OrbisUsbStorageDeviceInfo));
    info->device_id = device_id;
    info->vendor_id = 0x054c;
    info->product_id = 0x0268;
    info->bcd_device = 0x0100;

    strlcpy(info->vendor, "shadPS4", sizeof(info->vendor));
    info->vendor_len = strlen(info->vendor);
    strlcpy(info->product, "Virtual USB Storage", sizeof(info->product));
    info->product_len = strlen(info->product);
    strlcpy(info->serial_number, "0123456789ABCDEF", sizeof(info->serial_number));
    info->serial_number_len = strlen(info->serial_number);

    info->additional_features = ORBIS_USB_STORAGE_FEATURE_EXFAT;

    c_log(C_LOG_LIB_USBD, C_LOG_LEVEL_INFO, "sceUsbStorageGetDeviceInfo",
          "device_id = %u, vendor = '%s', product = '%s', serial = '%s'", device_id, info->vendor,
          info->product, info->serial_number);
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceUsbStorageIsExist(u32 device_id) {
    if (device_id != 0) {
        return 0;
    }
    return usb_exists(device_id) ? 1 : 0;
}

s32 PS4_SYSV_ABI sceUsbStorageRequestMapWSB(u32 device_id, const char* subpath, s32 flags, s32 unk,
                                            char* out_mount_path, s32* out_status) {
    char mount_path[USB_PATH_MAX];
    (void)flags;
    (void)unk;
    if (device_id != 0) {
        c_log(C_LOG_LIB_USBD, C_LOG_LEVEL_ERROR, "sceUsbStorageRequestMapWSB",
              "invalid device_id %u", device_id);
        return ORBIS_AUTO_MOUNTER_ERROR_NOENT;
    }

    snprintf(mount_path, sizeof(mount_path), "/mnt/usb%u", device_id);
    if (subpath && subpath[0] != '\0') {
        if (strncmp(subpath, "/mnt/usb", 8) == 0) {
            strlcpy(mount_path, subpath, sizeof(mount_path));
        } else {
            if (subpath[0] != '/') {
                strlcat(mount_path, "/", sizeof(mount_path));
            }
            strlcat(mount_path, subpath, sizeof(mount_path));
        }
    }

    if (out_mount_path) {
        strncpy(out_mount_path, mount_path, 127);
        out_mount_path[127] = '\0';
    }
    if (out_status) {
        *out_status = 0;
    }

    c_log(C_LOG_LIB_USBD, C_LOG_LEVEL_INFO, "sceUsbStorageRequestMapWSB",
          "mapped device_id %u to '%s'", device_id, mount_path);
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceUsbStorageRequestMap(u32 device_id, const char* subpath, s32 flags, s32 unk,
                                         char* out_mount_path, s32* out_status) {
    return sceUsbStorageRequestMapWSB(device_id, subpath, flags, unk, out_mount_path, out_status);
}

s32 PS4_SYSV_ABI sceUsbStorageRequestUnmap(u32 device_id, const char* directory) {
    if (!directory) {
        c_log(C_LOG_LIB_USBD, C_LOG_LEVEL_ERROR, "sceUsbStorageRequestUnmap", "directory is null");
        return ORBIS_AUTO_MOUNTER_ERROR_INVAL;
    }
    c_log(C_LOG_LIB_USBD, C_LOG_LEVEL_INFO, "sceUsbStorageRequestUnmap",
          "unmapped device_id %u, directory '%s'", device_id, directory);
    return ORBIS_OK;
}

static void usb_free_state(usb_getdents_state_t* state) {
    free(state->names);
    free(state->is_directory);
    memset(state, 0, sizeof(*state));
}

/* The folder's entries (but its hidden ones), in the order the host lists them */
static int usb_list(const char* dir, usb_getdents_state_t* state) {
    size_t names_size = 0, names_cap = 0;
    u32 cap = 0;
    RDIR* rdir = retro_opendir_include_hidden(dir, true);
    if (!rdir) {
        return 1;
    }
    while (retro_readdir(rdir)) {
        const char* name = retro_dirent_get_name(rdir);
        const size_t len = strlen(name);
        if (!len || name[0] == '.') {
            continue;
        }
        if (names_size + len + 1 > names_cap) {
            char* grown;
            names_cap = (names_size + len + 1) * 2;
            grown = (char*)realloc(state->names, names_cap);
            if (!grown) {
                retro_closedir(rdir);
                return 0;
            }
            state->names = grown;
        }
        if (state->count == cap) {
            u8* grown;
            cap = cap ? cap * 2 : 16;
            grown = (u8*)realloc(state->is_directory, cap);
            if (!grown) {
                retro_closedir(rdir);
                return 0;
            }
            state->is_directory = grown;
        }
        memcpy(state->names + names_size, name, len + 1);
        names_size += len + 1;
        state->is_directory[state->count++] = retro_dirent_is_dir(rdir, NULL) ? 1 : 0;
    }
    retro_closedir(rdir);
    return 1;
}

typedef struct usb_getdents_call {
    usb_getdents_state_t* state;
    s32 fd;
    void* buffer;
    u64 bufsize;
    u64* out_bytes_read;
    s32 result;
} usb_getdents_call_t;

static void usb_open_locked(void* ctx) {
    usb_getdents_call_t* call = (usb_getdents_call_t*)ctx;
    u32 i;
    for (i = 0; i < USB_GETDENTS_MAX; i++) {
        if (!g_usb_getdents_table[i].fd) {
            g_usb_getdents_table[i] = *call->state;
            call->fd = g_usb_getdents_table[i].fd = g_usb_next_fd++;
            call->result = ORBIS_OK;
            return;
        }
    }
    call->result = ORBIS_AUTO_MOUNTER_ERROR_NOTSUP;
}

s32 PS4_SYSV_ABI sceUsbStorageGetdentsOpen(u32 device_id, s32* out_fd, u64* out_bufsize) {
    char dir[USB_PATH_MAX];
    usb_getdents_state_t state;
    usb_getdents_call_t call;
    if (!out_fd || !out_bufsize) {
        c_log(C_LOG_LIB_USBD, C_LOG_LEVEL_ERROR, "sceUsbStorageGetdentsOpen",
              "null pointer parameter");
        return ORBIS_AUTO_MOUNTER_ERROR_INVAL;
    }
    if (device_id != 0) {
        c_log(C_LOG_LIB_USBD, C_LOG_LEVEL_ERROR, "sceUsbStorageGetdentsOpen",
              "invalid device_id %u", device_id);
        return ORBIS_AUTO_MOUNTER_ERROR_NOENT;
    }

    usb_storage_mount_path(device_id, dir, sizeof(dir));
    if (!path_is_valid(dir)) {
        path_mkdir(dir);
    }

    memset(&state, 0, sizeof(state));
    if (!usb_list(dir, &state)) {
        usb_free_state(&state);
        c_log(C_LOG_LIB_USBD, C_LOG_LEVEL_ERROR, "sceUsbStorageGetdentsOpen",
              "out of memory listing device %u", device_id);
        return ORBIS_AUTO_MOUNTER_ERROR_NOTSUP;
    }

    call.state = &state;
    call.fd = 0;
    combiner_run(&g_usb_getdents_mutex, usb_open_locked, &call);
    if (call.result != ORBIS_OK) {
        usb_free_state(&state);
        c_log(C_LOG_LIB_USBD, C_LOG_LEVEL_ERROR, "sceUsbStorageGetdentsOpen",
              "%d listings are open: no more", USB_GETDENTS_MAX);
        return call.result;
    }

    *out_fd = call.fd;
    *out_bufsize = 0x8000; /* 32 KB buffer */
    c_log(C_LOG_LIB_USBD, C_LOG_LEVEL_INFO, "sceUsbStorageGetdentsOpen",
          "opened getdents fd = %d for device %u, found %u entries", call.fd, device_id,
          state.count);
    return ORBIS_OK;
}

static usb_getdents_state_t* usb_find(s32 fd) {
    u32 i;
    if (fd <= 0) {
        return NULL;
    }
    for (i = 0; i < USB_GETDENTS_MAX; i++) {
        if (g_usb_getdents_table[i].fd == fd) {
            return &g_usb_getdents_table[i];
        }
    }
    return NULL;
}

static void usb_read_locked(void* ctx) {
    usb_getdents_call_t* call = (usb_getdents_call_t*)ctx;
    usb_getdents_state_t* state = usb_find(call->fd);
    const char* name;
    u64 bytes_written = 0;
    u32 i;
    if (!state) {
        c_log(C_LOG_LIB_USBD, C_LOG_LEVEL_ERROR, "sceUsbStorageGetdentsRead", "invalid fd %d",
              call->fd);
        *call->out_bytes_read = 0;
        call->result = ORBIS_AUTO_MOUNTER_ERROR_NOTSUP;
        return;
    }

    /* The next entry's name */
    name = state->names;
    for (i = 0; i < state->current_index; i++) {
        name += strlen(name) + 1;
    }

    while (state->current_index < state->count) {
        const size_t len = strlen(name);
        const u16 namlen = (u16)(len < 255 ? len : 255);
        const u16 reclen = (u16)((8 + namlen + 1 + 7) & ~7u);
        OrbisUsbStorageDirent* entry;

        if (bytes_written + reclen > call->bufsize) {
            break;
        }

        entry = (OrbisUsbStorageDirent*)((u8*)call->buffer + bytes_written);
        memset(entry, 0, reclen);
        entry->d_fileno = state->current_index + 1;
        entry->d_reclen = reclen;
        entry->d_type = state->is_directory[state->current_index] ? 4 : 8; /* DT_DIR, DT_REG */
        entry->d_namlen = (u8)namlen;
        memcpy(entry->d_name, name, namlen);
        entry->d_name[namlen] = '\0';

#ifndef NDEBUG
        c_log(C_LOG_LIB_USBD, C_LOG_LEVEL_TRACE, "sceUsbStorageGetdentsRead",
              "entry: '%s', type = %u", name, entry->d_type);
#endif

        bytes_written += reclen;
        state->current_index++;
        name += len + 1;
    }

    *call->out_bytes_read = bytes_written;
    c_log(C_LOG_LIB_USBD, C_LOG_LEVEL_DEBUG, "sceUsbStorageGetdentsRead",
          "fd = %d, returned %llu bytes (%u/%u entries)", call->fd,
          (unsigned long long)bytes_written, state->current_index, state->count);
    call->result = ORBIS_OK;
}

s32 PS4_SYSV_ABI sceUsbStorageGetdentsRead(s32 fd, void* buffer, u64 bufsize, u64* out_bytes_read) {
    usb_getdents_call_t call;
    if (!buffer || !out_bytes_read) {
        c_log(C_LOG_LIB_USBD, C_LOG_LEVEL_ERROR, "sceUsbStorageGetdentsRead",
              "null buffer or out_bytes_read");
        return ORBIS_AUTO_MOUNTER_ERROR_INVAL;
    }
    call.fd = fd;
    call.buffer = buffer;
    call.bufsize = bufsize;
    call.out_bytes_read = out_bytes_read;
    combiner_run(&g_usb_getdents_mutex, usb_read_locked, &call);
    return call.result;
}

static void usb_close_locked(void* ctx) {
    usb_getdents_call_t* call = (usb_getdents_call_t*)ctx;
    usb_getdents_state_t* state = usb_find(call->fd);
    if (state) {
        usb_free_state(state);
    }
    c_log(C_LOG_LIB_USBD, C_LOG_LEVEL_DEBUG, "sceUsbStorageGetdentsClose", "closed fd = %d",
          call->fd);
}

s32 PS4_SYSV_ABI sceUsbStorageGetdentsClose(s32 fd) {
    usb_getdents_call_t call;
    call.fd = fd;
    combiner_run(&g_usb_getdents_mutex, usb_close_locked, &call);
    return ORBIS_OK;
}
