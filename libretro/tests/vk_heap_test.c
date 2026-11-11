/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

/* The renderer's Vulkan memory heap (src/video_core/renderer_vulkan/vk_heap.c), tested on its
 * own against a stub driver: the type each kind of request lands in, pieces of one block
 * aligned and apart, freed pieces merged back, blocks growing from small, large and dedicated
 * requests getting memory of their own, noncoherent memory widened to the atom, the budget,
 * a driver that runs out, and 200 000 random allocations and frees never overlapping. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../src/video_core/renderer_vulkan/vk_heap.h"

static int failures = 0;

#define CHECK(cond, what)                                                                      \
    do {                                                                                       \
        if (!(cond)) {                                                                         \
            printf("FAIL %s (line %d)\n", what, __LINE__);                                     \
            ++failures;                                                                        \
        }                                                                                      \
    } while (0)

#define MiB ((VkDeviceSize)1 << 20)

/* ---- the stub driver ---- */

typedef struct stub_memory {
    VkDeviceSize size;
    uint32_t type;
    unsigned char* data;
    int mapped;
    int dedicated;
    int device_address;
} stub_memory;

static long live_memories = 0;
static long allocations = 0;
static long fail_after = -1; /* allocations left before the driver runs out; -1 never */
static int device_full = 0;  /* the driver refuses device local memory (types 0 and 3) */
static VkDeviceSize usage_by_others = 0;
static VkDeviceSize heap0_budget = 0;
static VkMappedMemoryRange last_range;
static long flushes = 0;

static VkResult VKAPI_CALL stub_allocate(VkDevice device, const VkMemoryAllocateInfo* info,
                                         const VkAllocationCallbacks* callbacks,
                                         VkDeviceMemory* out) {
    const VkBaseInStructure* next = (const VkBaseInStructure*)info->pNext;
    stub_memory* memory;
    (void)device;
    (void)callbacks;
    if (fail_after == 0 ||
        (device_full && (info->memoryTypeIndex == 0 || info->memoryTypeIndex == 3))) {
        return VK_ERROR_OUT_OF_DEVICE_MEMORY;
    }
    if (fail_after > 0) {
        --fail_after;
    }
    memory = (stub_memory*)calloc(1, sizeof(*memory));
    memory->size = info->allocationSize;
    memory->type = info->memoryTypeIndex;
    memory->data = (unsigned char*)calloc(1, (size_t)info->allocationSize);
    for (; next; next = next->pNext) {
        if (next->sType == VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO) {
            memory->dedicated = 1;
        } else if (next->sType == VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO) {
            memory->device_address = 1;
        }
    }
    ++live_memories;
    ++allocations;
    *out = (VkDeviceMemory)(uintptr_t)memory;
    return VK_SUCCESS;
}

static void VKAPI_CALL stub_free(VkDevice device, VkDeviceMemory handle,
                                 const VkAllocationCallbacks* callbacks) {
    stub_memory* memory = (stub_memory*)(uintptr_t)handle;
    (void)device;
    (void)callbacks;
    CHECK(!memory->mapped, "memory unmapped before it is freed");
    free(memory->data);
    free(memory);
    --live_memories;
}

static VkResult VKAPI_CALL stub_map(VkDevice device, VkDeviceMemory handle, VkDeviceSize offset,
                                    VkDeviceSize size, VkMemoryMapFlags flags, void** data) {
    stub_memory* memory = (stub_memory*)(uintptr_t)handle;
    (void)device;
    (void)flags;
    CHECK(!memory->mapped, "memory mapped once");
    CHECK(offset == 0 && size == VK_WHOLE_SIZE, "whole block mapped");
    memory->mapped = 1;
    *data = memory->data;
    return VK_SUCCESS;
}

static void VKAPI_CALL stub_unmap(VkDevice device, VkDeviceMemory handle) {
    stub_memory* memory = (stub_memory*)(uintptr_t)handle;
    (void)device;
    memory->mapped = 0;
}

static VkResult VKAPI_CALL stub_flush(VkDevice device, uint32_t count,
                                      const VkMappedMemoryRange* ranges) {
    (void)device;
    (void)count;
    last_range = ranges[0];
    ++flushes;
    return VK_SUCCESS;
}

static void VKAPI_CALL stub_properties2(VkPhysicalDevice physical,
                                        VkPhysicalDeviceMemoryProperties2* props) {
    VkPhysicalDeviceMemoryBudgetPropertiesEXT* budget =
        (VkPhysicalDeviceMemoryBudgetPropertiesEXT*)props->pNext;
    (void)physical;
    budget->heapBudget[0] = heap0_budget;
    budget->heapBudget[1] = 0; /* none given: 80% of the size */
    budget->heapUsage[0] = usage_by_others;
}

/* Types: 0 device, 1 host coherent, 2 host cached (not coherent), 3 device+host coherent */
static void make_info(vk_heap_info_t* info, int budget) {
    VkPhysicalDeviceMemoryProperties* m = &info->memory;
    memset(info, 0, sizeof(*info));
    info->device = (VkDevice)(uintptr_t)1;
    m->memoryHeapCount = 2;
    m->memoryHeaps[0].size = 1024 * MiB;
    m->memoryHeaps[0].flags = VK_MEMORY_HEAP_DEVICE_LOCAL_BIT;
    m->memoryHeaps[1].size = 2048 * MiB;
    m->memoryTypeCount = 4;
    m->memoryTypes[0].propertyFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    m->memoryTypes[0].heapIndex = 0;
    m->memoryTypes[1].propertyFlags =
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    m->memoryTypes[1].heapIndex = 1;
    m->memoryTypes[2].propertyFlags =
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_CACHED_BIT;
    m->memoryTypes[2].heapIndex = 1;
    m->memoryTypes[3].propertyFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT |
                                      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                      VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    m->memoryTypes[3].heapIndex = 0;
    info->buffer_image_granularity = 1024;
    info->non_coherent_atom_size = 256;
    info->device_address = 1;
    info->fns.allocate_memory = stub_allocate;
    info->fns.free_memory = stub_free;
    info->fns.map_memory = stub_map;
    info->fns.unmap_memory = stub_unmap;
    info->fns.flush_ranges = stub_flush;
    info->fns.invalidate_ranges = stub_flush;
    info->fns.get_memory_properties2 = budget ? stub_properties2 : NULL;
}

static VkMemoryRequirements requirements(VkDeviceSize size, VkDeviceSize alignment,
                                         uint32_t bits) {
    VkMemoryRequirements r;
    r.size = size;
    r.alignment = alignment;
    r.memoryTypeBits = bits;
    return r;
}

/* The renderer's kinds of request (vk_memory.cpp) */
static vk_heap_request_t request_of(int kind) {
    vk_heap_request_t r;
    memset(&r, 0, sizeof(r));
    switch (kind) {
    case 0: /* device local: images, device buffers */
        r.preferred = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
        r.not_preferred = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
        break;
    case 1: /* stream: written by the host in order, read by the device */
        r.flags = VK_HEAP_MAPPED;
        r.preferred = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
        r.not_preferred = VK_MEMORY_PROPERTY_HOST_CACHED_BIT;
        break;
    case 2: /* upload */
        r.flags = VK_HEAP_MAPPED;
        r.preferred = VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
        r.not_preferred = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT | VK_MEMORY_PROPERTY_HOST_CACHED_BIT;
        break;
    default: /* readback */
        r.flags = VK_HEAP_MAPPED;
        r.preferred = VK_MEMORY_PROPERTY_HOST_CACHED_BIT;
        r.not_preferred = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
        break;
    }
    r.flags |= VK_HEAP_LINEAR;
    return r;
}

static void test_types(void) {
    vk_heap_info_t info;
    vk_heap_t* heap;
    vk_heap_alloc_t* a;
    vk_heap_request_t r;
    VkMemoryRequirements req = requirements(4096, 256, 0xF);
    static const uint32_t expected[4] = {0, 3, 1, 2};
    int kind;
    make_info(&info, 0);
    heap = vk_heap_create(&info);
    CHECK(heap != NULL, "heap made");
    for (kind = 0; kind < 4; ++kind) {
        r = request_of(kind);
        CHECK(vk_heap_alloc(heap, &req, 0, &r, &a) == VK_SUCCESS, "allocated");
        CHECK(a->type == expected[kind], "each kind of request in its type");
        CHECK((a->mapped != NULL) == (kind != 0), "mapped when asked");
        vk_heap_free(heap, a);
    }
    /* A type the resource cannot use is never chosen */
    r = request_of(0);
    req.memoryTypeBits = 0x2;
    CHECK(vk_heap_alloc(heap, &req, 0, &r, &a) == VK_SUCCESS && a->type == 1,
          "only allowed types");
    vk_heap_free(heap, a);
    req.memoryTypeBits = 0x1;
    r = request_of(1);
    CHECK(vk_heap_alloc(heap, &req, 0, &r, &a) == VK_ERROR_FEATURE_NOT_PRESENT && !a,
          "mapped memory never from a device-only type");
    vk_heap_destroy(heap);
    CHECK(live_memories == 0, "destroy frees everything");
}

static void test_pieces(void) {
    vk_heap_info_t info;
    vk_heap_t* heap;
    vk_heap_alloc_t* a[64];
    vk_heap_request_t r = request_of(0);
    VkMemoryRequirements req;
    VkDeviceSize reserved, used;
    long before;
    int i, j;
    make_info(&info, 0);
    heap = vk_heap_create(&info);
    before = allocations;
    for (i = 0; i < 64; ++i) {
        req = requirements(4096 + (VkDeviceSize)i * 1000, (VkDeviceSize)1 << (i % 12), 0x1);
        CHECK(vk_heap_alloc(heap, &req, 0, &r, &a[i]) == VK_SUCCESS, "piece allocated");
        CHECK(a[i]->offset % req.alignment == 0, "piece aligned");
        CHECK(a[i]->size == req.size, "piece of the size asked");
    }
    CHECK(allocations - before == 1, "64 pieces from one block");
    for (i = 0; i < 64; ++i) {
        for (j = i + 1; j < 64; ++j) {
            CHECK(a[i]->memory != a[j]->memory || a[i]->offset + a[i]->size <= a[j]->offset ||
                      a[j]->offset + a[j]->size <= a[i]->offset,
                  "pieces apart");
        }
    }
    vk_heap_usage(heap, 0, &reserved, &used);
    CHECK(reserved == 8 * MiB, "the first block small: 1/8 of the largest");
    /* Every other one, then the rest: all merged back into one free span */
    for (i = 0; i < 64; i += 2) {
        vk_heap_free(heap, a[i]);
    }
    for (i = 1; i < 64; i += 2) {
        vk_heap_free(heap, a[i]);
    }
    vk_heap_usage(heap, 0, &reserved, &used);
    CHECK(used == 0 && reserved == 8 * MiB, "an empty block kept for reuse");
    CHECK(live_memories == 1, "kept block alive");
    req = requirements(4 * MiB - 4096, 4096, 0x1);
    CHECK(vk_heap_alloc(heap, &req, 0, &r, &a[0]) == VK_SUCCESS && a[0]->offset == 0,
          "the merged block whole again");
    CHECK(allocations - before == 1, "no new block for it");
    vk_heap_free(heap, a[0]);
    vk_heap_destroy(heap);
    CHECK(live_memories == 0, "destroy frees everything");
}

static void test_growth_and_own(void) {
    vk_heap_info_t info;
    vk_heap_t* heap;
    vk_heap_alloc_t* a[8];
    vk_heap_alloc_t* big;
    vk_heap_request_t r = request_of(0);
    VkMemoryRequirements req = requirements(3 * MiB, 4096, 0x1);
    VkDeviceSize reserved;
    stub_memory* memory;
    int i;
    make_info(&info, 0);
    heap = vk_heap_create(&info);
    /* 8 MiB, then 16, then 32 as they fill */
    for (i = 0; i < 8; ++i) {
        CHECK(vk_heap_alloc(heap, &req, 0, &r, &a[i]) == VK_SUCCESS, "allocated");
    }
    vk_heap_usage(heap, 0, &reserved, NULL);
    CHECK(reserved == 8 * MiB + 16 * MiB + 32 * MiB, "blocks doubling");
    /* Over half the largest block: memory of its own, freed with it */
    req = requirements(40 * MiB, 4096, 0x1);
    CHECK(vk_heap_alloc(heap, &req, 0, &r, &big) == VK_SUCCESS && big->offset == 0,
          "large allocated");
    memory = (stub_memory*)(uintptr_t)big->memory;
    CHECK(memory->size == 40 * MiB, "large in memory of its own");
    vk_heap_free(heap, big);
    vk_heap_usage(heap, 0, &reserved, NULL);
    CHECK(reserved == 56 * MiB, "own memory freed with it");
    /* Dedicated, for a buffer: VkMemoryDedicatedAllocateInfo and a device address */
    r.flags |= VK_HEAP_DEDICATED;
    r.buffer = (VkBuffer)(uintptr_t)0x1234;
    req = requirements(65536, 256, 0x1);
    CHECK(vk_heap_alloc(heap, &req, 0, &r, &big) == VK_SUCCESS, "dedicated allocated");
    memory = (stub_memory*)(uintptr_t)big->memory;
    CHECK(memory->dedicated && memory->device_address && memory->size == 65536,
          "dedicated: its own, for its buffer, with a device address");
    vk_heap_free(heap, big);
    /* An image's dedicated memory needs no device address */
    r.flags = VK_HEAP_DEDICATED;
    r.buffer = VK_NULL_HANDLE;
    r.image = (VkImage)(uintptr_t)0x5678;
    CHECK(vk_heap_alloc(heap, &req, 1, &r, &big) == VK_SUCCESS, "image dedicated allocated");
    memory = (stub_memory*)(uintptr_t)big->memory;
    CHECK(memory->dedicated && !memory->device_address, "image dedicated, no device address");
    vk_heap_free(heap, big);
    for (i = 0; i < 8; ++i) {
        vk_heap_free(heap, a[i]);
    }
    vk_heap_usage(heap, 0, &reserved, NULL);
    CHECK(reserved == 32 * MiB, "one empty block kept, the largest");
    vk_heap_destroy(heap);
    CHECK(live_memories == 0, "destroy frees everything");
}

static void test_noncoherent(void) {
    vk_heap_info_t info;
    vk_heap_t* heap;
    vk_heap_alloc_t *a, *b;
    vk_heap_request_t r = request_of(3);
    VkMemoryRequirements req = requirements(100, 4, 0xF);
    make_info(&info, 0);
    heap = vk_heap_create(&info);
    CHECK(vk_heap_alloc(heap, &req, 0, &r, &a) == VK_SUCCESS && a->type == 2, "readback");
    CHECK(vk_heap_alloc(heap, &req, 0, &r, &b) == VK_SUCCESS, "readback");
    CHECK(a->size == 256 && b->size == 256 && a->offset % 256 == 0 && b->offset % 256 == 0,
          "noncoherent pieces whole atoms");
    CHECK((unsigned char*)b->mapped - (unsigned char*)a->mapped ==
              (ptrdiff_t)(b->offset - a->offset),
          "one mapping for the block");
    memset(a->mapped, 0xAB, 100);
    memset(b->mapped, 0xCD, 100);
    CHECK(((unsigned char*)a->mapped)[99] == 0xAB, "pieces written apart");
    flushes = 0;
    CHECK(vk_heap_flush(heap, b, 10, 20) == VK_SUCCESS && flushes == 1, "flushed");
    CHECK(last_range.memory == b->memory && last_range.offset == b->offset &&
              last_range.size == 256,
          "flush widened to the atom, inside the piece");
    CHECK(vk_heap_invalidate(heap, a, 0, VK_WHOLE_SIZE) == VK_SUCCESS &&
              last_range.offset == a->offset && last_range.size == 256,
          "invalidate of the whole piece");
    vk_heap_free(heap, a);
    vk_heap_free(heap, b);
    r = request_of(2);
    CHECK(vk_heap_alloc(heap, &req, 0, &r, &a) == VK_SUCCESS && a->type == 1, "upload");
    flushes = 0;
    vk_heap_flush(heap, a, 0, VK_WHOLE_SIZE);
    CHECK(flushes == 0 && a->size == 100, "coherent memory: no flush, no widening");
    vk_heap_free(heap, a);
    vk_heap_destroy(heap);
}

static void test_budget_and_failure(void) {
    vk_heap_info_t info;
    vk_heap_t* heap;
    vk_heap_alloc_t *a, *b;
    vk_heap_request_t r = request_of(0);
    VkMemoryRequirements req = requirements(1 * MiB, 4096, 0x1);
    VkDeviceSize reserved;
    long before;
    make_info(&info, 1);
    heap0_budget = 100 * MiB;
    usage_by_others = 95 * MiB;
    heap = vk_heap_create(&info);
    r.flags |= VK_HEAP_WITHIN_BUDGET;
    CHECK(vk_heap_alloc(heap, &req, 0, &r, &a) == VK_SUCCESS, "small block within budget");
    vk_heap_usage(heap, 0, &reserved, NULL);
    CHECK(reserved == 4 * MiB, "the block made smaller to stay within budget");
    req = requirements(20 * MiB, 4096, 0x1);
    CHECK(vk_heap_alloc(heap, &req, 0, &r, &b) == VK_ERROR_OUT_OF_DEVICE_MEMORY && !b,
          "over budget refused");
    r.flags &= ~VK_HEAP_WITHIN_BUDGET;
    CHECK(vk_heap_alloc(heap, &req, 0, &r, &b) == VK_SUCCESS, "over budget when allowed");
    vk_heap_free(heap, b);
    vk_heap_free(heap, a);
    /* The driver out of memory: the empty block given back, then it succeeds */
    before = allocations;
    fail_after = 0;
    req = requirements(1 * MiB, 4096, 0x1);
    CHECK(vk_heap_alloc(heap, &req, 0, &r, &a) == VK_SUCCESS, "from the kept block");
    vk_heap_free(heap, a);
    req = requirements(40 * MiB, 4096, 0x1);
    CHECK(vk_heap_alloc(heap, &req, 0, &r, &a) == VK_ERROR_OUT_OF_DEVICE_MEMORY && !a,
          "driver out of memory: refused, nothing changed");
    vk_heap_usage(heap, 0, &reserved, NULL);
    CHECK(reserved == 0, "the empty block given back to the driver on the way");
    fail_after = -1;
    CHECK(allocations == before, "no allocation went through");

    /* An image allowed in any type, the device's budget used up: refused, not put in system
     * memory for good; asked again over the budget, in device memory; system memory only
     * where the driver itself has no device memory left. */
    usage_by_others = 100 * MiB;
    heap0_budget = 100 * MiB;
    vk_heap_destroy(heap);
    heap = vk_heap_create(&info);
    r = request_of(0);
    r.flags |= VK_HEAP_WITHIN_BUDGET;
    req = requirements(1 * MiB, 4096, 0xF);
    CHECK(vk_heap_alloc(heap, &req, 0, &r, &a) == VK_ERROR_OUT_OF_DEVICE_MEMORY && !a,
          "over the device's budget: refused, no spill to system memory");
    r.flags &= ~VK_HEAP_WITHIN_BUDGET;
    CHECK(vk_heap_alloc(heap, &req, 0, &r, &a) == VK_SUCCESS && a->type == 0,
          "over budget when allowed: still device memory");
    vk_heap_free(heap, a);
    vk_heap_destroy(heap);
    heap = vk_heap_create(&info);
    device_full = 1;
    CHECK(vk_heap_alloc(heap, &req, 0, &r, &a) == VK_SUCCESS &&
              !(info.memory.memoryTypes[a->type].propertyFlags &
                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT),
          "the driver out of device memory: system memory as the last resort");
    device_full = 0;
    vk_heap_free(heap, a);
    usage_by_others = 0;
    vk_heap_destroy(heap);
    CHECK(live_memories == 0, "destroy frees everything");
}

/* ---- random ---- */

#define LIVE 512

static unsigned long rng = 12345;
static unsigned long next_random(void) {
    rng = rng * 1103515245ul + 12345ul;
    return (rng >> 8) & 0xFFFFFFul;
}

static void test_random(void) {
    vk_heap_info_t info;
    vk_heap_t* heap;
    vk_heap_alloc_t* live[LIVE];
    unsigned char tag[LIVE];
    vk_heap_request_t r;
    VkMemoryRequirements req;
    VkDeviceSize reserved, used, expect_used = 0;
    long op, ok = 0;
    int i, j, slot, kind;
    make_info(&info, 0);
    info.max_spans = 4096;
    heap = vk_heap_create(&info);
    memset(live, 0, sizeof(live));
    for (op = 0; op < 200000; ++op) {
        slot = (int)(next_random() % LIVE);
        if (live[slot]) {
            CHECK(!live[slot]->mapped || ((unsigned char*)live[slot]->mapped)[0] == tag[slot],
                  "contents kept");
            expect_used -= live[slot]->size;
            vk_heap_free(heap, live[slot]);
            live[slot] = NULL;
            continue;
        }
        kind = (int)(next_random() % 4);
        r = request_of(kind);
        if (next_random() % 2) {
            r.flags &= ~VK_HEAP_LINEAR;
        }
        req = requirements(1 + next_random() % (next_random() % 8 ? 65536 : 6 * MiB),
                           (VkDeviceSize)1 << (next_random() % 17), 0xF);
        if (vk_heap_alloc(heap, &req, 0, &r, &live[slot]) != VK_SUCCESS) {
            CHECK(0, "random allocation");
            continue;
        }
        ++ok;
        expect_used += live[slot]->size;
        CHECK(live[slot]->offset % req.alignment == 0, "aligned");
        if (live[slot]->mapped) {
            tag[slot] = (unsigned char)op;
            memset(live[slot]->mapped, tag[slot], (size_t)live[slot]->size);
        }
        for (j = 0; j < LIVE; ++j) {
            if (j != slot && live[j] && live[j]->memory == live[slot]->memory &&
                live[j]->offset < live[slot]->offset + live[slot]->size &&
                live[slot]->offset < live[j]->offset + live[j]->size) {
                CHECK(0, "random pieces apart");
            }
        }
    }
    used = 0;
    for (i = 0; i < 2; ++i) {
        VkDeviceSize u;
        vk_heap_usage(heap, (uint32_t)i, NULL, &u);
        used += u;
    }
    CHECK(used == expect_used, "usage counted");
    for (i = 0; i < LIVE; ++i) {
        vk_heap_free(heap, live[i]);
    }
    for (i = 0; i < 2; ++i) {
        vk_heap_usage(heap, (uint32_t)i, &reserved, &used);
        CHECK(used == 0, "all given back");
    }
    printf("random: %ld allocations, %ld driver allocations\n", ok, allocations);
    vk_heap_destroy(heap);
    CHECK(live_memories == 0, "destroy frees everything");
}

int main(void) {
    test_types();
    test_pieces();
    test_growth_and_own();
    test_noncoherent();
    test_budget_and_failure();
    test_random();
    if (failures) {
        printf("%d failures\n", failures);
        return 1;
    }
    printf("vk_heap: all passed\n");
    return 0;
}
