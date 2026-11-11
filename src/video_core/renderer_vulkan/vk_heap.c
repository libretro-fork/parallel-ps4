/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <stdlib.h>
#include <string.h>

#include "video_core/renderer_vulkan/vk_heap.h"

#if defined(_MSC_VER) && (defined(_M_X64) || defined(_M_ARM64))
#include <intrin.h>
#pragma intrinsic(_BitScanReverse64)
#endif

#define VK_HEAP_NONE 0xFFFFFFFFu
#define VK_HEAP_CLASSES 64

/* vk_heap_alloc_t states; a span never handed out is zero, spare */
#define SPAN_SPARE 0u
#define SPAN_FREE 1u
#define SPAN_USED 2u

/* vk_heap_block_t kinds */
#define BLOCK_EMPTY 0u  /* a slot with no memory */
#define BLOCK_SHARED 1u /* handed out in spans */
#define BLOCK_OWN 2u    /* one span, the whole block */

#define DEFAULT_BLOCK_SIZE ((VkDeviceSize)64 << 20)
#define MIN_BLOCK_SIZE ((VkDeviceSize)1 << 20)
#define DEFAULT_MAX_SPANS 65536u
#define DEFAULT_MAX_BLOCKS 4096u
/* Blocks made or freed before the budgets are read again */
#define BUDGET_REFRESH 32u
/* Spans of a size class looked at for a good fit before taking one that surely fits */
#define GOOD_FIT_TRIES 8u

/* Memory the renderer never asks for: left out unless a request requires it */
#define AVOIDED_FLAGS                                                                              \
    (VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT | VK_MEMORY_PROPERTY_PROTECTED_BIT |                  \
     VK_MEMORY_PROPERTY_DEVICE_COHERENT_BIT_AMD | VK_MEMORY_PROPERTY_DEVICE_UNCACHED_BIT_AMD)

typedef struct vk_heap_block {
    VkDeviceMemory memory;
    VkDeviceSize size;
    VkDeviceSize used;
    unsigned char* mapped;
    uint64_t free_mask; /* size classes with a free span */
    uint32_t free_heads[VK_HEAP_CLASSES];
    uint32_t type;
    uint32_t cls;
    uint32_t kind;
    uint32_t prev, next; /* among the shared blocks of its type and class; next spare slot */
} vk_heap_block_t;

struct vk_heap {
    vk_heap_info_t info;
    vk_heap_alloc_t* spans;
    vk_heap_block_t* blocks;
    uint32_t span_top, span_spare, spans_left;
    uint32_t block_top, block_spare;
    uint32_t shared[VK_MAX_MEMORY_TYPES][2]; /* first shared block by type and class */
    VkDeviceSize block_max[VK_MAX_MEMORY_HEAPS];
    VkDeviceSize reserved[VK_MAX_MEMORY_HEAPS];
    VkDeviceSize used[VK_MAX_MEMORY_HEAPS];
    VkDeviceSize budget[VK_MAX_MEMORY_HEAPS];
    VkDeviceSize external[VK_MAX_MEMORY_HEAPS]; /* what others use of each heap */
    VkDeviceSize atom;
    uint32_t budget_age;
};

static unsigned floor_log2(uint64_t v) {
#if defined(__GNUC__) || defined(__clang__)
    return 63u - (unsigned)__builtin_clzll(v);
#elif defined(_MSC_VER) && (defined(_M_X64) || defined(_M_ARM64))
    unsigned long index;
    _BitScanReverse64(&index, v);
    return (unsigned)index;
#else
    unsigned n = 0;
    while (v >>= 1) {
        ++n;
    }
    return n;
#endif
}

static unsigned ceil_log2(uint64_t v) {
    return v <= 1 ? 0u : floor_log2(v - 1) + 1u;
}

static unsigned popcount32(uint32_t v) {
    unsigned n = 0;
    while (v) {
        v &= v - 1;
        ++n;
    }
    return n;
}

static VkDeviceSize align_up(VkDeviceSize v, VkDeviceSize alignment) {
    return (v + alignment - 1) & ~(alignment - 1);
}

/* ---- spans ---- */

static uint32_t span_get(vk_heap_t* heap) {
    uint32_t index;
    if (!heap->spans_left) {
        return VK_HEAP_NONE;
    }
    --heap->spans_left;
    if (heap->span_spare != VK_HEAP_NONE) {
        index = heap->span_spare;
        heap->span_spare = heap->spans[index].free_next;
    } else {
        index = heap->span_top++;
    }
    return index;
}

static void span_put(vk_heap_t* heap, uint32_t index) {
    vk_heap_alloc_t* span = &heap->spans[index];
    memset(span, 0, sizeof(*span));
    span->free_next = heap->span_spare;
    heap->span_spare = index;
    ++heap->spans_left;
}

static void class_insert(vk_heap_t* heap, vk_heap_block_t* block, uint32_t index) {
    vk_heap_alloc_t* span = &heap->spans[index];
    const unsigned cls = floor_log2(span->size);
    span->state = SPAN_FREE;
    span->free_prev = VK_HEAP_NONE;
    span->free_next = block->free_heads[cls];
    if (span->free_next != VK_HEAP_NONE) {
        heap->spans[span->free_next].free_prev = index;
    }
    block->free_heads[cls] = index;
    block->free_mask |= (uint64_t)1 << cls;
}

static void class_remove(vk_heap_t* heap, vk_heap_block_t* block, uint32_t index) {
    vk_heap_alloc_t* span = &heap->spans[index];
    const unsigned cls = floor_log2(span->size);
    if (span->free_prev != VK_HEAP_NONE) {
        heap->spans[span->free_prev].free_next = span->free_next;
    } else {
        block->free_heads[cls] = span->free_next;
        if (span->free_next == VK_HEAP_NONE) {
            block->free_mask &= ~((uint64_t)1 << cls);
        }
    }
    if (span->free_next != VK_HEAP_NONE) {
        heap->spans[span->free_next].free_prev = span->free_prev;
    }
    span->free_prev = span->free_next = VK_HEAP_NONE;
}

static int span_fits(const vk_heap_alloc_t* span, VkDeviceSize size, VkDeviceSize alignment) {
    const VkDeviceSize start = align_up(span->offset, alignment);
    return start + size <= span->offset + span->size;
}

/* Takes @size bytes at @alignment from shared block @bi: the span, or NONE if none fits or
 * the spans to split it with are lacking (nothing changed then). */
static uint32_t block_take(vk_heap_t* heap, uint32_t bi, VkDeviceSize size,
                           VkDeviceSize alignment) {
    vk_heap_block_t* block = &heap->blocks[bi];
    vk_heap_alloc_t* span;
    VkDeviceSize start, pad, tail;
    uint32_t index = VK_HEAP_NONE, tries, extra, piece;
    unsigned cls, sure;
    uint64_t mask;

    if (!block->free_mask || size > block->size) {
        return VK_HEAP_NONE;
    }
    /* Every span of class sure and above fits whatever its offset; below it, some may */
    sure = ceil_log2(size + alignment - 1);
    for (cls = floor_log2(size); cls < sure && cls < VK_HEAP_CLASSES && index == VK_HEAP_NONE;
         ++cls) {
        if (!(block->free_mask & ((uint64_t)1 << cls))) {
            continue;
        }
        piece = block->free_heads[cls];
        for (tries = 0; piece != VK_HEAP_NONE && tries < GOOD_FIT_TRIES; ++tries) {
            if (span_fits(&heap->spans[piece], size, alignment)) {
                index = piece;
                break;
            }
            piece = heap->spans[piece].free_next;
        }
    }
    if (index == VK_HEAP_NONE && sure < VK_HEAP_CLASSES) {
        mask = block->free_mask & ~(((uint64_t)1 << sure) - 1);
        if (mask) {
            index = block->free_heads[floor_log2(mask & (~mask + 1))];
        }
    }
    if (index == VK_HEAP_NONE) {
        return VK_HEAP_NONE;
    }

    span = &heap->spans[index];
    start = align_up(span->offset, alignment);
    pad = start - span->offset;
    tail = span->offset + span->size - (start + size);
    extra = (pad != 0) + (tail != 0);
    if (heap->spans_left < extra) {
        return VK_HEAP_NONE;
    }
    class_remove(heap, block, index);
    if (pad) {
        piece = span_get(heap);
        heap->spans[piece].offset = span->offset;
        heap->spans[piece].size = pad;
        heap->spans[piece].block = bi;
        heap->spans[piece].prev = span->prev;
        heap->spans[piece].next = index;
        if (span->prev != VK_HEAP_NONE) {
            heap->spans[span->prev].next = piece;
        }
        span->prev = piece;
        span->offset = start;
        span->size -= pad;
        class_insert(heap, block, piece);
    }
    if (tail) {
        piece = span_get(heap);
        heap->spans[piece].offset = start + size;
        heap->spans[piece].size = tail;
        heap->spans[piece].block = bi;
        heap->spans[piece].prev = index;
        heap->spans[piece].next = span->next;
        if (span->next != VK_HEAP_NONE) {
            heap->spans[span->next].prev = piece;
        }
        span->next = piece;
        span->size = size;
        class_insert(heap, block, piece);
    }
    span->state = SPAN_USED;
    block->used += size;
    return index;
}

/* Gives span @index back to its shared block, merged with free neighbours. */
static void block_give(vk_heap_t* heap, vk_heap_block_t* block, uint32_t index) {
    vk_heap_alloc_t* span = &heap->spans[index];
    uint32_t other;
    block->used -= span->size;
    other = span->prev;
    if (other != VK_HEAP_NONE && heap->spans[other].state == SPAN_FREE) {
        class_remove(heap, block, other);
        heap->spans[other].size += span->size;
        heap->spans[other].next = span->next;
        if (span->next != VK_HEAP_NONE) {
            heap->spans[span->next].prev = other;
        }
        span_put(heap, index);
        index = other;
        span = &heap->spans[index];
    }
    other = span->next;
    if (other != VK_HEAP_NONE && heap->spans[other].state == SPAN_FREE) {
        class_remove(heap, block, other);
        span->size += heap->spans[other].size;
        span->next = heap->spans[other].next;
        if (span->next != VK_HEAP_NONE) {
            heap->spans[span->next].prev = index;
        }
        span_put(heap, other);
    }
    span->memory = VK_NULL_HANDLE;
    span->mapped = NULL;
    class_insert(heap, block, index);
}

/* ---- blocks ---- */

static void refresh_budget(vk_heap_t* heap) {
    VkPhysicalDeviceMemoryBudgetPropertiesEXT budget;
    VkPhysicalDeviceMemoryProperties2 props;
    uint32_t i;
    heap->budget_age = 0;
    if (heap->info.fns.get_memory_properties2) {
        memset(&budget, 0, sizeof(budget));
        budget.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_BUDGET_PROPERTIES_EXT;
        memset(&props, 0, sizeof(props));
        props.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_PROPERTIES_2;
        props.pNext = &budget;
        heap->info.fns.get_memory_properties2(heap->info.physical_device, &props);
        for (i = 0; i < heap->info.memory.memoryHeapCount; ++i) {
            heap->budget[i] = budget.heapBudget[i] ? budget.heapBudget[i]
                                                   : heap->info.memory.memoryHeaps[i].size / 10 * 8;
            heap->external[i] = budget.heapUsage[i] > heap->reserved[i]
                                    ? budget.heapUsage[i] - heap->reserved[i]
                                    : 0;
        }
        return;
    }
    for (i = 0; i < heap->info.memory.memoryHeapCount; ++i) {
        heap->budget[i] = heap->info.memory.memoryHeaps[i].size / 10 * 8;
        heap->external[i] = 0;
    }
}

static void kind_link(vk_heap_t* heap, uint32_t bi) {
    vk_heap_block_t* block = &heap->blocks[bi];
    uint32_t* head = &heap->shared[block->type][block->cls];
    block->prev = VK_HEAP_NONE;
    block->next = *head;
    if (*head != VK_HEAP_NONE) {
        heap->blocks[*head].prev = bi;
    }
    *head = bi;
}

static void kind_unlink(vk_heap_t* heap, uint32_t bi) {
    vk_heap_block_t* block = &heap->blocks[bi];
    if (block->prev != VK_HEAP_NONE) {
        heap->blocks[block->prev].next = block->next;
    } else {
        heap->shared[block->type][block->cls] = block->next;
    }
    if (block->next != VK_HEAP_NONE) {
        heap->blocks[block->next].prev = block->prev;
    }
}

static void block_release(vk_heap_t* heap, uint32_t bi) {
    vk_heap_block_t* block = &heap->blocks[bi];
    const uint32_t memory_heap = heap->info.memory.memoryTypes[block->type].heapIndex;
    uint32_t index, next;
    if (block->mapped) {
        heap->info.fns.unmap_memory(heap->info.device, block->memory);
    }
    heap->info.fns.free_memory(heap->info.device, block->memory, NULL);
    heap->reserved[memory_heap] -= block->size;
    ++heap->budget_age;
    if (block->kind == BLOCK_SHARED) {
        kind_unlink(heap, bi);
    }
    /* Its spans: one free span when empty; all of them when the heap goes */
    for (index = 0; index < VK_HEAP_CLASSES; ++index) {
        while (block->free_heads[index] != VK_HEAP_NONE) {
            next = heap->spans[block->free_heads[index]].free_next;
            span_put(heap, block->free_heads[index]);
            block->free_heads[index] = next;
        }
    }
    memset(block, 0, sizeof(*block));
    block->next = heap->block_spare;
    heap->block_spare = bi;
}

/* A new block, its first span in *span: the whole of it, free (shared) or handed out (own). */
static VkResult block_create(vk_heap_t* heap, uint32_t type, uint32_t cls, VkDeviceSize size,
                             uint32_t kind, const vk_heap_request_t* request, uint32_t* out,
                             uint32_t* span) {
    VkMemoryAllocateInfo info;
    VkMemoryAllocateFlagsInfo flags_info;
    VkMemoryDedicatedAllocateInfo dedicated;
    const void** chain;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    vk_heap_block_t* block;
    uint32_t bi, i, memory_heap = heap->info.memory.memoryTypes[type].heapIndex;
    int for_buffers;
    VkResult result;

    /* One span to hand out or keep free; a free slot */
    if (!heap->spans_left ||
        (heap->block_spare == VK_HEAP_NONE && heap->block_top == heap->info.max_blocks)) {
        return VK_ERROR_OUT_OF_HOST_MEMORY;
    }
    if (heap->budget_age >= BUDGET_REFRESH) {
        refresh_budget(heap);
    }
    if ((request->flags & VK_HEAP_WITHIN_BUDGET) &&
        heap->external[memory_heap] + heap->reserved[memory_heap] + size >
            heap->budget[memory_heap]) {
        return VK_ERROR_OUT_OF_DEVICE_MEMORY;
    }

    memset(&info, 0, sizeof(info));
    info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    info.allocationSize = size;
    info.memoryTypeIndex = type;
    chain = &info.pNext;
    /* A block of the linear class may hold buffers; an own block, the buffer it is for */
    for_buffers = kind == BLOCK_OWN ? request->image == VK_NULL_HANDLE
                                    : (cls == 0 || heap->info.buffer_image_granularity <= 1);
    if (heap->info.device_address && for_buffers) {
        memset(&flags_info, 0, sizeof(flags_info));
        flags_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO;
        flags_info.flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT;
        *chain = &flags_info;
        chain = &flags_info.pNext;
    }
    if (kind == BLOCK_OWN && (request->image || request->buffer)) {
        memset(&dedicated, 0, sizeof(dedicated));
        dedicated.sType = VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO;
        dedicated.image = request->image;
        dedicated.buffer = request->buffer;
        *chain = &dedicated;
    }
    result = heap->info.fns.allocate_memory(heap->info.device, &info, NULL, &memory);
    if (result != VK_SUCCESS) {
        return result;
    }

    if (heap->block_spare != VK_HEAP_NONE) {
        bi = heap->block_spare;
        heap->block_spare = heap->blocks[bi].next;
    } else {
        bi = heap->block_top++;
    }
    block = &heap->blocks[bi];
    block->memory = memory;
    block->size = size;
    block->used = 0;
    block->mapped = NULL;
    block->free_mask = 0;
    for (i = 0; i < VK_HEAP_CLASSES; ++i) {
        block->free_heads[i] = VK_HEAP_NONE;
    }
    block->type = type;
    block->cls = cls;
    block->kind = kind;
    block->prev = block->next = VK_HEAP_NONE;
    heap->reserved[memory_heap] += size;
    ++heap->budget_age;

    i = span_get(heap);
    heap->spans[i].offset = 0;
    heap->spans[i].size = size;
    heap->spans[i].block = bi;
    heap->spans[i].prev = heap->spans[i].next = VK_HEAP_NONE;
    if (kind == BLOCK_SHARED) {
        class_insert(heap, block, i);
        kind_link(heap, bi);
    } else {
        heap->spans[i].state = SPAN_USED;
        block->used = size;
    }
    *out = bi;
    *span = i;
    return VK_SUCCESS;
}

/* Size for a new shared block that is to hold @need bytes: halved from the largest, as
 * VulkanMemoryAllocator does, while it stays over every block of its kind and twice @need,
 * so a game that needs little is given little; and halved further while over @headroom. */
static VkDeviceSize shared_block_size(const vk_heap_t* heap, uint32_t type, uint32_t cls,
                                      VkDeviceSize need, VkDeviceSize headroom) {
    const uint32_t memory_heap = heap->info.memory.memoryTypes[type].heapIndex;
    VkDeviceSize size = heap->block_max[memory_heap], largest = 0;
    uint32_t bi, i;
    for (bi = heap->shared[type][cls]; bi != VK_HEAP_NONE; bi = heap->blocks[bi].next) {
        if (heap->blocks[bi].size > largest) {
            largest = heap->blocks[bi].size;
        }
    }
    for (i = 0; i < 3; ++i) {
        if (size / 2 <= largest || size / 2 < need * 2 || size / 2 < MIN_BLOCK_SIZE) {
            break;
        }
        size /= 2;
    }
    while (size > headroom && size / 2 >= need * 2 && size / 2 >= MIN_BLOCK_SIZE) {
        size /= 2;
    }
    return size;
}

/* Frees the shared blocks with nothing in them, of memory heap @memory_heap. */
static uint32_t trim(vk_heap_t* heap, uint32_t memory_heap) {
    uint32_t type, cls, bi, next, freed = 0;
    for (type = 0; type < heap->info.memory.memoryTypeCount; ++type) {
        if (heap->info.memory.memoryTypes[type].heapIndex != memory_heap) {
            continue;
        }
        for (cls = 0; cls < 2; ++cls) {
            for (bi = heap->shared[type][cls]; bi != VK_HEAP_NONE; bi = next) {
                next = heap->blocks[bi].next;
                if (!heap->blocks[bi].used) {
                    block_release(heap, bi);
                    ++freed;
                }
            }
        }
    }
    return freed;
}

/* @size more of @memory_heap would go over its budget, and @request asks to stay within */
static int over_budget(vk_heap_t* heap, const vk_heap_request_t* request, uint32_t memory_heap,
                       VkDeviceSize size) {
    if (!(request->flags & VK_HEAP_WITHIN_BUDGET)) {
        return 0;
    }
    if (heap->budget_age >= BUDGET_REFRESH) {
        refresh_budget(heap);
    }
    return heap->external[memory_heap] + heap->reserved[memory_heap] + size >
           heap->budget[memory_heap];
}

static VkResult block_map(vk_heap_t* heap, vk_heap_block_t* block) {
    void* data = NULL;
    VkResult result;
    if (block->mapped) {
        return VK_SUCCESS;
    }
    result =
        heap->info.fns.map_memory(heap->info.device, block->memory, 0, VK_WHOLE_SIZE, 0, &data);
    if (result == VK_SUCCESS) {
        block->mapped = (unsigned char*)data;
    }
    return result;
}

/* ---- the interface ---- */

vk_heap_t* vk_heap_create(const vk_heap_info_t* info) {
    vk_heap_t* heap;
    uint32_t i;
    VkDeviceSize block_max;
    if (!info || !info->device || !info->fns.allocate_memory || !info->fns.free_memory ||
        !info->fns.map_memory || !info->fns.unmap_memory || !info->fns.flush_ranges ||
        !info->fns.invalidate_ranges || !info->memory.memoryTypeCount ||
        info->memory.memoryTypeCount > VK_MAX_MEMORY_TYPES || !info->memory.memoryHeapCount ||
        info->memory.memoryHeapCount > VK_MAX_MEMORY_HEAPS) {
        return NULL;
    }
    for (i = 0; i < info->memory.memoryTypeCount; ++i) {
        if (info->memory.memoryTypes[i].heapIndex >= info->memory.memoryHeapCount) {
            return NULL;
        }
    }
    heap = (vk_heap_t*)calloc(1, sizeof(*heap));
    if (!heap) {
        return NULL;
    }
    heap->info = *info;
    if (!heap->info.max_spans) {
        heap->info.max_spans = DEFAULT_MAX_SPANS;
    }
    if (!heap->info.max_blocks) {
        heap->info.max_blocks = DEFAULT_MAX_BLOCKS;
    }
    if (!heap->info.block_size) {
        heap->info.block_size = DEFAULT_BLOCK_SIZE;
    }
    heap->atom = info->non_coherent_atom_size ? info->non_coherent_atom_size : 1;
    if (heap->atom & (heap->atom - 1)) {
        free(heap);
        return NULL;
    }
    /* calloc: the pages are the system's zero pages until a span or block is first used */
    heap->spans = (vk_heap_alloc_t*)calloc(heap->info.max_spans, sizeof(*heap->spans));
    heap->blocks = (vk_heap_block_t*)calloc(heap->info.max_blocks, sizeof(*heap->blocks));
    if (!heap->spans || !heap->blocks) {
        free(heap->spans);
        free(heap->blocks);
        free(heap);
        return NULL;
    }
    heap->span_spare = heap->block_spare = VK_HEAP_NONE;
    heap->spans_left = heap->info.max_spans;
    for (i = 0; i < VK_MAX_MEMORY_TYPES; ++i) {
        heap->shared[i][0] = heap->shared[i][1] = VK_HEAP_NONE;
    }
    for (i = 0; i < info->memory.memoryHeapCount; ++i) {
        block_max = info->memory.memoryHeaps[i].size / 8;
        if (block_max > heap->info.block_size) {
            block_max = heap->info.block_size;
        }
        block_max &= ~(MIN_BLOCK_SIZE - 1);
        heap->block_max[i] = block_max < MIN_BLOCK_SIZE ? MIN_BLOCK_SIZE : block_max;
    }
    refresh_budget(heap);
    return heap;
}

void vk_heap_destroy(vk_heap_t* heap) {
    uint32_t bi;
    if (!heap) {
        return;
    }
    for (bi = 0; bi < heap->block_top; ++bi) {
        if (heap->blocks[bi].kind != BLOCK_EMPTY) {
            if (heap->blocks[bi].mapped) {
                heap->info.fns.unmap_memory(heap->info.device, heap->blocks[bi].memory);
            }
            heap->info.fns.free_memory(heap->info.device, heap->blocks[bi].memory, NULL);
        }
    }
    free(heap->spans);
    free(heap->blocks);
    free(heap);
}

VkResult vk_heap_alloc(vk_heap_t* heap, const VkMemoryRequirements* requirements,
                       int driver_dedicated, const vk_heap_request_t* request,
                       vk_heap_alloc_t** out) {
    uint32_t order[VK_MAX_MEMORY_TYPES];
    unsigned cost[VK_MAX_MEMORY_TYPES];
    uint32_t count = 0, n, i, type, cls, bi, index, memory_heap, attempt;
    VkMemoryPropertyFlags required, flags;
    VkDeviceSize size, alignment, headroom, used;
    VkResult result = VK_ERROR_OUT_OF_DEVICE_MEMORY, tried;
    vk_heap_block_t* block;
    vk_heap_alloc_t* span;
    unsigned type_cost, refused_cost = 0;
    int own, mapped, refused = 0;

    if (!out) {
        return VK_ERROR_INITIALIZATION_FAILED;
    }
    *out = NULL;
    if (!heap || !requirements || !request || !requirements->size ||
        (requirements->alignment & (requirements->alignment - 1))) {
        return VK_ERROR_INITIALIZATION_FAILED;
    }
    mapped = (request->flags & VK_HEAP_MAPPED) != 0;
    required = request->required | (mapped ? VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT : 0);

    /* The allowed types by cost, lowest first and in the driver's order among equals */
    for (type = 0; type < heap->info.memory.memoryTypeCount; ++type) {
        flags = heap->info.memory.memoryTypes[type].propertyFlags;
        if (!(requirements->memoryTypeBits & ((uint32_t)1 << type)) ||
            (flags & required) != required || (flags & AVOIDED_FLAGS & ~required)) {
            continue;
        }
        type_cost =
            popcount32(request->preferred & ~flags) + popcount32(request->not_preferred & flags);
        for (n = count; n > 0 && cost[n - 1] > type_cost; --n) {
            order[n] = order[n - 1];
            cost[n] = cost[n - 1];
        }
        cost[n] = type_cost;
        order[n] = type;
        ++count;
    }
    if (!count) {
        return VK_ERROR_FEATURE_NOT_PRESENT;
    }

    /* A second round, when the driver ran out, after giving back the empty blocks */
    for (attempt = 0; attempt < 2; ++attempt) {
        for (n = 0; n < count; ++n) {
            /* Refused for the budget: no falling back to a type it prefers less (system
             * memory for an image), where it would stay for good. The caller is told, frees
             * what it can or asks again over the budget, in the type it prefers. */
            if (refused && cost[n] > refused_cost) {
                break;
            }
            type = order[n];
            flags = heap->info.memory.memoryTypes[type].propertyFlags;
            memory_heap = heap->info.memory.memoryTypes[type].heapIndex;
            cls = (heap->info.buffer_image_granularity > 1 && !(request->flags & VK_HEAP_LINEAR))
                      ? 1u
                      : 0u;
            size = requirements->size;
            alignment = requirements->alignment ? requirements->alignment : 1;
            /* A flush or invalidate, widened to the atom, must not reach a neighbour */
            if ((flags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) &&
                !(flags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)) {
                if (alignment < heap->atom) {
                    alignment = heap->atom;
                }
                size = align_up(size, heap->atom);
            }
            own = (request->flags & VK_HEAP_DEDICATED) || driver_dedicated ||
                  size > heap->block_max[memory_heap] / 2;
            index = VK_HEAP_NONE;
            bi = VK_HEAP_NONE;
            if (!own) {
                for (bi = heap->shared[type][cls]; bi != VK_HEAP_NONE; bi = heap->blocks[bi].next) {
                    index = block_take(heap, bi, size, alignment);
                    if (index != VK_HEAP_NONE) {
                        break;
                    }
                }
                if (index == VK_HEAP_NONE && over_budget(heap, request, memory_heap, size)) {
                    refused = 1;
                    refused_cost = cost[n];
                    result = VK_ERROR_OUT_OF_DEVICE_MEMORY;
                    continue;
                }
                if (index == VK_HEAP_NONE) {
                    if (heap->budget_age >= BUDGET_REFRESH) {
                        refresh_budget(heap);
                    }
                    headroom = ~(VkDeviceSize)0;
                    if (request->flags & VK_HEAP_WITHIN_BUDGET) {
                        used = heap->external[memory_heap] + heap->reserved[memory_heap];
                        headroom =
                            used < heap->budget[memory_heap] ? heap->budget[memory_heap] - used : 0;
                    }
                    tried =
                        block_create(heap, type, cls,
                                     shared_block_size(heap, type, cls, size + alignment, headroom),
                                     BLOCK_SHARED, request, &bi, &index);
                    if (tried == VK_SUCCESS) {
                        index = block_take(heap, bi, size, alignment);
                        if (index == VK_HEAP_NONE) {
                            block_release(heap, bi);
                            tried = VK_ERROR_OUT_OF_HOST_MEMORY;
                        }
                    }
                    if (tried != VK_SUCCESS) {
                        result = tried;
                    }
                }
            }
            /* Memory of its own: asked for, or where no block could be made */
            if (index == VK_HEAP_NONE && own && over_budget(heap, request, memory_heap, size)) {
                refused = 1;
                refused_cost = cost[n];
                result = VK_ERROR_OUT_OF_DEVICE_MEMORY;
                continue;
            }
            if (index == VK_HEAP_NONE) {
                tried = block_create(heap, type, cls, size, BLOCK_OWN, request, &bi, &index);
                if (tried != VK_SUCCESS) {
                    result = tried;
                    continue;
                }
            }
            block = &heap->blocks[bi];
            span = &heap->spans[index];
            if (mapped || block->mapped) {
                tried = block_map(heap, block);
                if (tried != VK_SUCCESS) {
                    if (block->kind == BLOCK_OWN) {
                        span_put(heap, index);
                        block_release(heap, bi);
                    } else {
                        block_give(heap, block, index);
                    }
                    return tried;
                }
            }
            span->memory = block->memory;
            span->mapped = block->mapped ? block->mapped + span->offset : NULL;
            span->flags = flags;
            span->type = type;
            heap->used[memory_heap] += span->size;
            *out = span;
            return VK_SUCCESS;
        }
        /* Empty blocks kept for reuse count against the budget and the driver's memory */
        for (i = 0, n = 0; i < heap->info.memory.memoryHeapCount; ++i) {
            n += trim(heap, i);
        }
        if (!n) {
            break;
        }
    }
    return result;
}

void vk_heap_free(vk_heap_t* heap, vk_heap_alloc_t* alloc) {
    vk_heap_block_t* block;
    uint32_t index, bi, other, memory_heap;
    if (!heap || !alloc || alloc < heap->spans || alloc >= heap->spans + heap->span_top ||
        alloc->state != SPAN_USED) {
        return;
    }
    index = (uint32_t)(alloc - heap->spans);
    bi = alloc->block;
    block = &heap->blocks[bi];
    memory_heap = heap->info.memory.memoryTypes[block->type].heapIndex;
    heap->used[memory_heap] -= alloc->size;
    if (block->kind == BLOCK_OWN) {
        block->used = 0;
        span_put(heap, index);
        block_release(heap, bi);
        return;
    }
    block_give(heap, block, index);
    if (block->used) {
        return;
    }
    /* Keep one empty block of a kind, the largest, for what comes next */
    for (other = heap->shared[block->type][block->cls]; other != VK_HEAP_NONE;
         other = heap->blocks[other].next) {
        if (other == bi || heap->blocks[other].used) {
            continue;
        }
        if (heap->blocks[other].size >= block->size) {
            block_release(heap, bi);
        } else {
            block_release(heap, other);
        }
        return;
    }
}

static VkResult mapped_range(const vk_heap_t* heap, const vk_heap_alloc_t* alloc,
                             VkDeviceSize offset, VkDeviceSize size, VkMappedMemoryRange* range) {
    const VkDeviceSize block_size = heap->blocks[alloc->block].size;
    VkDeviceSize begin, end;
    if (!(alloc->flags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) ||
        (alloc->flags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) || !alloc->mapped ||
        offset >= alloc->size) {
        return VK_NOT_READY;
    }
    if (size == VK_WHOLE_SIZE || size > alloc->size - offset) {
        size = alloc->size - offset;
    }
    begin = (alloc->offset + offset) & ~(heap->atom - 1);
    end = align_up(alloc->offset + offset + size, heap->atom);
    memset(range, 0, sizeof(*range));
    range->sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
    range->memory = alloc->memory;
    range->offset = begin;
    range->size = end >= block_size ? block_size - begin : end - begin;
    return VK_SUCCESS;
}

VkResult vk_heap_flush(const vk_heap_t* heap, const vk_heap_alloc_t* alloc, VkDeviceSize offset,
                       VkDeviceSize size) {
    VkMappedMemoryRange range;
    if (!heap || !alloc || mapped_range(heap, alloc, offset, size, &range) != VK_SUCCESS) {
        return VK_SUCCESS;
    }
    return heap->info.fns.flush_ranges(heap->info.device, 1, &range);
}

VkResult vk_heap_invalidate(const vk_heap_t* heap, const vk_heap_alloc_t* alloc,
                            VkDeviceSize offset, VkDeviceSize size) {
    VkMappedMemoryRange range;
    if (!heap || !alloc || mapped_range(heap, alloc, offset, size, &range) != VK_SUCCESS) {
        return VK_SUCCESS;
    }
    return heap->info.fns.invalidate_ranges(heap->info.device, 1, &range);
}

void vk_heap_usage(const vk_heap_t* heap, uint32_t index, VkDeviceSize* reserved,
                   VkDeviceSize* used) {
    const int valid = heap && index < heap->info.memory.memoryHeapCount;
    if (reserved) {
        *reserved = valid ? heap->reserved[index] : 0;
    }
    if (used) {
        *used = valid ? heap->used[index] : 0;
    }
}
