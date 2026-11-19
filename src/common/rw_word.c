/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "common/rw_word.h"

void rw_word_init(retro_atomic_int_t* word) {
    retro_atomic_int_init(word, 0);
}

int rw_word_try_read(retro_atomic_int_t* word) {
    for (;;) {
        const int held = retro_atomic_load_acquire_int(word);
        if (held < 0) {
            return 0;
        }
        if (retro_atomic_cas_int(word, held, held + 1)) {
            return 1;
        }
    }
}

int rw_word_try_write(retro_atomic_int_t* word) {
    return retro_atomic_cas_int(word, 0, -1);
}

int rw_word_unlock_read(retro_atomic_int_t* word) {
    for (;;) {
        const int held = retro_atomic_load_acquire_int(word);
        if (held <= 0) {
            return -1;
        }
        if (retro_atomic_cas_int(word, held, held - 1)) {
            return held == 1;
        }
    }
}

int rw_word_unlock_write(retro_atomic_int_t* word) {
    return retro_atomic_cas_int(word, -1, 0) ? 1 : -1;
}
