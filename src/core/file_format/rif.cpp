// SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <ranges>

#include "common/assert.h"
#include "common/io_file.h"
#include "common/logging/log.h"
#include "common/path_util.h"
#include "core/file_format/rif.h"
#include "core/file_format/rif_c.h"

bool RIF::Open(const std::filesystem::path& ridx_path, const std::filesystem::path& rifa_path) {
    // Both read where they lie
    const auto ridx = Common::FS::FileView::Map(ridx_path);
    if (!ridx) {
        return false;
    }
    const auto rifa = Common::FS::FileView::Map(rifa_path);
    if (!rifa) {
        return false;
    }
    ASSERT_MSG(!ridx->empty(), "RIDX file at %s is empty!", (ridx_path.string()).c_str());
    ASSERT_MSG(!rifa->empty(), "RIFA file at %s is empty!", (rifa_path.string()).c_str());
    return Open(ridx->Bytes(), rifa->Bytes());
}

bool RIF::Open(std::span<const u8> ridx_buffer, std::span<const u8> rifa_buffer) {
    const u8* ridx_data = ridx_buffer.data();
    const u8* rifa_data = rifa_buffer.data();

    service_id = "";
    license_only_entitlements.clear();

    RIDXHeader ridx_header{};
    std::memcpy(&ridx_header, ridx_data, sizeof(ridx_header));

    if (ridx_header.magic != RIDX_MAGIC) {
        LOG_ERROR(Core, "Invalid RIDX magic number");
        return false;
    }
    if (ridx_header.version != RIDX_VERSION) {
        LOG_ERROR(Core, "Unsupported RIDX version: 0x%08x", ridx_header.version);
        return false;
    }

    RIFAHeader rifa_header{};
    std::memcpy(&rifa_header, rifa_data, sizeof(rifa_header));

    if (rifa_header.magic != RIFA_MAGIC) {
        LOG_ERROR(Core, "Invalid RIFA magic number");
        return false;
    }

    service_id = std::string(ridx_header.service_id, sizeof(ridx_header.service_id));

    for (u32 i = 0; i < ridx_header.entry_count; i++) {
        RIDXEntry entry{};
        std::memcpy(&entry, ridx_data + sizeof(RIDXHeader) + i * sizeof(RIDXEntry), sizeof(entry));

        if (entry.rif_size != sizeof(RIFData)) {
            LOG_ERROR(Core, "Invalid RIF size at %u", i);
            continue;
        }

        RIFData rif{};
        std::memcpy(&rif, rifa_data + entry.rif_offset, sizeof(rif));

        if (rif.magic != RIF_MAGIC) {
            LOG_ERROR(Core, "Invalid RIF magic number at %u", i);
            continue;
        }

        // License-only content has a specific type ID. Ignore anything else.
        if (rif.content_type == RIF_CONTENT_TYPE_PS4AL) {
            std::string entitlement_id(entry.entitlement_label, sizeof(entry.entitlement_label));
            license_only_entitlements.emplace(entitlement_id, rif);
        }
    }

    return true;
}

extern "C" bool rif_for_each_license_only_entitlement(
    const char* ridx_path, const char* rifa_path, void (*opened)(void* ctx),
    void (*use)(const char* label, size_t length, const u8* key, void* ctx), void* ctx) {
    static_assert(sizeof(RIFEntitlementKey::data) == RIF_ENTITLEMENT_KEY_SIZE);
    RIF rif;
    if (!rif.Open(Common::FS::PathFromUTF8String(ridx_path),
                  Common::FS::PathFromUTF8String(rifa_path))) {
        return false;
    }
    opened(ctx);
    for (const auto& entitlement : rif.GetLicenseOnlyEntitlements()) {
        use(entitlement.first.c_str(), entitlement.first.size(), entitlement.second.data, ctx);
    }
    return true;
}
