/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef SHADPS4_COMMON_PATCH_RATE_H
#define SHADPS4_COMMON_PATCH_RATE_H

/* The core's own kind of patch line for a rate the player picks (a frame
 * rate): its value is a formula of N, the rate, and the line writes what the
 * formula gives for the rate picked, in the line's format.
 *
 * A formula is numbers, N, + - * / ^ (power) and parentheses: "1/N",
 * "990000/N", "0.65^(30/N)". A number is taken as the game's own float
 * constant would be, rounded to single precision ("0.65" is 0.64999998); the
 * arithmetic is in double precision. Formats:
 *   rate_f32: a 32-bit float, little-endian;
 *   rate_u32: a 32-bit unsigned integer, little-endian, rounded down;
 *   rate_dec: decimal text of Size digits, zeros before, rounded down. */

#include <stddef.h>

#include <retro_common_api.h>

RETRO_BEGIN_DECLS

/* The value of @formula for the rate @n into *@out. 0 when the formula does
 * not parse or gives no finite number. */
int patch_rate_eval(const char* formula, double n, double* out);

/* Whether a line of @type is a rate line */
int patch_rate_is_line(const char* type);

/* The bytes a rate line of @type writes for @value, as hex digits into @hex
 * (@len at least 2 * 20 + 1): @size is rate_dec's digit count (1 to 20).
 * 0 when the type is not a rate line's, the size is wrong or the value does
 * not fit the format. */
int patch_rate_hex(const char* type, unsigned size, double value, char* hex, size_t len);

/* Whether a line limited to rates from @min to @max (each "" or NULL when
 * not limited) applies at @n */
int patch_rate_in_range(const char* min, const char* max, unsigned n);

RETRO_END_DECLS

#endif
