/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

/* Text written in pieces, cut into lines: what the game writes to its stdout and stderr. A
 * buffer of fixed size kept by each thread that writes, so threads never share one and never
 * wait for each other; a line longer than the buffer goes out in buffer-sized pieces. '\r' is
 * dropped, and so are empty lines. Nothing is allocated. */

#ifndef SHADPS4_LINE_BUFFER_H
#define SHADPS4_LINE_BUFFER_H

#include <stddef.h>

#include <retro_common_api.h>

RETRO_BEGIN_DECLS

#define LINE_BUFFER_SIZE 1024

typedef struct line_buffer {
    char text[LINE_BUFFER_SIZE];
    size_t len;
} line_buffer_t;

/* Takes a line out: @text is not terminated, @len is never 0 */
typedef void (*line_buffer_emit_t)(void* ctx, const char* text, size_t len);

/* @n bytes of @buf: every line they end goes to @emit, the rest is kept for the next. */
void line_buffer_feed(line_buffer_t* lb, const char* buf, size_t n, line_buffer_emit_t emit,
                      void* ctx);

/* What is kept goes to @emit as a line, if anything is. */
void line_buffer_flush(line_buffer_t* lb, line_buffer_emit_t emit, void* ctx);

RETRO_END_DECLS

#endif
