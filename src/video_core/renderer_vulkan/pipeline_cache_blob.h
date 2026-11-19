/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

/* The driver's pipeline cache as kept on disk (vkGetPipelineCacheData): whether a kept blob was
 * made by this device and driver, read from its header (VkPipelineCacheHeaderVersionOne, whose
 * fields are written least significant byte first whatever the host). A blob from another GPU
 * or driver is not handed to the driver at all: some drivers have been known to fall over on
 * one, though the specification says to ignore it. */

#ifndef SHADPS4_PIPELINE_CACHE_BLOB_H
#define SHADPS4_PIPELINE_CACHE_BLOB_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Bytes of VkPipelineCacheHeaderVersionOne */
#define PIPELINE_CACHE_BLOB_HEADER 32

/* 1 if the @size bytes at @data are a pipeline cache of the device @vendor_id / @device_id
 * whose driver has the cache identity @uuid (VkPhysicalDeviceProperties::pipelineCacheUUID),
 * 0 if not, or if they are too short to say. */
int pipeline_cache_blob_fits(const void* data, size_t size, uint32_t vendor_id, uint32_t device_id,
                             const uint8_t uuid[16]);

#ifdef __cplusplus
}
#endif

#endif
