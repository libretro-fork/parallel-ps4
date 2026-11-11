/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

/* The renderer's Vulkan memory: device memory taken from the driver in large blocks and handed
 * out in pieces (spans), each block mapped once, the first time a piece of it is wanted
 * mapped, and kept so for as long as it lives. Where VulkanMemoryAllocator stood.
 *
 * Nothing is allocated from the C heap after vk_heap_create: the spans and blocks are fixed
 * tables, made then and touched as they are first used, so their pages are only resident once
 * needed. A free span is found through 64 lists by size class with a bit mask of the ones in
 * use (two-level segregated fit), in a few steps whatever the number of spans; a freed span is
 * merged with free neighbours at once.
 *
 * An allocation is the span itself (vk_heap_alloc_t): its memory, offset, size, mapped pointer
 * and property flags are read off it directly, with no call and no copy, for as long as it is
 * not freed. Not thread-safe: the caller serializes vk_heap_alloc and vk_heap_free (the
 * renderer runs them in a Common::Combiner). vk_heap_flush and vk_heap_invalidate read only
 * what does not change while the span lives and can be called from any thread. */

#ifndef VK_HEAP_H
#define VK_HEAP_H

#include <stddef.h>
#include <stdint.h>

#ifndef VK_NO_PROTOTYPES
#define VK_NO_PROTOTYPES
#endif
#include <vulkan/vulkan.h>

#ifdef __cplusplus
extern "C" {
#endif

/* vk_heap_request_t flags */
#define VK_HEAP_MAPPED 0x01u        /* host visible memory, mapped: alloc->mapped is set */
#define VK_HEAP_DEDICATED 0x02u     /* a VkDeviceMemory of its own */
#define VK_HEAP_WITHIN_BUDGET 0x04u /* fail rather than go over the heap's budget */
#define VK_HEAP_LINEAR 0x08u        /* for a buffer or a linear image (bufferImageGranularity) */

typedef struct vk_heap vk_heap_t;

typedef struct vk_heap_fns {
    PFN_vkAllocateMemory allocate_memory;
    PFN_vkFreeMemory free_memory;
    PFN_vkMapMemory map_memory;
    PFN_vkUnmapMemory unmap_memory;
    PFN_vkFlushMappedMemoryRanges flush_ranges;
    PFN_vkInvalidateMappedMemoryRanges invalidate_ranges;
    /* With VK_EXT_memory_budget enabled: the heaps' budgets are read through it. Else null,
     * and a heap's budget is 80% of its size. */
    PFN_vkGetPhysicalDeviceMemoryProperties2 get_memory_properties2;
} vk_heap_fns_t;

typedef struct vk_heap_info {
    VkPhysicalDevice physical_device;
    VkDevice device;
    VkPhysicalDeviceMemoryProperties memory;
    VkDeviceSize buffer_image_granularity;
    VkDeviceSize non_coherent_atom_size;
    /* Largest block made for pieces, 0 for 64 MiB; at most 1/8 of a memory heap. The first
     * blocks of a kind are smaller, doubling up to it, and a request over half of it gets
     * memory of its own. */
    VkDeviceSize block_size;
    /* 0 for the defaults: 65536 spans, 4096 blocks */
    uint32_t max_spans;
    uint32_t max_blocks;
    /* Blocks that may hold buffers are allocated with VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT
     * (bufferDeviceAddress is enabled) */
    int device_address;
    vk_heap_fns_t fns;
} vk_heap_info_t;

typedef struct vk_heap_request {
    VkMemoryPropertyFlags required;
    VkMemoryPropertyFlags preferred;
    VkMemoryPropertyFlags not_preferred;
    unsigned flags; /* VK_HEAP_* */
    /* What a VK_HEAP_DEDICATED allocation is for (VkMemoryDedicatedAllocateInfo), or null */
    VkImage image;
    VkBuffer buffer;
} vk_heap_request_t;

/* A span of a block. The first six fields are what an allocation is; read them, change none. */
typedef struct vk_heap_span {
    VkDeviceMemory memory;
    VkDeviceSize offset;
    VkDeviceSize size;
    void* mapped; /* at offset, for VK_HEAP_MAPPED or host visible memory already mapped */
    VkMemoryPropertyFlags flags;
    uint32_t type;
    /* the heap's own */
    uint32_t block;
    uint32_t prev, next;           /* by address in the block */
    uint32_t free_prev, free_next; /* in its size class while free; next spare while spare */
    uint32_t state;
} vk_heap_alloc_t;

/* Null if @info is incomplete or the tables cannot be made. */
vk_heap_t* vk_heap_create(const vk_heap_info_t* info);
/* Frees every block. Spans still allocated are freed with them. */
void vk_heap_destroy(vk_heap_t* heap);

/* Memory for @requirements, of the lowest cost type among those allowed: a required flag
 * missing rules a type out, each preferred flag missing and each not preferred flag present
 * costs one. @driver_dedicated: the driver prefers or requires memory of its own for it
 * (VkMemoryDedicatedRequirements). On failure *out is null and nothing has changed. */
VkResult vk_heap_alloc(vk_heap_t* heap, const VkMemoryRequirements* requirements,
                       int driver_dedicated, const vk_heap_request_t* request,
                       vk_heap_alloc_t** out);
/* Null is ignored; so is a span that is not allocated. */
void vk_heap_free(vk_heap_t* heap, vk_heap_alloc_t* alloc);

/* Flushes or invalidates @size bytes from @offset into @alloc (VK_WHOLE_SIZE to its end),
 * widened to the device's nonCoherentAtomSize. Nothing to do for coherent memory. */
VkResult vk_heap_flush(const vk_heap_t* heap, const vk_heap_alloc_t* alloc, VkDeviceSize offset,
                       VkDeviceSize size);
VkResult vk_heap_invalidate(const vk_heap_t* heap, const vk_heap_alloc_t* alloc,
                            VkDeviceSize offset, VkDeviceSize size);

/* Bytes the heap holds of memory heap @index from the driver, and of those, handed out. */
void vk_heap_usage(const vk_heap_t* heap, uint32_t index, VkDeviceSize* reserved,
                   VkDeviceSize* used);

#ifdef __cplusplus
}
#endif

#endif
