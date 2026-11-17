/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

/* The game's memory lent to the GPU as Vulkan buffers
 * (src/video_core/buffer_cache/ guest_import.c) on a real device: memory mapped
 * as the address space maps its backing (a shared mapping of a memory file,
 * three chunks and a half of it), imported a chunk at a time as it is wanted.
 * The GPU copies from it the memory's bytes, and into it, as a readback does,
 * at the places an address space of scattered pages maps to: a range split at a
 * chunk's end, at a jump in the backing and around pages without backing, those
 * left as they were. A range across two chunks or past the end is refused, a
 * chunk is imported once, and everything is freed. Run with the validation
 * layer present and it is enabled: any message from it is a failure. Skipped
 * (exit 0) with no device or no VK_EXT_external_memory_host.
 *
 *   VK_ICD_FILENAMES=/usr/share/vulkan/icd.d/lvp_icd.json
 * build-tests/guest_import_device_test */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <dlfcn.h>
#include <sys/mman.h>
#include <unistd.h>

#include "../../src/video_core/buffer_cache/guest_import.h"

static int failures = 0;

#define CHECK(cond, what)                                                      \
  do {                                                                         \
    if (!(cond)) {                                                             \
      printf("FAIL %s (line %d)\n", what, __LINE__);                           \
      ++failures;                                                              \
    }                                                                          \
  } while (0)

static PFN_vkGetInstanceProcAddr get_instance_proc;
static PFN_vkGetDeviceProcAddr get_device_proc;
#define IFN(name) PFN_##name p_##name
static IFN(vkCreateInstance);
static IFN(vkEnumeratePhysicalDevices);
static IFN(vkEnumerateDeviceExtensionProperties);
static IFN(vkGetPhysicalDeviceProperties);
static IFN(vkGetPhysicalDeviceProperties2);
static IFN(vkGetPhysicalDeviceMemoryProperties);
static IFN(vkCreateDevice);
static IFN(vkDestroyInstance);
static IFN(vkCreateDebugUtilsMessengerEXT);
static IFN(vkDestroyDebugUtilsMessengerEXT);
static IFN(vkDestroyDevice);
static IFN(vkGetDeviceQueue);
static IFN(vkAllocateMemory);
static IFN(vkFreeMemory);
static IFN(vkMapMemory);
static IFN(vkCreateBuffer);
static IFN(vkDestroyBuffer);
static IFN(vkGetBufferMemoryRequirements);
static IFN(vkBindBufferMemory);
static IFN(vkGetMemoryHostPointerPropertiesEXT);
static IFN(vkCreateCommandPool);
static IFN(vkDestroyCommandPool);
static IFN(vkAllocateCommandBuffers);
static IFN(vkBeginCommandBuffer);
static IFN(vkEndCommandBuffer);
static IFN(vkCmdCopyBuffer);
static IFN(vkQueueSubmit);
static IFN(vkQueueWaitIdle);

static long validation_messages = 0;

static VkBool32 VKAPI_CALL
on_message(VkDebugUtilsMessageSeverityFlagBitsEXT severity,
           VkDebugUtilsMessageTypeFlagsEXT types,
           const VkDebugUtilsMessengerCallbackDataEXT *data, void *user) {
  (void)types;
  (void)user;
  if (severity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
    printf("validation: %s\n", data->pMessage);
    ++validation_messages;
  }
  return VK_FALSE;
}

#define CHUNK ((uint64_t)1 << GUEST_IMPORT_CHUNK_SHIFT)
#define MEMORY_SIZE (CHUNK * 3 + CHUNK / 2)
#define COPY_SIZE (1u << 20)

static unsigned char expected_byte(uint64_t at) {
  return (unsigned char)((at * 2654435761u) >> 13);
}

/* A game's address space of eight pages of 16 KiB at VIRT, as the backing page
 * table maps it: pages 0 to 2 one run of the backing over the end of chunk 0
 * into chunk 1, page 3 without backing, pages 4 and 5 in chunk 2 apart, pages 6
 * and 7 without backing */
#define PAGE ((uint64_t)16384)
#define VIRT ((uint64_t)1 << 32)
#define NO_BACKING UINT64_MAX
static const uint64_t page_backing[8] = {
    CHUNK - 2 * PAGE,     CHUNK - PAGE,         CHUNK,      NO_BACKING,
    2 * CHUNK + 5 * PAGE, 2 * CHUNK + 9 * PAGE, NO_BACKING, NO_BACKING,
};

static uint64_t page_of(uint64_t addr, uint64_t *in_page) {
  *in_page = (addr - VIRT) % PAGE;
  return (addr - VIRT) / PAGE;
}

/* As MemoryManager::BackingRun */
static uint64_t run_of(void *ctx, uint64_t addr, uint64_t size,
                       uint64_t *backing_offset) {
  uint64_t run = 0, expected = 0, in_page;
  int backed = 0;
  (void)ctx;
  while (size) {
    const uint64_t page = page_of(addr, &in_page);
    const uint64_t step = PAGE - in_page < size ? PAGE - in_page : size;
    const uint64_t at = page < 8 ? page_backing[page] : NO_BACKING;
    const int page_backed = at != NO_BACKING;
    if (run == 0) {
      backed = page_backed;
      *backing_offset = page_backed ? at + in_page : NO_BACKING;
    } else if (page_backed != backed ||
               (page_backed && at + in_page != expected)) {
      break;
    }
    expected = at + in_page + step;
    run += step;
    size -= step;
    addr += step;
  }
  return run;
}

#define MAX_PIECES 8
typedef struct pieces {
  unsigned n;
  VkBuffer buffer[MAX_PIECES];
  uint32_t chunk[MAX_PIECES];
  VkDeviceSize offset[MAX_PIECES];
  uint64_t addr[MAX_PIECES];
  uint64_t delta[MAX_PIECES];
  uint64_t size[MAX_PIECES];
} pieces_t;

static void on_piece(void *ctx, VkBuffer buffer, uint32_t chunk,
                     VkDeviceSize buffer_offset, uint64_t addr, uint64_t delta,
                     uint64_t size) {
  pieces_t *p = (pieces_t *)ctx;
  if (p->n == MAX_PIECES)
    return;
  p->buffer[p->n] = buffer;
  p->chunk[p->n] = chunk;
  p->offset[p->n] = buffer_offset;
  p->addr[p->n] = addr;
  p->delta[p->n] = delta;
  p->size[p->n] = size;
  p->n++;
}

int main(void) {
  void *lib = dlopen("libvulkan.so.1", RTLD_NOW);
  VkInstanceCreateInfo instance_info;
  VkApplicationInfo app;
  VkDebugUtilsMessengerCreateInfoEXT messenger_info;
  VkDebugUtilsMessengerEXT messenger = VK_NULL_HANDLE;
  VkInstance instance;
  VkPhysicalDevice physical;
  VkPhysicalDeviceProperties props;
  VkPhysicalDeviceExternalMemoryHostPropertiesEXT host_props;
  VkPhysicalDeviceProperties2 props2;
  VkDeviceQueueCreateInfo queue_info;
  VkDeviceCreateInfo device_info;
  VkDevice device;
  VkQueue queue;
  VkExtensionProperties exts[512];
  uint32_t ext_count = 512, i;
  int has_ext = 0, with_layer = 1;
  const char *layer = "VK_LAYER_KHRONOS_validation";
  const char *debug_ext = VK_EXT_DEBUG_UTILS_EXTENSION_NAME;
  const char *host_ext = VK_EXT_EXTERNAL_MEMORY_HOST_EXTENSION_NAME;
  float priority = 1.0f;
  uint32_t count = 1;
  guest_import_info_t info;
  guest_import_t *import;
  unsigned char *memory;
  int fd;
  VkBuffer dst;
  VkDeviceMemory dst_memory;
  VkBufferCreateInfo buffer_info;
  VkMemoryRequirements req;
  VkMemoryAllocateInfo alloc;
  VkPhysicalDeviceMemoryProperties mem_props;
  unsigned char *dst_mapped;
  VkCommandPoolCreateInfo pool_info;
  VkCommandPool pool;
  VkCommandBufferAllocateInfo cb_info;
  VkCommandBuffer cb;
  VkCommandBufferBeginInfo begin;
  VkSubmitInfo submit;
  uint32_t imported, failed;
  /* Ranges copied: one in each chunk, the last one's end, a chunk's very start
   */
  static const uint64_t starts[] = {4096, CHUNK + 12288, CHUNK * 2 + 777 * 4096,
                                    MEMORY_SIZE - COPY_SIZE, CHUNK * 3};
  const uint32_t n_starts = sizeof(starts) / sizeof(starts[0]);
  VkBuffer src[sizeof(starts) / sizeof(starts[0])];
  VkDeviceSize src_offset[sizeof(starts) / sizeof(starts[0])];
  VkBuffer again;
  VkDeviceSize again_offset;

  if (lib) {
    *(void **)&get_instance_proc = dlsym(lib, "vkGetInstanceProcAddr");
  }
  if (!lib || !get_instance_proc) {
    printf("guest_import_device: no Vulkan loader, skipped\n");
    return 0;
  }
  p_vkCreateInstance =
      (PFN_vkCreateInstance)get_instance_proc(NULL, "vkCreateInstance");
  memset(&app, 0, sizeof(app));
  app.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
  app.apiVersion = VK_API_VERSION_1_3;
  memset(&messenger_info, 0, sizeof(messenger_info));
  messenger_info.sType =
      VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
  messenger_info.messageSeverity =
      VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
      VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
  messenger_info.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                               VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT;
  messenger_info.pfnUserCallback = on_message;
  memset(&instance_info, 0, sizeof(instance_info));
  instance_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
  instance_info.pApplicationInfo = &app;
  instance_info.enabledLayerCount = 1;
  instance_info.ppEnabledLayerNames = &layer;
  instance_info.enabledExtensionCount = 1;
  instance_info.ppEnabledExtensionNames = &debug_ext;
  if (p_vkCreateInstance(&instance_info, NULL, &instance) != VK_SUCCESS) {
    with_layer = 0;
    instance_info.enabledLayerCount = 0;
    instance_info.enabledExtensionCount = 0;
    if (p_vkCreateInstance(&instance_info, NULL, &instance) != VK_SUCCESS) {
      printf("guest_import_device: no Vulkan instance, skipped\n");
      return 0;
    }
  }
#define LOADI(name) p_##name = (PFN_##name)get_instance_proc(instance, #name)
  LOADI(vkEnumeratePhysicalDevices);
  LOADI(vkEnumerateDeviceExtensionProperties);
  LOADI(vkGetPhysicalDeviceProperties);
  LOADI(vkGetPhysicalDeviceProperties2);
  LOADI(vkGetPhysicalDeviceMemoryProperties);
  LOADI(vkCreateDevice);
  LOADI(vkDestroyInstance);
  LOADI(vkCreateDebugUtilsMessengerEXT);
  LOADI(vkDestroyDebugUtilsMessengerEXT);
  get_device_proc = (PFN_vkGetDeviceProcAddr)get_instance_proc(
      instance, "vkGetDeviceProcAddr");
  if (with_layer) {
    p_vkCreateDebugUtilsMessengerEXT(instance, &messenger_info, NULL,
                                     &messenger);
  }
  if (p_vkEnumeratePhysicalDevices(instance, &count, &physical) < 0 || !count) {
    printf("guest_import_device: no device, skipped\n");
    return 0;
  }
  p_vkGetPhysicalDeviceProperties(physical, &props);
  p_vkEnumerateDeviceExtensionProperties(physical, NULL, &ext_count, exts);
  for (i = 0; i < ext_count; i++)
    if (!strcmp(exts[i].extensionName, host_ext))
      has_ext = 1;
  if (!has_ext) {
    printf("guest_import_device: %s has no %s, skipped\n", props.deviceName,
           host_ext);
    return 0;
  }
  memset(&host_props, 0, sizeof(host_props));
  host_props.sType =
      VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_MEMORY_HOST_PROPERTIES_EXT;
  memset(&props2, 0, sizeof(props2));
  props2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
  props2.pNext = &host_props;
  p_vkGetPhysicalDeviceProperties2(physical, &props2);
  printf("guest_import_device: %s, import alignment %lu%s\n", props.deviceName,
         (unsigned long)host_props.minImportedHostPointerAlignment,
         with_layer ? ", validation on" : "");

  memset(&queue_info, 0, sizeof(queue_info));
  queue_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
  queue_info.queueFamilyIndex = 0;
  queue_info.queueCount = 1;
  queue_info.pQueuePriorities = &priority;
  memset(&device_info, 0, sizeof(device_info));
  device_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
  device_info.queueCreateInfoCount = 1;
  device_info.pQueueCreateInfos = &queue_info;
  device_info.enabledExtensionCount = 1;
  device_info.ppEnabledExtensionNames = &host_ext;
  if (p_vkCreateDevice(physical, &device_info, NULL, &device) != VK_SUCCESS) {
    printf("FAIL device made\n");
    return 1;
  }
#define LOADD(name) p_##name = (PFN_##name)get_device_proc(device, #name)
  LOADD(vkDestroyDevice);
  LOADD(vkGetDeviceQueue);
  LOADD(vkAllocateMemory);
  LOADD(vkFreeMemory);
  LOADD(vkMapMemory);
  LOADD(vkCreateBuffer);
  LOADD(vkDestroyBuffer);
  LOADD(vkGetBufferMemoryRequirements);
  LOADD(vkBindBufferMemory);
  LOADD(vkGetMemoryHostPointerPropertiesEXT);
  LOADD(vkCreateCommandPool);
  LOADD(vkDestroyCommandPool);
  LOADD(vkAllocateCommandBuffers);
  LOADD(vkBeginCommandBuffer);
  LOADD(vkEndCommandBuffer);
  LOADD(vkCmdCopyBuffer);
  LOADD(vkQueueSubmit);
  LOADD(vkQueueWaitIdle);
  p_vkGetDeviceQueue(device, 0, 0, &queue);
  p_vkGetPhysicalDeviceMemoryProperties(physical, &mem_props);

  /* The game's memory as the address space maps it: a shared mapping of a
   * memory file */
  fd = memfd_create("guest_import_test", 0);
  CHECK(fd >= 0 && ftruncate(fd, (off_t)MEMORY_SIZE) == 0, "memory file made");
  memory = (unsigned char *)mmap(NULL, MEMORY_SIZE, PROT_READ | PROT_WRITE,
                                 MAP_SHARED, fd, 0);
  CHECK(memory != MAP_FAILED, "memory mapped");
  if (memory == MAP_FAILED)
    return 1;
  for (i = 0; i < n_starts; i++) {
    uint64_t k;
    for (k = 0; k < COPY_SIZE; k++)
      memory[starts[i] + k] = expected_byte(starts[i] + k);
  }

  memset(&info, 0, sizeof(info));
  info.device = device;
  info.memory = mem_props;
  info.alignment = host_props.minImportedHostPointerAlignment;
  info.base = memory;
  info.size = MEMORY_SIZE;
  info.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
               VK_BUFFER_USAGE_TRANSFER_DST_BIT |
               VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
  info.fns.allocate_memory = p_vkAllocateMemory;
  info.fns.free_memory = p_vkFreeMemory;
  info.fns.create_buffer = p_vkCreateBuffer;
  info.fns.destroy_buffer = p_vkDestroyBuffer;
  info.fns.get_buffer_memory_requirements = p_vkGetBufferMemoryRequirements;
  info.fns.bind_buffer_memory = p_vkBindBufferMemory;
  info.fns.get_memory_host_pointer_properties =
      p_vkGetMemoryHostPointerPropertiesEXT;
  import = guest_import_create(&info);
  CHECK(import != NULL, "import made");
  if (!import)
    return 1;
  CHECK(guest_import_chunks(import, NULL) == 0,
        "nothing imported before it is wanted");

  for (i = 0; i < n_starts; i++) {
    src[i] = guest_import_buffer(import, starts[i], COPY_SIZE, &src_offset[i]);
    CHECK(src[i] != VK_NULL_HANDLE, "a range within a chunk: its buffer");
    CHECK(src_offset[i] == (starts[i] & (CHUNK - 1)),
          "its offset in the chunk");
  }
  imported = guest_import_chunks(import, &failed);
  CHECK(imported == 4 && failed == 0,
        "four chunks imported, as they were wanted");
  again = guest_import_buffer(import, starts[0] + 64, 64, &again_offset);
  CHECK(again == src[0] && again_offset == starts[0] + 64,
        "the same chunk's buffer again");
  CHECK(guest_import_chunks(import, NULL) == 4, "and not imported again");
  CHECK(guest_import_buffer(import, CHUNK - 64, 128, &again_offset) ==
            VK_NULL_HANDLE,
        "a range across two chunks: none");
  CHECK(guest_import_buffer(import, MEMORY_SIZE - 64, 128, &again_offset) ==
            VK_NULL_HANDLE,
        "a range past the end: none");
  CHECK(guest_import_buffer(import, MEMORY_SIZE, 1, &again_offset) ==
            VK_NULL_HANDLE,
        "a range beyond the end: none");

  /* Where the copies go */
  memset(&buffer_info, 0, sizeof(buffer_info));
  buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  buffer_info.size = (VkDeviceSize)COPY_SIZE * n_starts;
  buffer_info.usage =
      VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
  p_vkCreateBuffer(device, &buffer_info, NULL, &dst);
  p_vkGetBufferMemoryRequirements(device, dst, &req);
  memset(&alloc, 0, sizeof(alloc));
  alloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
  alloc.allocationSize = req.size;
  for (i = 0; i < mem_props.memoryTypeCount; i++)
    if ((req.memoryTypeBits & (1u << i)) &&
        (mem_props.memoryTypes[i].propertyFlags &
         (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
          VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)) ==
            (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
             VK_MEMORY_PROPERTY_HOST_COHERENT_BIT))
      break;
  alloc.memoryTypeIndex = i;
  CHECK(p_vkAllocateMemory(device, &alloc, NULL, &dst_memory) == VK_SUCCESS,
        "destination memory");
  p_vkBindBufferMemory(device, dst, dst_memory, 0);
  p_vkMapMemory(device, dst_memory, 0, VK_WHOLE_SIZE, 0, (void **)&dst_mapped);
  memset(dst_mapped, 0xcd, (size_t)COPY_SIZE * n_starts);

  memset(&pool_info, 0, sizeof(pool_info));
  pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
  pool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
  p_vkCreateCommandPool(device, &pool_info, NULL, &pool);
  memset(&cb_info, 0, sizeof(cb_info));
  cb_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  cb_info.commandPool = pool;
  cb_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  cb_info.commandBufferCount = 1;
  p_vkAllocateCommandBuffers(device, &cb_info, &cb);
  memset(&begin, 0, sizeof(begin));
  begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  p_vkBeginCommandBuffer(cb, &begin);
  for (i = 0; i < n_starts; i++) {
    VkBufferCopy copy;
    copy.srcOffset = src_offset[i];
    copy.dstOffset = (VkDeviceSize)COPY_SIZE * i;
    copy.size = COPY_SIZE;
    p_vkCmdCopyBuffer(cb, src[i], dst, 1, &copy);
  }
  p_vkEndCommandBuffer(cb);
  /* Written after the copy was recorded, before it ran: the GPU reads the
   * memory as it is */
  memory[starts[1] + 100] = 0x5a;
  memset(&submit, 0, sizeof(submit));
  submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  submit.commandBufferCount = 1;
  submit.pCommandBuffers = &cb;
  CHECK(p_vkQueueSubmit(queue, 1, &submit, VK_NULL_HANDLE) == VK_SUCCESS,
        "submitted");
  p_vkQueueWaitIdle(queue);
  {
    int same = 1;
    for (i = 0; i < n_starts && same; i++) {
      uint64_t k;
      for (k = 0; k < COPY_SIZE; k++) {
        const unsigned char want =
            (i == 1 && k == 100) ? 0x5a : expected_byte(starts[i] + k);
        if (dst_mapped[(size_t)COPY_SIZE * i + k] != want) {
          printf("range %u byte %lu: %02x, not %02x\n", i, (unsigned long)k,
                 dst_mapped[(size_t)COPY_SIZE * i + k], want);
          same = 0;
          break;
        }
      }
    }
    CHECK(same, "the GPU copied the memory's bytes, from every chunk");
  }

  /* Where a range of the scattered address space lies */
  {
    uint32_t chunk = 99;
    VkDeviceSize offset = 0;
    VkBuffer b = guest_import_target(import, run_of, NULL, VIRT + 100, 100, 4,
                                     &chunk, &offset);
    CHECK(b == src[0] && chunk == 0 && offset == CHUNK - 2 * PAGE + 100,
          "a range in one run in one chunk: its buffer, chunk and offset");
    CHECK(guest_import_target(import, run_of, NULL, VIRT + 100, 100, 256,
                              &chunk, &offset) == VK_NULL_HANDLE,
          "at an offset the alignment does not divide: none");
    CHECK(guest_import_target(import, run_of, NULL, VIRT + PAGE, 2 * PAGE, 0,
                              &chunk, &offset) == VK_NULL_HANDLE,
          "one run across two chunks: none");
    CHECK(guest_import_target(import, run_of, NULL, VIRT + 3 * PAGE, 64, 0,
                              &chunk, &offset) == VK_NULL_HANDLE,
          "a page without backing: none");
    CHECK(guest_import_target(import, run_of, NULL, VIRT + 4 * PAGE, 2 * PAGE,
                              0, &chunk, &offset) == VK_NULL_HANDLE,
          "two pages apart in the backing: none");
  }

  /* A readback of [VIRT + 100, VIRT + 6 pages - 50) from dst straight into the
   * memory */
  {
    pieces_t p;
    const uint64_t range_begin = VIRT + 100, size = 6 * PAGE - 150;
    uint64_t k;
    int same = 1;
    memset(&p, 0, sizeof(p));
    guest_import_split(import, run_of, NULL, range_begin, size, on_piece, &p);
    CHECK(p.n == 4,
          "four pieces: to the chunk's end, the next chunk, and two apart");
    CHECK(p.n == 4 && p.chunk[0] == 0 &&
              p.offset[0] == CHUNK - 2 * PAGE + 100 && p.delta[0] == 0 &&
              p.size[0] == 2 * PAGE - 100,
          "the first up to the end of chunk 0");
    CHECK(p.n == 4 && p.chunk[1] == 1 && p.offset[1] == 0 &&
              p.delta[1] == 2 * PAGE - 100 && p.size[1] == PAGE,
          "the second from the start of chunk 1");
    CHECK(p.n == 4 && p.chunk[2] == 2 && p.offset[2] == 5 * PAGE &&
              p.delta[2] == 4 * PAGE - 100 && p.size[2] == PAGE,
          "the page without backing passed over");
    CHECK(p.n == 4 && p.chunk[3] == 2 && p.offset[3] == 9 * PAGE &&
              p.delta[3] == 5 * PAGE - 100 && p.size[3] == PAGE - 50,
          "the last where its page lies apart");
    CHECK(p.n == 4 && p.addr[2] == VIRT + 4 * PAGE, "each with its address");

    for (k = 0; k < size; k++)
      dst_mapped[k] = (unsigned char)(0x80 ^ expected_byte(k));
    memset(memory + CHUNK - 2 * PAGE, 0x11, 3 * PAGE);
    memset(memory + 2 * CHUNK + 4 * PAGE, 0x11, 7 * PAGE);
    p_vkBeginCommandBuffer(cb, &begin);
    for (i = 0; i < p.n; i++) {
      VkBufferCopy copy;
      copy.srcOffset = p.delta[i];
      copy.dstOffset = p.offset[i];
      copy.size = p.size[i];
      if (p.buffer[i] != VK_NULL_HANDLE)
        p_vkCmdCopyBuffer(cb, dst, p.buffer[i], 1, &copy);
    }
    p_vkEndCommandBuffer(cb);
    CHECK(p_vkQueueSubmit(queue, 1, &submit, VK_NULL_HANDLE) == VK_SUCCESS,
          "submitted");
    p_vkQueueWaitIdle(queue);
    for (i = 0; i < p.n && same; i++)
      for (k = 0; k < p.size[i]; k++) {
        const unsigned char want =
            (unsigned char)(0x80 ^ expected_byte(p.delta[i] + k));
        const uint64_t at = (p.chunk[i] * CHUNK) + p.offset[i] + k;
        if (memory[at] != want) {
          printf("piece %u byte %lu: %02x, not %02x\n", i, (unsigned long)k,
                 memory[at], want);
          same = 0;
          break;
        }
      }
    CHECK(same, "the GPU wrote each piece into the memory where its pages lie");
    CHECK(memory[CHUNK - 2 * PAGE + 99] == 0x11 &&
              memory[2 * CHUNK + 6 * PAGE] == 0x11 &&
              memory[2 * CHUNK + 10 * PAGE - 50] == 0x11 &&
              memory[2 * CHUNK + 9 * PAGE - 1] == 0x11,
          "and nothing around them");
  }

  p_vkDestroyCommandPool(device, pool, NULL);
  p_vkDestroyBuffer(device, dst, NULL);
  p_vkFreeMemory(device, dst_memory, NULL);
  guest_import_destroy(import);
  p_vkDestroyDevice(device, NULL);
  if (messenger)
    p_vkDestroyDebugUtilsMessengerEXT(instance, messenger, NULL);
  p_vkDestroyInstance(instance, NULL);
  munmap(memory, MEMORY_SIZE);
  close(fd);
  CHECK(validation_messages == 0, "no validation messages");
  printf("guest_import_device: %s\n", failures ? "FAILED" : "passed");
  return failures != 0;
}
