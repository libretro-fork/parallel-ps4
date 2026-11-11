/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

/* The lost device's report (vk_device_fault.c) against a driver made up for
 * it: what the driver says reaches the log, and a driver that says nothing,
 * or has not the extension, is said so. */

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "common/c_bridge.h"
#include "video_core/renderer_vulkan/vk_device_fault.h"

static int failures;
static char lines[8][512];
static int num_lines;
static int keep_from, skipped;

#define CHECK(cond) \
   do { \
      if (!(cond)) { \
         printf("FAILED %s:%d: %s\n", __FILE__, __LINE__, #cond); \
         failures++; \
      } \
   } while (0)

void c_log(enum c_log_class log_class, enum c_log_level level, const char *func, const char *fmt,
      ...) {
   va_list args;
   (void)func;
   CHECK(log_class == C_LOG_RENDER_VULKAN && level == C_LOG_LEVEL_CRITICAL);
   if (skipped < keep_from) {
      skipped++;
      return;
   }
   if (num_lines == 8)
      return;
   va_start(args, fmt);
   vsprintf(lines[num_lines++], fmt, args);
   va_end(args);
}

static VkResult fail_with;
static const void *reached;

static VKAPI_ATTR void VKAPI_CALL fake_checkpoints(VkQueue queue, uint32_t *count,
      VkCheckpointDataNV *data) {
   (void)queue;
   CHECK(*count >= 2);
   data[0].stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
   data[0].pCheckpointMarker = (void *)reached;
   data[1].stage = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
   data[1].pCheckpointMarker = (void *)((const char *)reached - 48);
   *count = 2;
}

static VKAPI_ATTR VkResult VKAPI_CALL fake_get(VkDevice device, VkDeviceFaultCountsEXT *counts,
      VkDeviceFaultInfoEXT *info) {
   (void)device;
   if (fail_with != VK_SUCCESS)
      return fail_with;
   if (!info) {
      counts->addressInfoCount = 2;
      counts->vendorInfoCount = 1;
      counts->vendorBinarySize = 4096;
      return VK_SUCCESS;
   }
   CHECK(counts->addressInfoCount == 2 && counts->vendorInfoCount == 1);
   CHECK(counts->vendorBinarySize == 0 && info->pVendorBinaryData == NULL);
   /* No terminator: the report must not run past it */
   memset(info->description, 'x', sizeof(info->description));
   memcpy(info->description, "page fault", 10);
   info->description[10] = '\0';
   info->pAddressInfos[0].addressType = VK_DEVICE_FAULT_ADDRESS_TYPE_READ_INVALID_EXT;
   info->pAddressInfos[0].reportedAddress = ((VkDeviceAddress)0x12 << 32) | 0x3456789au;
   info->pAddressInfos[1].addressType = VK_DEVICE_FAULT_ADDRESS_TYPE_WRITE_INVALID_EXT;
   info->pAddressInfos[1].reportedAddress = 0x1000;
   memset(info->pVendorInfos[0].description, 'y', sizeof(info->pVendorInfos[0].description));
   info->pVendorInfos[0].vendorFaultCode = 0x2a;
   info->pVendorInfos[0].vendorFaultData = 7;
   return VK_SUCCESS;
}

int main(void) {
   vk_device_fault_report((VkDevice)0, NULL, (VkQueue)0, NULL);
   CHECK(num_lines == 1 && strstr(lines[0], "no VK_EXT_device_fault"));

   num_lines = 0;
   fail_with = VK_ERROR_DEVICE_LOST;
   vk_device_fault_report((VkDevice)0, fake_get, (VkQueue)0, NULL);
   CHECK(num_lines == 1 && strstr(lines[0], "no account"));

   num_lines = 0;
   fail_with = VK_SUCCESS;
   vk_device_fault_report((VkDevice)0, fake_get, (VkQueue)0, NULL);
   CHECK(num_lines == 4);
   CHECK(strstr(lines[0], "'page fault'") && strstr(lines[0], "2 addresses"));
   CHECK(strstr(lines[1], "read of an invalid address: 0x123456789a"));
   CHECK(strstr(lines[2], "write to an invalid address: 0x000001000"));
   CHECK(strstr(lines[3], "code 0x2a, data 0x7") && strlen(lines[3]) < 400);
   /* What was recorded last is said, and what the driver says the GPU reached */
   {
      const void *first = vk_device_fault_mark("first", 1, 2, 3, 4);
      int i;
      for (i = 0; i < 300; i++)
         reached = vk_device_fault_mark("many", (uint64_t)i, 0, 0, ((uint64_t)7 << 32) | 9u);
      (void)first;
      num_lines = 0;
      keep_from = 1;
      skipped = 0;
      fail_with = VK_ERROR_DEVICE_LOST;
      vk_device_fault_report((VkDevice)0, fake_get, (VkQueue)0, fake_checkpoints);
      /* After the line on the fault: two checkpoints, then what was recorded from there on */
      CHECK(strstr(lines[0], "the GPU had begun #301 many 0x00000012b"));
      CHECK(strstr(lines[0], "0x700000009"));
      CHECK(strstr(lines[1], "the GPU had finished #300 many 0x00000012a"));
      /* from 24 before the earlier of the two the GPU reached on */
      CHECK(strstr(lines[2], "then #276 many 0x000000112"));
      CHECK(strstr(lines[3], "then #277 many 0x000000113"));
      keep_from = 0;
   }
   if (failures) {
      printf("vk_device_fault_test: %d failed\n", failures);
      return 1;
   }
   printf("vk_device_fault_test: passed\n");
   return 0;
}
