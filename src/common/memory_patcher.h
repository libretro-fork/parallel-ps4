// SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once
#include <cstring>
#include <string>
#include <vector>

#include "common/patch_catalog.h"

#if defined(WIN32)
#define EXPORT __declspec(dllexport)
#else
#define EXPORT __attribute__((visibility("default")))
#endif

namespace MemoryPatcher {

extern EXPORT uintptr_t g_eboot_address;
extern uint64_t g_eboot_image_size;
extern std::string g_eboot_name;
extern std::string g_game_serial;
extern std::string patch_file;

enum PatchMask : uint8_t {
    None,
    Mask,
    Mask_Jump32,
};

struct patchInfo {
    std::string gameSerial;
    std::string modNameStr;
    std::string offsetStr;
    std::string valueStr;
    std::string targetStr;
    std::string sizeStr;
    bool isOffset;
    bool littleEndian;
    PatchMask patchMask;
    int maskOffset;
};

std::string convertValueToHex(const std::string type, const std::string valueStr);

void OnGameLoaded();

/// The game's patches in the patch repositories the core carries (src/resources/patches),
/// for the Patch Manager core options. False when memory ran out.
bool BuildCatalog(patch_catalog_t& cat, const char* serial);
/// The Patch Manager's choice: the entries switched on in the catalog for @serial, as a bit
/// set of PATCH_CATALOG_WORDS words. Applied with the other patches once the game is loaded.
void SetCatalogPatches(const char* serial, const uint32_t* on);

void PatchMemory(const patchInfo& patch);

static std::vector<int32_t> PatternToByte(const std::string& pattern);
uintptr_t PatternScan(const std::string& signature);

} // namespace MemoryPatcher