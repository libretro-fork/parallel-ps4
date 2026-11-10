/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef SHADNET_PB_WIRE_H
#define SHADNET_PB_WIRE_H

/* Protocol Buffers wire format (proto3) for plain C89 structs.
 *
 * A message is a struct plus a table of its fields (pb_msg_desc_t), written
 * by gen_messages.py from shadnet.proto. Encoding follows proto3: a scalar
 * field at its zero value is left out, repeated scalars are packed, a
 * message field is written when its has_ flag is set. Decoding takes
 * repeated scalars packed or not, skips fields it does not know, and keeps
 * the last value of a scalar seen twice.
 *
 * Strings and bytes are borrowed both ways: a decoded pb_bytes_t points
 * into the buffer it was decoded from, and an encoded one is read from
 * wherever the caller pointed it. Repeated fields decode into arrays laid
 * out in a caller's scratch block, sized first with pb_decode_scratch(). */

#include <stddef.h>
#include <stdint.h>

#include <boolean.h>
#include <retro_common_api.h>

RETRO_BEGIN_DECLS

typedef struct pb_bytes {
    const uint8_t* data;
    size_t size;
} pb_bytes_t;

enum pb_type {
    PB_UINT32 = 0,
    PB_INT32,
    PB_UINT64,
    PB_INT64,
    PB_BOOL,
    PB_STRING,
    PB_BYTES,
    PB_MESSAGE
};

struct pb_msg_desc;

typedef struct pb_field {
    uint32_t number;
    uint8_t type;     /* enum pb_type */
    uint8_t repeated; /* the member is { T* items; size_t count; } */
    size_t offset;
    size_t has_offset; /* a message field's uint8_t has_ flag */
    const struct pb_msg_desc* sub;
} pb_field_t;

typedef struct pb_msg_desc {
    size_t size;
    const pb_field_t* fields; /* by field number */
    size_t num_fields;
} pb_msg_desc_t;

/* The size of @msg encoded */
size_t pb_encoded_size(const pb_msg_desc_t* desc, const void* msg);

/* Writes @msg to @out, which holds pb_encoded_size() bytes; returns the
 * number written. */
size_t pb_encode(const pb_msg_desc_t* desc, const void* msg, uint8_t* out);

/* The scratch pb_decode needs for the arrays of @data, or (size_t)-1 when
 * @data is not a well-formed message. */
size_t pb_decode_scratch(const pb_msg_desc_t* desc, const uint8_t* data, size_t size);

/* Decodes @data into @msg, which is zeroed first. Repeated fields are laid
 * out in @scratch, of at least pb_decode_scratch() bytes. Strings, bytes
 * and arrays stay valid while @data and @scratch do. Returns false when
 * @data is malformed. */
bool pb_decode(const pb_msg_desc_t* desc, void* msg, const uint8_t* data, size_t size,
               void* scratch, size_t scratch_size);

RETRO_END_DECLS

#endif
