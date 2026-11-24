/* SPDX-FileCopyrightText: Copyright 2025 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <rthreads/retro_eventcount.h>
#include <streams/file_stream.h>
#include <string.h>

#include "common/combine.h"
#include "core/libraries/usbd/emulated/dimensions.h"

/* After libusb.h: on Windows it brings windows.h, whose C_ASSERT this one replaces */
#include "common/c_bridge.h"

#define DIM_REPLY 32

/* Write transfers answered at submit, waiting for sceUsbdHandleEvents to be given back */
#define DIM_DONE_SLOTS 64

static const u8 COMMAND_KEY[16] = {0x55, 0xFE, 0xF6, 0xB0, 0x62, 0xBF, 0x0B, 0x41,
                                   0xC9, 0xB3, 0x7C, 0xB4, 0x97, 0x3E, 0x29, 0x7B};

static const u8 CHAR_CONSTANT[17] = {0xB7, 0xD5, 0xD7, 0xE6, 0xE7, 0xBA, 0x3C, 0xA8, 0xD8,
                                     0x75, 0x47, 0x68, 0xCF, 0x23, 0xE9, 0xFE, 0xAA};

typedef struct dimensions_figure {
    RFILE* dim_file;
    u8 data[DIMENSIONS_FIGURE_SIZE];
    u8 index;
    u8 pad;
    u32 id;
} dimensions_figure_t;

/* The toypad */
static combiner_t m_dimensions_mutex;
static dimensions_figure_t m_figures[MAX_DIMENSIONS_FIGURES];
static u32 m_random_a, m_random_b, m_random_c, m_random_d;
static usb_reply_ring_t m_figure_added_removed_responses;

/* The answers queued for the IN endpoint, and the writes waiting to be given back */
static combiner_t m_query_mutex;
static usb_reply_ring_t m_queries;
static struct libusb_transfer* m_done[DIM_DONE_SLOTS];
static u32 m_done_head;
static u32 m_done_count;
/* Told whenever an answer is queued: a read waits on it */
static retro_eventcount_t m_answered;

static u32 rotl32(u32 v, unsigned n) {
    return (v << n) | (v >> (32 - n));
}

static u32 rotr32(u32 v, unsigned n) {
    return (v >> n) | (v << (32 - n));
}

static u32 le32(const u8* b) {
    return (u32)b[0] | ((u32)b[1] << 8) | ((u32)b[2] << 16) | ((u32)b[3] << 24);
}

static void put_le32(u8* b, u32 v) {
    b[0] = (u8)(v & 0xFF);
    b[1] = (u8)((v >> 8) & 0xFF);
    b[2] = (u8)((v >> 16) & 0xFF);
    b[3] = (u8)((v >> 24) & 0xFF);
}

static u8 dim_checksum(const u8* data, u32 num_of_bytes) {
    int checksum = 0;
    u8 i;
    C_ASSERT(num_of_bytes <= DIM_REPLY, "GenerateChecksum");
    for (i = 0; i < num_of_bytes; i++) {
        checksum += data[i];
    }
    return (u8)(checksum & 0xFF);
}

static void dim_save(dimensions_figure_t* figure) {
    if (!figure->dim_file) {
        return;
    }
    filestream_seek(figure->dim_file, 0, RETRO_VFS_SEEK_POSITION_START);
    filestream_write(figure->dim_file, figure->data, sizeof(figure->data));
}

static void dim_close(dimensions_figure_t* figure) {
    if (figure->dim_file) {
        filestream_close(figure->dim_file);
        figure->dim_file = NULL;
    }
}

/* XTEA, 32 rounds, with the figure's key or (NULL) the command key */
static void dim_decrypt(const u8* buf, const u8* key, u8* out) {
    u32 data_one = le32(buf), data_two = le32(buf + 4);
    const u8* k = key ? key : COMMAND_KEY;
    const u32 key_one = le32(k), key_two = le32(k + 4), key_three = le32(k + 8),
              key_four = le32(k + 12);
    u32 sum = 0xC6EF3720;
    const u32 delta = 0x9E3779B9;
    int i;
    for (i = 0; i < 32; i++) {
        data_two -=
            (((data_one << 4) + key_three) ^ (data_one + sum) ^ ((data_one >> 5) + key_four));
        data_one -= (((data_two << 4) + key_one) ^ (data_two + sum) ^ ((data_two >> 5) + key_two));
        sum -= delta;
    }
    C_ASSERT_MSG(sum == 0, "Decrypt", "Decryption failed, sum inequal to 0");
    put_le32(out, data_one);
    put_le32(out + 4, data_two);
}

static void dim_encrypt(const u8* buf, const u8* key, u8* out) {
    u32 data_one = le32(buf), data_two = le32(buf + 4);
    const u8* k = key ? key : COMMAND_KEY;
    const u32 key_one = le32(k), key_two = le32(k + 4), key_three = le32(k + 8),
              key_four = le32(k + 12);
    u32 sum = 0;
    const u32 delta = 0x9E3779B9;
    int i;
    for (i = 0; i < 32; i++) {
        sum += delta;
        data_one += (((data_two << 4) + key_one) ^ (data_two + sum) ^ ((data_two >> 5) + key_two));
        data_two +=
            (((data_one << 4) + key_three) ^ (data_one + sum) ^ ((data_one >> 5) + key_four));
    }
    put_le32(out, data_one);
    put_le32(out + 4, data_two);
}

static u32 dim_randomize(const u8* key, u8 count) {
    u32 scrambled = 0;
    u8 i;
    for (i = 0; i < count; i++) {
        const u32 v4 = rotr32(scrambled, 25);
        const u32 v5 = rotr32(scrambled, 10);
        scrambled = le32(&key[i * 4]) + v4 + v5 - scrambled;
    }
    return scrambled;
}

static u32 dim_scramble(const u8* uid, u8 count) {
    u8 to_scramble[7 + 17];
    u32 randomized;
    memcpy(to_scramble, uid, 7);
    memcpy(to_scramble + 7, CHAR_CONSTANT, sizeof(CHAR_CONSTANT));
    to_scramble[(count * 4) - 1] = 0xaa;
    randomized = dim_randomize(to_scramble, count);
    /* The randomized bytes (little endian) read big endian */
    return ((randomized & 0xFF) << 24) | (((randomized >> 8) & 0xFF) << 16) |
           (((randomized >> 16) & 0xFF) << 8) | ((randomized >> 24) & 0xFF);
}

static void dim_figure_key(const u8* buf, u8* key) {
    u8 uid[7];
    u8 i;
    uid[0] = buf[0];
    uid[1] = buf[1];
    uid[2] = buf[2];
    uid[3] = buf[4];
    uid[4] = buf[5];
    uid[5] = buf[6];
    uid[6] = buf[7];
    for (i = 0; i < 4; i++) {
        const u32 s = dim_scramble(uid, (u8)(3 + i));
        key[i * 4 + 0] = (u8)((s >> 24) & 0xFF);
        key[i * 4 + 1] = (u8)((s >> 16) & 0xFF);
        key[i * 4 + 2] = (u8)((s >> 8) & 0xFF);
        key[i * 4 + 3] = (u8)(s & 0xFF);
    }
}

static u32 dim_figure_id(const u8* buf) {
    u8 figure_key[16];
    u8 decrypted[8];
    u32 fig_num;
    dim_figure_key(buf, figure_key);
    dim_decrypt(&buf[36 * 4], figure_key, decrypted);
    fig_num = le32(decrypted);
    /* Characters have their model number encrypted in page 36 */
    if (fig_num < 1000) {
        return fig_num;
    }
    /* Vehicles/Gadgets have their model number written as little endian in page 36 */
    return le32(&buf[36 * 4]);
}

static u32 dim_get_next(void) {
    const u32 e = m_random_a - rotl32(m_random_b, 21);
    m_random_a = m_random_b ^ rotl32(m_random_c, 19);
    m_random_b = m_random_c + rotl32(m_random_d, 6);
    m_random_c = m_random_d + e;
    m_random_d = e + m_random_a;
    return m_random_d;
}

static void dim_initialize_rng(u32 seed) {
    int i;
    m_random_a = 0xF1EA5EED;
    m_random_b = seed;
    m_random_c = seed;
    m_random_d = seed;
    for (i = 0; i < 42; i++) {
        dim_get_next();
    }
}

static void dim_queue_answer(usb_reply_ring_t* ring, const u8* answer) {
    usb_reply_ring_push(ring, answer, DIM_REPLY);
    retro_eventcount_notify(&m_answered);
}

/* The figure added (@direction 0) or removed (1) answer */
static void dim_figure_change(const dimensions_figure_t* figure, u8 direction) {
    u8 response[DIM_REPLY];
    memset(response, 0, sizeof(response));
    response[0] = 0x56;
    response[1] = 0x0b;
    response[2] = figure->pad;
    response[3] = 0x00;
    response[4] = figure->index;
    response[5] = direction;
    response[6] = figure->data[0];
    response[7] = figure->data[1];
    response[8] = figure->data[2];
    response[9] = figure->data[4];
    response[10] = figure->data[5];
    response[11] = figure->data[6];
    response[12] = figure->data[7];
    response[13] = dim_checksum(response, 13);
    dim_queue_answer(&m_figure_added_removed_responses, response);
}

typedef struct dim_figure_call {
    u8 pad;
    u8 index;
    int full_remove;
    const u8* buf;
    RFILE* file;
} dim_figure_call_t;

static void dim_load_locked(void* ctx) {
    dim_figure_call_t* call = (dim_figure_call_t*)ctx;
    dimensions_figure_t* figure = &m_figures[call->index];
    const u32 id = dim_figure_id(call->buf);
    if (figure->dim_file != call->file) {
        dim_close(figure);
    }
    figure->dim_file = call->file;
    figure->id = id;
    figure->pad = call->pad;
    figure->index = (u8)(call->index + 1);
    memcpy(figure->data, call->buf, sizeof(figure->data));
    /* When a figure is added to the toypad, respond to the game with the pad they were added
     * to, their index, the direction (0x00 in byte 6 for added) and their UID */
    dim_figure_change(figure, 0x00);
}

static void dim_load(const u8* buf, RFILE* file, u8 pad, u8 index) {
    dim_figure_call_t call;
    call.pad = pad;
    call.index = index;
    call.buf = buf;
    call.file = file;
    combiner_run(&m_dimensions_mutex, dim_load_locked, &call);
}

void dimensions_load_figure(const char* file_name, u8 pad, u8 index) {
    u8 data[DIMENSIONS_FIGURE_SIZE];
    RFILE* file = filestream_open(
        file_name, RETRO_VFS_FILE_ACCESS_READ_WRITE | RETRO_VFS_FILE_ACCESS_UPDATE_EXISTING,
        RETRO_VFS_FILE_ACCESS_HINT_NONE);
    C_ASSERT(file && filestream_read(file, data, sizeof(data)) == (s64)sizeof(data), "LoadFigure");
    dim_load(data, file, pad, index);
}

static void dim_remove_locked(void* ctx) {
    dim_figure_call_t* call = (dim_figure_call_t*)ctx;
    dimensions_figure_t* figure = &m_figures[call->index];
    if (figure->index == 255) {
        return;
    }
    /* When a figure is removed from the toypad, respond to the game with the pad they were
     * removed from, their index, the direction (0x01 in byte 6 for removed) and their UID */
    if (call->full_remove) {
        dim_figure_change(figure, 0x01);
        dim_save(figure);
        dim_close(figure);
    }
    figure->index = 255;
    figure->pad = 255;
    figure->id = 0;
}

void dimensions_remove_figure(u8 pad, u8 index, int full_remove) {
    dim_figure_call_t call;
    call.pad = pad;
    call.index = index;
    call.full_remove = full_remove;
    combiner_run(&m_dimensions_mutex, dim_remove_locked, &call);
}

static void dim_pick_locked(void* ctx) {
    const dim_figure_call_t* call = (const dim_figure_call_t*)ctx;
    const dimensions_figure_t* figure = &m_figures[call->index];
    if (figure->index == 255) {
        return;
    }
    /* Picked up (0x01) until the move is cancelled or a space chosen; put back (0x00) */
    dim_figure_change(figure, call->full_remove ? 0x01 : 0x00);
}

void dimensions_temp_remove_figure(u8 index) {
    dim_figure_call_t call;
    call.index = index;
    call.full_remove = 1;
    combiner_run(&m_dimensions_mutex, dim_pick_locked, &call);
}

void dimensions_cancel_remove_figure(u8 index) {
    dim_figure_call_t call;
    call.index = index;
    call.full_remove = 0;
    combiner_run(&m_dimensions_mutex, dim_pick_locked, &call);
}

void dimensions_move_figure(u8 new_pad, u8 new_index, u8 old_pad, u8 old_index) {
    u8 data[DIMENSIONS_FIGURE_SIZE];
    RFILE* in_file;
    if (old_index == new_index) {
        /* Don't bother removing and loading again, just send response to the game */
        dimensions_cancel_remove_figure(new_index);
        return;
    }
    /* Remove any figure from the space moved to, then this one from its space, then load it
     * to the new one */
    dimensions_remove_figure(new_pad, new_index, 1);
    memcpy(data, m_figures[old_index].data, sizeof(data));
    in_file = m_figures[old_index].dim_file;
    m_figures[old_index].dim_file = NULL;
    dimensions_remove_figure(old_pad, old_index, 0);
    dim_load(data, in_file, new_pad, new_index);
}

static void dim_blank_response(u8 type, u8 sequence, u8* reply_buf) {
    reply_buf[0] = 0x55;
    reply_buf[1] = type;
    reply_buf[2] = sequence;
    reply_buf[3] = dim_checksum(reply_buf, 3);
}

static void dim_generate_random_number(const u8* buf, u8 sequence, u8* reply_buf) {
    u8 value[8];
    u8 value_to_encrypt[8];
    u8 encrypted[8];
    u32 seed;
    /* Decrypt payload into an 8 byte array; the seed is its first 4 bytes, read as the host
     * reads them */
    dim_decrypt(buf, NULL, value);
    memcpy(&seed, value, sizeof(seed));
    dim_initialize_rng(seed);
    /* Encrypt 8 bytes: the decrypted confirmation, then blank */
    memset(value_to_encrypt, 0, sizeof(value_to_encrypt));
    memcpy(value_to_encrypt, &value[4], 4);
    dim_encrypt(value_to_encrypt, NULL, encrypted);
    reply_buf[0] = 0x55;
    reply_buf[1] = 0x09;
    reply_buf[2] = sequence;
    memcpy(&reply_buf[3], encrypted, sizeof(encrypted));
    reply_buf[11] = dim_checksum(reply_buf, 11);
}

static void dim_challenge_response(const u8* buf, u8 sequence, u8* reply_buf) {
    u8 value[8];
    u8 value_to_encrypt[8];
    u8 encrypted[8];
    dim_decrypt(buf, NULL, value);
    /* The next random number (little endian), then the confirmation from the payload */
    put_le32(value_to_encrypt, dim_get_next());
    memcpy(&value_to_encrypt[4], value, 4);
    dim_encrypt(value_to_encrypt, NULL, encrypted);
    reply_buf[0] = 0x55;
    reply_buf[1] = 0x09;
    reply_buf[2] = sequence;
    memcpy(&reply_buf[3], encrypted, sizeof(encrypted));
    reply_buf[11] = dim_checksum(reply_buf, 11);
}

typedef struct dim_block_call {
    u8 index;
    u8 page;
    const u8* to_write_buf;
    u8* reply_buf;
    u8 sequence;
} dim_block_call_t;

static void dim_query_block_locked(void* ctx) {
    const dim_block_call_t* call = (const dim_block_call_t*)ctx;
    /* Index from game begins at 1 rather than 0 */
    const u8 figure_index = (u8)(call->index - 1);
    call->reply_buf[0] = 0x55;
    call->reply_buf[1] = 0x12;
    call->reply_buf[2] = call->sequence;
    call->reply_buf[3] = 0x00;
    if (figure_index < MAX_DIMENSIONS_FIGURES) {
        const dimensions_figure_t* figure = &m_figures[figure_index];
        /* Query 4 pages of 4 bytes from the figure */
        if (figure->index != 255 && (4 * call->page) < ((0x2D * 4) - 16)) {
            memcpy(&call->reply_buf[4], figure->data + (4 * call->page), 16);
        }
    }
    call->reply_buf[20] = dim_checksum(call->reply_buf, 20);
}

static void dim_write_block_locked(void* ctx) {
    const dim_block_call_t* call = (const dim_block_call_t*)ctx;
    const u8 figure_index = (u8)(call->index - 1);
    call->reply_buf[0] = 0x55;
    call->reply_buf[1] = 0x02;
    call->reply_buf[2] = call->sequence;
    call->reply_buf[3] = 0x00;
    if (figure_index < MAX_DIMENSIONS_FIGURES) {
        dimensions_figure_t* figure = &m_figures[figure_index];
        /* Copy 4 bytes to the page on the figure requested by the game */
        if (figure->index != 255 && call->page < 0x2D) {
            /* Id is written to page 36 */
            if (call->page == 36) {
                figure->id = le32(call->to_write_buf);
            }
            memcpy(figure->data + (call->page * 4), call->to_write_buf, 4);
            dim_save(figure);
        }
    }
    call->reply_buf[4] = dim_checksum(call->reply_buf, 4);
}

static void dim_block(u8 index, u8 page, const u8* to_write_buf, u8* reply_buf, u8 sequence,
                      void (*locked)(void*)) {
    dim_block_call_t call;
    call.index = index;
    call.page = page;
    call.to_write_buf = to_write_buf;
    call.reply_buf = reply_buf;
    call.sequence = sequence;
    combiner_run(&m_dimensions_mutex, locked, &call);
}

static void dim_get_model(const u8* buf, u8 sequence, u8* reply_buf) {
    u8 value[8];
    u8 value_to_encrypt[8];
    u8 encrypted[8];
    u8 figure_index;
    /* Decrypt payload: byte 1 is the index, 4-7 the confirmation */
    dim_decrypt(buf, NULL, value);
    figure_index = (u8)(value[0] - 1);
    memset(value_to_encrypt, 0, sizeof(value_to_encrypt));
    /* The figure's id (little endian), then the confirmation */
    if (figure_index < MAX_DIMENSIONS_FIGURES) {
        put_le32(value_to_encrypt, m_figures[figure_index].id);
        memcpy(&value_to_encrypt[4], &value[4], 4);
    }
    dim_encrypt(value_to_encrypt, NULL, encrypted);
    reply_buf[0] = 0x55;
    reply_buf[1] = 0x0a;
    reply_buf[2] = sequence;
    reply_buf[3] = 0x00;
    memcpy(&reply_buf[4], encrypted, sizeof(encrypted));
    reply_buf[12] = dim_checksum(reply_buf, 12);
}

typedef struct dim_read_call {
    struct libusb_transfer* transfer;
    int responded;
} dim_read_call_t;

/* An added or removed figure, under the toypad's own combiner as they are queued */
static void dim_pop_figure_change_locked(void* ctx) {
    dim_read_call_t* call = (dim_read_call_t*)ctx;
    const u8* response = usb_reply_ring_front(&m_figure_added_removed_responses);
    if (response) {
        memcpy(call->transfer->buffer, response, 0x20);
        usb_reply_ring_pop(&m_figure_added_removed_responses);
        call->responded = 1;
    }
}

/* An added or removed figure first, else the oldest answer to a write */
static void dim_read_locked(void* ctx) {
    dim_read_call_t* call = (dim_read_call_t*)ctx;
    const u8* response;
    combiner_run(&m_dimensions_mutex, dim_pop_figure_change_locked, call);
    if (!call->responded) {
        response = usb_reply_ring_front(&m_queries);
        if (!response) {
            return;
        }
        memcpy(call->transfer->buffer, response, 0x20);
        usb_reply_ring_pop(&m_queries);
        call->responded = 1;
    }
    call->transfer->length = 32;
}

static void dim_push_query_locked(void* ctx) {
    dim_queue_answer(&m_queries, (const u8*)ctx);
}

static enum libusb_transfer_status dim_handle_async(struct libusb_transfer* transfer) {
    C_ASSERT(transfer->length == 32, "HandleAsyncTransfer");

    switch (transfer->endpoint) {
    case 0x81: {
        /* Read: an added/removed figure, or an answer to a write; waits for one */
        dim_read_call_t call;
        call.transfer = transfer;
        call.responded = 0;
        for (;;) {
            int key;
            combiner_run(&m_query_mutex, dim_read_locked, &call);
            if (call.responded) {
                break;
            }
            key = retro_eventcount_prepare_wait(&m_answered);
            combiner_run(&m_query_mutex, dim_read_locked, &call);
            if (call.responded) {
                retro_eventcount_cancel_wait(&m_answered);
                break;
            }
            retro_eventcount_commit_wait(&m_answered, key);
        }
        break;
    }
    case 0x01: {
        /* Write: byte 3 the command, byte 4 the sequence, the payload, then a checksum */
        static const u8 wake[] = {0x55, 0x0e, 0x01, 0x28, 0x63, 0x29, 0x20, 0x4c, 0x45,
                                  0x47, 0x4f, 0x20, 0x32, 0x30, 0x31, 0x34, 0x46};
        const u8 command = transfer->buffer[2];
        const u8 sequence = transfer->buffer[3];
        u8 q_result[DIM_REPLY];
        memset(q_result, 0, sizeof(q_result));

        switch (command) {
        case 0xB0: /* Wake: the device's own answer */
            memcpy(q_result, wake, sizeof(wake));
            break;
        case 0xB1: /* Seed */
            dim_generate_random_number(&transfer->buffer[4], sequence, q_result);
            break;
        case 0xB3: /* Challenge */
            dim_challenge_response(&transfer->buffer[4], sequence, q_result);
            break;
        case 0xC0: /* Color */
        case 0xC1: /* Get Pad Color */
        case 0xC2: /* Fade */
        case 0xC3: /* Flash */
        case 0xC4: /* Fade Random */
        case 0xC6: /* Fade All */
        case 0xC7: /* Flash All */
        case 0xC8: /* Color All */
            dim_blank_response(0x01, sequence, q_result);
            break;
        case 0xD2: /* Read 4 pages from the figure at index buf[4], from page buf[5] */
            dim_block(transfer->buffer[4], transfer->buffer[5], NULL, q_result, sequence,
                      dim_query_block_locked);
            break;
        case 0xD3: /* Write 4 bytes to page buf[5] of the figure at index buf[4] */
            dim_block(transfer->buffer[4], transfer->buffer[5], &transfer->buffer[6], q_result,
                      sequence, dim_write_block_locked);
            break;
        case 0xD4: /* Model */
            dim_get_model(&transfer->buffer[4], sequence, q_result);
            break;
        case 0xD0: /* Tag List */
        case 0xE1: /* PWD */
        case 0xE5: /* Active */
        case 0xFF: /* LEDS Query */
            c_log(C_LOG_LIB_USBD, C_LOG_LEVEL_ERROR, "HandleAsyncTransfer",
                  "Unimplemented LD Function: %x", command);
            break;
        default:
            c_log(C_LOG_LIB_USBD, C_LOG_LEVEL_ERROR, "HandleAsyncTransfer",
                  "Unknown LD Function: %x", command);
            break;
        }
        combiner_run(&m_query_mutex, dim_push_query_locked, q_result);
        break;
    }
    default:
        break;
    }
    return LIBUSB_TRANSFER_COMPLETED;
}

typedef struct dim_done_call {
    struct libusb_transfer* transfer;
    int ok;
} dim_done_call_t;

static void dim_push_done_locked(void* ctx) {
    dim_done_call_t* call = (dim_done_call_t*)ctx;
    if (m_done_count == DIM_DONE_SLOTS) {
        return;
    }
    m_done[(m_done_head + m_done_count) % DIM_DONE_SLOTS] = call->transfer;
    m_done_count++;
    call->ok = 1;
}

static void dim_pop_done_locked(void* ctx) {
    dim_done_call_t* call = (dim_done_call_t*)ctx;
    if (!m_done_count) {
        return;
    }
    call->transfer = m_done[m_done_head];
    m_done_head = (m_done_head + 1) % DIM_DONE_SLOTS;
    m_done_count--;
    call->ok = 1;
}

/* A write is answered here and now, so a read waiting for its answer is not stuck behind it;
 * the write itself is given back by the next sceUsbdHandleEvents, as libusb does */
static s32 dim_submit_transfer(struct libusb_transfer* transfer) {
    dim_done_call_t call;
    if (transfer->endpoint != 0x01) {
        return usb_backend_queue_transfer(transfer);
    }
    dim_handle_async(transfer);
    call.transfer = transfer;
    call.ok = 0;
    combiner_run(&m_query_mutex, dim_push_done_locked, &call);
    if (!call.ok) {
        c_log(C_LOG_LIB_USBD, C_LOG_LEVEL_ERROR, "SubmitTransfer",
              "%u writes are waiting to be given back: this one is not", DIM_DONE_SLOTS);
        return LIBUSB_ERROR_NO_MEM;
    }
    return LIBUSB_SUCCESS;
}

static int dim_give_back_done(void) {
    dim_done_call_t call;
    struct libusb_transfer* transfer;
    u8 flags;
    call.transfer = NULL;
    call.ok = 0;
    combiner_run(&m_query_mutex, dim_pop_done_locked, &call);
    if (!call.ok) {
        return 0;
    }
    transfer = call.transfer;
    flags = transfer->flags;
    transfer->status = LIBUSB_TRANSFER_COMPLETED;
    transfer->actual_length = transfer->length;
    if (transfer->callback) {
        transfer->callback(transfer);
    }
    if (flags & LIBUSB_TRANSFER_FREE_TRANSFER) {
        libusb_free_transfer(transfer);
    }
    return 1;
}

static void dim_init(void) {
    u8 i;
    combiner_init(&m_dimensions_mutex);
    combiner_init(&m_query_mutex);
    retro_eventcount_init(&m_answered);
    for (i = 0; i < MAX_DIMENSIONS_FIGURES; i++) {
        m_figures[i].index = 255;
        m_figures[i].pad = 255;
    }
}

static struct libusb_endpoint_descriptor m_endpoint_descriptors[2] = {
    {0x7, 0x5, 0x81, 0x3, 0x20, 0x1, 0x0, 0x0, NULL, 0},
    {0x7, 0x5, 0x1, 0x3, 0x20, 0x1, 0x0, 0x0, NULL, 0}};
static struct libusb_interface_descriptor m_interface_descriptor = {0x9, 0x4, 0x0, 0x0,  0x2,  0x3,
                                                                    0x0, 0x0, 0x0, NULL, NULL, 0};
static struct libusb_config_descriptor m_config_descriptor = {0x9,  0x2,  0x29, 0x1,  0x1, 0x0,
                                                              0x80, 0xFA, NULL, NULL, 0};
static struct libusb_device_descriptor m_device_descriptor = {
    0x12, 0x1, 0x200, 0x0, 0x0, 0x0, 0x20, 0x0E6F, 0x0241, 0x200, 0x1, 0x2, 0x0, 0x1};

const usb_emulated_device_t usb_dimensions_toypad = {m_endpoint_descriptors,
                                                     &m_interface_descriptor,
                                                     &m_config_descriptor,
                                                     &m_device_descriptor,
                                                     dim_init,
                                                     NULL,
                                                     dim_handle_async,
                                                     dim_submit_transfer,
                                                     dim_give_back_done,
                                                     32};
