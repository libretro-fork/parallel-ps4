// SPDX-FileCopyrightText: Copyright 2025-2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "common/elf_info.h"
#include "common/io_file.h"
#include "common/thread.h"
#include "core/emulator_settings.h"

#include "video_core/cache_storage.h"
#include "video_core/renderer_vulkan/vk_instance.h"
#include "video_core/renderer_vulkan/vk_pipeline_cache.h"

#include <atomic>
#include <functional>
#include <thread>

#include <queues/mpsc_stack.h>
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
std::atomic<bool> g_stop{};
// Saves between their check of DataBase::opened and their push, for Close to wait out
std::atomic<u32> g_saving{};

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
    default:
        UNREACHABLE();
    }
}

void DataBase::ProcessIO(void* arg) {
    const auto* db = static_cast<const DataBase*>(arg);
    Common::SetCurrentThreadName("shadPS4:PipelineCacheIO");

    for (;;) {
        const int key = retro_eventcount_prepare_wait(&g_work);
        mpsc_stack_node_t* node = mpsc_stack_drain(&g_pending);
        if (!node) {
            // Close stops the writer only once no Save can push any more: what was
            // pushed is written before it goes.
            if (g_stop.load(std::memory_order_acquire)) {
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

            auto path = db->cache_path / request->name;
            path.replace_extension(GetBlobFileExtension(request->type));
            using namespace Common::FS;
            const IOFile file{path, FileAccessMode::Create, FileHint::Sequential};
            if (request->words.empty()) {
                file.Write(request->bytes);
            } else {
                file.Write(request->words);
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
    if (!std::filesystem::exists(cache_path)) {
        std::filesystem::create_directories(cache_path);
    }

    mpsc_stack_init(&g_pending);
    retro_eventcount_init(&g_work);
    g_stop.store(false, std::memory_order_relaxed);
    io_worker = sthread_create(ProcessIO, this);
    opened.store(io_worker != nullptr, std::memory_order_seq_cst);
}

void DataBase::Close() {
    if (!IsOpened()) {
        return;
    }

    // No Save gets past its check after this; wait out the ones already past it (a few
    // instructions each), then let the writer finish what was pushed and go.
    opened.store(false, std::memory_order_seq_cst);
    while (g_saving.load(std::memory_order_seq_cst) != 0) {
        std::this_thread::yield();
    }
    g_stop.store(true, std::memory_order_release);
    retro_eventcount_notify(&g_work);
    sthread_join(io_worker);
    io_worker = nullptr;

    LOG_INFO(Render, "Cache dumped");
}

bool DataBase::Push(BlobType type, std::string&& name, std::vector<u8>&& bytes,
                    std::vector<u32>&& words) {
    g_saving.fetch_add(1, std::memory_order_seq_cst);
    if (!opened.load(std::memory_order_seq_cst)) {
        g_saving.fetch_sub(1, std::memory_order_release);
        return false;
    }
    auto* request = new Request{{}, type, std::move(name), std::move(bytes), std::move(words)};
    mpsc_stack_push(&g_pending, &request->node);
    g_saving.fetch_sub(1, std::memory_order_release);
    retro_eventcount_notify(&g_work);
    return true;
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
    auto path = cache_path / name;
    path.replace_extension(GetBlobFileExtension(type));
    return Common::FS::FileView::Map(path);
}

void DataBase::ForEachBlob(BlobType type,
                           const std::function<void(std::span<const u8> data)>& func) {
    const auto& ext = GetBlobFileExtension(type);
    for (const auto& file_name : std::filesystem::directory_iterator{cache_path}) {
        if (file_name.path().extension().string().ends_with(ext)) {
            if (const auto view = Common::FS::FileView::Map(file_name.path())) {
                func(view->Bytes());
            }
        }
    }
}

} // namespace Storage
