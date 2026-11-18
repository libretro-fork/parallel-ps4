/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef SHADPS4_COMMON_HEX_DUMP_H
#define SHADPS4_COMMON_HEX_DUMP_H

/* Memory written out for a crash report: which bytes to show around an
 * address a register holds, and a line of them as hex and as text. Nothing
 * here reads memory: the caller reads it, in whatever way cannot fault. */

#include <stddef.h>
#include <stdint.h>

#include <retro_common_api.h>

RETRO_BEGIN_DECLS

#define HEX_DUMP_LINE_BYTES 16
/* Room for a line of HEX_DUMP_LINE_BYTES bytes and its end */
#define HEX_DUMP_LINE_CHARS 72

/* The lines to show around @address: from @before bytes before it to @after
 * bytes after, widened to whole lines. Never starts below 0 nor runs past
 * the end of the address space. */
void hex_dump_window(uint64_t address, uint64_t before, uint64_t after, uint64_t* start,
                     uint64_t* length);

/* Writes @count bytes (at most HEX_DUMP_LINE_BYTES) into @out as
 * "26 6c 11 f0 42 45 15 dc  00 01 ...  |&l..BE......|": hex in two groups of
 * eight, the columns of a short line left blank, then the bytes as text, a
 * dot for what does not print. Always ends @out; returns the characters
 * written, 0 when @size is too small for the line. */
size_t hex_dump_line(char* out, size_t size, const unsigned char* bytes, size_t count);

/* Whether @count bytes (a line's worth is what it is meant for) look like
 * packed data, a compressed or encoded stream: no more than one zero among
 * them and no value more than three times. Pointers, counters, text padded
 * with zeroes and cleared memory do not; it is how a crash report finds
 * where a run of data written over the game's objects starts and ends. */
int hex_dump_packed(const unsigned char* bytes, size_t count);

RETRO_END_DECLS

#endif
