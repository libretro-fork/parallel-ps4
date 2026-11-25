// SPDX-FileCopyrightText: Copyright 2024-2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <compat/strl.h>
#include <encodings/utf.h>
#include <file/file_path.h>
#include <retro_dirent.h>
#include <retro_miscellaneous.h>
#include <streams/file_stream.h>

#include "common/elf_info.h"
#include "common/embedded_resources.h"
#include "common/logging/log.h"
#include "common/patch_halve.h"
#include "common/patch_index.h"
#include "common/path_util.h"
#include "common/small_vector.h"
#include "common/xml.h"
#include "core/file_format/psf.h"
#include "memory_patcher.h"

namespace MemoryPatcher {

EXPORT uintptr_t g_eboot_address;
uint64_t g_eboot_image_size;
char g_eboot_name[256];
char g_game_serial[32];
// The Patch Manager's choice (SetCatalogPatches)
static char catalog_serial[16];
static uint32_t catalog_on[PATCH_CATALOG_WORDS];

namespace {

// A value written as hex after 0x, $ or #; decimal otherwise. @digits is past the prefix.
int NumberBase(const char* s, const char** digits) {
    if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) {
        *digits = s + 2;
        return 16;
    }
    if (s[0] == '$' || s[0] == '#') {
        *digits = s + 1;
        return 16;
    }
    *digits = s;
    return 10;
}

// @s as an unsigned number in its base (NumberBase), what follows the digits left out as
// std::stoul did; false when there are no digits
bool ParseUnsigned(const char* s, unsigned long long* out) {
    const char* digits = nullptr;
    const int base = NumberBase(s, &digits);
    char* end = nullptr;
    *out = strtoull(digits, &end, base);
    return end != digits;
}

// @bytes bytes of @value in hex, most significant first: at least that many digits
void AppendHex(char* out, size_t* length, size_t size, unsigned long long value, size_t bytes) {
    const int written =
        snprintf(out + *length, size - *length, "%0*llx", static_cast<int>(bytes * 2), value);
    if (written > 0) {
        *length += static_cast<size_t>(written);
    }
}

u32 ByteSwap32(u32 v) {
    return (v >> 24) | ((v >> 8) & 0xff00) | ((v << 8) & 0xff0000) | (v << 24);
}

u64 ByteSwap64(u64 v) {
    return (u64(ByteSwap32(u32(v))) << 32) | ByteSwap32(u32(v >> 32));
}

// The bytes a patch line of @type writes, in hex, into @out (at least 8 times the value's
// length and 16 more). False when the value is not one of its type.
bool ConvertValueToHex(const char* type, const char* value, char* out, size_t size) {
    size_t length = 0;
    out[0] = '\0';
    unsigned long long number = 0;
    if (strcmp(type, "byte") == 0 || strcmp(type, "bytes16") == 0 || strcmp(type, "bytes32") == 0 ||
        strcmp(type, "bytes64") == 0) {
        if (!ParseUnsigned(value, &number)) {
            return false;
        }
        const size_t bytes = type[4] == '\0' ? 1 : type[5] == '1' ? 2 : type[5] == '3' ? 4 : 8;
        if (bytes < 8) {
            number &= 0xffffffffull;
        }
        AppendHex(out, &length, size, number, bytes);
    } else if (strcmp(type, "float32") == 0) {
        char* end = nullptr;
        const float f = strtof(value, &end);
        if (end == value) {
            return false;
        }
        u32 bits = 0;
        memcpy(&bits, &f, sizeof(bits));
        AppendHex(out, &length, size, ByteSwap32(bits), sizeof(bits));
    } else if (strcmp(type, "float64") == 0) {
        char* end = nullptr;
        const double d = strtod(value, &end);
        if (end == value) {
            return false;
        }
        u64 bits = 0;
        memcpy(&bits, &d, sizeof(bits));
        AppendHex(out, &length, size, ByteSwap64(bits), sizeof(bits));
    } else if (strcmp(type, "utf8") == 0) {
        // Its bytes and a NUL
        for (const char* c = value;; ++c) {
            AppendHex(out, &length, size, static_cast<unsigned char>(*c), 1);
            if (!*c) {
                break;
            }
        }
    } else if (strcmp(type, "utf16") == 0) {
        // In UTF-16, little-endian, and a NUL
        const char* walk = value;
        for (;;) {
            u32 code = utf8_walk(&walk);
            if (code == 0) {
                break;
            }
            u16 units[2];
            size_t count = 1;
            if (code <= 0xFFFF) {
                units[0] = static_cast<u16>(code);
            } else {
                code -= 0x10000;
                units[0] = static_cast<u16>(0xD800 | (code >> 10));
                units[1] = static_cast<u16>(0xDC00 | (code & 0x3FF));
                count = 2;
            }
            for (size_t i = 0; i < count; ++i) {
                AppendHex(out, &length, size, units[i] & 0xFF, 1);
                AppendHex(out, &length, size, units[i] >> 8, 1);
            }
        }
        AppendHex(out, &length, size, 0, 2);
    } else if (strcmp(type, "bytes") == 0 || strcmp(type, "mask") == 0 ||
               strcmp(type, "mask_jump32") == 0) {
        strlcpy(out, value, size);
    } else {
        LOG_INFO(Loader, "Error applying Patch, unknown type: %s", type);
    }
    return true;
}

// The game's version, as its patches name it (AppVer)
void GetAppVersion(char* out, size_t size) {
    if (!psf_game_get_string("APP_VER", out, size, nullptr)) {
        strlcpy(out, "Unknown version", size);
    }
}

// The bytes of @hex (two digits each) into @out, at most @size; how many, or -1 when a pair
// is not hex
long ParseHexBytes(const char* hex, u8* out, size_t size) {
    size_t count = 0;
    for (size_t i = 0; hex[i] && hex[i + 1] && count < size; i += 2) {
        const char pair[3] = {hex[i], hex[i + 1], '\0'};
        char* end = nullptr;
        const unsigned long byte = strtoul(pair, &end, 16);
        if (end != pair + 2) {
            return -1;
        }
        out[count++] = static_cast<u8>(byte);
    }
    return static_cast<long>(count);
}

} // Anonymous namespace

// One patch (a <Metadata>) of a patch file, for the executable and the version of the game
// it names; a mask is looked for whatever the version.
static void ApplyMetadata(const Common::Xml::Node& node, const char* app_version) {
    const char* const current_patch_name = node.attribute("Name").value();
    const bool version_matches = strcmp(node.attribute("AppVer").value(), app_version) == 0;

    if (strcmp(node.attribute("AppElf").value(), g_eboot_name) != 0) {
        return;
    }

    const Common::Xml::Node patch_list = node.first_child();
    for (const Common::Xml::Node patch_line : patch_list.children()) {
        const char* const type = patch_line.attribute("Type").value();
        const bool is_mask = strcmp(type, "mask") == 0;
        const bool is_jump = strcmp(type, "mask_jump32") == 0;
        const bool is_halve = strcmp(type, "mask_halve") == 0;
        if (!version_matches && !is_mask && !is_jump && !is_halve) {
            continue;
        }

        if (is_halve) {
            // The core's own kind of line: the decimal number of Size bytes at Offset from the
            // pattern is halved where it is (common/patch_halve.h), whatever it is by now
            const char* const pattern = patch_line.attribute("Address").value();
            const char* const offset = patch_line.attribute("Offset").value();
            const char* const size = patch_line.attribute("Size").value();
            const uintptr_t found = PatternScan(pattern);
            unsigned long before = 0;
            unsigned long after = 0;
            if (found == 0 || !offset[0] || !size[0]) {
                LOG_ERROR(Loader, "Patch %s: its pattern is not in the game, not applied: %s",
                          current_patch_name, pattern);
            } else if (patch_halve_decimal(
                           reinterpret_cast<unsigned char*>(found + strtol(offset, nullptr, 10)),
                           static_cast<size_t>(strtoul(size, nullptr, 10)), &before, &after)) {
                LOG_INFO(Loader, "Applied patch: %s, %llu halved to %llu", current_patch_name,
                         static_cast<unsigned long long>(before),
                         static_cast<unsigned long long>(after));
            } else {
                LOG_ERROR(Loader, "Patch %s: no number to halve where its pattern is, not applied",
                          current_patch_name);
            }
            continue;
        }

        const char* const address = patch_line.attribute("Address").value();
        const char* const value = patch_line.attribute("Value").value();
        const char* const mask_offset = patch_line.attribute("Offset").value();
        const char* target = "";
        const char* size = "";
        // The bytes to write, in hex: up to four digits for each byte of a UTF-16 value
        const size_t hex_size = strlen(value) * 8 + 32;
        char* const hex = static_cast<char*>(malloc(hex_size));
        if (!hex) {
            LOG_ERROR(Loader, "Patch %s: out of memory, not applied", current_patch_name);
            continue;
        }
        if (is_jump) {
            target = patch_line.attribute("Target").value();
            size = patch_line.attribute("Size").value();
            strlcpy(hex, value, hex_size);
        } else if (!ConvertValueToHex(type, value, hex, hex_size)) {
            LOG_ERROR(Loader,
                      "Patch %s: its value \"%s\" for \"%s\" is not a %s, not applied. If it "
                      "worked on earlier versions, it was in a format shadPS4 read wrongly, "
                      "and the patch should be fixed.",
                      current_patch_name, value, address, type);
            free(hex);
            continue;
        }

        const bool little_endian = strcmp(type, "bytes16") == 0 || strcmp(type, "bytes32") == 0 ||
                                   strcmp(type, "bytes64") == 0;
        PatchMask patch_mask = PatchMask::None;
        if (is_mask) {
            patch_mask = PatchMask::Mask;
        } else if (is_jump) {
            patch_mask = PatchMask::Mask_Jump32;
        }
        int mask_offset_value = 0;
        if ((is_mask || is_jump) && mask_offset[0]) {
            mask_offset_value = static_cast<int>(strtol(mask_offset, nullptr, 10));
        }

        patchInfo patch{};
        patch.modNameStr = current_patch_name;
        patch.offsetStr = address;
        patch.valueStr = hex;
        patch.targetStr = target;
        patch.sizeStr = size;
        patch.littleEndian = little_endian;
        patch.patchMask = patch_mask;
        patch.maskOffset = mask_offset_value;
        PatchMemory(patch);
        free(hex);
    }
}

static void ApplyPatchesFromXML(const char* path) {
    Common::Xml::Document doc;
    if (!doc.Load(path)) {
        LOG_ERROR(Loader, "Could not read or parse patch XML: %s", path);
        return;
    }
    char app_version[64];
    GetAppVersion(app_version, sizeof(app_version));
    for (const Common::Xml::Node node : doc.child("Patch").children()) {
        if (strcmp(node.name(), "Metadata") == 0 &&
            strcmp(node.attribute("isEnabled").value(), "true") == 0) {
            ApplyMetadata(node, app_version);
        }
    }
}

namespace {

constexpr const char PatchPrefix[] = "src/resources/patches/";

using CatalogSources = Common::SmallVector<patch_catalog_source_t, 256>;

/// A patch file the core carries, filed under its repository: the folder it is in, named by a
/// string that lasts
void AddCatalogSource(void* ctx, const char* path, const u8* data, size_t size) {
    static constexpr const char* repositories[] = {"shadPS4", "GoldHEN", "ParaLLEl"};
    const char* const rest = path + sizeof(PatchPrefix) - 1;
    const char* const slash = strchr(rest, '/');
    if (!slash) {
        return;
    }
    const size_t length = static_cast<size_t>(slash - rest);
    for (const char* repository : repositories) {
        if (strlen(repository) == length && strncmp(rest, repository, length) == 0) {
            static_cast<CatalogSources*>(ctx)->push_back({repository, data, size});
            return;
        }
    }
}

} // Anonymous namespace

bool BuildCatalog(patch_catalog_t& cat, const char* serial) {
    CatalogSources sources;
    Common::ForEachEmbeddedResource(PatchPrefix, AddCatalogSource, &sources);
    return patch_catalog_build(&cat, sources.data(), sources.size(), serial) != 0;
}

void SetCatalogPatches(const char* serial, const uint32_t* on) {
    snprintf(catalog_serial, sizeof(catalog_serial), "%s", serial);
    memcpy(catalog_on, on, sizeof(catalog_on));
}

// The patches switched on in the Patch Manager, from the repositories the core carries
static void ApplyCatalogPatches() {
    bool any = false;
    for (const uint32_t word : catalog_on) {
        any = any || word != 0;
    }
    if (!any || !catalog_serial[0]) {
        return;
    }
    char app_version[64];
    GetAppVersion(app_version, sizeof(app_version));
    // Large: kept off the stack
    static patch_catalog_t cat;
    if (!BuildCatalog(cat, catalog_serial)) {
        LOG_ERROR(Loader, "Out of memory reading the patches for %s", catalog_serial);
    }
    for (unsigned i = 0; i < cat.metadata_count; i++) {
        const int entry = cat.metadata[i].entry;
        if (entry < 0 || (catalog_on[entry / 32] & (1u << (entry % 32))) == 0) {
            continue;
        }
        LOG_INFO(Loader, "Patch Manager: %s (%s)", cat.entries[entry].name,
                 cat.entries[entry].repository);
        ApplyMetadata(Common::Xml::Node(cat.metadata[i].node), app_version);
    }
    patch_catalog_free(&cat);
}

void OnGameLoaded() {
    ApplyCatalogPatches();
    // Each repository in the patches folder, by its files.json
    const char* const patch_dir = Common::FS::GetUserPathUTF8(Common::FS::PathType::PatchesDir);
    RDIR* const dir = retro_opendir_include_hidden(patch_dir, true);
    if (!dir) {
        return;
    }
    while (retro_readdir(dir)) {
        const char* const name = retro_dirent_get_name(dir);
        if (!retro_dirent_is_dir(dir, nullptr) || strcmp(name, ".") == 0 ||
            strcmp(name, "..") == 0) {
            continue;
        }
        char repo[PATH_MAX_LENGTH];
        char files_json[PATH_MAX_LENGTH];
        fill_pathname_join_special(repo, patch_dir, name, sizeof(repo));
        fill_pathname_join_special(files_json, repo, "files.json", sizeof(files_json));
        if (!path_is_valid(files_json) || path_is_directory(files_json)) {
            LOG_ERROR(Loader, "The folder '%s' does not contain the files.json file.", name);
            continue;
        }
        char patch_name[1024];
        if (!patch_index_find(files_json, g_game_serial, patch_name, sizeof(patch_name))) {
            continue;
        }
        char game_patch_file[PATH_MAX_LENGTH];
        fill_pathname_join_special(game_patch_file, repo, patch_name, sizeof(game_patch_file));
        if (path_is_valid(game_patch_file)) {
            ApplyPatchesFromXML(game_patch_file);
        }
    }
    retro_closedir(dir);
}

void PatchMemory(const patchInfo& patch) {
    // Send a request to modify the process memory.
    void* cheat_address = nullptr;

    if (patch.patchMask == PatchMask::None) {
        const long offset = strtol(patch.offsetStr, nullptr, 16);
        if (patch.isOffset) {
            cheat_address = reinterpret_cast<void*>(g_eboot_address + offset);
        } else {
            cheat_address = reinterpret_cast<void*>(g_eboot_address + (offset - 0x400000));
        }
    }

    if (patch.patchMask == PatchMask::Mask) {
        // Not found (another patch has rewritten the bytes, or this version of the game does
        // not have them): nothing is written, where 0 + the offset was written to
        const uintptr_t found = PatternScan(patch.offsetStr);
        if (found == 0) {
            LOG_ERROR(Loader, "Patch %s: its pattern is not in the game, not applied: %s",
                      patch.modNameStr, patch.offsetStr);
            return;
        }
        cheat_address = reinterpret_cast<void*>(found + patch.maskOffset);
    }

    const size_t value_length = strlen(patch.valueStr);
    if (patch.patchMask == PatchMask::Mask_Jump32) {
        const int jump_size = static_cast<int>(strtol(patch.sizeStr, nullptr, 10));

        constexpr int MAX_PATTERN_LENGTH = 256;
        if (jump_size < 5) {
            LOG_ERROR(Loader, "Jump size must be at least 5 bytes");
            return;
        }
        if (jump_size > MAX_PATTERN_LENGTH) {
            LOG_ERROR(Loader, "Jump size must be no more than %d bytes.", MAX_PATTERN_LENGTH);
            return;
        }

        // Find the base address using "Address"
        const uintptr_t base_address = PatternScan(patch.offsetStr);
        if (base_address == 0) {
            LOG_ERROR(Loader, "PatternScan failed for mask_jump32 with pattern: %s",
                      patch.offsetStr);
            return;
        }
        const uintptr_t patch_address = base_address + patch.maskOffset;

        // Fills the original region (jump_size bytes) with NOPs
        memset(reinterpret_cast<void*>(patch_address), 0x90, static_cast<size_t>(jump_size));

        // Use "Target" to locate the start of the code cave
        const uintptr_t jump_target = PatternScan(patch.targetStr);
        if (jump_target == 0) {
            LOG_ERROR(Loader, "PatternScan failed to Target with pattern: %s", patch.targetStr);
            return;
        }

        // Converts the Value attribute to a byte array (payload)
        u8* const payload = static_cast<u8*>(malloc(value_length / 2 + 1));
        if (!payload) {
            return;
        }
        const long payload_size = ParseHexBytes(patch.valueStr, payload, value_length / 2 + 1);
        if (payload_size < 0) {
            LOG_ERROR(Loader, "Invalid byte in Value: %s", patch.valueStr);
            free(payload);
            return;
        }

        // Calculates the end of the code cave (where the return jump will be inserted)
        const uintptr_t code_cave_end = jump_target + static_cast<uintptr_t>(payload_size);

        // Write the payload to the code cave, from jump_target
        memcpy(reinterpret_cast<void*>(jump_target), payload, static_cast<size_t>(payload_size));
        free(payload);

        // Inserts the initial jump in the original region to divert to the code cave
        u8 jump_instruction[5];
        jump_instruction[0] = 0xE9;
        const s32 rel_jump = static_cast<s32>(jump_target - patch_address - 5);
        memcpy(&jump_instruction[1], &rel_jump, sizeof(rel_jump));
        memcpy(reinterpret_cast<void*>(patch_address), jump_instruction, sizeof(jump_instruction));

        // Inserts jump back at the end of the code cave to resume execution after patching
        u8 jump_back[5];
        jump_back[0] = 0xE9;
        // Calculates the relative offset to return to the instruction immediately following the
        // overwritten region
        const s32 target_return =
            static_cast<s32>((patch_address + jump_size) - (code_cave_end + 5));
        memcpy(&jump_back[1], &target_return, sizeof(target_return));
        memcpy(reinterpret_cast<void*>(code_cave_end), jump_back, sizeof(jump_back));

        LOG_INFO(Loader,
                 "Applied Patch mask_jump32: %s, PatchAddress: 0x%" PRIxPTR
                 ", JumpTarget: 0x%" PRIxPTR ", "
                 "CodeCaveEnd: 0x%" PRIxPTR ", JumpSize: %d",
                 patch.modNameStr, patch_address, jump_target, code_cave_end, jump_size);
        return;
    }

    if (cheat_address == nullptr) {
        LOG_ERROR(Loader, "Failed to get address for patch %s", patch.modNameStr);
        return;
    }

    // The bytes, two hex digits each; a pair that is not hex writes 0, as strtol read it
    u8* const byte_patch = static_cast<u8*>(malloc(value_length / 2 + 1));
    if (!byte_patch) {
        return;
    }
    size_t count = 0;
    for (size_t i = 0; i + 1 < value_length + 1 && patch.valueStr[i]; i += 2) {
        const char pair[3] = {patch.valueStr[i], patch.valueStr[i + 1], '\0'};
        byte_patch[count++] = static_cast<u8>(strtol(pair, nullptr, 16));
    }
    if (patch.littleEndian) {
        for (size_t i = 0; i < count / 2; ++i) {
            const u8 byte = byte_patch[i];
            byte_patch[i] = byte_patch[count - 1 - i];
            byte_patch[count - 1 - i] = byte;
        }
    }

    memcpy(cheat_address, byte_patch, count);
    free(byte_patch);

    LOG_INFO(Loader, "Applied patch: %s, Offset: 0x%" PRIxPTR ", Value: %s", patch.modNameStr,
             reinterpret_cast<uintptr_t>(cheat_address), patch.valueStr);
}

uintptr_t PatternScan(const char* signature) {
    // The pattern's bytes, -1 where any will do: one at most for each character
    const size_t length = strlen(signature);
    s32* const pattern = static_cast<s32*>(malloc((length + 1) * sizeof(s32)));
    if (!pattern) {
        return 0;
    }
    size_t pattern_size = 0;
    for (const char* current = signature; current < signature + length; ++current) {
        if (*current == '?') {
            ++current;
            if (*current == '?') {
                ++current;
            }
            pattern[pattern_size++] = -1;
        } else {
            char* end = nullptr;
            pattern[pattern_size++] = static_cast<s32>(strtoul(current, &end, 16));
            current = end;
        }
    }

    const u8* const scan_bytes = reinterpret_cast<const u8*>(g_eboot_address);
    uintptr_t found = 0;
    for (u64 i = 0; pattern_size && i + pattern_size <= g_eboot_image_size && !found; ++i) {
        bool match = true;
        for (size_t j = 0; j < pattern_size; ++j) {
            if (pattern[j] != -1 && scan_bytes[i + j] != pattern[j]) {
                match = false;
                break;
            }
        }
        if (match) {
            found = reinterpret_cast<uintptr_t>(&scan_bytes[i]);
        }
    }
    free(pattern);
    return found;
}

} // namespace MemoryPatcher
