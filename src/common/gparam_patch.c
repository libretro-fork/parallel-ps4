/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <encodings/deflate.h>
#include <retro_atomic.h>

#include "common/gparam_patch.h"

/* The bytes of a value of each GPARAM type (0: a type with no value the
 * rules change, or none) and how it is read: 1 to 8 whole numbers (signed 8,
 * 16, 32, 64 bits, then unsigned), 9 float, 10 double, 11 bool, 12 to 14
 * vectors of 2 to 4 floats, 15 a colour of 4 bytes. */
static const unsigned char gparam_type_size[16] = {0, 1, 2, 4, 8, 1,  2,  4,
                                                   8, 4, 8, 1, 8, 12, 16, 4};

static uint32_t gparam_le32(const unsigned char* p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static uint64_t gparam_le64(const unsigned char* p) {
    return (uint64_t)gparam_le32(p) | ((uint64_t)gparam_le32(p + 4) << 32);
}

static uint32_t gparam_be32(const unsigned char* p) {
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | (uint32_t)p[3];
}

static void gparam_put_le32(unsigned char* p, uint32_t v) {
    p[0] = (unsigned char)v;
    p[1] = (unsigned char)(v >> 8);
    p[2] = (unsigned char)(v >> 16);
    p[3] = (unsigned char)(v >> 24);
}

static void gparam_put_be32(unsigned char* p, uint32_t v) {
    p[0] = (unsigned char)(v >> 24);
    p[1] = (unsigned char)(v >> 16);
    p[2] = (unsigned char)(v >> 8);
    p[3] = (unsigned char)v;
}

static int gparam_lower(int c) {
    return c >= 'A' && c <= 'Z' ? c - 'A' + 'a' : c;
}

static int gparam_is_space(unsigned c) {
    return c == ' ' || c == '\t' || c == 0x3000;
}

/* Whether the UTF-16LE name at @off of @data (to its terminator, within
 * @len) is @name: ASCII case aside, spaces at either end aside */
static int gparam_name_is(const unsigned char* data, size_t len, size_t off, const char* name) {
    size_t end = off;
    size_t n;
    while (end + 1 < len && (data[end] | data[end + 1]) != 0)
        end += 2;
    if (end + 1 >= len)
        return 0;
    while (off < end && gparam_is_space((unsigned)data[off] | ((unsigned)data[off + 1] << 8)))
        off += 2;
    while (end > off && gparam_is_space((unsigned)data[end - 2] | ((unsigned)data[end - 1] << 8)))
        end -= 2;
    while (*name == ' ' || *name == '\t')
        name++;
    n = strlen(name);
    while (n > 0 && (name[n - 1] == ' ' || name[n - 1] == '\t'))
        n--;
    if ((end - off) / 2 != n)
        return 0;
    for (; off < end; off += 2, name++) {
        if (data[off + 1] != 0 || gparam_lower(data[off]) != gparam_lower((unsigned char)*name))
            return 0;
    }
    return 1;
}

/* Where the UTF-16LE string at @off ends, past its terminator; 0 if it does
 * not end within @len */
static size_t gparam_skip_name(const unsigned char* data, size_t len, size_t off) {
    while (off + 1 < len) {
        if ((data[off] | data[off + 1]) == 0)
            return off + 2;
        off += 2;
    }
    return 0;
}

static double gparam_op(int op, double cur, double v) {
    switch (op) {
    case GPARAM_OP_MAX:
        return cur < v ? v : cur;
    case GPARAM_OP_MIN:
        return cur > v ? v : cur;
    case GPARAM_OP_MUL:
        return cur * v;
    default:
        return v;
    }
}

/* One value of @type at @p under @rule; 1 when it changed */
static int gparam_edit_value(unsigned char* p, unsigned type, const gparam_rule_t* rule) {
    if (type >= 1 && type <= 8) {
        static const unsigned char bits[9] = {0, 8, 16, 32, 64, 8, 16, 32, 64};
        const unsigned size = bits[type] / 8;
        const int is_signed = type <= 4;
        uint64_t raw = 0;
        double cur, next, lo, hi;
        int64_t out;
        unsigned i;
        for (i = 0; i < size; i++)
            raw |= (uint64_t)p[i] << (8 * i);
        if (is_signed && size < 8 && (raw >> (bits[type] - 1)) & 1)
            raw |= ~(uint64_t)0 << bits[type];
        cur = is_signed ? (double)(int64_t)raw : (double)raw;
        next = gparam_op(rule->op, cur, rule->value);
        next = next < 0 ? -floor(-next + 0.5) : floor(next + 0.5);
        /* The type's range; 64 bits are kept to what a double holds exactly */
        if (size == 8) {
            hi = 9007199254740992.0;
            lo = is_signed ? -hi : 0.0;
        } else if (is_signed) {
            hi = ldexp(1.0, bits[type] - 1) - 1.0;
            lo = -hi - 1.0;
        } else {
            hi = ldexp(1.0, bits[type]) - 1.0;
            lo = 0.0;
        }
        if (next < lo)
            next = lo;
        if (next > hi)
            next = hi;
        if (next == cur)
            return 0;
        out = (int64_t)next;
        for (i = 0; i < size; i++)
            p[i] = (unsigned char)((uint64_t)out >> (8 * i));
        return 1;
    }
    if (type == 9) {
        uint32_t u = gparam_le32(p);
        float f;
        float g;
        memcpy(&f, &u, 4);
        if (f != f)
            return 0;
        g = (float)gparam_op(rule->op, (double)f, rule->value);
        if (g == f)
            return 0;
        memcpy(&u, &g, 4);
        gparam_put_le32(p, u);
        return 1;
    }
    if (type == 10) {
        uint64_t u = gparam_le64(p);
        double d;
        double e;
        unsigned i;
        memcpy(&d, &u, 8);
        if (d != d)
            return 0;
        e = gparam_op(rule->op, d, rule->value);
        if (e == d)
            return 0;
        memcpy(&u, &e, 8);
        for (i = 0; i < 8; i++)
            p[i] = (unsigned char)(u >> (8 * i));
        return 1;
    }
    if (type == 11) {
        const unsigned char b = gparam_op(rule->op, p[0] ? 1.0 : 0.0, rule->value) != 0.0;
        if (b == (p[0] != 0))
            return 0;
        p[0] = b;
        return 1;
    }
    /* Vectors and colours: not changed by these rules */
    return 0;
}

/* The rules applied to one GPARAM file, @data of @len bytes. Values changed,
 * or -1 when it is not one */
static int gparam_apply_one(unsigned char* data, size_t len, const gparam_rule_t* rules,
                            unsigned count) {
    uint32_t param_count, param_offsets, params, field_offsets, fields, values;
    uint32_t i;
    int changed = 0;
    static const unsigned char magic[8] = {'f', 0, 'i', 0, 'l', 0, 't', 0};
    if (len < 0x30 || memcmp(data, magic, 8) != 0 || gparam_le32(data + 8) != 3)
        return -1;
    param_count = gparam_le32(data + 0x10);
    param_offsets = gparam_le32(data + 0x18);
    params = gparam_le32(data + 0x1c);
    field_offsets = gparam_le32(data + 0x20);
    fields = gparam_le32(data + 0x24);
    values = gparam_le32(data + 0x28);
    if (param_offsets > len || param_count > (len - param_offsets) / 4)
        return -1;
    for (i = 0; i < param_count; i++) {
        size_t p = (size_t)params + gparam_le32(data + param_offsets + 4 * i);
        uint32_t field_count, field_list, j;
        size_t name;
        unsigned r;
        int wanted = 0;
        if (p + 8 > len)
            return -1;
        field_count = gparam_le32(data + p);
        field_list = gparam_le32(data + p + 4);
        /* The key, then the name */
        name = gparam_skip_name(data, len, p + 8);
        if (name == 0)
            return -1;
        for (r = 0; r < count && !wanted; r++)
            wanted = gparam_name_is(data, len, name, rules[r].param);
        if (!wanted)
            continue;
        if ((size_t)field_offsets + field_list > len ||
            field_count > (len - field_offsets - field_list) / 4)
            return -1;
        for (j = 0; j < field_count; j++) {
            size_t f = (size_t)fields + gparam_le32(data + field_offsets + field_list + 4 * j);
            size_t at, fname;
            unsigned type, capacity, size, k;
            if (f + 12 > len)
                return -1;
            at = (size_t)values + gparam_le32(data + f);
            type = data[f + 8];
            capacity = data[f + 9];
            size = type < 16 ? gparam_type_size[type] : 0;
            fname = gparam_skip_name(data, len, f + 12);
            if (fname == 0 || size == 0)
                continue;
            if (at > len || (size_t)capacity * size > len - at)
                return -1;
            for (r = 0; r < count; r++) {
                if (!gparam_name_is(data, len, name, rules[r].param) ||
                    !gparam_name_is(data, len, fname, rules[r].field))
                    continue;
                for (k = 0; k < capacity; k++)
                    changed += gparam_edit_value(data + at + (size_t)k * size, type, &rules[r]);
            }
        }
    }
    return changed;
}

/* A BND4's format byte as SoulsFormats reads it: bit-reversed unless the
 * binder says its bits are big-endian or the byte says so itself */
static unsigned gparam_bnd_format(unsigned raw, int bit_big_endian) {
    unsigned out = 0;
    unsigned b;
    if (bit_big_endian || ((raw & 1) && !(raw & 0x80)))
        return raw;
    for (b = 0; b < 8; b++)
        if (raw & (1u << b))
            out |= 0x80u >> b;
    return out;
}

static int gparam_apply_bnd4(unsigned char* data, size_t len, const gparam_rule_t* rules,
                             unsigned count) {
    uint32_t file_count, i;
    uint64_t entry_size;
    unsigned format;
    int long_offsets, compression;
    int changed = 0;
    if (len < 0x40 || data[9] != 0 || gparam_le64(data + 0x10) != 0x40)
        return -1;
    file_count = gparam_le32(data + 0x0c);
    entry_size = gparam_le64(data + 0x20);
    format = gparam_bnd_format(data[0x31], data[0x0a] == 0);
    long_offsets = (format & 0x10) != 0;
    compression = (format & 0x20) != 0;
    if (entry_size < (uint64_t)(0x10 + (long_offsets ? 8 : 4) + (compression ? 8 : 0)) ||
        entry_size > 0x40 || file_count > (len - 0x40) / entry_size)
        return -1;
    for (i = 0; i < file_count; i++) {
        const unsigned char* e = data + 0x40 + (size_t)i * (size_t)entry_size;
        const uint64_t stored = gparam_le64(e + 8);
        const size_t at = compression ? 0x18 : 0x10;
        const uint64_t offset = long_offsets ? gparam_le64(e + at) : gparam_le32(e + at);
        const uint64_t size = compression ? gparam_le64(e + 0x10) : stored;
        int n;
        /* A compressed file (its size stored differs) is left as it is */
        if (size != stored || offset > len || size > len - offset)
            continue;
        n = gparam_apply_one(data + offset, (size_t)size, rules, count);
        if (n > 0)
            changed += n;
    }
    return changed;
}

int gparam_apply(unsigned char* data, size_t len, const gparam_rule_t* rules, unsigned count) {
    if (!data || !rules || count == 0)
        return -1;
    if (len >= 4 && memcmp(data, "BND4", 4) == 0)
        return gparam_apply_bnd4(data, len, rules, count);
    return gparam_apply_one(data, len, rules, count);
}

int gparam_rule_parse(gparam_rule_t* rule, const char* param, const char* field, const char* op,
                      const char* value) {
    char* end = NULL;
    size_t pl, fl;
    if (!rule || !param || !field || !value)
        return 0;
    pl = strlen(param);
    fl = strlen(field);
    if (pl == 0 || fl == 0 || pl >= GPARAM_NAME_MAX || fl >= GPARAM_NAME_MAX)
        return 0;
    if (!op || !op[0] || strcmp(op, "set") == 0)
        rule->op = GPARAM_OP_SET;
    else if (strcmp(op, "max") == 0)
        rule->op = GPARAM_OP_MAX;
    else if (strcmp(op, "min") == 0)
        rule->op = GPARAM_OP_MIN;
    else if (strcmp(op, "mul") == 0)
        rule->op = GPARAM_OP_MUL;
    else
        return 0;
    rule->value = strtod(value, &end);
    if (end == value || *end != '\0' || rule->value != rule->value)
        return 0;
    memcpy(rule->param, param, pl + 1);
    memcpy(rule->field, field, fl + 1);
    return 1;
}

int gparam_patch_dcx(const unsigned char* src, size_t len, const gparam_rule_t* rules,
                     unsigned count, unsigned char* out, size_t out_cap, size_t* out_len) {
    uint32_t head, plain_size, packed_size;
    unsigned char* plain;
    void* stream;
    size_t got = 0, wrote = 0;
    int status, changed, result = -1;
    if (out_len)
        *out_len = 0;
    if (!src || len < 0x4c || memcmp(src, "DCX\0", 4) != 0 || memcmp(src + 0x18, "DCS\0", 4) != 0 ||
        memcmp(src + 0x24, "DCP\0DFLT", 8) != 0)
        return -1;
    /* The header's last chunk, DCA, says where the compressed bytes start */
    if (memcmp(src + 0x44, "DCA\0", 4) != 0)
        return -1;
    head = 0x44 + gparam_be32(src + 0x48);
    plain_size = gparam_be32(src + 0x1c);
    packed_size = gparam_be32(src + 0x20);
    if (head < 0x4c || head > len || packed_size > len - head || plain_size == 0 ||
        plain_size > 0x4000000)
        return -1;
    plain = (unsigned char*)malloc(plain_size);
    if (!plain)
        return -1;
    stream = rinflate_new(15);
    if (!stream) {
        free(plain);
        return -1;
    }
    rinflate_set_in(stream, src + head, packed_size);
    rinflate_set_out(stream, plain, plain_size);
    do {
        size_t r = 0, w = 0;
        status = rinflate_process(stream, &r, &w);
        got += w;
        if (r == 0 && w == 0 && status == RDEFLATE_PROCESS_NEXT)
            break;
    } while (status == RDEFLATE_PROCESS_NEXT);
    rinflate_free(stream);
    if (status != RDEFLATE_PROCESS_END || got != plain_size) {
        free(plain);
        return -1;
    }
    changed = gparam_apply(plain, plain_size, rules, count);
    if (changed <= 0) {
        free(plain);
        return changed < 0 ? -1 : 0;
    }
    if (out && out_cap > head && (stream = rdeflate_new(9, 15)) != NULL) {
        rdeflate_set_in(stream, plain, plain_size);
        rdeflate_set_out(stream, out + head, out_cap - head);
        rdeflate_finish(stream);
        do {
            size_t r = 0, w = 0;
            status = rdeflate_process(stream, &r, &w);
            wrote += w;
            if (r == 0 && w == 0 && status == RDEFLATE_PROCESS_NEXT)
                break;
        } while (status == RDEFLATE_PROCESS_NEXT);
        rdeflate_free(stream);
        if (status == RDEFLATE_PROCESS_END && wrote <= 0xffffffffu) {
            memcpy(out, src, head);
            gparam_put_be32(out + 0x20, (uint32_t)wrote);
            if (out_len)
                *out_len = head + wrote;
            result = changed;
        }
    }
    free(plain);
    return result;
}

typedef struct gparam_stored {
    char path[GPARAM_PATH_MAX];
    unsigned char* data;
    size_t len;
} gparam_stored_t;

static gparam_stored_t gparam_store[GPARAM_STORE_MAX];
/* How many of gparam_store are filled: published after the entry is */
static retro_atomic_int_t gparam_store_count;

static int gparam_path_is(const char* a, const char* b) {
    for (; *a && *b; a++, b++) {
        if (gparam_lower((unsigned char)*a) != gparam_lower((unsigned char)*b))
            return 0;
    }
    return *a == *b;
}

int gparam_store_add(const char* path, unsigned char* data, size_t len) {
    const int n = retro_atomic_load_acquire_int(&gparam_store_count);
    const size_t pl = path ? strlen(path) : GPARAM_PATH_MAX;
    if (n >= GPARAM_STORE_MAX || pl >= GPARAM_PATH_MAX) {
        free(data);
        return 0;
    }
    memcpy(gparam_store[n].path, path, pl + 1);
    gparam_store[n].data = data;
    gparam_store[n].len = len;
    retro_atomic_store_release_int(&gparam_store_count, n + 1);
    return 1;
}

const unsigned char* gparam_store_find(const char* path, size_t* len) {
    const int n = retro_atomic_load_acquire_int(&gparam_store_count);
    int i;
    if (n == 0 || !path)
        return NULL;
    for (i = 0; i < n; i++) {
        if (gparam_path_is(gparam_store[i].path, path)) {
            if (len)
                *len = gparam_store[i].len;
            return gparam_store[i].data;
        }
    }
    return NULL;
}

void gparam_store_clear(void) {
    const int n = retro_atomic_load_acquire_int(&gparam_store_count);
    int i;
    retro_atomic_store_release_int(&gparam_store_count, 0);
    for (i = 0; i < n; i++) {
        free(gparam_store[i].data);
        gparam_store[i].data = NULL;
    }
}
