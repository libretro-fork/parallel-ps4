/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef SHADPS4_USBD_USBSTORAGE_H
#define SHADPS4_USBD_USBSTORAGE_H

/* libSceUsbStorage: one virtual USB stick, a folder in the user directory mounted as
 * /mnt/usb0. Defined in usbstorage.c, registered in core/libraries/libs.cpp. */

#include <retro_common_api.h>
#include <stddef.h>

#include "common/types.h"

RETRO_BEGIN_DECLS

/* The host folder of USB device @device_id (<user>/mnt/usb<id>) into @out */
void usb_storage_mount_path(u32 device_id, char* out, size_t size);

/* PS4 libSceUsbStorage / auto_mounter error codes (module 0x80f4) */
#define ORBIS_AUTO_MOUNTER_ERROR_INVAL ((s32)0x80f40002)
#define ORBIS_AUTO_MOUNTER_ERROR_NOENT ((s32)0x80f40004)
#define ORBIS_AUTO_MOUNTER_ERROR_NOTSUP ((s32)0x80f40005)

/* OrbisUsbStorageDeviceInfo's additional_features */
#define ORBIS_USB_STORAGE_FEATURE_READ_ONLY (1ull << 4)
#define ORBIS_USB_STORAGE_FEATURE_FAT (1ull << 6)
#define ORBIS_USB_STORAGE_FEATURE_EXFAT (1ull << 7)

typedef struct OrbisUsbStorageDeviceInfo {
    u32 device_id;           /* +0x00 */
    u8 pad0[12];             /* +0x04 */
    u16 vendor_id;           /* +0x10 */
    u16 product_id;          /* +0x12 */
    u16 bcd_device;          /* +0x14 */
    char vendor[256];        /* +0x16 */
    u8 pad1[2];              /* +0x116 */
    u64 vendor_len;          /* +0x118 */
    char product[256];       /* +0x120 */
    u64 product_len;         /* +0x220 */
    char serial_number[256]; /* +0x228 */
    u64 serial_number_len;   /* +0x328 */
    u64 additional_features; /* +0x330 */
} OrbisUsbStorageDeviceInfo;
typedef char
    orbis_usb_storage_device_info_size_check[sizeof(OrbisUsbStorageDeviceInfo) == 0x338 ? 1 : -1];

#pragma pack(push, 1)
typedef struct OrbisUsbStorageDirent {
    u32 d_fileno;
    u16 d_reclen;
    u8 d_type;
    u8 d_namlen;
    char d_name[256];
} OrbisUsbStorageDirent;
#pragma pack(pop)
typedef char orbis_usb_storage_dirent_size_check[sizeof(OrbisUsbStorageDirent) == 264 ? 1 : -1];

s32 PS4_SYSV_ABI sceUsbStorageInit(void* attr);
s32 PS4_SYSV_ABI sceUsbStorageTerm(void);
s32 PS4_SYSV_ABI sceUsbStorageRegisterCallback(s32 event_type, void* callback, void* arg);
s32 PS4_SYSV_ABI sceUsbStorageUnregisterCallback(s32 event_type, void* callback);
s32 PS4_SYSV_ABI sceUsbStorageRegisterCallbackForMapAvailable(u32 device_id, const char* directory,
                                                              void* callback, void* arg);
s32 PS4_SYSV_ABI sceUsbStorageUnregisterCallbackForMapAvailable(u32 device_id,
                                                                const char* directory,
                                                                void* callback);
s32 PS4_SYSV_ABI sceUsbStorageRegisterCallbackForMapUnavailable(u32 device_id,
                                                                const char* directory,
                                                                void* callback, void* arg);
s32 PS4_SYSV_ABI sceUsbStorageUnregisterCallbackForMapUnavailable(u32 device_id,
                                                                  const char* directory,
                                                                  void* callback);
s32 PS4_SYSV_ABI sceUsbStorageGetDeviceList(u32* device_ids, u32* count);
s32 PS4_SYSV_ABI sceUsbStorageGetDeviceInfo(u32 device_id, OrbisUsbStorageDeviceInfo* info);
s32 PS4_SYSV_ABI sceUsbStorageIsExist(u32 device_id);
s32 PS4_SYSV_ABI sceUsbStorageRequestMapWSB(u32 device_id, const char* subpath, s32 flags, s32 unk,
                                            char* out_mount_path, s32* out_status);
s32 PS4_SYSV_ABI sceUsbStorageRequestMap(u32 device_id, const char* subpath, s32 flags, s32 unk,
                                         char* out_mount_path, s32* out_status);
s32 PS4_SYSV_ABI sceUsbStorageRequestUnmap(u32 device_id, const char* directory);
s32 PS4_SYSV_ABI sceUsbStorageGetdentsOpen(u32 device_id, s32* out_fd, u64* out_bufsize);
s32 PS4_SYSV_ABI sceUsbStorageGetdentsRead(s32 fd, void* buffer, u64 bufsize, u64* out_bytes_read);
s32 PS4_SYSV_ABI sceUsbStorageGetdentsClose(s32 fd);

/* Once, when the library is registered */
void usb_storage_library_init(void);

RETRO_END_DECLS

#endif
