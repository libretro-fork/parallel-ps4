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

#define MARKS 4096u
#define MARKS_AFTER 160u /* said after the last the GPU reached */
#define MARKS_LAST 16u
#define MARKS_BEFORE 24u
#define CHECKPOINTS 16u

typedef struct fault_mark {
    const char* what;
    uint64_t v[4];
    unsigned long seq;
} fault_mark;

static fault_mark marks[MARKS];
static unsigned long mark_seq;

const void* vk_device_fault_mark(const char* what, uint64_t a, uint64_t b, uint64_t c, uint64_t d) {
    fault_mark* m = &marks[mark_seq % MARKS];
    m->what = what;
    m->v[0] = a;
    m->v[1] = b;
    m->v[2] = c;
    m->v[3] = d;
    m->seq = ++mark_seq;
    return m;
}

static void say_mark(const char* lead, const fault_mark* m) {
    c_log(C_LOG_RENDER_VULKAN, C_LOG_LEVEL_CRITICAL, "vk_device_fault_report",
          "  %s#%lu %s 0x%lx%08lx 0x%lx%08lx 0x%lx%08lx 0x%lx%08lx", lead, m->seq, m->what,
          (unsigned long)(m->v[0] >> 32), (unsigned long)(m->v[0] & 0xffffffffu),
          (unsigned long)(m->v[1] >> 32), (unsigned long)(m->v[1] & 0xffffffffu),
          (unsigned long)(m->v[2] >> 32), (unsigned long)(m->v[2] & 0xffffffffu),
          (unsigned long)(m->v[3] >> 32), (unsigned long)(m->v[3] & 0xffffffffu));
}

/* What the GPU had got to, and what was recorded for it last */
static void say_marks(VkQueue queue, PFN_vkGetQueueCheckpointDataNV checkpoints) {
    unsigned long last = mark_seq, from, to, n, reached_low = 0, reached_high = 0;
    if (checkpoints) {
        VkCheckpointDataNV data[CHECKPOINTS];
        uint32_t count = CHECKPOINTS, i;
        memset(data, 0, sizeof(data));
        for (i = 0; i < CHECKPOINTS; i++)
            data[i].sType = VK_STRUCTURE_TYPE_CHECKPOINT_DATA_NV;
        checkpoints(queue, &count, data);
        for (i = 0; i < count && i < CHECKPOINTS; i++) {
            const fault_mark* m = (const fault_mark*)data[i].pCheckpointMarker;
            const char* lead =
                data[i].stage == VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT      ? "the GPU had begun "
                : data[i].stage == VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT ? "the GPU had finished "
                                                                        : "the GPU was at ";
            if (m >= marks && m < marks + MARKS) {
                say_mark(lead, m);
                if (!reached_low || m->seq < reached_low)
                    reached_low = m->seq;
                if (m->seq > reached_high)
                    reached_high = m->seq;
            }
        }
        if (!count)
            c_log(C_LOG_RENDER_VULKAN, C_LOG_LEVEL_CRITICAL, "vk_device_fault_report",
                  "  the driver names no checkpoint the GPU reached");
    }
    /* What was recorded from the last the GPU reached on: the one it stopped in is the first
     * of these, or what came between it and the next */
    if (reached_low && last - reached_low < MARKS) {
        /* with what led up to it: the targets and the pass it is in */
        from = reached_low > MARKS_BEFORE ? reached_low - 1 - MARKS_BEFORE : 0;
        to = reached_high + MARKS_AFTER < last ? reached_high + MARKS_AFTER : last;
        for (n = from; n < to; n++)
            say_mark("then ", &marks[n % MARKS]);
    } else {
        to = 0;
    }
    from = last > MARKS_LAST ? last - MARKS_LAST : 0;
    if (from < to)
        from = to;
    for (n = from; n < last; n++)
        say_mark("recorded ", &marks[n % MARKS]);
}

static void say_fault(VkDevice device, PFN_vkGetDeviceFaultInfoEXT get);

void vk_device_fault_report(VkDevice device, PFN_vkGetDeviceFaultInfoEXT get, VkQueue queue,
                            PFN_vkGetQueueCheckpointDataNV checkpoints) {
    say_fault(device, get);
    say_marks(queue, checkpoints);
}

static void say_fault(VkDevice device, PFN_vkGetDeviceFaultInfoEXT get) {
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
