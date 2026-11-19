/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <string.h>

#include "video_core/renderer_vulkan/pipeline_cache_blob.h"

/* VK_PIPELINE_CACHE_HEADER_VERSION_ONE */
#define HEADER_VERSION_ONE 1u

static uint32_t read_le32(const uint8_t* p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

int pipeline_cache_blob_fits(const void* data, size_t size, uint32_t vendor_id, uint32_t device_id,
                             const uint8_t uuid[16]) {
    const uint8_t* bytes = (const uint8_t*)data;
    uint32_t header_size;
    if (bytes == NULL || size < PIPELINE_CACHE_BLOB_HEADER) {
        return 0;
    }
    header_size = read_le32(bytes);
    /* A header that says it is shorter than version one's, or longer than the blob */
    if (header_size < PIPELINE_CACHE_BLOB_HEADER || header_size > size) {
        return 0;
    }
    return read_le32(bytes + 4) == HEADER_VERSION_ONE && read_le32(bytes + 8) == vendor_id &&
           read_le32(bytes + 12) == device_id && memcmp(bytes + 16, uuid, 16) == 0;
}
