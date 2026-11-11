// SPDX-FileCopyrightText: Copyright 2024-2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <cstdlib>
#include <deque>
#include <utility>
#include <vector>

#include <formats/image.h>
#include <formats/rpng.h>
#include <imgui.h>
#include <retro_atomic.h>
#include "common/assert.h"
#include "common/event_mutex.h"
#include "common/io_file.h"
#include "common/polyfill_thread.h"
#include "common/thread.h"
#include "core/emulator_settings.h"
#include "imgui_impl_vulkan.h"
#include "texture_manager.h"

namespace ImGui {

namespace Core::TextureManager {
struct Inner {
    retro_atomic_int_t count{0};
    ImTextureID texture_id = nullptr;
    u32 width = 0;
    u32 height = 0;

    Vulkan::UploadTextureData upload_data;

    ~Inner();
};
} // namespace Core::TextureManager

using namespace Core::TextureManager;

RefCountedTexture::RefCountedTexture(Inner* inner) : inner(inner) {
    retro_atomic_fetch_add_int(&inner->count, 1);
}

RefCountedTexture RefCountedTexture::DecodePngTexture(std::vector<u8> data) {
    return DecodePngTexture(Common::FS::FileView{std::move(data)});
}

RefCountedTexture RefCountedTexture::DecodePngTexture(Common::FS::FileView data) {
    const auto core = new Inner;
    Core::TextureManager::DecodePngTexture(std::move(data), core);
    return RefCountedTexture(core);
}

RefCountedTexture RefCountedTexture::DecodePngFile(std::filesystem::path path) {
    const auto core = new Inner;
    Core::TextureManager::DecodePngFile(std::move(path), core);
    return RefCountedTexture(core);
}

RefCountedTexture::RefCountedTexture() : inner(nullptr) {}

RefCountedTexture::RefCountedTexture(const RefCountedTexture& other) : inner(other.inner) {
    if (inner != nullptr) {
        retro_atomic_fetch_add_int(&inner->count, 1);
    }
}

RefCountedTexture::RefCountedTexture(RefCountedTexture&& other) noexcept : inner(other.inner) {
    other.inner = nullptr;
}

RefCountedTexture& RefCountedTexture::operator=(const RefCountedTexture& other) {
    if (this == &other)
        return *this;
    inner = other.inner;
    if (inner != nullptr) {
        retro_atomic_fetch_add_int(&inner->count, 1);
    }
    return *this;
}

RefCountedTexture& RefCountedTexture::operator=(RefCountedTexture&& other) noexcept {
    if (this == &other)
        return *this;
    std::swap(inner, other.inner);
    return *this;
}

RefCountedTexture::~RefCountedTexture() {
    if (inner != nullptr) {
        if (retro_atomic_fetch_sub_int(&inner->count, 1) == 1) {
            delete inner;
        }
    }
}

RefCountedTexture::Image RefCountedTexture::GetTexture() const {
    if (inner == nullptr) {
        return {};
    }
    return Image{
        .im_id = inner->texture_id,
        .width = inner->width,
        .height = inner->height,
    };
}

RefCountedTexture::operator bool() const {
    return inner != nullptr && inner->texture_id != nullptr;
}

struct Job {
    Inner* core;
    /// The PNG, lent or kept; a file is mapped by the worker
    Common::FS::FileView data;
    std::filesystem::path path;
};

struct UploadJob {
    Inner* core = nullptr;
    Vulkan::UploadTextureData data;
    int tick = 0; // Used to skip the first frame when destroying to await the current frame to draw
};

static bool g_is_worker_running = false;
static std::jthread g_worker_thread;
static Common::EventCondition g_worker_cv;

static Common::EventMutex g_job_list_mtx;
static std::deque<Job> g_job_list;

static Common::EventMutex g_upload_mtx;
static std::deque<UploadJob> g_upload_list;

namespace Core::TextureManager {

Inner::~Inner() {
    if (upload_data.im_texture != nullptr) {
        std::unique_lock lk{g_upload_mtx};
        g_upload_list.emplace_back(UploadJob{
            .data = this->upload_data,
            .tick = 2,
        });
    }
}

void WorkerLoop() {
    Common::SetCurrentThreadName("shadPS4:ImGuiTextureManager");
    Common::EventMutex mtx;
    while (g_is_worker_running) {
        std::unique_lock lk{mtx};
        g_worker_cv.wait(lk);
        if (!g_is_worker_running) {
            break;
        }
        while (true) {
            g_job_list_mtx.lock();
            if (g_job_list.empty()) {
                g_job_list_mtx.unlock();
                break;
            }
            auto [core, png_raw, path] = std::move(g_job_list.front());
            g_job_list.pop_front();
            g_job_list_mtx.unlock();

            if (emu_settings_get_vkcrash_diagnostic_enabled()) {
                // FIXME: Crash diagnostic hangs when building the command buffer here
                continue;
            }

            if (!path.empty()) { // Decode PNG from file, mapped where it lies
                auto mapped = Common::FS::FileView::Map(path);
                if (!mapped) {
                    LOG_ERROR(ImGui, "Failed to open PNG file: {}", path.string());
                    continue;
                }
                png_raw = std::move(*mapped);
            }

            // Decoded with libretro-common's rpng as R,G,B,A bytes, the texture's own format,
            // straight into the texture's upload buffer: the PNG is read where it lies and the
            // pixels are written once.
            rpng_t* rpng = rpng_alloc();
            rpng_info info{};
            if (!rpng ||
                // rpng only reads the bytes
                !rpng_set_buf_ptr(rpng, const_cast<u8*>(png_raw.data()), png_raw.size()) ||
                !rpng_start(rpng)) {
                rpng_free(rpng);
                LOG_ERROR(ImGui, "Failed to decode PNG {}", path.string());
                continue;
            }
            while (rpng_iterate_image(rpng)) {
            }
            if (!rpng_get_info(rpng, &info) || info.width == 0 || info.height == 0) {
                rpng_free(rpng);
                LOG_ERROR(ImGui, "Failed to decode PNG {}", path.string());
                continue;
            }
            struct Decode {
                rpng_t* rpng;
                size_t size;
                unsigned width;
            } decode{rpng, png_raw.size(), info.width};
            auto texture = Vulkan::UploadTexture(
                vk::Format::eR8G8B8A8Unorm, info.width, info.height,
                static_cast<size_t>(info.width) * info.height * 4,
                [](void* ctx, void* dst) {
                    const auto& d = *static_cast<const Decode*>(ctx);
                    if (!rpng_set_output(d.rpng, static_cast<uint32_t*>(dst), d.width)) {
                        return false;
                    }
                    void* pixels = nullptr;
                    unsigned w = 0;
                    unsigned h = 0;
                    int ret;
                    do {
                        ret = rpng_process_image(d.rpng, &pixels, d.size, &w, &h, true);
                    } while (ret == IMAGE_PROCESS_NEXT);
                    return ret == IMAGE_PROCESS_END && pixels == dst;
                },
                &decode);
            rpng_free(rpng);
            if (!texture.im_texture) {
                LOG_ERROR(ImGui, "Failed to decode PNG {}", path.string());
                continue;
            }
            const unsigned width = info.width;
            const unsigned height = info.height;

            core->upload_data = texture;
            core->width = width;
            core->height = height;

            std::unique_lock upload_lk{g_upload_mtx};
            g_upload_list.emplace_back(UploadJob{
                .core = core,
            });
        }
    }
}

void StartWorker() {
    ASSERT(!g_is_worker_running);
    g_worker_thread = std::jthread(WorkerLoop);
    g_is_worker_running = true;
}

void StopWorker() {
    ASSERT(g_is_worker_running);
    g_is_worker_running = false;
    g_worker_cv.notify_one();
}

void DecodePngTexture(Common::FS::FileView data, Inner* core) {
    retro_atomic_fetch_add_int(&core->count, 1);
    Job job{
        .core = core,
        .data = std::move(data),
    };
    std::unique_lock lk{g_job_list_mtx};
    g_job_list.push_back(std::move(job));
    g_worker_cv.notify_one();
}

void DecodePngFile(std::filesystem::path path, Inner* core) {
    retro_atomic_fetch_add_int(&core->count, 1);
    Job job{
        .core = core,
        .path = std::move(path),
    };
    std::unique_lock lk{g_job_list_mtx};
    g_job_list.push_back(std::move(job));
    g_worker_cv.notify_one();
}

// Uploads submitted and not yet run, oldest first; the render thread's alone
static std::vector<Inner*> g_uploading;

void Submit() {
    // Nothing here waits for the GPU: an upload is published once its fence has signalled, and
    // a destroyed texture's objects go once the queues have passed it.
    Vulkan::CollectRetired();
    std::erase_if(g_uploading, [](Inner* core) {
        if (!core->upload_data.Uploaded()) {
            return false;
        }
        core->texture_id = core->upload_data.im_texture;
        if (retro_atomic_fetch_sub_int(&core->count, 1) == 1) {
            delete core;
        }
        return true;
    });

    UploadJob upload;
    {
        std::unique_lock lk{g_upload_mtx};
        if (g_upload_list.empty()) {
            return;
        }
        // Upload one texture at a time to avoid slow down
        upload = g_upload_list.front();
        g_upload_list.pop_front();
        if (upload.tick > 0) {
            --upload.tick;
            g_upload_list.emplace_back(upload);
            return;
        }
    }
    if (upload.core != nullptr) {
        upload.core->upload_data.Upload();
        g_uploading.push_back(upload.core);
    } else {
        upload.data.Destroy();
    }
}
} // namespace Core::TextureManager

} // namespace ImGui