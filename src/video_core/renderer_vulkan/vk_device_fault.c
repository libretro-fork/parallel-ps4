/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <stdlib.h>
#include <string.h>

#include "common/c_bridge.h"
#include "video_core/renderer_vulkan/vk_device_fault.h"

#define SAY(level, fmt, a, b, c)                                                                   \
    c_log(C_LOG_RENDER_VULKAN, level, "vk_device_fault_report", fmt, a, b, c)

static const char* address_type(VkDeviceFaultAddressTypeEXT type) {
    switch (type) {
    case VK_DEVICE_FAULT_ADDRESS_TYPE_READ_INVALID_EXT:
        return "read of an invalid address";
    case VK_DEVICE_FAULT_ADDRESS_TYPE_WRITE_INVALID_EXT:
        return "write to an invalid address";
    case VK_DEVICE_FAULT_ADDRESS_TYPE_EXECUTE_INVALID_EXT:
        return "execution of an invalid address";
    case VK_DEVICE_FAULT_ADDRESS_TYPE_INSTRUCTION_POINTER_UNKNOWN_EXT:
        return "instruction pointer, cause unknown";
    case VK_DEVICE_FAULT_ADDRESS_TYPE_INSTRUCTION_POINTER_INVALID_EXT:
        return "invalid instruction";
    case VK_DEVICE_FAULT_ADDRESS_TYPE_INSTRUCTION_POINTER_FAULT_EXT:
        return "instruction that faulted";
    default:
        return "no address";
    }
}

void vk_device_fault_report(VkDevice device, PFN_vkGetDeviceFaultInfoEXT get) {
    VkDeviceFaultCountsEXT counts;
    VkDeviceFaultInfoEXT info;
    VkResult result;
    uint32_t i;
    if (!get) {
        SAY(C_LOG_LEVEL_CRITICAL,
            "The GPU device was lost; this driver cannot say why (no VK_EXT_device_fault)%s%s%s",
            "", "", "");
        return;
    }
    memset(&counts, 0, sizeof(counts));
    counts.sType = VK_STRUCTURE_TYPE_DEVICE_FAULT_COUNTS_EXT;
    result = get(device, &counts, NULL);
    if (result != VK_SUCCESS && result != VK_INCOMPLETE) {
        SAY(C_LOG_LEVEL_CRITICAL,
            "The GPU device was lost; the driver gave no account of it (%d)%s%s", (int)result, "",
            "");
        return;
    }
    /* The vendor's dump is of no use in a log */
    counts.vendorBinarySize = 0;
    memset(&info, 0, sizeof(info));
    info.sType = VK_STRUCTURE_TYPE_DEVICE_FAULT_INFO_EXT;
    if (counts.addressInfoCount)
        info.pAddressInfos = (VkDeviceFaultAddressInfoEXT*)calloc(
            counts.addressInfoCount, sizeof(VkDeviceFaultAddressInfoEXT));
    if (counts.vendorInfoCount)
        info.pVendorInfos = (VkDeviceFaultVendorInfoEXT*)calloc(counts.vendorInfoCount,
                                                                sizeof(VkDeviceFaultVendorInfoEXT));
    if ((counts.addressInfoCount && !info.pAddressInfos) ||
        (counts.vendorInfoCount && !info.pVendorInfos)) {
        counts.addressInfoCount = 0;
        counts.vendorInfoCount = 0;
    }
    result = get(device, &counts, &info);
    if (result != VK_SUCCESS && result != VK_INCOMPLETE) {
        SAY(C_LOG_LEVEL_CRITICAL,
            "The GPU device was lost; the driver gave no account of it (%d)%s%s", (int)result, "",
            "");
    } else {
        info.description[VK_MAX_DESCRIPTION_SIZE - 1] = '\0';
        SAY(C_LOG_LEVEL_CRITICAL,
            "The GPU device was lost. The driver says: '%s' (%u addresses, %u vendor entries)",
            info.description, (unsigned)counts.addressInfoCount, (unsigned)counts.vendorInfoCount);
        for (i = 0; i < counts.addressInfoCount; i++) {
            const VkDeviceFaultAddressInfoEXT* a = &info.pAddressInfos[i];
            /* The address is known to within the precision given */
            SAY(C_LOG_LEVEL_CRITICAL, "  %s: 0x%lx%08lx", address_type(a->addressType),
                (unsigned long)(a->reportedAddress >> 32),
                (unsigned long)(a->reportedAddress & 0xffffffffu));
        }
        for (i = 0; i < counts.vendorInfoCount; i++) {
            VkDeviceFaultVendorInfoEXT* v = &info.pVendorInfos[i];
            v->description[VK_MAX_DESCRIPTION_SIZE - 1] = '\0';
            SAY(C_LOG_LEVEL_CRITICAL, "  vendor: '%s', code 0x%lx, data 0x%lx", v->description,
                (unsigned long)v->vendorFaultCode, (unsigned long)v->vendorFaultData);
        }
    }
    free(info.pAddressInfos);
    free(info.pVendorInfos);
}
