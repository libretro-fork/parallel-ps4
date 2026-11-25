/* SPDX-FileCopyrightText: Copyright 2024-2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef SHADPS4_LIBRARIES_SAVE_DATA_SAVE_INSTANCE_H
#define SHADPS4_LIBRARIES_SAVE_DATA_SAVE_INSTANCE_H

/* One save folder, mounted at /savedataN while the game uses it (save_instance.c), and what
 * the save library's parts share: where saves are, a save's param.sfo. */

#include <stddef.h>

#include <boolean.h>
#include <retro_common_api.h>
#include <retro_miscellaneous.h>

#include "common/types.h"
#include "core/file_format/psf.h"
#include "core/libraries/save_data/savedata.h"

RETRO_BEGIN_DECLS

/* A save's param.sfo keys */
#define SAVE_PARAM_ACCOUNT_ID "ACCOUNT_ID"
#define SAVE_PARAM_ATTRIBUTE "ATTRIBUTE"
#define SAVE_PARAM_CATEGORY "CATEGORY"
#define SAVE_PARAM_DETAIL "DETAIL"
#define SAVE_PARAM_FORMAT "FORMAT"
#define SAVE_PARAM_MAINTITLE "MAINTITLE"
#define SAVE_PARAM_PARAMS "PARAMS"
#define SAVE_PARAM_SAVEDATA_BLOCKS "SAVEDATA_BLOCKS"
#define SAVE_PARAM_SAVEDATA_DIRECTORY "SAVEDATA_DIRECTORY"
#define SAVE_PARAM_SAVEDATA_LIST_PARAM "SAVEDATA_LIST_PARAM"
#define SAVE_PARAM_SUBTITLE "SUBTITLE"
#define SAVE_PARAM_TITLE_ID "TITLE_ID"

/* The longest serial or folder name kept, terminator included */
#define SAVE_NAME_SIZE 64

/* What setting a save up can end in */
enum {
    SAVE_SETUP_OK = 0,
    SAVE_SETUP_BROKEN, /* corrupted, or its param.sfo unreadable */
    SAVE_SETUP_NO_SPACE,
    SAVE_SETUP_FAILED
};

typedef struct save_instance {
    int slot_num;
    s32 user_id;
    char game_serial[SAVE_NAME_SIZE];
    char dir_name[SAVE_NAME_SIZE];
    char save_path[PATH_MAX_LENGTH];
    char mount_point[SAVE_DATA_MOUNT_POINT_DATA_MAXSIZE];
    psf_t param_sfo;
    int max_blocks;
    bool exists;
    bool mounted;
    bool read_only;
} save_instance_t;

/* Where all of a title's saves are: <home>/<user>/savedata/<serial> */
void save_make_title_path(char* out, size_t size, s32 user_id, const char* game_serial);
/* Where one save is: <home>/<user>/savedata/<serial>/<dir> */
void save_make_dir_path(char* out, size_t size, s32 user_id, const char* game_serial,
                        const char* dir_name);
/* @dir/@rel */
void save_join(char* out, size_t size, const char* dir, const char* rel);

/* The blocks SAVEDATA_BLOCKS gives, or the most there may be */
u64 save_max_blocks_from_sfo(const psf_t* psf);
/* A new save's param.sfo: a title in the console's language, @dir_name and @game_serial */
void save_default_param_sfo(psf_t* psf, const char* dir_name, const char* game_serial);

/* OrbisSaveDataParam::FromSFO: the param's texts and user value, and when it was written */
void save_param_from_sfo(OrbisSaveDataParam* param, const psf_t* sfo);
/* OrbisSaveDataParam::ToSFO */
void save_param_to_sfo(const OrbisSaveDataParam* param, psf_t* sfo);

/* The game's INSTALL_DIR_SAVEDATA, or else its serial, cut to fit in @size */
void save_game_serial(char* out, size_t size);

/* A new instance at slot @slot_num (0 to 15), for @dir_name of @game_serial; the blocks are
 * clamped to what a save may have */
void save_instance_init(save_instance_t* s, int slot_num, s32 user_id, const char* game_serial,
                        const char* dir_name, int max_blocks);
/* Frees what it holds (unmounted first if mounted) */
void save_instance_free(save_instance_t* s);
/* Makes the save if it is not there, checks it (restoring its backup when it is broken), and
 * mounts it: SAVE_SETUP_* */
int save_instance_setup_and_mount(save_instance_t* s, bool read_only, bool copy_icon,
                                  bool ignore_corrupt, bool dont_restore_backup);
/* Writes its param.sfo and unmounts it: false if the param.sfo could not be written (it is
 * unmounted all the same) */
bool save_instance_umount(save_instance_t* s);
/* <save>/sce_sys/icon0.png */
void save_instance_icon_path(const save_instance_t* s, char* out, size_t size);

/* Writes @size bytes at @path whole, replacing it: false if it could not */
bool save_write_file(const char* path, const void* data, size_t size);
/* Writes the game's sce_sys/save_data.png at @path: false if the game has none */
bool save_write_game_icon(const char* path);
/* Shows @msg in the emulator's message dialog, with OK, until it is pressed */
void save_show_message(const char* msg);
/* The bytes in the folder @dir and everything in it */
u64 save_dir_size(const char* dir);

RETRO_END_DECLS

#endif
