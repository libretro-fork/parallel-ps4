/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <limits.h>
#include <string.h>

#include <compat/strl.h>
#include <formats/rjson.h>
#include <formats/rjson_helpers.h>
#include <formats/rjson_stream.h>
#include <streams/file_stream.h>

#include "common/json_util.h"

#define JSON_UTIL_TMP_SUFFIX ".tmp"
#define JSON_UTIL_PATH_MAX 4096

bool json_util_reader_open(json_util_reader_t* reader, const char* path) {
    reader->json = NULL;
    reader->file =
        filestream_open(path, RETRO_VFS_FILE_ACCESS_READ, RETRO_VFS_FILE_ACCESS_HINT_NONE);
    if (!reader->file)
        return false;
    reader->json = rjson_open_filestream(reader->file);
    if (!reader->json) {
        filestream_close(reader->file);
        reader->file = NULL;
        return false;
    }
    rjson_set_options(reader->json, RJSON_OPTION_ALLOW_UTF8BOM);
    return true;
}

void json_util_reader_close(json_util_reader_t* reader) {
    if (reader->json)
        rjson_free(reader->json);
    if (reader->file)
        filestream_close(reader->file);
    reader->json = NULL;
    reader->file = NULL;
}

static bool json_util_tmp_path(char* s, size_t len, const char* path) {
    size_t _len = strlcpy(s, path, len);
    if (_len >= len)
        return false;
    return strlcpy(s + _len, JSON_UTIL_TMP_SUFFIX, len - _len) < len - _len;
}

bool json_util_writer_open(json_util_writer_t* writer, const char* path) {
    char tmp[JSON_UTIL_PATH_MAX];
    writer->writer = NULL;
    writer->file = NULL;
    if (!json_util_tmp_path(tmp, sizeof(tmp), path))
        return false;
    writer->file =
        filestream_open(tmp, RETRO_VFS_FILE_ACCESS_WRITE, RETRO_VFS_FILE_ACCESS_HINT_NONE);
    if (!writer->file)
        return false;
    writer->writer = rjsonwriter_open_filestream(writer->file);
    if (!writer->writer) {
        filestream_close(writer->file);
        writer->file = NULL;
        filestream_delete(tmp);
        return false;
    }
    return true;
}

bool json_util_writer_close(json_util_writer_t* writer, const char* path, bool commit) {
    char tmp[JSON_UTIL_PATH_MAX];
    bool ok = true;
    if (writer->writer) {
        rjsonwriter_raw(writer->writer, "\n", 1);
        ok = rjsonwriter_free(writer->writer);
    }
    if (writer->file && filestream_close(writer->file) != 0)
        ok = false;
    writer->writer = NULL;
    writer->file = NULL;
    if (!json_util_tmp_path(tmp, sizeof(tmp), path))
        return false;
    if (!ok || !commit) {
        filestream_delete(tmp);
        return false;
    }
    return filestream_rename(tmp, path) == 0;
}

bool json_util_skip(rjson_t* json, enum rjson_type type) {
    unsigned depth;
    if (type == RJSON_ERROR)
        return false;
    if (type != RJSON_OBJECT && type != RJSON_ARRAY)
        return true;
    depth = rjson_get_context_depth(json);
    for (;;) {
        enum rjson_type next = rjson_next(json);
        if (next == RJSON_ERROR || next == RJSON_DONE)
            return false;
        if ((next == RJSON_OBJECT_END || next == RJSON_ARRAY_END) &&
            rjson_get_context_depth(json) < depth)
            return true;
    }
}

static void json_util_write_indent(rjsonwriter_t* writer, int indent) {
    rjsonwriter_add_newline(writer);
    rjsonwriter_add_spaces(writer, indent * 2);
}

bool json_util_copy_value(rjson_t* json, enum rjson_type type, rjsonwriter_t* writer, int indent) {
    size_t len;
    const char* str;

    switch (type) {
    case RJSON_STRING:
        str = rjson_get_string(json, &len);
        rjsonwriter_add_string_len(writer, str, (int)len);
        return true;
    case RJSON_NUMBER:
        str = rjson_get_string(json, &len);
        rjsonwriter_raw(writer, str, (int)len);
        return true;
    case RJSON_TRUE:
    case RJSON_FALSE:
        rjsonwriter_add_bool(writer, type == RJSON_TRUE);
        return true;
    case RJSON_NULL:
        rjsonwriter_add_null(writer);
        return true;
    case RJSON_OBJECT:
    case RJSON_ARRAY: {
        bool is_object = (type == RJSON_OBJECT);
        bool first = true;
        if (is_object)
            rjsonwriter_add_start_object(writer);
        else
            rjsonwriter_add_start_array(writer);
        for (;;) {
            enum rjson_type next = rjson_next(json);
            if (next == RJSON_OBJECT_END || next == RJSON_ARRAY_END)
                break;
            if (next == RJSON_ERROR || next == RJSON_DONE)
                return false;
            if (!first)
                rjsonwriter_add_comma(writer);
            json_util_write_indent(writer, indent + 1);
            if (is_object) {
                /* member name, then its value */
                str = rjson_get_string(json, &len);
                rjsonwriter_add_string_len(writer, str, (int)len);
                rjsonwriter_add_colon(writer);
                rjsonwriter_add_space(writer);
                next = rjson_next(json);
            }
            if (!json_util_copy_value(json, next, writer, indent + 1))
                return false;
            first = false;
        }
        json_util_write_end(writer, indent + 1, first, is_object ? '}' : ']');
        return true;
    }
    default:
        break;
    }
    return false;
}

size_t json_util_get_string(rjson_t* json, char* s, size_t len) {
    size_t _len = 0;
    const char* str = rjson_get_string(json, &_len);
    if (len) {
        size_t n = (_len < len) ? _len : len - 1;
        memcpy(s, str, n);
        s[n] = '\0';
    }
    return _len;
}

bool json_util_get_bool(enum rjson_type type, bool* out) {
    if (type != RJSON_TRUE && type != RJSON_FALSE)
        return false;
    *out = (type == RJSON_TRUE);
    return true;
}

/* Parses a JSON integer. Fractions and exponents are refused, like a
 * typed integer read would. */
static bool json_util_parse_integer(rjson_t* json, enum rjson_type type, bool* negative,
                                    uint64_t* magnitude) {
    size_t len, i = 0;
    uint64_t v = 0;
    const char* str;
    if (type != RJSON_NUMBER)
        return false;
    str = rjson_get_string(json, &len);
    *negative = false;
    if (i < len && str[i] == '-') {
        *negative = true;
        i++;
    }
    if (i == len)
        return false;
    for (; i < len; i++) {
        unsigned d;
        if (str[i] < '0' || str[i] > '9')
            return false;
        d = (unsigned)(str[i] - '0');
        if (v > (UINT64_MAX - d) / 10)
            return false;
        v = v * 10 + d;
    }
    *magnitude = v;
    return true;
}

bool json_util_get_int(rjson_t* json, enum rjson_type type, int* out) {
    bool negative;
    uint64_t v;
    if (!json_util_parse_integer(json, type, &negative, &v))
        return false;
    if (negative) {
        if (v > (uint64_t)INT_MAX + 1)
            return false;
        *out = (v == (uint64_t)INT_MAX + 1) ? INT_MIN : -(int)v;
    } else {
        if (v > (uint64_t)INT_MAX)
            return false;
        *out = (int)v;
    }
    return true;
}

bool json_util_get_uint(rjson_t* json, enum rjson_type type, unsigned* out) {
    bool negative;
    uint64_t v;
    if (!json_util_parse_integer(json, type, &negative, &v) || negative || v > (uint64_t)UINT_MAX)
        return false;
    *out = (unsigned)v;
    return true;
}

bool json_util_get_u64(rjson_t* json, enum rjson_type type, uint64_t* out) {
    bool negative;
    uint64_t v;
    if (!json_util_parse_integer(json, type, &negative, &v) || negative)
        return false;
    *out = v;
    return true;
}

bool json_util_get_double(rjson_t* json, enum rjson_type type, double* out) {
    if (type != RJSON_NUMBER)
        return false;
    *out = rjson_get_double(json);
    return true;
}

void json_util_write_key(rjsonwriter_t* writer, int indent, const char* key, bool first) {
    if (!first)
        rjsonwriter_add_comma(writer);
    json_util_write_indent(writer, indent);
    rjsonwriter_add_string(writer, key);
    rjsonwriter_add_colon(writer);
    rjsonwriter_add_space(writer);
}

void json_util_write_end(rjsonwriter_t* writer, int indent, bool empty, char bracket) {
    if (!empty)
        json_util_write_indent(writer, indent - 1);
    rjsonwriter_raw(writer, &bracket, 1);
}

void json_util_write_u64(rjsonwriter_t* writer, uint64_t value) {
    char buf[24];
    int i = (int)sizeof(buf);
    do {
        buf[--i] = (char)('0' + (int)(value % 10));
        value /= 10;
    } while (value);
    rjsonwriter_raw(writer, buf + i, (int)sizeof(buf) - i);
}
