/* SPDX-FileCopyrightText: Copyright 2024-2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef SHADPS4_LIBRARIES_SAVE_DATA_SAVE_BACKUP_H
#define SHADPS4_LIBRARIES_SAVE_DATA_SAVE_BACKUP_H

/* Save backups (save_backup.c): a thread copies a save into its sce_backup folder when asked,
 * and a broken save is restored from it. */

#include <stddef.h>

#include <boolean.h>
#include <retro_common_api.h>
#include <retro_miscellaneous.h>

#include "common/types.h"
#include "core/libraries/save_data/save_instance.h"

RETRO_BEGIN_DECLS

typedef enum SaveDataBackupStatus {
    SAVE_DATA_BACKUP_STATUS_NOT_STARTED = 0,
    SAVE_DATA_BACKUP_STATUS_WAITING = 1,
    SAVE_DATA_BACKUP_STATUS_RUNNING = 2,
    SAVE_DATA_BACKUP_STATUS_STOPPING = 3
} SaveDataBackupStatus;

/* What asked for a backup: the event the game is told of when it is done */
#define SAVE_DATA_EVENT_UMOUNT_BACKUP 1u
#define SAVE_DATA_EVENT_BACKUP 2u
#define SAVE_DATA_EVENT_SAVE_DATA_MEMORY_SYNC 3u
#define SAVE_DATA_EVENT_DO_NOT_SAVE 1000000u /* no event: for the backup thread alone */

typedef struct save_backup_request {
    bool done;
    s32 user_id;
    char title_id[SAVE_NAME_SIZE];
    char dir_name[SAVE_NAME_SIZE];
    u32 origin; /* SAVE_DATA_EVENT_* */
    char save_path[PATH_MAX_LENGTH];
} save_backup_request_t;

/* Nothing happens if the thread runs already */
void save_backup_start_thread(void);
void save_backup_stop_thread(void);

/* Queues a backup of the save @dir_name of @title_id: false if the thread does not run, the
 * save is queued already or the queue is full */
bool save_backup_new_request(s32 user_id, const char* title_id, const char* dir_name, u32 origin);

/* Puts the save at @save_path back as its backup has it: false if there is no backup */
bool save_backup_restore(const char* save_path);

SaveDataBackupStatus save_backup_status(void);

/* Whether a backup of @save_path is queued or running */
bool save_backup_is_executing_for(const char* save_path);

/* @save_path/sce_backup */
void save_backup_make_path(const char* save_path, char* out, size_t size);

/* The oldest backup done that the game has not been told of: false if none */
bool save_backup_pop_last_event(save_backup_request_t* out);

float save_backup_progress(void);
void save_backup_clear_progress(void);

RETRO_END_DECLS

#endif
