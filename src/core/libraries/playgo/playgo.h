/* SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef SHADPS4_LIBRARIES_PLAYGO_H
#define SHADPS4_LIBRARIES_PLAYGO_H

/* libScePlayGo: every chunk of the game is installed, and fast. Defined in playgo.c, registered
 * in libs.cpp. */

#include <retro_common_api.h>

#include "common/types.h"
#include "core/libraries/playgo/playgo_types.h"

RETRO_BEGIN_DECLS

s32 PS4_SYSV_ABI sceDbgPlayGoRequestNextChunk(void);
s32 PS4_SYSV_ABI sceDbgPlayGoSnapshot(void);
s32 PS4_SYSV_ABI scePlayGoClose(OrbisPlayGoHandle handle);
s32 PS4_SYSV_ABI scePlayGoGetChunkId(OrbisPlayGoHandle handle, OrbisPlayGoChunkId* outChunkIdList,
                                     u32 numberOfEntries, u32* outEntries);
s32 PS4_SYSV_ABI scePlayGoGetEta(OrbisPlayGoHandle handle, const OrbisPlayGoChunkId* chunkIds,
                                 u32 numberOfEntries, OrbisPlayGoEta* outEta);
s32 PS4_SYSV_ABI scePlayGoGetInstallSpeed(OrbisPlayGoHandle handle,
                                          OrbisPlayGoInstallSpeed* outSpeed);
s32 PS4_SYSV_ABI scePlayGoGetLanguageMask(OrbisPlayGoHandle handle,
                                          OrbisPlayGoLanguageMask* outLanguageMask);
s32 PS4_SYSV_ABI scePlayGoGetLocus(OrbisPlayGoHandle handle, const OrbisPlayGoChunkId* chunkIds,
                                   u32 numberOfEntries, OrbisPlayGoLocus* outLoci);
s32 PS4_SYSV_ABI scePlayGoGetProgress(OrbisPlayGoHandle handle, const OrbisPlayGoChunkId* chunkIds,
                                      u32 numberOfEntries, OrbisPlayGoProgress* outProgress);
s32 PS4_SYSV_ABI scePlayGoGetToDoList(OrbisPlayGoHandle handle, OrbisPlayGoToDo* outTodoList,
                                      u32 numberOfEntries, u32* outEntries);
s32 PS4_SYSV_ABI scePlayGoInitialize(OrbisPlayGoInitParams* param);
s32 PS4_SYSV_ABI scePlayGoOpen(OrbisPlayGoHandle* outHandle, const void* param);
s32 PS4_SYSV_ABI scePlayGoPrefetch(OrbisPlayGoHandle handle, const OrbisPlayGoChunkId* chunkIds,
                                   u32 numberOfEntries, OrbisPlayGoLocus minimumLocus);
s32 PS4_SYSV_ABI scePlayGoSetInstallSpeed(OrbisPlayGoHandle handle, OrbisPlayGoInstallSpeed speed);
s32 PS4_SYSV_ABI scePlayGoSetLanguageMask(OrbisPlayGoHandle handle,
                                          OrbisPlayGoLanguageMask languageMask);
s32 PS4_SYSV_ABI scePlayGoSetToDoList(OrbisPlayGoHandle handle, const OrbisPlayGoToDo* todoList,
                                      u32 numberOfEntries);
s32 PS4_SYSV_ABI scePlayGoTerminate(void);

RETRO_END_DECLS

#endif
