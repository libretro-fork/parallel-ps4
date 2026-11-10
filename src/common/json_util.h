/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef SHADPS4_COMMON_JSON_UTIL_H
#define SHADPS4_COMMON_JSON_UTIL_H

/* Small C89 helpers shared by the code that reads and writes JSON with
 * libretro-common's streaming rjson parser and rjsonwriter. */

#include <stddef.h>
#include <stdint.h>

#include <boolean.h>
#include <formats/rjson.h>
#include <retro_common_api.h>
#include <streams/file_stream.h>

RETRO_BEGIN_DECLS

typedef struct json_util_reader {
    RFILE* file;
    rjson_t* json;
} json_util_reader_t;

typedef struct json_util_writer {
    RFILE* file;
    rjsonwriter_t* writer;
} json_util_writer_t;

/* Opens path for streaming reads. Returns false when the file can't be
 * opened. */
bool json_util_reader_open(json_util_reader_t* reader, const char* path);
void json_util_reader_close(json_util_reader_t* reader);

/* Opens a writer on "<path>.tmp". json_util_writer_close flushes it and,
 * when commit is true and everything was written, renames it over path.
 * Returns true only when path was replaced. */
bool json_util_writer_open(json_util_writer_t* writer, const char* path);
bool json_util_writer_close(json_util_writer_t* writer, const char* path, bool commit);

/* type is the value rjson_next just returned. For an object or array the
 * rest of it is consumed. Returns false on a parse error. */
bool json_util_skip(rjson_t* json, enum rjson_type type);

/* Copies the value rjson_next just returned (type) to writer, nested
 * objects and arrays included. indent is the nesting level of the value. */
bool json_util_copy_value(rjson_t* json, enum rjson_type type, rjsonwriter_t* writer, int indent);

/* Copies the current string or number into s (always terminated when len
 * is not 0). Returns the full source length. */
size_t json_util_get_string(rjson_t* json, char* s, size_t len);

/* Typed reads of the value rjson_next just returned. Each returns false
 * and leaves *out alone when the value has the wrong type or range. */
bool json_util_get_bool(enum rjson_type type, bool* out);
bool json_util_get_int(rjson_t* json, enum rjson_type type, int* out);
bool json_util_get_uint(rjson_t* json, enum rjson_type type, unsigned* out);
bool json_util_get_u64(rjson_t* json, enum rjson_type type, uint64_t* out);
bool json_util_get_double(rjson_t* json, enum rjson_type type, double* out);

/* Writer helpers producing two-space indented output. A member is a comma
 * (unless first), a newline, indentation, the quoted key, a colon and a
 * space. json_util_write_end closes the object or array opened at
 * indent - 1, with bracket being '}' or ']'. */
void json_util_write_key(rjsonwriter_t* writer, int indent, const char* key, bool first);
void json_util_write_end(rjsonwriter_t* writer, int indent, bool empty, char bracket);
void json_util_write_u64(rjsonwriter_t* writer, uint64_t value);

RETRO_END_DECLS

#endif
