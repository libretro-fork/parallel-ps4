// SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <array>
#include <cstdio>
#include <cstring>

#include <streams/file_stream.h>

#include "common/logging/log.h"
#include "common/path_util.h"
#include "video_core/renderer_vulkan/vk_vertex_capture.h"

namespace Vulkan::VertexCapture {

namespace {

// state: 0 idle, 1 asked for, 2 recording, 3 recorded, readbacks still to come
constexpr int Idle = 0;
constexpr int Asked = 1;
constexpr int Recording = 2;
constexpr int Draining = 3;

/// The file: "PS4VCAP1", then chunks of {u32 type, u32 draw, u64 size} and size bytes.
constexpr u32 ChunkDraw = 1;  ///< {u32 primitive, num_indices, num_instances, count}, Bindings
constexpr u32 ChunkGuest = 2; ///< {u64 address}, the bytes in the game's memory
constexpr u32 ChunkHost = 3;  ///< {u64 address}, the bytes the GPU has

constexpr u64 MaxBytes = 768ull << 20;
constexpr u32 SubmitsCaptured = 2;

// The GPU thread's
RFILE* file{};
u64 written{};
u32 draws{};
u32 submits_left{};
u32 host_pending{};
/// Ranges recorded already, by address and size: the GPU's own are recorded each time
std::array<u64, 4096> seen{};

void Chunk(u32 type, u32 draw, const void* head, u64 head_size, const void* body, u64 body_size) {
    if (!file) {
        return;
    }
    const u64 size = head_size + body_size;
    filestream_write(file, &type, sizeof(type));
    filestream_write(file, &draw, sizeof(draw));
    filestream_write(file, &size, sizeof(size));
    filestream_write(file, head, static_cast<s64>(head_size));
    if (body_size) {
        filestream_write(file, body, static_cast<s64>(body_size));
    }
    written += 16 + size;
}

void Close() {
    if (file) {
        filestream_close(file);
        file = nullptr;
    }
    LOG_INFO(Render_Vulkan, "Vertex capture: {} draws, {} MiB, in vertex_capture.bin", draws,
             written >> 20);
    retro_atomic_store_release_int(&state, Idle);
}

} // Anonymous namespace

retro_atomic_int_t state{Idle};

void Request() {
    retro_atomic_cas_int(&state, Idle, Asked);
}

void OnSubmit() {
    const int now = retro_atomic_load_relaxed_int(&state);
    if (now == Asked) {
        const auto path =
            Common::FS::GetUserPath(Common::FS::PathType::LogDir) / "vertex_capture.bin";
        file = filestream_open(path.string().c_str(), RETRO_VFS_FILE_ACCESS_WRITE,
                               RETRO_VFS_FILE_ACCESS_HINT_NONE);
        if (!file) {
            LOG_ERROR(Render_Vulkan, "Vertex capture: cannot write {}", path.string());
            retro_atomic_store_release_int(&state, Idle);
            return;
        }
        filestream_write(file, "PS4VCAP1", 8);
        written = 8;
        draws = 0;
        host_pending = 0;
        submits_left = SubmitsCaptured;
        seen.fill(0);
        retro_atomic_store_release_int(&state, Recording);
    } else if (now == Recording && draws != 0 && --submits_left == 0) {
        if (host_pending == 0) {
            Close();
        } else {
            retro_atomic_store_release_int(&state, Draining);
        }
    }
}

u32 Draw(u32 primitive, u32 num_indices, u32 num_instances, std::span<const Binding> bindings) {
    const u32 head[4] = {primitive, num_indices, num_instances, static_cast<u32>(bindings.size())};
    Chunk(ChunkDraw, draws, head, sizeof(head), bindings.data(), bindings.size_bytes());
    return draws++;
}

bool Wants(u64 address, u64 size, bool gpu_written) {
    if (written + 2 * size > MaxBytes) {
        return false;
    }
    if (gpu_written) {
        return true;
    }
    const u64 key = address * 0x9E3779B97F4A7C15ull ^ size;
    u64& slot = seen[(key >> 20) % seen.size()];
    if (slot == key) {
        return false;
    }
    slot = key;
    return true;
}

void Guest(u32 draw, u64 address, const u8* data, u64 size) {
    Chunk(ChunkGuest, draw, &address, sizeof(address), data, size);
}

void Host(u32 draw, u64 address, const u8* data, u64 size) {
    Chunk(ChunkHost, draw, &address, sizeof(address), data, size);
    if (host_pending != 0 && --host_pending == 0 &&
        retro_atomic_load_relaxed_int(&state) == Draining) {
        Close();
    }
}

void HostPending() {
    ++host_pending;
}

} // namespace Vulkan::VertexCapture
