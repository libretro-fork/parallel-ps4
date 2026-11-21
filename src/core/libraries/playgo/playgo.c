/* SPDX-FileCopyrightText: Copyright 2024-2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <stddef.h>

#include <boolean.h>
#include <retro_atomic.h>

#include "common/c_bridge.h"
#include "core/file_format/playgo_chunk.h"
#include "core/file_sys/fs_c.h"
#include "core/libraries/error_codes.h"
#include "core/libraries/kernel/kernel_c.h"
#include "core/libraries/playgo/playgo.h"
#include "core/libraries/system/systemservice_c.h"

#define PLAYGO_HANDLE 1

/* What scePlayGoInitialize sets up and scePlayGoTerminate ends. Made once, not per
 * initialization: the chunk sizes' table is the largest a file can have. */
static struct {
    bool initialized;
    OrbisPlayGoHandle handle;
    /* The install speed (low 8 bits) and the game's time it was set at, in milliseconds (the
     * rest): one atomic word, set and read by any thread */
    retro_atomic_64_t speed_word;
    OrbisPlayGoLanguageMask lang_mask;
    playgo_file_t file;
} g_playgo;

/* The game's time now, in milliseconds */
static s64 playgo_now_ms(void) {
    return (s64)(kernel_game_now() / 1000000);
}

s32 PS4_SYSV_ABI sceDbgPlayGoRequestNextChunk(void) {
    c_log(C_LOG_LIB_PLAYGO, C_LOG_LEVEL_ERROR, "sceDbgPlayGoRequestNextChunk", "(STUBBED)called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceDbgPlayGoSnapshot(void) {
    c_log(C_LOG_LIB_PLAYGO, C_LOG_LEVEL_ERROR, "sceDbgPlayGoSnapshot", "(STUBBED)called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI scePlayGoClose(OrbisPlayGoHandle handle) {
    c_log(C_LOG_LIB_PLAYGO, C_LOG_LEVEL_INFO, "scePlayGoClose", "called");

    if (handle != PLAYGO_HANDLE) {
        return ORBIS_PLAYGO_ERROR_BAD_HANDLE;
    }
    if (!g_playgo.initialized) {
        return ORBIS_PLAYGO_ERROR_NOT_INITIALIZED;
    }
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI scePlayGoGetChunkId(OrbisPlayGoHandle handle, OrbisPlayGoChunkId* outChunkIdList,
                                     u32 numberOfEntries, u32* outEntries) {
    u32 i;
    c_log(C_LOG_LIB_PLAYGO, C_LOG_LEVEL_DEBUG, "scePlayGoGetChunkId", "called");

    if (handle != PLAYGO_HANDLE) {
        return ORBIS_PLAYGO_ERROR_BAD_HANDLE;
    }
    if (outEntries == NULL) {
        return ORBIS_PLAYGO_ERROR_BAD_POINTER;
    }
    if (outChunkIdList != NULL && numberOfEntries == 0) {
        return ORBIS_PLAYGO_ERROR_BAD_SIZE;
    }
    if (!g_playgo.initialized) {
        return ORBIS_PLAYGO_ERROR_NOT_INITIALIZED;
    }

    if (g_playgo.file.file_size == 0) {
        *outEntries = 0;
        return ORBIS_OK;
    }

    if (outChunkIdList == NULL) {
        *outEntries = g_playgo.file.chunk_count;
        return ORBIS_OK;
    }

    if (numberOfEntries > g_playgo.file.chunk_count) {
        numberOfEntries = g_playgo.file.chunk_count;
    }

    for (i = 0; i < numberOfEntries; i++) {
        outChunkIdList[i] = (OrbisPlayGoChunkId)i;
    }
    *outEntries = numberOfEntries;
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI scePlayGoGetEta(OrbisPlayGoHandle handle, const OrbisPlayGoChunkId* chunkIds,
                                 u32 numberOfEntries, OrbisPlayGoEta* outEta) {
    c_log(C_LOG_LIB_PLAYGO, C_LOG_LEVEL_DEBUG, "scePlayGoGetEta", "called");
    if (handle != PLAYGO_HANDLE) {
        return ORBIS_PLAYGO_ERROR_BAD_HANDLE;
    }
    if (chunkIds == NULL || outEta == NULL) {
        return ORBIS_PLAYGO_ERROR_BAD_POINTER;
    }
    if (numberOfEntries == 0) {
        return ORBIS_PLAYGO_ERROR_BAD_SIZE;
    }

    *outEta = 0; /* all is loaded */
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI scePlayGoGetInstallSpeed(OrbisPlayGoHandle handle,
                                          OrbisPlayGoInstallSpeed* outSpeed) {
    s64 word;
    OrbisPlayGoInstallSpeed speed;
    c_log(C_LOG_LIB_PLAYGO, C_LOG_LEVEL_INFO, "scePlayGoGetInstallSpeed", "called");

    if (handle != PLAYGO_HANDLE) {
        return ORBIS_PLAYGO_ERROR_BAD_HANDLE;
    }
    if (outSpeed == NULL) {
        return ORBIS_PLAYGO_ERROR_BAD_POINTER;
    }
    if (!g_playgo.initialized) {
        return ORBIS_PLAYGO_ERROR_NOT_INITIALIZED;
    }

    word = retro_atomic_load_acquire_64(&g_playgo.speed_word);
    speed = (OrbisPlayGoInstallSpeed)(word & 0xff);
    /* Thirty seconds of the game's time: trickling again, unless it was set meanwhile */
    if (speed == ORBIS_PLAYGO_INSTALL_SPEED_SUSPENDED &&
        playgo_now_ms() - (word >> 8) > 30 * 1000) {
        const s64 trickle = (word & ~(s64)0xff) | ORBIS_PLAYGO_INSTALL_SPEED_TRICKLE;
        if (retro_atomic_cas_64(&g_playgo.speed_word, word, trickle)) {
            speed = ORBIS_PLAYGO_INSTALL_SPEED_TRICKLE;
        } else {
            speed = (OrbisPlayGoInstallSpeed)(retro_atomic_load_acquire_64(&g_playgo.speed_word) &
                                              0xff);
        }
    }
    *outSpeed = speed;

    return ORBIS_OK;
}

s32 PS4_SYSV_ABI scePlayGoGetLanguageMask(OrbisPlayGoHandle handle,
                                          OrbisPlayGoLanguageMask* outLanguageMask) {
    c_log(C_LOG_LIB_PLAYGO, C_LOG_LEVEL_INFO, "scePlayGoGetLanguageMask", "called");

    if (handle != PLAYGO_HANDLE) {
        return ORBIS_PLAYGO_ERROR_BAD_HANDLE;
    }
    if (outLanguageMask == NULL) {
        return ORBIS_PLAYGO_ERROR_BAD_POINTER;
    }
    if (!g_playgo.initialized) {
        return ORBIS_PLAYGO_ERROR_NOT_INITIALIZED;
    }

    *outLanguageMask = g_playgo.lang_mask;
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI scePlayGoGetLocus(OrbisPlayGoHandle handle, const OrbisPlayGoChunkId* chunkIds,
                                   u32 numberOfEntries, OrbisPlayGoLocus* outLoci) {
    u32 i;
    if (handle != PLAYGO_HANDLE) {
        return ORBIS_PLAYGO_ERROR_BAD_HANDLE;
    }
    if (chunkIds == NULL || outLoci == NULL) {
        return ORBIS_PLAYGO_ERROR_BAD_POINTER;
    }
    if (numberOfEntries == 0) {
        return ORBIS_PLAYGO_ERROR_BAD_SIZE;
    }

    c_log(C_LOG_LIB_PLAYGO, C_LOG_LEVEL_DEBUG, "scePlayGoGetLocus",
          "called handle = %u, chunkIds = %u, numberOfEntries = %u", (unsigned)handle,
          (unsigned)*chunkIds, (unsigned)numberOfEntries);

    if (!g_playgo.initialized) {
        return ORBIS_PLAYGO_ERROR_NOT_INITIALIZED;
    }
    if (g_playgo.file.file_size == 0) {
        return ORBIS_PLAYGO_ERROR_NOT_SUPPORT_PLAYGO;
    }

    for (i = 0; i < numberOfEntries; i++) {
        if (chunkIds[i] < g_playgo.file.chunk_count) {
            outLoci[i] = ORBIS_PLAYGO_LOCUS_LOCAL_FAST;
        } else {
            outLoci[i] = ORBIS_PLAYGO_LOCUS_NOT_DOWNLOADED;
            return ORBIS_PLAYGO_ERROR_BAD_CHUNK_ID;
        }
    }
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI scePlayGoGetProgress(OrbisPlayGoHandle handle, const OrbisPlayGoChunkId* chunkIds,
                                      u32 numberOfEntries, OrbisPlayGoProgress* outProgress) {
    u64 total_size = 0;
    u32 i;
    c_log(C_LOG_LIB_PLAYGO, C_LOG_LEVEL_DEBUG, "scePlayGoGetProgress",
          "called handle = %u, numberOfEntries = %u", (unsigned)handle, (unsigned)numberOfEntries);

    if (handle != PLAYGO_HANDLE) {
        return ORBIS_PLAYGO_ERROR_BAD_HANDLE;
    }
    if (chunkIds == NULL || outProgress == NULL) {
        return ORBIS_PLAYGO_ERROR_BAD_POINTER;
    }
    if (numberOfEntries == 0) {
        return ORBIS_PLAYGO_ERROR_BAD_SIZE;
    }
    if (!g_playgo.initialized) {
        return ORBIS_PLAYGO_ERROR_NOT_INITIALIZED;
    }
    if (g_playgo.file.file_size == 0) {
        return ORBIS_PLAYGO_ERROR_BAD_CHUNK_ID;
    }

    outProgress->progressSize = 0;
    outProgress->totalSize = 0;

    for (i = 0; i < numberOfEntries; i++) {
        const u32 chunk_id = chunkIds[i];
        if (chunk_id < g_playgo.file.chunk_count) {
            total_size += g_playgo.file.chunk_sizes[chunk_id];
        } else {
            return ORBIS_PLAYGO_ERROR_BAD_CHUNK_ID;
        }
    }

    outProgress->progressSize = total_size;
    outProgress->totalSize = total_size;

    return ORBIS_OK;
}

s32 PS4_SYSV_ABI scePlayGoGetToDoList(OrbisPlayGoHandle handle, OrbisPlayGoToDo* outTodoList,
                                      u32 numberOfEntries, u32* outEntries) {
    c_log(C_LOG_LIB_PLAYGO, C_LOG_LEVEL_INFO, "scePlayGoGetToDoList",
          "called handle = %u numberOfEntries = %u", (unsigned)handle, (unsigned)numberOfEntries);

    if (handle != PLAYGO_HANDLE) {
        return ORBIS_PLAYGO_ERROR_BAD_HANDLE;
    }
    if (outTodoList == NULL || outEntries == NULL) {
        return ORBIS_PLAYGO_ERROR_BAD_POINTER;
    }
    if (numberOfEntries == 0) {
        return ORBIS_PLAYGO_ERROR_BAD_SIZE;
    }
    if (!g_playgo.initialized) {
        return ORBIS_PLAYGO_ERROR_NOT_INITIALIZED;
    }
    *outEntries = 0; /* nothing to do */
    return ORBIS_OK;
}

/* The language mask of a system language: its bit from the top, none past the 48th */
static u64 playgo_convert_language(u64 system_lang) {
    if (system_lang < 48) {
        return (u64)1 << (63 - system_lang);
    }
    return 0;
}

/* Reads the PlayGo file where it lies */
static void playgo_read_file(const u8* data, size_t size, void* ctx) {
    if (!playgo_file_open(&g_playgo.file, data, size)) {
        c_log(C_LOG_LIB_PLAYGO, C_LOG_LEVEL_WARNING, "scePlayGoInitialize",
              "Could not parse PlayGo file");
    }
    (void)ctx;
}

s32 PS4_SYSV_ABI scePlayGoInitialize(OrbisPlayGoInitParams* param) {
    s32 system_lang = 0;
    c_log(C_LOG_LIB_PLAYGO, C_LOG_LEVEL_INFO, "scePlayGoInitialize", "called, bufSize = %u",
          (unsigned)param->bufSize);
    if (param->bufAddr == NULL) {
        return ORBIS_PLAYGO_ERROR_BAD_POINTER;
    }
    if (param->bufSize < 0x200000) {
        return ORBIS_PLAYGO_ERROR_BAD_SIZE;
    }
    if (g_playgo.initialized) {
        return ORBIS_PLAYGO_ERROR_ALREADY_INITIALIZED;
    }

    g_playgo.handle = 0;
    retro_atomic_store_release_64(&g_playgo.speed_word, ORBIS_PLAYGO_INSTALL_SPEED_FULL);
    g_playgo.file.file_size = 0;
    g_playgo.file.chunk_count = 0;

    if (!fs_lend_guest_file("/app0/sce_sys/playgo-chunk.dat", playgo_read_file, NULL)) {
        c_log(C_LOG_LIB_PLAYGO, C_LOG_LEVEL_WARNING, "scePlayGoInitialize",
              "Could not open PlayGo file");
    }

    system_service_param_get_int(SYSTEM_SERVICE_PARAM_ID_LANG, &system_lang);
    /* As the C++ did: the s32 taken as a u64, so a negative one is no language */
    g_playgo.lang_mask = playgo_convert_language((u64)(s64)system_lang);
    g_playgo.initialized = true;

    return ORBIS_OK;
}

s32 PS4_SYSV_ABI scePlayGoOpen(OrbisPlayGoHandle* outHandle, const void* param) {
    c_log(C_LOG_LIB_PLAYGO, C_LOG_LEVEL_INFO, "scePlayGoOpen", "called");

    if (outHandle == NULL) {
        return ORBIS_PLAYGO_ERROR_BAD_POINTER;
    }
    if (param) {
        return ORBIS_PLAYGO_ERROR_INVALID_ARGUMENT;
    }
    if (!g_playgo.initialized) {
        return ORBIS_PLAYGO_ERROR_NOT_INITIALIZED;
    }
    if (g_playgo.file.file_size == 0) {
        return ORBIS_PLAYGO_ERROR_NOT_SUPPORT_PLAYGO;
    }

    g_playgo.handle = *outHandle = PLAYGO_HANDLE;
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI scePlayGoPrefetch(OrbisPlayGoHandle handle, const OrbisPlayGoChunkId* chunkIds,
                                   u32 numberOfEntries, OrbisPlayGoLocus minimumLocus) {
    c_log(C_LOG_LIB_PLAYGO, C_LOG_LEVEL_INFO, "scePlayGoPrefetch", "called");

    if (handle != PLAYGO_HANDLE) {
        return ORBIS_PLAYGO_ERROR_BAD_HANDLE;
    }
    if (chunkIds == NULL) {
        return ORBIS_PLAYGO_ERROR_BAD_POINTER;
    }
    if (numberOfEntries == 0) {
        return ORBIS_PLAYGO_ERROR_BAD_SIZE;
    }
    if (!g_playgo.initialized) {
        return ORBIS_PLAYGO_ERROR_NOT_INITIALIZED;
    }

    switch (minimumLocus) {
    case ORBIS_PLAYGO_LOCUS_NOT_DOWNLOADED:
    case ORBIS_PLAYGO_LOCUS_LOCAL_SLOW:
    case ORBIS_PLAYGO_LOCUS_LOCAL_FAST:
        break;
    default:
        return ORBIS_PLAYGO_ERROR_BAD_LOCUS;
    }
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI scePlayGoSetInstallSpeed(OrbisPlayGoHandle handle, OrbisPlayGoInstallSpeed speed) {
    c_log(C_LOG_LIB_PLAYGO, C_LOG_LEVEL_INFO, "scePlayGoSetInstallSpeed", "called");

    if (handle != PLAYGO_HANDLE) {
        return ORBIS_PLAYGO_ERROR_BAD_HANDLE;
    }
    if (!g_playgo.initialized) {
        return ORBIS_PLAYGO_ERROR_NOT_INITIALIZED;
    }

    switch (speed) {
    case ORBIS_PLAYGO_INSTALL_SPEED_SUSPENDED:
    case ORBIS_PLAYGO_INSTALL_SPEED_TRICKLE:
    case ORBIS_PLAYGO_INSTALL_SPEED_FULL:
        break;
    default:
        return ORBIS_PLAYGO_ERROR_INVALID_ARGUMENT;
    }

    retro_atomic_store_release_64(&g_playgo.speed_word, (playgo_now_ms() << 8) | (s64)speed);

    return ORBIS_OK;
}

s32 PS4_SYSV_ABI scePlayGoSetLanguageMask(OrbisPlayGoHandle handle,
                                          OrbisPlayGoLanguageMask languageMask) {
    c_log(C_LOG_LIB_PLAYGO, C_LOG_LEVEL_INFO, "scePlayGoSetLanguageMask", "called");

    if (handle != PLAYGO_HANDLE) {
        return ORBIS_PLAYGO_ERROR_BAD_HANDLE;
    }
    if (!g_playgo.initialized) {
        return ORBIS_PLAYGO_ERROR_NOT_INITIALIZED;
    }

    g_playgo.lang_mask = languageMask;
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI scePlayGoSetToDoList(OrbisPlayGoHandle handle, const OrbisPlayGoToDo* todoList,
                                      u32 numberOfEntries) {
    c_log(C_LOG_LIB_PLAYGO, C_LOG_LEVEL_INFO, "scePlayGoSetToDoList", "called");

    if (handle != PLAYGO_HANDLE) {
        return ORBIS_PLAYGO_ERROR_BAD_HANDLE;
    }
    if (todoList == NULL) {
        return ORBIS_PLAYGO_ERROR_BAD_POINTER;
    }
    if (numberOfEntries == 0) {
        return ORBIS_PLAYGO_ERROR_BAD_SIZE;
    }
    if (!g_playgo.initialized) {
        return ORBIS_PLAYGO_ERROR_NOT_INITIALIZED;
    }
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI scePlayGoTerminate(void) {
    c_log(C_LOG_LIB_PLAYGO, C_LOG_LEVEL_INFO, "scePlayGoTerminate", "called");

    if (!g_playgo.initialized) {
        return ORBIS_PLAYGO_ERROR_NOT_INITIALIZED;
    }
    g_playgo.initialized = false;
    return ORBIS_OK;
}
