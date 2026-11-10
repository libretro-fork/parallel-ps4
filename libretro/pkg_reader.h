// SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

// A PKG file read in place: its files are decrypted and inflated block by block as they
// are read, nothing is extracted. The format and the keys are those of upstream
// shadPS4's PKG code (src/core/file_format/pkg.cpp, src/core/crypto), which upstream
// removed (be22674f); the cryptography is LibreSSL's in the place of Crypto++. The two
// RSA keys are not in the core: they are read from a file the user puts in
// system/shadPS4/ (PkgKeysFile).

#pragma once

#include <array>
#include <filesystem>
#include <mutex>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "common/io_file.h"
#include "common/types.h"

namespace Libretro {

std::string_view PkgEntryName(u32 type);

// What a PKG holds, from its header and its param.sfo, without the PFS decrypted.
struct PkgSummary {
    std::string title_id;
    std::string content_id;
    std::string category; // "gd" a game, "gp" its update, "ac" additional content
    std::string app_ver;
};

bool ReadPkgSummary(const std::filesystem::path& path, PkgSummary& summary, std::string& error);

// The RSA keys a fake-signed PKG is opened with: big-endian numbers.
struct PkgKeys {
    std::vector<u8> derived_key3_modulus;
    std::vector<u8> derived_key3_public_exponent;
    std::vector<u8> derived_key3_private_exponent;
    std::vector<u8> fake_modulus;
    std::vector<u8> fake_public_exponent;
    std::vector<u8> fake_private_exponent;
};

// system/shadPS4/pkg_keys.txt: one "name = hex" a line, the names those of PkgKeys; a
// public exponent left out is 65537.
std::filesystem::path PkgKeysFile(const std::filesystem::path& user_dir);
bool LoadPkgKeys(const std::filesystem::path& file, PkgKeys& keys, std::string& error);

class PkgReader {
public:
    struct Node {
        bool directory = false;
        u64 size = 0;
        // A file of the PFS: its first block in the PFSC block map and its block count.
        u32 first_block = 0;
        u32 blocks = 0;
        // A file of the PKG's own entries (sce_sys/...): where it is in the PKG, and, for one
        // that is encrypted, its key and IV.
        bool entry = false;
        bool entry_encrypted = false;
        u32 entry_id = 0;
        u64 entry_offset = 0;
        std::array<u8, 32> entry_ivkey{};
        // A directory: the names in it.
        std::vector<std::string> children;
    };

    // keys_file is PkgKeysFile's. Nothing is logged: the core installs a PKG with it in the
    // frontend's process, where the emulator's log is not set up.
    bool Open(const std::filesystem::path& path, const std::filesystem::path& keys_file,
              std::string& error);

    const std::filesystem::path& Path() const {
        return path;
    }

    // rel is relative to the PKG's root, '/'-separated; the case does not have to match.
    const Node* Find(std::string_view rel) const;

    // The paths of all its files.
    std::vector<std::string> Files() const;

    // Reads up to size bytes of a file from offset straight into dst; returns the bytes read.
    u64 Read(const Node& node, u64 offset, u64 size, void* dst);

    PkgReader();
    ~PkgReader();
    PkgReader(const PkgReader&) = delete;
    PkgReader& operator=(const PkgReader&) = delete;

private:
    // Decrypts the PFS image's sectors from first_sector on, size bytes (whole sectors), from
    // where they lie in the mapped PKG into out; past the PKG's end it is zeros.
    bool ReadSectors(u64 first_sector, u64 size, u8* out);
    // A block of the PFSC image decoded into out (BlockSize bytes).
    bool DecodeBlock(u32 index, u8* out);
    // A block, decoded once into the cache; null when it cannot be read.
    const u8* Block(u32 index);
    u64 ReadEntry(const Node& node, u64 offset, u64 size, u8* dst);
    bool ParsePfs(std::string& error);
    void AddNode(const std::string& rel, Node node);

    std::filesystem::path path;
    // The PKG, mapped: its sectors are decrypted from where they lie.
    Common::FS::IOFile file;
    std::span<const u8> image;
    std::mutex mutex;
    // XTS-AES with the data and tweak keys, set once: per sector only the IV changes
    void* xts = nullptr;

    u64 pfs_image_offset = 0;
    u64 pfsc_offset = 0;
    std::vector<u64> block_map;
    std::array<u8, 32> dk3{};
    std::array<u8, 16> data_key{};
    std::array<u8, 16> tweak_key{};

    std::unordered_map<std::string, Node> nodes;     // by the path as the PKG has it
    std::unordered_map<std::string, std::string> lower; // lower-case path -> path

    // The blocks read last, in slots made once (with a zeroed tail, for the PFS walk that
    // reads a record at a block's end), and the sectors of the block being decoded.
    std::vector<u8> cache_memory;
    std::array<u32, 16> cache_index{};
    std::array<u64, 16> cache_used{};
    u64 cache_clock = 0;
    std::vector<u8> sector_scratch;
};

} // namespace Libretro
