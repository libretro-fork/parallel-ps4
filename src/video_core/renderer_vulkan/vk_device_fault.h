/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef VK_DEVICE_FAULT_H
#define VK_DEVICE_FAULT_H

/* What the driver says of a device it has lost (VK_EXT_device_fault): its own
 * description, the addresses it faulted on and the vendor's codes, written to
 * the log as critical lines, which are written whatever the log's level. */

#ifndef VK_NO_PROTOTYPES
#define VK_NO_PROTOTYPES
#endif
#include <vulkan/vulkan.h>

#ifdef __cplusplus
extern "C" {
#endif

/* @get: the device's vkGetDeviceFaultInfoEXT, or null where the device has
 * not the extension, which is then all that is said. */
void vk_device_fault_report(VkDevice device, PFN_vkGetDeviceFaultInfoEXT get);

#ifdef __cplusplus
}
#endif

#endif
