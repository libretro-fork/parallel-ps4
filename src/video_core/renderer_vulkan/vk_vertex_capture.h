// SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <span>

#include <retro_atomic.h>

#include "common/types.h"

/// A capture, on request, of the vertex inputs of every draw of a whole frame: each
/// vertex buffer's bytes as the game's memory has them and as the GPU is given them, and
/// whether the GPU wrote them. Written to vertex_capture.bin in the log directory and read by
/// tools/vertex_capture_report.c, which says where a broken mesh's vertices came from.
/// Costs one load per draw while no capture is asked for.
namespace Vulkan::VertexCapture {

/// One vertex input of a draw
struct Binding {
    u32 semantic;
    u32 format; ///< VkFormat
    u32 stride;
    u32 flags;
    u64 address;
    u64 size;
};
constexpr u32 BindingGpuWritten = 1u << 0;   ///< In a page the GPU has written to
constexpr u32 BindingPerInstance = 1u << 1;  ///< Stepped per instance
constexpr u32 BindingStreamBuffer = 1u << 2; ///< Given to the GPU from the stream buffer

extern retro_atomic_int_t state;

/// Asks for a capture, from the next submit on. Any thread.
void Request();

/// True while draws are to be recorded. GPU thread.
inline bool Active() {
    return retro_atomic_load_relaxed_int(&state) == 2;
}

/// At each submit: starts a capture asked for. GPU thread.
void OnSubmit();

/// At each frame the game shows: a capture ends at the second after it began, with one whole
/// frame in it. GPU thread.
void OnFlip();

/// Records a draw; its number, for the data that follows. @num_vertices vertices from
/// @first_vertex of each input are the ones the draw reads, 0 when that is not known.
u32 Draw(u32 primitive, u32 num_indices, u32 num_instances, u32 first_vertex, u32 num_vertices,
         std::span<const Binding> bindings);

/// True if this range's bytes are still wanted: not past the capture's size, and not a
/// range already recorded that the GPU has not written.
bool Wants(u64 address, u64 size, bool gpu_written);

/// A vertex buffer's bytes for draw @draw: in the game's memory, and as the GPU has them.
void Guest(u32 draw, u64 address, const u8* data, u64 size);
void Host(u32 draw, u64 address, const u8* data, u64 size);

/// A Host() that will come later (a readback): the capture is not closed before it has.
void HostPending();

} // namespace Vulkan::VertexCapture
