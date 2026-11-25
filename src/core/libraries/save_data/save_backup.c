/* SPDX-FileCopyrightText: Copyright 2024-2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

/* Save backups: one thread copies a save into its sce_backup folder when asked, through a
 * folder of its own so a backup is never half there; a broken save is put back from it. The
 * queue is a fixed ring that the combiner runs on one at a time, as is a backup or a restore. */

#include <string.h>

#include <compat/strl.h>
#include <file/file_path.h>
#include <retro_atomic.h>
#include <retro_dirent.h>
#include <rthreads/retro_eventcount.h>
#include <streams/file_stream.h>

#include "common/c_bridge.h"
#include "common/combine.h"
#include "common/enum_names.h"
#include "common/run_once.h"
#include "common/stop_thread.h"
#include "common/thread_name.h"
#include "core/libraries/kernel/kernel_c.h"
#include "core/libraries/save_data/save_backup.h"

#define SAVE_BACKUP_DIR "sce_backup"         /* the backup */
#define SAVE_BACKUP_DIR_TMP "sce_backup_tmp" /* a backup being made */
#define SAVE_BACKUP_DIR_OLD "sce_backup_old" /* the backup before it */

/* The saves queued at once (one each), and one more for the stop */
#define SAVE_BACKUP_QUEUE 32
/* The backups done the game may still ask about */
#define SAVE_BACKUP_RESULTS 20

static stop_thread_t g_backup_thread = STOP_THREAD_INIT;
/* Notified when a request is queued: the thread waits on it for its queue to have one */
static retro_eventcount_t g_backup_queued;

/* A backup or a restore at a time */
static combiner_t g_backup_running;
/* The queue and the results */
static combiner_t g_backup_queue_lock;
static save_backup_request_t g_backup_queue[SAVE_BACKUP_QUEUE + 1];
static unsigned g_backup_queue_head, g_backup_queue_count;
static save_backup_request_t g_result_queue[SAVE_BACKUP_RESULTS];
static unsigned g_result_head, g_result_count;

static retro_atomic_int_t g_backup_progress;
static retro_atomic_int_t g_backup_status;

static run_once_t g_backup_once = RUN_ONCE_INIT;

static void save_backup_init_once(void* arg) {
    (void)arg;
    retro_eventcount_init(&g_backup_queued);
    combiner_init(&g_backup_running);
    combiner_init(&g_backup_queue_lock);
    retro_atomic_int_init(&g_backup_progress, 0);
    retro_atomic_int_init(&g_backup_status, SAVE_DATA_BACKUP_STATUS_NOT_STARTED);
}

static void save_backup_ensure_init(void) {
    run_once(&g_backup_once, save_backup_init_once, NULL);
}

static SaveDataBackupStatus save_backup_load_status(void) {
    return (SaveDataBackupStatus)retro_atomic_load_seq_cst_int(&g_backup_status);
}

static void save_backup_set_status(SaveDataBackupStatus status) {
    (void)retro_atomic_exchange_int(&g_backup_status, (int)status);
}

static bool save_backup_is_entry(const char* name) {
    return strcmp(name, ".") != 0 && strcmp(name, "..") != 0;
}

/* Copies @from to @to through the VFS, which clones each file where the file system can and
 * otherwise moves it without holding all of it */
static void save_backup_copy(const char* from, const char* to) {
    if (!path_copy_tree(from, to)) {
        c_log(C_LOG_LIB_SAVE_DATA, C_LOG_LEVEL_ERROR, "save_backup_copy",
              "Backup: failed to copy %s to %s", from, to);
    }
}

typedef struct save_backup_call {
    const char* save_path;
    bool ok; /* a restore's answer */
} save_backup_call_t;

static void save_backup_run(void* ctx) {
    const char* dir = ((save_backup_call_t*)ctx)->save_path;
    char backup_dir[PATH_MAX_LENGTH], backup_tmp[PATH_MAX_LENGTH], backup_old[PATH_MAX_LENGTH];
    struct RDIR* rdir;
    int total = 0, current = 0;
    bool had_backup;

    if (!path_is_valid(dir)) {
        return;
    }
    fill_pathname_join(backup_dir, dir, SAVE_BACKUP_DIR, sizeof(backup_dir));
    fill_pathname_join(backup_tmp, dir, SAVE_BACKUP_DIR_TMP, sizeof(backup_tmp));
    fill_pathname_join(backup_old, dir, SAVE_BACKUP_DIR_OLD, sizeof(backup_old));
    path_remove_tree(backup_tmp);
    path_remove_tree(backup_old);

    /* What is copied: everything but the backup (counted first, for the progress) */
    rdir = retro_opendir_include_hidden(dir, true);
    if (rdir != NULL) {
        while (retro_readdir(rdir)) {
            const char* name = retro_dirent_get_name(rdir);
            if (save_backup_is_entry(name) && strcmp(name, SAVE_BACKUP_DIR) != 0) {
                total++;
            }
        }
        retro_closedir(rdir);
    }

    (void)retro_atomic_exchange_int(&g_backup_progress, 0);
    path_mkdir(backup_tmp);
    rdir = retro_opendir_include_hidden(dir, true);
    if (rdir != NULL) {
        while (retro_readdir(rdir) && current < total) {
            const char* name = retro_dirent_get_name(rdir);
            char from[PATH_MAX_LENGTH], to[PATH_MAX_LENGTH];
            if (!save_backup_is_entry(name) || strcmp(name, SAVE_BACKUP_DIR) == 0 ||
                strcmp(name, SAVE_BACKUP_DIR_TMP) == 0) {
                continue;
            }
            fill_pathname_join(from, dir, name, sizeof(from));
            fill_pathname_join(to, backup_tmp, name, sizeof(to));
            save_backup_copy(from, to);
            current++;
            (void)retro_atomic_exchange_int(&g_backup_progress, current * 100 / total);
        }
        retro_closedir(rdir);
    }
    had_backup = path_is_valid(backup_dir);
    if (had_backup) {
        filestream_rename(backup_dir, backup_old);
    }
    filestream_rename(backup_tmp, backup_dir);
    if (had_backup) {
        path_remove_tree(backup_old);
    }
}

/* The queue's front, copied into @ctx: false if it is empty */
typedef struct save_backup_take {
    save_backup_request_t* req;
    bool taken;
} save_backup_take_t;

static void save_backup_take_locked(void* ctx) {
    save_backup_take_t* t = (save_backup_take_t*)ctx;
    t->taken = g_backup_queue_count != 0;
    if (t->taken) {
        *t->req = g_backup_queue[g_backup_queue_head];
    }
}

static bool save_backup_take(save_backup_request_t* req) {
    save_backup_take_t t;
    t.req = req;
    t.taken = false;
    combiner_run(&g_backup_queue_lock, save_backup_take_locked, &t);
    return t.taken;
}

static void save_backup_push_result_locked(const save_backup_request_t* req) {
    if (g_result_count == SAVE_BACKUP_RESULTS) {
        /* The oldest goes */
        g_result_head = (g_result_head + 1) % SAVE_BACKUP_RESULTS;
        g_result_count--;
    }
    g_result_queue[(g_result_head + g_result_count) % SAVE_BACKUP_RESULTS] = *req;
    g_result_count++;
}

/* The front is done: off the queue, and among the results unless it is not to be told */
static void save_backup_finish_locked(void* ctx) {
    const save_backup_request_t* req = (const save_backup_request_t*)ctx;
    g_backup_queue[g_backup_queue_head].done = true;
    g_backup_queue_head = (g_backup_queue_head + 1) % (SAVE_BACKUP_QUEUE + 1);
    g_backup_queue_count--;
    if (req->origin != SAVE_DATA_EVENT_DO_NOT_SAVE) {
        save_backup_push_result_locked(req);
    }
}

static void save_backup_pop_stop_locked(void* ctx) {
    (void)ctx;
    g_backup_queue_head = (g_backup_queue_head + 1) % (SAVE_BACKUP_QUEUE + 1);
    g_backup_queue_count--;
}

static int save_backup_stopping(void* ctx) {
    (void)ctx;
    return save_backup_load_status() == SAVE_DATA_BACKUP_STATUS_STOPPING;
}

static void save_backup_body(void* arg) {
    static save_backup_request_t req; /* the thread's alone */
    (void)arg;
    thread_name_set("shadPS4:SaveData:BackupThread");
    while (save_backup_load_status() != SAVE_DATA_BACKUP_STATUS_STOPPING) {
        save_backup_call_t call;
        save_backup_set_status(SAVE_DATA_BACKUP_STATUS_WAITING);

        /* Until a request is queued: the wait is opened before the queue is looked at again,
         * so one queued in between is not slept through */
        while (!save_backup_take(&req)) {
            const int key = retro_eventcount_prepare_wait(&g_backup_queued);
            if (save_backup_take(&req)) {
                retro_eventcount_cancel_wait(&g_backup_queued);
                break;
            }
            retro_eventcount_commit_wait(&g_backup_queued, key);
        }
        if (req.save_path[0] == '\0') {
            /* The stop: off the queue, so a thread started again does not stop at it */
            combiner_run(&g_backup_queue_lock, save_backup_pop_stop_locked, NULL);
            break;
        }
        save_backup_set_status(SAVE_DATA_BACKUP_STATUS_RUNNING);

        c_log(C_LOG_LIB_SAVE_DATA, C_LOG_LEVEL_INFO, "save_backup_body",
              "Backing up the following directory: %s", req.save_path);
        call.save_path = req.save_path;
        call.ok = true;
        combiner_run(&g_backup_running, save_backup_run, &call);
        c_log(C_LOG_LIB_SAVE_DATA, C_LOG_LEVEL_DEBUG, "save_backup_body",
              "Backing up the following directory: %s finished", req.save_path);
        combiner_run(&g_backup_queue_lock, save_backup_finish_locked, &req);

        /* Not too often: five seconds of the game's time, or until the thread is told to stop */
        kernel_game_wait(&g_backup_queued, kernel_game_deadline_in(5000000000ULL),
                         save_backup_stopping, NULL);
    }
    save_backup_set_status(SAVE_DATA_BACKUP_STATUS_NOT_STARTED);
}

void save_backup_start_thread(void) {
    save_backup_ensure_init();
    if (save_backup_load_status() != SAVE_DATA_BACKUP_STATUS_NOT_STARTED) {
        return;
    }
    c_log(C_LOG_LIB_SAVE_DATA, C_LOG_LEVEL_DEBUG, "save_backup_start_thread",
          "Starting backup thread");
    save_backup_set_status(SAVE_DATA_BACKUP_STATUS_WAITING);
    /* One that ended by itself is joined first */
    stop_thread_join(&g_backup_thread);
    stop_thread_start(&g_backup_thread, save_backup_body, NULL);
}

static void save_backup_push_stop_locked(void* ctx) {
    save_backup_request_t* slot;
    (void)ctx;
    /* The ring keeps one place for this */
    slot = &g_backup_queue[(g_backup_queue_head + g_backup_queue_count) % (SAVE_BACKUP_QUEUE + 1)];
    memset(slot, 0, sizeof(*slot));
    g_backup_queue_count++;
}

static void save_backup_drop_stops_locked(void* ctx) {
    unsigned i, kept = 0;
    (void)ctx;
    for (i = 0; i < g_backup_queue_count; i++) {
        const save_backup_request_t* it =
            &g_backup_queue[(g_backup_queue_head + i) % (SAVE_BACKUP_QUEUE + 1)];
        if (it->save_path[0] != '\0') {
            if (kept != i) {
                g_backup_queue[(g_backup_queue_head + kept) % (SAVE_BACKUP_QUEUE + 1)] = *it;
            }
            kept++;
        }
    }
    g_backup_queue_count = kept;
}

void save_backup_stop_thread(void) {
    SaveDataBackupStatus status;
    save_backup_ensure_init();
    status = save_backup_load_status();
    if (status == SAVE_DATA_BACKUP_STATUS_NOT_STARTED ||
        status == SAVE_DATA_BACKUP_STATUS_STOPPING) {
        return;
    }
    c_log(C_LOG_LIB_SAVE_DATA, C_LOG_LEVEL_DEBUG, "save_backup_stop_thread",
          "Stopping backup thread");
    save_backup_set_status(SAVE_DATA_BACKUP_STATUS_STOPPING);
    combiner_run(&g_backup_queue_lock, save_backup_push_stop_locked, NULL);
    retro_eventcount_notify(&g_backup_queued);
    /* The thread ends once it reaches that request: waited for, not looked in on */
    stop_thread_join(&g_backup_thread);
    /* A thread that saw the stop before it took that request leaves it queued: it goes, so a
     * thread started again does not stop at it, and what was queued behind it is backed up */
    combiner_run(&g_backup_queue_lock, save_backup_drop_stops_locked, NULL);
}

typedef struct save_backup_new {
    const save_backup_request_t* req;
    bool queued;
} save_backup_new_t;

static void save_backup_new_locked(void* ctx) {
    save_backup_new_t* n = (save_backup_new_t*)ctx;
    unsigned i;
    n->queued = false;
    for (i = 0; i < g_backup_queue_count; i++) {
        const save_backup_request_t* it =
            &g_backup_queue[(g_backup_queue_head + i) % (SAVE_BACKUP_QUEUE + 1)];
        if (strcmp(it->dir_name, n->req->dir_name) == 0) {
#ifndef NDEBUG
            c_log(C_LOG_LIB_SAVE_DATA, C_LOG_LEVEL_TRACE, "save_backup_new_request",
                  "Backup request to %s ignored. Already queued", n->req->dir_name);
#endif
            return;
        }
    }
    if (g_backup_queue_count >= SAVE_BACKUP_QUEUE) {
        c_log(C_LOG_LIB_SAVE_DATA, C_LOG_LEVEL_ERROR, "save_backup_new_request",
              "Backup request to %s ignored: %d saves are queued", n->req->dir_name,
              SAVE_BACKUP_QUEUE);
        return;
    }
    g_backup_queue[(g_backup_queue_head + g_backup_queue_count) % (SAVE_BACKUP_QUEUE + 1)] =
        *n->req;
    g_backup_queue_count++;
    n->queued = true;
}

bool save_backup_new_request(s32 user_id, const char* title_id, const char* dir_name, u32 origin) {
    save_backup_new_t n;
    SaveDataBackupStatus status;
    save_backup_request_t local;

    save_backup_ensure_init();
    memset(&local, 0, sizeof(local));
    local.user_id = user_id;
    strlcpy(local.title_id, title_id, sizeof(local.title_id));
    strlcpy(local.dir_name, dir_name, sizeof(local.dir_name));
    local.origin = origin;
    save_make_dir_path(local.save_path, sizeof(local.save_path), user_id, title_id, dir_name);

    status = save_backup_load_status();
    if (status != SAVE_DATA_BACKUP_STATUS_WAITING && status != SAVE_DATA_BACKUP_STATUS_RUNNING) {
        c_log(C_LOG_LIB_SAVE_DATA, C_LOG_LEVEL_ERROR, "save_backup_new_request",
              "Called backup while status is %s. Backup request to %s ignored",
              enum_name(&enum_names_SaveDataBackupStatus, (s64)status), local.save_path);
        return false;
    }
    n.req = &local;
    n.queued = false;
    combiner_run(&g_backup_queue_lock, save_backup_new_locked, &n);
    if (!n.queued) {
        return false;
    }
    retro_eventcount_notify(&g_backup_queued);
    return true;
}

static void save_backup_restore_run(void* ctx) {
    save_backup_call_t* call = (save_backup_call_t*)ctx;
    char backup_dir[PATH_MAX_LENGTH];
    struct RDIR* rdir;
    fill_pathname_join(backup_dir, call->save_path, SAVE_BACKUP_DIR, sizeof(backup_dir));
    call->ok = false;
    if (!path_is_valid(call->save_path) || !path_is_valid(backup_dir)) {
        return;
    }
    rdir = retro_opendir_include_hidden(call->save_path, true);
    if (rdir != NULL) {
        while (retro_readdir(rdir)) {
            const char* name = retro_dirent_get_name(rdir);
            char entry[PATH_MAX_LENGTH];
            if (!save_backup_is_entry(name) || strcmp(name, SAVE_BACKUP_DIR) == 0) {
                continue;
            }
            fill_pathname_join(entry, call->save_path, name, sizeof(entry));
            path_remove_tree(entry);
        }
        retro_closedir(rdir);
    }
    path_copy_tree(backup_dir, call->save_path);
    call->ok = true;
}

bool save_backup_restore(const char* save_path) {
    save_backup_call_t call;
    save_backup_ensure_init();
    c_log(C_LOG_LIB_SAVE_DATA, C_LOG_LEVEL_INFO, "save_backup_restore", "Restoring backup for %s",
          save_path);
    call.save_path = save_path;
    call.ok = false;
    combiner_run(&g_backup_running, save_backup_restore_run, &call);
    return call.ok;
}

SaveDataBackupStatus save_backup_status(void) {
    save_backup_ensure_init();
    return save_backup_load_status();
}

typedef struct save_backup_find {
    const char* save_path;
    bool found;
} save_backup_find_t;

static void save_backup_find_locked(void* ctx) {
    save_backup_find_t* f = (save_backup_find_t*)ctx;
    unsigned i;
    f->found = false;
    for (i = 0; i < g_backup_queue_count; i++) {
        const save_backup_request_t* it =
            &g_backup_queue[(g_backup_queue_head + i) % (SAVE_BACKUP_QUEUE + 1)];
        if (strcmp(it->save_path, f->save_path) == 0) {
            /* The first with that path */
            f->found = !it->done;
            return;
        }
    }
}

bool save_backup_is_executing_for(const char* save_path) {
    save_backup_find_t f;
    save_backup_ensure_init();
    f.save_path = save_path;
    f.found = false;
    combiner_run(&g_backup_queue_lock, save_backup_find_locked, &f);
    return f.found;
}

void save_backup_make_path(const char* save_path, char* out, size_t size) {
    fill_pathname_join(out, save_path, SAVE_BACKUP_DIR, size);
}

typedef struct save_backup_pop {
    save_backup_request_t* out;
    bool popped;
} save_backup_pop_t;

static void save_backup_pop_locked(void* ctx) {
    save_backup_pop_t* p = (save_backup_pop_t*)ctx;
    p->popped = g_result_count != 0;
    if (p->popped) {
        *p->out = g_result_queue[g_result_head];
        g_result_head = (g_result_head + 1) % SAVE_BACKUP_RESULTS;
        g_result_count--;
    }
}

bool save_backup_pop_last_event(save_backup_request_t* out) {
    save_backup_pop_t p;
    save_backup_ensure_init();
    p.out = out;
    p.popped = false;
    combiner_run(&g_backup_queue_lock, save_backup_pop_locked, &p);
    return p.popped;
}

float save_backup_progress(void) {
    save_backup_ensure_init();
    return (float)retro_atomic_load_seq_cst_int(&g_backup_progress) / 100.0f;
}

void save_backup_clear_progress(void) {
    save_backup_ensure_init();
    (void)retro_atomic_exchange_int(&g_backup_progress, 0);
}
