// SPDX-FileCopyrightText: Copyright 2024-2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "common/assert.h"
#include "common/number_utils.h"
#include "video_core/amdgpu/pixel_format.h"
#include "video_core/renderer_vulkan/liverpool_to_vk.h"

#define INVALID_NUMBER_FORMAT_COMBO                                                                \
    LOG_ERROR(Render_Vulkan, "Unsupported number type %.*s for format %.*s",                       \
              static_cast<int>(AmdGpu::NameOf(number_type).size()),                                \
              AmdGpu::NameOf(number_type).data(), static_cast<int>(AmdGpu::NameOf(format).size()), \
              AmdGpu::NameOf(format).data());

namespace Vulkan::LiverpoolToVK {

using DepthBuffer = AmdGpu::DepthBuffer;

VkStencilOp StencilOp(AmdGpu::StencilFunc op) {
    switch (op) {
    case AmdGpu::StencilFunc::Keep:
        return VK_STENCIL_OP_KEEP;
    case AmdGpu::StencilFunc::Zero:
        return VK_STENCIL_OP_ZERO;
    case AmdGpu::StencilFunc::ReplaceTest:
        return VK_STENCIL_OP_REPLACE;
    case AmdGpu::StencilFunc::AddClamp:
        return VK_STENCIL_OP_INCREMENT_AND_CLAMP;
    case AmdGpu::StencilFunc::SubClamp:
        return VK_STENCIL_OP_DECREMENT_AND_CLAMP;
    case AmdGpu::StencilFunc::Invert:
        return VK_STENCIL_OP_INVERT;
    case AmdGpu::StencilFunc::AddWrap:
        return VK_STENCIL_OP_INCREMENT_AND_WRAP;
    case AmdGpu::StencilFunc::SubWrap:
        return VK_STENCIL_OP_DECREMENT_AND_WRAP;
    case AmdGpu::StencilFunc::ReplaceOp:
        return VK_STENCIL_OP_REPLACE;
    case AmdGpu::StencilFunc::Ones:
        LOG_WARNING(Render_Vulkan, "Unsupported stencil op %u, using Replace.",
                    static_cast<u32>(op));
        return VK_STENCIL_OP_REPLACE;
    case AmdGpu::StencilFunc::And:
    case AmdGpu::StencilFunc::Or:
    case AmdGpu::StencilFunc::Xor:
    case AmdGpu::StencilFunc::Nand:
    case AmdGpu::StencilFunc::Nor:
    case AmdGpu::StencilFunc::Xnor:
        // Bitwise stencil operations have no Vulkan equivalent; eKeep is the safest fallback.
        LOG_WARNING(Render_Vulkan, "Unsupported bitwise stencil op %u, using Keep.",
                    static_cast<u32>(op));
        return VK_STENCIL_OP_KEEP;
    default:
        UNREACHABLE();
        return VK_STENCIL_OP_KEEP;
    }
}

VkCompareOp CompareOp(AmdGpu::CompareFunc func) {
    switch (func) {
    case AmdGpu::CompareFunc::Always:
        return VK_COMPARE_OP_ALWAYS;
    case AmdGpu::CompareFunc::Equal:
        return VK_COMPARE_OP_EQUAL;
    case AmdGpu::CompareFunc::GreaterEqual:
        return VK_COMPARE_OP_GREATER_OR_EQUAL;
    case AmdGpu::CompareFunc::Greater:
        return VK_COMPARE_OP_GREATER;
    case AmdGpu::CompareFunc::LessEqual:
        return VK_COMPARE_OP_LESS_OR_EQUAL;
    case AmdGpu::CompareFunc::Less:
        return VK_COMPARE_OP_LESS;
    case AmdGpu::CompareFunc::NotEqual:
        return VK_COMPARE_OP_NOT_EQUAL;
    case AmdGpu::CompareFunc::Never:
        return VK_COMPARE_OP_NEVER;
    default:
        UNREACHABLE();
        return VK_COMPARE_OP_ALWAYS;
    }
}

bool IsPrimitiveCulled(AmdGpu::PrimitiveType type) {
    switch (type) {
    case AmdGpu::PrimitiveType::TriangleList:
    case AmdGpu::PrimitiveType::TriangleFan:
    case AmdGpu::PrimitiveType::TriangleStrip:
    case AmdGpu::PrimitiveType::PatchPrimitive:
    case AmdGpu::PrimitiveType::AdjTriangleList:
    case AmdGpu::PrimitiveType::AdjTriangleStrip:
    case AmdGpu::PrimitiveType::QuadList:
    case AmdGpu::PrimitiveType::QuadStrip:
    case AmdGpu::PrimitiveType::Polygon:
        return true;
    case AmdGpu::PrimitiveType::None:
    case AmdGpu::PrimitiveType::PointList:
    case AmdGpu::PrimitiveType::LineList:
    case AmdGpu::PrimitiveType::LineStrip:
    case AmdGpu::PrimitiveType::AdjLineList:
    case AmdGpu::PrimitiveType::AdjLineStrip:
    case AmdGpu::PrimitiveType::RectList: // Screen-aligned rectangles that are not culled
    case AmdGpu::PrimitiveType::LineLoop:
        return false;
    default:
        UNREACHABLE();
        return true;
    }
}

VkPrimitiveTopology PrimitiveType(AmdGpu::PrimitiveType type) {
    switch (type) {
    case AmdGpu::PrimitiveType::PointList:
        return VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
    case AmdGpu::PrimitiveType::LineList:
        return VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
    case AmdGpu::PrimitiveType::LineStrip:
        return VK_PRIMITIVE_TOPOLOGY_LINE_STRIP;
    case AmdGpu::PrimitiveType::TriangleList:
        return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    case AmdGpu::PrimitiveType::TriangleFan:
    case AmdGpu::PrimitiveType::Polygon:
        return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN;
    case AmdGpu::PrimitiveType::TriangleStrip:
        return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
    case AmdGpu::PrimitiveType::QuadStrip:
        LOG_WARNING(Render_Vulkan, "Unimplemented primitive type, using TriangleStrip.");
        return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
    case AmdGpu::PrimitiveType::AdjLineList:
        return VK_PRIMITIVE_TOPOLOGY_LINE_LIST_WITH_ADJACENCY;
    case AmdGpu::PrimitiveType::AdjLineStrip:
        return VK_PRIMITIVE_TOPOLOGY_LINE_STRIP_WITH_ADJACENCY;
    case AmdGpu::PrimitiveType::AdjTriangleList:
        return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST_WITH_ADJACENCY;
    case AmdGpu::PrimitiveType::AdjTriangleStrip:
        return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP_WITH_ADJACENCY;
    case AmdGpu::PrimitiveType::PatchPrimitive:
        return VK_PRIMITIVE_TOPOLOGY_PATCH_LIST;
    case AmdGpu::PrimitiveType::QuadList:
    case AmdGpu::PrimitiveType::RectList:
        return VK_PRIMITIVE_TOPOLOGY_PATCH_LIST;
    default:
        UNREACHABLE_MSG("Unimplemented primitive type: %u", static_cast<u32>(type));
        return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    }
}

VkPolygonMode PolygonMode(AmdGpu::PolygonMode mode) {
    switch (mode) {
    case AmdGpu::PolygonMode::Point:
        return VK_POLYGON_MODE_POINT;
    case AmdGpu::PolygonMode::Line:
        return VK_POLYGON_MODE_LINE;
    case AmdGpu::PolygonMode::Fill:
        return VK_POLYGON_MODE_FILL;
    default:
        UNREACHABLE();
        return VK_POLYGON_MODE_FILL;
    }
}

VkCullModeFlags CullMode(AmdGpu::CullMode mode) {
    switch (mode) {
    case AmdGpu::CullMode::None:
        return VK_CULL_MODE_NONE;
    case AmdGpu::CullMode::Front:
        return VK_CULL_MODE_FRONT_BIT;
    case AmdGpu::CullMode::Back:
        return VK_CULL_MODE_BACK_BIT;
    case AmdGpu::CullMode::FrontAndBack:
        return VK_CULL_MODE_FRONT_AND_BACK;
    default:
        UNREACHABLE();
        return VK_CULL_MODE_NONE;
    }
}

VkFrontFace FrontFace(AmdGpu::FrontFace face) {
    switch (face) {
    case AmdGpu::FrontFace::Clockwise:
        return VK_FRONT_FACE_CLOCKWISE;
    case AmdGpu::FrontFace::CounterClockwise:
        return VK_FRONT_FACE_COUNTER_CLOCKWISE;
    default:
        UNREACHABLE();
        return VK_FRONT_FACE_CLOCKWISE;
    }
}

VkBlendFactor BlendFactor(AmdGpu::BlendControl::BlendFactor factor) {
    using BlendFactor = AmdGpu::BlendControl::BlendFactor;
    switch (factor) {
    case BlendFactor::Zero:
        return VK_BLEND_FACTOR_ZERO;
    case BlendFactor::One:
        return VK_BLEND_FACTOR_ONE;
    case BlendFactor::SrcColor:
        return VK_BLEND_FACTOR_SRC_COLOR;
    case BlendFactor::OneMinusSrcColor:
        return VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR;
    case BlendFactor::SrcAlpha:
        return VK_BLEND_FACTOR_SRC_ALPHA;
    case BlendFactor::OneMinusSrcAlpha:
        return VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    case BlendFactor::DstAlpha:
        return VK_BLEND_FACTOR_DST_ALPHA;
    case BlendFactor::OneMinusDstAlpha:
        return VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA;
    case BlendFactor::DstColor:
        return VK_BLEND_FACTOR_DST_COLOR;
    case BlendFactor::OneMinusDstColor:
        return VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR;
    case BlendFactor::SrcAlphaSaturate:
        return VK_BLEND_FACTOR_SRC_ALPHA_SATURATE;
    case BlendFactor::ConstantColor:
        return VK_BLEND_FACTOR_CONSTANT_COLOR;
    case BlendFactor::OneMinusConstantColor:
        return VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_COLOR;
    case BlendFactor::Src1Color:
        return VK_BLEND_FACTOR_SRC1_COLOR;
    case BlendFactor::InvSrc1Color:
        return VK_BLEND_FACTOR_ONE_MINUS_SRC1_COLOR;
    case BlendFactor::Src1Alpha:
        return VK_BLEND_FACTOR_SRC1_ALPHA;
    case BlendFactor::InvSrc1Alpha:
        return VK_BLEND_FACTOR_ONE_MINUS_SRC1_ALPHA;
    case BlendFactor::ConstantAlpha:
        return VK_BLEND_FACTOR_CONSTANT_ALPHA;
    case BlendFactor::OneMinusConstantAlpha:
        return VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_ALPHA;
    default:
        UNREACHABLE_MSG("Unknown blend factor: %u", static_cast<u32>(factor));
    }
}

bool IsDualSourceBlendFactor(AmdGpu::BlendControl::BlendFactor factor) {
    using BlendFactor = AmdGpu::BlendControl::BlendFactor;
    switch (factor) {
    case BlendFactor::Src1Color:
    case BlendFactor::Src1Alpha:
    case BlendFactor::InvSrc1Color:
    case BlendFactor::InvSrc1Alpha:
        return true;
    default:
        return false;
    }
}

VkBlendOp BlendOp(AmdGpu::BlendControl::BlendFunc func) {
    using BlendFunc = AmdGpu::BlendControl::BlendFunc;
    switch (func) {
    case BlendFunc::Add:
        return VK_BLEND_OP_ADD;
    case BlendFunc::Subtract:
        return VK_BLEND_OP_SUBTRACT;
    case BlendFunc::Min:
        return VK_BLEND_OP_MIN;
    case BlendFunc::Max:
        return VK_BLEND_OP_MAX;
    case BlendFunc::ReverseSubtract:
        return VK_BLEND_OP_REVERSE_SUBTRACT;
    default:
        UNREACHABLE_MSG("Unknown blend op: %u", static_cast<u32>(func));
    }
}

void NormalizeMinMaxBlend(VkBlendOp& op, VkBlendFactor& src, VkBlendFactor& dst,
                          AmdGpu::NumberFormat format) {
    if (format != AmdGpu::NumberFormat::Unorm && format != AmdGpu::NumberFormat::Srgb) {
        return;
    }
    if (op != VK_BLEND_OP_MIN && op != VK_BLEND_OP_MAX) {
        return;
    }
    if ((src != VK_BLEND_FACTOR_ZERO && src != VK_BLEND_FACTOR_ONE) ||
        (dst != VK_BLEND_FACTOR_ZERO && dst != VK_BLEND_FACTOR_ONE) ||
        (src != VK_BLEND_FACTOR_ZERO && dst != VK_BLEND_FACTOR_ZERO)) {
        return;
    }
    if (op == VK_BLEND_OP_MIN) {
        src = VK_BLEND_FACTOR_ZERO;
        dst = VK_BLEND_FACTOR_ZERO;
    }
    op = VK_BLEND_OP_ADD;
}

VkLogicOp LogicOp(AmdGpu::ColorControl::LogicOp logic_op) {
    using LogicOp = AmdGpu::ColorControl::LogicOp;
    switch (logic_op) {
    case LogicOp::Clear:
        return VK_LOGIC_OP_CLEAR;
    case LogicOp::Nor:
        return VK_LOGIC_OP_NOR;
    case LogicOp::AndInverted:
        return VK_LOGIC_OP_AND_INVERTED;
    case LogicOp::CopyInverted:
        return VK_LOGIC_OP_COPY_INVERTED;
    case LogicOp::AndReverse:
        return VK_LOGIC_OP_AND_REVERSE;
    case LogicOp::Invert:
        return VK_LOGIC_OP_INVERT;
    case LogicOp::BrushXor:
        LOG_WARNING(Render_Vulkan, "Unimplemented logic op 0x%x, using closest equivalent",
                    u32(logic_op));
        [[fallthrough]];
    case LogicOp::Xor:
        return VK_LOGIC_OP_XOR;
    case LogicOp::Nand:
        return VK_LOGIC_OP_NAND;
    case LogicOp::And:
        return VK_LOGIC_OP_AND;
    case LogicOp::Equiv:
        return VK_LOGIC_OP_EQUIVALENT;
    case LogicOp::Noop:
        return VK_LOGIC_OP_NO_OP;
    case LogicOp::OrInverted:
        return VK_LOGIC_OP_OR_INVERTED;
    case LogicOp::Copy:
        return VK_LOGIC_OP_COPY;
    case LogicOp::OrReverse:
        return VK_LOGIC_OP_OR_REVERSE;
    case LogicOp::Or:
        return VK_LOGIC_OP_OR;
    case LogicOp::Set:
        return VK_LOGIC_OP_SET;
    default:
        UNREACHABLE_MSG("Unknown logic op %u", u32(logic_op));
    }
}

// https://github.com/chaotic-cx/mesa-mirror/blob/0954afff5/src/amd/vulkan/radv_sampler.c#L21
VkSamplerAddressMode ClampMode(AmdGpu::ClampMode mode) {
    switch (mode) {
    case AmdGpu::ClampMode::Wrap:
        return VK_SAMPLER_ADDRESS_MODE_REPEAT;
    case AmdGpu::ClampMode::Mirror:
        return VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
    case AmdGpu::ClampMode::ClampLastTexel:
        return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    case AmdGpu::ClampMode::MirrorOnceHalfBorder:
    case AmdGpu::ClampMode::MirrorOnceBorder:
        LOG_WARNING(Render_Vulkan, "Unimplemented clamp mode %u, using closest equivalent.",
                    static_cast<u32>(mode));
        [[fallthrough]];
    case AmdGpu::ClampMode::MirrorOnceLastTexel:
        return VK_SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_EDGE;
    case AmdGpu::ClampMode::ClampHalfBorder:
        LOG_WARNING(Render_Vulkan, "Unimplemented clamp mode %u, using closest equivalent.",
                    static_cast<u32>(mode));
        [[fallthrough]];
    case AmdGpu::ClampMode::ClampBorder:
        return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    default:
        UNREACHABLE();
    }
}

VkCompareOp DepthCompare(AmdGpu::DepthCompare comp) {
    switch (comp) {
    case AmdGpu::DepthCompare::Never:
        return VK_COMPARE_OP_NEVER;
    case AmdGpu::DepthCompare::Less:
        return VK_COMPARE_OP_LESS;
    case AmdGpu::DepthCompare::Equal:
        return VK_COMPARE_OP_EQUAL;
    case AmdGpu::DepthCompare::LessEqual:
        return VK_COMPARE_OP_LESS_OR_EQUAL;
    case AmdGpu::DepthCompare::Greater:
        return VK_COMPARE_OP_GREATER;
    case AmdGpu::DepthCompare::NotEqual:
        return VK_COMPARE_OP_NOT_EQUAL;
    case AmdGpu::DepthCompare::GreaterEqual:
        return VK_COMPARE_OP_GREATER_OR_EQUAL;
    case AmdGpu::DepthCompare::Always:
        return VK_COMPARE_OP_ALWAYS;
    default:
        UNREACHABLE();
    }
}

VkFilter Filter(AmdGpu::Filter filter) {
    switch (filter) {
    case AmdGpu::Filter::Point:
    case AmdGpu::Filter::AnisoPoint:
        return VK_FILTER_NEAREST;
    case AmdGpu::Filter::Bilinear:
    case AmdGpu::Filter::AnisoLinear:
        return VK_FILTER_LINEAR;
    default:
        UNREACHABLE();
    }
}

VkSamplerReductionMode FilterMode(AmdGpu::FilterMode mode) {
    switch (mode) {
    case AmdGpu::FilterMode::Blend:
        return VK_SAMPLER_REDUCTION_MODE_WEIGHTED_AVERAGE;
    case AmdGpu::FilterMode::Min:
        return VK_SAMPLER_REDUCTION_MODE_MIN;
    case AmdGpu::FilterMode::Max:
        return VK_SAMPLER_REDUCTION_MODE_MAX;
    default:
        UNREACHABLE();
    }
}

VkSamplerMipmapMode MipFilter(AmdGpu::MipFilter filter) {
    switch (filter) {
    case AmdGpu::MipFilter::Point:
        return VK_SAMPLER_MIPMAP_MODE_NEAREST;
    case AmdGpu::MipFilter::Linear:
        return VK_SAMPLER_MIPMAP_MODE_LINEAR;
    case AmdGpu::MipFilter::None:
        return VK_SAMPLER_MIPMAP_MODE_NEAREST;
    default:
        UNREACHABLE();
    }
}

VkBorderColor BorderColor(AmdGpu::BorderColor color) {
    switch (color) {
    case AmdGpu::BorderColor::TransparentBlack:
        return VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK;
    case AmdGpu::BorderColor::OpaqueBlack:
        return VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK;
    case AmdGpu::BorderColor::White:
        return VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
    case AmdGpu::BorderColor::Custom:
        return VK_BORDER_COLOR_FLOAT_CUSTOM_EXT;
    default:
        UNREACHABLE();
    }
}

VkComponentSwizzle ComponentSwizzle(AmdGpu::CompSwizzle comp_swizzle) {
    switch (comp_swizzle) {
    case AmdGpu::CompSwizzle::Zero:
        return VK_COMPONENT_SWIZZLE_ZERO;
    case AmdGpu::CompSwizzle::One:
        return VK_COMPONENT_SWIZZLE_ONE;
    case AmdGpu::CompSwizzle::Red:
        return VK_COMPONENT_SWIZZLE_R;
    case AmdGpu::CompSwizzle::Green:
        return VK_COMPONENT_SWIZZLE_G;
    case AmdGpu::CompSwizzle::Blue:
        return VK_COMPONENT_SWIZZLE_B;
    case AmdGpu::CompSwizzle::Alpha:
        return VK_COMPONENT_SWIZZLE_A;
    default:
        UNREACHABLE();
    }
}

VkComponentMapping ComponentMapping(AmdGpu::CompMapping comp_mapping) {
    return VkComponentMapping{
        .r = ComponentSwizzle(comp_mapping.r),
        .g = ComponentSwizzle(comp_mapping.g),
        .b = ComponentSwizzle(comp_mapping.b),
        .a = ComponentSwizzle(comp_mapping.a),
    };
}

// Texel buffer feature flags are not needed as format is interpreted in-shader.
static constexpr VkFormatFeatureFlags2 BufferRead = VK_FORMAT_FEATURE_2_VERTEX_BUFFER_BIT;
static constexpr VkFormatFeatureFlags2 BufferWrite = static_cast<VkFormatFeatureFlags2>(0);
static constexpr VkFormatFeatureFlags2 ImageRead = VK_FORMAT_FEATURE_2_TRANSFER_SRC_BIT |
                                                   VK_FORMAT_FEATURE_2_TRANSFER_DST_BIT |
                                                   VK_FORMAT_FEATURE_2_SAMPLED_IMAGE_BIT;
static constexpr VkFormatFeatureFlags2 ImageWrite =
    VK_FORMAT_FEATURE_2_STORAGE_IMAGE_BIT | VK_FORMAT_FEATURE_2_STORAGE_READ_WITHOUT_FORMAT_BIT |
    VK_FORMAT_FEATURE_2_STORAGE_WRITE_WITHOUT_FORMAT_BIT;
static constexpr VkFormatFeatureFlags2 Mrt = VK_FORMAT_FEATURE_2_COLOR_ATTACHMENT_BIT;

// Table 8.13 Data and Image Formats [Sea Islands Series Instruction Set Architecture]
static constexpr VkFormatFeatureFlags2 GetDataFormatFeatureFlags(
    const AmdGpu::DataFormat data_format) {
    switch (data_format) {
    case AmdGpu::DataFormat::FormatInvalid:
    case AmdGpu::DataFormat::Format8:
    case AmdGpu::DataFormat::Format16:
    case AmdGpu::DataFormat::Format8_8:
    case AmdGpu::DataFormat::Format32:
    case AmdGpu::DataFormat::Format16_16:
    case AmdGpu::DataFormat::Format10_11_11:
    case AmdGpu::DataFormat::Format11_11_10:
    case AmdGpu::DataFormat::Format10_10_10_2:
    case AmdGpu::DataFormat::Format2_10_10_10:
    case AmdGpu::DataFormat::Format8_8_8_8:
    case AmdGpu::DataFormat::Format32_32:
    case AmdGpu::DataFormat::Format16_16_16_16:
    case AmdGpu::DataFormat::Format32_32_32_32:
        return BufferRead | BufferWrite | ImageRead | ImageWrite | Mrt;
    case AmdGpu::DataFormat::Format32_32_32:
        return BufferRead | BufferWrite | ImageRead;
    case AmdGpu::DataFormat::Format5_6_5:
    case AmdGpu::DataFormat::Format1_5_5_5:
    case AmdGpu::DataFormat::Format5_5_5_1:
    case AmdGpu::DataFormat::Format4_4_4_4:
        return ImageRead | ImageWrite | Mrt;
    case AmdGpu::DataFormat::Format8_24:
    case AmdGpu::DataFormat::Format24_8:
    case AmdGpu::DataFormat::FormatX24_8_32:
        return ImageRead | Mrt;
    case AmdGpu::DataFormat::FormatGB_GR:
    case AmdGpu::DataFormat::FormatBG_RG:
    case AmdGpu::DataFormat::Format5_9_9_9:
    case AmdGpu::DataFormat::FormatBc1:
    case AmdGpu::DataFormat::FormatBc2:
    case AmdGpu::DataFormat::FormatBc3:
    case AmdGpu::DataFormat::FormatBc4:
    case AmdGpu::DataFormat::FormatBc5:
    case AmdGpu::DataFormat::FormatBc6:
    case AmdGpu::DataFormat::FormatBc7:
    case AmdGpu::DataFormat::Format4_4:
    case AmdGpu::DataFormat::Format6_5_5:
    case AmdGpu::DataFormat::Format1:
    case AmdGpu::DataFormat::Format1_Reversed:
    case AmdGpu::DataFormat::Format32_As_8:
    case AmdGpu::DataFormat::Format32_As_8_8:
    case AmdGpu::DataFormat::Format32_As_32_32_32_32:
        return ImageRead;
    case AmdGpu::DataFormat::FormatFmask8_S2_F1:
    case AmdGpu::DataFormat::FormatFmask8_S4_F1:
    case AmdGpu::DataFormat::FormatFmask8_S8_F1:
    case AmdGpu::DataFormat::FormatFmask8_S2_F2:
    case AmdGpu::DataFormat::FormatFmask8_S4_F2:
    case AmdGpu::DataFormat::FormatFmask8_S4_F4:
    case AmdGpu::DataFormat::FormatFmask16_S16_F1:
    case AmdGpu::DataFormat::FormatFmask16_S8_F2:
    case AmdGpu::DataFormat::FormatFmask32_S16_F2:
    case AmdGpu::DataFormat::FormatFmask32_S8_F4:
    case AmdGpu::DataFormat::FormatFmask32_S8_F8:
    case AmdGpu::DataFormat::FormatFmask64_S16_F4:
    case AmdGpu::DataFormat::FormatFmask64_S16_F8:
        return ImageRead | ImageWrite;
    }
    UNREACHABLE_MSG("Missing feature flags for data format %u", static_cast<u32>(data_format));
}

// Table 8.13 Data and Image Formats [Sea Islands Series Instruction Set Architecture]
static constexpr VkFormatFeatureFlags2 GetNumberFormatFeatureFlags(
    const AmdGpu::NumberFormat number_format) {
    switch (number_format) {
    case AmdGpu::NumberFormat::Unorm:
    case AmdGpu::NumberFormat::Snorm:
    case AmdGpu::NumberFormat::Uint:
    case AmdGpu::NumberFormat::Sint:
    case AmdGpu::NumberFormat::Float:
        return BufferRead | BufferWrite | ImageRead | ImageWrite | Mrt;
    case AmdGpu::NumberFormat::Uscaled:
    case AmdGpu::NumberFormat::Sscaled:
    case AmdGpu::NumberFormat::SnormNz:
        return BufferRead | ImageRead;
    case AmdGpu::NumberFormat::Srgb:
        return ImageRead | Mrt;
    case AmdGpu::NumberFormat::Ubnorm:
    case AmdGpu::NumberFormat::UbnormNz:
    case AmdGpu::NumberFormat::Ubint:
    case AmdGpu::NumberFormat::Ubscaled:
        return ImageRead;
    }
    UNREACHABLE_MSG("Missing feature flags for number format %u", static_cast<u32>(number_format));
}

static constexpr SurfaceFormatInfo CreateSurfaceFormatInfo(const AmdGpu::DataFormat data_format,
                                                           const AmdGpu::NumberFormat number_format,
                                                           const VkFormat vk_format) {
    return {
        .data_format = data_format,
        .number_format = number_format,
        .vk_format = vk_format,
        .flags =
            GetDataFormatFeatureFlags(data_format) & GetNumberFormatFeatureFlags(number_format),
    };
}

// Uscaled, Sscaled, and Ubnorm formats are automatically remapped and handled in shader.
static constexpr SurfaceFormatInfo formats[] = {
    // Invalid
    CreateSurfaceFormatInfo(AmdGpu::DataFormat::FormatInvalid, AmdGpu::NumberFormat::Unorm,
                            VK_FORMAT_UNDEFINED),
    CreateSurfaceFormatInfo(AmdGpu::DataFormat::FormatInvalid, AmdGpu::NumberFormat::Snorm,
                            VK_FORMAT_UNDEFINED),
    CreateSurfaceFormatInfo(AmdGpu::DataFormat::FormatInvalid, AmdGpu::NumberFormat::Uscaled,
                            VK_FORMAT_UNDEFINED),
    CreateSurfaceFormatInfo(AmdGpu::DataFormat::FormatInvalid, AmdGpu::NumberFormat::Sscaled,
                            VK_FORMAT_UNDEFINED),
    CreateSurfaceFormatInfo(AmdGpu::DataFormat::FormatInvalid, AmdGpu::NumberFormat::Uint,
                            VK_FORMAT_UNDEFINED),
    CreateSurfaceFormatInfo(AmdGpu::DataFormat::FormatInvalid, AmdGpu::NumberFormat::Sint,
                            VK_FORMAT_UNDEFINED),
    CreateSurfaceFormatInfo(AmdGpu::DataFormat::FormatInvalid, AmdGpu::NumberFormat::SnormNz,
                            VK_FORMAT_UNDEFINED),
    CreateSurfaceFormatInfo(AmdGpu::DataFormat::FormatInvalid, AmdGpu::NumberFormat::Float,
                            VK_FORMAT_UNDEFINED),
    CreateSurfaceFormatInfo(AmdGpu::DataFormat::FormatInvalid, AmdGpu::NumberFormat::Srgb,
                            VK_FORMAT_UNDEFINED),
    CreateSurfaceFormatInfo(AmdGpu::DataFormat::FormatInvalid, AmdGpu::NumberFormat::Ubnorm,
                            VK_FORMAT_UNDEFINED),
    CreateSurfaceFormatInfo(AmdGpu::DataFormat::FormatInvalid, AmdGpu::NumberFormat::UbnormNz,
                            VK_FORMAT_UNDEFINED),
    CreateSurfaceFormatInfo(AmdGpu::DataFormat::FormatInvalid, AmdGpu::NumberFormat::Ubint,
                            VK_FORMAT_UNDEFINED),
    CreateSurfaceFormatInfo(AmdGpu::DataFormat::FormatInvalid, AmdGpu::NumberFormat::Ubscaled,
                            VK_FORMAT_UNDEFINED),
    // 8
    CreateSurfaceFormatInfo(AmdGpu::DataFormat::Format8, AmdGpu::NumberFormat::Unorm,
                            VK_FORMAT_R8_UNORM),
    CreateSurfaceFormatInfo(AmdGpu::DataFormat::Format8, AmdGpu::NumberFormat::Snorm,
                            VK_FORMAT_R8_SNORM),
    CreateSurfaceFormatInfo(AmdGpu::DataFormat::Format8, AmdGpu::NumberFormat::Uint,
                            VK_FORMAT_R8_UINT),
    CreateSurfaceFormatInfo(AmdGpu::DataFormat::Format8, AmdGpu::NumberFormat::Sint,
                            VK_FORMAT_R8_SINT),
    CreateSurfaceFormatInfo(AmdGpu::DataFormat::Format8, AmdGpu::NumberFormat::Srgb,
                            VK_FORMAT_R8_SRGB),
    // 16
    CreateSurfaceFormatInfo(AmdGpu::DataFormat::Format16, AmdGpu::NumberFormat::Unorm,
                            VK_FORMAT_R16_UNORM),
    CreateSurfaceFormatInfo(AmdGpu::DataFormat::Format16, AmdGpu::NumberFormat::Snorm,
                            VK_FORMAT_R16_SNORM),
    CreateSurfaceFormatInfo(AmdGpu::DataFormat::Format16, AmdGpu::NumberFormat::Uint,
                            VK_FORMAT_R16_UINT),
    CreateSurfaceFormatInfo(AmdGpu::DataFormat::Format16, AmdGpu::NumberFormat::Sint,
                            VK_FORMAT_R16_SINT),
    CreateSurfaceFormatInfo(AmdGpu::DataFormat::Format16, AmdGpu::NumberFormat::Float,
                            VK_FORMAT_R16_SFLOAT),
    // 8_8
    CreateSurfaceFormatInfo(AmdGpu::DataFormat::Format8_8, AmdGpu::NumberFormat::Unorm,
                            VK_FORMAT_R8G8_UNORM),
    CreateSurfaceFormatInfo(AmdGpu::DataFormat::Format8_8, AmdGpu::NumberFormat::Snorm,
                            VK_FORMAT_R8G8_SNORM),
    CreateSurfaceFormatInfo(AmdGpu::DataFormat::Format8_8, AmdGpu::NumberFormat::Uint,
                            VK_FORMAT_R8G8_UINT),
    CreateSurfaceFormatInfo(AmdGpu::DataFormat::Format8_8, AmdGpu::NumberFormat::Sint,
                            VK_FORMAT_R8G8_SINT),
    CreateSurfaceFormatInfo(AmdGpu::DataFormat::Format8_8, AmdGpu::NumberFormat::Srgb,
                            VK_FORMAT_R8G8_SRGB),
    // 32
    CreateSurfaceFormatInfo(AmdGpu::DataFormat::Format32, AmdGpu::NumberFormat::Uint,
                            VK_FORMAT_R32_UINT),
    CreateSurfaceFormatInfo(AmdGpu::DataFormat::Format32, AmdGpu::NumberFormat::Sint,
                            VK_FORMAT_R32_SINT),
    CreateSurfaceFormatInfo(AmdGpu::DataFormat::Format32, AmdGpu::NumberFormat::Float,
                            VK_FORMAT_R32_SFLOAT),
    // 16_16
    CreateSurfaceFormatInfo(AmdGpu::DataFormat::Format16_16, AmdGpu::NumberFormat::Unorm,
                            VK_FORMAT_R16G16_UNORM),
    CreateSurfaceFormatInfo(AmdGpu::DataFormat::Format16_16, AmdGpu::NumberFormat::Snorm,
                            VK_FORMAT_R16G16_SNORM),
    CreateSurfaceFormatInfo(AmdGpu::DataFormat::Format16_16, AmdGpu::NumberFormat::Uint,
                            VK_FORMAT_R16G16_UINT),
    CreateSurfaceFormatInfo(AmdGpu::DataFormat::Format16_16, AmdGpu::NumberFormat::Sint,
                            VK_FORMAT_R16G16_SINT),
    CreateSurfaceFormatInfo(AmdGpu::DataFormat::Format16_16, AmdGpu::NumberFormat::Float,
                            VK_FORMAT_R16G16_SFLOAT),
    // 10_11_11
    CreateSurfaceFormatInfo(AmdGpu::DataFormat::Format10_11_11, AmdGpu::NumberFormat::Float,
                            VK_FORMAT_B10G11R11_UFLOAT_PACK32),
    // 11_11_10 - Remapped to 10_11_11.
    // 10_10_10_2 - Remapped to 2_10_10_10.
    // 2_10_10_10
    CreateSurfaceFormatInfo(AmdGpu::DataFormat::Format2_10_10_10, AmdGpu::NumberFormat::Unorm,
                            VK_FORMAT_A2B10G10R10_UNORM_PACK32),
    CreateSurfaceFormatInfo(AmdGpu::DataFormat::Format2_10_10_10, AmdGpu::NumberFormat::Snorm,
                            VK_FORMAT_A2B10G10R10_SNORM_PACK32),
    CreateSurfaceFormatInfo(AmdGpu::DataFormat::Format2_10_10_10, AmdGpu::NumberFormat::Uint,
                            VK_FORMAT_A2B10G10R10_UINT_PACK32),
    CreateSurfaceFormatInfo(AmdGpu::DataFormat::Format2_10_10_10, AmdGpu::NumberFormat::Sint,
                            VK_FORMAT_A2B10G10R10_SINT_PACK32),
    // 8_8_8_8
    CreateSurfaceFormatInfo(AmdGpu::DataFormat::Format8_8_8_8, AmdGpu::NumberFormat::Unorm,
                            VK_FORMAT_R8G8B8A8_UNORM),
    CreateSurfaceFormatInfo(AmdGpu::DataFormat::Format8_8_8_8, AmdGpu::NumberFormat::Snorm,
                            VK_FORMAT_R8G8B8A8_SNORM),
    CreateSurfaceFormatInfo(AmdGpu::DataFormat::Format8_8_8_8, AmdGpu::NumberFormat::Uint,
                            VK_FORMAT_R8G8B8A8_UINT),
    CreateSurfaceFormatInfo(AmdGpu::DataFormat::Format8_8_8_8, AmdGpu::NumberFormat::Sint,
                            VK_FORMAT_R8G8B8A8_SINT),
    CreateSurfaceFormatInfo(AmdGpu::DataFormat::Format8_8_8_8, AmdGpu::NumberFormat::Srgb,
                            VK_FORMAT_R8G8B8A8_SRGB),
    // 32_32
    CreateSurfaceFormatInfo(AmdGpu::DataFormat::Format32_32, AmdGpu::NumberFormat::Uint,
                            VK_FORMAT_R32G32_UINT),
    CreateSurfaceFormatInfo(AmdGpu::DataFormat::Format32_32, AmdGpu::NumberFormat::Sint,
                            VK_FORMAT_R32G32_SINT),
    CreateSurfaceFormatInfo(AmdGpu::DataFormat::Format32_32, AmdGpu::NumberFormat::Float,
                            VK_FORMAT_R32G32_SFLOAT),
    // 16_16_16_16
    CreateSurfaceFormatInfo(AmdGpu::DataFormat::Format16_16_16_16, AmdGpu::NumberFormat::Unorm,
                            VK_FORMAT_R16G16B16A16_UNORM),
    CreateSurfaceFormatInfo(AmdGpu::DataFormat::Format16_16_16_16, AmdGpu::NumberFormat::Snorm,
                            VK_FORMAT_R16G16B16A16_SNORM),
    CreateSurfaceFormatInfo(AmdGpu::DataFormat::Format16_16_16_16, AmdGpu::NumberFormat::Uint,
                            VK_FORMAT_R16G16B16A16_UINT),
    CreateSurfaceFormatInfo(AmdGpu::DataFormat::Format16_16_16_16, AmdGpu::NumberFormat::Sint,
                            VK_FORMAT_R16G16B16A16_SINT),
    CreateSurfaceFormatInfo(AmdGpu::DataFormat::Format16_16_16_16, AmdGpu::NumberFormat::SnormNz,
                            VK_FORMAT_R16G16B16A16_SNORM),
    CreateSurfaceFormatInfo(AmdGpu::DataFormat::Format16_16_16_16, AmdGpu::NumberFormat::Float,
                            VK_FORMAT_R16G16B16A16_SFLOAT),
    // 32_32_32
    CreateSurfaceFormatInfo(AmdGpu::DataFormat::Format32_32_32, AmdGpu::NumberFormat::Uint,
                            VK_FORMAT_R32G32B32_UINT),
    CreateSurfaceFormatInfo(AmdGpu::DataFormat::Format32_32_32, AmdGpu::NumberFormat::Sint,
                            VK_FORMAT_R32G32B32_SINT),
    CreateSurfaceFormatInfo(AmdGpu::DataFormat::Format32_32_32, AmdGpu::NumberFormat::Float,
                            VK_FORMAT_R32G32B32_SFLOAT),
    // 32_32_32_32
    CreateSurfaceFormatInfo(AmdGpu::DataFormat::Format32_32_32_32, AmdGpu::NumberFormat::Uint,
                            VK_FORMAT_R32G32B32A32_UINT),
    CreateSurfaceFormatInfo(AmdGpu::DataFormat::Format32_32_32_32, AmdGpu::NumberFormat::Sint,
                            VK_FORMAT_R32G32B32A32_SINT),
    CreateSurfaceFormatInfo(AmdGpu::DataFormat::Format32_32_32_32, AmdGpu::NumberFormat::Float,
                            VK_FORMAT_R32G32B32A32_SFLOAT),
    // 5_6_5
    CreateSurfaceFormatInfo(AmdGpu::DataFormat::Format5_6_5, AmdGpu::NumberFormat::Unorm,
                            VK_FORMAT_R5G6B5_UNORM_PACK16),
    // 1_5_5_5
    CreateSurfaceFormatInfo(AmdGpu::DataFormat::Format1_5_5_5, AmdGpu::NumberFormat::Unorm,
                            VK_FORMAT_A1R5G5B5_UNORM_PACK16),
    // 5_5_5_1
    CreateSurfaceFormatInfo(AmdGpu::DataFormat::Format5_5_5_1, AmdGpu::NumberFormat::Unorm,
                            VK_FORMAT_R5G5B5A1_UNORM_PACK16),
    // 4_4_4_4
    CreateSurfaceFormatInfo(AmdGpu::DataFormat::Format4_4_4_4, AmdGpu::NumberFormat::Unorm,
                            VK_FORMAT_B4G4R4A4_UNORM_PACK16),
    // 8_24
    // 24_8
    // X24_8_32
    // GB_GR
    // BG_RG
    // 5_9_9_9
    CreateSurfaceFormatInfo(AmdGpu::DataFormat::Format5_9_9_9, AmdGpu::NumberFormat::Float,
                            VK_FORMAT_E5B9G9R9_UFLOAT_PACK32),
    // BC1
    CreateSurfaceFormatInfo(AmdGpu::DataFormat::FormatBc1, AmdGpu::NumberFormat::Unorm,
                            VK_FORMAT_BC1_RGBA_UNORM_BLOCK),
    CreateSurfaceFormatInfo(AmdGpu::DataFormat::FormatBc1, AmdGpu::NumberFormat::Srgb,
                            VK_FORMAT_BC1_RGBA_SRGB_BLOCK),
    // BC2
    CreateSurfaceFormatInfo(AmdGpu::DataFormat::FormatBc2, AmdGpu::NumberFormat::Unorm,
                            VK_FORMAT_BC2_UNORM_BLOCK),
    CreateSurfaceFormatInfo(AmdGpu::DataFormat::FormatBc2, AmdGpu::NumberFormat::Srgb,
                            VK_FORMAT_BC2_SRGB_BLOCK),
    // BC3
    CreateSurfaceFormatInfo(AmdGpu::DataFormat::FormatBc3, AmdGpu::NumberFormat::Unorm,
                            VK_FORMAT_BC3_UNORM_BLOCK),
    CreateSurfaceFormatInfo(AmdGpu::DataFormat::FormatBc3, AmdGpu::NumberFormat::Srgb,
                            VK_FORMAT_BC3_SRGB_BLOCK),
    // BC4
    CreateSurfaceFormatInfo(AmdGpu::DataFormat::FormatBc4, AmdGpu::NumberFormat::Unorm,
                            VK_FORMAT_BC4_UNORM_BLOCK),
    CreateSurfaceFormatInfo(AmdGpu::DataFormat::FormatBc4, AmdGpu::NumberFormat::Snorm,
                            VK_FORMAT_BC4_SNORM_BLOCK),
    // BC5
    CreateSurfaceFormatInfo(AmdGpu::DataFormat::FormatBc5, AmdGpu::NumberFormat::Unorm,
                            VK_FORMAT_BC5_UNORM_BLOCK),
    CreateSurfaceFormatInfo(AmdGpu::DataFormat::FormatBc5, AmdGpu::NumberFormat::Snorm,
                            VK_FORMAT_BC5_SNORM_BLOCK),
    // BC6
    CreateSurfaceFormatInfo(AmdGpu::DataFormat::FormatBc6, AmdGpu::NumberFormat::Unorm,
                            VK_FORMAT_BC6H_UFLOAT_BLOCK),
    CreateSurfaceFormatInfo(AmdGpu::DataFormat::FormatBc6, AmdGpu::NumberFormat::Snorm,
                            VK_FORMAT_BC6H_SFLOAT_BLOCK),
    // BC7
    CreateSurfaceFormatInfo(AmdGpu::DataFormat::FormatBc7, AmdGpu::NumberFormat::Unorm,
                            VK_FORMAT_BC7_UNORM_BLOCK),
    CreateSurfaceFormatInfo(AmdGpu::DataFormat::FormatBc7, AmdGpu::NumberFormat::Srgb,
                            VK_FORMAT_BC7_SRGB_BLOCK),
};

TableView<SurfaceFormatInfo> SurfaceFormats() {
    return {formats, static_cast<u32>(sizeof(formats) / sizeof(formats[0]))};
}

/// The table made from the formats above, at compile time
static constexpr SurfaceFormatTable MakeSurfaceFormatTable() {
    SurfaceFormatTable result{};
    for (VkFormat& entry : result.formats) {
        entry = VK_FORMAT_UNDEFINED;
    }
    for (const SurfaceFormatInfo& supported_format : formats) {
        result.formats[GetSurfaceFormatTableIndex(supported_format.data_format,
                                                  supported_format.number_format)] =
            supported_format.vk_format;
    }
    return result;
}

constexpr SurfaceFormatTable surface_format_table = MakeSurfaceFormatTable();

static constexpr DepthFormatInfo CreateDepthFormatInfo(
    const DepthBuffer::ZFormat z_format, const DepthBuffer::StencilFormat stencil_format,
    const VkFormat vk_format) {
    return {
        .z_format = z_format,
        .stencil_format = stencil_format,
        .vk_format = vk_format,
        .flags = VK_FORMAT_FEATURE_2_DEPTH_STENCIL_ATTACHMENT_BIT,
    };
}

static constexpr DepthFormatInfo depth_formats[] = {
    // Invalid
    CreateDepthFormatInfo(DepthBuffer::ZFormat::Invalid, DepthBuffer::StencilFormat::Invalid,
                          VK_FORMAT_UNDEFINED),
    CreateDepthFormatInfo(DepthBuffer::ZFormat::Invalid, DepthBuffer::StencilFormat::Stencil8,
                          VK_FORMAT_D32_SFLOAT_S8_UINT),
    // 16
    CreateDepthFormatInfo(DepthBuffer::ZFormat::Z16, DepthBuffer::StencilFormat::Invalid,
                          VK_FORMAT_D16_UNORM),
    CreateDepthFormatInfo(DepthBuffer::ZFormat::Z16, DepthBuffer::StencilFormat::Stencil8,
                          VK_FORMAT_D16_UNORM_S8_UINT),
    // 32_Float
    CreateDepthFormatInfo(DepthBuffer::ZFormat::Z32Float, DepthBuffer::StencilFormat::Invalid,
                          VK_FORMAT_D32_SFLOAT),
    CreateDepthFormatInfo(DepthBuffer::ZFormat::Z32Float, DepthBuffer::StencilFormat::Stencil8,
                          VK_FORMAT_D32_SFLOAT_S8_UINT),
};

TableView<DepthFormatInfo> DepthFormats() {
    return {depth_formats, static_cast<u32>(sizeof(depth_formats) / sizeof(depth_formats[0]))};
}

VkFormat DepthFormat(DepthBuffer::ZFormat z_format, DepthBuffer::StencilFormat stencil_format) {
    for (const DepthFormatInfo& format_info : depth_formats) {
        if (format_info.z_format == z_format && format_info.stencil_format == stencil_format) {
            return format_info.vk_format;
        }
    }
    UNREACHABLE_MSG("Unknown z_format=%u and stencil_format=%u", static_cast<u32>(z_format),
                    static_cast<u32>(stencil_format));
    return VK_FORMAT_UNDEFINED;
}

VkClearValue ColorBufferClearValue(const AmdGpu::ColorBuffer& color_buffer) {
    const AmdGpu::CompMapping comp_swizzle = color_buffer.Swizzle();
    const AmdGpu::DataFormat format = AmdGpu::DataFormat(color_buffer.info.format);
    const AmdGpu::NumberFormat number_type = color_buffer.GetFixedNumberFormat();

    const u32& c0 = color_buffer.clear_word0;
    const u32& c1 = color_buffer.clear_word1;

    VkClearColorValue color{};

    switch (number_type) {
    case AmdGpu::NumberFormat::Uint:
    case AmdGpu::NumberFormat::Sint:
        color.uint32[3] = 1;
        break;
    default:
        color.float32[3] = 1.0f;
        break;
    }

    switch (format) {
    case AmdGpu::DataFormat::Format8:
        switch (number_type) {
        case AmdGpu::NumberFormat::Unorm:
        case AmdGpu::NumberFormat::Srgb: // Should we handle gamma correction here?
            color.float32[0] = NumberUtils::U8ToUnorm(c0 & 0xff);
            break;
            break;
        case AmdGpu::NumberFormat::Snorm:
        case AmdGpu::NumberFormat::SnormNz:
            color.float32[0] = NumberUtils::S8ToSnorm(c0 & 0xff);
            break;
        case AmdGpu::NumberFormat::Uint:
        case AmdGpu::NumberFormat::Sint:
            color.uint32[0] = c0;
            break;
        default:
            INVALID_NUMBER_FORMAT_COMBO;
            break;
        }
        break;
    case AmdGpu::DataFormat::Format16:
        switch (number_type) {
        case AmdGpu::NumberFormat::Unorm:
            color.float32[0] = NumberUtils::U16ToUnorm(c0 & 0xffff);
            break;
        case AmdGpu::NumberFormat::Snorm:
        case AmdGpu::NumberFormat::SnormNz:
            color.float32[0] = NumberUtils::S16ToSnorm(c0 & 0xffff);
            break;
        case AmdGpu::NumberFormat::Uint:
        case AmdGpu::NumberFormat::Sint:
            color.uint32[0] = c0;
            break;
        case AmdGpu::NumberFormat::Float:
            color.float32[0] = NumberUtils::Uf16ToF32(c0 & 0xffff);
            break;
        default:
            INVALID_NUMBER_FORMAT_COMBO;
            break;
        }
        break;
    case AmdGpu::DataFormat::Format8_8:
        switch (number_type) {
        case AmdGpu::NumberFormat::Unorm:
        case AmdGpu::NumberFormat::Srgb: // Should we handle gamma correction here?
            color.float32[0] = NumberUtils::U8ToUnorm(c0 & 0xff);
            color.float32[1] = NumberUtils::U8ToUnorm((c0 >> 8) & 0xff);
            break;
        case AmdGpu::NumberFormat::Snorm:
        case AmdGpu::NumberFormat::SnormNz:
            color.float32[0] = NumberUtils::S8ToSnorm(c0 & 0xff);
            color.float32[1] = NumberUtils::S8ToSnorm((c0 >> 8) & 0xff);
            break;
        case AmdGpu::NumberFormat::Uint:
        case AmdGpu::NumberFormat::Sint:
            color.uint32[0] = c0 & 0xff;
            color.uint32[1] = (c0 >> 8) & 0xff;
            break;
        default:
            INVALID_NUMBER_FORMAT_COMBO;
            break;
        }
        break;
    case AmdGpu::DataFormat::Format32:
        switch (number_type) {
        case AmdGpu::NumberFormat::Uint:
        case AmdGpu::NumberFormat::Sint:
            color.uint32[0] = c0;
            break;
        case AmdGpu::NumberFormat::Float:
            color.float32[0] = *(reinterpret_cast<const float*>(&c0));
            break;
        default:
            INVALID_NUMBER_FORMAT_COMBO;
            break;
        }
        break;
    case AmdGpu::DataFormat::Format16_16:
        switch (number_type) {
        case AmdGpu::NumberFormat::Unorm:
            color.float32[0] = NumberUtils::U16ToUnorm(c0 & 0xffff);
            color.float32[1] = NumberUtils::U16ToUnorm((c0 >> 16) & 0xffff);
            break;
        case AmdGpu::NumberFormat::Snorm:
        case AmdGpu::NumberFormat::SnormNz:
            color.float32[0] = NumberUtils::S16ToSnorm(c0 & 0xffff);
            color.float32[1] = NumberUtils::S16ToSnorm((c0 >> 16) & 0xffff);
            break;
        case AmdGpu::NumberFormat::Uint:
        case AmdGpu::NumberFormat::Sint:
            color.uint32[0] = c0 & 0xffff;
            color.uint32[1] = (c0 >> 16) & 0xffff;
            break;
        case AmdGpu::NumberFormat::Float:
            color.float32[0] = NumberUtils::Uf16ToF32(c0 & 0xffff);
            color.float32[1] = NumberUtils::Uf16ToF32((c0 >> 16) & 0xffff);
            break;
        default:
            INVALID_NUMBER_FORMAT_COMBO;
            break;
        }
        break;
    case AmdGpu::DataFormat::Format10_11_11:
        color.float32[0] = NumberUtils::Uf11ToF32(c0 & 0x7ff);
        color.float32[1] = NumberUtils::Uf11ToF32((c0 >> 11) & 0x7ff);
        color.float32[2] = NumberUtils::Uf10ToF32((c0 >> 22) & 0x3ff);
        break;
    case AmdGpu::DataFormat::Format11_11_10:
        color.float32[0] = NumberUtils::Uf10ToF32(c0 & 0x3ff);
        color.float32[1] = NumberUtils::Uf11ToF32((c0 >> 10) & 0x7ff);
        color.float32[2] = NumberUtils::Uf11ToF32((c0 >> 21) & 0x7ff);
        break;
    case AmdGpu::DataFormat::Format5_9_9_9: {
        int exponent;
        union {
            float f;
            u32 u;
        } scale;

        exponent = (c0 >> 27) - 10;
        scale.u = (exponent + 127) << 23;

        color.float32[0] = (c0 & 0x1ff) * scale.f;
        color.float32[1] = ((c0 >> 9) & 0x1ff) * scale.f;
        color.float32[2] = ((c0 >> 18) & 0x1ff) * scale.f;
        break;
    }
    case AmdGpu::DataFormat::Format10_10_10_2:
        switch (number_type) {
        case AmdGpu::NumberFormat::Unorm:
            color.float32[0] = NumberUtils::U2ToUnorm(c0 & 0x3);
            color.float32[1] = NumberUtils::U10ToUnorm((c0 >> 2) & 0x3ff);
            color.float32[2] = NumberUtils::U10ToUnorm((c0 >> 12) & 0x3ff);
            color.float32[3] = NumberUtils::U10ToUnorm(c0 >> 22);
            break;
        case AmdGpu::NumberFormat::Snorm:
        case AmdGpu::NumberFormat::SnormNz:
            color.float32[0] = NumberUtils::S2ToSnorm(c0 & 0x3);
            color.float32[1] = NumberUtils::S10ToSnorm((c0 >> 2) & 0x3ff);
            color.float32[2] = NumberUtils::S10ToSnorm((c0 >> 12) & 0x3ff);
            color.float32[3] = NumberUtils::S2ToSnorm(c0 >> 22);
            break;
        case AmdGpu::NumberFormat::Uint:
        case AmdGpu::NumberFormat::Sint:
            color.uint32[0] = c0 & 0x3;
            color.uint32[1] = (c0 >> 2) & 0x3ff;
            color.uint32[2] = (c0 >> 12) & 0x3ff;
            color.uint32[3] = c0 >> 22;
            break;
        default:
            INVALID_NUMBER_FORMAT_COMBO;
            break;
        }
        break;
    case AmdGpu::DataFormat::Format2_10_10_10:
        switch (number_type) {
        case AmdGpu::NumberFormat::Unorm:
            color.float32[0] = NumberUtils::U10ToUnorm(c0 & 0x3ff);
            color.float32[1] = NumberUtils::U10ToUnorm((c0 >> 10) & 0x3ff);
            color.float32[2] = NumberUtils::U10ToUnorm((c0 >> 20) & 0x3ff);
            color.float32[3] = NumberUtils::U2ToUnorm(c0 >> 30);
            break;
        case AmdGpu::NumberFormat::Snorm:
        case AmdGpu::NumberFormat::SnormNz:
            color.float32[0] = NumberUtils::S10ToSnorm(c0 & 0x3ff);
            color.float32[1] = NumberUtils::S10ToSnorm((c0 >> 10) & 0x3ff);
            color.float32[2] = NumberUtils::S10ToSnorm((c0 >> 20) & 0x3ff);
            color.float32[3] = NumberUtils::S2ToSnorm(c0 >> 30);
            break;
        case AmdGpu::NumberFormat::Uint:
        case AmdGpu::NumberFormat::Sint:
            color.uint32[0] = c0 & 0x3ff;
            color.uint32[1] = (c0 >> 10) & 0x3ff;
            color.uint32[2] = (c0 >> 20) & 0x3ff;
            color.uint32[3] = c0 >> 30;
            break;
        default:
            INVALID_NUMBER_FORMAT_COMBO;
            break;
        }
        break;
    case AmdGpu::DataFormat::Format8_8_8_8:
        switch (number_type) {
        case AmdGpu::NumberFormat::Unorm:
        case AmdGpu::NumberFormat::Srgb: // Should we handle gamma correction here?
            color.float32[0] = NumberUtils::U8ToUnorm(c0 & 0xff);
            color.float32[1] = NumberUtils::U8ToUnorm((c0 >> 8) & 0xff);
            color.float32[2] = NumberUtils::U8ToUnorm((c0 >> 16) & 0xff);
            color.float32[3] = NumberUtils::U8ToUnorm(c0 >> 24);
            break;
        case AmdGpu::NumberFormat::Snorm:
        case AmdGpu::NumberFormat::SnormNz:
            color.float32[0] = NumberUtils::S8ToSnorm(c0 & 0xff);
            color.float32[1] = NumberUtils::S8ToSnorm((c0 >> 8) & 0xff);
            color.float32[2] = NumberUtils::S8ToSnorm((c0 >> 16) & 0xff);
            color.float32[3] = NumberUtils::S8ToSnorm(c0 >> 24);
            break;
        case AmdGpu::NumberFormat::Uint:
        case AmdGpu::NumberFormat::Sint:
            color.uint32[0] = c0 & 0xff;
            color.uint32[1] = (c0 >> 8) & 0xff;
            color.uint32[2] = (c0 >> 16) & 0xff;
            color.uint32[3] = c0 >> 24;
            break;
        default:
            INVALID_NUMBER_FORMAT_COMBO;
            break;
        }
        break;
    case AmdGpu::DataFormat::Format32_32:
        switch (number_type) {
        case AmdGpu::NumberFormat::Uint:
        case AmdGpu::NumberFormat::Sint:
            color.uint32[0] = c0;
            color.uint32[1] = c1;
            break;
        case AmdGpu::NumberFormat::Float:
            color.float32[0] = *(reinterpret_cast<const float*>(&c0));
            color.float32[1] = *(reinterpret_cast<const float*>(&c1));
            break;
        default:
            INVALID_NUMBER_FORMAT_COMBO;
            break;
        }
        break;
    case AmdGpu::DataFormat::Format16_16_16_16:
        switch (number_type) {
        case AmdGpu::NumberFormat::Unorm:
            color.float32[0] = NumberUtils::U16ToUnorm(c0 & 0xffff);
            color.float32[1] = NumberUtils::U16ToUnorm((c0 >> 16) & 0xffff);
            color.float32[2] = NumberUtils::U16ToUnorm(c1 & 0xffff);
            color.float32[3] = NumberUtils::U16ToUnorm((c1 >> 16) & 0xffff);
            break;
        case AmdGpu::NumberFormat::Snorm:
        case AmdGpu::NumberFormat::SnormNz:
            color.float32[0] = NumberUtils::S16ToSnorm(c0 & 0xffff);
            color.float32[1] = NumberUtils::S16ToSnorm((c0 >> 16) & 0xffff);
            color.float32[2] = NumberUtils::S16ToSnorm(c1 & 0xffff);
            color.float32[3] = NumberUtils::S16ToSnorm((c1 >> 16) & 0xffff);
            break;
        case AmdGpu::NumberFormat::Uint:
        case AmdGpu::NumberFormat::Sint:
            color.uint32[0] = c0 & 0xffff;
            color.uint32[1] = (c0 >> 16) & 0xffff;
            color.uint32[2] = c1 & 0xffff;
            color.uint32[3] = (c1 >> 16) & 0xffff;
            break;
        case AmdGpu::NumberFormat::Float:
            color.float32[0] = NumberUtils::Uf16ToF32(c0 & 0xffff);
            color.float32[1] = NumberUtils::Uf16ToF32((c0 >> 16) & 0xffff);
            color.float32[2] = NumberUtils::Uf16ToF32(c1 & 0xffff);
            color.float32[3] = NumberUtils::Uf16ToF32((c1 >> 16) & 0xffff);
            break;
        default:
            INVALID_NUMBER_FORMAT_COMBO;
            break;
        }
        break;
    case AmdGpu::DataFormat::Format32_32_32_32:
        switch (number_type) {
        case AmdGpu::NumberFormat::Uint:
        case AmdGpu::NumberFormat::Sint:
            color.uint32[0] = c0;
            color.uint32[1] = c0;
            color.uint32[2] = c0;
            color.uint32[3] = c1;
            break;
        case AmdGpu::NumberFormat::Float:
            color.float32[0] = *(reinterpret_cast<const float*>(&c0));
            color.float32[1] = *(reinterpret_cast<const float*>(&c0));
            color.float32[2] = *(reinterpret_cast<const float*>(&c0));
            color.float32[3] = *(reinterpret_cast<const float*>(&c1));
            break;
        default:
            INVALID_NUMBER_FORMAT_COMBO;
            break;
        }
        break;
    case AmdGpu::DataFormat::Format5_6_5:
        color.float32[0] = NumberUtils::U5ToUnorm(c0 & 0x1f);
        color.float32[1] = NumberUtils::U6ToUnorm((c0 >> 5) & 0x3f);
        color.float32[2] = NumberUtils::U5ToUnorm(c0 >> 11);
        break;
    case AmdGpu::DataFormat::Format1_5_5_5:
        color.float32[0] = NumberUtils::U5ToUnorm(c0 & 0x1f);
        color.float32[1] = NumberUtils::U5ToUnorm((c0 >> 5) & 0x1f);
        color.float32[2] = NumberUtils::U5ToUnorm((c0 >> 10) & 0x1f);
        color.float32[3] = (c0 >> 15) ? 1.0f : 0.0f;
        break;
    case AmdGpu::DataFormat::Format5_5_5_1:
        color.float32[0] = (c0 & 0x1) ? 1.0f : 0.0f;
        color.float32[1] = NumberUtils::U5ToUnorm((c0 >> 1) & 0x1f);
        color.float32[2] = NumberUtils::U5ToUnorm((c0 >> 6) & 0x1f);
        color.float32[3] = NumberUtils::U5ToUnorm((c0 >> 11) & 0x1f);
        break;
    case AmdGpu::DataFormat::Format4_4_4_4:
        color.float32[0] = NumberUtils::U4ToUnorm(c0 & 0xf);
        color.float32[1] = NumberUtils::U4ToUnorm((c0 >> 4) & 0xf);
        color.float32[2] = NumberUtils::U4ToUnorm((c0 >> 8) & 0xf);
        color.float32[3] = NumberUtils::U4ToUnorm(c0 >> 12);
        break;
    default:
        LOG_ERROR(Render_Vulkan, "Unsupported color buffer format: %.*s",
                  static_cast<int>(AmdGpu::NameOf(format).size()), AmdGpu::NameOf(format).data());
        break;
    }

    comp_swizzle.Apply(color.float32);
    VkClearValue value{};
    value.color = color;
    return value;
}

VkSampleCountFlagBits RawNumSamples(u32 num_samples) {
    switch (num_samples) {
    case 1:
        return VK_SAMPLE_COUNT_1_BIT;
    case 2:
        return VK_SAMPLE_COUNT_2_BIT;
    case 4:
        return VK_SAMPLE_COUNT_4_BIT;
    case 8:
        return VK_SAMPLE_COUNT_8_BIT;
    case 16:
        return VK_SAMPLE_COUNT_16_BIT;
    default:
        UNREACHABLE();
    }
}

VkSampleCountFlagBits NumSamples(u32 num_samples, VkSampleCountFlags supported_flags) {
    VkSampleCountFlagBits flags = RawNumSamples(num_samples);
    // Half sample counts until supported, with a minimum of 1.
    while (!(supported_flags & flags) && num_samples > 1) {
        num_samples /= 2;
        flags = RawNumSamples(num_samples);
    }
    return flags;
}

} // namespace Vulkan::LiverpoolToVK
