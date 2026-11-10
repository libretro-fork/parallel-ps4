// SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <algorithm>
#include <cctype>
#include <cstring>

#include <encodings/deflate.h>
#include <openssl/aes.h>
#include <openssl/bn.h>
#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <openssl/rsa.h>
#include <openssl/sha.h>

#include "common/endian.h"
#include "common/string_util.h"
#include "core/file_format/pfs.h"
#include "pkg_reader.h"

namespace Libretro {

namespace {

constexpr u32 PkgMagic = 0x7F434E54;
constexpr u32 PfscMagic = 0x43534650;
constexpr u64 SectorSize = 0x1000;
constexpr u64 BlockSize = 0x10000;
constexpr std::size_t CachedBlocks = 16;

// The PKG header as upstream's pkg.h had it.
struct PkgHeader {
    u32_be magic;
    u32_be pkg_type;
    u32_be pkg_0x8;
    u32_be pkg_file_count;
    u32_be pkg_table_entry_count;
    u16_be pkg_sc_entry_count;
    u16_be pkg_table_entry_count_2;
    u32_be pkg_table_entry_offset;
    u32_be pkg_sc_entry_data_size;
    u64_be pkg_body_offset;
    u64_be pkg_body_size;
    u64_be pkg_content_offset;
    u64_be pkg_content_size;
    u8 pkg_content_id[0x24];
    u8 pkg_padding[0xC];
    u32_be pkg_drm_type;
    u32_be pkg_content_type;
    u32_be pkg_content_flags;
    u32_be pkg_promote_size;
    u32_be pkg_version_date;
    u32_be pkg_version_hash;
    u32_be pkg_0x088;
    u32_be pkg_0x08C;
    u32_be pkg_0x090;
    u32_be pkg_0x094;
    u32_be pkg_iro_tag;
    u32_be pkg_drm_type_version;
    u8 pkg_zeroes_1[0x60];
    u8 digest_entries1[0x20];
    u8 digest_entries2[0x20];
    u8 digest_table_digest[0x20];
    u8 digest_body_digest[0x20];
    u8 pkg_zeroes_2[0x280];
    u32_be pkg_0x400;
    u32_be pfs_image_count;
    u64_be pfs_image_flags;
    u64_be pfs_image_offset;
    u64_be pfs_image_size;
    u64_be mount_image_offset;
    u64_be mount_image_size;
    u64_be pkg_size;
    u32_be pfs_signed_size;
    u32_be pfs_cache_size;
    u8 pfs_image_digest[0x20];
    u8 pfs_signed_digest[0x20];
    u64_be pfs_split_size_nth_0;
    u64_be pfs_split_size_nth_1;
    u8 pkg_zeroes_3[0xB50];
    u8 pkg_digest[0x20];
};

// An entry of the PKG's table, as it is in the file (big-endian).
struct PkgEntry {
    u32_be id;
    u32_be filename_offset;
    u32_be flags1;
    u32_be flags2;
    u32_be offset;
    u32_be size;
    u64_be padding;
};
static_assert(sizeof(PkgEntry) == 32);

bool ReadHeader(const Common::FS::IOFile& file, PkgHeader& header, std::string& error) {
    if (!file.IsOpen()) {
        error = "the file could not be opened";
        return false;
    }
    if (file.ReadRaw<u8>(&header, sizeof(header)) != sizeof(header) ||
        header.magic != PkgMagic) {
        error = "not a PS4 PKG";
        return false;
    }
    return true;
}

bool ReadEntries(const Common::FS::IOFile& file, const PkgHeader& header,
                 std::vector<PkgEntry>& entries) {
    entries.resize(header.pkg_table_entry_count);
    return file.Seek(header.pkg_table_entry_offset) &&
           file.ReadRaw<u8>(entries.data(), entries.size() * sizeof(PkgEntry)) ==
               entries.size() * sizeof(PkgEntry);
}

// RSAES-PKCS1-v1_5 with one of the PKG keys; the first 32 bytes of the message.
bool RsaDecrypt(const std::vector<u8>& modulus, const std::vector<u8>& public_exponent,
                const std::vector<u8>& private_exponent, const u8* ciphertext,
                std::array<u8, 32>& out) {
    if (modulus.size() != 256) {
        return false;
    }
    RSA* rsa = RSA_new();
    if (!rsa) {
        return false;
    }
    BIGNUM* n = BN_bin2bn(modulus.data(), static_cast<int>(modulus.size()), nullptr);
    BIGNUM* e = BN_bin2bn(public_exponent.data(), static_cast<int>(public_exponent.size()), nullptr);
    BIGNUM* d =
        BN_bin2bn(private_exponent.data(), static_cast<int>(private_exponent.size()), nullptr);
    if (!n || !e || !d || RSA_set0_key(rsa, n, e, d) != 1) {
        BN_free(n);
        BN_free(e);
        BN_free(d);
        RSA_free(rsa);
        return false;
    }
    std::array<u8, 256> message{};
    const int length = RSA_private_decrypt(256, ciphertext, message.data(), rsa, RSA_PKCS1_PADDING);
    RSA_free(rsa);
    if (length < static_cast<int>(out.size())) {
        return false;
    }
    std::memcpy(out.data(), message.data(), out.size());
    return true;
}

// The key and IV of an entry: SHA-256 of the entry's table record and DK3.
std::array<u8, 32> EntryIvKey(const PkgEntry& entry, const std::array<u8, 32>& dk3) {
    std::array<u8, 64> input{};
    std::memcpy(input.data(), &entry, sizeof(entry));
    std::memcpy(input.data() + sizeof(entry), dk3.data(), dk3.size());
    std::array<u8, 32> ivkey{};
    SHA256(input.data(), input.size(), ivkey.data());
    return ivkey;
}

// AES-128-CBC without padding: the IV is the first half of ivkey, the key the second.
bool AesCbcDecrypt(const std::array<u8, 32>& ivkey, const u8* in, u8* out, std::size_t size) {
    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) {
        return false;
    }
    int written = 0;
    const bool ok =
        EVP_DecryptInit_ex(ctx, EVP_aes_128_cbc(), nullptr, ivkey.data() + 16, ivkey.data()) ==
            1 &&
        EVP_CIPHER_CTX_set_padding(ctx, 0) == 1 &&
        EVP_DecryptUpdate(ctx, out, &written, in, static_cast<int>(size)) == 1;
    EVP_CIPHER_CTX_free(ctx);
    return ok;
}

// A block's zlib stream, inflated straight into the block. Each thread that
// reads blocks keeps one inflater, reset per block rather than made each time.
void Inflate(const u8* in, std::size_t in_size, u8* out, std::size_t out_size) {
  struct Inflater {
    void *stream = rinflate_new(15);
    ~Inflater() { rinflate_free(stream); }
  };
  thread_local Inflater inflater;
  if (!inflater.stream) {
    return;
  }
  rinflate_reset(inflater.stream, 15);
  rinflate_set_in(inflater.stream, in, in_size);
  rinflate_set_out(inflater.stream, out, out_size);
  std::size_t taken = 0;
  std::size_t made = 0;
  int result = RDEFLATE_PROCESS_NEXT;
  while (result == RDEFLATE_PROCESS_NEXT && taken < in_size &&
         made < out_size) {
    std::size_t read = 0;
    std::size_t wrote = 0;
    result = rinflate_process(inflater.stream, &read, &wrote);
    taken += read;
    made += wrote;
    if (read == 0 && wrote == 0) {
      break;
    }
  }
}

// The text values of a param.sfo. Not PSF: this runs in the frontend's process too, where
// the emulator's log is not set up.
std::unordered_map<std::string, std::string> SfoStrings(const std::vector<u8>& sfo) {
    std::unordered_map<std::string, std::string> values;
    const auto u32_at = [&](std::size_t at) -> u32 {
        return at + 4 <= sfo.size() ? u32(sfo[at]) | u32(sfo[at + 1]) << 8 |
                                          u32(sfo[at + 2]) << 16 | u32(sfo[at + 3]) << 24
                                    : 0;
    };
    const auto text_at = [&](std::size_t at, std::size_t max) {
        std::string text;
        for (; at < sfo.size() && max && sfo[at]; at++, max--) {
            text += static_cast<char>(sfo[at]);
        }
        return text;
    };
    if (sfo.size() < 0x14 || u32_at(0) != 0x46535000) { // "\0PSF"
        return values;
    }
    const u32 keys = u32_at(8);
    const u32 data = u32_at(12);
    const u32 count = u32_at(16);
    for (u32 i = 0; i < count; i++) {
        const std::size_t entry = 0x14 + std::size_t(i) * 0x10;
        if (entry + 0x10 > sfo.size()) {
            break;
        }
        const u32 key_offset = u32(sfo[entry]) | u32(sfo[entry + 1]) << 8;
        const bool text = sfo[entry + 2] == 0x04 && sfo[entry + 3] == 0x02; // utf-8, ends in 0
        if (text) {
            values[text_at(keys + key_offset, 64)] =
                text_at(data + std::size_t(u32_at(entry + 12)), u32_at(entry + 4));
        }
    }
    return values;
}

std::string Normalize(std::string_view rel) {
    std::string path = std::filesystem::path(rel).lexically_normal().generic_string();
    while (!path.empty() && path.front() == '/') {
        path.erase(0, 1);
    }
    while (!path.empty() && path.back() == '/') {
        path.pop_back();
    }
    if (path == ".") {
        path.clear();
    }
    return path;
}

} // namespace

std::filesystem::path PkgKeysFile(const std::filesystem::path& user_dir) {
    return user_dir / "pkg_keys.txt";
}

bool LoadPkgKeys(const std::filesystem::path& path, PkgKeys& keys, std::string& error) {
    Common::FS::IOFile file(path, Common::FS::FileAccessMode::Read);
    if (!file.IsOpen()) {
        error = "PKG content needs the PKG keys in " + path.string();
        return false;
    }
    std::string text(file.GetSize(), '\0');
    file.ReadRaw<char>(text.data(), text.size());
    keys = {};
    const std::pair<std::string_view, std::vector<u8>*> names[] = {
        {"derived_key3_modulus", &keys.derived_key3_modulus},
        {"derived_key3_public_exponent", &keys.derived_key3_public_exponent},
        {"derived_key3_private_exponent", &keys.derived_key3_private_exponent},
        {"fake_modulus", &keys.fake_modulus},
        {"fake_public_exponent", &keys.fake_public_exponent},
        {"fake_private_exponent", &keys.fake_private_exponent},
    };
    std::size_t start = 0;
    while (start < text.size()) {
        std::size_t end = text.find('\n', start);
        if (end == std::string::npos) {
            end = text.size();
        }
        const std::string line = text.substr(start, end - start);
        start = end + 1;
        const auto equals = line.find('=');
        if (line.empty() || line[0] == '#' || equals == std::string::npos) {
            continue;
        }
        std::string name;
        for (const char c : line.substr(0, equals)) {
            if (!std::isspace(static_cast<unsigned char>(c))) {
                name += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            }
        }
        std::string hex;
        for (const char c : line.substr(equals + 1)) {
            if (std::isxdigit(static_cast<unsigned char>(c))) {
                hex += c;
            }
        }
        if (hex.size() >= 2 && hex[0] == '0' && (hex[1] == 'x' || hex[1] == 'X')) {
            hex.erase(0, 2);
        }
        if (hex.size() % 2) {
            hex.insert(hex.begin(), '0');
        }
        for (const auto& [key, value] : names) {
            if (name == key) {
                value->clear();
                for (std::size_t i = 0; i < hex.size(); i += 2) {
                    value->push_back(static_cast<u8>(std::stoi(hex.substr(i, 2), nullptr, 16)));
                }
            }
        }
    }
    for (auto* exponent : {&keys.derived_key3_public_exponent, &keys.fake_public_exponent}) {
        if (exponent->empty()) {
            *exponent = {0x01, 0x00, 0x01};
        }
    }
    for (const auto& [key, value] : names) {
        if (value->empty()) {
            error = std::string{key} + " is missing from " + path.string();
            return false;
        }
    }
    if (keys.derived_key3_modulus.size() != 256 || keys.fake_modulus.size() != 256) {
        error = "a modulus in " + path.string() + " is not 256 bytes";
        return false;
    }
    return true;
}

bool ReadPkgSummary(const std::filesystem::path& path, PkgSummary& summary, std::string& error) {
    Common::FS::IOFile file(path, Common::FS::FileAccessMode::Read);
    PkgHeader header{};
    if (!ReadHeader(file, header, error)) {
        return false;
    }
    summary.content_id.assign(reinterpret_cast<const char*>(header.pkg_content_id),
                              strnlen(reinterpret_cast<const char*>(header.pkg_content_id),
                                      sizeof(header.pkg_content_id)));
    summary.title_id = summary.content_id.size() >= 16 ? summary.content_id.substr(7, 9) : "";

    std::vector<PkgEntry> entries;
    if (!ReadEntries(file, header, entries)) {
        error = "the PKG's table could not be read";
        return false;
    }
    for (const PkgEntry& entry : entries) {
        if (entry.id != 0x1000) { // param.sfo, which is not encrypted
            continue;
        }
        std::vector<u8> sfo(entry.size);
        if (!file.Seek(entry.offset) || file.ReadRaw<u8>(sfo.data(), sfo.size()) != sfo.size()) {
            error = "the PKG's param.sfo could not be read";
            return false;
        }
        auto values = SfoStrings(sfo);
        summary.category = values["CATEGORY"];
        summary.app_ver = values["APP_VER"];
        if (!values["TITLE_ID"].empty()) {
            summary.title_id = values["TITLE_ID"];
        }
        return true;
    }
    error = "the PKG has no param.sfo";
    return false;
}

bool PkgReader::Open(const std::filesystem::path& pkg_path,
                     const std::filesystem::path& keys_file, std::string& error) {
    path = pkg_path;
    file.Open(path, Common::FS::FileAccessMode::Read);
    PkgHeader header{};
    if (!ReadHeader(file, header, error)) {
        return false;
    }
    if (header.pkg_size > file.GetSize()) {
        error = "the PKG is shorter than its header says (an incomplete download?)";
        return false;
    }
    pfs_image_offset = header.pfs_image_offset;

    PkgKeys rsa;
    if (!LoadPkgKeys(keys_file, rsa, error)) {
        return false;
    }

    std::vector<PkgEntry> entries;
    if (!ReadEntries(file, header, entries)) {
        error = "the PKG's table could not be read";
        return false;
    }

    // DK3 from the entry keys, then EKPFS from the image key: the key of the PFS.
    std::array<u8, 32> ekpfs{};
    bool have_dk3 = false;
    bool have_ekpfs = false;
    for (const PkgEntry& entry : entries) {
        if (entry.id == 0x10) {
            // A seed digest (32), 7 digests (32 each), then 7 keys (256 each): DK3 is the 4th.
            std::array<u8, 256> key{};
            if (file.Seek(static_cast<u64>(entry.offset) + 32 + 7 * 32 + 3 * 256) &&
                file.ReadRaw<u8>(key.data(), key.size()) == key.size()) {
                have_dk3 = RsaDecrypt(rsa.derived_key3_modulus, rsa.derived_key3_public_exponent,
                                      rsa.derived_key3_private_exponent, key.data(), dk3);
            }
        }
    }
    for (const PkgEntry& entry : entries) {
        if (entry.id == 0x20 && have_dk3) {
            std::array<u8, 256> image_key{};
            std::array<u8, 256> decrypted{};
            if (file.Seek(entry.offset) &&
                file.ReadRaw<u8>(image_key.data(), image_key.size()) == image_key.size() &&
                AesCbcDecrypt(EntryIvKey(entry, dk3), image_key.data(), decrypted.data(),
                              decrypted.size())) {
                have_ekpfs = RsaDecrypt(rsa.fake_modulus, rsa.fake_public_exponent,
                                        rsa.fake_private_exponent, decrypted.data(), ekpfs);
            }
        }
    }
    if (!have_dk3 || !have_ekpfs) {
        error = "the keys in pkg_keys.txt do not open this PKG (not a fake-signed PKG?)";
        return false;
    }

    // The data and tweak keys: HMAC-SHA256 with EKPFS of the index 1 and the PFS's seed.
    std::array<u8, 20> input{};
    input[0] = 1;
    if (!file.Seek(pfs_image_offset + 0x370) || file.ReadRaw<u8>(input.data() + 4, 16) != 16) {
        error = "the PFS's seed could not be read";
        return false;
    }
    std::array<u8, 32> keys{};
    unsigned int keys_size = 0;
    HMAC(EVP_sha256(), ekpfs.data(), static_cast<int>(ekpfs.size()), input.data(), input.size(),
         keys.data(), &keys_size);
    std::memcpy(tweak_key.data(), keys.data(), 16);
    std::memcpy(data_key.data(), keys.data() + 16, 16);

    if (!ParsePfs(error)) {
        return false;
    }

    // The PKG's own entries are the files of sce_sys the PFS does not have, as upstream
    // extracted them there.
    for (const PkgEntry& entry : entries) {
        const std::string_view name = PkgEntryName(entry.id);
        const std::string rel = "sce_sys/" + (name.empty() ? std::to_string(u32(entry.id))
                                                           : std::string{name});
        if (nodes.contains(rel)) {
            continue;
        }
        Node node;
        node.entry = true;
        node.entry_id = entry.id;
        node.entry_offset = entry.offset;
        node.size = entry.size;
        AddNode(rel, std::move(node));
    }
    return true;
}

void PkgReader::AddNode(const std::string& rel, Node node) {
    // Its folders, up to the root.
    std::string child = rel;
    while (!child.empty()) {
        const auto slash = child.find_last_of('/');
        const std::string parent = slash == std::string::npos ? "" : child.substr(0, slash);
        const std::string name = slash == std::string::npos ? child : child.substr(slash + 1);
        auto [it, added] = nodes.try_emplace(parent);
        it->second.directory = true;
        if (std::find(it->second.children.begin(), it->second.children.end(), name) ==
            it->second.children.end()) {
            it->second.children.push_back(name);
        }
        if (!added && parent != child) {
            break;
        }
        child = parent;
    }
    lower[Common::ToLower(rel)] = rel;
    auto& slot = nodes[rel];
    if (slot.directory && node.directory) {
        return; // a folder already made for one of its files
    }
    if (node.directory) {
        node.children = std::move(slot.children);
    }
    slot = std::move(node);
}

bool PkgReader::DecryptSectors(u8* data, u64 size, u64 first_sector) {
    // XTS-AES-128 over sectors of 0x1000, the sector number as the tweak.
    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) {
        return false;
    }
    std::array<u8, 32> key{};
    std::memcpy(key.data(), data_key.data(), 16);
    std::memcpy(key.data() + 16, tweak_key.data(), 16);
    bool ok = true;
    for (u64 done = 0; ok && done + SectorSize <= size; done += SectorSize) {
        std::array<u8, 16> iv{};
        const u64 sector = first_sector + done / SectorSize;
        for (int i = 0; i < 8; i++) {
            iv[i] = static_cast<u8>(sector >> (8 * i));
        }
        int written = 0;
        ok = EVP_DecryptInit_ex(ctx, EVP_aes_128_xts(), nullptr, key.data(), iv.data()) == 1 &&
             EVP_DecryptUpdate(ctx, data + done, &written, data + done,
                               static_cast<int>(SectorSize)) == 1;
    }
    EVP_CIPHER_CTX_free(ctx);
    return ok;
}

const std::vector<u8>* PkgReader::Block(u32 index) {
    for (auto it = cache.begin(); it != cache.end(); ++it) {
        if (it->first == index) {
            cache.splice(cache.begin(), cache, it);
            return &cache.front().second;
        }
    }
    if (index + 1 >= block_map.size()) {
        return nullptr;
    }
    const u64 offset = block_map[index];
    const u64 size = block_map[index + 1] - offset;
    // The sectors the block is in: it need not start at one.
    const u64 start = pfsc_offset + offset;
    const u64 skip = start % SectorSize;
    const u64 span = (skip + size + SectorSize - 1) / SectorSize * SectorSize;
    std::vector<u8> sectors(span);
    if (!file.Seek(pfs_image_offset + start - skip)) {
        return nullptr;
    }
    file.ReadRaw<u8>(sectors.data(), sectors.size()); // a short read at the end stays zeroed
    if (!DecryptSectors(sectors.data(), sectors.size(), (start - skip) / SectorSize)) {
        return nullptr;
    }

    std::vector<u8> block(BlockSize);
    if (size >= BlockSize) {
        std::memcpy(block.data(), sectors.data() + skip, BlockSize); // stored as it is
    } else {
        Inflate(sectors.data() + skip, size, block.data(), block.size());
    }
    if (cache.size() >= CachedBlocks) {
        cache.pop_back();
    }
    cache.emplace_front(index, std::move(block));
    return &cache.front().second;
}

bool PkgReader::ParsePfs(std::string& error) {
    // The PFSC image (the compressed one) starts at a 64 KiB boundary from 0x20000.
    pfsc_offset = 0;
    const u64 image_size = file.GetSize() - pfs_image_offset;
    for (u64 at = 0x20000; at + SectorSize <= image_size && at < 0x1000000; at += BlockSize) {
        std::array<u8, SectorSize> sector{};
        if (!file.Seek(pfs_image_offset + at) ||
            file.ReadRaw<u8>(sector.data(), sector.size()) != sector.size() ||
            !DecryptSectors(sector.data(), sector.size(), at / SectorSize)) {
            break;
        }
        u32 magic = 0;
        std::memcpy(&magic, sector.data(), sizeof(magic));
        if (magic == PfscMagic) {
            pfsc_offset = at;
            break;
        }
    }
    if (pfsc_offset == 0) {
        error = "the PKG's PFS image has no PFSC";
        return false;
    }

    PFSCHdr header{};
    {
        std::array<u8, SectorSize> sector{};
        file.Seek(pfs_image_offset + pfsc_offset);
        file.ReadRaw<u8>(sector.data(), sector.size());
        DecryptSectors(sector.data(), sector.size(), pfsc_offset / SectorSize);
        std::memcpy(&header, sector.data(), sizeof(header));
    }
    if (header.block_sz2 <= 0 || header.data_length <= 0) {
        error = "the PKG's PFSC header is not valid";
        return false;
    }
    const u64 num_blocks = static_cast<u64>(header.data_length / header.block_sz2);
    // The block map: one offset into the PFSC image per block, and one past the last.
    {
        const u64 map_start = pfsc_offset + static_cast<u64>(header.block_offsets);
        const u64 map_bytes = (num_blocks + 1) * 8;
        const u64 skip = map_start % SectorSize;
        const u64 span = (skip + map_bytes + SectorSize - 1) / SectorSize * SectorSize;
        std::vector<u8> sectors(span);
        file.Seek(pfs_image_offset + map_start - skip);
        file.ReadRaw<u8>(sectors.data(), sectors.size());
        DecryptSectors(sectors.data(), sectors.size(), (map_start - skip) / SectorSize);
        block_map.resize(num_blocks + 1);
        std::memcpy(block_map.data(), sectors.data() + skip, map_bytes);
    }

    // The inodes and the directory entries, read as upstream's PKG::Extract did.
    std::vector<Inode> inodes;
    std::unordered_map<u32, std::string> paths; // inode -> path
    std::string current_dir;
    u32 inode_count = 0;
    int counter = 0;
    bool dinode_reached = false;
    bool uroot_reached = false;
    // Room past the block for a Dirent read at its end.
    std::vector<u8> data(BlockSize + sizeof(Dirent));
    for (u64 i = 0; i < num_blocks; i++) {
        const auto* block = Block(static_cast<u32>(i));
        if (!block) {
            break;
        }
        std::memcpy(data.data(), block->data(), BlockSize);
        if (i == 0) {
            std::memcpy(&inode_count, data.data() + 0x30, 4);
        }
        const u64 inode_blocks = (static_cast<u64>(inode_count) * 0xA8 + BlockSize - 1) / BlockSize;
        if (i >= 1 && i <= inode_blocks) {
            for (u64 p = 0; p + sizeof(Inode) <= BlockSize; p += 0xA8) {
                Inode node;
                std::memcpy(&node, data.data() + p, sizeof(node));
                if (node.Mode == 0) {
                    break;
                }
                inodes.push_back(node);
            }
        }

        if (std::string_view(reinterpret_cast<const char*>(data.data()) + 0x10, 15) ==
            "flat_path_table") {
            uroot_reached = true;
        }
        if (uroot_reached) {
            for (u64 j = 0; j < BlockSize;) {
                Dirent dirent;
                std::memcpy(&dirent, data.data() + j, sizeof(dirent));
                if (dirent.ino != 0) {
                    counter++;
                } else {
                    // The root is the inode after the superroot's entries.
                    paths[counter] = "";
                    uroot_reached = false;
                    break;
                }
                if (dirent.entsize <= 0) {
                    break;
                }
                j += dirent.entsize;
            }
        }

        if (data[0x10] == '.' &&
            std::string_view(reinterpret_cast<const char*>(data.data()) + 0x28, 2) == "..") {
            dinode_reached = true;
        }
        bool end_reached = false;
        if (dinode_reached) {
            for (u64 j = 0; j < BlockSize;) {
                Dirent dirent;
                std::memcpy(&dirent, data.data() + j, sizeof(dirent));
                if (dirent.ino == 0 || dirent.entsize <= 0 || dirent.namelen < 0 ||
                    dirent.namelen > 255) {
                    break;
                }
                const std::string name(dirent.name, dirent.namelen);
                const u32 ino = static_cast<u32>(dirent.ino);
                if (dirent.type == PFS_CURRENT_DIR) {
                    current_dir = paths[ino];
                } else if (dirent.type == PFS_FILE || dirent.type == PFS_DIR) {
                    const std::string rel = current_dir.empty() ? name : current_dir + "/" + name;
                    paths[ino] = rel;
                    Node node;
                    node.directory = dirent.type == PFS_DIR;
                    if (!node.directory && ino < inodes.size()) {
                        node.size = static_cast<u64>(inodes[ino].Size);
                        node.first_block = inodes[ino].loc;
                        node.blocks = inodes[ino].Blocks;
                    }
                    AddNode(rel, std::move(node));
                    counter++;
                    if (static_cast<u32>(counter + 1) == inode_count) {
                        end_reached = true;
                    }
                }
                j += dirent.entsize;
            }
            if (end_reached) {
                break;
            }
        }
    }
    cache.clear();
    if (!nodes.contains("eboot.bin") && !nodes.contains("sce_sys/param.sfo")) {
        error = "no files were found in the PKG's PFS";
        return false;
    }
    return true;
}

std::vector<std::string> PkgReader::Files() const {
    std::vector<std::string> files;
    for (const auto& [rel, node] : nodes) {
        if (!node.directory) {
            files.push_back(rel);
        }
    }
    std::sort(files.begin(), files.end());
    return files;
}

const PkgReader::Node* PkgReader::Find(std::string_view rel) const {
    const std::string normal = Normalize(rel);
    if (const auto it = nodes.find(normal); it != nodes.end()) {
        return &it->second;
    }
    if (const auto it = lower.find(Common::ToLower(normal)); it != lower.end()) {
        if (const auto node = nodes.find(it->second); node != nodes.end()) {
            return &node->second;
        }
    }
    return nullptr;
}

bool PkgReader::ReadEntry(const Node& node, std::vector<u8>& out) {
    out.resize(node.size);
    if (!file.Seek(node.entry_offset) || file.ReadRaw<u8>(out.data(), out.size()) != out.size()) {
        return false;
    }
    // license.dat, license.info, nptitle.dat and npbind.dat are encrypted.
    if (node.entry_id >= 0x400 && node.entry_id <= 0x403) {
        PkgEntry record{};
        // The key comes from the entry's table record, which is read again here.
        PkgHeader header{};
        std::string error;
        std::vector<PkgEntry> entries;
        if (!file.Seek(0) || !ReadHeader(file, header, error) ||
            !ReadEntries(file, header, entries)) {
            return false;
        }
        for (const PkgEntry& entry : entries) {
            if (entry.id == node.entry_id) {
                record = entry;
            }
        }
        const std::size_t whole = out.size() / 16 * 16;
        std::vector<u8> plain(whole);
        if (whole && AesCbcDecrypt(EntryIvKey(record, dk3), out.data(), plain.data(), whole)) {
            std::memcpy(out.data(), plain.data(), whole);
        }
    }
    return true;
}

u64 PkgReader::Read(const Node& node, u64 offset, u64 size, void* dst) {
    if (node.directory || offset >= node.size) {
        return 0;
    }
    size = std::min(size, node.size - offset);
    std::scoped_lock lock{mutex};
    if (node.entry) {
        std::vector<u8> data;
        if (!ReadEntry(node, data)) {
            return 0;
        }
        std::memcpy(dst, data.data() + offset, size);
        return size;
    }
    u8* out = static_cast<u8*>(dst);
    u64 done = 0;
    while (done < size) {
        const u64 at = offset + done;
        const u64 index = at / BlockSize;
        if (index >= node.blocks) {
            break;
        }
        const auto* block = Block(node.first_block + static_cast<u32>(index));
        if (!block) {
            break;
        }
        const u64 within = at % BlockSize;
        const u64 count = std::min(size - done, BlockSize - within);
        std::memcpy(out + done, block->data() + within, count);
        done += count;
    }
    return done;
}

} // namespace Libretro
