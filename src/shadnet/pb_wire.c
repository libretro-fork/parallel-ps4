/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <string.h>

#include "shadnet/pb_wire.h"

#define PB_WIRE_VARINT 0
#define PB_WIRE_FIXED64 1
#define PB_WIRE_LEN 2
#define PB_WIRE_FIXED32 5

/* The most fields a message has; shadnet.proto's largest has 30 */
#define PB_MAX_FIELDS 64
#define PB_ALIGN(n) (((n) + 7) & ~(size_t)7)
#define PB_BAD ((size_t)-1)

typedef struct pb_array {
    void* items;
    size_t count;
} pb_array_t;

static size_t pb_elem_size(const pb_field_t* f) {
    switch (f->type) {
    case PB_UINT32:
    case PB_INT32:
        return 4;
    case PB_UINT64:
    case PB_INT64:
        return 8;
    case PB_BOOL:
        return 1;
    case PB_STRING:
    case PB_BYTES:
        return sizeof(pb_bytes_t);
    default:
        return f->sub->size;
    }
}

static bool pb_is_numeric(const pb_field_t* f) {
    return f->type <= PB_BOOL;
}

/* ------------------------------------------------------------------ */
/* Encoding                                                           */
/* ------------------------------------------------------------------ */

static size_t pb_varint_size(uint64_t v) {
    size_t n = 1;
    while (v >= 0x80) {
        v >>= 7;
        n++;
    }
    return n;
}

static uint8_t* pb_put_varint(uint8_t* out, uint64_t v) {
    while (v >= 0x80) {
        *out++ = (uint8_t)(v | 0x80);
        v >>= 7;
    }
    *out++ = (uint8_t)v;
    return out;
}

/* A numeric element as the varint it is written as; int32 is sign-extended
 * to 64 bits, as protobuf writes it. */
static uint64_t pb_numeric(const pb_field_t* f, const void* p) {
    switch (f->type) {
    case PB_UINT32:
        return *(const uint32_t*)p;
    case PB_INT32:
        return (uint64_t)(int64_t)*(const int32_t*)p;
    case PB_UINT64:
        return *(const uint64_t*)p;
    case PB_INT64:
        return (uint64_t)*(const int64_t*)p;
    default:
        return *(const uint8_t*)p ? 1 : 0;
    }
}

static size_t pb_tag_size(const pb_field_t* f) {
    return pb_varint_size((uint64_t)f->number << 3);
}

static size_t pb_packed_payload(const pb_field_t* f, const pb_array_t* a) {
    size_t i, n = 0, es = pb_elem_size(f);
    for (i = 0; i < a->count; i++)
        n += pb_varint_size(pb_numeric(f, (const uint8_t*)a->items + i * es));
    return n;
}

size_t pb_encoded_size(const pb_msg_desc_t* desc, const void* msg) {
    const uint8_t* base = (const uint8_t*)msg;
    size_t i, n = 0;
    for (i = 0; i < desc->num_fields; i++) {
        const pb_field_t* f = &desc->fields[i];
        const void* p = base + f->offset;
        if (f->repeated) {
            const pb_array_t* a = (const pb_array_t*)p;
            size_t j, es = pb_elem_size(f);
            if (!a->count)
                continue;
            if (pb_is_numeric(f)) {
                size_t payload = pb_packed_payload(f, a);
                n += pb_tag_size(f) + pb_varint_size(payload) + payload;
                continue;
            }
            for (j = 0; j < a->count; j++) {
                const uint8_t* e = (const uint8_t*)a->items + j * es;
                size_t len = (f->type == PB_MESSAGE) ? pb_encoded_size(f->sub, e)
                                                     : ((const pb_bytes_t*)e)->size;
                n += pb_tag_size(f) + pb_varint_size(len) + len;
            }
        } else if (pb_is_numeric(f)) {
            uint64_t v = pb_numeric(f, p);
            if (v)
                n += pb_tag_size(f) + pb_varint_size(v);
        } else if (f->type == PB_MESSAGE) {
            size_t len;
            if (!base[f->has_offset])
                continue;
            len = pb_encoded_size(f->sub, p);
            n += pb_tag_size(f) + pb_varint_size(len) + len;
        } else {
            size_t len = ((const pb_bytes_t*)p)->size;
            if (len)
                n += pb_tag_size(f) + pb_varint_size(len) + len;
        }
    }
    return n;
}

static uint8_t* pb_put_tag(uint8_t* out, const pb_field_t* f, unsigned wire) {
    return pb_put_varint(out, ((uint64_t)f->number << 3) | wire);
}

static uint8_t* pb_put_len(uint8_t* out, const pb_field_t* f, const void* e) {
    if (f->type == PB_MESSAGE) {
        size_t len = pb_encoded_size(f->sub, e);
        out = pb_put_tag(out, f, PB_WIRE_LEN);
        out = pb_put_varint(out, len);
        return out + pb_encode(f->sub, e, out);
    } else {
        const pb_bytes_t* b = (const pb_bytes_t*)e;
        out = pb_put_tag(out, f, PB_WIRE_LEN);
        out = pb_put_varint(out, b->size);
        if (b->size)
            memcpy(out, b->data, b->size);
        return out + b->size;
    }
}

size_t pb_encode(const pb_msg_desc_t* desc, const void* msg, uint8_t* out) {
    const uint8_t* base = (const uint8_t*)msg;
    uint8_t* start = out;
    size_t i;
    for (i = 0; i < desc->num_fields; i++) {
        const pb_field_t* f = &desc->fields[i];
        const void* p = base + f->offset;
        if (f->repeated) {
            const pb_array_t* a = (const pb_array_t*)p;
            size_t j, es = pb_elem_size(f);
            if (!a->count)
                continue;
            if (pb_is_numeric(f)) {
                out = pb_put_tag(out, f, PB_WIRE_LEN);
                out = pb_put_varint(out, pb_packed_payload(f, a));
                for (j = 0; j < a->count; j++)
                    out = pb_put_varint(out, pb_numeric(f, (const uint8_t*)a->items + j * es));
                continue;
            }
            for (j = 0; j < a->count; j++)
                out = pb_put_len(out, f, (const uint8_t*)a->items + j * es);
        } else if (pb_is_numeric(f)) {
            uint64_t v = pb_numeric(f, p);
            if (v) {
                out = pb_put_tag(out, f, PB_WIRE_VARINT);
                out = pb_put_varint(out, v);
            }
        } else if (f->type == PB_MESSAGE) {
            if (base[f->has_offset])
                out = pb_put_len(out, f, p);
        } else if (((const pb_bytes_t*)p)->size) {
            out = pb_put_len(out, f, p);
        }
    }
    return (size_t)(out - start);
}

/* ------------------------------------------------------------------ */
/* Decoding                                                           */
/* ------------------------------------------------------------------ */

typedef struct pb_reader {
    const uint8_t* p;
    const uint8_t* end;
} pb_reader_t;

static bool pb_get_varint(pb_reader_t* r, uint64_t* v) {
    uint64_t x = 0;
    unsigned shift = 0;
    while (r->p < r->end && shift < 64) {
        uint8_t b = *r->p++;
        x |= (uint64_t)(b & 0x7F) << shift;
        if (!(b & 0x80)) {
            *v = x;
            return true;
        }
        shift += 7;
    }
    return false;
}

/* One field's header and, for a length-delimited one, its bytes */
typedef struct pb_item {
    uint32_t number;
    unsigned wire;
    uint64_t value;      /* a varint */
    const uint8_t* data; /* a length-delimited payload */
    size_t size;
} pb_item_t;

static bool pb_next(pb_reader_t* r, pb_item_t* it) {
    uint64_t key, len;
    if (!pb_get_varint(r, &key) || (key >> 3) == 0 || (key >> 3) > 0x1FFFFFFF)
        return false;
    it->number = (uint32_t)(key >> 3);
    it->wire = (unsigned)(key & 7);
    switch (it->wire) {
    case PB_WIRE_VARINT:
        return pb_get_varint(r, &it->value);
    case PB_WIRE_FIXED64:
        if (r->end - r->p < 8)
            return false;
        r->p += 8;
        return true;
    case PB_WIRE_FIXED32:
        if (r->end - r->p < 4)
            return false;
        r->p += 4;
        return true;
    case PB_WIRE_LEN:
        if (!pb_get_varint(r, &len) || len > (uint64_t)(r->end - r->p))
            return false;
        it->data = r->p;
        it->size = (size_t)len;
        r->p += len;
        return true;
    default:
        return false; /* groups are not proto3 */
    }
}

static const pb_field_t* pb_find(const pb_msg_desc_t* desc, uint32_t number) {
    size_t lo = 0, hi = desc->num_fields;
    while (lo < hi) {
        size_t mid = (lo + hi) / 2;
        if (desc->fields[mid].number < number)
            lo = mid + 1;
        else
            hi = mid;
    }
    return (lo < desc->num_fields && desc->fields[lo].number == number) ? &desc->fields[lo] : NULL;
}

/* How a known field's item lands: 0 ignore, 1 one varint, 2 a packed run,
 * 3 one length-delimited value. */
static int pb_kind(const pb_field_t* f, unsigned wire) {
    if (pb_is_numeric(f)) {
        if (wire == PB_WIRE_VARINT)
            return 1;
        if (wire == PB_WIRE_LEN && f->repeated)
            return 2;
        return 0;
    }
    return wire == PB_WIRE_LEN ? 3 : 0;
}

/* The number of varints in a packed run, or PB_BAD */
static size_t pb_packed_count(const uint8_t* p, size_t size) {
    size_t i, n = 0;
    for (i = 0; i < size; i++)
        if (!(p[i] & 0x80))
            n++;
    if (size && (p[size - 1] & 0x80))
        return PB_BAD;
    return n;
}

size_t pb_decode_scratch(const pb_msg_desc_t* desc, const uint8_t* data, size_t size) {
    size_t counts[PB_MAX_FIELDS];
    size_t i, total = 0;
    pb_reader_t r;
    pb_item_t it;
    if (desc->num_fields > PB_MAX_FIELDS)
        return PB_BAD;
    memset(counts, 0, sizeof(counts));
    r.p = data;
    r.end = data + size;
    while (r.p < r.end) {
        const pb_field_t* f;
        int kind;
        if (!pb_next(&r, &it))
            return PB_BAD;
        if (!(f = pb_find(desc, it.number)) || !(kind = pb_kind(f, it.wire)))
            continue;
        if (kind == 2) {
            size_t n = pb_packed_count(it.data, it.size);
            if (n == PB_BAD)
                return PB_BAD;
            counts[f - desc->fields] += n;
            continue;
        }
        if (f->type == PB_MESSAGE) {
            size_t sub = pb_decode_scratch(f->sub, it.data, it.size);
            if (sub == PB_BAD)
                return PB_BAD;
            total += sub;
        }
        if (f->repeated)
            counts[f - desc->fields]++;
    }
    for (i = 0; i < desc->num_fields; i++)
        if (counts[i])
            total += PB_ALIGN(counts[i] * pb_elem_size(&desc->fields[i]));
    return total;
}

typedef struct pb_scratch {
    uint8_t* base;
    size_t size;
    size_t used;
} pb_scratch_t;

static void pb_store_numeric(const pb_field_t* f, void* p, uint64_t v) {
    switch (f->type) {
    case PB_UINT32:
        *(uint32_t*)p = (uint32_t)v;
        break;
    case PB_INT32:
        *(int32_t*)p = (int32_t)(uint32_t)v;
        break;
    case PB_UINT64:
        *(uint64_t*)p = v;
        break;
    case PB_INT64:
        *(int64_t*)p = (int64_t)v;
        break;
    default:
        *(uint8_t*)p = v ? 1 : 0;
        break;
    }
}

static bool pb_decode_into(const pb_msg_desc_t* desc, uint8_t* base, const uint8_t* data,
                           size_t size, pb_scratch_t* s);

/* One length-delimited value into @p: bytes borrowed, or a message decoded */
static bool pb_store_len(const pb_field_t* f, void* p, const pb_item_t* it, pb_scratch_t* s) {
    if (f->type == PB_MESSAGE)
        return pb_decode_into(f->sub, (uint8_t*)p, it->data, it->size, s);
    ((pb_bytes_t*)p)->data = it->data;
    ((pb_bytes_t*)p)->size = it->size;
    return true;
}

static bool pb_decode_into(const pb_msg_desc_t* desc, uint8_t* base, const uint8_t* data,
                           size_t size, pb_scratch_t* s) {
    size_t counts[PB_MAX_FIELDS];
    size_t i;
    pb_reader_t r;
    pb_item_t it;

    memset(base, 0, desc->size);
    if (desc->num_fields > PB_MAX_FIELDS)
        return false;

    /* First pass: how long each array is */
    memset(counts, 0, sizeof(counts));
    r.p = data;
    r.end = data + size;
    while (r.p < r.end) {
        const pb_field_t* f;
        int kind;
        if (!pb_next(&r, &it))
            return false;
        if (!(f = pb_find(desc, it.number)) || !f->repeated || !(kind = pb_kind(f, it.wire)))
            continue;
        if (kind == 2) {
            size_t n = pb_packed_count(it.data, it.size);
            if (n == PB_BAD)
                return false;
            counts[f - desc->fields] += n;
        } else {
            counts[f - desc->fields]++;
        }
    }
    for (i = 0; i < desc->num_fields; i++) {
        size_t bytes;
        if (!counts[i])
            continue;
        bytes = PB_ALIGN(counts[i] * pb_elem_size(&desc->fields[i]));
        if (bytes > s->size - s->used)
            return false;
        ((pb_array_t*)(base + desc->fields[i].offset))->items = s->base + s->used;
        memset(s->base + s->used, 0, bytes);
        s->used += bytes;
    }

    /* Second pass: the values */
    r.p = data;
    while (r.p < r.end) {
        const pb_field_t* f;
        void* p;
        int kind;
        pb_next(&r, &it);
        if (!(f = pb_find(desc, it.number)) || !(kind = pb_kind(f, it.wire)))
            continue;
        p = base + f->offset;
        if (f->repeated) {
            pb_array_t* a = (pb_array_t*)p;
            size_t es = pb_elem_size(f);
            if (kind == 2) {
                pb_reader_t pr;
                uint64_t v;
                pr.p = it.data;
                pr.end = it.data + it.size;
                while (pr.p < pr.end && pb_get_varint(&pr, &v))
                    pb_store_numeric(f, (uint8_t*)a->items + es * a->count++, v);
            } else if (kind == 1) {
                pb_store_numeric(f, (uint8_t*)a->items + es * a->count++, it.value);
            } else if (!pb_store_len(f, (uint8_t*)a->items + es * a->count++, &it, s)) {
                return false;
            }
        } else if (kind == 1) {
            pb_store_numeric(f, p, it.value);
        } else {
            if (f->type == PB_MESSAGE)
                base[f->has_offset] = 1;
            if (!pb_store_len(f, p, &it, s))
                return false;
        }
    }
    return true;
}

bool pb_decode(const pb_msg_desc_t* desc, void* msg, const uint8_t* data, size_t size,
               void* scratch, size_t scratch_size) {
    pb_scratch_t s;
    s.base = (uint8_t*)scratch;
    s.size = scratch_size;
    s.used = 0;
    return pb_decode_into(desc, (uint8_t*)msg, data, size, &s);
}
