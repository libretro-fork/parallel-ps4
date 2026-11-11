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
   if (num_lines == 8)
      return;
   va_start(args, fmt);
   vsprintf(lines[num_lines++], fmt, args);
   va_end(args);
}

static VkResult fail_with;

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
   vk_device_fault_report((VkDevice)0, NULL);
   CHECK(num_lines == 1 && strstr(lines[0], "no VK_EXT_device_fault"));

   num_lines = 0;
   fail_with = VK_ERROR_DEVICE_LOST;
   vk_device_fault_report((VkDevice)0, fake_get);
   CHECK(num_lines == 1 && strstr(lines[0], "no account"));

   num_lines = 0;
   fail_with = VK_SUCCESS;
   vk_device_fault_report((VkDevice)0, fake_get);
   CHECK(num_lines == 4);
   CHECK(strstr(lines[0], "'page fault'") && strstr(lines[0], "2 addresses"));
   CHECK(strstr(lines[1], "read of an invalid address: 0x123456789a"));
   CHECK(strstr(lines[2], "write to an invalid address: 0x000001000"));
   CHECK(strstr(lines[3], "code 0x2a, data 0x7") && strlen(lines[3]) < 400);
   if (failures) {
      printf("vk_device_fault_test: %d failed\n", failures);
      return 1;
   }
   printf("vk_device_fault_test: passed\n");
   return 0;
}
