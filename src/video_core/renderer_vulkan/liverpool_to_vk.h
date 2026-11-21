// SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <span>
#include "common/assert.h"
#include "video_core/amdgpu/pixel_format.h"
#include "video_core/amdgpu/regs_color.h"
#include "video_core/amdgpu/regs_depth.h"
#include "video_core/amdgpu/regs_primitive.h"
#include "video_core/amdgpu/regs_vertex.h"
#include "video_core/amdgpu/resource.h"
#include "video_core/renderer_vulkan/vk_common.h"

namespace Vulkan::LiverpoolToVK {

VkStencilOp StencilOp(AmdGpu::StencilFunc op);

VkCompareOp CompareOp(AmdGpu::CompareFunc func);

bool IsPrimitiveCulled(AmdGpu::PrimitiveType type);

VkPrimitiveTopology PrimitiveType(AmdGpu::PrimitiveType type);

VkPolygonMode PolygonMode(AmdGpu::PolygonMode mode);

VkCullModeFlags CullMode(AmdGpu::CullMode mode);

VkFrontFace FrontFace(AmdGpu::FrontFace mode);

VkBlendFactor BlendFactor(AmdGpu::BlendControl::BlendFactor factor);

bool IsDualSourceBlendFactor(AmdGpu::BlendControl::BlendFactor factor);

VkBlendOp BlendOp(AmdGpu::BlendControl::BlendFunc func);

void NormalizeMinMaxBlend(VkBlendOp& op, VkBlendFactor& src, VkBlendFactor& dst,
                          AmdGpu::NumberFormat format);

VkLogicOp LogicOp(AmdGpu::ColorControl::LogicOp logic_op);

VkSamplerAddressMode ClampMode(AmdGpu::ClampMode mode);

VkCompareOp DepthCompare(AmdGpu::DepthCompare comp);

VkFilter Filter(AmdGpu::Filter filter);

VkSamplerReductionMode FilterMode(AmdGpu::FilterMode mode);

VkSamplerMipmapMode MipFilter(AmdGpu::MipFilter filter);

VkBorderColor BorderColor(AmdGpu::BorderColor color);

VkComponentSwizzle ComponentSwizzle(AmdGpu::CompSwizzle comp_swizzle);

VkComponentMapping ComponentMapping(AmdGpu::CompMapping comp_mapping);

/// A run of table entries, walked with a range for
template <typename T>
struct TableView {
    const T* items;
    u32 count;

    const T* begin() const {
        return items;
    }
    const T* end() const {
        return items + count;
    }
};

struct SurfaceFormatInfo {
    AmdGpu::DataFormat data_format;
    AmdGpu::NumberFormat number_format;
    VkFormat vk_format;
    VkFormatFeatureFlags2 flags;
};
TableView<SurfaceFormatInfo> SurfaceFormats();

struct DepthFormatInfo {
    AmdGpu::DepthBuffer::ZFormat z_format;
    AmdGpu::DepthBuffer::StencilFormat stencil_format;
    VkFormat vk_format;
    VkFormatFeatureFlags2 flags;
};
TableView<DepthFormatInfo> DepthFormats();

VkFormat DepthFormat(AmdGpu::DepthBuffer::ZFormat z_format,
                     AmdGpu::DepthBuffer::StencilFormat stencil_format);

VkClearValue ColorBufferClearValue(const AmdGpu::ColorBuffer& color_buffer);

VkSampleCountFlagBits NumSamples(u32 num_samples, VkSampleCountFlags supported_flags);

// Table 8.13 Data and Image Formats [Sea Islands Series Instruction Set Architecture]
constexpr size_t amd_gpu_data_format_bit_size = 6;   // All values are under 64
constexpr size_t amd_gpu_number_format_bit_size = 4; // All values are under 16
constexpr size_t surface_format_table_size =
    (1u << amd_gpu_data_format_bit_size) * (1u << amd_gpu_number_format_bit_size);

/// The Vulkan format of each data and number format pair (GetSurfaceFormatTableIndex)
struct SurfaceFormatTable {
    VkFormat formats[surface_format_table_size];
};
extern const SurfaceFormatTable surface_format_table;

constexpr size_t GetSurfaceFormatTableIndex(AmdGpu::DataFormat data_format,
                                            AmdGpu::NumberFormat num_format) {
    DEBUG_ASSERT(u32(data_format) < 1 << amd_gpu_data_format_bit_size);
    DEBUG_ASSERT(u32(num_format) < 1 << amd_gpu_number_format_bit_size);
    size_t result = static_cast<size_t>(num_format) |
                    (static_cast<size_t>(data_format) << amd_gpu_number_format_bit_size);
    return result;
}

constexpr VkFormat SurfaceFormat(AmdGpu::DataFormat data_format,
                                 AmdGpu::NumberFormat num_format) noexcept {
    VkFormat result =
        surface_format_table.formats[GetSurfaceFormatTableIndex(data_format, num_format)];
    bool found = result != VK_FORMAT_UNDEFINED || data_format == AmdGpu::DataFormat::FormatInvalid;
    ASSERT_MSG(found, "Unknown data_format=%u and num_format=%u", static_cast<u32>(data_format),
               static_cast<u32>(num_format));
    return result;
}

static inline bool IsFormatDepthCompatible(VkFormat fmt) {
    switch (fmt) {
    // 32-bit float compatible
    case VK_FORMAT_D32_SFLOAT:
    case VK_FORMAT_R32_SFLOAT:
    case VK_FORMAT_R32_UINT:
    // 16-bit unorm compatible
    case VK_FORMAT_D16_UNORM:
    case VK_FORMAT_R16_UNORM:
        return true;
    default:
        return false;
    }
}

static inline bool IsFormatStencilCompatible(VkFormat fmt) {
    switch (fmt) {
    // 8-bit uint compatible
    case VK_FORMAT_S8_UINT:
    case VK_FORMAT_R8_UINT:
    case VK_FORMAT_R8_UNORM:
        return true;
    default:
        return false;
    }
}

static inline VkFormat PromoteFormatToDepth(VkFormat fmt) {
    if (fmt == VK_FORMAT_R32_SFLOAT || fmt == VK_FORMAT_R32_UINT) {
        return VK_FORMAT_D32_SFLOAT;
    } else if (fmt == VK_FORMAT_R16_UNORM) {
        return VK_FORMAT_D16_UNORM;
    }
    UNREACHABLE_MSG("Unexpected depth format %d", static_cast<int>(fmt));
}

} // namespace Vulkan::LiverpoolToVK
