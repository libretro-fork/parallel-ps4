/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "common/slot_table.h"

int slot_table_take(retro_atomic_int_t* slots, int first, int end) {
    int i;
    for (i = first; i < end; i++) {
        if (retro_atomic_load_acquire_int(&slots[i]) == 0 &&
            retro_atomic_cas_int(&slots[i], 0, 1)) {
            return i;
        }
    }
    return -1;
}

int slot_table_taken(retro_atomic_int_t* slots, int index) {
    return retro_atomic_load_acquire_int(&slots[index]) != 0;
}

void slot_table_give(retro_atomic_int_t* slots, int index) {
    retro_atomic_store_release_int(&slots[index], 0);
}
