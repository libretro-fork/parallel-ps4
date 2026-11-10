// SPDX-FileCopyrightText: Copyright 2025-2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "common/elf_info.h"
#include "common/io_file.h"
#include "common/polyfill_thread.h"
#include "common/thread.h"
#include "core/emulator_settings.h"

#include "video_core/cache_storage.h"
#include "video_core/renderer_vulkan/vk_instance.h"
#include "video_core/renderer_vulkan/vk_pipeline_cache.h"

#include <condition_variable>
#include <functional>
#include <future>
#include <mutex>
#include <queue>

namespace {

std::mutex submit_mutex{};
u32 num_requests{};
std::condition_variable_any request_cv{};
std::queue<std::packaged_task<void()>> req_queue{};
std::mutex m_request{};

} // namespace

namespace Storage {

void ProcessIO(const std::stop_token& stoken) {
    Common::SetCurrentThreadName("shadPS4:PipelineCacheIO");

    while (!stoken.stop_requested()) {
        {
            std::unique_lock lk{submit_mutex};
            Common::CondvarWait(request_cv, lk, stoken, [&] { return num_requests; });
        }

        if (stoken.stop_requested()) {
            break;
        }

        while (num_requests) {
            std::packaged_task<void()> request{};
            {
                std::scoped_lock lock{m_request};
                if (req_queue.empty()) {
                    continue;
                }
                request = std::move(req_queue.front());
                req_queue.pop();
            }

            if (request.valid()) {
                request();
                request.get_future().wait();
            }

            --num_requests;
        }
    }
}

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

void DataBase::Open() {
    if (opened) {
        return;
    }

    const auto& game_info = Common::ElfInfo::Instance();

    using namespace Common::FS;
    cache_path = GetUserPath(PathType::CacheDir) / game_info.GameSerial();
    if (!std::filesystem::exists(cache_path)) {
        std::filesystem::create_directories(cache_path);
    }

    io_worker = std::jthread{ProcessIO};
    opened = true;
}

void DataBase::Close() {
    if (!IsOpened()) {
        return;
    }

    io_worker.request_stop();
    io_worker.join();

    opened = false;
    LOG_INFO(Render, "Cache dumped");
}

template <typename T>
bool WriteVector(const BlobType type, std::filesystem::path&& path_, std::vector<T>&& v) {
    {
        // The blob moves into the request: written from where it is, never copied
        auto request =
            std::packaged_task<void()>{[type, path = std::move(path_), v = std::move(v)]() mutable {
                path.replace_extension(GetBlobFileExtension(type));
                using namespace Common::FS;
                const auto file = IOFile{path, FileAccessMode::Create, FileHint::Sequential};
                file.Write(v);
            }};
        std::scoped_lock lock{m_request};
        req_queue.emplace(std::move(request));
    }

    std::scoped_lock lk{submit_mutex};
    ++num_requests;
    request_cv.notify_one();
    return true;
}

bool DataBase::Save(BlobType type, const std::string& name, std::vector<u8>&& data) {
    if (!opened) {
        return false;
    }

    auto path = cache_path / name;
    return WriteVector(type, std::move(path), std::move(data));
}

bool DataBase::Save(BlobType type, const std::string& name, std::vector<u32>&& data) {
    if (!opened) {
        return false;
    }

    auto path = cache_path / name;
    return WriteVector(type, std::move(path), std::move(data));
}

std::optional<Common::FS::FileView> DataBase::Load(BlobType type, const std::string& name) {
    if (!opened) {
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
