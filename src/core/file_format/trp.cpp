// SPDX-FileCopyrightText: Copyright 2024-2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <cstring>
#include <system_error>

#include "common/aes.h"
#include "common/io_file.h"
#include "common/key_manager.h"
#include "common/logging/log.h"
#include "common/path_util.h"
#include "core/file_format/trp.h"

static void DecryptEFSM(std::span<const u8, 16> trophyKey, std::span<const u8, 16> NPcommID,
                        std::span<const u8, 16> efsmIv, std::span<const u8> ciphertext,
                        std::span<u8> decrypted) {
    // Step 1: Encrypt NPcommID
    std::array<u8, 16> trophyIv{};
    std::array<u8, 16> trpKey;
    // Convert spans to pointers for the aes functions
    aes::encrypt_cbc(NPcommID.data(), NPcommID.size(), trophyKey.data(), trophyKey.size(),
                     trophyIv.data(), trpKey.data(), trpKey.size(), false);

    // Step 2: Decrypt EFSM
    aes::decrypt_cbc(ciphertext.data(), ciphertext.size(), trpKey.data(), trpKey.size(),
                     const_cast<u8*>(efsmIv.data()), decrypted.data(), decrypted.size(), nullptr);
}

TRP::TRP() = default;
TRP::~TRP() = default;

static void removePadding(std::vector<u8>& vec) {
    for (auto it = vec.rbegin(); it != vec.rend(); ++it) {
        if (*it == '>') {
            size_t pos = std::distance(vec.begin(), it.base());
            vec.resize(pos);
            break;
        }
    }
}

bool TRP::Extract(const std::filesystem::path& trophyPath, std::string npCommId,
                  const std::filesystem::path& outputPath) {
    if (trophyPath.extension() != ".trp") {
        return false;
    }
    const auto file = Common::FS::FileView::Map(trophyPath);
    if (!file) {
        LOG_ERROR(Common_Filesystem, "Unable to open trophy file: {}", trophyPath.string());
        return false;
    }
    return Extract(file->Bytes(), std::move(npCommId), outputPath);
}

bool TRP::Extract(std::span<const u8> trp, std::string npCommId,
                  const std::filesystem::path& outputPath) {
    // Retrieve trophy key
    const key_manager_key& user_key_vec = key_manager_get_keys()->release_trophy_key;

    if (user_key_vec.size != 16) {
        LOG_INFO(Common_Filesystem, "Trophy decryption key is not specified");
        return false;
    }

    std::array<u8, 16> user_key{};
    std::memcpy(user_key.data(), user_key_vec.data, user_key.size());

    s32 trpFileIndex = 0;
    bool success = true;
    const auto cleanupOnFailure = [&outputPath] {
        std::error_code ec;
        std::filesystem::remove_all(outputPath, ec);
    };
    try {
        TrpHeader header;
        if (trp.size() < sizeof(header)) {
            LOG_ERROR(Common_Filesystem, "Failed to read TRP header");
            return false;
        }
        std::memcpy(&header, trp.data(), sizeof(header));

        if (header.magic != TRP_MAGIC) {
            LOG_ERROR(Common_Filesystem,
                      "Wrong trophy magic number: got {:#010x}, expected {:#010x}",
                      static_cast<u32>(header.magic), TRP_MAGIC);
            return false;
        }

        s64 seekPos = sizeof(TrpHeader);
        std::error_code dir_ec;
        std::filesystem::create_directories(outputPath / "Icons", dir_ec);
        if (!dir_ec) {
            std::filesystem::create_directories(outputPath / "Xml", dir_ec);
        }
        if (dir_ec) {
            LOG_ERROR(Common_Filesystem, "Failed to create output directories for {}: {}", npCommId,
                      dir_ec.message());
            cleanupOnFailure();
            return false;
        }

        // Process each entry in the TRP file
        for (u32 i = 0; i < header.entry_num; i++) {
            TrpEntry entry;
            if (seekPos < 0 || static_cast<u64>(seekPos) + sizeof(entry) > trp.size()) {
                LOG_ERROR(Common_Filesystem, "Failed to read TRP entry");
                success = false;
                break;
            }
            std::memcpy(&entry, trp.data() + seekPos, sizeof(entry));
            seekPos += static_cast<s64>(header.entry_size);

            std::string_view name(entry.entry_name);

            if (entry.flag == ENTRY_FLAG_PNG) {
                if (!ProcessPngEntry(trp, entry, outputPath, name)) {
                    success = false;
                    // Continue with next entry
                }
            } else if (entry.flag == ENTRY_FLAG_ENCRYPTED_XML) {
                // Check if we have a valid NPCommID for decryption
                if (npCommId.size() >= 12 && npCommId[0] == 'N' && npCommId[1] == 'P') {
                    if (!ProcessEncryptedXmlEntry(trp, entry, outputPath, name, user_key,
                                                  npCommId)) {
                        success = false;
                        // Continue with next entry
                    }
                } else {
                    LOG_WARNING(Common_Filesystem,
                                "Skipping encrypted XML entry - invalid NPCommID");
                    // Skip this entry but continue
                }
            } else {
                LOG_DEBUG(Common_Filesystem, "Unknown entry flag: {} for {}",
                          static_cast<u32>(entry.flag), name);
            }
            trpFileIndex++;
        }

    } catch (const std::filesystem::filesystem_error& e) {
        LOG_CRITICAL(Common_Filesystem, "Filesystem error during trophy extraction: {}", e.what());
        cleanupOnFailure();
        return false;
    } catch (const std::exception& e) {
        LOG_CRITICAL(Common_Filesystem, "Error during trophy extraction: {}", e.what());
        cleanupOnFailure();
        return false;
    }

    if (!success) {
        cleanupOnFailure();
        return false;
    }

    LOG_INFO(Common_Filesystem, "Successfully extracted {} trophy files for {}", trpFileIndex,
             npCommId);
    return true;
}

/// The bytes of @entry inside @trp, or nothing when it reaches past the end
static std::span<const u8> EntryBytes(std::span<const u8> trp, const TrpEntry& entry) {
    const u64 pos = entry.entry_pos;
    const u64 len = entry.entry_len;
    if (pos > trp.size() || len > trp.size() - pos) {
        return {};
    }
    return trp.subspan(pos, len);
}

bool TRP::ProcessPngEntry(std::span<const u8> trp, const TrpEntry& entry,
                          const std::filesystem::path& outputPath, std::string_view name) {
    const auto icon = EntryBytes(trp, entry);
    if (icon.size() != entry.entry_len) {
        LOG_ERROR(Common_Filesystem, "Failed to read PNG data");
        return false;
    }

    // Written straight from the TRP
    auto outputFile = outputPath / "Icons" / name;
    size_t written = Common::FS::IOFile::WriteBytes(outputFile, icon);
    if (written != icon.size()) {
        LOG_ERROR(Common_Filesystem, "PNG write failed: wanted {} bytes, wrote {}", icon.size(),
                  written);
        return false;
    }

    return true;
}

bool TRP::ProcessEncryptedXmlEntry(std::span<const u8> trp, const TrpEntry& entry,
                                   const std::filesystem::path& outputPath, std::string_view name,
                                   const std::array<u8, 16>& user_key,
                                   const std::string& npCommId) {
    constexpr size_t IV_LEN = 16;

    const auto bytes = EntryBytes(trp, entry);
    if (bytes.size() != entry.entry_len) {
        LOG_ERROR(Common_Filesystem, "Failed to read encrypted XML data");
        return false;
    }
    if (entry.entry_len <= IV_LEN) {
        LOG_ERROR(Common_Filesystem, "Encrypted XML entry too small");
        return false;
    }

    // The IV, then the encrypted data, decrypted from where it lies
    std::array<u8, IV_LEN> esfmIv;
    std::memcpy(esfmIv.data(), bytes.data(), IV_LEN);
    const std::span<const u8> ESFM = bytes.subspan(IV_LEN);
    std::vector<u8> XML(ESFM.size());

    // Decrypt the data
    std::span<const u8, 16> key_span(user_key);

    // Convert npCommId string to span (pad or truncate to 16 bytes)
    std::array<u8, 16> npcommid_array{};
    size_t copy_len = std::min(npCommId.size(), npcommid_array.size());
    std::memcpy(npcommid_array.data(), npCommId.data(), copy_len);
    std::span<const u8, 16> npcommid_span(npcommid_array);

    DecryptEFSM(key_span, npcommid_span, esfmIv, ESFM, XML);

    // Remove padding
    removePadding(XML);

    // Create output filename
    std::string xml_name(entry.entry_name);
    size_t pos = xml_name.find("ESFM");
    if (pos != std::string::npos) {
        xml_name.replace(pos, 4, "XML");
    }

    auto outputFile = outputPath / "Xml" / xml_name;
    size_t written = Common::FS::IOFile::WriteBytes(outputFile, XML);
    if (written != XML.size()) {
        LOG_ERROR(Common_Filesystem, "XML write failed: wanted {} bytes, wrote {}", XML.size(),
                  written);
        return false;
    }

    return true;
}