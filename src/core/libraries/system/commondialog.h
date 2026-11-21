// SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <stddef.h>

#include "common/types.h"
#include "core/libraries/system/commondialog_c.h"

namespace Libraries::CommonDialog {

enum class Status : u32 {
    NONE = 0,
    INITIALIZED = 1,
    RUNNING = 2,
    FINISHED = 3,
};

enum class Result : u32 {
    OK = 0,
    USER_CANCELED = 1,
};

enum class Error : u32 {
    OK = 0,
    NOT_SYSTEM_INITIALIZED = 0x80B80001,
    ALREADY_SYSTEM_INITIALIZED = 0x80B80002,
    NOT_INITIALIZED = 0x80B80003,
    ALREADY_INITIALIZED = 0x80B80004,
    NOT_FINISHED = 0x80B80005,
    INVALID_STATE = 0x80B80006,
    RESULT_NONE = 0x80B80007,
    BUSY = 0x80B80008,
    OUT_OF_MEMORY = 0x80B80009,
    PARAM_INVALID = 0x80B8000A,
    NOT_RUNNING = 0x80B8000B,
    ALREADY_CLOSE = 0x80B8000C,
    ARG_NULL = 0x80B8000D,
    UNEXPECTED_FATAL = 0x80B8000E,
    NOT_SUPPORTED = 0x80B8000F,
    INHIBIT_SHAREPLAY_CLIENT = 0x80B80010,
};

// The two flags every dialog library shares, which commondialog.c (C89) holds
inline bool& g_isInitialized = g_common_dialog_initialized;
inline bool& g_isUsed = g_common_dialog_used;

using BaseParam = common_dialog_base_param;

static_assert(static_cast<u32>(Status::FINISHED) == COMMON_DIALOG_STATUS_FINISHED);
static_assert(static_cast<u32>(Result::USER_CANCELED) == COMMON_DIALOG_RESULT_USER_CANCELED);
static_assert(static_cast<u32>(Error::NOT_RUNNING) == COMMON_DIALOG_ERROR_NOT_RUNNING);
static_assert(static_cast<u32>(Error::INHIBIT_SHAREPLAY_CLIENT) ==
              COMMON_DIALOG_ERROR_INHIBIT_SHAREPLAY_CLIENT);

} // namespace Libraries::CommonDialog
