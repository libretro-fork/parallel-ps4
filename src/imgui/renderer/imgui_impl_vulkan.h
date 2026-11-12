// SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

// Based on imgui_impl_vulkan.h from Dear ImGui repository

#pragma once

#include "common/types.h"
#include "video_core/renderer_vulkan/vk_common.h"

struct ImDrawData;

namespace ImGui {
struct Texture {
    vk::DescriptorSet descriptor_set{nullptr};
    // Holds the picture in the output's own form (with HDR Output, PQ): drawn as it is
    bool output{false};
};
} // namespace ImGui

namespace ImGui::Vulkan {

struct InitInfo {
    vk::Instance instance;
    vk::PhysicalDevice physical_device;
    vk::Device device;
    uint32_t queue_family;
    vk::Queue queue;
    // The queue the frames that draw the textures go to, when it is not queue: a texture is
    // retired once both have passed it
    vk::Queue draw_queue;
    uint32_t image_count;               // >= 2
    vk::DeviceSize min_allocation_size; // Minimum allocation size
    vk::PipelineCache pipeline_cache;
    uint32_t subpass;
    vk::PipelineRenderingCreateInfoKHR pipeline_rendering_create_info;

    // (Optional) Allocation, Logging
    const vk::AllocationCallbacks* allocator{};
    void (*check_vk_result_fn)(vk::Result err);
};

// Prepare all resources needed for uploading textures
// Caller should clean up the returned data.
struct UploadTextureData {
    vk::Image image;
    vk::ImageView image_view;
    vk::DeviceMemory image_memory;

    vk::CommandBuffer command_buffer; // Submit to the queue
    vk::Buffer upload_buffer;
    vk::DeviceMemory upload_buffer_memory;

    ImTextureID im_texture;
    // Signalled when the upload has run
    vk::Fence fence;

    /// Submits the upload and returns; Uploaded() says when it has run.
    void Upload();
    /// Whether the upload has run; the first time it has, its buffers go back. Never waits.
    bool Uploaded();

    void Destroy();
};

ImTextureID AddTexture(vk::ImageView image_view, vk::ImageLayout image_layout,
                       vk::Sampler sampler = VK_NULL_HANDLE);

UploadTextureData UploadTexture(const void* data, vk::Format format, u32 width, u32 height,
                                size_t size);

/// As UploadTexture, with the pixels written by @fill straight into the upload buffer (size
/// bytes, rows width pixels apart) instead of copied there. When @fill returns false, nothing
/// is made and the result's im_texture is null.
using UploadFill = bool (*)(void* ctx, void* dst);
UploadTextureData UploadTexture(vk::Format format, u32 width, u32 height, size_t size,
                                UploadFill fill, void* ctx);

void RemoveTexture(ImTextureID descriptor_set);

/// Frees what the GPU has finished with among the textures destroyed; never waits. Called
/// once a frame, on the thread that uploads and destroys them.
void CollectRetired();

bool Init(InitInfo info);
void Shutdown();
// Records the font atlas texture requests of this frame; call outside a render pass, before the
// RenderDrawData() that samples them.
void UpdateTextures(ImDrawData& draw_data, vk::CommandBuffer command_buffer);
void RenderDrawData(ImDrawData& draw_data, vk::CommandBuffer command_buffer,
                    vk::Pipeline pipeline = VK_NULL_HANDLE);

void SetBlendEnabled(bool enabled);
void OnSurfaceFormatChange(vk::Format surface_format);

} // namespace ImGui::Vulkan
