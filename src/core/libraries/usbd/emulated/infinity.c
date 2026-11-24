/* SPDX-FileCopyrightText: Copyright 2025 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <streams/file_stream.h>
#include <string.h>

#include "common/combine.h"
#include "core/libraries/usbd/emulated/infinity.h"

/* After libusb.h: on Windows it brings windows.h, whose C_ASSERT this one replaces */
#include "common/c_bridge.h"

#define INF_REPLY 32

typedef struct infinity_figure {
    RFILE* inf_file;
    u8 data[INFINITY_FIGURE_SIZE];
    int present;
    u8 order_added;
} infinity_figure_t;

/* The base */
static combiner_t infinity_mutex;
static infinity_figure_t infinity_figures[MAX_INFINITY_FIGURES];
static u32 random_a, random_b, random_c, random_d;
static u8 m_figure_order;
static usb_reply_ring_t m_figure_added_removed_responses;

/* The answers queued for the IN endpoint */
static usb_reply_ring_t m_queries;

static u32 rotl32(u32 v, unsigned n) {
    return (v << n) | (v >> (32 - n));
}

static u8 inf_checksum(const u8* data, int num_of_bytes) {
    int checksum = 0;
    int i;
    for (i = 0; i < num_of_bytes; i++) {
        checksum += data[i];
    }
    return (u8)(checksum & 0xFF);
}

static void inf_save(infinity_figure_t* figure) {
    if (!figure->inf_file) {
        return;
    }
    filestream_seek(figure->inf_file, 0, RETRO_VFS_SEEK_POSITION_START);
    filestream_write(figure->inf_file, figure->data, sizeof(figure->data));
}

static void inf_close(infinity_figure_t* figure) {
    if (figure->inf_file) {
        filestream_close(figure->inf_file);
        figure->inf_file = NULL;
    }
}

static u8 inf_derive_position(u8 position) {
    switch (position) {
    case 0:
    case 1:
    case 2:
        return 1;
    case 3:
    case 4:
    case 5:
        return 2;
    case 6:
    case 7:
    case 8:
        return 3;
    default:
        return 0;
    }
}

static infinity_figure_t* inf_figure_by_order(u8 order_added) {
    u8 i;
    for (i = 0; i < MAX_INFINITY_FIGURES; i++) {
        if (infinity_figures[i].order_added == order_added) {
            return &infinity_figures[i];
        }
    }
    return &infinity_figures[0];
}

typedef struct inf_figure_call {
    u8 slot;
    const u8* buf;
    RFILE* file;
} inf_figure_call_t;

static void inf_remove_locked(void* ctx) {
    inf_figure_call_t* call = (inf_figure_call_t*)ctx;
    infinity_figure_t* figure = &infinity_figures[call->slot];
    u8 response[INF_REPLY];
    u8 slot;
    if (!figure->present) {
        return;
    }
    slot = inf_derive_position(call->slot);
    if (slot == 0) {
        return;
    }
    figure->present = 0;

    memset(response, 0, sizeof(response));
    response[0] = 0xab;
    response[1] = 0x04;
    response[2] = slot;
    response[3] = 0x09;
    response[4] = figure->order_added;
    response[5] = 0x01;
    response[6] = inf_checksum(response, 6);
    usb_reply_ring_push(&m_figure_added_removed_responses, response, INF_REPLY);

    inf_save(figure);
    inf_close(figure);
}

void infinity_remove_figure(u8 pad, u8 slot, int full_remove) {
    inf_figure_call_t call;
    (void)pad;
    (void)full_remove;
    call.slot = slot;
    combiner_run(&infinity_mutex, inf_remove_locked, &call);
}

static void inf_load_locked(void* ctx) {
    inf_figure_call_t* call = (inf_figure_call_t*)ctx;
    infinity_figure_t* figure = &infinity_figures[call->slot];
    u8 response[INF_REPLY];
    u8 order_added;
    u8 position;

    if (figure->inf_file != call->file) {
        inf_close(figure);
    }
    figure->inf_file = call->file;
    memcpy(figure->data, call->buf, sizeof(figure->data));
    figure->present = 1;
    if (figure->order_added == 255) {
        figure->order_added = m_figure_order;
        m_figure_order++;
    }
    order_added = figure->order_added;

    position = inf_derive_position(call->slot);
    if (position == 0) {
        return;
    }

    memset(response, 0, sizeof(response));
    response[0] = 0xab;
    response[1] = 0x04;
    response[2] = position;
    response[3] = 0x09;
    response[4] = order_added;
    response[5] = 0x00;
    response[6] = inf_checksum(response, 6);
    usb_reply_ring_push(&m_figure_added_removed_responses, response, INF_REPLY);
}

void infinity_load_figure(const char* file_name, u8 pad, u8 slot) {
    u8 data[INFINITY_FIGURE_SIZE];
    inf_figure_call_t call;
    RFILE* file = filestream_open(
        file_name, RETRO_VFS_FILE_ACCESS_READ_WRITE | RETRO_VFS_FILE_ACCESS_UPDATE_EXISTING,
        RETRO_VFS_FILE_ACCESS_HINT_NONE);
    (void)pad;
    C_ASSERT(file && filestream_read(file, data, sizeof(data)) == (s64)sizeof(data), "LoadFigure");
    call.slot = slot;
    call.buf = data;
    call.file = file;
    combiner_run(&infinity_mutex, inf_load_locked, &call);
}

static void inf_blank_response(u8 sequence, u8* reply_buf) {
    reply_buf[0] = 0xaa;
    reply_buf[1] = 0x01;
    reply_buf[2] = sequence;
    reply_buf[3] = inf_checksum(reply_buf, 3);
}

static u32 inf_descramble(u64 num_to_descramble) {
    u64 mask = 0x8E55AA1B3999E8AAull;
    u32 ret = 0;
    int i;
    for (i = 0; i < 64; i++) {
        if (mask & 0x8000000000000000ull) {
            ret = (ret << 1) | (u32)(num_to_descramble & 0x01);
        }
        num_to_descramble >>= 1;
        mask <<= 1;
    }
    return ret;
}

static u64 inf_scramble(u32 num_to_scramble, u32 garbage) {
    u64 mask = 0x8E55AA1B3999E8AAull;
    u64 ret = 0;
    int i;
    for (i = 0; i < 64; i++) {
        ret <<= 1;
        if ((mask & 1) != 0) {
            ret |= (num_to_scramble & 1);
            num_to_scramble >>= 1;
        } else {
            ret |= (garbage & 1);
            garbage >>= 1;
        }
        mask >>= 1;
    }
    return ret;
}

static u32 inf_get_next(void) {
    u32 a = random_a;
    u32 b = random_b;
    u32 c = random_c;
    u32 ret = rotl32(random_b, 27);
    const u32 temp = (a + ((ret ^ 0xFFFFFFFFu) + 1));
    b ^= rotl32(c, 17);
    a = random_d;
    c += a;
    ret = b + temp;
    a += temp;

    random_c = a;
    random_a = b;
    random_b = c;
    random_d = ret;
    return ret;
}

static void inf_generate_seed(u32 seed) {
    int i;
    random_a = 0xF1EA5EED;
    random_b = seed;
    random_c = seed;
    random_d = seed;
    for (i = 0; i < 23; i++) {
        inf_get_next();
    }
}

static void inf_descramble_and_seed(const u8* buf, u8 sequence, u8* reply_buf) {
    const u64 value = (u64)buf[4] << 56 | (u64)buf[5] << 48 | (u64)buf[6] << 40 |
                      (u64)buf[7] << 32 | (u64)buf[8] << 24 | (u64)buf[9] << 16 |
                      (u64)buf[10] << 8 | (u64)buf[11];
    inf_generate_seed(inf_descramble(value));
    inf_blank_response(sequence, reply_buf);
}

static void inf_next_and_scramble(u8 sequence, u8* reply_buf) {
    const u64 scrambled = inf_scramble(inf_get_next(), 0);
    int i;
    memset(reply_buf, 0, INF_REPLY);
    reply_buf[0] = 0xAA;
    reply_buf[1] = 0x09;
    reply_buf[2] = sequence;
    for (i = 0; i < 8; i++) {
        reply_buf[3 + i] = (u8)((scrambled >> (56 - 8 * i)) & 0xFF);
    }
    reply_buf[11] = inf_checksum(reply_buf, 11);
}

static void inf_present_figures(u8 sequence, u8* reply_buf) {
    int x = 3;
    u8 i;
    for (i = 0; i < MAX_INFINITY_FIGURES; i++) {
        const u8 slot = i == 0 ? 0x10 : (i < 4) ? 0x20 : 0x30;
        if (infinity_figures[i].present) {
            reply_buf[x] = (u8)(slot + infinity_figures[i].order_added);
            reply_buf[x + 1] = 0x09;
            x += 2;
        }
    }
    reply_buf[0] = 0xaa;
    reply_buf[1] = (u8)(x - 2);
    reply_buf[2] = sequence;
    reply_buf[x] = inf_checksum(reply_buf, x);
}

typedef struct inf_block_call {
    u8 fig_num;
    u8 block;
    const u8* to_write_buf;
    u8* reply_buf;
    u8 sequence;
} inf_block_call_t;

static void inf_query_block_locked(void* ctx) {
    const inf_block_call_t* call = (const inf_block_call_t*)ctx;
    const infinity_figure_t* figure = inf_figure_by_order(call->fig_num);
    const u8 file_block = (call->block == 0) ? 1 : (u8)(call->block * 4);
    call->reply_buf[0] = 0xaa;
    call->reply_buf[1] = 0x12;
    call->reply_buf[2] = call->sequence;
    call->reply_buf[3] = 0x00;
    if (figure->present && file_block < 20) {
        memcpy(&call->reply_buf[4], figure->data + (16 * file_block), 16);
    }
    call->reply_buf[20] = inf_checksum(call->reply_buf, 20);
}

static void inf_write_block_locked(void* ctx) {
    const inf_block_call_t* call = (const inf_block_call_t*)ctx;
    infinity_figure_t* figure = inf_figure_by_order(call->fig_num);
    const u8 file_block = (call->block == 0) ? 1 : (u8)(call->block * 4);
    call->reply_buf[0] = 0xaa;
    call->reply_buf[1] = 0x02;
    call->reply_buf[2] = call->sequence;
    call->reply_buf[3] = 0x00;
    if (figure->present && file_block < 20) {
        memcpy(figure->data + (file_block * 16), call->to_write_buf, 16);
        inf_save(figure);
    }
    call->reply_buf[4] = inf_checksum(call->reply_buf, 4);
}

static void inf_figure_identifier_locked(void* ctx) {
    const inf_block_call_t* call = (const inf_block_call_t*)ctx;
    const infinity_figure_t* figure = inf_figure_by_order(call->fig_num);
    call->reply_buf[0] = 0xaa;
    call->reply_buf[1] = 0x09;
    call->reply_buf[2] = call->sequence;
    call->reply_buf[3] = 0x00;
    if (figure->present) {
        memcpy(&call->reply_buf[4], figure->data, 7);
    }
    call->reply_buf[11] = inf_checksum(call->reply_buf, 11);
}

static void inf_block(u8 fig_num, u8 block, const u8* to_write_buf, u8* reply_buf, u8 sequence,
                      void (*locked)(void*)) {
    inf_block_call_t call;
    call.fig_num = fig_num;
    call.block = block;
    call.to_write_buf = to_write_buf;
    call.reply_buf = reply_buf;
    call.sequence = sequence;
    combiner_run(&infinity_mutex, locked, &call);
}

static void inf_init(void) {
    u8 i;
    combiner_init(&infinity_mutex);
    for (i = 0; i < MAX_INFINITY_FIGURES; i++) {
        infinity_figures[i].order_added = 255;
    }
}

static s32 inf_control_transfer(struct libusb_device_handle* dev_handle, u8 bmRequestType,
                                u8 bRequest, u16 wValue, u16 wIndex, u8* data, u16 wLength,
                                u32 timeout) {
    (void)dev_handle;
    (void)bmRequestType;
    (void)bRequest;
    (void)wValue;
    (void)wIndex;
    (void)data;
    (void)wLength;
    (void)timeout;
    return LIBUSB_SUCCESS;
}

static enum libusb_transfer_status inf_handle_async(struct libusb_transfer* transfer) {
    switch (transfer->endpoint) {
    case 0x81: {
        /* Respond after FF command */
        const u8* response = usb_reply_ring_front(&m_figure_added_removed_responses);
        if (response) {
            memcpy(transfer->buffer, response, 0x20);
            usb_reply_ring_pop(&m_figure_added_removed_responses);
        } else if ((response = usb_reply_ring_front(&m_queries)) != NULL) {
            memcpy(transfer->buffer, response, 0x20);
            usb_reply_ring_pop(&m_queries);
        }
        break;
    }
    case 0x01: {
        static const u8 answer_80[] = {0xaa, 0x15, 0x00, 0x00, 0x0f, 0x01, 0x00, 0x03,
                                       0x02, 0x09, 0x09, 0x43, 0x20, 0x32, 0x62, 0x36,
                                       0x36, 0x4b, 0x34, 0x99, 0x67, 0x31, 0x93, 0x8c};
        const u8 command = transfer->buffer[2];
        const u8 sequence = transfer->buffer[3];
        u8 q_result[INF_REPLY];
        c_log(C_LOG_LIB_USBD, C_LOG_LEVEL_INFO, "HandleAsyncTransfer",
              "Infinity Backend Transfer command: %x", command);
        memset(q_result, 0, sizeof(q_result));

        switch (command) {
        case 0x80:
            memcpy(q_result, answer_80, sizeof(answer_80));
            break;
        case 0x81:
            /* Initiate Challenge */
            inf_descramble_and_seed(transfer->buffer, sequence, q_result);
            break;
        case 0x83:
            /* Challenge Response */
            inf_next_and_scramble(sequence, q_result);
            break;
        case 0x90:
        case 0x92:
        case 0x93:
        case 0x95:
        case 0x96:
            /* Color commands */
            inf_blank_response(sequence, q_result);
            break;
        case 0xA1:
            /* Get Present Figures */
            inf_present_figures(sequence, q_result);
            break;
        case 0xA2:
            /* Read Block from Figure */
            inf_block(transfer->buffer[4], transfer->buffer[5], NULL, q_result, sequence,
                      inf_query_block_locked);
            break;
        case 0xA3:
            /* Write block to figure */
            inf_block(transfer->buffer[4], transfer->buffer[5], &transfer->buffer[7], q_result,
                      sequence, inf_write_block_locked);
            break;
        case 0xB4:
            /* Get figure ID */
            inf_block(transfer->buffer[4], 0, NULL, q_result, sequence,
                      inf_figure_identifier_locked);
            break;
        case 0xB5:
            /* Get status? */
            inf_blank_response(sequence, q_result);
            break;
        default:
            c_log(C_LOG_LIB_USBD, C_LOG_LEVEL_ERROR, "HandleAsyncTransfer",
                  "Unhandled Infinity Query: %u", command);
            break;
        }

        usb_reply_ring_push(&m_queries, q_result, INF_REPLY);
        break;
    }
    default:
        c_log(C_LOG_LIB_USBD, C_LOG_LEVEL_ERROR, "HandleAsyncTransfer",
              "Unhandled Infinity Endpoint: %u", transfer->endpoint);
        break;
    }
    return LIBUSB_TRANSFER_COMPLETED;
}

static u8 m_endpoint_out_extra[9] = {0x09, 0x21, 0x11, 0x01, 0x00, 0x01, 0x22, 0x1d, 0x00};
static struct libusb_endpoint_descriptor m_endpoint_descriptors[2] = {
    {0x7, 0x5, 0x81, 0x3, 0x20, 0x1, 0x0, 0x0, NULL, 0},
    {0x7, 0x5, 0x1, 0x3, 0x20, 0x1, 0x0, 0x0, m_endpoint_out_extra, 9}};
static struct libusb_interface_descriptor m_interface_descriptor = {0x9, 0x4, 0x0, 0x0,  0x2,  0x3,
                                                                    0x0, 0x0, 0x0, NULL, NULL, 0};
static struct libusb_config_descriptor m_config_descriptor = {0x9,  0x2,  0x29, 0x1,  0x1, 0x0,
                                                              0x80, 0xFA, NULL, NULL, 0};
static struct libusb_device_descriptor m_device_descriptor = {
    0x12, 0x1, 0x200, 0x0, 0x0, 0x0, 0x20, 0x0E6F, 0x0129, 0x200, 0x1, 0x2, 0x3, 0x1};

const usb_emulated_device_t usb_infinity_base = {m_endpoint_descriptors,
                                                 &m_interface_descriptor,
                                                 &m_config_descriptor,
                                                 &m_device_descriptor,
                                                 inf_init,
                                                 inf_control_transfer,
                                                 inf_handle_async,
                                                 NULL,
                                                 NULL,
                                                 0};
