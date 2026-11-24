/* SPDX-FileCopyrightText: Copyright 2025 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <stdlib.h>
#include <string.h>

#include "core/libraries/usbd/usb_backend.h"

/* After libusb.h: on Windows it brings windows.h, whose C_ASSERT this one replaces */
#include "common/c_bridge.h"

/* The transfers waiting for sceUsbdHandleEvents (an emulated device's): no allocation, so a
 * game with more in flight than this has the newest refused */
#define USB_FLIGHT_MAX 256

static const usb_emulated_device_t* emu;
static libusb_context* g_libusb_context;
static int s_has_removed_driver;

static struct libusb_transfer* flight_list[USB_FLIGHT_MAX];
static u32 flight_count;

/* The interface an emulated device's configuration descriptor points at */
static struct libusb_interface emu_interface;

void usb_backend_select(const usb_emulated_device_t* device) {
    emu = device;
    flight_count = 0;
    if (emu && emu->init) {
        emu->init();
    }
}

int usb_reply_ring_push(usb_reply_ring_t* ring, const u8* reply, u32 size) {
    u8* slot;
    if (ring->count == USB_REPLY_RING_SLOTS) {
        c_log(C_LOG_LIB_USBD, C_LOG_LEVEL_ERROR, "usb_reply_ring_push",
              "%u answers are waiting to be read: this one is dropped", ring->count);
        return 0;
    }
    slot = ring->slot[(ring->head + ring->count) % USB_REPLY_RING_SLOTS];
    memcpy(slot, reply, size);
    memset(slot + size, 0, USB_REPLY_SIZE - size);
    ring->count++;
    return 1;
}

const u8* usb_reply_ring_front(const usb_reply_ring_t* ring) {
    return ring->count ? ring->slot[ring->head] : NULL;
}

void usb_reply_ring_pop(usb_reply_ring_t* ring) {
    if (ring->count) {
        ring->head = (ring->head + 1) % USB_REPLY_RING_SLOTS;
        ring->count--;
    }
}

static void flight_insert(u32 at, struct libusb_transfer* transfer) {
    memmove(&flight_list[at + 1], &flight_list[at], (flight_count - at) * sizeof(*flight_list));
    flight_list[at] = transfer;
    flight_count++;
}

s32 usb_backend_queue_transfer(struct libusb_transfer* transfer) {
    u32 i;
    C_ASSERT(transfer->type == LIBUSB_TRANSFER_TYPE_INTERRUPT, "usb_backend_queue_transfer");
    if (flight_count == USB_FLIGHT_MAX) {
        c_log(C_LOG_LIB_USBD, C_LOG_LEVEL_ERROR, "SubmitTransfer",
              "%u transfers are in flight: this one is refused", flight_count);
        return LIBUSB_ERROR_NO_MEM;
    }
    /* An infinite timeout goes last; otherwise before the first with a longer one */
    if (flight_count && transfer->timeout != 0) {
        for (i = 0; i < flight_count; i++) {
            if (flight_list[i]->timeout > transfer->timeout) {
                flight_insert(i, transfer);
                return LIBUSB_SUCCESS;
            }
        }
    }
    flight_insert(flight_count, transfer);
    return LIBUSB_SUCCESS;
}

s32 usb_backend_init(void) {
    return libusb_init(&g_libusb_context);
}

void usb_backend_exit(void) {
    libusb_exit(g_libusb_context);
}

s64 usb_backend_get_device_list(struct libusb_device*** list) {
    struct libusb_device** fake;
    if (!emu) {
        return libusb_get_device_list(g_libusb_context, list);
    }
    fake = (struct libusb_device**)calloc(2, sizeof(struct libusb_device*));
    fake[0] = usb_backend_get_device(NULL);
    fake[1] = NULL;
    *list = fake;
    return 1;
}

void usb_backend_free_device_list(struct libusb_device** list, s32 unref_devices) {
    if (!emu) {
        libusb_free_device_list(list, unref_devices);
        return;
    }
    if (!list) {
        return;
    }
    if (unref_devices) {
        int i = 0;
        struct libusb_device* dev;
        while ((dev = list[i++]) != NULL) {
            free(dev);
        }
    }
    free(list);
}

s32 usb_backend_get_configuration(struct libusb_device_handle* dev, s32* config) {
    if (!emu) {
        return libusb_get_configuration(dev, config);
    }
    /* *config is left as it was */
    return LIBUSB_SUCCESS;
}

s32 usb_backend_get_device_descriptor(struct libusb_device* dev,
                                      struct libusb_device_descriptor* desc) {
    if (!emu) {
        return libusb_get_device_descriptor(dev, desc);
    }
    memcpy(desc, emu->device, sizeof(struct libusb_device_descriptor));
    return LIBUSB_SUCCESS;
}

s32 usb_backend_get_active_config_descriptor(struct libusb_device* dev,
                                             struct libusb_config_descriptor** config) {
    if (!emu) {
        return libusb_get_active_config_descriptor(dev, config);
    }
    /* The device's own descriptors, pointing at each other; nothing to free */
    emu->interface_desc->endpoint = emu->endpoints;
    emu_interface.altsetting = emu->interface_desc;
    emu_interface.num_altsetting = 1;
    emu->config->interface = &emu_interface;
    *config = emu->config;
    return LIBUSB_SUCCESS;
}

s32 usb_backend_get_config_descriptor(struct libusb_device* dev, u8 config_index,
                                      struct libusb_config_descriptor** config) {
    if (!emu) {
        return libusb_get_config_descriptor(dev, config_index, config);
    }
    if (config_index >= 1) {
        return LIBUSB_ERROR_NOT_FOUND;
    }
    return usb_backend_get_active_config_descriptor(dev, config);
}

void usb_backend_free_config_descriptor(struct libusb_config_descriptor* config) {
    if (!emu) {
        libusb_free_config_descriptor(config);
    }
    /* An emulated device's is its own: not freed */
}

u8 usb_backend_get_bus_number(struct libusb_device* dev) {
    return emu ? 0 : libusb_get_bus_number(dev);
}

u8 usb_backend_get_device_address(struct libusb_device* dev) {
    return emu ? 0 : libusb_get_device_address(dev);
}

s32 usb_backend_get_max_packet_size(struct libusb_device* dev, u8 endpoint) {
    if (!emu) {
        return libusb_get_max_packet_size(dev, endpoint);
    }
    if (emu->max_packet_size) {
        return emu->max_packet_size;
    }
    return emu->device->bMaxPacketSize0;
}

s32 usb_backend_open_device(struct libusb_device* dev, struct libusb_device_handle** dev_handle) {
    UsbDeviceHandle* handle;
    if (!emu) {
        return libusb_open(dev, dev_handle);
    }
    handle = (UsbDeviceHandle*)calloc(1, sizeof(UsbDeviceHandle));
    if (!handle) {
        return LIBUSB_ERROR_NO_MEM;
    }
    handle->dev = dev;
    *dev_handle = (struct libusb_device_handle*)handle;
    return LIBUSB_SUCCESS;
}

void usb_backend_close_device(struct libusb_device_handle* dev_handle) {
    if (!emu) {
        libusb_close(dev_handle);
        return;
    }
    c_log(C_LOG_LIB_USBD, C_LOG_LEVEL_WARNING, "CloseDevice",
          "Guest decided to close device, might be an implementation issue");
    free(dev_handle);
}

struct libusb_device* usb_backend_get_device(struct libusb_device_handle* dev_handle) {
    UsbDevice* fake;
    if (!emu) {
        return libusb_get_device(dev_handle);
    }
    fake = (UsbDevice*)calloc(1, sizeof(UsbDevice));
    fake->bus_number = 0;
    fake->port_number = 0;
    fake->device_address = 0;
    fake->device_descriptor = *emu->device;
    fake->ctx = g_libusb_context;
    return (struct libusb_device*)fake;
}

s32 usb_backend_set_configuration(struct libusb_device_handle* dev_handle, s32 configuration) {
    return emu ? LIBUSB_SUCCESS : libusb_set_configuration(dev_handle, configuration);
}

s32 usb_backend_claim_interface(struct libusb_device_handle* dev_handle, s32 interface_number) {
    return emu ? LIBUSB_SUCCESS : libusb_claim_interface(dev_handle, interface_number);
}

struct libusb_device_handle* usb_backend_open_device_with_vid_pid(u16 vendor_id, u16 product_id) {
    struct libusb_device_handle* dev_handle = NULL;
    if (!emu) {
        return libusb_open_device_with_vid_pid(g_libusb_context, vendor_id, product_id);
    }
    usb_backend_open_device(usb_backend_get_device(NULL), &dev_handle);
    return dev_handle;
}

s32 usb_backend_reset_device(struct libusb_device_handle* dev_handle) {
    if (!emu) {
        return libusb_reset_device(dev_handle);
    }
    c_log(C_LOG_LIB_USBD, C_LOG_LEVEL_WARNING, "ResetDevice",
          "Guest decided to reset device, might be an implementation issue");
    return LIBUSB_SUCCESS;
}

/* Windows and macOS have no kernel driver to detach: it is only pretended */
#if defined(_WIN32) || defined(__APPLE__)
#define USB_PRETEND_KERNEL_DRIVER 1
#else
#define USB_PRETEND_KERNEL_DRIVER 0
#endif

s32 usb_backend_kernel_driver_active(struct libusb_device_handle* dev_handle,
                                     s32 interface_number) {
    if (emu || USB_PRETEND_KERNEL_DRIVER) {
        return s_has_removed_driver ? 0 : 1;
    }
    return libusb_kernel_driver_active(dev_handle, interface_number);
}

s32 usb_backend_detach_kernel_driver(struct libusb_device_handle* dev_handle,
                                     s32 interface_number) {
    if (emu || USB_PRETEND_KERNEL_DRIVER) {
        s_has_removed_driver = 1;
        return LIBUSB_SUCCESS;
    }
    return libusb_detach_kernel_driver(dev_handle, interface_number);
}

s32 usb_backend_attach_kernel_driver(struct libusb_device_handle* dev_handle,
                                     s32 interface_number) {
    if (emu || USB_PRETEND_KERNEL_DRIVER) {
        s_has_removed_driver = 0;
        return LIBUSB_SUCCESS;
    }
    return libusb_attach_kernel_driver(dev_handle, interface_number);
}

s32 usb_backend_control_transfer(struct libusb_device_handle* dev_handle, u8 bmRequestType,
                                 u8 bRequest, u16 wValue, u16 wIndex, u8* data, u16 wLength,
                                 u32 timeout) {
    if (!emu) {
        return libusb_control_transfer(dev_handle, bmRequestType, bRequest, wValue, wIndex, data,
                                       wLength, timeout);
    }
    if (emu->control_transfer) {
        return emu->control_transfer(dev_handle, bmRequestType, bRequest, wValue, wIndex, data,
                                     wLength, timeout);
    }
    c_log(C_LOG_LIB_USBD, C_LOG_LEVEL_WARNING, "ControlTransfer",
          "Backend has not handled control transfers");
    return LIBUSB_ERROR_PIPE;
}

s32 usb_backend_submit_transfer(struct libusb_transfer* transfer) {
    if (!emu) {
        return libusb_submit_transfer(transfer);
    }
    if (emu->submit_transfer) {
        return emu->submit_transfer(transfer);
    }
    return usb_backend_queue_transfer(transfer);
}

s32 usb_backend_try_lock_events(void) {
    return libusb_try_lock_events(g_libusb_context);
}

void usb_backend_lock_events(void) {
    libusb_lock_events(g_libusb_context);
}

void usb_backend_unlock_events(void) {
    libusb_unlock_events(g_libusb_context);
}

s32 usb_backend_event_handling_ok(void) {
    return libusb_event_handling_ok(g_libusb_context);
}

s32 usb_backend_event_handler_active(void) {
    return libusb_event_handler_active(g_libusb_context);
}

void usb_backend_lock_event_waiters(void) {
    libusb_lock_event_waiters(g_libusb_context);
}

void usb_backend_unlock_event_waiters(void) {
    libusb_unlock_event_waiters(g_libusb_context);
}

s32 usb_backend_wait_for_event(struct timeval* tv) {
    return libusb_wait_for_event(g_libusb_context, tv);
}

s32 usb_backend_handle_events_timeout(struct timeval* tv) {
    if (!emu) {
        return libusb_handle_events_timeout(g_libusb_context, tv);
    }
    return usb_backend_handle_events();
}

/* One transfer given back: the oldest of the shortest timeout */
s32 usb_backend_handle_events(void) {
    if (!emu) {
        return libusb_handle_events(g_libusb_context);
    }
    if (emu->give_back_done && emu->give_back_done()) {
        return LIBUSB_SUCCESS;
    }
    if (flight_count) {
        struct libusb_transfer* transfer = flight_list[0];
        const u8 flags = transfer->flags;
        transfer->status = emu->handle_async(transfer);
        transfer->actual_length = transfer->length;
        if (transfer->callback) {
            transfer->callback(transfer);
        }
        if (flags & LIBUSB_TRANSFER_FREE_TRANSFER) {
            libusb_free_transfer(transfer);
        }
        flight_count--;
        memmove(&flight_list[0], &flight_list[1], flight_count * sizeof(*flight_list));
    }
    return LIBUSB_SUCCESS;
}

s32 usb_backend_handle_events_locked(struct timeval* tv) {
    return libusb_handle_events_locked(g_libusb_context, tv);
}

s32 usb_backend_check_connected(struct libusb_device_handle* dev) {
    int config;
    if (emu) {
        return LIBUSB_SUCCESS;
    }
    /* There's no libusb version of this function. Simulate by querying data. */
    return libusb_get_configuration(dev, &config);
}
