// SPDX-FileCopyrightText: Copyright 2024-2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <cstring>

#include "playgo_chunk.h"

bool PlaygoFile::Open(const std::filesystem::path& filepath) {
    // Read where it lies
    const auto file = Common::FS::FileView::Map(filepath);
    return file && Open(file->Bytes());
}

bool PlaygoFile::Open(std::span<const u8> data) {
    if (data.size() < sizeof(PlaygoHeader)) {
        return false;
    }
    std::memcpy(&playgoHeader, data.data(), sizeof(PlaygoHeader));
    if (playgoHeader.magic != PLAYGO_MAGIC) {
        return false;
    }

    const auto load_chunk = [&](const chunk_t chunk, std::string& out) -> bool {
        if (chunk.offset + chunk.length > data.size()) {
            return false;
        }
        out.assign(reinterpret_cast<const char*>(data.data() + chunk.offset), chunk.length);
        return true;
    };

    std::string chunk_attrs_data, chunk_mchunks_data, chunk_labels_data, mchunk_attrs_data;
    if (!load_chunk(playgoHeader.chunk_attrs, chunk_attrs_data))
        return false;
    if (!load_chunk(playgoHeader.chunk_mchunks, chunk_mchunks_data))
        return false;
    if (!load_chunk(playgoHeader.chunk_labels, chunk_labels_data))
        return false;
    if (!load_chunk(playgoHeader.mchunk_attrs, mchunk_attrs_data))
        return false;

    chunks.resize(playgoHeader.chunk_count);

    auto chunk_attrs = reinterpret_cast<playgo_chunk_attr_entry_t*>(&chunk_attrs_data[0]);
    auto chunk_mchunks = reinterpret_cast<u16*>(&chunk_mchunks_data[0]);
    auto chunk_labels = reinterpret_cast<char*>(&chunk_labels_data[0]);
    auto mchunk_attrs = reinterpret_cast<playgo_mchunk_attr_entry_t*>(&mchunk_attrs_data[0]);

    for (u16 i = 0; i < playgoHeader.chunk_count; i++) {
        chunks[i].req_locus = chunk_attrs[i].req_locus;
        chunks[i].language_mask = chunk_attrs[i].language_mask;
        chunks[i].label_name = std::string(chunk_labels + chunk_attrs[i].label_offset);

        u64 total_size = 0;
        u16 mchunk_count = chunk_attrs[i].mchunk_count;
        if (mchunk_count != 0) {
            auto mchunks =
                reinterpret_cast<u16*>(((u8*)chunk_mchunks + chunk_attrs[i].mchunks_offset));
            for (u16 j = 0; j < mchunk_count; j++) {
                u16 mchunk_id = mchunks[j];
                total_size += mchunk_attrs[mchunk_id].size.size;
            }
        }
        chunks[i].total_size = total_size;
    }
    return true;
}
