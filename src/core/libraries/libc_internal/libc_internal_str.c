/* SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <string.h>

#include "core/libraries/libc_internal/libc_internal_str.h"

s32 PS4_SYSV_ABI internal_strcpy_s(char* dest, size_t dest_size, const char* src) {
#ifdef _WIN64
    return strcpy_s(dest, dest_size, src);
#else
    (void)dest_size;
    strcpy(dest, src);
    return 0; /* ALL OK */
#endif
}

s32 PS4_SYSV_ABI internal_strcat_s(char* dest, size_t dest_size, const char* src) {
#ifdef _WIN64
    return strcat_s(dest, dest_size, src);
#else
    (void)dest_size;
    strcat(dest, src);
    return 0; /* ALL OK */
#endif
}

s32 PS4_SYSV_ABI internal_strcmp(const char* str1, const char* str2) {
    return strcmp(str1, str2);
}

s32 PS4_SYSV_ABI internal_strncmp(const char* str1, const char* str2, size_t num) {
    return strncmp(str1, str2, num);
}

size_t PS4_SYSV_ABI internal_strlen(const char* str) {
    return strlen(str);
}

char* PS4_SYSV_ABI internal_strncpy(char* dest, const char* src, size_t count) {
    return strncpy(dest, src, count);
}

s32 PS4_SYSV_ABI internal_strncpy_s(char* dest, size_t destsz, const char* src, size_t count) {
#ifdef _WIN64
    return strncpy_s(dest, destsz, src, count);
#else
    /* As the C++ did: the whole of @src, whatever @count says */
    (void)destsz;
    (void)count;
    strcpy(dest, src);
    return 0;
#endif
}

char* PS4_SYSV_ABI internal_strcat(char* dest, const char* src) {
    return strcat(dest, src);
}

const char* PS4_SYSV_ABI internal_strchr(const char* str, int c) {
    return strchr(str, c);
}
