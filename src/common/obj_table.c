/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <stdlib.h>

#include "common/obj_table.h"

#define OBJ_LIVE 1
#define OBJ_DOOMED 2
#define OBJ_USER 4
/* Taken by an add, not yet live: no hold gets it */
#define OBJ_FILLING OBJ_DOOMED

int obj_table_init(obj_table_t* table, int count) {
    int i;
    table->slots = (obj_slot_t*)calloc((size_t)count, sizeof(*table->slots));
    table->count = table->slots ? count : 0;
    if (!table->slots) {
        return 0;
    }
    for (i = 0; i < count; i++) {
        retro_atomic_int_init(&table->slots[i].word, 0);
        retro_atomic_ptr_init(&table->slots[i].object, NULL);
    }
    retro_eventcount_init(&table->users_left);
    return 1;
}

void obj_table_free(obj_table_t* table) {
    if (table->slots) {
        retro_eventcount_free(&table->users_left);
    }
    free(table->slots);
    table->slots = NULL;
    table->count = 0;
}

int obj_table_add(obj_table_t* table, void* object, int first, int end) {
    int i;
    if (first < 0) {
        first = 0;
    }
    if (end > table->count) {
        end = table->count;
    }
    for (i = first; i < end; i++) {
        obj_slot_t* slot = &table->slots[i];
        if (retro_atomic_load_acquire_int(&slot->word) != 0 ||
            !retro_atomic_cas_int(&slot->word, 0, OBJ_FILLING)) {
            continue;
        }
        retro_atomic_store_release_ptr(&slot->object, object);
        retro_atomic_store_release_int(&slot->word, OBJ_LIVE);
        return i;
    }
    return -1;
}

void* obj_table_hold(obj_table_t* table, int index) {
    obj_slot_t* slot;
    if (index < 0 || index >= table->count) {
        return NULL;
    }
    slot = &table->slots[index];
    for (;;) {
        const int word = retro_atomic_load_acquire_int(&slot->word);
        if ((word & (OBJ_LIVE | OBJ_DOOMED)) != OBJ_LIVE) {
            return NULL;
        }
        if (retro_atomic_cas_int(&slot->word, word, word + OBJ_USER)) {
            return retro_atomic_load_acquire_ptr(&slot->object);
        }
    }
}

int obj_table_release(obj_table_t* table, int index) {
    obj_slot_t* slot = &table->slots[index];
    const int word = retro_atomic_fetch_add_seq_cst_int(&slot->word, -OBJ_USER) - OBJ_USER;
    retro_eventcount_notify(&table->users_left);
    /* The last user of a doomed one: it is this thread's to free */
    if (word == (OBJ_LIVE | OBJ_DOOMED)) {
        return retro_atomic_cas_int(&slot->word, word, OBJ_DOOMED);
    }
    return 0;
}

int obj_table_doom(obj_table_t* table, int index) {
    obj_slot_t* slot = &table->slots[index];
    for (;;) {
        const int word = retro_atomic_load_acquire_int(&slot->word);
        if ((word & (OBJ_LIVE | OBJ_DOOMED)) != OBJ_LIVE) {
            return -1;
        }
        /* No users: off the table at once, this thread's to free */
        if (word == OBJ_LIVE) {
            if (retro_atomic_cas_int(&slot->word, word, OBJ_DOOMED)) {
                return 1;
            }
            continue;
        }
        if (retro_atomic_cas_int(&slot->word, word, word | OBJ_DOOMED)) {
            return 0;
        }
    }
}

void obj_table_reuse(obj_table_t* table, int index) {
    obj_slot_t* slot = &table->slots[index];
    retro_atomic_store_release_ptr(&slot->object, NULL);
    retro_atomic_store_release_int(&slot->word, 0);
}

int obj_table_users(obj_table_t* table, int index) {
    if (index < 0 || index >= table->count) {
        return 0;
    }
    return retro_atomic_load_acquire_int(&table->slots[index].word) / OBJ_USER;
}

int obj_table_live(obj_table_t* table, int index) {
    if (index < 0 || index >= table->count) {
        return 0;
    }
    return (retro_atomic_load_acquire_int(&table->slots[index].word) & (OBJ_LIVE | OBJ_DOOMED)) ==
           OBJ_LIVE;
}

void* obj_table_peek(obj_table_t* table, int index) {
    if (index < 0 || index >= table->count) {
        return NULL;
    }
    return retro_atomic_load_acquire_ptr(&table->slots[index].object);
}
