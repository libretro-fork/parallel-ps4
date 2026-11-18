/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

/* The overlays (ovl.h) on the presenter's images: built once a frame, drawn over the game's
 * picture in one pass, with ImGui's Vulkan backend's pipeline and state. */

#ifndef SHADPS4_VK_OVERLAY_H
#define SHADPS4_VK_OVERLAY_H

#include <stddef.h>
#include <stdint.h>

#include "ovl.h"

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
    /* The fonts tried after it, in turn, for a character it does not have (other scripts,
     * symbols), lent likewise; those that cannot be read are left out */
    const ovl_face_desc_t* fallback_faces;
    unsigned fallback_count;
    const uint8_t* splash; /* the game's splash screen (a PNG), lent likewise; null for none */
    size_t splash_size;
} ovl_vk_info_t;

/* Made with the swapchain: the fonts, the atlas, the images' slots, the pipeline and the
 * frames' buffers, and the splash's texture when it is to be shown. 0 when the device would
 * not make them (without the splash, the game's picture shows). */
int ovl_vk_init(const ovl_vk_info_t* info);

/* What the last ovl_vk_init that failed could not make */
const char* ovl_vk_error(void);

/* After the device has gone idle */
void ovl_vk_shutdown(void);

/* Builds the frame's overlays over a @width x @height picture, @delta seconds after the last;
 * the FPS counter shows @framerate */
void ovl_vk_new_frame(unsigned width, unsigned height, float delta, float framerate);

/* From any thread: an overlay is showing or waiting to, so frames are to be drawn even when
 * the game shows none */
int ovl_vk_must_keep_drawing(void);

/* The splash is decoded and still to be shown, in place of the game's picture */
int ovl_vk_splash_ready(void);

/* Draws it over the whole of @view (in the colour attachment layout), the rest black, when it
 * is ready: the first pass on the image, ahead of every overlay */
void ovl_vk_render_splash(VkCommandBuffer cmd, VkImageView view, VkExtent2D extent);

/* It is no longer shown (the game hid it): freed once no frame in flight draws it */
void ovl_vk_splash_done(void);

/* Draws what ovl_vk_new_frame built over @view (in the colour attachment layout), keeping what
 * is there; with nothing to draw, records nothing */
void ovl_vk_render(VkCommandBuffer cmd, VkImageView view, VkExtent2D extent);

#ifdef __cplusplus
}
#endif

#endif
