/* SPDX-FileCopyrightText: Copyright 2025 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef SHADPS4_USBD_USB_BACKEND_H
#define SHADPS4_USBD_USB_BACKEND_H

/* What libSceUsbd runs on: the host's libusb, or an emulated toy device (a Skylanders portal,
 * a Disney Infinity base, a LEGO Dimensions toypad) whose descriptors and transfers are made
 * here. The device is picked once, when the library is registered (usbd.c). */

#include <libusb.h>
#include <string.h>
#if defined(_WIN32)
#include <windows.h>
#else
#include <pthread.h>
#endif

#include <retro_common_api.h>

#include "common/types.h"

RETRO_BEGIN_DECLS

#if defined(_WIN32)
typedef CRITICAL_SECTION usbi_mutex_t;
#else
typedef pthread_mutex_t usbi_mutex_t;
#endif

struct usb_list_head {
    struct usb_list_head *prev, *next;
};

/* libusb's own device handle and device, as made for an emulated device */
typedef struct UsbDeviceHandle {
    usbi_mutex_t lock;
    unsigned long claimed_interfaces;
    struct usb_list_head list;
    struct libusb_device* dev;
    int auto_detach_kernel_driver;
} UsbDeviceHandle;

typedef struct UsbDevice {
    volatile long refcnt;

    struct libusb_context* ctx;
    struct libusb_device* parent_dev;

    u8 bus_number;
    u8 port_number;
    u8 device_address;
    enum libusb_speed speed;

    struct usb_list_head list;
    unsigned long session_data;

    struct libusb_device_descriptor device_descriptor;
    volatile long attached;
} UsbDevice;

/* An emulated device: its descriptors, and what it does with transfers */
typedef struct usb_emulated_device {
    struct libusb_endpoint_descriptor* endpoints; /* the IN and OUT pair */
    struct libusb_interface_descriptor* interface_desc;
    struct libusb_config_descriptor* config;
    struct libusb_device_descriptor* device;
    /* Once, when the device is picked; may be NULL */
    void (*init)(void);
    s32 (*control_transfer)(struct libusb_device_handle* dev_handle, u8 bmRequestType, u8 bRequest,
                            u16 wValue, u16 wIndex, u8* data, u16 wLength, u32 timeout);
    /* An interrupt transfer's answer, when sceUsbdHandleEvents reaches it */
    enum libusb_transfer_status (*handle_async)(struct libusb_transfer* transfer);
    /* NULL: every transfer waits in the flight list for sceUsbdHandleEvents */
    s32 (*submit_transfer)(struct libusb_transfer* transfer);
    /* NULL, or a transfer the device answered before sceUsbdHandleEvents, given back first:
     * returns 1 when it gave one back */
    int (*give_back_done)(void);
    /* 0: the device descriptor's bMaxPacketSize0 */
    s32 max_packet_size;
} usb_emulated_device_t;

/* NULL for the host's libusb */
void usb_backend_select(const usb_emulated_device_t* device);

/* The flight list's way: queued for sceUsbdHandleEvents, ordered by timeout */
s32 usb_backend_queue_transfer(struct libusb_transfer* transfer);

s32 usb_backend_init(void);
void usb_backend_exit(void);

s64 usb_backend_get_device_list(struct libusb_device*** list);
void usb_backend_free_device_list(struct libusb_device** list, s32 unref_devices);

s32 usb_backend_get_configuration(struct libusb_device_handle* dev, s32* config);
s32 usb_backend_get_device_descriptor(struct libusb_device* dev,
                                      struct libusb_device_descriptor* desc);
s32 usb_backend_get_active_config_descriptor(struct libusb_device* dev,
                                             struct libusb_config_descriptor** config);
s32 usb_backend_get_config_descriptor(struct libusb_device* dev, u8 config_index,
                                      struct libusb_config_descriptor** config);
void usb_backend_free_config_descriptor(struct libusb_config_descriptor* config);

u8 usb_backend_get_bus_number(struct libusb_device* dev);
u8 usb_backend_get_device_address(struct libusb_device* dev);
s32 usb_backend_get_max_packet_size(struct libusb_device* dev, u8 endpoint);

s32 usb_backend_open_device(struct libusb_device* dev, struct libusb_device_handle** dev_handle);
void usb_backend_close_device(struct libusb_device_handle* dev_handle);
struct libusb_device* usb_backend_get_device(struct libusb_device_handle* dev_handle);

s32 usb_backend_set_configuration(struct libusb_device_handle* dev_handle, s32 configuration);
s32 usb_backend_claim_interface(struct libusb_device_handle* dev_handle, s32 interface_number);
struct libusb_device_handle* usb_backend_open_device_with_vid_pid(u16 vendor_id, u16 product_id);
s32 usb_backend_reset_device(struct libusb_device_handle* dev_handle);

s32 usb_backend_kernel_driver_active(struct libusb_device_handle* dev_handle, s32 interface_number);
s32 usb_backend_detach_kernel_driver(struct libusb_device_handle* dev_handle, s32 interface_number);
s32 usb_backend_attach_kernel_driver(struct libusb_device_handle* dev_handle, s32 interface_number);

s32 usb_backend_control_transfer(struct libusb_device_handle* dev_handle, u8 bmRequestType,
                                 u8 bRequest, u16 wValue, u16 wIndex, u8* data, u16 wLength,
                                 u32 timeout);
s32 usb_backend_submit_transfer(struct libusb_transfer* transfer);

s32 usb_backend_try_lock_events(void);
void usb_backend_lock_events(void);
void usb_backend_unlock_events(void);
s32 usb_backend_event_handling_ok(void);
s32 usb_backend_event_handler_active(void);
void usb_backend_lock_event_waiters(void);
void usb_backend_unlock_event_waiters(void);
s32 usb_backend_wait_for_event(struct timeval* tv);

s32 usb_backend_handle_events_timeout(struct timeval* tv);
s32 usb_backend_handle_events(void);
s32 usb_backend_handle_events_locked(struct timeval* tv);

s32 usb_backend_check_connected(struct libusb_device_handle* dev);

/* An emulated device's answers waiting to be read, oldest first (no allocation: a full ring
 * drops the new answer, with an error logged) */
#define USB_REPLY_SIZE 64
#define USB_REPLY_RING_SLOTS 256

typedef struct usb_reply_ring {
    u8 slot[USB_REPLY_RING_SLOTS][USB_REPLY_SIZE];
    u32 head;
    u32 count;
} usb_reply_ring_t;

/* @size bytes of @reply, the rest of the slot zeroed: returns 0 when the ring is full */
int usb_reply_ring_push(usb_reply_ring_t* ring, const u8* reply, u32 size);
/* NULL when empty */
const u8* usb_reply_ring_front(const usb_reply_ring_t* ring);
void usb_reply_ring_pop(usb_reply_ring_t* ring);

RETRO_END_DECLS

#endif
