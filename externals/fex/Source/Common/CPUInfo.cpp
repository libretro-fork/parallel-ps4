// SPDX-License-Identifier: MIT
#include <FEXHeaderUtils/Filesystem.h>

#include <cstdio>

#include <cstddef>
#include <cstdint>
#ifdef _WIN32
#include <thread>
#else
#include <linux/limits.h>
#endif

namespace FEX::CPUInfo {
#ifndef _WIN32
uint32_t CalculateNumberOfCPUs() {
  char Tmp[64];
  size_t CPUs = 1;

  for (;; ++CPUs) {
    snprintf(Tmp, sizeof(Tmp), "/sys/devices/system/cpu/cpu%zu", CPUs);
    if (!FHU::Filesystem::Exists(Tmp)) {
      break;
    }
  }

  return CPUs;
}
#else
uint32_t CalculateNumberOfCPUs() {
  // May not return correct number of cores if some are parked.
  return std::thread::hardware_concurrency();
}
#endif
} // namespace FEX::CPUInfo
