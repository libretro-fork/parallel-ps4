// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright © 2023 Skyline Team and Contributors (https://github.com/skyline-emu/)
// Copyright © 2015-2023 The Khronos Group Inc.
// Copyright © 2015-2023 Valve Corporation
// Copyright © 2015-2023 LunarG, Inc.

#include "common/assert.h"
#include "common/enum.h"
#include "video_core/texture_cache/host_compatibility.h"

namespace VideoCore {

/**
 * @brief All classes of format compatibility according to the Vulkan specification
 * @url
 * https://github.com/KhronosGroup/Vulkan-ValidationLayers/blob/d37c676f/layers/generated/vk_format_utils.h#L47-L131
 */
enum class CompatibilityClass {
    NONE = 0,
    _128BIT = 1 << 0,
    _16BIT = 1 << 1,
    _192BIT = 1 << 2,
    _24BIT = 1 << 3,
    _256BIT = 1 << 4,
    _32BIT = 1 << 5,
    _48BIT = 1 << 6,
    _64BIT = 1 << 7,
    _8BIT = 1 << 8,
    _96BIT = 1 << 9,
    BC1_RGB = 1 << 10,
    BC1_RGBA = 1 << 11,
    BC2 = 1 << 12,
    BC3 = 1 << 13,
    BC4 = 1 << 14,
    BC5 = 1 << 15,
    BC6H = 1 << 16,
    BC7 = 1 << 17,
    D16 = 1 << 18,
    D16S8 = 1 << 19,
    D24 = 1 << 20,
    D24S8 = 1 << 21,
    D32 = 1 << 22,
    D32S8 = 1 << 23,
    S8 = 1 << 24,
};
DECLARE_ENUM_FLAG_OPERATORS(CompatibilityClass)

/**
 * @brief The format compatibility class according to the Vulkan specification
 * @url
 * https://registry.khronos.org/vulkan/specs/1.3-extensions/html/vkspec.html#formats-compatibility-classes
 * @url
 * https://github.com/KhronosGroup/Vulkan-ValidationLayers/blob/d37c676f/layers/generated/vk_format_utils.cpp#L70-L812
 */
/// @format's compatibility class; the formats the table had, as before
static CompatibilityClass ClassOf(VkFormat format) {
    switch (format) {
    case VK_FORMAT_A1R5G5B5_UNORM_PACK16:
    case VK_FORMAT_A4B4G4R4_UNORM_PACK16:
    case VK_FORMAT_A4R4G4B4_UNORM_PACK16:
    case VK_FORMAT_B4G4R4A4_UNORM_PACK16:
    case VK_FORMAT_B5G5R5A1_UNORM_PACK16:
    case VK_FORMAT_B5G6R5_UNORM_PACK16:
    case VK_FORMAT_R10X6_UNORM_PACK16:
    case VK_FORMAT_R12X4_UNORM_PACK16:
    case VK_FORMAT_R16_SFLOAT:
    case VK_FORMAT_R16_SINT:
    case VK_FORMAT_R16_SNORM:
    case VK_FORMAT_R16_SSCALED:
    case VK_FORMAT_R16_UINT:
    case VK_FORMAT_R16_UNORM:
    case VK_FORMAT_R16_USCALED:
    case VK_FORMAT_R4G4B4A4_UNORM_PACK16:
    case VK_FORMAT_R5G5B5A1_UNORM_PACK16:
    case VK_FORMAT_R5G6B5_UNORM_PACK16:
    case VK_FORMAT_R8G8_SINT:
    case VK_FORMAT_R8G8_SNORM:
    case VK_FORMAT_R8G8_SRGB:
    case VK_FORMAT_R8G8_SSCALED:
    case VK_FORMAT_R8G8_UINT:
    case VK_FORMAT_R8G8_UNORM:
    case VK_FORMAT_R8G8_USCALED:
        return CompatibilityClass::_16BIT;
    case VK_FORMAT_A2B10G10R10_SINT_PACK32:
    case VK_FORMAT_A2B10G10R10_SNORM_PACK32:
    case VK_FORMAT_A2B10G10R10_SSCALED_PACK32:
    case VK_FORMAT_A2B10G10R10_UINT_PACK32:
    case VK_FORMAT_A2B10G10R10_UNORM_PACK32:
    case VK_FORMAT_A2B10G10R10_USCALED_PACK32:
    case VK_FORMAT_A2R10G10B10_SINT_PACK32:
    case VK_FORMAT_A2R10G10B10_SNORM_PACK32:
    case VK_FORMAT_A2R10G10B10_SSCALED_PACK32:
    case VK_FORMAT_A2R10G10B10_UINT_PACK32:
    case VK_FORMAT_A2R10G10B10_UNORM_PACK32:
    case VK_FORMAT_A2R10G10B10_USCALED_PACK32:
    case VK_FORMAT_A8B8G8R8_SINT_PACK32:
    case VK_FORMAT_A8B8G8R8_SNORM_PACK32:
    case VK_FORMAT_A8B8G8R8_SRGB_PACK32:
    case VK_FORMAT_A8B8G8R8_SSCALED_PACK32:
    case VK_FORMAT_A8B8G8R8_UINT_PACK32:
    case VK_FORMAT_A8B8G8R8_UNORM_PACK32:
    case VK_FORMAT_A8B8G8R8_USCALED_PACK32:
    case VK_FORMAT_B10G11R11_UFLOAT_PACK32:
    case VK_FORMAT_B8G8R8A8_SINT:
    case VK_FORMAT_B8G8R8A8_SNORM:
    case VK_FORMAT_B8G8R8A8_SRGB:
    case VK_FORMAT_B8G8R8A8_SSCALED:
    case VK_FORMAT_B8G8R8A8_UINT:
    case VK_FORMAT_B8G8R8A8_UNORM:
    case VK_FORMAT_B8G8R8A8_USCALED:
    case VK_FORMAT_E5B9G9R9_UFLOAT_PACK32:
    case VK_FORMAT_R10X6G10X6_UNORM_2PACK16:
    case VK_FORMAT_R12X4G12X4_UNORM_2PACK16:
    case VK_FORMAT_R16G16_SFLOAT:
    case VK_FORMAT_R16G16_SINT:
    case VK_FORMAT_R16G16_SNORM:
    case VK_FORMAT_R16G16_SSCALED:
    case VK_FORMAT_R16G16_UINT:
    case VK_FORMAT_R16G16_UNORM:
    case VK_FORMAT_R16G16_USCALED:
    case VK_FORMAT_R32_SFLOAT:
    case VK_FORMAT_R32_SINT:
    case VK_FORMAT_R32_UINT:
    case VK_FORMAT_R8G8B8A8_SINT:
    case VK_FORMAT_R8G8B8A8_SNORM:
    case VK_FORMAT_R8G8B8A8_SRGB:
    case VK_FORMAT_R8G8B8A8_SSCALED:
    case VK_FORMAT_R8G8B8A8_UINT:
    case VK_FORMAT_R8G8B8A8_UNORM:
    case VK_FORMAT_R8G8B8A8_USCALED:
        return CompatibilityClass::_32BIT;
    case VK_FORMAT_B8G8R8_SINT:
    case VK_FORMAT_B8G8R8_SNORM:
    case VK_FORMAT_B8G8R8_SRGB:
    case VK_FORMAT_B8G8R8_SSCALED:
    case VK_FORMAT_B8G8R8_UINT:
    case VK_FORMAT_B8G8R8_UNORM:
    case VK_FORMAT_B8G8R8_USCALED:
    case VK_FORMAT_R8G8B8_SINT:
    case VK_FORMAT_R8G8B8_SNORM:
    case VK_FORMAT_R8G8B8_SRGB:
    case VK_FORMAT_R8G8B8_SSCALED:
    case VK_FORMAT_R8G8B8_UINT:
    case VK_FORMAT_R8G8B8_UNORM:
    case VK_FORMAT_R8G8B8_USCALED:
        return CompatibilityClass::_24BIT;
    case VK_FORMAT_BC1_RGBA_SRGB_BLOCK:
    case VK_FORMAT_BC1_RGBA_UNORM_BLOCK:
        return CompatibilityClass::BC1_RGBA | CompatibilityClass::_64BIT;
    case VK_FORMAT_BC1_RGB_SRGB_BLOCK:
    case VK_FORMAT_BC1_RGB_UNORM_BLOCK:
        return CompatibilityClass::BC1_RGB | CompatibilityClass::_64BIT;
    case VK_FORMAT_BC2_SRGB_BLOCK:
    case VK_FORMAT_BC2_UNORM_BLOCK:
        return CompatibilityClass::BC2 | CompatibilityClass::_128BIT;
    case VK_FORMAT_BC3_SRGB_BLOCK:
    case VK_FORMAT_BC3_UNORM_BLOCK:
        return CompatibilityClass::BC3 | CompatibilityClass::_128BIT;
    case VK_FORMAT_BC4_SNORM_BLOCK:
    case VK_FORMAT_BC4_UNORM_BLOCK:
        return CompatibilityClass::BC4 | CompatibilityClass::_64BIT;
    case VK_FORMAT_BC5_SNORM_BLOCK:
    case VK_FORMAT_BC5_UNORM_BLOCK:
        return CompatibilityClass::BC5 | CompatibilityClass::_128BIT;
    case VK_FORMAT_BC6H_SFLOAT_BLOCK:
    case VK_FORMAT_BC6H_UFLOAT_BLOCK:
        return CompatibilityClass::BC6H | CompatibilityClass::_128BIT;
    case VK_FORMAT_BC7_SRGB_BLOCK:
    case VK_FORMAT_BC7_UNORM_BLOCK:
        return CompatibilityClass::BC7 | CompatibilityClass::_128BIT;
    case VK_FORMAT_D16_UNORM:
        return CompatibilityClass::D16;
    case VK_FORMAT_D16_UNORM_S8_UINT:
        return CompatibilityClass::D16S8;
    case VK_FORMAT_D24_UNORM_S8_UINT:
        return CompatibilityClass::D24S8;
    case VK_FORMAT_D32_SFLOAT:
        return CompatibilityClass::D32;
    case VK_FORMAT_D32_SFLOAT_S8_UINT:
        return CompatibilityClass::D32S8;
    case VK_FORMAT_R16G16B16A16_SFLOAT:
    case VK_FORMAT_R16G16B16A16_SINT:
    case VK_FORMAT_R16G16B16A16_SNORM:
    case VK_FORMAT_R16G16B16A16_SSCALED:
    case VK_FORMAT_R16G16B16A16_UINT:
    case VK_FORMAT_R16G16B16A16_UNORM:
    case VK_FORMAT_R16G16B16A16_USCALED:
    case VK_FORMAT_R32G32_SFLOAT:
    case VK_FORMAT_R32G32_SINT:
    case VK_FORMAT_R32G32_UINT:
    case VK_FORMAT_R64_SFLOAT:
    case VK_FORMAT_R64_SINT:
    case VK_FORMAT_R64_UINT:
        return CompatibilityClass::_64BIT;
    case VK_FORMAT_R16G16B16_SFLOAT:
    case VK_FORMAT_R16G16B16_SINT:
    case VK_FORMAT_R16G16B16_SNORM:
    case VK_FORMAT_R16G16B16_SSCALED:
    case VK_FORMAT_R16G16B16_UINT:
    case VK_FORMAT_R16G16B16_UNORM:
    case VK_FORMAT_R16G16B16_USCALED:
        return CompatibilityClass::_48BIT;
    case VK_FORMAT_R32G32B32A32_SFLOAT:
    case VK_FORMAT_R32G32B32A32_SINT:
    case VK_FORMAT_R32G32B32A32_UINT:
    case VK_FORMAT_R64G64_SFLOAT:
    case VK_FORMAT_R64G64_SINT:
    case VK_FORMAT_R64G64_UINT:
        return CompatibilityClass::_128BIT;
    case VK_FORMAT_R32G32B32_SFLOAT:
    case VK_FORMAT_R32G32B32_SINT:
    case VK_FORMAT_R32G32B32_UINT:
        return CompatibilityClass::_96BIT;
    case VK_FORMAT_R4G4_UNORM_PACK8:
    case VK_FORMAT_R8_SINT:
    case VK_FORMAT_R8_SNORM:
    case VK_FORMAT_R8_SRGB:
    case VK_FORMAT_R8_SSCALED:
    case VK_FORMAT_R8_UINT:
    case VK_FORMAT_R8_UNORM:
    case VK_FORMAT_R8_USCALED:
        return CompatibilityClass::_8BIT;
    case VK_FORMAT_R64G64B64A64_SFLOAT:
    case VK_FORMAT_R64G64B64A64_SINT:
    case VK_FORMAT_R64G64B64A64_UINT:
        return CompatibilityClass::_256BIT;
    case VK_FORMAT_R64G64B64_SFLOAT:
    case VK_FORMAT_R64G64B64_SINT:
    case VK_FORMAT_R64G64B64_UINT:
        return CompatibilityClass::_192BIT;
    case VK_FORMAT_S8_UINT:
        return CompatibilityClass::S8;
    case VK_FORMAT_X8_D24_UNORM_PACK32:
        return CompatibilityClass::D24;
    case VK_FORMAT_UNDEFINED:
        return CompatibilityClass::NONE;
    default:
        break;
    }
    // The table had no other format: its lookup ended the run there
    UNREACHABLE_MSG("No compatibility class for format %d", static_cast<int>(format));
    return CompatibilityClass::NONE;
}

bool IsVulkanFormatCompatible(VkFormat base, VkFormat view) {
    if (base == view) {
        return true;
    }
    const CompatibilityClass base_comp = ClassOf(base);
    const CompatibilityClass view_comp = ClassOf(view);
    return (base_comp & view_comp) == view_comp;
}

} // namespace VideoCore
