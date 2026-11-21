/* SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef SHADPS4_LIBC_INTERNAL_STR_H
#define SHADPS4_LIBC_INTERNAL_STR_H

/* libSceLibcInternal's string functions: the host's. Defined in libc_internal_str.c,
 * registered in libc_internal.cpp. */

#include <stddef.h>

#include <retro_common_api.h>

#include "common/types.h"

RETRO_BEGIN_DECLS

s32 PS4_SYSV_ABI internal_strcpy_s(char* dest, size_t dest_size, const char* src);
s32 PS4_SYSV_ABI internal_strcat_s(char* dest, size_t dest_size, const char* src);
s32 PS4_SYSV_ABI internal_strcmp(const char* str1, const char* str2);
s32 PS4_SYSV_ABI internal_strncmp(const char* str1, const char* str2, size_t num);
size_t PS4_SYSV_ABI internal_strlen(const char* str);
char* PS4_SYSV_ABI internal_strncpy(char* dest, const char* src, size_t count);
s32 PS4_SYSV_ABI internal_strncpy_s(char* dest, size_t destsz, const char* src, size_t count);
char* PS4_SYSV_ABI internal_strcat(char* dest, const char* src);
const char* PS4_SYSV_ABI internal_strchr(const char* str, int c);

RETRO_END_DECLS

#endif
