// SPDX-FileCopyrightText: Copyright 2025-2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "common/elf_info.h"
#include "common/io_file.h"
#include "common/path_util.h"
#include "common/thread_name.h"
#include "core/emulator_settings.h"

#include "video_core/cache_storage.h"
#include "video_core/renderer_vulkan/vk_instance.h"
#include "video_core/renderer_vulkan/vk_pipeline_cache.h"

#include <functional>

#include <queues/mpsc_stack.h>
#include <retro_atomic.h>
#include <rthreads/retro_eventcount.h>
#include <rthreads/rthreads.h>

namespace Storage {

namespace {

/// A blob to write, moved in by Save: written from where it lies, never copied.
struct Request {
    mpsc_stack_node_t node; // first: the stack links requests through it
    BlobType type;
    std::string name;
    std::vector<u8> bytes;
    std::vector<u32> words;
};

// Save pushes onto a lock-free stack from any thread; the writer drains it.
mpsc_stack_t g_pending;
retro_eventcount_t g_work;
retro_atomic_int_t g_stop{};
// Saves between their check of DataBase::opened and their push, for Close to wait out; the
// last of them to leave wakes it
retro_atomic_int_t g_saving{};
retro_eventcount_t g_saved;

// A Save leaves: the last one out wakes a Close waiting for it
void LeaveSave() {
    if (retro_atomic_fetch_sub_int(&g_saving, 1) == 1) {
        retro_eventcount_notify(&g_saved);
    }
}

} // namespace

constexpr std::string GetBlobFileExtension(BlobType type) {
    switch (type) {
    case BlobType::ShaderMeta: {
        return "meta";
    }
    case BlobType::ShaderBinary: {
        return "spv";
    }
    case BlobType::PipelineKey: {
        return "key";
    }
    case BlobType::ShaderProfile: {
        return "bin";
    }
    case BlobType::DriverCache: {
        return "vkc";
    }
    default:
        UNREACHABLE();
    }
}

// The pack's file name in the game's cache folder
constexpr const char* PackName = "blobs.pack";

/// Writes a pack record (blob_pack.h) at the end of @file, @at its length so far, moved past
/// it: what goes before the data, then the data from where it lies. False on a short write,
/// @at then the file's length as it is, the record cut short for the next open to drop.
static bool AppendRecord(const Common::FS::IOFile& file, u64& at, u32 type, std::string_view name,
                         const void* data, u64 size) {
    std::vector<u8> head(BLOB_PACK_HEAD_BYTES(name.size()));
    const size_t n = blob_pack_record_head(head.data(), at, type, name.data(),
                                           static_cast<u32>(name.size()), data, size);
    if (file.WriteRaw<u8>(head.data(), n) != n ||
        file.WriteRaw<u8>(static_cast<const u8*>(data), size) != size) {
        at = file.GetSize();
        return false;
    }
    at += n + size;
    return true;
}

void DataBase::ProcessIO(void* arg) {
    const auto* db = static_cast<const DataBase*>(arg);
    thread_name_set("shadPS4:PipelineCacheIO");
    // Appended to from the first blob on, closed as the writer goes
    Common::FS::IOFile pack;
    u64 pack_at = 0;

    for (;;) {
        const int key = retro_eventcount_prepare_wait(&g_work);
        mpsc_stack_node_t* node = mpsc_stack_drain(&g_pending);
        if (!node) {
            // Close stops the writer only once no Save can push any more: what was
            // pushed is written before it goes.
            if (retro_atomic_load_acquire_int(&g_stop) != 0) {
                retro_eventcount_cancel_wait(&g_work);
                break;
            }
            retro_eventcount_commit_wait(&g_work, key);
            continue;
        }
        retro_eventcount_cancel_wait(&g_work);

        // Oldest first, as they were saved
        for (node = mpsc_stack_reverse(node); node;) {
            auto* request = reinterpret_cast<Request*>(node);
            node = node->next;

            if (request->type != BlobType::DriverCache) {
                // Into the pack: a record cut short by the process ending is dropped, and only
                // it, at the next open
                if (!pack.IsOpen()) {
                    pack.Open(db->pack_path, Common::FS::FileAccessMode::Append,
                              Common::FS::FileHint::Sequential);
                    pack_at = pack.IsOpen() ? pack.GetSize() : 0;
                }
                if (pack.IsOpen()) {
                    const bool words = !request->words.empty();
                    (void)AppendRecord(
                        pack, pack_at, static_cast<u32>(request->type), request->name,
                        words ? static_cast<const void*>(request->words.data())
                              : static_cast<const void*>(request->bytes.data()),
                        words ? request->words.size() * sizeof(u32) : request->bytes.size());
                }
                delete request;
                continue;
            }

            // The driver's cache, saved whole again and again: a file of its own
            auto path = db->cache_path / request->name;
            path.replace_extension(GetBlobFileExtension(request->type));
            // Written aside and renamed into place: the emulator's process is ended when the
            // content is closed, and a blob cut short by that was read back at the next start
            // as a broken shader
            auto part = path;
            part += ".part";
            using namespace Common::FS;
            bool written = false;
            {
                const IOFile file{part, FileAccessMode::Create, FileHint::Sequential};
                const u64 size =
                    request->words.empty() ? request->bytes.size() : request->words.size();
                written =
                    file.IsOpen() && (request->words.empty() ? file.Write(request->bytes)
                                                             : file.Write(request->words)) == size;
            }
            if (!written || !Rename(part, path)) {
                Remove(part);
            }
            delete request;
        }
    }
}

void DataBase::Open() {
    if (IsOpened()) {
        return;
    }

    const auto& game_info = Common::ElfInfo::Instance();

    using namespace Common::FS;
    cache_path = GetUserPath(PathType::CacheDir) / game_info.GameSerial();
    if (!Common::FS::Exists(cache_path)) {
        Common::FS::CreateDirectories(cache_path);
    }

    // What a write of the driver's cache cut short left behind
    std::vector<std::string> parts;
    ForEachEntry(cache_path, [&](std::string_view name, bool is_dir) {
        if (!is_dir && name.ends_with(".part")) {
            parts.emplace_back(name);
        }
    });
    for (const auto& name : parts) {
        Remove(cache_path / PathFromUTF8String(name));
    }
    pack_path = cache_path / PackName;
    OpenPack();

    mpsc_stack_init(&g_pending);
    retro_eventcount_init(&g_work);
    retro_eventcount_init(&g_saved);
    retro_atomic_store_relaxed_int(&g_stop, 0);
    io_worker = sthread_create(ProcessIO, this);
    (void)retro_atomic_exchange_int(&opened, io_worker != nullptr);
}

void DataBase::ReleasePack() {
    blob_pack_index_free(&pack_index);
    pack_view.reset();
}

bool DataBase::WriteNewPack() const {
    u8 header[BLOB_PACK_HEADER_SIZE];
    blob_pack_header(header);
    const Common::FS::IOFile file{pack_path, Common::FS::FileAccessMode::Create};
    return file.IsOpen() && file.WriteRaw<u8>(header, sizeof(header)) == sizeof(header);
}

void DataBase::OpenPack() {
    using namespace Common::FS;
    ReleasePack();
    pack_view = FileView::Map(pack_path);
    if (!pack_view || !blob_pack_index_build(&pack_index, pack_view->data(), pack_view->size())) {
        // None yet, empty, or not a pack of this layout
        ReleasePack();
        (void)WriteNewPack();
        return;
    }
    const u64 size = pack_view->size();
    const bool cut_short = pack_index.valid_len < size;
    const bool mostly_dead =
        pack_index.dead_bytes >= (256u << 10) && pack_index.dead_bytes * 4 >= size;
    if (!cut_short && !mostly_dead) {
        return;
    }
    // Written again with its live records only: nothing is appended after a record cut short,
    // and blobs saved again do not pile up. Aside, then renamed into place.
    if (cut_short) {
        LOG_WARNING(Render, "Pipeline cache: a record cut short at the end dropped");
    }
    auto part = pack_path;
    part += ".part";
    bool written = false;
    {
        const IOFile file{part, FileAccessMode::Create, FileHint::Sequential};
        u8 header[BLOB_PACK_HEADER_SIZE];
        blob_pack_header(header);
        written = file.IsOpen() && file.WriteRaw<u8>(header, sizeof(header)) == sizeof(header);
        u64 at = BLOB_PACK_HEADER_SIZE;
        for (u32 i = 0; written && i < pack_index.count; i++) {
            const blob_pack_entry_t& e = pack_index.entries[i];
            if (!e.live) {
                continue;
            }
            written = AppendRecord(file, at, e.type, {e.name, e.name_len}, e.data, e.size);
        }
    }
    // Windows renames over no mapped file: the old one let go first
    ReleasePack();
    if (!written || !Rename(part, pack_path)) {
        Remove(part);
        (void)WriteNewPack();
        return;
    }
    pack_view = FileView::Map(pack_path);
    if (!pack_view || !blob_pack_index_build(&pack_index, pack_view->data(), pack_view->size())) {
        ReleasePack();
        (void)WriteNewPack();
    }
}

void DataBase::Clear() {
    // The pack let go of first: Windows removes no mapped file
    ReleasePack();
    std::vector<std::string> names;
    Common::FS::ForEachEntry(cache_path, [&](std::string_view name, bool is_dir) {
        if (!is_dir) {
            names.emplace_back(name);
        }
    });
    for (const auto& name : names) {
        Common::FS::Remove(cache_path / Common::FS::PathFromUTF8String(name));
    }
    (void)WriteNewPack();
}

void DataBase::Close() {
    if (!IsOpened()) {
        return;
    }

    // Its last word, while Saves still go through
    if (const auto* hook =
            static_cast<const CloseHook*>(retro_atomic_exchange_ptr(&close_hook, nullptr))) {
        hook->func(hook->arg);
    }

    // No Save gets past its check after this; wait for the ones already past it (a few
    // instructions each) to leave, woken by the last, then let the writer finish what was
    // pushed and go.
    (void)retro_atomic_exchange_int(&opened, 0);
    for (;;) {
        const int key = retro_eventcount_prepare_wait(&g_saved);
        if (retro_atomic_load_seq_cst_int(&g_saving) == 0) {
            retro_eventcount_cancel_wait(&g_saved);
            break;
        }
        retro_eventcount_commit_wait(&g_saved, key);
    }
    retro_atomic_store_release_int(&g_stop, 1);
    retro_eventcount_notify(&g_work);
    sthread_join(io_worker);
    io_worker = nullptr;

    LOG_INFO(Render, "Cache dumped");
}

bool DataBase::Push(BlobType type, std::string&& name, std::vector<u8>&& bytes,
                    std::vector<u32>&& words) {
    retro_atomic_fetch_add_int(&g_saving, 1);
    if (retro_atomic_load_seq_cst_int(&opened) == 0) {
        LeaveSave();
        return false;
    }
    auto* request = new Request{{}, type, std::move(name), std::move(bytes), std::move(words)};
    mpsc_stack_push(&g_pending, &request->node);
    LeaveSave();
    retro_eventcount_notify(&g_work);
    return true;
}

void DataBase::SetCloseHook(const CloseHook* hook) {
    (void)retro_atomic_exchange_ptr(&close_hook, const_cast<CloseHook*>(hook));
}

bool DataBase::Save(BlobType type, std::string name, std::vector<u8>&& data) {
    return Push(type, std::move(name), std::move(data), {});
}

bool DataBase::Save(BlobType type, std::string name, std::vector<u32>&& data) {
    return Push(type, std::move(name), {}, std::move(data));
}

std::optional<Common::FS::FileView> DataBase::Load(BlobType type, const std::string& name) {
    if (!IsOpened()) {
        return std::nullopt;
    }
    if (type == BlobType::DriverCache) {
        auto path = cache_path / name;
        path.replace_extension(GetBlobFileExtension(type));
        return Common::FS::FileView::Map(path);
    }
    const blob_pack_entry_t* e = blob_pack_find(&pack_index, static_cast<u32>(type), name.data(),
                                                static_cast<u32>(name.size()));
    if (!e) {
        return std::nullopt;
    }
    return Common::FS::FileView::Borrow({e->data, static_cast<size_t>(e->size)});
}

void DataBase::ForEachBlob(BlobType type,
                           const std::function<void(std::span<const u8> data)>& func) {
    for (u32 i = 0; i < pack_index.count; i++) {
        const blob_pack_entry_t& e = pack_index.entries[i];
        if (e.live && e.type == static_cast<u32>(type)) {
            func({e.data, static_cast<size_t>(e.size)});
        }
    }
}

} // namespace Storage
