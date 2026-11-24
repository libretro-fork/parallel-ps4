/* SPDX-FileCopyrightText: Copyright 2025 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <streams/file_stream.h>
#include <string.h>

#include "common/combine.h"
#include "core/libraries/usbd/emulated/skylander.h"

/* After libusb.h: on Windows it brings windows.h, whose C_ASSERT this one replaces */
#include "common/c_bridge.h"

#define SKY_REMOVED 0
#define SKY_READY 1
#define SKY_REMOVING 2
#define SKY_ADDED 3

/* A figure's status changes not yet reported (two per load or removal); a full queue drops
 * the newest */
#define SKY_STATUS_SLOTS 64

typedef struct skylander {
    RFILE* sky_file;
    u8 status;
    u8 queued_status[SKY_STATUS_SLOTS];
    u32 queued_head;
    u32 queued_count;
    u8 data[SKY_FIGURE_SIZE];
    u32 last_id;
} skylander_t;

typedef struct sky_led_color {
    u8 red;
    u8 green;
    u8 blue;
} sky_led_color_t;

/* The portal */
static combiner_t sky_mutex;
static int m_activated;
static u8 m_interrupt_counter;
static sky_led_color_t m_color_right;
static sky_led_color_t m_color_left;
static sky_led_color_t m_color_trap;
static skylander_t skylanders[MAX_SKYLANDERS];
static u8 ui_skylanders[MAX_SKYLANDERS];

/* The answers queued for the IN endpoint */
static usb_reply_ring_t m_queries;

static void sky_queue_status(skylander_t* s, u8 status) {
    if (s->queued_count == SKY_STATUS_SLOTS) {
        return;
    }
    s->queued_status[(s->queued_head + s->queued_count) % SKY_STATUS_SLOTS] = status;
    s->queued_count++;
}

static u8 sky_pop_status(skylander_t* s) {
    const u8 status = s->queued_status[s->queued_head];
    s->queued_head = (s->queued_head + 1) % SKY_STATUS_SLOTS;
    s->queued_count--;
    return status;
}

static void sky_save(skylander_t* s) {
    if (!s->sky_file) {
        return;
    }
    filestream_seek(s->sky_file, 0, RETRO_VFS_SEEK_POSITION_START);
    filestream_write(s->sky_file, s->data, sizeof(s->data));
}

static void sky_close(skylander_t* s) {
    if (s->sky_file) {
        filestream_close(s->sky_file);
        s->sky_file = NULL;
    }
}

typedef struct sky_figure_call {
    u8 slot;
    const u8* buf;
    RFILE* file;
    u8 found_slot;
} sky_figure_call_t;

static void sky_remove_locked(void* ctx) {
    sky_figure_call_t* call = (sky_figure_call_t*)ctx;
    skylander_t* thesky;
    /* A slot whose figure found no room on the portal (0xFF) has nothing to take off */
    if (ui_skylanders[call->slot] >= MAX_SKYLANDERS) {
        return;
    }
    thesky = &skylanders[ui_skylanders[call->slot]];
    if (thesky->status & 1) {
        thesky->status = 2;
        sky_queue_status(thesky, SKY_REMOVING);
        sky_queue_status(thesky, SKY_REMOVED);
        sky_save(thesky);
        sky_close(thesky);
    }
}

void skylander_remove_figure(u8 pad, u8 slot, int full_remove) {
    sky_figure_call_t call;
    (void)pad;
    (void)full_remove;
    call.slot = slot;
    combiner_run(&sky_mutex, sky_remove_locked, &call);
}

static void sky_load_locked(void* ctx) {
    sky_figure_call_t* call = (sky_figure_call_t*)ctx;
    u32 sky_serial = 0;
    u8 found_slot = 0xFF;
    int i;
    for (i = 3; i > -1; i--) {
        sky_serial <<= 8;
        sky_serial |= call->buf[i];
    }

    /* mimics spot retaining on the portal */
    for (i = 0; i < MAX_SKYLANDERS; i++) {
        if ((skylanders[i].status & 1) == 0) {
            if (skylanders[i].last_id == sky_serial) {
                found_slot = (u8)i;
                break;
            }
            if (i < found_slot) {
                found_slot = (u8)i;
            }
        }
    }

    if (found_slot != 0xFF) {
        skylander_t* s = &skylanders[found_slot];
        memcpy(s->data, call->buf, sizeof(s->data));
        if (s->sky_file != call->file) {
            sky_close(s);
        }
        s->sky_file = call->file;
        call->file = NULL;
        s->status = SKY_ADDED;
        sky_queue_status(s, SKY_ADDED);
        sky_queue_status(s, SKY_READY);
        s->last_id = sky_serial;
    }
    call->found_slot = found_slot;
}

static u8 sky_load_skylander(const u8* buf, RFILE* file) {
    sky_figure_call_t call;
    call.buf = buf;
    call.file = file;
    combiner_run(&sky_mutex, sky_load_locked, &call);
    if (call.file) {
        /* No room on the portal: the file is let go */
        filestream_close(call.file);
    }
    return call.found_slot;
}

void skylander_load_figure(const char* file_name, u8 pad, u8 slot) {
    u8 data[SKY_FIGURE_SIZE];
    RFILE* file = filestream_open(
        file_name, RETRO_VFS_FILE_ACCESS_READ_WRITE | RETRO_VFS_FILE_ACCESS_UPDATE_EXISTING,
        RETRO_VFS_FILE_ACCESS_HINT_NONE);
    (void)pad;
    C_ASSERT(file && filestream_read(file, data, sizeof(data)) == (s64)sizeof(data), "LoadFigure");
    ui_skylanders[slot] = sky_load_skylander(data, file);
}

static void sky_activate_locked(void* ctx) {
    int i;
    (void)ctx;
    if (m_activated) {
        /* If the portal was already active no change is needed */
        return;
    }
    /* If not we need to advertise change to all the figures present on the portal */
    for (i = 0; i < MAX_SKYLANDERS; i++) {
        if (skylanders[i].status & 1) {
            sky_queue_status(&skylanders[i], SKY_ADDED);
            sky_queue_status(&skylanders[i], SKY_READY);
        }
    }
    m_activated = 1;
}

typedef struct sky_led_call {
    u8 side, red, green, blue;
} sky_led_call_t;

static void sky_set_color(sky_led_color_t* color, const sky_led_call_t* call) {
    color->red = call->red;
    color->green = call->green;
    color->blue = call->blue;
}

/* Side: 0x00 right, 0x01 left and right, 0x02 left, 0x03 trap */
static void sky_set_leds_locked(void* ctx) {
    const sky_led_call_t* call = (const sky_led_call_t*)ctx;
    if (call->side == 0x00) {
        sky_set_color(&m_color_right, call);
    } else if (call->side == 0x01) {
        sky_set_color(&m_color_right, call);
        sky_set_color(&m_color_left, call);
    } else if (call->side == 0x02) {
        sky_set_color(&m_color_left, call);
    } else if (call->side == 0x03) {
        sky_set_color(&m_color_trap, call);
    }
}

static void sky_set_leds(u8 side, u8 red, u8 green, u8 blue) {
    sky_led_call_t call;
    call.side = side;
    call.red = red;
    call.green = green;
    call.blue = blue;
    combiner_run(&sky_mutex, sky_set_leds_locked, &call);
}

/* The status answer, the default one of the IN endpoint (64 bytes into @out) */
static void sky_get_status_locked(void* ctx) {
    u8* interrupt_response = (u8*)ctx;
    u32 status = 0;
    int i;
    for (i = MAX_SKYLANDERS - 1; i >= 0; i--) {
        skylander_t* s = &skylanders[i];
        if (s->queued_count) {
            s->status = sky_pop_status(s);
        }
        status <<= 2;
        status |= s->status;
    }
    memset(interrupt_response, 0, USB_REPLY_SIZE);
    interrupt_response[0] = 0x53;
    interrupt_response[5] = m_interrupt_counter++;
    interrupt_response[6] = m_activated ? 0x01 : 0x00;
    memcpy(&interrupt_response[1], &status, sizeof(status));
}

typedef struct sky_block_call {
    u8 sky_num;
    u8 block;
    const u8* to_write_buf;
    u8* reply_buf;
} sky_block_call_t;

static void sky_query_block_locked(void* ctx) {
    const sky_block_call_t* call = (const sky_block_call_t*)ctx;
    const skylander_t* s = &skylanders[call->sky_num];
    call->reply_buf[0] = 'Q';
    call->reply_buf[2] = call->block;
    if (s->status & SKY_READY) {
        call->reply_buf[1] = (u8)(0x10 | call->sky_num);
        memcpy(call->reply_buf + 3, s->data + (16 * call->block), 16);
    } else {
        call->reply_buf[1] = 0x01;
    }
}

static void sky_write_block_locked(void* ctx) {
    const sky_block_call_t* call = (const sky_block_call_t*)ctx;
    skylander_t* s = &skylanders[call->sky_num];
    call->reply_buf[0] = 'W';
    call->reply_buf[2] = call->block;
    if (s->status & 1) {
        call->reply_buf[1] = (u8)(0x10 | call->sky_num);
        memcpy(s->data + (call->block * 16), call->to_write_buf, 16);
        sky_save(s);
    } else {
        call->reply_buf[1] = 0x01;
    }
}

static void sky_block(u8 sky_num, u8 block, const u8* to_write_buf, u8* reply_buf,
                      void (*locked)(void*)) {
    sky_block_call_t call;
    if (sky_num >= MAX_SKYLANDERS || block >= 64) {
        return;
    }
    call.sky_num = sky_num;
    call.block = block;
    call.to_write_buf = to_write_buf;
    call.reply_buf = reply_buf;
    combiner_run(&sky_mutex, locked, &call);
}

static void sky_init(void) {
    combiner_init(&sky_mutex);
    m_activated = 1;
}

/* The commands, from the portal's protocol:
 * 'A' activate (2nd byte 1) or deactivate (0); answered { 'A', on, ff, 77 } on a wired portal.
 * 'C' colour { 'C', r, g, b }; most recent portals have no LED and ignore it.
 * 'J' a side's colour { 'J', side (0 right, 2 left), r, g, b, fade ms (le16) }.
 * 'L' a light while audio plays { 'L', position (0 right, 1 trap, 2 left), r, g, b }.
 * 'M' audio firmware version; answered as a Trap Team wired portal.
 * 'Q' query { 'Q', 0x10 + figure, block }; answered with the figure in the 2nd byte (0x01 on
 *     an error), the block in the 3rd and its 16 bytes from the 4th. 64 blocks per figure.
 * 'R' ready; answered { 'R', 02, 1b }.
 * 'S' status, the default: 2 bits a figure (present; both set once when added, the high one
 *     when removed), a counter byte, then whether the portal is active.
 * 'W' write { 'W', 0x10 + figure, block, 16 bytes }; answered with the block in the 3rd byte. */
static s32 sky_control_transfer(struct libusb_device_handle* dev_handle, u8 bmRequestType,
                                u8 bRequest, u16 wValue, u16 wIndex, u8* data, u16 wLength,
                                u32 timeout) {
    s32 expected_count = 0;
    /* Data queued to be sent back via the Interrupt Transfer (if needed) */
    u8 interrupt_response[USB_REPLY_SIZE];
    (void)dev_handle;
    (void)wValue;
    (void)wIndex;
    (void)timeout;
    if (bmRequestType != 0x21) {
        return LIBUSB_ERROR_PIPE;
    }
    if (bRequest != 0x09) {
        return 8;
    }
    memset(interrupt_response, 0, sizeof(interrupt_response));
    switch (data[0]) {
    case 'A':
        if (wLength == 2) {
            interrupt_response[0] = 0x41;
            interrupt_response[1] = data[1];
            interrupt_response[2] = 0xFF;
            interrupt_response[3] = 0x77;
            usb_reply_ring_push(&m_queries, interrupt_response, USB_REPLY_SIZE);
            expected_count = 10;
            /* Activated either way, as the portal always has been here */
            combiner_run(&sky_mutex, sky_activate_locked, NULL);
        }
        break;
    case 'C':
        if (wLength == 4) {
            sky_set_leds(0x01, data[1], data[2], data[3]);
            expected_count = 12;
        }
        break;
    case 'J':
        if (wLength == 7) {
            expected_count = 15;
            interrupt_response[0] = data[0];
            usb_reply_ring_push(&m_queries, interrupt_response, USB_REPLY_SIZE);
            sky_set_leds(data[1], data[2], data[3], data[4]);
        }
        break;
    case 'L':
        if (wLength == 5) {
            u8 side = data[1];
            expected_count = 13;
            if (side == 0x02) {
                side = 0x04;
            }
            sky_set_leds(side, data[2], data[3], data[4]);
        }
        break;
    case 'M':
        if (wLength == 2) {
            expected_count = 10;
            interrupt_response[0] = data[0];
            interrupt_response[1] = data[1];
            interrupt_response[2] = 0x00;
            interrupt_response[3] = 0x19;
            usb_reply_ring_push(&m_queries, interrupt_response, USB_REPLY_SIZE);
        }
        break;
    case 'Q':
        if (wLength == 3) {
            sky_block(data[1] & 0xF, data[2], NULL, interrupt_response, sky_query_block_locked);
            usb_reply_ring_push(&m_queries, interrupt_response, USB_REPLY_SIZE);
            expected_count = 11;
        }
        break;
    case 'R':
        if (wLength == 2) {
            interrupt_response[0] = 0x52;
            interrupt_response[1] = 0x02;
            interrupt_response[2] = 0x1b;
            usb_reply_ring_push(&m_queries, interrupt_response, USB_REPLY_SIZE);
            expected_count = 10;
        }
        break;
    case 'S':
        /* The status answers are made by sky_get_status */
        if (wLength == 1) {
            expected_count = 9;
        }
        break;
    case 'V':
        if (wLength == 4) {
            expected_count = 12;
        }
        break;
    case 'W':
        if (wLength == 19) {
            sky_block(data[1] & 0xF, data[2], &data[3], interrupt_response, sky_write_block_locked);
            usb_reply_ring_push(&m_queries, interrupt_response, USB_REPLY_SIZE);
            expected_count = 27;
        }
        break;
    default:
        c_log(C_LOG_LIB_USBD, C_LOG_LEVEL_ERROR, "ControlTransfer",
              "Unhandled Skylander Portal Query: %u", data[0]);
        break;
    }
    return expected_count;
}

static enum libusb_transfer_status sky_handle_async(struct libusb_transfer* transfer) {
    switch (transfer->endpoint) {
    case 0x81: {
        const u8* query = usb_reply_ring_front(&m_queries);
        if (!query) {
            u8 status[USB_REPLY_SIZE];
            combiner_run(&sky_mutex, sky_get_status_locked, status);
            memcpy(transfer->buffer, status, 32);
        } else {
            memcpy(transfer->buffer, query, 32);
            usb_reply_ring_pop(&m_queries);
        }
        transfer->length = 32;
        break;
    }
    case 0x02:
        c_log(C_LOG_LIB_USBD, C_LOG_LEVEL_INFO, "HandleAsyncTransfer", "OUT ENDPOINT");
        break;
    default:
        break;
    }
    return LIBUSB_TRANSFER_COMPLETED;
}

static u8 m_endpoint_out_extra[9] = {0x09, 0x21, 0x11, 0x01, 0x00, 0x01, 0x22, 0x1d, 0x00};
static struct libusb_endpoint_descriptor m_endpoint_descriptors[2] = {
    {0x7, 0x5, 0x81, 0x3, 0x40, 0x1, 0x0, 0x0, NULL, 0},
    {0x7, 0x5, 0x2, 0x3, 0x40, 0x1, 0x0, 0x0, m_endpoint_out_extra, 9}};
static struct libusb_interface_descriptor m_interface_descriptor = {0x9, 0x4, 0x0, 0x0,  0x2,  0x3,
                                                                    0x0, 0x0, 0x0, NULL, NULL, 0};
static struct libusb_config_descriptor m_config_descriptor = {0x9,  0x2,  0x29, 0x1,  0x1, 0x0,
                                                              0x80, 0xFA, NULL, NULL, 0};
static struct libusb_device_descriptor m_device_descriptor = {
    0x12, 0x1, 0x200, 0x0, 0x0, 0x0, 0x40, 0x1430, 0x150, 0x100, 0x1, 0x2, 0x0, 0x1};

const usb_emulated_device_t usb_skylander_portal = {m_endpoint_descriptors,
                                                    &m_interface_descriptor,
                                                    &m_config_descriptor,
                                                    &m_device_descriptor,
                                                    sky_init,
                                                    sky_control_transfer,
                                                    sky_handle_async,
                                                    NULL,
                                                    NULL,
                                                    0};
