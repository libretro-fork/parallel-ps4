/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

/* The size and block of each Vulkan format, made from the Vulkan headers' format traits
 * (vulkan_format_traits.hpp) by a script, for the C API. */

#ifndef VK_FORMAT_TRAITS_H
#define VK_FORMAT_TRAITS_H

#include <vulkan/vulkan.h>

#ifdef __cplusplus
extern "C" {
#endif

/* The bytes of one texel block of @format; 0 for a format of none */
unsigned vk_format_block_size(VkFormat format);

/* The texels across, down and deep one block of @format covers */
void vk_format_block_extent(VkFormat format, unsigned* width, unsigned* height, unsigned* depth);

#ifdef __cplusplus
}
#endif

#endif
