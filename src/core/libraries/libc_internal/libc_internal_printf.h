/* SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef SHADPS4_LIBC_INTERNAL_PRINTF_H
#define SHADPS4_LIBC_INTERNAL_PRINTF_H

/* The game's printf formatting, of its own variable arguments (common/va_ctx.h): Marco
 * Paland's tiny printf, as Vita3K adapted it. Defined in libc_internal_printf.c. */

#include <stddef.h>

#include <retro_common_api.h>

#include "common/va_ctx.h"

RETRO_BEGIN_DECLS

/* snprintf: the format is the context's next argument. At most @n - 1 characters and a
 * terminator are written to @s (nothing if @n is 0, or @s is NULL); the length the whole would
 * have, without its terminator, is returned. */
int libc_snprintf_ctx(char* s, size_t n, va_ctx_t* ctx);

RETRO_END_DECLS

#endif
