/* SPDX-FileCopyrightText: Copyright 2024-2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

/* What the save data dialog shows: the saves read when it opens, and its messages. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <compat/strl.h>
#include <file/file_path.h>

#include "common/c_bridge.h"
#include "common/enum_names.h"
#include "core/file_sys/fs_c.h"
#include "core/libraries/kernel/kernel_c.h"
#include "core/libraries/save_data/dialog/savedatadialog_ui.h"
#include "video_core/overlay/ovl.h"

#define SAVE_DIALOG_FW_450 0x4500000u

static size_t sdd_strnlen(const char* s, size_t max) {
    const char* end = (const char*)memchr(s, 0, max);
    return end != NULL ? (size_t)(end - s) : max;
}

/* A fixed field's text into @out, terminated, cut to fit */
static void sdd_get(char* out, size_t out_size, const char* field, size_t field_size) {
    size_t n = sdd_strnlen(field, field_size);
    if (n > out_size - 1) {
        n = out_size - 1;
    }
    memcpy(out, field, n);
    out[n] = '\0';
}

/* @length bytes into @out, terminated, cut to fit */
static void sdd_put(char* out, size_t size, const char* text, size_t length) {
    const size_t n = length < size - 1 ? length : size - 1;
    memcpy(out, text, n);
    out[n] = '\0';
}

static void sdd_space_size(char* out, size_t out_size, u64 size) {
    if (size > 1024ull * 1024 * 1024) { /* > 1 GB */
        snprintf(out, out_size, "%.2f GB", (double)(size / 1024 / 1024) / 1024.0f);
    } else if (size > 1024 * 1024) { /* > 1 MB */
        snprintf(out, out_size, "%.2f MB", (double)(size / 1024) / 1024.0f);
    } else if (size > 1024) { /* > 1 KB */
        snprintf(out, out_size, "%.2f KB", (double)size / 1024.0f);
    } else {
        snprintf(out, out_size, "%llu B", (unsigned long long)size);
    }
}

static void sdd_decode_lent(const u8* data, size_t size, void* ctx) {
    *(int*)ctx = ovl_tex_decode_icon(data, size);
}

/* A save's icon, decoded into the overlay's icon atlas from the file mapped where it lies;
 * here, on the game's thread while the dialog opens, as the rest of what it shows is read */
static int sdd_decode_icon(const char* path) {
    int icon = 0;
    fs_lend_host_file(path, sdd_decode_lent, &icon);
    return icon;
}

/* A new item in the save list, its room grown as needed: NULL if there is none */
static save_dialog_item_t* sdd_push_item(save_dialog_state_t* s) {
    if (s->save_count == s->save_room) {
        const size_t want = s->save_room != 0 ? s->save_room * 2 : 8;
        save_dialog_item_t* grown =
            (save_dialog_item_t*)realloc(s->save_list, want * sizeof(*grown));
        if (grown == NULL) {
            return NULL;
        }
        s->save_list = grown;
        s->save_room = want;
    }
    memset(&s->save_list[s->save_count], 0, sizeof(s->save_list[0]));
    return &s->save_list[s->save_count++];
}

static void sdd_read_save(save_dialog_state_t* s, const char* dir_name) {
    char dir_path[PATH_MAX_LENGTH];
    char sfo_path[PATH_MAX_LENGTH];
    char icon_path[PATH_MAX_LENGTH];
    char corrupt_path[PATH_MAX_LENGTH];
    save_dialog_item_t* item;
    psf_t sfo;
    const char* value;
    size_t length;
    time_t last_write;
    struct tm* tm;

    save_make_dir_path(dir_path, sizeof(dir_path), s->user_id, s->title_id, dir_name);
    save_join(sfo_path, sizeof(sfo_path), dir_path, "sce_sys/param.sfo");
    if (!path_is_valid(sfo_path)) {
        return;
    }
    item = sdd_push_item(s);
    if (item == NULL) {
        return;
    }
    psf_init(&sfo);
    psf_open_path(&sfo, sfo_path);

    strlcpy(item->dir_name, dir_name, sizeof(item->dir_name));
    if (psf_get_string(&sfo, SAVE_PARAM_MAINTITLE, &value, &length)) {
        sdd_put(item->title, sizeof(item->title), value, length);
        item->has_param = true;
    } else {
        strlcpy(item->title, "Unknown", sizeof(item->title));
    }
    if (psf_get_string(&sfo, SAVE_PARAM_SUBTITLE, &value, &length)) {
        sdd_put(item->subtitle, sizeof(item->subtitle), value, length);
    }
    save_param_from_sfo(&item->param, &sfo);
    item->last_write = sfo.last_write;
    psf_free(&sfo);

    last_write = (time_t)item->last_write;
    tm = localtime(&last_write);
    if (tm == NULL || strftime(item->date, sizeof(item->date), "%d %b, %Y %H:%M", tm) == 0) {
        item->date[0] = '\0';
    }
    sdd_space_size(item->size, sizeof(item->size), save_dir_size(dir_path));

    save_join(icon_path, sizeof(icon_path), dir_path, "sce_sys/icon0.png");
    if (path_is_valid(icon_path)) {
        item->icon = sdd_decode_icon(icon_path);
    }
    save_join(corrupt_path, sizeof(corrupt_path), dir_path, "sce_sys/corrupted");
    item->is_corrupted = path_is_valid(corrupt_path);
}

/* The text for a save, a load or a delete */
static const char* sdd_by_type(u32 type, const char* save, const char* load, const char* del) {
    switch (type) {
    case SAVE_DATA_DIALOG_TYPE_SAVE:
        return save;
    case SAVE_DATA_DIALOG_TYPE_LOAD:
        return load;
    case SAVE_DATA_DIALOG_TYPE_DELETE:
        return del;
    default:
        c_unreachable(__FILE__, __LINE__, "save_dialog_state_build", "dialog type");
        return "";
    }
}

static void sdd_system_state(save_dialog_state_t* s, const OrbisSaveDataDialogParam* param) {
    const SaveDataDialogSystemMessageParam* sys = param->sysMsgParam;
    const u32 type = param->dispType;
    char space[32];
    s->state = SAVE_DIALOG_STATE_SYSTEM;
    if (sys == NULL) {
        return;
    }
    switch (sys->msgType) {
    case SAVE_DATA_SYSTEM_MESSAGE_NODATA:
        s->return_cancel = true;
        strlcpy(s->msg, "There is no saved data", sizeof(s->msg));
        break;
    case SAVE_DATA_SYSTEM_MESSAGE_CONFIRM:
        s->show_no = true;
        strlcpy(s->msg,
                sdd_by_type(type, "Do you want to save?", "Do you want to load this saved data?",
                            "Do you want to delete this saved data?"),
                sizeof(s->msg));
        break;
    case SAVE_DATA_SYSTEM_MESSAGE_OVERWRITE:
        s->show_no = true;
        strlcpy(s->msg,
                sdd_by_type(type, "Do you want to overwrite the existing saved data?",
                            "##UNKNOWN##", "##UNKNOWN##"),
                sizeof(s->msg));
        break;
    case SAVE_DATA_SYSTEM_MESSAGE_NOSPACE: {
        char text[256];
        s->return_cancel = true;
        sdd_space_size(space, sizeof(space), sys->value * SAVE_DATA_BLOCK_SIZE);
        snprintf(text, sizeof(text),
                 "There is not enough space to save the data. To continue %s free space is "
                 "required.",
                 space);
        strlcpy(s->msg, sdd_by_type(type, text, "##UNKNOWN##", "##UNKNOWN##"), sizeof(s->msg));
    } break;
    case SAVE_DATA_SYSTEM_MESSAGE_PROGRESS:
        s->hide_ok = true;
        s->show_cancel = s->enable_back == 1;
        strlcpy(s->msg, sdd_by_type(type, "Saving...", "Loading...", "Deleting..."),
                sizeof(s->msg));
        break;
    case SAVE_DATA_SYSTEM_MESSAGE_FILE_CORRUPTED:
        s->return_cancel = true;
        strlcpy(s->msg, "The saved data is corrupted.", sizeof(s->msg));
        break;
    case SAVE_DATA_SYSTEM_MESSAGE_FINISHED:
        s->return_cancel = true;
        strlcpy(s->msg,
                sdd_by_type(type, "Saved successfully.", "Loading complete.", "Deletion complete."),
                sizeof(s->msg));
        break;
    case SAVE_DATA_SYSTEM_MESSAGE_NOSPACE_CONTINUABLE: {
        char text[256];
        s->return_cancel = true;
        sdd_space_size(space, sizeof(space), sys->value * SAVE_DATA_BLOCK_SIZE);
        snprintf(text, sizeof(text),
                 "There is not enough space to save the data. %s free space is required.", space);
        strlcpy(s->msg, sdd_by_type(type, text, "##UNKNOWN##", "##UNKNOWN##"), sizeof(s->msg));
    } break;
    case SAVE_DATA_SYSTEM_MESSAGE_CORRUPTED_AND_DELETED: {
        const char* msg1 = "The saved data is corrupted and will be deleted.";
        s->show_cancel = s->enable_back != 0; /* true when the game gave no option */
        strlcpy(s->msg, sdd_by_type(type, msg1, msg1, "##UNKNOWN##"), sizeof(s->msg));
    } break;
    case SAVE_DATA_SYSTEM_MESSAGE_CORRUPTED_AND_CREATED: {
        const char* msg2 = "The saved data is corrupted. This saved data will be deleted and a "
                           "new one will be created.";
        s->show_cancel = s->enable_back != 0;
        strlcpy(s->msg, sdd_by_type(type, msg2, msg2, "##UNKNOWN##"), sizeof(s->msg));
    } break;
    case SAVE_DATA_SYSTEM_MESSAGE_CORRUPTED_AND_RESTORE: {
        const char* msg3 = "The saved data is corrupted. The data that was backed up by the "
                           "system will be restored.";
        s->show_cancel = s->enable_back != 0;
        strlcpy(s->msg, sdd_by_type(type, msg3, msg3, "##UNKNOWN##"), sizeof(s->msg));
    } break;
    case SAVE_DATA_SYSTEM_MESSAGE_TOTAL_SIZE_EXCEEDED:
        strlcpy(s->msg,
                sdd_by_type(type, "Cannot create more saved data", "##UNKNOWN##", "##UNKNOWN##"),
                sizeof(s->msg));
        break;
    default:
        snprintf(s->msg, sizeof(s->msg), "Unknown message type: %s",
                 enum_name(&enum_names_SaveDataSystemMessageType, (s64)sys->msgType));
        break;
    }
}

static void sdd_error_state(save_dialog_state_t* s, const OrbisSaveDataDialogParam* param) {
    const SaveDataDialogErrorCodeParam* err = param->errorCodeParam;
    s->state = SAVE_DIALOG_STATE_ERROR_CODE;
    if (err == NULL) {
        return;
    }
    switch (err->errorCode) {
    case SAVE_DATA_ERROR_NOT_FOUND:
        strlcpy(s->msg, "There is not saved data.", sizeof(s->msg));
        break;
    case SAVE_DATA_ERROR_BROKEN:
        strlcpy(s->msg, "The data is corrupted.", sizeof(s->msg));
        break;
    default:
        snprintf(s->msg, sizeof(s->msg), "An error has occurred. (%X)", (unsigned)err->errorCode);
        break;
    }
}

static void sdd_progress_state(save_dialog_state_t* s, const OrbisSaveDataDialogParam* param) {
    const SaveDataDialogProgressBarParam* bar = param->progressBarParam;
    s->state = SAVE_DIALOG_STATE_PROGRESS_BAR;
    if (bar == NULL) {
        return;
    }
    if (bar->msg != NULL) {
        strlcpy(s->msg, bar->msg, sizeof(s->msg));
        return;
    }
    switch (bar->sysMsgType) {
    case SAVE_DATA_DIALOG_PROGRESS_PROGRESS:
        switch (s->type) {
        case SAVE_DATA_DIALOG_TYPE_SAVE:
            strlcpy(s->msg, "Saving...", sizeof(s->msg));
            break;
        case SAVE_DATA_DIALOG_TYPE_LOAD:
            strlcpy(s->msg, "Loading...", sizeof(s->msg));
            break;
        case SAVE_DATA_DIALOG_TYPE_DELETE:
            strlcpy(s->msg, "Deleting...", sizeof(s->msg));
            break;
        default:
            break;
        }
        break;
    case SAVE_DATA_DIALOG_PROGRESS_RESTORE:
        strlcpy(s->msg, "Restoring saved data...", sizeof(s->msg));
        break;
    default:
        break;
    }
}

typedef struct sdd_lent_icon {
    int icon;
} sdd_lent_icon_t;

static void sdd_decode_game_icon(const u8* data, size_t size, void* ctx) {
    ((sdd_lent_icon_t*)ctx)->icon = ovl_tex_decode_icon(data, size);
}

void save_dialog_state_build(save_dialog_state_t* s, const OrbisSaveDataDialogParam* param) {
    const SaveDataDialogItems* item = param->items;
    char game_serial[SAVE_NAME_SIZE];

    /* What it held goes; the room for its saves and view stays */
    s->mode = param->mode;
    s->type = param->dispType;
    s->user_data = param->userData;
    s->enable_back = param->optionParam != NULL
                         ? (param->optionParam->back == SAVE_DATA_DIALOG_BACK_ENABLE ? 1 : 0)
                         : -1;
    s->user_id = 0;
    s->title_id[0] = '\0';
    s->save_count = 0;
    s->focus_kind = SAVE_DIALOG_FOCUS_NONE;
    s->focus_pos = 0;
    s->focus_dir[0] = '\0';
    s->style = 0;
    s->has_new_item = false;
    memset(&s->new_item, 0, sizeof(s->new_item));
    s->state = SAVE_DIALOG_STATE_NONE;
    s->msg[0] = '\0';
    s->user_type = 0;
    s->user_msg_type = 0;
    s->hide_ok = s->show_no = s->show_cancel = s->return_cancel = false;

    save_game_serial(game_serial, sizeof(game_serial));

    if (item != NULL) {
        u32 i;
        s->user_id = item->userId;
        if (item->titleId == NULL) {
            strlcpy(s->title_id, game_serial, sizeof(s->title_id));
        } else {
            sdd_get(s->title_id, sizeof(s->title_id), item->titleId->data,
                    sizeof(item->titleId->data));
        }
        if (item->dirName != NULL) {
            for (i = 0; i < item->dirNameNum; i++) {
                char dir_name[SAVE_NAME_SIZE];
                sdd_get(dir_name, sizeof(dir_name), item->dirName[i].data,
                        sizeof(item->dirName[i].data));
                if (dir_name[0] != '\0') {
                    sdd_read_save(s, dir_name);
                }
            }
        }

        if (s->type == SAVE_DATA_DIALOG_TYPE_SAVE && item->newItem != NULL) {
            const SaveDataDialogNewItem* new_item = item->newItem;
            int icon = 0;
            if (new_item->iconBuf != NULL && new_item->iconSize != 0) {
                /* Decoded where the game keeps it */
                icon = ovl_tex_decode_icon((const u8*)new_item->iconBuf, new_item->iconSize);
            } else {
                sdd_lent_icon_t lent;
                lent.icon = 0;
                fs_lend_guest_file("/app0/sce_sys/save_data.png", sdd_decode_game_icon, &lent);
                icon = lent.icon;
            }
            s->has_new_item = true;
            s->new_item.icon = icon;
            strlcpy(s->new_item.title, new_item->title != NULL ? new_item->title : "New Save",
                    sizeof(s->new_item.title));
        }

        if (item->focusPos != SAVE_DATA_DIALOG_FOCUS_DIRNAME) {
            s->focus_kind = SAVE_DIALOG_FOCUS_POS;
            s->focus_pos = item->focusPos;
        } else if (item->focusPosDirName != NULL) {
            s->focus_kind = SAVE_DIALOG_FOCUS_DIR;
            sdd_get(s->focus_dir, sizeof(s->focus_dir), item->focusPosDirName->data,
                    sizeof(item->focusPosDirName->data));
        }
        s->style = item->itemStyle;
    }

    switch (s->mode) {
    case SAVE_DATA_DIALOG_MODE_USER_MSG:
        s->state = SAVE_DIALOG_STATE_USER;
        if (param->userMsgParam != NULL) {
            s->user_type = param->userMsgParam->buttonType;
            s->user_msg_type = param->userMsgParam->msgType;
            if (param->userMsgParam->msg != NULL) {
                strlcpy(s->msg, param->userMsgParam->msg, sizeof(s->msg));
            }
        }
        break;
    case SAVE_DATA_DIALOG_MODE_SYSTEM_MSG:
        sdd_system_state(s, param);
        break;
    case SAVE_DATA_DIALOG_MODE_ERROR_CODE:
        sdd_error_state(s, param);
        break;
    case SAVE_DATA_DIALOG_MODE_PROGRESS_BAR:
        sdd_progress_state(s, param);
        break;
    default:
        break;
    }
}

/* The item focused when the list appears, -1 for none */
static int sdd_focus_index(const save_dialog_state_t* s) {
    const int count = (s->has_new_item ? 1 : 0) + (int)s->save_count;
    size_t i;
    if (s->focus_kind == SAVE_DIALOG_FOCUS_POS) {
        int idx = 0, max_idx = 0;
        bool is_min;
        s64 max_write = 0;
        if (s->focus_pos == SAVE_DATA_DIALOG_FOCUS_LISTHEAD ||
            s->focus_pos == SAVE_DATA_DIALOG_FOCUS_DATAHEAD) {
            return 0;
        }
        if (s->focus_pos == SAVE_DATA_DIALOG_FOCUS_LISTTAIL ||
            s->focus_pos == SAVE_DATA_DIALOG_FOCUS_DATATAIL) {
            return count - 1 > 0 ? count - 1 : 0;
        }
        /* By date, as the ImGui dialog chose it */
        is_min = s->focus_pos == SAVE_DATA_DIALOG_FOCUS_DATAOLDEST;
        if (s->has_new_item) {
            idx++;
        }
        for (i = 0; i < s->save_count; i++) {
            if ((s->save_list[i].last_write > max_write) ^ is_min) {
                max_write = s->save_list[i].last_write;
                max_idx = idx;
            }
            idx++;
        }
        return max_idx;
    }
    if (s->focus_kind == SAVE_DIALOG_FOCUS_DIR) {
        int idx = 0, found = -1;
        if (s->focus_dir[0] == '\0') {
            return 0;
        }
        if (s->has_new_item) {
            if (strcmp(s->focus_dir, s->new_item.dir_name) == 0) {
                found = idx;
            }
            idx++;
        }
        for (i = 0; i < s->save_count; i++) {
            if (strcmp(s->save_list[i].dir_name, s->focus_dir) == 0) {
                found = idx;
                break;
            }
            idx++;
        }
        return found;
    }
    return -1;
}

static void sdd_view(ovl_save_item_t* v, const save_dialog_item_t* item) {
    v->title = item->title;
    v->subtitle = item->subtitle;
    v->date = item->date;
    v->size = item->size;
    v->icon = item->icon;
    v->corrupted = item->is_corrupted ? 1 : 0;
}

ovl_save_dialog_param_t save_dialog_state_make_param(save_dialog_state_t* s) {
    ovl_save_dialog_param_t p;
    const size_t count = s->save_count + 1;
    size_t n = 0, i;
    memset(&p, 0, sizeof(p));
    if (s->view_room < count) {
        ovl_save_item_t* grown = (ovl_save_item_t*)realloc(s->view, count * sizeof(*grown));
        if (grown != NULL) {
            s->view = grown;
            s->view_room = count;
        }
    }
    if (s->view != NULL && s->view_room >= count) {
        if (s->has_new_item) {
            sdd_view(&s->view[n++], &s->new_item);
        }
        for (i = 0; i < s->save_count; i++) {
            sdd_view(&s->view[n++], &s->save_list[i]);
        }
    }

    p.mode = (int)s->mode;
    p.type = (int)s->type;
    p.style = (int)s->style;
    p.enable_back = s->enable_back;
    p.items = s->view;
    p.item_count = (int)n;
    p.has_new_item = s->has_new_item ? 1 : 0;
    p.focus = sdd_focus_index(s);
    p.old_firmware = kernel_firmware_version() < SAVE_DIALOG_FW_450 ? 1 : 0;
    switch (s->state) {
    case SAVE_DIALOG_STATE_USER:
        p.msg = s->msg;
        p.user_buttons = (int)s->user_type;
        p.user_error = s->user_msg_type == SAVE_DATA_DIALOG_USER_MSG_ERROR ? 1 : 0;
        break;
    case SAVE_DIALOG_STATE_SYSTEM:
        p.msg = s->msg;
        p.hide_ok = s->hide_ok;
        p.show_no = s->show_no;
        p.show_cancel = s->show_cancel;
        p.return_cancel = s->return_cancel;
        break;
    case SAVE_DIALOG_STATE_ERROR_CODE:
    case SAVE_DIALOG_STATE_PROGRESS_BAR:
        p.msg = s->msg;
        break;
    default:
        break;
    }
    return p;
}

void save_dialog_state_make_result(const save_dialog_state_t* s, save_dialog_result_t* out,
                                   int button, int result, int item) {
    const int first_save = s->has_new_item ? 1 : 0;
    out->mode = s->mode;
    out->result = (u32)result;
    out->button_id = (u32)button;
    out->user_data = s->user_data;
    out->dir_name[0] = '\0';
    out->has_param = false;
    if (item >= 0) {
        /* A save chosen from the list */
        const save_dialog_item_t* chosen = NULL;
        if (item < first_save) {
            chosen = &s->new_item;
        } else if ((size_t)(item - first_save) < s->save_count) {
            chosen = &s->save_list[item - first_save];
        }
        if (chosen != NULL) {
            strlcpy(out->dir_name, chosen->dir_name, sizeof(out->dir_name));
            out->has_param = chosen->has_param;
            out->param = chosen->param;
        }
    } else if (s->mode != SAVE_DATA_DIALOG_MODE_LIST && s->save_count != 0) {
        /* A message is about its first save */
        strlcpy(out->dir_name, s->save_list[0].dir_name, sizeof(out->dir_name));
    }
}

void save_dialog_state_release_icons(save_dialog_state_t* s) {
    size_t i;
    for (i = 0; i < s->save_count; i++) {
        if (s->save_list[i].icon != 0) {
            ovl_tex_release(s->save_list[i].icon);
            s->save_list[i].icon = 0;
        }
    }
    if (s->has_new_item && s->new_item.icon != 0) {
        ovl_tex_release(s->new_item.icon);
        s->new_item.icon = 0;
    }
}

void save_dialog_result_copy_to(const save_dialog_result_t* r, OrbisSaveDataDialogResult* out) {
    out->mode = r->mode;
    out->result = r->result;
    out->buttonId = r->button_id;
    if (out->dirName != NULL) {
        sdd_put(out->dirName->data, sizeof(out->dirName->data), r->dir_name, strlen(r->dir_name));
    }
    if (out->param != NULL && r->has_param) {
        *out->param = r->param;
    }
    out->userData = r->user_data;
}
