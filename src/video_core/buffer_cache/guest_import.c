/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <stdlib.h>
#include <string.h>

#include <retro_atomic.h>

#include "guest_import.h"

#define CHUNK_SIZE ((uint64_t)1 << GUEST_IMPORT_CHUNK_SHIFT)

/* A chunk's state. The thread that moves it from UNTRIED to IMPORTING imports it; its buffer
 * and memory are written before the state is released as IMPORTED. */
#define CHUNK_UNTRIED 0
#define CHUNK_IMPORTING 1
#define CHUNK_IMPORTED 2
#define CHUNK_FAILED 3

typedef struct guest_chunk {
    VkBuffer buffer;
    VkDeviceMemory memory;
    retro_atomic_int_t state;
} guest_chunk_t;

struct guest_import {
    guest_import_info_t info;
    guest_chunk_t* chunks;
    uint32_t count;
    retro_atomic_int_t imported;
    retro_atomic_int_t failed;
};

guest_import_t* guest_import_create(const guest_import_info_t* info) {
    guest_import_t* import;
    uint32_t i;
    const guest_import_fns_t* f = &info->fns;
    if (!info->device || !info->base || !info->size || !info->alignment ||
        (info->alignment & (info->alignment - 1)) != 0 || CHUNK_SIZE % info->alignment != 0 ||
        ((uintptr_t)info->base % info->alignment) != 0 || info->size % info->alignment != 0 ||
        !f->allocate_memory || !f->free_memory || !f->create_buffer || !f->destroy_buffer ||
        !f->get_buffer_memory_requirements || !f->bind_buffer_memory ||
        !f->get_memory_host_pointer_properties)
        return NULL;
    import = (guest_import_t*)calloc(1, sizeof(*import));
    if (!import)
        return NULL;
    import->info = *info;
    import->count = (uint32_t)((info->size + CHUNK_SIZE - 1) >> GUEST_IMPORT_CHUNK_SHIFT);
    import->chunks = (guest_chunk_t*)calloc(import->count, sizeof(*import->chunks));
    if (!import->chunks) {
        free(import);
        return NULL;
    }
    for (i = 0; i < import->count; i++)
        retro_atomic_int_init(&import->chunks[i].state, CHUNK_UNTRIED);
    retro_atomic_int_init(&import->imported, 0);
    retro_atomic_int_init(&import->failed, 0);
    return import;
}

void guest_import_destroy(guest_import_t* import) {
    uint32_t i;
    if (!import)
        return;
    for (i = 0; i < import->count; i++) {
        guest_chunk_t* c = &import->chunks[i];
        if (retro_atomic_load_acquire_int(&c->state) != CHUNK_IMPORTED)
            continue;
        import->info.fns.destroy_buffer(import->info.device, c->buffer, NULL);
        import->info.fns.free_memory(import->info.device, c->memory, NULL);
    }
    free(import->chunks);
    free(import);
}

/* The memory type of @bits to import into: host coherent where there is one, so that what the
 * game writes needs no flush */
static int pick_type(const VkPhysicalDeviceMemoryProperties* memory, uint32_t bits) {
    uint32_t i;
    int any = -1;
    for (i = 0; i < memory->memoryTypeCount; i++) {
        if (!(bits & (1u << i)))
            continue;
        if (memory->memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)
            return (int)i;
        if (any < 0)
            any = (int)i;
    }
    return any;
}

static int import_chunk(guest_import_t* import, uint32_t index) {
    const guest_import_info_t* info = &import->info;
    const guest_import_fns_t* f = &info->fns;
    guest_chunk_t* c = &import->chunks[index];
    uint64_t start = (uint64_t)index << GUEST_IMPORT_CHUNK_SHIFT;
    uint64_t size = info->size - start < CHUNK_SIZE ? info->size - start : CHUNK_SIZE;
    void* host = info->base + start;
    VkMemoryHostPointerPropertiesEXT host_props;
    VkExternalMemoryBufferCreateInfo external;
    VkBufferCreateInfo buffer_info;
    VkMemoryRequirements requirements;
    VkImportMemoryHostPointerInfoEXT import_info;
    VkMemoryAllocateInfo alloc;
    int type;

    memset(&host_props, 0, sizeof(host_props));
    host_props.sType = VK_STRUCTURE_TYPE_MEMORY_HOST_POINTER_PROPERTIES_EXT;
    if (f->get_memory_host_pointer_properties(
            info->device, VK_EXTERNAL_MEMORY_HANDLE_TYPE_HOST_ALLOCATION_BIT_EXT, host,
            &host_props) != VK_SUCCESS)
        return 0;

    memset(&external, 0, sizeof(external));
    external.sType = VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_BUFFER_CREATE_INFO;
    external.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_HOST_ALLOCATION_BIT_EXT;
    memset(&buffer_info, 0, sizeof(buffer_info));
    buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buffer_info.pNext = &external;
    buffer_info.size = size;
    buffer_info.usage = info->usage;
    buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    if (f->create_buffer(info->device, &buffer_info, NULL, &c->buffer) != VK_SUCCESS)
        return 0;
    f->get_buffer_memory_requirements(info->device, c->buffer, &requirements);
    type = pick_type(&info->memory, requirements.memoryTypeBits & host_props.memoryTypeBits);
    if (type < 0 || requirements.size > size) {
        f->destroy_buffer(info->device, c->buffer, NULL);
        c->buffer = VK_NULL_HANDLE;
        return 0;
    }

    memset(&import_info, 0, sizeof(import_info));
    import_info.sType = VK_STRUCTURE_TYPE_IMPORT_MEMORY_HOST_POINTER_INFO_EXT;
    import_info.handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_HOST_ALLOCATION_BIT_EXT;
    import_info.pHostPointer = host;
    memset(&alloc, 0, sizeof(alloc));
    alloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    alloc.pNext = &import_info;
    alloc.allocationSize = size;
    alloc.memoryTypeIndex = (uint32_t)type;
    if (f->allocate_memory(info->device, &alloc, NULL, &c->memory) != VK_SUCCESS) {
        f->destroy_buffer(info->device, c->buffer, NULL);
        c->buffer = VK_NULL_HANDLE;
        return 0;
    }
    if (f->bind_buffer_memory(info->device, c->buffer, c->memory, 0) != VK_SUCCESS) {
        f->destroy_buffer(info->device, c->buffer, NULL);
        f->free_memory(info->device, c->memory, NULL);
        c->buffer = VK_NULL_HANDLE;
        c->memory = VK_NULL_HANDLE;
        return 0;
    }
    return 1;
}

/* Imports chunk @index if no thread has tried yet; its state after */
static int claim_chunk(guest_import_t* import, uint32_t index) {
    guest_chunk_t* c = &import->chunks[index];
    int state = retro_atomic_load_acquire_int(&c->state);
    if (state != CHUNK_UNTRIED)
        return state;
    if (!retro_atomic_cas_int(&c->state, CHUNK_UNTRIED, CHUNK_IMPORTING))
        return CHUNK_IMPORTING; /* another thread's to import, or done */
    if (import_chunk(import, index)) {
        state = CHUNK_IMPORTED;
        retro_atomic_fetch_add_int(&import->imported, 1);
    } else {
        state = CHUNK_FAILED;
        retro_atomic_fetch_add_int(&import->failed, 1);
    }
    retro_atomic_store_release_int(&c->state, state);
    return state;
}

void guest_import_prepare(guest_import_t* import, uint64_t offset, uint64_t size) {
    uint64_t index, last;
    if (!import || size == 0 || offset >= import->info.size)
        return;
    if (size > import->info.size - offset)
        size = import->info.size - offset;
    last = (offset + size - 1) >> GUEST_IMPORT_CHUNK_SHIFT;
    for (index = offset >> GUEST_IMPORT_CHUNK_SHIFT; index <= last; index++)
        (void)claim_chunk(import, (uint32_t)index);
}

VkBuffer guest_import_buffer(guest_import_t* import, uint64_t offset, uint64_t size,
                             VkDeviceSize* buffer_offset) {
    uint32_t index;
    int state;
    if (!import || size == 0 || offset >= import->info.size || size > import->info.size - offset)
        return VK_NULL_HANDLE;
    index = (uint32_t)(offset >> GUEST_IMPORT_CHUNK_SHIFT);
    if (((offset + size - 1) >> GUEST_IMPORT_CHUNK_SHIFT) != index)
        return VK_NULL_HANDLE; /* across two chunks */
    state = retro_atomic_load_acquire_int(&import->chunks[index].state);
    if (state == CHUNK_UNTRIED && import->info.import_on_use)
        state = claim_chunk(import, index);
    if (state != CHUNK_IMPORTED)
        return VK_NULL_HANDLE;
    *buffer_offset = offset & (CHUNK_SIZE - 1);
    return import->chunks[index].buffer;
}

VkBuffer guest_import_target(guest_import_t* import, guest_import_run_fn run, void* run_ctx,
                             uint64_t addr, uint64_t size, uint64_t alignment, uint32_t* chunk,
                             VkDeviceSize* buffer_offset) {
    uint64_t offset = 0;
    VkBuffer buffer;
    if (!import || size == 0 || run(run_ctx, addr, size, &offset) != size || offset == UINT64_MAX)
        return VK_NULL_HANDLE;
    buffer = guest_import_buffer(import, offset, size, buffer_offset);
    if (buffer == VK_NULL_HANDLE || (alignment && *buffer_offset % alignment != 0))
        return VK_NULL_HANDLE;
    *chunk = (uint32_t)(offset >> GUEST_IMPORT_CHUNK_SHIFT);
    return buffer;
}

void guest_import_split(guest_import_t* import, guest_import_run_fn run, void* run_ctx,
                        uint64_t addr, uint64_t size, guest_import_piece_fn piece,
                        void* piece_ctx) {
    uint64_t delta = 0;
    while (delta < size) {
        uint64_t offset = 0;
        uint64_t length = run(run_ctx, addr + delta, size - delta, &offset);
        if (length == 0 || length > size - delta)
            return; /* a run function that breaks its word */
        if (offset != UINT64_MAX) {
            const uint64_t chunk_left = CHUNK_SIZE - (offset & (CHUNK_SIZE - 1));
            VkDeviceSize buffer_offset = 0;
            VkBuffer buffer;
            if (length > chunk_left)
                length = chunk_left;
            buffer = guest_import_buffer(import, offset, length, &buffer_offset);
            piece(piece_ctx, buffer, (uint32_t)(offset >> GUEST_IMPORT_CHUNK_SHIFT), buffer_offset,
                  addr + delta, delta, length);
        }
        delta += length;
    }
}

uint32_t guest_import_chunks(guest_import_t* import, uint32_t* failed) {
    if (failed)
        *failed = import ? (uint32_t)retro_atomic_load_acquire_int(&import->failed) : 0;
    return import ? (uint32_t)retro_atomic_load_acquire_int(&import->imported) : 0;
}
