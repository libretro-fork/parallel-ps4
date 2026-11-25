/* SPDX-FileCopyrightText: Copyright 2024-2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef SHADPS4_LIBRARIES_SAVE_DATA_DIALOG_SAVEDATADIALOG_UI_H
#define SHADPS4_LIBRARIES_SAVE_DATA_DIALOG_SAVEDATADIALOG_UI_H

/* What the save data dialog shows (savedatadialog_ui.c): read from the game's saves when it
 * opens, and drawn by the overlay (video_core/overlay/ovl_savedialog.c), which it lends its
 * texts and the saves' icons. */

#include <stddef.h>

#include <boolean.h>
#include <retro_common_api.h>

#include "common/types.h"
#include "core/libraries/save_data/save_instance.h"
#include "core/libraries/save_data/savedata.h"
#include "core/libraries/system/commondialog_c.h"
#include "video_core/overlay/ovl_layers.h"

RETRO_BEGIN_DECLS

typedef enum SaveDataDialogMode {
    SAVE_DATA_DIALOG_MODE_INVALID = 0,
    SAVE_DATA_DIALOG_MODE_LIST = 1,
    SAVE_DATA_DIALOG_MODE_USER_MSG = 2,
    SAVE_DATA_DIALOG_MODE_SYSTEM_MSG = 3,
    SAVE_DATA_DIALOG_MODE_ERROR_CODE = 4,
    SAVE_DATA_DIALOG_MODE_PROGRESS_BAR = 5
} SaveDataDialogMode;

/* DialogType */
#define SAVE_DATA_DIALOG_TYPE_SAVE 1u
#define SAVE_DATA_DIALOG_TYPE_LOAD 2u
#define SAVE_DATA_DIALOG_TYPE_DELETE 3u

/* ButtonId */
#define SAVE_DATA_DIALOG_BUTTON_INVALID 0u
#define SAVE_DATA_DIALOG_BUTTON_OK 1u
#define SAVE_DATA_DIALOG_BUTTON_YES 1u
#define SAVE_DATA_DIALOG_BUTTON_NO 2u

/* UserMessageType */
#define SAVE_DATA_DIALOG_USER_MSG_NORMAL 0u
#define SAVE_DATA_DIALOG_USER_MSG_ERROR 1u

/* FocusPos */
#define SAVE_DATA_DIALOG_FOCUS_LISTHEAD 0u
#define SAVE_DATA_DIALOG_FOCUS_LISTTAIL 1u
#define SAVE_DATA_DIALOG_FOCUS_DATAHEAD 2u
#define SAVE_DATA_DIALOG_FOCUS_DATATAIL 3u
#define SAVE_DATA_DIALOG_FOCUS_DATALATEST 4u
#define SAVE_DATA_DIALOG_FOCUS_DATAOLDEST 5u
#define SAVE_DATA_DIALOG_FOCUS_DIRNAME 6u

typedef enum SaveDataSystemMessageType {
    SAVE_DATA_SYSTEM_MESSAGE_NODATA = 1,
    SAVE_DATA_SYSTEM_MESSAGE_CONFIRM = 2,
    SAVE_DATA_SYSTEM_MESSAGE_OVERWRITE = 3,
    SAVE_DATA_SYSTEM_MESSAGE_NOSPACE = 4,
    SAVE_DATA_SYSTEM_MESSAGE_PROGRESS = 5,
    SAVE_DATA_SYSTEM_MESSAGE_FILE_CORRUPTED = 6,
    SAVE_DATA_SYSTEM_MESSAGE_FINISHED = 7,
    SAVE_DATA_SYSTEM_MESSAGE_NOSPACE_CONTINUABLE = 8,
    SAVE_DATA_SYSTEM_MESSAGE_CORRUPTED_AND_DELETED = 10,
    SAVE_DATA_SYSTEM_MESSAGE_CORRUPTED_AND_CREATED = 11,
    SAVE_DATA_SYSTEM_MESSAGE_CORRUPTED_AND_RESTORE = 13,
    SAVE_DATA_SYSTEM_MESSAGE_TOTAL_SIZE_EXCEEDED = 14
} SaveDataSystemMessageType;

/* ProgressSystemMessageType */
#define SAVE_DATA_DIALOG_PROGRESS_INVALID 0u
#define SAVE_DATA_DIALOG_PROGRESS_PROGRESS 1u
#define SAVE_DATA_DIALOG_PROGRESS_RESTORE 2u

/* OptionBack */
#define SAVE_DATA_DIALOG_BACK_ENABLE 0u
#define SAVE_DATA_DIALOG_BACK_DISABLE 1u

/* OrbisSaveDataDialogProgressBarTarget */
#define SAVE_DATA_DIALOG_PROGRESS_BAR_TARGET_DEFAULT 0u

typedef struct SaveDataDialogAnimationParam {
    u32 userOK;
    u32 userCancel;
    u8 _reserved[32];
} SaveDataDialogAnimationParam;

typedef struct SaveDataDialogNewItem {
    const char* title;
    void* iconBuf;
    size_t iconSize;
    u8 _reserved[32];
} SaveDataDialogNewItem;

typedef struct SaveDataDialogItems {
    s32 userId;
    u8 _pad[4];
    const OrbisSaveDataTitleId* titleId;
    const OrbisSaveDataDirName* dirName;
    u32 dirNameNum;
    u8 _pad2[4];
    const SaveDataDialogNewItem* newItem;
    u32 focusPos;
    u8 _pad3[4];
    const OrbisSaveDataDirName* focusPosDirName;
    u32 itemStyle;
    u8 _reserved[32];
} SaveDataDialogItems;

typedef struct SaveDataDialogUserMessageParam {
    u32 buttonType;
    u32 msgType;
    const char* msg;
    u8 _reserved[32];
} SaveDataDialogUserMessageParam;

typedef struct SaveDataDialogSystemMessageParam {
    u32 msgType;
    u8 _pad[4];
    u64 value;
    u8 _reserved[32];
} SaveDataDialogSystemMessageParam;

typedef struct SaveDataDialogErrorCodeParam {
    u32 errorCode;
    u8 _reserved[32];
} SaveDataDialogErrorCodeParam;

typedef struct SaveDataDialogProgressBarParam {
    u32 barType;
    u8 _pad[4];
    const char* msg;
    u32 sysMsgType;
    u8 _reserved[28];
} SaveDataDialogProgressBarParam;

typedef struct SaveDataDialogOptionParam {
    u32 back;
    u8 _reserved[32];
} SaveDataDialogOptionParam;

typedef struct OrbisSaveDataDialogParam {
    common_dialog_base_param baseParam;
    s32 size;
    u32 mode;
    u32 dispType;
    u8 _pad[4];
    SaveDataDialogAnimationParam* animParam;
    SaveDataDialogItems* items;
    SaveDataDialogUserMessageParam* userMsgParam;
    SaveDataDialogSystemMessageParam* sysMsgParam;
    SaveDataDialogErrorCodeParam* errorCodeParam;
    SaveDataDialogProgressBarParam* progressBarParam;
    void* userData;
    SaveDataDialogOptionParam* optionParam;
    u8 _reserved[24];
} OrbisSaveDataDialogParam;

typedef struct OrbisSaveDataDialogResult {
    u32 mode;
    u32 result;
    u32 buttonId;
    u8 _pad[4];
    OrbisSaveDataDirName* dirName;
    OrbisSaveDataParam* param;
    void* userData;
    u8 _reserved[32];
} OrbisSaveDataDialogResult;

/* A dialog's result, kept until the game asks for it */
typedef struct save_dialog_result {
    u32 mode;
    u32 result; /* COMMON_DIALOG_RESULT_* */
    u32 button_id;
    char dir_name[SAVE_NAME_SIZE];
    bool has_param; /* the save chosen has a title */
    OrbisSaveDataParam param;
    void* user_data;
} save_dialog_result_t;

/* A save the dialog shows */
typedef struct save_dialog_item {
    char dir_name[SAVE_NAME_SIZE];
    int icon; /* the overlay's image (ovl_tex_decode_icon), 0 for none */
    char title[256];
    char subtitle[256];
    char date[64];
    char size[32];
    s64 last_write;
    bool has_param; /* its param.sfo has a title: param is what the game is given */
    OrbisSaveDataParam param;
    bool is_corrupted;
} save_dialog_item_t;

/* What a message shows */
enum {
    SAVE_DIALOG_STATE_NONE = 0,
    SAVE_DIALOG_STATE_USER,
    SAVE_DIALOG_STATE_SYSTEM,
    SAVE_DIALOG_STATE_ERROR_CODE,
    SAVE_DIALOG_STATE_PROGRESS_BAR
};

/* Where the focus starts */
enum { SAVE_DIALOG_FOCUS_NONE = 0, SAVE_DIALOG_FOCUS_POS, SAVE_DIALOG_FOCUS_DIR };

typedef struct save_dialog_state {
    u32 mode; /* SaveDataDialogMode */
    u32 type; /* SAVE_DATA_DIALOG_TYPE_* */
    void* user_data;
    int enable_back; /* the game's option: 1, 0, or -1 when it gave none */

    s32 user_id;
    char title_id[SAVE_NAME_SIZE];
    /* The saves: kept, and their room, from one dialog to the next */
    save_dialog_item_t* save_list;
    size_t save_count;
    size_t save_room;
    int focus_kind; /* SAVE_DIALOG_FOCUS_* */
    u32 focus_pos;
    char focus_dir[SAVE_NAME_SIZE];
    u32 style;

    bool has_new_item;
    save_dialog_item_t new_item;

    int state; /* SAVE_DIALOG_STATE_* */
    char msg[2048];
    u32 user_type, user_msg_type;                      /* a user message's */
    bool hide_ok, show_no, show_cancel, return_cancel; /* a system message's */

    /* What the overlay draws from, lending the strings above */
    ovl_save_item_t* view;
    size_t view_room;
} save_dialog_state_t;

/* Reads what @param asks to show into @s, which it keeps */
void save_dialog_state_build(save_dialog_state_t* s, const OrbisSaveDataDialogParam* param);
/* What the overlay is opened with, lending @s's texts and icons */
ovl_save_dialog_param_t save_dialog_state_make_param(save_dialog_state_t* s);
/* The result of a dialog finished with @button and @result, @item the save chosen (an index
 * into what make_param lent, -1 for none) */
void save_dialog_state_make_result(const save_dialog_state_t* s, save_dialog_result_t* out,
                                   int button, int result, int item);
/* Lets the icons go, once the overlay no longer draws them */
void save_dialog_state_release_icons(save_dialog_state_t* s);

/* SaveDialogResult::CopyTo */
void save_dialog_result_copy_to(const save_dialog_result_t* r, OrbisSaveDataDialogResult* out);

RETRO_END_DECLS

#endif
