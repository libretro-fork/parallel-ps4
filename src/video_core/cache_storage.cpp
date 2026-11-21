// SPDX-FileCopyrightText: Copyright 2025-2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <stdlib.h>
#include <string.h>

#include <file/file_path.h>
#include <queues/mpsc_stack.h>
#include <retro_atomic.h>
#include <retro_dirent.h>
#include <rthreads/retro_eventcount.h>
#include <rthreads/rthreads.h>
#include <streams/file_stream.h>

#include "common/assert.h"
#include "common/elf_info.h"
#include "common/io_file.h"
#include "common/logging/log.h"
#include "common/path_util.h"
#include "common/thread_name.h"
#include "video_core/cache_storage.h"

namespace Storage {

namespace {

/// A blob to write, handed over by Save: written from where it lies, never copied.
struct Request {
    mpsc_stack_node_t node; // first: the stack links requests through it
    BlobType type;
    char name[DataBase::MaxNameLength + 1];
    u32 name_len;
    void* data; // malloc'd, freed with the request
    size_t size;
};

void FreeRequest(Request* request) {
    free(request->data);
    delete request;
}

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

static const char* GetBlobFileExtension(BlobType type) {
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

/// The file of a blob kept on its own (the driver's cache): @name.<extension> in @dir
static void DriverCachePath(char* out, size_t out_size, const char* dir, const char* name,
                            BlobType type) {
    char file_name[DataBase::MaxNameLength + 8];
    snprintf(file_name, sizeof(file_name), "%s.%s", name, GetBlobFileExtension(type));
    fill_pathname_join_special(out, dir, file_name, out_size);
}

/// Renames @from to @to, over a file there already
static bool RenameOver(const char* from, const char* to) {
    if (path_is_valid(to)) {
        filestream_delete(to);
    }
    return filestream_rename(from, to) == 0;
}

/// Removes the files of @dir: all of them, or those whose names end in @suffix
static void RemoveFiles(const char* dir, const char* suffix) {
    struct RDIR* rdir = retro_opendir(dir);
    if (!rdir) {
        return;
    }
    const size_t suffix_len = suffix ? strlen(suffix) : 0;
    while (retro_readdir(rdir)) {
        const char* name = retro_dirent_get_name(rdir);
        if (!name || retro_dirent_is_dir(rdir, nullptr) || !strcmp(name, ".") ||
            !strcmp(name, "..")) {
            continue;
        }
        const size_t len = strlen(name);
        if (suffix && (len < suffix_len || strcmp(name + len - suffix_len, suffix) != 0)) {
            continue;
        }
        char path[PATH_MAX_LENGTH];
        fill_pathname_join_special(path, dir, name, sizeof(path));
        filestream_delete(path);
    }
    retro_closedir(rdir);
}

/// Writes a pack record (blob_pack.h) at the end of @file, @at its length so far, moved past
/// it: what goes before the data, then the data from where it lies. False on a short write,
/// @at then the file's length as it is, the record cut short for the next open to drop.
static bool AppendRecord(const Common::FS::IOFile& file, u64& at, u32 type, const char* name,
                         u32 name_len, const void* data, u64 size) {
    u8 head[BLOB_PACK_HEAD_BYTES(DataBase::MaxNameLength)];
    ASSERT(name_len <= DataBase::MaxNameLength);
    const size_t n = blob_pack_record_head(head, at, type, name, name_len, data, size);
    if (file.WriteRaw<u8>(head, n) != n ||
        file.WriteRaw<u8>(static_cast<const u8*>(data), size) != size) {
        at = file.GetSize();
        return false;
    }
    at += n + size;
    return true;
}

void DataBase::ProcessIO(void* arg) {
    const Storage::DataBase* db = static_cast<const DataBase*>(arg);
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
            Request* request = reinterpret_cast<Request*>(node);
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
                    (void)AppendRecord(pack, pack_at, static_cast<u32>(request->type),
                                       request->name, request->name_len, request->data,
                                       request->size);
                }
                FreeRequest(request);
                continue;
            }

            // The driver's cache, saved whole again and again: a file of its own
            char path[PATH_MAX_LENGTH];
            char part[PATH_MAX_LENGTH];
            DriverCachePath(path, sizeof(path), db->cache_path, request->name, request->type);
            // Written aside and renamed into place: the emulator's process is ended when the
            // content is closed, and a blob cut short by that was read back at the next start
            // as a broken shader
            snprintf(part, sizeof(part), "%s.part", path);
            using namespace Common::FS;
            bool written = false;
            {
                const IOFile file{part, FileAccessMode::Create, FileHint::Sequential};
                written = file.IsOpen() &&
                          file.WriteRaw<u8>(request->data, request->size) == request->size;
            }
            if (!written || !RenameOver(part, path)) {
                filestream_delete(part);
            }
            FreeRequest(request);
        }
    }
}

void DataBase::Open() {
    if (IsOpened()) {
        return;
    }

    const Common::ElfInfo& game_info = Common::ElfInfo::Instance();

    fill_pathname_join_special(cache_path,
                               Common::FS::GetUserPathUTF8(Common::FS::PathType::CacheDir),
                               game_info.GameSerial(), sizeof(cache_path));
    if (!path_is_directory(cache_path)) {
        path_mkdir(cache_path);
    }

    // What a write of the driver's cache cut short left behind
    RemoveFiles(cache_path, ".part");
    fill_pathname_join_special(pack_path, cache_path, PackName, sizeof(pack_path));
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
    pack_file.Close();
}

/// Maps @path into @file; false when there is nothing there or it cannot be mapped
static bool MapFile(Common::FS::IOFile& file, const char* path) {
    file.Close();
    // A missing file is an answer, not an error to log
    if (!path_is_valid(path)) {
        return false;
    }
    if (file.Open(path, Common::FS::FileAccessMode::Read, Common::FS::FileHint::Mapped) != 0) {
        return false;
    }
    // An empty file has nothing to map, and is nothing to read
    if (file.GetSize() == 0 || file.Mapped().empty()) {
        file.Close();
        return false;
    }
    return true;
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
    if (!MapFile(pack_file, pack_path) ||
        !blob_pack_index_build(&pack_index, pack_file.Mapped().data(), pack_file.Mapped().size())) {
        // None yet, empty, or not a pack of this layout
        ReleasePack();
        (void)WriteNewPack();
        return;
    }
    const u64 size = pack_file.Mapped().size();
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
    char part[PATH_MAX_LENGTH];
    snprintf(part, sizeof(part), "%s.part", pack_path);
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
            written = AppendRecord(file, at, e.type, e.name, e.name_len, e.data, e.size);
        }
    }
    // Windows renames over no mapped file: the old one let go first
    ReleasePack();
    if (!written || !RenameOver(part, pack_path)) {
        filestream_delete(part);
        (void)WriteNewPack();
        return;
    }
    if (!MapFile(pack_file, pack_path) ||
        !blob_pack_index_build(&pack_index, pack_file.Mapped().data(), pack_file.Mapped().size())) {
        ReleasePack();
        (void)WriteNewPack();
    }
}

void DataBase::Clear() {
    // The pack let go of first: Windows removes no mapped file
    ReleasePack();
    driver_file.Close();
    RemoveFiles(cache_path, nullptr);
    (void)WriteNewPack();
}

void DataBase::Close() {
    if (!IsOpened()) {
        return;
    }

    // Its last word, while Saves still go through
    if (const Storage::CloseHook* hook =
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

bool DataBase::Save(BlobType type, const char* name, void* data, size_t size) {
    const size_t name_len = strlen(name);
    ASSERT_MSG(name_len <= MaxNameLength, "Cache blob name too long: %s", name);
    retro_atomic_fetch_add_int(&g_saving, 1);
    if (retro_atomic_load_seq_cst_int(&opened) == 0) {
        LeaveSave();
        free(data);
        return false;
    }
    Request* request = new Request{};
    request->type = type;
    memcpy(request->name, name, name_len + 1);
    request->name_len = static_cast<u32>(name_len);
    request->data = data;
    request->size = size;
    mpsc_stack_push(&g_pending, &request->node);
    LeaveSave();
    retro_eventcount_notify(&g_work);
    return true;
}

bool DataBase::SaveCopy(BlobType type, const char* name, const void* data, size_t size) {
    if (!IsOpened()) {
        return false;
    }
    void* const copy = malloc(size ? size : 1);
    ASSERT_MSG(copy, "Out of memory for a cache blob");
    memcpy(copy, data, size);
    return Save(type, name, copy, size);
}

void DataBase::SetCloseHook(const CloseHook* hook) {
    (void)retro_atomic_exchange_ptr(&close_hook, const_cast<CloseHook*>(hook));
}

Common::Span<const u8> DataBase::Load(BlobType type, const char* name) {
    if (!IsOpened()) {
        return {};
    }
    if (type == BlobType::DriverCache) {
        char path[PATH_MAX_LENGTH];
        DriverCachePath(path, sizeof(path), cache_path, name, type);
        if (!MapFile(driver_file, path)) {
            return {};
        }
        return Common::Span<const u8>{driver_file.Mapped().data(), driver_file.Mapped().size()};
    }
    const blob_pack_entry_t* e =
        blob_pack_find(&pack_index, static_cast<u32>(type), name, static_cast<u32>(strlen(name)));
    if (!e) {
        return {};
    }
    return Common::Span<const u8>{e->data, static_cast<size_t>(e->size)};
}

void DataBase::ForEachBlob(BlobType type, void (*func)(void* user, Common::Span<const u8> data),
                           void* user) {
    for (u32 i = 0; i < pack_index.count; i++) {
        const blob_pack_entry_t& e = pack_index.entries[i];
        if (e.live && e.type == static_cast<u32>(type)) {
            func(user, Common::Span<const u8>{e.data, static_cast<size_t>(e.size)});
        }
    }
}

} // namespace Storage
