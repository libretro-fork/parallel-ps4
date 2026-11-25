/* SPDX-FileCopyrightText: Copyright 2024-2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

/* A save folder: made, checked, mounted at /savedataN and unmounted; where saves are kept. */

#include <stdio.h>
#include <string.h>

#include <compat/strl.h>
#include <file/file_path.h>
#include <retro_dirent.h>
#include <streams/file_stream.h>

#include "common/c_bridge.h"
#include "core/emulator_settings.h"
#include "core/file_sys/fs_c.h"
#include "core/libraries/kernel/kernel_c.h"
#include "core/libraries/save_data/save_backup.h"
#include "core/libraries/save_data/save_instance.h"
#include "video_core/overlay/ovl_layers.h"

#define SAVE_SCE_SYS "sce_sys" /* the system folder inside a save */

/* A new save's title, by the console's language */
static const struct {
    int language;
    const char* title;
} save_default_titles[] = {
    {0 /* ja_JP */, "セーブデータ"},
    {1 /* en_US */, "Saved Data"},
    {2 /* fr_FR */, "Données sauvegardées"},
    {3 /* es_ES */, "Datos guardados"},
    {4 /* de_DE */, "Gespeicherte Daten"},
    {5 /* it_IT */, "Dati salvati"},
    {6 /* nl_NL */, "Opgeslagen data"},
    {7 /* pt_PT */, "Dados guardados"},
    {8 /* ru_RU */, "Сохраненные данные"},
    {9 /* ko_KR */, "저장 데이터"},
    {10 /* zh_CN */, "保存数据"},
    {12 /* fi_FI */, "Tallennetut tiedot"},
    {13 /* sv_SE */, "Sparade data"},
    {14 /* da_DK */, "Gemte data"},
    {15 /* no_NO */, "Lagrede data"},
    {16 /* pl_PL */, "Zapisane dane"},
    {17 /* pt_BR */, "Dados salvos"},
    {19 /* tr_TR */, "Kayıtlı Veriler"},
};

static size_t save_strnlen(const char* s, size_t max) {
    const char* end = (const char*)memchr(s, 0, max);
    return end != NULL ? (size_t)(end - s) : max;
}

/* Up to @max bytes of @src (to its terminator) into @out, terminated, cut to fit */
static void save_copy_text(char* out, size_t size, const char* src, size_t length) {
    const size_t n = length < size - 1 ? length : size - 1;
    memcpy(out, src, n);
    out[n] = '\0';
}

void save_join(char* out, size_t size, const char* dir, const char* rel) {
    fill_pathname_join(out, dir, rel, size);
}

void save_make_title_path(char* out, size_t size, s32 user_id, const char* game_serial) {
    char home[PATH_MAX_LENGTH];
    char user[16];
    char tmp[PATH_MAX_LENGTH];
    emu_settings_get_home_dir(home, sizeof(home));
    snprintf(user, sizeof(user), "%d", (int)user_id);
    fill_pathname_join(tmp, home, user, sizeof(tmp));
    fill_pathname_join(out, tmp, "savedata", size);
    strlcpy(tmp, out, sizeof(tmp));
    fill_pathname_join(out, tmp, game_serial, size);
}

void save_make_dir_path(char* out, size_t size, s32 user_id, const char* game_serial,
                        const char* dir_name) {
    char title[PATH_MAX_LENGTH];
    save_make_title_path(title, sizeof(title), user_id, game_serial);
    fill_pathname_join(out, title, dir_name, size);
}

u64 save_max_blocks_from_sfo(const psf_t* psf) {
    const u8* value;
    u32 length;
    u8 bytes[8] = {0};
    u64 blocks = 0;
    int i;
    if (!psf_get_binary(psf, SAVE_PARAM_SAVEDATA_BLOCKS, &value, &length)) {
        return SAVE_DATA_BLOCKS_MAX;
    }
    /* Little-endian, where it lies */
    memcpy(bytes, value, length < 8 ? length : 8);
    for (i = 7; i >= 0; i--) {
        blocks = (blocks << 8) | bytes[i];
    }
    return blocks;
}

void save_default_param_sfo(psf_t* psf, const char* dir_name, const char* game_serial) {
    const int language = emu_settings_get_console_language();
    const char* title = "Saved Data"; /* en_US when the language has none */
    size_t i;
    for (i = 0; i < sizeof(save_default_titles) / sizeof(save_default_titles[0]); i++) {
        if (save_default_titles[i].language == language) {
            title = save_default_titles[i].title;
            break;
        }
    }
    /* TODO Link with user service */
    psf_set_binary_u64(psf, SAVE_PARAM_ACCOUNT_ID, 0, false);
    psf_set_string(psf, SAVE_PARAM_MAINTITLE, title, strlen(title), false);
    psf_set_string(psf, SAVE_PARAM_SUBTITLE, "", 0, false);
    psf_set_string(psf, SAVE_PARAM_DETAIL, "", 0, false);
    psf_set_string(psf, SAVE_PARAM_SAVEDATA_DIRECTORY, dir_name, strlen(dir_name), false);
    psf_set_integer(psf, SAVE_PARAM_SAVEDATA_LIST_PARAM, 0, false);
    psf_set_string(psf, SAVE_PARAM_TITLE_ID, game_serial, strlen(game_serial), false);
    psf_set_binary_u64(psf, SAVE_PARAM_SAVEDATA_BLOCKS, 0, false);
}

void save_param_from_sfo(OrbisSaveDataParam* param, const psf_t* sfo) {
    const char* value;
    size_t length;
    s32 user_param = 0;
    memset(param, 0, sizeof(*param));
    if (psf_get_string(sfo, SAVE_PARAM_MAINTITLE, &value, &length)) {
        save_copy_text(param->title, sizeof(param->title), value, length);
    } else {
        strlcpy(param->title, "Unknown", sizeof(param->title));
    }
    if (psf_get_string(sfo, SAVE_PARAM_SUBTITLE, &value, &length)) {
        save_copy_text(param->subTitle, sizeof(param->subTitle), value, length);
    }
    if (psf_get_string(sfo, SAVE_PARAM_DETAIL, &value, &length)) {
        save_copy_text(param->detail, sizeof(param->detail), value, length);
    }
    psf_get_integer(sfo, SAVE_PARAM_SAVEDATA_LIST_PARAM, &user_param);
    param->userParam = (u32)user_param;
    param->mtime = sfo->last_write;
}

void save_param_to_sfo(const OrbisSaveDataParam* param, psf_t* sfo) {
    psf_set_string(sfo, SAVE_PARAM_MAINTITLE, param->title,
                   save_strnlen(param->title, sizeof(param->title)), true);
    psf_set_string(sfo, SAVE_PARAM_SUBTITLE, param->subTitle,
                   save_strnlen(param->subTitle, sizeof(param->subTitle)), true);
    psf_set_string(sfo, SAVE_PARAM_DETAIL, param->detail,
                   save_strnlen(param->detail, sizeof(param->detail)), true);
    psf_set_integer(sfo, SAVE_PARAM_SAVEDATA_LIST_PARAM, (s32)param->userParam, true);
}

void save_game_serial(char* out, size_t size) {
    const char* value;
    size_t length;
    if (psf_get_string(psf_game(), "INSTALL_DIR_SAVEDATA", &value, &length)) {
        save_copy_text(out, size, value, length);
    } else {
        strlcpy(out, kernel_game_serial(), size);
    }
}

bool save_write_file(const char* path, const void* data, size_t size) {
    return filestream_write_file_atomic(path, data, (int64_t)size);
}

typedef struct save_icon_write {
    const char* path;
    bool ok;
} save_icon_write_t;

static void save_write_lent(const u8* data, size_t size, void* ctx) {
    save_icon_write_t* w = (save_icon_write_t*)ctx;
    w->ok = save_write_file(w->path, data, size);
}

bool save_write_game_icon(const char* path) {
    save_icon_write_t w;
    w.path = path;
    w.ok = false;
    /* Through the mount stack, so a game in an archive has one too */
    return fs_lend_guest_file("/app0/sce_sys/save_data.png", save_write_lent, &w) && w.ok;
}

void save_show_message(const char* msg) {
    ovl_msg_dialog_param_t p;
    memset(&p, 0, sizeof(p));
    p.mode = OVL_MSG_MODE_USER;
    p.button_type = OVL_MSG_OK;
    p.msg = msg;
    p.button1 = "";
    p.button2 = "";
    ovl_msg_dialog_open(OVL_MSG_DIALOG_EMULATOR, &p);
    ovl_msg_dialog_wait(OVL_MSG_DIALOG_EMULATOR);
}

u64 save_dir_size(const char* dir) {
    u64 total = 0;
    struct RDIR* rdir = retro_opendir_include_hidden(dir, true);
    if (rdir == NULL) {
        return 0;
    }
    while (retro_readdir(rdir)) {
        const char* name = retro_dirent_get_name(rdir);
        char entry[PATH_MAX_LENGTH];
        if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0) {
            continue;
        }
        fill_pathname_join(entry, dir, name, sizeof(entry));
        if (retro_dirent_is_dir(rdir, NULL)) {
            total += save_dir_size(entry);
        } else {
            const int64_t size = path_get_size(entry);
            if (size > 0) {
                total += (u64)size;
            }
        }
    }
    retro_closedir(rdir);
    return total;
}

static void save_sys_path(const save_instance_t* s, const char* name, char* out, size_t size) {
    char sys[PATH_MAX_LENGTH];
    fill_pathname_join(sys, s->save_path, SAVE_SCE_SYS, sizeof(sys));
    fill_pathname_join(out, sys, name, size);
}

void save_instance_icon_path(const save_instance_t* s, char* out, size_t size) {
    save_sys_path(s, "icon0.png", out, size);
}

void save_instance_init(save_instance_t* s, int slot_num, s32 user_id, const char* game_serial,
                        const char* dir_name, int max_blocks) {
    char sfo_path[PATH_MAX_LENGTH];
    C_ASSERT_MSG(slot_num >= 0 && slot_num < 16, "save_instance_init",
                 "slot_num >= 0 && slot_num < 16");
    s->slot_num = slot_num;
    s->user_id = user_id;
    strlcpy(s->game_serial, game_serial, sizeof(s->game_serial));
    strlcpy(s->dir_name, dir_name, sizeof(s->dir_name));
    if (max_blocks < (int)SAVE_DATA_BLOCKS_MIN2) {
        max_blocks = (int)SAVE_DATA_BLOCKS_MIN2;
    } else if (max_blocks > (int)SAVE_DATA_BLOCKS_MAX) {
        max_blocks = (int)SAVE_DATA_BLOCKS_MAX;
    }
    s->max_blocks = max_blocks;
    save_make_dir_path(s->save_path, sizeof(s->save_path), user_id, s->game_serial, s->dir_name);
    snprintf(s->mount_point, sizeof(s->mount_point), "/savedata%d", slot_num);
    psf_init(&s->param_sfo);
    save_sys_path(s, "param.sfo", sfo_path, sizeof(sfo_path));
    s->exists = path_is_valid(sfo_path);
    s->mounted = fs_is_mounted(s->mount_point);
    s->read_only = false;
}

void save_instance_free(save_instance_t* s) {
    if (s->mounted) {
        save_instance_umount(s);
    }
    psf_free(&s->param_sfo);
}

/* The save's folders and its param.sfo */
static bool save_instance_create_files(save_instance_t* s) {
    char sys[PATH_MAX_LENGTH];
    char sfo_path[PATH_MAX_LENGTH];
    fill_pathname_join(sys, s->save_path, SAVE_SCE_SYS, sizeof(sys));
    path_mkdir(sys);
    save_default_param_sfo(&s->param_sfo, s->dir_name, s->game_serial);
    psf_set_binary_u64(&s->param_sfo, SAVE_PARAM_SAVEDATA_BLOCKS, (u64)s->max_blocks, true);
    save_sys_path(s, "param.sfo", sfo_path, sizeof(sfo_path));
    if (!psf_encode_path(&s->param_sfo, sfo_path)) {
        c_log(C_LOG_LIB_SAVE_DATA, C_LOG_LEVEL_ERROR, "save_instance_create_files",
              "Failed to write param.sfo at %s", sfo_path);
        return false;
    }
    return true;
}

int save_instance_setup_and_mount(save_instance_t* s, bool read_only, bool copy_icon,
                                  bool ignore_corrupt, bool dont_restore_backup) {
    char sfo_path[PATH_MAX_LENGTH];
    char corrupt_path[PATH_MAX_LENGTH];
    if (s->mounted) {
        c_unreachable(__FILE__, __LINE__, "save_instance_setup_and_mount",
                      "Save instance is already mounted");
    }
    save_sys_path(s, "param.sfo", sfo_path, sizeof(sfo_path));
    save_sys_path(s, "corrupted", corrupt_path, sizeof(corrupt_path));

    s->exists = path_is_valid(sfo_path); /* again, in case */
    if (!s->exists) {
        if (!save_instance_create_files(s)) {
            return SAVE_SETUP_FAILED;
        }
        if (copy_icon) {
            char icon[PATH_MAX_LENGTH];
            save_instance_icon_path(s, icon, sizeof(icon));
            if (path_is_valid(icon)) {
                filestream_delete(icon);
            }
            save_write_game_icon(icon);
        }
        s->exists = true;
    } else {
        bool broken = false;
        if (!ignore_corrupt && path_is_valid(corrupt_path)) {
            c_log(C_LOG_LIB_SAVE_DATA, C_LOG_LEVEL_ERROR, "save_instance_setup_and_mount",
                  "Corrupted save data: %s", corrupt_path);
            broken = true;
        } else if (!psf_open_path(&s->param_sfo, sfo_path)) {
            c_log(C_LOG_LIB_SAVE_DATA, C_LOG_LEVEL_ERROR, "save_instance_setup_and_mount",
                  "Failed to read param.sfo: %s", sfo_path);
            broken = true;
        }
        if (broken) {
            if (dont_restore_backup) {
                return SAVE_SETUP_BROKEN;
            }
            if (save_backup_restore(s->save_path)) {
                return save_instance_setup_and_mount(s, read_only, copy_icon, ignore_corrupt, true);
            }
        }
    }

    if (!ignore_corrupt && !read_only) {
        /* Marks it as in use until it is unmounted */
        RFILE* f = filestream_open(corrupt_path, RETRO_VFS_FILE_ACCESS_WRITE,
                                   RETRO_VFS_FILE_ACCESS_HINT_NONE);
        if (f != NULL) {
            filestream_close(f);
        }
    }

    s->max_blocks = (int)save_max_blocks_from_sfo(&s->param_sfo);

    fs_mount(s->save_path, s->mount_point, read_only);
    s->mounted = true;
    s->read_only = read_only;
    return SAVE_SETUP_OK;
}

bool save_instance_umount(save_instance_t* s) {
    char sfo_path[PATH_MAX_LENGTH];
    char corrupt_path[PATH_MAX_LENGTH];
    bool ok;
    if (!s->mounted) {
        c_unreachable(__FILE__, __LINE__, "save_instance_umount", "Save instance is not mounted");
        return false;
    }
    s->mounted = false;
    save_sys_path(s, "param.sfo", sfo_path, sizeof(sfo_path));
    save_sys_path(s, "corrupted", corrupt_path, sizeof(corrupt_path));
    ok = psf_encode_path(&s->param_sfo, sfo_path);
    if (!ok) {
        c_log(C_LOG_LIB_SAVE_DATA, C_LOG_LEVEL_ERROR, "save_instance_umount",
              "Failed to write param.sfo at %s", sfo_path);
    }
    psf_free(&s->param_sfo);
    filestream_delete(corrupt_path);
    fs_unmount(s->mount_point);
    return ok;
}
