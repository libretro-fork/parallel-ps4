/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef SHADPS4_COMMON_PATCH_HALVE_H
#define SHADPS4_COMMON_PATCH_HALVE_H

/* A patch line that halves a number a game carries as decimal text of a
 * fixed width (a "Key= 33333" line of a setup text in its executable): the
 * number is read where it is, halved, and written back over the same bytes,
 * spaces before it where it has fewer digits (" 8333": a number however the
 * game reads it). What another patch wrote there before is what is halved. */

#include <stddef.h>

#include <retro_common_api.h>

RETRO_BEGIN_DECLS

/* Halves the decimal number in @field (@width bytes: spaces, then digits to
 * the end). 1 and the old and new numbers, or 0 with nothing written when the
 * bytes are not such a number, it is 0 or 1, or @width is 0 or over 18. */
int patch_halve_decimal(unsigned char* field, size_t width, unsigned long* before,
                        unsigned long* after);

RETRO_END_DECLS

#endif
