// SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

// Glue: ovl's frames onto Vulkan. The pipeline and its state are ImGui's Vulkan backend's
// (imgui_impl_vulkan.cpp), so what ovl draws comes out as ImGui drew it: the same vertex and
// fragment shaders, blending, linear repeating sampler, and scissors.

#include <algorithm>
#include <array>
#include <cfloat>
#include <chrono>
#include <cstring>
#include <vector>

#include "common/assert.h"
#include "common/embedded_resources.h"
#include "core/debug_state.h"
#include "core/emulator_settings.h"
#include "video_core/host_shaders/host_spirv.h"
#include "video_core/overlay/ovl.h"
#include "video_core/overlay/ovl_layers.h"
#include "video_core/overlay/vk_overlay.h"
#include "video_core/renderer_vulkan/vk_instance.h"
#include "video_core/renderer_vulkan/vk_platform.h"

namespace Overlay {

namespace {

// What a frame can upload of the atlas: new glyphs a frame are a few kilobytes; more waits a
// frame
constexpr vk::DeviceSize StagingBytes = 2u << 20;
// A frame's vertices and indices, room for every overlay at once; more grows them
constexpr vk::DeviceSize VertexBytes = 65536 * sizeof(ovl_vertex_t);
constexpr vk::DeviceSize IndexBytes = 98304 * sizeof(u32);

struct HostBuffer {
    vk::Buffer buffer{};
    vk::DeviceMemory memory{};
    vk::DeviceSize size{};
    u8* map{};
};

// One for each swapchain image: a slot comes round again once the frame that used it is done
struct Slot {
    HostBuffer staging;
    HostBuffer vertex;
    HostBuffer index;
};

struct State {
    vk::Device device;
    vk::PhysicalDeviceMemoryProperties memory_properties;
    vk::Image atlas;
    vk::DeviceMemory atlas_memory;
    vk::ImageView atlas_view;
    bool atlas_sampled = false; // in the shader read layout, after its first upload
    vk::Sampler sampler;
    vk::DescriptorPool descriptor_pool;
    vk::DescriptorSetLayout set_layout;
    vk::DescriptorSet atlas_set;
    vk::PipelineLayout pipeline_layout;
    vk::Pipeline pipeline;
    std::vector<Slot> slots;
    u32 slot = 0;
    const ovl_draw_data_t* draw_data = nullptr;

    // The game's frame rate, over its last 60 frames
    std::chrono::steady_clock::time_point last_frame_time{};
    std::chrono::steady_clock::time_point last_game_frame_time{};
    std::array<float, 60> game_frame_seconds{};
    float game_frame_seconds_sum = 0.0f;
    std::size_t game_frame_index = 0;
};

State* state = nullptr;

u32 FindMemoryType(vk::MemoryPropertyFlags properties, u32 type_bits) {
    const auto& props = state->memory_properties;
    for (u32 i = 0; i < props.memoryTypeCount; i++) {
        if ((props.memoryTypes[i].propertyFlags & properties) == properties &&
            (type_bits & (1u << i))) {
            return i;
        }
    }
    UNREACHABLE_MSG("No memory type for the overlay");
}

HostBuffer MakeHostBuffer(vk::DeviceSize size, vk::BufferUsageFlags usage) {
    const vk::Device device = state->device;
    HostBuffer b{};
    b.buffer = Vulkan::Check<"create overlay buffer">(device.createBuffer({
        .size = size,
        .usage = usage,
        .sharingMode = vk::SharingMode::eExclusive,
    }));
    const vk::MemoryRequirements req = device.getBufferMemoryRequirements(b.buffer);
    b.memory = Vulkan::Check<"allocate overlay buffer">(device.allocateMemory({
        .allocationSize = req.size,
        .memoryTypeIndex = FindMemoryType(vk::MemoryPropertyFlagBits::eHostVisible |
                                              vk::MemoryPropertyFlagBits::eHostCoherent,
                                          req.memoryTypeBits),
    }));
    Vulkan::Check(device.bindBufferMemory(b.buffer, b.memory, 0));
    b.map = static_cast<u8*>(Vulkan::Check(device.mapMemory(b.memory, 0, VK_WHOLE_SIZE)));
    b.size = size;
    return b;
}

void FreeHostBuffer(HostBuffer& b) {
    if (b.buffer) {
        state->device.destroyBuffer(b.buffer);
        state->device.freeMemory(b.memory);
    }
    b = {};
}

vk::ShaderModule MakeShader(size_t (*make)(uint32_t*, size_t, float), float arg) {
    std::array<u32, HOST_SPIRV_MAX_WORDS> words;
    const size_t n = make(words.data(), words.size(), arg);
    ASSERT(n != 0);
    return Vulkan::Check<"create overlay shader">(state->device.createShaderModule({
        .codeSize = n * sizeof(u32),
        .pCode = words.data(),
    }));
}

void MakePipeline(vk::Format surface_format) {
    const vk::Device device = state->device;
    const vk::ShaderModule vert = MakeShader(
        [](uint32_t* out, size_t cap, float) { return host_spirv_overlay_vert(out, cap); }, 0.0f);
    // HDR Output: every overlay as PQ, as ImGui's backend drew them
    const float white = emu_settings_get_hdr_allowed()
                            ? static_cast<float>(emu_settings_get_hdr_paper_white()) / 10000.0f
                            : 0.0f;
    const vk::ShaderModule frag = MakeShader(host_spirv_overlay_frag, white);

    const vk::PipelineShaderStageCreateInfo stages[2]{
        {.stage = vk::ShaderStageFlagBits::eVertex, .module = vert, .pName = "main"},
        {.stage = vk::ShaderStageFlagBits::eFragment, .module = frag, .pName = "main"},
    };
    const vk::VertexInputBindingDescription binding{
        .stride = sizeof(ovl_vertex_t),
        .inputRate = vk::VertexInputRate::eVertex,
    };
    const vk::VertexInputAttributeDescription attributes[3]{
        {.location = 0, .format = vk::Format::eR32G32Sfloat, .offset = offsetof(ovl_vertex_t, x)},
        {.location = 1, .format = vk::Format::eR32G32Sfloat, .offset = offsetof(ovl_vertex_t, u)},
        {.location = 2,
         .format = vk::Format::eR8G8B8A8Unorm,
         .offset = offsetof(ovl_vertex_t, col)},
    };
    const vk::PipelineVertexInputStateCreateInfo vertex_info{
        .vertexBindingDescriptionCount = 1,
        .pVertexBindingDescriptions = &binding,
        .vertexAttributeDescriptionCount = 3,
        .pVertexAttributeDescriptions = attributes,
    };
    const vk::PipelineInputAssemblyStateCreateInfo ia_info{
        .topology = vk::PrimitiveTopology::eTriangleList,
    };
    const vk::PipelineViewportStateCreateInfo viewport_info{
        .viewportCount = 1,
        .scissorCount = 1,
    };
    const vk::PipelineRasterizationStateCreateInfo raster_info{
        .polygonMode = vk::PolygonMode::eFill,
        .cullMode = vk::CullModeFlagBits::eNone,
        .frontFace = vk::FrontFace::eCounterClockwise,
        .lineWidth = 1.0f,
    };
    const vk::PipelineMultisampleStateCreateInfo ms_info{
        .rasterizationSamples = vk::SampleCountFlagBits::e1,
    };
    const vk::PipelineColorBlendAttachmentState blend{
        .blendEnable = VK_TRUE,
        .srcColorBlendFactor = vk::BlendFactor::eSrcAlpha,
        .dstColorBlendFactor = vk::BlendFactor::eOneMinusSrcAlpha,
        .colorBlendOp = vk::BlendOp::eAdd,
        .srcAlphaBlendFactor = vk::BlendFactor::eOne,
        .dstAlphaBlendFactor = vk::BlendFactor::eOneMinusSrcAlpha,
        .alphaBlendOp = vk::BlendOp::eAdd,
        .colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
                          vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA,
    };
    const vk::PipelineDepthStencilStateCreateInfo depth_info{};
    const vk::PipelineColorBlendStateCreateInfo blend_info{
        .attachmentCount = 1,
        .pAttachments = &blend,
    };
    const vk::DynamicState dynamic_states[2]{vk::DynamicState::eViewport,
                                             vk::DynamicState::eScissor};
    const vk::PipelineDynamicStateCreateInfo dynamic_info{
        .dynamicStateCount = 2,
        .pDynamicStates = dynamic_states,
    };
    const vk::PipelineRenderingCreateInfo rendering_info{
        .colorAttachmentCount = 1,
        .pColorAttachmentFormats = &surface_format,
    };
    state->pipeline = Vulkan::Check<"create overlay pipeline">(
        device.createGraphicsPipeline({}, {
                                              .pNext = &rendering_info,
                                              .stageCount = 2,
                                              .pStages = stages,
                                              .pVertexInputState = &vertex_info,
                                              .pInputAssemblyState = &ia_info,
                                              .pViewportState = &viewport_info,
                                              .pRasterizationState = &raster_info,
                                              .pMultisampleState = &ms_info,
                                              .pDepthStencilState = &depth_info,
                                              .pColorBlendState = &blend_info,
                                              .pDynamicState = &dynamic_info,
                                              .layout = state->pipeline_layout,
                                          }));
    device.destroyShaderModule(vert);
    device.destroyShaderModule(frag);
}

void MakeAtlas() {
    const vk::Device device = state->device;
    // Coverage and the placed mask, sampled as (G, G, G, R): ImGui's white RGBA atlas
    state->atlas = Vulkan::Check<"create overlay atlas">(device.createImage({
        .imageType = vk::ImageType::e2D,
        .format = vk::Format::eR8G8Unorm,
        .extent = {OVL_ATLAS_SIZE, OVL_ATLAS_SIZE, 1},
        .mipLevels = 1,
        .arrayLayers = 1,
        .samples = vk::SampleCountFlagBits::e1,
        .tiling = vk::ImageTiling::eOptimal,
        .usage = vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst,
        .sharingMode = vk::SharingMode::eExclusive,
        .initialLayout = vk::ImageLayout::eUndefined,
    }));
    const vk::MemoryRequirements req = device.getImageMemoryRequirements(state->atlas);
    state->atlas_memory = Vulkan::Check<"allocate overlay atlas">(device.allocateMemory({
        .allocationSize = req.size,
        .memoryTypeIndex =
            FindMemoryType(vk::MemoryPropertyFlagBits::eDeviceLocal, req.memoryTypeBits),
    }));
    Vulkan::Check(device.bindImageMemory(state->atlas, state->atlas_memory, 0));
    state->atlas_view = Vulkan::Check<"create overlay atlas view">(device.createImageView({
        .image = state->atlas,
        .viewType = vk::ImageViewType::e2D,
        .format = vk::Format::eR8G8Unorm,
        .components = {vk::ComponentSwizzle::eG, vk::ComponentSwizzle::eG, vk::ComponentSwizzle::eG,
                       vk::ComponentSwizzle::eR},
        .subresourceRange{
            .aspectMask = vk::ImageAspectFlagBits::eColor,
            .levelCount = 1,
            .layerCount = 1,
        },
    }));
}

void MakeDescriptors() {
    const vk::Device device = state->device;
    state->sampler = Vulkan::Check<"create overlay sampler">(device.createSampler({
        .magFilter = vk::Filter::eLinear,
        .minFilter = vk::Filter::eLinear,
        .mipmapMode = vk::SamplerMipmapMode::eLinear,
        .addressModeU = vk::SamplerAddressMode::eRepeat,
        .addressModeV = vk::SamplerAddressMode::eRepeat,
        .addressModeW = vk::SamplerAddressMode::eRepeat,
        .maxAnisotropy = 1.0f,
        .minLod = -1000,
        .maxLod = 1000,
    }));
    const vk::DescriptorPoolSize pool_size{
        .type = vk::DescriptorType::eCombinedImageSampler,
        .descriptorCount = 64,
    };
    state->descriptor_pool = Vulkan::Check(device.createDescriptorPool({
        .flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet,
        .maxSets = 64,
        .poolSizeCount = 1,
        .pPoolSizes = &pool_size,
    }));
    const vk::DescriptorSetLayoutBinding binding{
        .descriptorType = vk::DescriptorType::eCombinedImageSampler,
        .descriptorCount = 1,
        .stageFlags = vk::ShaderStageFlagBits::eFragment,
    };
    state->set_layout = Vulkan::Check(device.createDescriptorSetLayout({
        .bindingCount = 1,
        .pBindings = &binding,
    }));
    const vk::PushConstantRange push_constants{
        .stageFlags = vk::ShaderStageFlagBits::eVertex,
        .offset = 0,
        .size = sizeof(float) * 4,
    };
    state->pipeline_layout = Vulkan::Check(device.createPipelineLayout({
        .setLayoutCount = 1,
        .pSetLayouts = &state->set_layout,
        .pushConstantRangeCount = 1,
        .pPushConstantRanges = &push_constants,
    }));
    state->atlas_set = Vulkan::Check(device.allocateDescriptorSets({
                                         .descriptorPool = state->descriptor_pool,
                                         .descriptorSetCount = 1,
                                         .pSetLayouts = &state->set_layout,
                                     }))
                           .front();
    const vk::DescriptorImageInfo image_info{
        .sampler = state->sampler,
        .imageView = state->atlas_view,
        .imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal,
    };
    device.updateDescriptorSets(
        vk::WriteDescriptorSet{
            .dstSet = state->atlas_set,
            .descriptorCount = 1,
            .descriptorType = vk::DescriptorType::eCombinedImageSampler,
            .pImageInfo = &image_info,
        },
        {});
}

constexpr vk::ImageSubresourceRange ColorRange{
    .aspectMask = vk::ImageAspectFlagBits::eColor,
    .levelCount = 1,
    .layerCount = 1,
};

// What changed of the atlas since the last frame, into the image: cleared when the atlas
// started again, then the rectangle written, as much of it as the slot's staging holds
void UploadAtlas(vk::CommandBuffer cmdbuf, Slot& slot) {
    int rect[4];
    int cleared = 0;
    if (!ovl_atlas_take_dirty(rect, &cleared)) {
        return;
    }
    const vk::ImageMemoryBarrier to_transfer{
        .srcAccessMask = vk::AccessFlagBits::eShaderRead,
        .dstAccessMask = vk::AccessFlagBits::eTransferWrite,
        .oldLayout = (cleared || !state->atlas_sampled) ? vk::ImageLayout::eUndefined
                                                        : vk::ImageLayout::eShaderReadOnlyOptimal,
        .newLayout = vk::ImageLayout::eTransferDstOptimal,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = state->atlas,
        .subresourceRange = ColorRange,
    };
    cmdbuf.pipelineBarrier(vk::PipelineStageFlagBits::eFragmentShader,
                           vk::PipelineStageFlagBits::eTransfer, {}, {}, {}, to_transfer);
    if (cleared || !state->atlas_sampled) {
        const vk::ClearColorValue zero{std::array<float, 4>{0.0f, 0.0f, 0.0f, 0.0f}};
        cmdbuf.clearColorImage(state->atlas, vk::ImageLayout::eTransferDstOptimal, zero,
                               ColorRange);
        if (rect[2] > 0) {
            const vk::ImageMemoryBarrier after_clear{
                .srcAccessMask = vk::AccessFlagBits::eTransferWrite,
                .dstAccessMask = vk::AccessFlagBits::eTransferWrite,
                .oldLayout = vk::ImageLayout::eTransferDstOptimal,
                .newLayout = vk::ImageLayout::eTransferDstOptimal,
                .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .image = state->atlas,
                .subresourceRange = ColorRange,
            };
            cmdbuf.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer,
                                   vk::PipelineStageFlagBits::eTransfer, {}, {}, {}, after_clear);
        }
    }
    if (rect[2] > 0 && rect[3] > 0) {
        const size_t pitch = static_cast<size_t>(rect[2]) * 2;
        const int rows = std::min(rect[3], static_cast<int>(StagingBytes / pitch));
        if (rows < rect[3]) {
            ovl_atlas_mark_dirty(rect[0], rect[1] + rows, rect[2], rect[3] - rows);
        }
        const u8* pixels = ovl_atlas_pixels();
        for (int y = 0; y < rows; y++) {
            std::memcpy(slot.staging.map + pitch * y,
                        pixels + (static_cast<size_t>(rect[1] + y) * OVL_ATLAS_SIZE + rect[0]) * 2,
                        pitch);
        }
        const vk::BufferImageCopy region{
            .imageSubresource{
                .aspectMask = vk::ImageAspectFlagBits::eColor,
                .layerCount = 1,
            },
            .imageOffset = {rect[0], rect[1], 0},
            .imageExtent = {static_cast<u32>(rect[2]), static_cast<u32>(rows), 1},
        };
        cmdbuf.copyBufferToImage(slot.staging.buffer, state->atlas,
                                 vk::ImageLayout::eTransferDstOptimal, region);
    }
    const vk::ImageMemoryBarrier to_sampled{
        .srcAccessMask = vk::AccessFlagBits::eTransferWrite,
        .dstAccessMask = vk::AccessFlagBits::eShaderRead,
        .oldLayout = vk::ImageLayout::eTransferDstOptimal,
        .newLayout = vk::ImageLayout::eShaderReadOnlyOptimal,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = state->atlas,
        .subresourceRange = ColorRange,
    };
    cmdbuf.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer,
                           vk::PipelineStageFlagBits::eFragmentShader, {}, {}, {}, to_sampled);
    state->atlas_sampled = true;
}

// A frame's vertices and indices, into the slot's buffers, grown if the frame needs more
void UploadGeometry(Slot& slot, const ovl_draw_data_t& dd) {
    const vk::DeviceSize vtx_bytes = dd.vtx_count * sizeof(ovl_vertex_t);
    const vk::DeviceSize idx_bytes = dd.idx_count * sizeof(u32);
    if (vtx_bytes > slot.vertex.size) {
        FreeHostBuffer(slot.vertex);
        slot.vertex = MakeHostBuffer(vtx_bytes * 2, vk::BufferUsageFlagBits::eVertexBuffer);
    }
    if (idx_bytes > slot.index.size) {
        FreeHostBuffer(slot.index);
        slot.index = MakeHostBuffer(idx_bytes * 2, vk::BufferUsageFlagBits::eIndexBuffer);
    }
    std::memcpy(slot.vertex.map, dd.vtx, vtx_bytes);
    std::memcpy(slot.index.map, dd.idx, idx_bytes);
}

} // namespace

void Initialize(const Vulkan::Instance& instance, u32 image_count, vk::Format surface_format) {
    if (state) {
        return;
    }
    // The UI font, read in place from what the core carries, at ImGui's size: 32 pixels drawn
    // at half (FontGlobalScale)
    const auto noto = Common::GetEmbeddedResource("src/imgui/renderer/fonts/NotoSans-Regular.ttf");
    ASSERT_MSG(!noto.empty(), "The overlay's font is not in the core");
    const ovl_face_desc_t faces[1]{{noto.data(), noto.size(), 0}};
    ovl_font_t* font = ovl_font_add(faces, 1, 32.0f);
    ASSERT_MSG(font && ovl_create(font, 0.5f), "The overlay could not be made");

    state = new State{};
    state->device = instance.GetDevice();
    state->memory_properties = instance.GetPhysicalDevice().getMemoryProperties();
    MakeAtlas();
    MakeDescriptors();
    MakePipeline(surface_format);
    state->slots.resize(image_count);
    for (Slot& slot : state->slots) {
        slot.staging = MakeHostBuffer(StagingBytes, vk::BufferUsageFlagBits::eTransferSrc);
        slot.vertex = MakeHostBuffer(VertexBytes, vk::BufferUsageFlagBits::eVertexBuffer);
        slot.index = MakeHostBuffer(IndexBytes, vk::BufferUsageFlagBits::eIndexBuffer);
    }
}

void Shutdown(vk::Device device) {
    if (!state) {
        return;
    }
    for (Slot& slot : state->slots) {
        FreeHostBuffer(slot.staging);
        FreeHostBuffer(slot.vertex);
        FreeHostBuffer(slot.index);
    }
    device.destroyPipeline(state->pipeline);
    device.destroyPipelineLayout(state->pipeline_layout);
    device.destroyDescriptorSetLayout(state->set_layout);
    device.destroyDescriptorPool(state->descriptor_pool);
    device.destroySampler(state->sampler);
    device.destroyImageView(state->atlas_view);
    device.destroyImage(state->atlas);
    device.freeMemory(state->atlas_memory);
    delete state;
    state = nullptr;
    ovl_destroy();
}

void NewFrame(u32 width, u32 height, bool is_reusing_frame) {
    using namespace std::chrono;
    if (!state) {
        return;
    }
    State& s = *state;
    const auto now = steady_clock::now();
    const auto seconds = [](steady_clock::duration d) {
        return std::max(duration<float>(d).count(), FLT_MIN);
    };
    const float delta = s.last_frame_time.time_since_epoch().count() != 0
                            ? seconds(now - s.last_frame_time)
                            : 1.0f / 60.0f;
    s.last_frame_time = now;
    if (!is_reusing_frame) {
        const float game_delta = s.last_game_frame_time.time_since_epoch().count() != 0
                                     ? seconds(now - s.last_game_frame_time)
                                     : 1.0f / 60.0f;
        s.last_game_frame_time = now;
        DebugState.FrameDeltaTime = game_delta;
        float& slot = s.game_frame_seconds[s.game_frame_index];
        s.game_frame_seconds_sum += game_delta - slot;
        slot = game_delta;
        s.game_frame_index = (s.game_frame_index + 1) % s.game_frame_seconds.size();
        DebugState.Framerate = s.game_frame_seconds_sum > 0.0f
                                   ? 1.0f / (s.game_frame_seconds_sum /
                                             static_cast<float>(s.game_frame_seconds.size()))
                                   : FLT_MAX;
    }

    ovl_new_frame(static_cast<float>(width), static_cast<float>(height), delta);
    ovl_fps_draw(DebugState.Framerate);
    s.draw_data = ovl_render();
}

void Render(vk::CommandBuffer cmdbuf, vk::ImageView image_view, vk::Extent2D extent) {
    if (!state || !state->draw_data || state->draw_data->idx_count == 0) {
        return;
    }
    State& s = *state;
    const ovl_draw_data_t& dd = *s.draw_data;
    const float fb_w = static_cast<float>(extent.width);
    const float fb_h = static_cast<float>(extent.height);
    if (fb_w <= 0.0f || fb_h <= 0.0f) {
        return;
    }
    s.slot = (s.slot + 1) % static_cast<u32>(s.slots.size());
    Slot& slot = s.slots[s.slot];

    if (emu_settings_get_vkhost_markers()) {
        cmdbuf.beginDebugUtilsLabelEXT(vk::DebugUtilsLabelEXT{.pLabelName = "Overlay Render"});
    }
    // Atlas uploads go ahead of the pass that samples them
    UploadAtlas(cmdbuf, slot);
    UploadGeometry(slot, dd);

    const vk::RenderingAttachmentInfo color_attachment{
        .imageView = image_view,
        .imageLayout = vk::ImageLayout::eColorAttachmentOptimal,
        .loadOp = vk::AttachmentLoadOp::eLoad,
        .storeOp = vk::AttachmentStoreOp::eStore,
    };
    cmdbuf.beginRendering(vk::RenderingInfo{
        .renderArea = {{0, 0}, extent},
        .layerCount = 1,
        .colorAttachmentCount = 1,
        .pColorAttachments = &color_attachment,
    });
    cmdbuf.bindPipeline(vk::PipelineBindPoint::eGraphics, s.pipeline);
    cmdbuf.bindVertexBuffers(0, slot.vertex.buffer, vk::DeviceSize{0});
    cmdbuf.bindIndexBuffer(slot.index.buffer, 0, vk::IndexType::eUint32);
    cmdbuf.setViewport(0, vk::Viewport{0.0f, 0.0f, fb_w, fb_h, 0.0f, 1.0f});
    // The display's pixels to clip space, as ImGui's backend
    const float scale[2]{2.0f / dd.display_w, 2.0f / dd.display_h};
    const float translate[2]{-1.0f, -1.0f};
    cmdbuf.pushConstants(s.pipeline_layout, vk::ShaderStageFlagBits::eVertex, 0, sizeof(scale),
                         scale);
    cmdbuf.pushConstants(s.pipeline_layout, vk::ShaderStageFlagBits::eVertex, sizeof(scale),
                         sizeof(translate), translate);
    vk::DescriptorSet bound{};
    for (u32 i = 0; i < dd.cmd_count; i++) {
        const ovl_cmd_t& cmd = dd.cmds[i];
        // Within the image, as vkCmdSetScissor requires
        const float x0 = std::max(cmd.clip[0], 0.0f);
        const float y0 = std::max(cmd.clip[1], 0.0f);
        const float x1 = std::min(cmd.clip[2], fb_w);
        const float y1 = std::min(cmd.clip[3], fb_h);
        if (x1 <= x0 || y1 <= y0 || cmd.idx_count == 0) {
            continue;
        }
        cmdbuf.setScissor(0, vk::Rect2D{{static_cast<s32>(x0), static_cast<s32>(y0)},
                                        {static_cast<u32>(x1 - x0), static_cast<u32>(y1 - y0)}});
        const vk::DescriptorSet set =
            cmd.tex == OVL_TEX_ATLAS ? s.atlas_set : vk::DescriptorSet{(VkDescriptorSet)(cmd.tex)};
        if (set != bound) {
            cmdbuf.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, s.pipeline_layout, 0, set,
                                      {});
            bound = set;
        }
        cmdbuf.drawIndexed(cmd.idx_count, 1, cmd.idx_offset, 0, 0);
    }
    cmdbuf.endRendering();
    if (emu_settings_get_vkhost_markers()) {
        cmdbuf.endDebugUtilsLabelEXT();
    }
}

} // namespace Overlay
