/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

/* The overlays (ovl.h) on the presenter's images: built once a frame, drawn over the game's
 * picture in one pass, with ImGui's Vulkan backend's pipeline and state. */

#ifndef SHADPS4_VK_OVERLAY_H
#define SHADPS4_VK_OVERLAY_H

#include <stddef.h>
#include <stdint.h>

#ifndef VK_NO_PROTOTYPES
#define VK_NO_PROTOTYPES
#endif
#include <vulkan/vulkan.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct ovl_vk_info {
    PFN_vkGetDeviceProcAddr get_device_proc;
    VkDevice device;
    VkPhysicalDeviceMemoryProperties memory_properties;
    unsigned image_count; /* the swapchain's: a frame's buffers for each */
    VkFormat surface_format;
    const uint8_t* font; /* the UI font (a TrueType file), lent for as long as it runs */
    size_t font_size;
} ovl_vk_info_t;

/* Made with the swapchain: the fonts, the atlas, the images' slots, the pipeline and the
 * frames' buffers. 0 when the device would not make them. */
int ovl_vk_init(const ovl_vk_info_t* info);

/* After the device has gone idle */
void ovl_vk_shutdown(void);

/* Builds the frame's overlays over a @width x @height picture, @delta seconds after the last;
 * the FPS counter shows @framerate */
void ovl_vk_new_frame(unsigned width, unsigned height, float delta, float framerate);

/* From any thread: an overlay is showing or waiting to, so frames are to be drawn even when
 * the game shows none */
int ovl_vk_must_keep_drawing(void);

/* Draws what ovl_vk_new_frame built over @view (in the colour attachment layout), keeping what
 * is there; with nothing to draw, records nothing */
void ovl_vk_render(VkCommandBuffer cmd, VkImageView view, VkExtent2D extent);

#ifdef __cplusplus
}
#endif

#endif
