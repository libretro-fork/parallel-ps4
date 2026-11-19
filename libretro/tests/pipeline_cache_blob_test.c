/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

/* The driver's kept pipeline cache (pipeline_cache_blob.c): a blob of this device and driver is
 * taken; one of another vendor, device or driver, another header version, a header longer than
 * the blob or shorter than version one's, a blob cut short and none at all are not. The header
 * is read least significant byte first, as Vulkan writes it on any host. */

#include <stdio.h>
#include <string.h>

#include "video_core/renderer_vulkan/pipeline_cache_blob.h"

static int failures;

static void put_le32(unsigned char* p, unsigned long v) {
    p[0] = (unsigned char)(v & 0xff);
    p[1] = (unsigned char)((v >> 8) & 0xff);
    p[2] = (unsigned char)((v >> 16) & 0xff);
    p[3] = (unsigned char)((v >> 24) & 0xff);
}

static void make(unsigned char* blob, unsigned long header_size, unsigned long version,
                 unsigned long vendor, unsigned long device, const unsigned char* uuid) {
    put_le32(blob, header_size);
    put_le32(blob + 4, version);
    put_le32(blob + 8, vendor);
    put_le32(blob + 12, device);
    memcpy(blob + 16, uuid, 16);
}

static void expect(const char* what, int got, int want) {
    if (got != want) {
        printf("pipeline_cache_blob: %s: %d, not %d\n", what, got, want);
        failures++;
    }
}

int main(void) {
    unsigned char uuid[16], other[16], blob[64];
    int i;
    for (i = 0; i < 16; i++) {
        uuid[i] = (unsigned char)(0xa0 + i);
        other[i] = uuid[i];
    }
    other[15] ^= 1;
    memset(blob, 0x5a, sizeof(blob));

    make(blob, 32, 1, 0x10de, 0x2684, uuid);
    expect("this device's", pipeline_cache_blob_fits(blob, 64, 0x10de, 0x2684, uuid), 1);
    expect("this device's, the header alone", pipeline_cache_blob_fits(blob, 32, 0x10de, 0x2684, uuid), 1);
    expect("another vendor's", pipeline_cache_blob_fits(blob, 64, 0x1002, 0x2684, uuid), 0);
    expect("another device's", pipeline_cache_blob_fits(blob, 64, 0x10de, 0x2685, uuid), 0);
    expect("another driver's", pipeline_cache_blob_fits(blob, 64, 0x10de, 0x2684, other), 0);
    expect("cut short", pipeline_cache_blob_fits(blob, 31, 0x10de, 0x2684, uuid), 0);
    expect("none", pipeline_cache_blob_fits(NULL, 64, 0x10de, 0x2684, uuid), 0);

    make(blob, 32, 2, 0x10de, 0x2684, uuid);
    expect("another header version", pipeline_cache_blob_fits(blob, 64, 0x10de, 0x2684, uuid), 0);
    make(blob, 65, 1, 0x10de, 0x2684, uuid);
    expect("a header longer than the blob", pipeline_cache_blob_fits(blob, 64, 0x10de, 0x2684, uuid), 0);
    make(blob, 16, 1, 0x10de, 0x2684, uuid);
    expect("a header shorter than version one's", pipeline_cache_blob_fits(blob, 64, 0x10de, 0x2684, uuid), 0);
    /* A longer header of a later layout, still version one at its start, is taken */
    make(blob, 48, 1, 0x10de, 0x2684, uuid);
    expect("a longer header", pipeline_cache_blob_fits(blob, 64, 0x10de, 0x2684, uuid), 1);

    if (failures == 0) {
        printf("pipeline_cache_blob: all passed\n");
    }
    return failures != 0;
}
