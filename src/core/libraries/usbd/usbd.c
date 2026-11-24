/* SPDX-FileCopyrightText: Copyright 2024-2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

/* libSceUsbd: libusb's API, on the host's libusb or an emulated toy device (usb_backend.c) */

#include <libusb.h>

#include "core/emulator_settings.h"
#include "core/libraries/error_codes.h"
#include "core/libraries/usbd/emulated/dimensions.h"
#include "core/libraries/usbd/emulated/infinity.h"
#include "core/libraries/usbd/emulated/skylander.h"
#include "core/libraries/usbd/usb_backend.h"
#include "core/libraries/usbd/usbd.h"

/* After libusb.h: on Windows it brings windows.h, whose C_ASSERT this one replaces */
#include "common/c_bridge.h"

static s32 libusb_to_orbis_error(int retVal) {
    if (retVal == LIBUSB_ERROR_OTHER) {
        return (s32)0x802400FF;
    }
    if (retVal < 0) {
        c_log(C_LOG_LIB_USBD, C_LOG_LEVEL_ERROR, "libusb_to_orbis_error", "libusb returned: %s",
              libusb_error_name(retVal));
        return (s32)(0x80240000 - retVal);
    }
    return retVal;
}

void usbd_library_init(void) {
    switch (emu_settings_get_usb_device_backend()) {
    case EMU_USB_BACKEND_SKYLANDERS_PORTAL:
        usb_backend_select(&usb_skylander_portal);
        break;
    case EMU_USB_BACKEND_INFINITY_BASE:
        usb_backend_select(&usb_infinity_base);
        break;
    case EMU_USB_BACKEND_DIMENSIONS_TOYPAD:
        usb_backend_select(&usb_dimensions_toypad);
        break;
    default:
        usb_backend_select(NULL);
        break;
    }
}

s32 PS4_SYSV_ABI sceUsbdInit(void) {
    c_log(C_LOG_LIB_USBD, C_LOG_LEVEL_DEBUG, "sceUsbdInit", "called");

    return libusb_to_orbis_error(usb_backend_init());
}

void PS4_SYSV_ABI sceUsbdExit(void) {
    c_log(C_LOG_LIB_USBD, C_LOG_LEVEL_DEBUG, "sceUsbdExit", "called");

    usb_backend_exit();
}

s64 PS4_SYSV_ABI sceUsbdGetDeviceList(SceUsbdDevice*** list) {
    c_log(C_LOG_LIB_USBD, C_LOG_LEVEL_DEBUG, "sceUsbdGetDeviceList", "called");

    return libusb_to_orbis_error((int)usb_backend_get_device_list(list));
}

void PS4_SYSV_ABI sceUsbdFreeDeviceList(SceUsbdDevice** list, s32 unref_devices) {
    c_log(C_LOG_LIB_USBD, C_LOG_LEVEL_DEBUG, "sceUsbdFreeDeviceList", "called");

    usb_backend_free_device_list(list, unref_devices);
}

SceUsbdDevice* PS4_SYSV_ABI sceUsbdRefDevice(SceUsbdDevice* device) {
    c_log(C_LOG_LIB_USBD, C_LOG_LEVEL_DEBUG, "sceUsbdRefDevice", "called");

    return libusb_ref_device(device);
}

void PS4_SYSV_ABI sceUsbdUnrefDevice(SceUsbdDevice* device) {
    c_log(C_LOG_LIB_USBD, C_LOG_LEVEL_DEBUG, "sceUsbdUnrefDevice", "called");

    libusb_unref_device(device);
}

s32 PS4_SYSV_ABI sceUsbdGetConfiguration(SceUsbdDeviceHandle* dev_handle, s32* config) {
    c_log(C_LOG_LIB_USBD, C_LOG_LEVEL_DEBUG, "sceUsbdGetConfiguration", "called");

    return libusb_to_orbis_error(usb_backend_get_configuration(dev_handle, config));
}

s32 PS4_SYSV_ABI sceUsbdGetDeviceDescriptor(SceUsbdDevice* device, SceUsbdDeviceDescriptor* desc) {
    c_log(C_LOG_LIB_USBD, C_LOG_LEVEL_DEBUG, "sceUsbdGetDeviceDescriptor", "called");

    return libusb_to_orbis_error(usb_backend_get_device_descriptor(device, desc));
}

s32 PS4_SYSV_ABI sceUsbdGetActiveConfigDescriptor(SceUsbdDevice* device,
                                                  SceUsbdConfigDescriptor** config) {
    c_log(C_LOG_LIB_USBD, C_LOG_LEVEL_DEBUG, "sceUsbdGetActiveConfigDescriptor", "called");

    return libusb_to_orbis_error(usb_backend_get_active_config_descriptor(device, config));
}

s32 PS4_SYSV_ABI sceUsbdGetConfigDescriptor(SceUsbdDevice* device, u8 config_index,
                                            SceUsbdConfigDescriptor** config) {
    c_log(C_LOG_LIB_USBD, C_LOG_LEVEL_DEBUG, "sceUsbdGetConfigDescriptor", "called");

    return libusb_to_orbis_error(usb_backend_get_config_descriptor(device, config_index, config));
}

s32 PS4_SYSV_ABI sceUsbdGetConfigDescriptorByValue(SceUsbdDevice* device, u8 bConfigurationValue,
                                                   SceUsbdConfigDescriptor** config) {
    c_log(C_LOG_LIB_USBD, C_LOG_LEVEL_DEBUG, "sceUsbdGetConfigDescriptorByValue", "called");

    return libusb_to_orbis_error(
        libusb_get_config_descriptor_by_value(device, bConfigurationValue, config));
}

void PS4_SYSV_ABI sceUsbdFreeConfigDescriptor(SceUsbdConfigDescriptor* config) {
    c_log(C_LOG_LIB_USBD, C_LOG_LEVEL_DEBUG, "sceUsbdFreeConfigDescriptor", "called");

    usb_backend_free_config_descriptor(config);
}

u8 PS4_SYSV_ABI sceUsbdGetBusNumber(SceUsbdDevice* device) {
    c_log(C_LOG_LIB_USBD, C_LOG_LEVEL_DEBUG, "sceUsbdGetBusNumber", "called");

    return usb_backend_get_bus_number(device);
}

u8 PS4_SYSV_ABI sceUsbdGetDeviceAddress(SceUsbdDevice* device) {
    c_log(C_LOG_LIB_USBD, C_LOG_LEVEL_DEBUG, "sceUsbdGetDeviceAddress", "called");

    return usb_backend_get_device_address(device);
}

SceUsbdSpeed PS4_SYSV_ABI sceUsbdGetDeviceSpeed(SceUsbdDevice* device) {
    c_log(C_LOG_LIB_USBD, C_LOG_LEVEL_DEBUG, "sceUsbdGetDeviceSpeed", "called");

    return (SceUsbdSpeed)(libusb_get_device_speed(device));
}

s32 PS4_SYSV_ABI sceUsbdGetMaxPacketSize(SceUsbdDevice* device, u8 endpoint) {
    c_log(C_LOG_LIB_USBD, C_LOG_LEVEL_DEBUG, "sceUsbdGetMaxPacketSize", "called");

    return libusb_to_orbis_error(usb_backend_get_max_packet_size(device, endpoint));
}

s32 PS4_SYSV_ABI sceUsbdGetMaxIsoPacketSize(SceUsbdDevice* device, u8 endpoint) {
    c_log(C_LOG_LIB_USBD, C_LOG_LEVEL_DEBUG, "sceUsbdGetMaxIsoPacketSize", "called");

    return libusb_to_orbis_error(libusb_get_max_iso_packet_size(device, endpoint));
}

s32 PS4_SYSV_ABI sceUsbdOpen(SceUsbdDevice* device, SceUsbdDeviceHandle** dev_handle) {
    c_log(C_LOG_LIB_USBD, C_LOG_LEVEL_DEBUG, "sceUsbdOpen", "called");

    return libusb_to_orbis_error(usb_backend_open_device(device, dev_handle));
}

void PS4_SYSV_ABI sceUsbdClose(SceUsbdDeviceHandle* dev_handle) {
    c_log(C_LOG_LIB_USBD, C_LOG_LEVEL_DEBUG, "sceUsbdClose", "called");

    usb_backend_close_device(dev_handle);
}

SceUsbdDevice* PS4_SYSV_ABI sceUsbdGetDevice(SceUsbdDeviceHandle* dev_handle) {
    c_log(C_LOG_LIB_USBD, C_LOG_LEVEL_DEBUG, "sceUsbdGetDevice", "called");

    return usb_backend_get_device(dev_handle);
}

s32 PS4_SYSV_ABI sceUsbdSetConfiguration(SceUsbdDeviceHandle* dev_handle, s32 config) {
    c_log(C_LOG_LIB_USBD, C_LOG_LEVEL_DEBUG, "sceUsbdSetConfiguration", "called");

    return libusb_to_orbis_error(usb_backend_set_configuration(dev_handle, config));
}

s32 PS4_SYSV_ABI sceUsbdClaimInterface(SceUsbdDeviceHandle* dev_handle, s32 interface_number) {
    c_log(C_LOG_LIB_USBD, C_LOG_LEVEL_DEBUG, "sceUsbdClaimInterface", "called");

    if (sceUsbdKernelDriverActive(dev_handle, interface_number)) {
        sceUsbdDetachKernelDriver(dev_handle, interface_number);
    }

    return libusb_to_orbis_error(usb_backend_claim_interface(dev_handle, interface_number));
}

s32 PS4_SYSV_ABI sceUsbdReleaseInterface(SceUsbdDeviceHandle* dev_handle, s32 interface_number) {
    c_log(C_LOG_LIB_USBD, C_LOG_LEVEL_DEBUG, "sceUsbdReleaseInterface", "called");

    return libusb_to_orbis_error(libusb_release_interface(dev_handle, interface_number));
}

SceUsbdDeviceHandle* PS4_SYSV_ABI sceUsbdOpenDeviceWithVidPid(u16 vendor_id, u16 product_id) {
    c_log(C_LOG_LIB_USBD, C_LOG_LEVEL_DEBUG, "sceUsbdOpenDeviceWithVidPid", "called");

    return usb_backend_open_device_with_vid_pid(vendor_id, product_id);
}

s32 PS4_SYSV_ABI sceUsbdSetInterfaceAltSetting(SceUsbdDeviceHandle* dev_handle,
                                               int interface_number, int alternate_setting) {
    c_log(C_LOG_LIB_USBD, C_LOG_LEVEL_DEBUG, "sceUsbdSetInterfaceAltSetting", "called");

    return libusb_to_orbis_error(
        libusb_set_interface_alt_setting(dev_handle, interface_number, alternate_setting));
}

s32 PS4_SYSV_ABI sceUsbdClearHalt(SceUsbdDeviceHandle* dev_handle, u8 endpoint) {
    c_log(C_LOG_LIB_USBD, C_LOG_LEVEL_DEBUG, "sceUsbdClearHalt", "called");

    return libusb_to_orbis_error(libusb_clear_halt(dev_handle, endpoint));
}

s32 PS4_SYSV_ABI sceUsbdResetDevice(SceUsbdDeviceHandle* dev_handle) {
    c_log(C_LOG_LIB_USBD, C_LOG_LEVEL_DEBUG, "sceUsbdResetDevice", "called");

    return libusb_to_orbis_error(usb_backend_reset_device(dev_handle));
}

s32 PS4_SYSV_ABI sceUsbdKernelDriverActive(SceUsbdDeviceHandle* dev_handle, int interface_number) {
    c_log(C_LOG_LIB_USBD, C_LOG_LEVEL_DEBUG, "sceUsbdKernelDriverActive", "called");

    return libusb_to_orbis_error(usb_backend_kernel_driver_active(dev_handle, interface_number));
}

s32 PS4_SYSV_ABI sceUsbdDetachKernelDriver(SceUsbdDeviceHandle* dev_handle, int interface_number) {
    c_log(C_LOG_LIB_USBD, C_LOG_LEVEL_DEBUG, "sceUsbdDetachKernelDriver", "called");

    return libusb_to_orbis_error(usb_backend_detach_kernel_driver(dev_handle, interface_number));
}

s32 PS4_SYSV_ABI sceUsbdAttachKernelDriver(SceUsbdDeviceHandle* dev_handle, int interface_number) {
    c_log(C_LOG_LIB_USBD, C_LOG_LEVEL_DEBUG, "sceUsbdAttachKernelDriver", "called");

    return libusb_to_orbis_error(usb_backend_attach_kernel_driver(dev_handle, interface_number));
}

u8* PS4_SYSV_ABI sceUsbdControlTransferGetData(SceUsbdTransfer* transfer) {
    c_log(C_LOG_LIB_USBD, C_LOG_LEVEL_DEBUG, "sceUsbdControlTransferGetData", "called");

    return libusb_control_transfer_get_data(transfer);
}

SceUsbdControlSetup* PS4_SYSV_ABI sceUsbdControlTransferGetSetup(SceUsbdTransfer* transfer) {
    c_log(C_LOG_LIB_USBD, C_LOG_LEVEL_DEBUG, "sceUsbdControlTransferGetSetup", "called");

    return libusb_control_transfer_get_setup(transfer);
}

void PS4_SYSV_ABI sceUsbdFillControlSetup(u8* buf, u8 bmRequestType, u8 bRequest, u16 wValue,
                                          u16 wIndex, u16 wLength) {
    c_log(C_LOG_LIB_USBD, C_LOG_LEVEL_DEBUG, "sceUsbdFillControlSetup", "called");

    libusb_fill_control_setup(buf, bmRequestType, bRequest, wValue, wIndex, wLength);
}

SceUsbdTransfer* PS4_SYSV_ABI sceUsbdAllocTransfer(int iso_packets) {
    c_log(C_LOG_LIB_USBD, C_LOG_LEVEL_DEBUG, "sceUsbdAllocTransfer", "called");

    return libusb_alloc_transfer(iso_packets);
}

s32 PS4_SYSV_ABI sceUsbdSubmitTransfer(SceUsbdTransfer* transfer) {
    c_log(C_LOG_LIB_USBD, C_LOG_LEVEL_DEBUG, "sceUsbdSubmitTransfer", "called");

    return libusb_to_orbis_error(usb_backend_submit_transfer(transfer));
}

s32 PS4_SYSV_ABI sceUsbdCancelTransfer(SceUsbdTransfer* transfer) {
    c_log(C_LOG_LIB_USBD, C_LOG_LEVEL_DEBUG, "sceUsbdCancelTransfer", "called");

    return libusb_to_orbis_error(libusb_cancel_transfer(transfer));
}

void PS4_SYSV_ABI sceUsbdFreeTransfer(SceUsbdTransfer* transfer) {
    c_log(C_LOG_LIB_USBD, C_LOG_LEVEL_DEBUG, "sceUsbdFreeTransfer", "called");

    libusb_free_transfer(transfer);
}

void PS4_SYSV_ABI sceUsbdFillControlTransfer(SceUsbdTransfer* transfer,
                                             SceUsbdDeviceHandle* dev_handle, u8* buffer,
                                             SceUsbdTransferCallback callback, void* user_data,
                                             u32 timeout) {
    c_log(C_LOG_LIB_USBD, C_LOG_LEVEL_DEBUG, "sceUsbdFillControlTransfer", "called");

    libusb_fill_control_transfer(transfer, dev_handle, buffer, callback, user_data, timeout);
}

void PS4_SYSV_ABI sceUsbdFillBulkTransfer(SceUsbdTransfer* transfer,
                                          SceUsbdDeviceHandle* dev_handle, u8 endpoint, u8* buffer,
                                          s32 length, SceUsbdTransferCallback callback,
                                          void* user_data, u32 timeout) {
    c_log(C_LOG_LIB_USBD, C_LOG_LEVEL_DEBUG, "sceUsbdFillBulkTransfer", "called");

    libusb_fill_bulk_transfer(transfer, dev_handle, endpoint, buffer, length, callback, user_data,
                              timeout);
}

void PS4_SYSV_ABI sceUsbdFillInterruptTransfer(SceUsbdTransfer* transfer,
                                               SceUsbdDeviceHandle* dev_handle, u8 endpoint,
                                               u8* buffer, s32 length,
                                               SceUsbdTransferCallback callback, void* user_data,
                                               u32 timeout) {
    c_log(C_LOG_LIB_USBD, C_LOG_LEVEL_DEBUG, "sceUsbdFillInterruptTransfer", "called");

    libusb_fill_interrupt_transfer(transfer, dev_handle, endpoint, buffer, length, callback,
                                   user_data, timeout);
}

void PS4_SYSV_ABI sceUsbdFillIsoTransfer(SceUsbdTransfer* transfer, SceUsbdDeviceHandle* dev_handle,
                                         u8 endpoint, u8* buffer, s32 length, s32 num_iso_packets,
                                         SceUsbdTransferCallback callback, void* user_data,
                                         u32 timeout) {
    c_log(C_LOG_LIB_USBD, C_LOG_LEVEL_DEBUG, "sceUsbdFillIsoTransfer", "called");

    libusb_fill_iso_transfer(transfer, dev_handle, endpoint, buffer, length, num_iso_packets,
                             callback, user_data, timeout);
}

void PS4_SYSV_ABI sceUsbdSetIsoPacketLengths(SceUsbdTransfer* transfer, u32 length) {
    c_log(C_LOG_LIB_USBD, C_LOG_LEVEL_DEBUG, "sceUsbdSetIsoPacketLengths", "called");

    libusb_set_iso_packet_lengths(transfer, length);
}

u8* PS4_SYSV_ABI sceUsbdGetIsoPacketBuffer(SceUsbdTransfer* transfer, u32 packet) {
    c_log(C_LOG_LIB_USBD, C_LOG_LEVEL_DEBUG, "sceUsbdGetIsoPacketBuffer", "called");

    return libusb_get_iso_packet_buffer(transfer, packet);
}

s32 PS4_SYSV_ABI sceUsbdControlTransfer(SceUsbdDeviceHandle* dev_handle, u8 request_type,
                                        u8 bRequest, u16 wValue, u16 wIndex, u8* data, s32 wLength,
                                        u32 timeout) {
    c_log(C_LOG_LIB_USBD, C_LOG_LEVEL_DEBUG, "sceUsbdControlTransfer", "called");

    return libusb_to_orbis_error(usb_backend_control_transfer(
        dev_handle, request_type, bRequest, wValue, wIndex, data, wLength, timeout));
}

s32 PS4_SYSV_ABI sceUsbdBulkTransfer(SceUsbdDeviceHandle* dev_handle, u8 endpoint, u8* data,
                                     s32 length, s32* actual_length, u32 timeout) {
    c_log(C_LOG_LIB_USBD, C_LOG_LEVEL_DEBUG, "sceUsbdBulkTransfer", "called");

    return libusb_to_orbis_error(
        libusb_bulk_transfer(dev_handle, endpoint, data, length, actual_length, timeout));
}

s32 PS4_SYSV_ABI sceUsbdInterruptTransfer(SceUsbdDeviceHandle* dev_handle, u8 endpoint, u8* data,
                                          s32 length, s32* actual_length, u32 timeout) {
    c_log(C_LOG_LIB_USBD, C_LOG_LEVEL_DEBUG, "sceUsbdInterruptTransfer", "called");

    return libusb_to_orbis_error(
        libusb_interrupt_transfer(dev_handle, endpoint, data, length, actual_length, timeout));
}

s32 PS4_SYSV_ABI sceUsbdGetDescriptor(SceUsbdDeviceHandle* dev_handle, u8 descType, u8 descIndex,
                                      u8* data, s32 length) {
    c_log(C_LOG_LIB_USBD, C_LOG_LEVEL_DEBUG, "sceUsbdGetDescriptor", "called");

    return libusb_to_orbis_error(
        libusb_get_descriptor(dev_handle, descType, descIndex, data, length));
}

s32 PS4_SYSV_ABI sceUsbdGetStringDescriptor(SceUsbdDeviceHandle* dev_handle, u8 desc_index,
                                            u16 langid, u8* data, s32 length) {
    c_log(C_LOG_LIB_USBD, C_LOG_LEVEL_DEBUG, "sceUsbdGetStringDescriptor", "called");

    return libusb_to_orbis_error(
        libusb_get_string_descriptor(dev_handle, desc_index, langid, data, length));
}

s32 PS4_SYSV_ABI sceUsbdGetStringDescriptorAscii(SceUsbdDeviceHandle* dev_handle, u8 desc_index,
                                                 u8* data, s32 length) {
    c_log(C_LOG_LIB_USBD, C_LOG_LEVEL_DEBUG, "sceUsbdGetStringDescriptorAscii", "called");

    return libusb_to_orbis_error(
        libusb_get_string_descriptor_ascii(dev_handle, desc_index, data, length));
}

s32 PS4_SYSV_ABI sceUsbdTryLockEvents(void) {
    c_log(C_LOG_LIB_USBD, C_LOG_LEVEL_DEBUG, "sceUsbdTryLockEvents", "called");

    return usb_backend_try_lock_events();
}

void PS4_SYSV_ABI sceUsbdLockEvents(void) {
    c_log(C_LOG_LIB_USBD, C_LOG_LEVEL_DEBUG, "sceUsbdLockEvents", "called");

    usb_backend_lock_events();
}

void PS4_SYSV_ABI sceUsbdUnlockEvents(void) {
    c_log(C_LOG_LIB_USBD, C_LOG_LEVEL_DEBUG, "sceUsbdUnlockEvents", "called");

    usb_backend_unlock_events();
}

s32 PS4_SYSV_ABI sceUsbdEventHandlingOk(void) {
    c_log(C_LOG_LIB_USBD, C_LOG_LEVEL_DEBUG, "sceUsbdEventHandlingOk", "called");

    return usb_backend_event_handling_ok();
}

s32 PS4_SYSV_ABI sceUsbdEventHandlerActive(void) {
    c_log(C_LOG_LIB_USBD, C_LOG_LEVEL_DEBUG, "sceUsbdEventHandlerActive", "called");

    return usb_backend_event_handler_active();
}

void PS4_SYSV_ABI sceUsbdLockEventWaiters(void) {
    c_log(C_LOG_LIB_USBD, C_LOG_LEVEL_DEBUG, "sceUsbdLockEventWaiters", "called");

    usb_backend_lock_event_waiters();
}

void PS4_SYSV_ABI sceUsbdUnlockEventWaiters(void) {
    c_log(C_LOG_LIB_USBD, C_LOG_LEVEL_DEBUG, "sceUsbdUnlockEventWaiters", "called");

    usb_backend_unlock_event_waiters();
}

s32 PS4_SYSV_ABI sceUsbdWaitForEvent(struct timeval* tv) {
    c_log(C_LOG_LIB_USBD, C_LOG_LEVEL_DEBUG, "sceUsbdWaitForEvent", "called");

    return libusb_to_orbis_error(usb_backend_wait_for_event(tv));
}

s32 PS4_SYSV_ABI sceUsbdHandleEventsTimeout(struct timeval* tv) {
    c_log(C_LOG_LIB_USBD, C_LOG_LEVEL_DEBUG, "sceUsbdHandleEventsTimeout", "called");

    return libusb_to_orbis_error(usb_backend_handle_events_timeout(tv));
}

s32 PS4_SYSV_ABI sceUsbdHandleEvents(void) {
    c_log(C_LOG_LIB_USBD, C_LOG_LEVEL_DEBUG, "sceUsbdHandleEvents", "called");

    return libusb_to_orbis_error(usb_backend_handle_events());
}

s32 PS4_SYSV_ABI sceUsbdHandleEventsLocked(struct timeval* tv) {
    c_log(C_LOG_LIB_USBD, C_LOG_LEVEL_DEBUG, "sceUsbdHandleEventsLocked", "called");

    return libusb_to_orbis_error(usb_backend_handle_events_timeout(tv));
}

s32 PS4_SYSV_ABI sceUsbdCheckConnected(SceUsbdDeviceHandle* dev_handle) {
    c_log(C_LOG_LIB_USBD, C_LOG_LEVEL_DEBUG, "sceUsbdCheckConnected", "called");

    return libusb_to_orbis_error(usb_backend_check_connected(dev_handle));
}

int PS4_SYSV_ABI Func_65F6EF33E38FFF50(void) {
    c_log(C_LOG_LIB_USBD, C_LOG_LEVEL_ERROR, "Func_65F6EF33E38FFF50", "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI Func_97F056BAD90AADE7(void) {
    c_log(C_LOG_LIB_USBD, C_LOG_LEVEL_ERROR, "Func_97F056BAD90AADE7", "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI Func_C55104A33B35B264(void) {
    c_log(C_LOG_LIB_USBD, C_LOG_LEVEL_ERROR, "Func_C55104A33B35B264", "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI Func_D56B43060720B1E0(void) {
    c_log(C_LOG_LIB_USBD, C_LOG_LEVEL_ERROR, "Func_D56B43060720B1E0", "(STUBBED) called");
    return ORBIS_OK;
}
