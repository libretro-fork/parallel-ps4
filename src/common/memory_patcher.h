// SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <stddef.h>
#include <stdint.h>

#include "common/patch_catalog.h"

#if defined(WIN32)
#define EXPORT __declspec(dllexport)
#else
#define EXPORT __attribute__((visibility("default")))
#endif

namespace MemoryPatcher {

extern EXPORT uintptr_t g_eboot_address;
extern uint64_t g_eboot_image_size;
/// The executable's file name, as the patches name it (AppElf)
extern char g_eboot_name[256];
/// The game's serial, to find its patches by
extern char g_game_serial[32];

enum PatchMask : uint8_t {
    None,
    Mask,
    Mask_Jump32,
};

/// One line of a patch, its strings borrowed for the time it is applied
struct patchInfo {
    const char* modNameStr;
    const char* offsetStr;
    const char* valueStr;
    const char* targetStr;
    const char* sizeStr;
    bool isOffset;
    bool littleEndian;
    PatchMask patchMask;
    int maskOffset;
};

void OnGameLoaded();

/// The game's patches in the patch repositories the core carries (src/resources/patches),
/// for the Patch Manager core options. False when memory ran out.
bool BuildCatalog(patch_catalog_t& cat, const char* serial);
/// The Patch Manager's choice: the entries switched on in the catalog for @serial, as a bit
/// set of PATCH_CATALOG_WORDS words. Applied with the other patches once the game is loaded.
void SetCatalogPatches(const char* serial, const uint32_t* on);

void PatchMemory(const patchInfo& patch);

/// Where @signature (hex bytes, ?? for any) is first found in the executable; 0 when it is not
uintptr_t PatternScan(const char* signature);

} // namespace MemoryPatcher
