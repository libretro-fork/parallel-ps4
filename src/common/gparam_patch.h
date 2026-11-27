/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef SHADPS4_COMMON_GPARAM_PATCH_H
#define SHADPS4_COMMON_GPARAM_PATCH_H

/* Patch lines that change the values of FromSoftware's graphics param files
 * (GPARAM, version 3: Bloodborne's drawparam .gparam.dcx and .gparambnd.dcx)
 * as the game reads them: a field of a param, both by the names the files
 * give them (ShadowParam, Shadow Map Resolution), set to a value, raised to
 * it, lowered to it or multiplied by it, in every file that has it. The game
 * gets its own file with those values changed, the rest as it was.
 *
 * A GPARAM file has params; each has fields; each field has one or more
 * values (one per variant of the param in the file), all of one type. The
 * edit is made where the values are, so nothing of the file moves: the
 * file is only compressed again. */

#include <stddef.h>

#include <retro_common_api.h>

RETRO_BEGIN_DECLS

enum gparam_op {
    GPARAM_OP_SET = 0, /* the value */
    GPARAM_OP_MAX,     /* at least the value: raised to it where lower */
    GPARAM_OP_MIN,     /* at most the value: lowered to it where higher */
    GPARAM_OP_MUL      /* multiplied by the value (rounded for whole numbers) */
};

#define GPARAM_NAME_MAX 64
#define GPARAM_RULES_MAX 64

typedef struct gparam_rule {
    char param[GPARAM_NAME_MAX]; /* the param's name; case and spaces around it aside */
    char field[GPARAM_NAME_MAX]; /* the field's name, the same way */
    int op;                      /* enum gparam_op */
    double value;
} gparam_rule_t;

/* A rule from a patch line's text: @op is "set", "max", "min" or "mul" ("set"
 * where empty), @value a decimal number. 0 when a name is empty or too long,
 * the op unknown or the value not a number. */
int gparam_rule_parse(gparam_rule_t* rule, const char* param, const char* field, const char* op,
                      const char* value);

/* Applies @rules to a GPARAM file, or to each GPARAM file stored (not
 * compressed) in a BND4, in place in @data (@len bytes). The number of values
 * changed; -1 when @data is neither or is malformed (nothing changed then). */
int gparam_apply(unsigned char* data, size_t len, const gparam_rule_t* rules, unsigned count);

/* A DCX file (DFLT: zlib) of a GPARAM or a BND4 of them, @src of @len bytes,
 * with @rules applied: written to @out, whose room is @out_cap, its size in
 * *@out_len. The number of values changed (0: nothing to change, nothing
 * written); -1 when @src is not such a file or the result is over
 * @out_cap. Memory for the decompressed file and the codec is taken and given
 * back in the call. */
int gparam_patch_dcx(const unsigned char* src, size_t len, const gparam_rule_t* rules,
                     unsigned count, unsigned char* out, size_t out_cap, size_t* out_len);

/* The game's param files with the rules applied, by the guest path the game
 * opens them by, made once before the game runs and only read after: the
 * file layer serves a file found here in place of the game's own. */
#define GPARAM_STORE_MAX 512
#define GPARAM_PATH_MAX 192

/* Keeps @data (malloc'd, @len bytes; the store frees it) for @path. 0 when
 * the store is full or the path too long (nothing kept: @data is freed). */
int gparam_store_add(const char* path, unsigned char* data, size_t len);

/* The changed file for @path, ASCII case aside, and its size; NULL when
 * there is none. From any thread. */
const unsigned char* gparam_store_find(const char* path, size_t* len);

/* Empties the store; only while no game is running. */
void gparam_store_clear(void);

RETRO_END_DECLS

#endif
