/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

/* The game's memory lent to the GPU as Vulkan buffers, so that a readback lands in it in place.
 *
 * The game's memory is backed by one host mapping (the address space's backing). With
 * VK_EXT_external_memory_host each chunk of it can be imported as device memory and bound to a
 * buffer: the GPU then copies what it wrote straight into the game's memory, with no staging
 * buffer to take and no copy by the CPU out of it afterwards.
 *
 * Not for uploads. The game is told the GPU is done with its memory (an end-of-pipe fence) when
 * the work is recorded, not when the GPU runs it, so it may write the memory again before the
 * GPU has read it; an upload has to be a copy taken as it is recorded.
 *
 * A chunk is imported the first time it is wanted (import_on_use), or as the game maps the
 * memory it backs (guest_import_prepare), and kept until guest_import_destroy. A chunk the
 * driver will not import is noted as such and not tried again; the caller then uses a staging
 * buffer, as it always did. guest_import_prepare may be called from any thread alongside the
 * rest, which are called from one thread at a time. Nothing waits: a chunk another thread is
 * importing is not there yet, as one never imported. */

#ifndef GUEST_IMPORT_H
#define GUEST_IMPORT_H

#include <stdint.h>

#ifndef VK_NO_PROTOTYPES
#define VK_NO_PROTOTYPES
#endif
#include <vulkan/vulkan.h>

#ifdef __cplusplus
extern "C" {
#endif

/* The size of a chunk: one buffer and one device memory each */
#define GUEST_IMPORT_CHUNK_SHIFT 26 /* 64 MiB */

typedef struct guest_import guest_import_t;

typedef struct guest_import_fns {
    PFN_vkAllocateMemory allocate_memory;
    PFN_vkFreeMemory free_memory;
    PFN_vkCreateBuffer create_buffer;
    PFN_vkDestroyBuffer destroy_buffer;
    PFN_vkGetBufferMemoryRequirements get_buffer_memory_requirements;
    PFN_vkBindBufferMemory bind_buffer_memory;
    PFN_vkGetMemoryHostPointerPropertiesEXT get_memory_host_pointer_properties;
} guest_import_fns_t;

typedef struct guest_import_info {
    VkDevice device;
    VkPhysicalDeviceMemoryProperties memory;
    /* VkPhysicalDeviceExternalMemoryHostPropertiesEXT::minImportedHostPointerAlignment */
    VkDeviceSize alignment;
    /* The game's memory: its start and size, both multiples of the alignment */
    uint8_t* base;
    uint64_t size;
    /* What the buffers are for (transfer destination, storage for the tiler) */
    VkBufferUsageFlags usage;
    /* Non-zero: a chunk is imported the first time it is wanted. Zero: only by
     * guest_import_prepare, so that no import is made where the memory is wanted. */
    int import_on_use;
    guest_import_fns_t fns;
} guest_import_info_t;

/* Null if @info cannot be used (no extension functions, a base or size the driver cannot take,
 * or the table of chunks could not be made). Imports nothing yet. */
guest_import_t* guest_import_create(const guest_import_info_t* info);
/* Frees every chunk imported. The GPU must be done with them. */
void guest_import_destroy(guest_import_t* import);

/* Imports every chunk [@offset, @offset + @size) of the game's memory (offsets from its base)
 * touches that has not been tried yet. From any thread. */
void guest_import_prepare(guest_import_t* import, uint64_t offset, uint64_t size);

/* The buffer holding [@offset, @offset + @size) of the game's memory (offsets from its base),
 * and in *@buffer_offset the range's offset in it; VK_NULL_HANDLE when the range crosses from
 * one chunk into the next, lies outside the memory, or its chunk could not be imported. */
VkBuffer guest_import_buffer(guest_import_t* import, uint64_t offset, uint64_t size,
                             VkDeviceSize* buffer_offset);

/* The run of the game's address space [@addr, @addr + @size) starts with, from @addr: its
 * length, and in *@backing_offset where it lies in the backing (UINT64_MAX for a run of pages
 * without backing). Never 0 for a @size that is not. */
typedef uint64_t (*guest_import_run_fn)(void* ctx, uint64_t addr, uint64_t size,
                                        uint64_t* backing_offset);

/* The buffer [@addr, @addr + @size) of the game's address space lies in, with its chunk in
 * *@chunk and the offset in it in *@buffer_offset, which @alignment divides; VK_NULL_HANDLE
 * where it does not lie in one run of the backing in one chunk that could be imported. */
VkBuffer guest_import_target(guest_import_t* import, guest_import_run_fn run, void* run_ctx,
                             uint64_t addr, uint64_t size, uint64_t alignment, uint32_t* chunk,
                             VkDeviceSize* buffer_offset);

/* One piece of a range split by guest_import_split, @delta bytes into it: in @buffer (chunk
 * @chunk) at @buffer_offset, or with @buffer VK_NULL_HANDLE where its chunk could not be
 * imported, and then @addr is where in the game's address space it goes */
typedef void (*guest_import_piece_fn)(void* ctx, VkBuffer buffer, uint32_t chunk,
                                      VkDeviceSize buffer_offset, uint64_t addr, uint64_t delta,
                                      uint64_t size);

/* [@addr, @addr + @size) of the game's address space as pieces each in one chunk, in order.
 * Pages without backing make no piece: there is nothing there to write, as for the CPU. */
void guest_import_split(guest_import_t* import, guest_import_run_fn run, void* run_ctx,
                        uint64_t addr, uint64_t size, guest_import_piece_fn piece, void* piece_ctx);

/* Chunks imported, and chunks the driver would not take */
uint32_t guest_import_chunks(guest_import_t* import, uint32_t* failed);

#ifdef __cplusplus
}
#endif

#endif
