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

/* Notes what is about to be recorded for the GPU (a compute dispatch, a
 * tiling, a copy into a buffer), with up to four numbers that say which: the
 * last of these are in the report of a lost device. @what is a string that
 * lasts. Returns the note's address, the marker for a checkpoint
 * (VK_NV_device_diagnostic_checkpoints) that the report then names as the
 * last the GPU reached. For the one thread that records. */
const void* vk_device_fault_mark(const char* what, uint64_t a, uint64_t b, uint64_t c, uint64_t d);

/* @get: the device's vkGetDeviceFaultInfoEXT, or null where the device has
 * not the extension. @checkpoints: its vkGetQueueCheckpointDataNV for
 * @queue, or null. */
void vk_device_fault_report(VkDevice device, PFN_vkGetDeviceFaultInfoEXT get, VkQueue queue,
                            PFN_vkGetQueueCheckpointDataNV checkpoints);

#ifdef __cplusplus
}
#endif

#endif
