/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <string.h>

#include "common/word_name.h"

void word_name_init(word_name_t* name) {
    int i;
    for (i = 0; i < WORD_NAME_WORDS; i++) {
        retro_atomic_int_init(&name->words[i], 0);
    }
}

void word_name_set(word_name_t* name, const char* text) {
    unsigned char bytes[WORD_NAME_WORDS * 4];
    size_t len = text ? strlen(text) : 0;
    int i;
    if (len > WORD_NAME_MAX) {
        len = WORD_NAME_MAX;
    }
    memset(bytes, 0, sizeof(bytes));
    if (len) {
        memcpy(bytes, text, len);
    }
    for (i = 0; i < WORD_NAME_WORDS; i++) {
        const unsigned word = (unsigned)bytes[i * 4] | ((unsigned)bytes[i * 4 + 1] << 8) |
                              ((unsigned)bytes[i * 4 + 2] << 16) |
                              ((unsigned)bytes[i * 4 + 3] << 24);
        retro_atomic_store_release_int(&name->words[i], (int)word);
    }
}

size_t word_name_get(const word_name_t* name, char* out) {
    char bytes[WORD_NAME_WORDS * 4];
    int i;
    for (i = 0; i < WORD_NAME_WORDS; i++) {
        const unsigned word =
            (unsigned)retro_atomic_load_acquire_int((retro_atomic_int_t*)&name->words[i]);
        bytes[i * 4] = (char)(word & 0xff);
        bytes[i * 4 + 1] = (char)((word >> 8) & 0xff);
        bytes[i * 4 + 2] = (char)((word >> 16) & 0xff);
        bytes[i * 4 + 3] = (char)(word >> 24);
    }
    bytes[WORD_NAME_MAX] = '\0';
    memcpy(out, bytes, WORD_NAME_MAX + 1);
    return strlen(out);
}
