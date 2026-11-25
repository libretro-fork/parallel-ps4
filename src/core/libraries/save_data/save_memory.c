/* SPDX-FileCopyrightText: Copyright 2024-2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

/* Save data memory: each slot's bytes, held from their first use, written whole to memory.dat
 * at each write. The slots are a fixed table that one combiner runs on at a time. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <compat/strl.h>
#include <file/file_path.h>
#include <streams/file_stream.h>

#include "common/c_bridge.h"
#include "common/combine.h"
#include "common/run_once.h"
#include "core/file_sys/fs_c.h"
#include "core/libraries/save_data/save_backup.h"
#include "core/libraries/save_data/save_instance.h"
#include "core/libraries/save_data/save_memory.h"

#define SAVE_MEMORY_DIR "sce_sdmemory"
#define SAVE_MEMORY_FILE "memory.dat"

typedef struct save_memory_slot {
    bool used;
    u32 slot_id;
    s32 user_id;
    char game_serial[SAVE_NAME_SIZE];
    char folder_path[PATH_MAX_LENGTH];
    psf_t sfo;
    u8* memory;         /* the bytes, once read (loaded) */
    size_t memory_size; /* how many */
    bool loaded;
} save_memory_slot_t;

static combiner_t g_slot_lock;
static save_memory_slot_t g_slots[SAVE_MEMORY_SLOTS];
static run_once_t g_slot_once = RUN_ONCE_INIT;

static void save_memory_init_once(void* arg) {
    (void)arg;
    combiner_init(&g_slot_lock);
}

static void save_memory_lock(void (*call)(void* ctx), void* ctx) {
    run_once(&g_slot_once, save_memory_init_once, NULL);
    combiner_run(&g_slot_lock, call, ctx);
}

static save_memory_slot_t* save_memory_find(u32 slot_id) {
    int i;
    for (i = 0; i < SAVE_MEMORY_SLOTS; i++) {
        if (g_slots[i].used && g_slots[i].slot_id == slot_id) {
            return &g_slots[i];
        }
    }
    return NULL;
}

static void save_memory_file_path(const save_memory_slot_t* slot, const char* rel, char* out,
                                  size_t size) {
    fill_pathname_join(out, slot->folder_path, rel, size);
}

/* The bytes from memory.dat, the first time they are needed; what it lacks is zeros */
static void save_memory_load(save_memory_slot_t* slot) {
    char path[PATH_MAX_LENGTH];
    RFILE* f;
    if (slot->loaded) {
        return;
    }
    slot->loaded = true;
    if (slot->memory == NULL && slot->memory_size != 0) {
        slot->memory = (u8*)calloc(slot->memory_size, 1);
        if (slot->memory == NULL) {
            slot->memory_size = 0;
            return;
        }
    }
    save_memory_file_path(slot, SAVE_MEMORY_FILE, path, sizeof(path));
    f = filestream_open(path, RETRO_VFS_FILE_ACCESS_READ, RETRO_VFS_FILE_ACCESS_HINT_NONE);
    if (f != NULL) {
        if (slot->memory_size != 0) {
            filestream_read(f, slot->memory, (int64_t)slot->memory_size);
        }
        filestream_close(f);
    }
}

static void save_memory_persist_slot(save_memory_slot_t* slot) {
    char path[PATH_MAX_LENGTH];
    char msg[PATH_MAX_LENGTH + 64];
    path_mkdir(slot->folder_path);
    save_memory_file_path(slot, SAVE_MEMORY_FILE, path, sizeof(path));
    if (save_write_file(path, slot->memory != NULL ? (const void*)slot->memory : (const void*)"",
                        slot->memory_size)) {
        return;
    }
    c_log(C_LOG_LIB_SAVE_DATA, C_LOG_LEVEL_ERROR, "save_memory_persist",
          "Failed to persist save memory at %s", path);
    snprintf(msg, sizeof(msg), "Failed to persist save memory\nat %s", path);
    save_show_message(msg);
}

typedef struct save_memory_call {
    u32 slot_id;
    const void* in;
    void* out;
    size_t size;
    s64 offset;
    s32 user_id;
    const char* game_serial;
    size_t result;
    bool ok;
} save_memory_call_t;

static void save_memory_persist_locked(void* ctx) {
    save_memory_call_t* c = (save_memory_call_t*)ctx;
    save_memory_slot_t* slot = save_memory_find(c->slot_id);
    if (slot != NULL) {
        save_memory_load(slot);
        save_memory_persist_slot(slot);
    }
}

void save_memory_persist(u32 slot_id) {
    save_memory_call_t c;
    memset(&c, 0, sizeof(c));
    c.slot_id = slot_id;
    save_memory_lock(save_memory_persist_locked, &c);
}

void save_memory_dir_name(u32 slot_id, char* out, size_t size) {
    if (slot_id > 0) {
        snprintf(out, size, "%s%u", SAVE_MEMORY_DIR, (unsigned)slot_id);
    } else {
        strlcpy(out, SAVE_MEMORY_DIR, size);
    }
}

void save_memory_save_path(s32 user_id, u32 slot_id, const char* game_serial, char* out,
                           size_t size) {
    char dir[SAVE_NAME_SIZE];
    save_memory_dir_name(slot_id, dir, sizeof(dir));
    save_make_dir_path(out, size, user_id, game_serial, dir);
}

static void save_memory_setup_locked(void* ctx) {
    save_memory_call_t* c = (save_memory_call_t*)ctx;
    save_memory_slot_t* slot = save_memory_find(c->slot_id);
    char dir[SAVE_NAME_SIZE];
    char path[PATH_MAX_LENGTH];
    int64_t size;
    int i;

    c->ok = false;
    c->result = 0;
    if (slot == NULL) {
        for (i = 0; i < SAVE_MEMORY_SLOTS && slot == NULL; i++) {
            if (!g_slots[i].used) {
                slot = &g_slots[i];
            }
        }
        if (slot == NULL) {
            c_log(C_LOG_LIB_SAVE_DATA, C_LOG_LEVEL_ERROR, "save_memory_setup",
                  "No room for save memory slot %u: %d are in use", (unsigned)c->slot_id,
                  SAVE_MEMORY_SLOTS);
            return;
        }
        psf_init(&slot->sfo);
        slot->memory = NULL;
    }
    /* Anew: what it held goes */
    free(slot->memory);
    slot->memory = NULL;
    psf_free(&slot->sfo);
    slot->used = true;
    slot->slot_id = c->slot_id;
    slot->user_id = c->user_id;
    strlcpy(slot->game_serial, c->game_serial, sizeof(slot->game_serial));
    save_memory_save_path(c->user_id, c->slot_id, c->game_serial, slot->folder_path,
                          sizeof(slot->folder_path));
    slot->memory_size = c->size;
    slot->loaded = false;
    c->ok = true;

    save_memory_dir_name(c->slot_id, dir, sizeof(dir));
    save_default_param_sfo(&slot->sfo, dir, c->game_serial);

    save_join(path, sizeof(path), slot->folder_path, "sce_sys/param.sfo");
    if (!path_is_valid(path)) {
        return;
    }
    {
        char corrupt[PATH_MAX_LENGTH];
        save_memory_file_path(slot, "corrupted", corrupt, sizeof(corrupt));
        if (!psf_open_path(&slot->sfo, path) || path_is_valid(corrupt)) {
            if (!save_backup_restore(slot->folder_path)) { /* could not restore the backup */
                return;
            }
        }
    }
    save_memory_file_path(slot, SAVE_MEMORY_FILE, path, sizeof(path));
    size = path_get_size(path);
    if (size > 0) {
        c->result = (size_t)size;
    }
}

bool save_memory_setup(s32 user_id, u32 slot_id, const char* game_serial, size_t memory_size,
                       size_t* existed_size) {
    save_memory_call_t c;
    memset(&c, 0, sizeof(c));
    c.slot_id = slot_id;
    c.user_id = user_id;
    c.game_serial = game_serial;
    c.size = memory_size;
    save_memory_lock(save_memory_setup_locked, &c);
    *existed_size = c.result;
    return c.ok;
}

static void save_memory_set_icon_locked(void* ctx) {
    save_memory_call_t* c = (save_memory_call_t*)ctx;
    save_memory_slot_t* slot = save_memory_find(c->slot_id);
    char icon[PATH_MAX_LENGTH];
    char sys[PATH_MAX_LENGTH];
    if (slot == NULL) {
        return;
    }
    save_join(sys, sizeof(sys), slot->folder_path, "sce_sys");
    save_join(icon, sizeof(icon), sys, "icon0.png");
    if (c->in == NULL) {
        if (path_is_valid(icon)) {
            filestream_delete(icon);
        }
        /* Through the mount stack, so a game in an archive gives the slot its icon too */
        path_mkdir(sys);
        save_write_game_icon(icon);
    } else {
        save_write_file(icon, c->in, c->size);
    }
}

void save_memory_set_icon(u32 slot_id, const void* buf, size_t buf_size) {
    save_memory_call_t c;
    memset(&c, 0, sizeof(c));
    c.slot_id = slot_id;
    c.in = buf;
    c.size = buf_size;
    save_memory_lock(save_memory_set_icon_locked, &c);
}

static void save_memory_find_locked(void* ctx) {
    save_memory_call_t* c = (save_memory_call_t*)ctx;
    c->out = save_memory_find(c->slot_id);
}

bool save_memory_is_initialized(u32 slot_id) {
    save_memory_call_t c;
    memset(&c, 0, sizeof(c));
    c.slot_id = slot_id;
    save_memory_lock(save_memory_find_locked, &c);
    return c.out != NULL;
}

psf_t* save_memory_param_sfo(u32 slot_id) {
    save_memory_call_t c;
    memset(&c, 0, sizeof(c));
    c.slot_id = slot_id;
    save_memory_lock(save_memory_find_locked, &c);
    return c.out != NULL ? &((save_memory_slot_t*)c.out)->sfo : NULL;
}

static void save_memory_copy_lent(const u8* data, size_t size, void* ctx) {
    save_memory_call_t* c = (save_memory_call_t*)ctx;
    c->result = size < c->size ? size : c->size;
    if (c->result != 0) {
        memcpy(c->out, data, c->result);
    }
}

static void save_memory_copy_icon_locked(void* ctx) {
    save_memory_call_t* c = (save_memory_call_t*)ctx;
    save_memory_slot_t* slot = save_memory_find(c->slot_id);
    char icon[PATH_MAX_LENGTH];
    c->result = 0;
    if (slot == NULL) {
        return;
    }
    save_join(icon, sizeof(icon), slot->folder_path, "sce_sys/icon0.png");
    /* Copied once, from the mapped file into the game's buffer */
    fs_lend_host_file(icon, save_memory_copy_lent, c);
}

size_t save_memory_copy_icon(u32 slot_id, void* buf, size_t size) {
    save_memory_call_t c;
    memset(&c, 0, sizeof(c));
    c.slot_id = slot_id;
    c.out = buf;
    c.size = size;
    save_memory_lock(save_memory_copy_icon_locked, &c);
    return c.result;
}

static void save_memory_save_sfo_locked(void* ctx) {
    save_memory_call_t* c = (save_memory_call_t*)ctx;
    save_memory_slot_t* slot = save_memory_find(c->slot_id);
    char sys[PATH_MAX_LENGTH];
    char path[PATH_MAX_LENGTH];
    c->ok = false;
    if (slot == NULL) {
        return;
    }
    save_join(sys, sizeof(sys), slot->folder_path, "sce_sys");
    save_join(path, sizeof(path), sys, "param.sfo");
    path_mkdir(sys);
    c->ok = psf_encode_path(&slot->sfo, path);
    if (!c->ok) {
        c_log(C_LOG_LIB_SAVE_DATA, C_LOG_LEVEL_ERROR, "save_memory_save_sfo",
              "Failed to encode param.sfo");
    }
}

bool save_memory_save_sfo(u32 slot_id) {
    save_memory_call_t c;
    memset(&c, 0, sizeof(c));
    c.slot_id = slot_id;
    save_memory_lock(save_memory_save_sfo_locked, &c);
    return c.ok;
}

static void save_memory_read_locked(void* ctx) {
    save_memory_call_t* c = (save_memory_call_t*)ctx;
    save_memory_slot_t* slot = save_memory_find(c->slot_id);
    size_t n;
    if (slot == NULL) {
        return;
    }
    save_memory_load(slot);
    if (c->offset < 0 || (u64)c->offset >= slot->memory_size) {
        return; /* nothing there */
    }
    n = c->size;
    if (n > slot->memory_size - (size_t)c->offset) {
        n = slot->memory_size - (size_t)c->offset;
    }
    memcpy(c->out, slot->memory + c->offset, n);
}

void save_memory_read(u32 slot_id, void* buf, size_t buf_size, s64 offset) {
    save_memory_call_t c;
    memset(&c, 0, sizeof(c));
    c.slot_id = slot_id;
    c.out = buf;
    c.size = buf_size;
    c.offset = offset;
    save_memory_lock(save_memory_read_locked, &c);
}

static void save_memory_write_locked(void* ctx) {
    save_memory_call_t* c = (save_memory_call_t*)ctx;
    save_memory_slot_t* slot = save_memory_find(c->slot_id);
    char dir[SAVE_NAME_SIZE];
    u64 end;
    if (slot == NULL || c->offset < 0) {
        return;
    }
    save_memory_load(slot);
    end = (u64)c->offset + c->size;
    if (end > slot->memory_size) {
        /* Grows, zeros where nothing was written */
        u8* grown = (u8*)realloc(slot->memory, (size_t)end);
        if (grown == NULL) {
            c_log(C_LOG_LIB_SAVE_DATA, C_LOG_LEVEL_ERROR, "save_memory_write",
                  "No memory for %llu bytes of save memory", (unsigned long long)end);
            return;
        }
        memset(grown + slot->memory_size, 0, (size_t)end - slot->memory_size);
        slot->memory = grown;
        slot->memory_size = (size_t)end;
    }
    if (c->size != 0) {
        memcpy(slot->memory + c->offset, c->in, c->size);
    }
    save_memory_persist_slot(slot);
    save_memory_dir_name(c->slot_id, dir, sizeof(dir));
    save_backup_new_request(slot->user_id, slot->game_serial, dir, SAVE_DATA_EVENT_DO_NOT_SAVE);
}

void save_memory_write(u32 slot_id, const void* buf, size_t buf_size, s64 offset) {
    save_memory_call_t c;
    memset(&c, 0, sizeof(c));
    c.slot_id = slot_id;
    c.in = buf;
    c.size = buf_size;
    c.offset = offset;
    save_memory_lock(save_memory_write_locked, &c);
}
