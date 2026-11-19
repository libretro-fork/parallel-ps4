/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <string.h>

#include "common/line_buffer.h"

void line_buffer_feed(line_buffer_t* lb, const char* buf, size_t n, line_buffer_emit_t emit,
                      void* ctx) {
    size_t i;
    for (i = 0; i < n; i++) {
        const char c = buf[i];
        if (c == '\r') {
            continue;
        }
        if (c == '\n') {
            line_buffer_flush(lb, emit, ctx);
            continue;
        }
        if (lb->len == LINE_BUFFER_SIZE) {
            emit(ctx, lb->text, lb->len);
            lb->len = 0;
        }
        lb->text[lb->len++] = c;
    }
}

void line_buffer_flush(line_buffer_t* lb, line_buffer_emit_t emit, void* ctx) {
    if (lb->len != 0) {
        emit(ctx, lb->text, lb->len);
        lb->len = 0;
    }
}
