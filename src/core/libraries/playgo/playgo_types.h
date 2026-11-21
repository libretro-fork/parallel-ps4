/* SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef SHADPS4_LIBRARIES_PLAYGO_TYPES_H
#define SHADPS4_LIBRARIES_PLAYGO_TYPES_H

#include <stdint.h>

#include "common/types.h"

typedef u32 OrbisPlayGoHandle;
typedef u16 OrbisPlayGoChunkId;
typedef s64 OrbisPlayGoEta;
typedef u64 OrbisPlayGoLanguageMask;

/* OrbisPlayGoLocus: an s8 */
typedef s8 OrbisPlayGoLocus;
#define ORBIS_PLAYGO_LOCUS_NOT_DOWNLOADED 0
#define ORBIS_PLAYGO_LOCUS_LOCAL_SLOW 2
#define ORBIS_PLAYGO_LOCUS_LOCAL_FAST 3

/* OrbisPlayGoInstallSpeed: an s32 */
typedef s32 OrbisPlayGoInstallSpeed;
#define ORBIS_PLAYGO_INSTALL_SPEED_SUSPENDED 0
#define ORBIS_PLAYGO_INSTALL_SPEED_TRICKLE 1
#define ORBIS_PLAYGO_INSTALL_SPEED_FULL 2

typedef struct OrbisPlayGoInitParams {
    const void* bufAddr;
    u32 bufSize;
    u32 reserved;
} OrbisPlayGoInitParams;

typedef struct OrbisPlayGoToDo {
    OrbisPlayGoChunkId chunkId;
    OrbisPlayGoLocus locus;
    s8 reserved;
} OrbisPlayGoToDo;

typedef struct OrbisPlayGoProgress {
    u64 progressSize;
    u64 totalSize;
} OrbisPlayGoProgress;

/* The sizes the game sees */
typedef char playgo_init_params_size_check[sizeof(OrbisPlayGoInitParams) == 16 ? 1 : -1];
typedef char playgo_todo_size_check[sizeof(OrbisPlayGoToDo) == 4 ? 1 : -1];
typedef char playgo_progress_size_check[sizeof(OrbisPlayGoProgress) == 16 ? 1 : -1];

#define ORBIS_PLAYGO_ERROR_UNKNOWN ((int32_t)0x80B20001)
#define ORBIS_PLAYGO_ERROR_FATAL ((int32_t)0x80B20002)
#define ORBIS_PLAYGO_ERROR_NO_MEMORY ((int32_t)0x80B20003)
#define ORBIS_PLAYGO_ERROR_INVALID_ARGUMENT ((int32_t)0x80B20004)
#define ORBIS_PLAYGO_ERROR_NOT_INITIALIZED ((int32_t)0x80B20005)
#define ORBIS_PLAYGO_ERROR_ALREADY_INITIALIZED ((int32_t)0x80B20006)
#define ORBIS_PLAYGO_ERROR_ALREADY_STARTED ((int32_t)0x80B20007)
#define ORBIS_PLAYGO_ERROR_NOT_STARTED ((int32_t)0x80B20008)
#define ORBIS_PLAYGO_ERROR_BAD_HANDLE ((int32_t)0x80B20009)
#define ORBIS_PLAYGO_ERROR_BAD_POINTER ((int32_t)0x80B2000A)
#define ORBIS_PLAYGO_ERROR_BAD_SIZE ((int32_t)0x80B2000B)
#define ORBIS_PLAYGO_ERROR_BAD_CHUNK_ID ((int32_t)0x80B2000C)
#define ORBIS_PLAYGO_ERROR_BAD_SPEED ((int32_t)0x80B2000D)
#define ORBIS_PLAYGO_ERROR_NOT_SUPPORT_PLAYGO ((int32_t)0x80B2000E)
#define ORBIS_PLAYGO_ERROR_EPERM ((int32_t)0x80B2000F)
#define ORBIS_PLAYGO_ERROR_BAD_LOCUS ((int32_t)0x80B20010)
#define ORBIS_PLAYGO_ERROR_NEED_DATA_DISC ((int32_t)0x80B20011)

#endif
