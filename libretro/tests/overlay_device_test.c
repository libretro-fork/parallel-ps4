/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

/* The overlays drawn on a device as the core draws them
 * (video_core/overlay/vk_overlay.cpp): ovl's FPS counter through
 * host_spirv_overlay_vert/frag, the atlas as R8G8 sampled through a (G, G, G,
 * R) view with a linear repeating sampler, ImGui's blending and scissors. What
 * comes out is checked against the same frame drawn here on the CPU as a GPU
 * draws it (pixel centres, top-left rule, bilinear texels, source-alpha
 * blending), which ovl_test and the ImGui comparison it was made with tie to
 * ImGui's look. Skipped (exit 0) with no device.
 *
 *   VK_ICD_FILENAMES=/usr/share/vulkan/icd.d/lvp_icd.json \
 *       build-tests/overlay_device_test
 * src/imgui/renderer/fonts/NotoSans-Regular.ttf */

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <dlfcn.h>

#ifndef VK_NO_PROTOTYPES
#define VK_NO_PROTOTYPES
#endif
#include <vulkan/vulkan.h>

#include "../../src/video_core/host_shaders/host_spirv.h"
#include "../../src/video_core/overlay/ovl.h"

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
#define FN(name) static PFN_##name p_##name
FN(vkCreateInstance);
FN(vkEnumeratePhysicalDevices);
FN(vkGetPhysicalDeviceMemoryProperties);
FN(vkCreateDevice);
FN(vkGetDeviceQueue);
FN(vkAllocateMemory);
FN(vkMapMemory);
FN(vkCreateBuffer);
FN(vkGetBufferMemoryRequirements);
FN(vkBindBufferMemory);
FN(vkCreateImage);
FN(vkGetImageMemoryRequirements);
FN(vkBindImageMemory);
FN(vkCreateImageView);
FN(vkCreateRenderPass);
FN(vkCreateFramebuffer);
FN(vkCreateShaderModule);
FN(vkCreateDescriptorSetLayout);
FN(vkCreatePipelineLayout);
FN(vkCreateGraphicsPipelines);
FN(vkCreateDescriptorPool);
FN(vkAllocateDescriptorSets);
FN(vkUpdateDescriptorSets);
FN(vkCreateCommandPool);
FN(vkAllocateCommandBuffers);
FN(vkBeginCommandBuffer);
FN(vkEndCommandBuffer);
FN(vkCmdBeginRenderPass);
FN(vkCmdEndRenderPass);
FN(vkCmdBindPipeline);
FN(vkCmdBindDescriptorSets);
FN(vkCmdBindVertexBuffers);
FN(vkCmdBindIndexBuffer);
FN(vkCmdDrawIndexed);
FN(vkCmdSetScissor);
FN(vkCmdSetViewport);
FN(vkCmdCopyImageToBuffer);
FN(vkCmdCopyBufferToImage);
FN(vkCmdPipelineBarrier);
FN(vkCmdPushConstants);
FN(vkCreateSampler);
FN(vkQueueSubmit);
FN(vkQueueWaitIdle);

#define W 256
#define H 64

static VkDevice device;
static VkQueue queue;
static VkCommandBuffer cb;
static VkPhysicalDeviceMemoryProperties mem;

static VkDeviceMemory alloc(const VkMemoryRequirements *req,
                            VkMemoryPropertyFlags want) {
  VkMemoryAllocateInfo ai;
  VkDeviceMemory memory = VK_NULL_HANDLE;
  uint32_t i;
  memset(&ai, 0, sizeof(ai));
  ai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
  ai.allocationSize = req->size;
  for (i = 0; i < mem.memoryTypeCount; i++)
    if ((req->memoryTypeBits & (1u << i)) &&
        (mem.memoryTypes[i].propertyFlags & want) == want)
      break;
  ai.memoryTypeIndex = i;
  if (i == mem.memoryTypeCount ||
      p_vkAllocateMemory(device, &ai, NULL, &memory) != VK_SUCCESS)
    return VK_NULL_HANDLE;
  return memory;
}

/* A buffer the host writes or reads, mapped */
static VkBuffer host_buffer(VkDeviceSize size, VkBufferUsageFlags usage,
                            void **map) {
  VkBufferCreateInfo bi;
  VkMemoryRequirements req;
  VkDeviceMemory memory;
  VkBuffer buffer = VK_NULL_HANDLE;
  memset(&bi, 0, sizeof(bi));
  bi.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  bi.size = size;
  bi.usage = usage;
  if (p_vkCreateBuffer(device, &bi, NULL, &buffer) != VK_SUCCESS)
    return VK_NULL_HANDLE;
  p_vkGetBufferMemoryRequirements(device, buffer, &req);
  memory = alloc(&req, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                           VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
  if (!memory)
    return VK_NULL_HANDLE;
  p_vkBindBufferMemory(device, buffer, memory, 0);
  p_vkMapMemory(device, memory, 0, VK_WHOLE_SIZE, 0, map);
  return buffer;
}

static VkImage make_image(VkFormat format, uint32_t w, uint32_t h,
                          VkImageUsageFlags usage) {
  VkImageCreateInfo ii;
  VkMemoryRequirements req;
  VkDeviceMemory memory;
  VkImage image;
  memset(&ii, 0, sizeof(ii));
  ii.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
  ii.imageType = VK_IMAGE_TYPE_2D;
  ii.format = format;
  ii.extent.width = w;
  ii.extent.height = h;
  ii.extent.depth = 1;
  ii.mipLevels = 1;
  ii.arrayLayers = 1;
  ii.samples = VK_SAMPLE_COUNT_1_BIT;
  ii.usage = usage;
  if (p_vkCreateImage(device, &ii, NULL, &image) != VK_SUCCESS)
    return VK_NULL_HANDLE;
  p_vkGetImageMemoryRequirements(device, image, &req);
  memory = alloc(&req, 0);
  if (!memory)
    return VK_NULL_HANDLE;
  p_vkBindImageMemory(device, image, memory, 0);
  return image;
}

static VkImageView make_view(VkImage image, VkFormat format, int swizzle) {
  VkImageViewCreateInfo vi;
  VkImageView view = VK_NULL_HANDLE;
  memset(&vi, 0, sizeof(vi));
  vi.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
  vi.image = image;
  vi.viewType = VK_IMAGE_VIEW_TYPE_2D;
  vi.format = format;
  if (swizzle) {
    vi.components.r = VK_COMPONENT_SWIZZLE_G;
    vi.components.g = VK_COMPONENT_SWIZZLE_G;
    vi.components.b = VK_COMPONENT_SWIZZLE_G;
    vi.components.a = VK_COMPONENT_SWIZZLE_R;
  }
  vi.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  vi.subresourceRange.levelCount = 1;
  vi.subresourceRange.layerCount = 1;
  p_vkCreateImageView(device, &vi, NULL, &view);
  return view;
}

static VkShaderModule make_module(const uint32_t *words, size_t n) {
  VkShaderModuleCreateInfo mi;
  VkShaderModule module = VK_NULL_HANDLE;
  memset(&mi, 0, sizeof(mi));
  mi.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
  mi.codeSize = n * 4;
  mi.pCode = words;
  p_vkCreateShaderModule(device, &mi, NULL, &module);
  return module;
}

/* A pass over the target, cleared, left for the copy back */
static VkRenderPass make_pass(void) {
  VkAttachmentDescription att;
  VkAttachmentReference ref;
  VkSubpassDescription sub;
  VkRenderPassCreateInfo ri;
  VkRenderPass pass = VK_NULL_HANDLE;
  memset(&att, 0, sizeof(att));
  att.format = VK_FORMAT_R8G8B8A8_UNORM;
  att.samples = VK_SAMPLE_COUNT_1_BIT;
  att.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
  att.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
  att.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  att.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  att.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  att.finalLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
  ref.attachment = 0;
  ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
  memset(&sub, 0, sizeof(sub));
  sub.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
  sub.colorAttachmentCount = 1;
  sub.pColorAttachments = &ref;
  memset(&ri, 0, sizeof(ri));
  ri.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
  ri.attachmentCount = 1;
  ri.pAttachments = &att;
  ri.subpassCount = 1;
  ri.pSubpasses = &sub;
  p_vkCreateRenderPass(device, &ri, NULL, &pass);
  return pass;
}

/* The core's pipeline (vk_overlay.cpp, MakePipeline), for a render pass */
static VkPipeline make_pipeline(VkRenderPass pass, VkPipelineLayout layout,
                                VkShaderModule vert, VkShaderModule frag) {
  VkPipelineShaderStageCreateInfo stages[2];
  VkVertexInputBindingDescription binding;
  VkVertexInputAttributeDescription attributes[3];
  VkPipelineVertexInputStateCreateInfo vertex_input;
  VkPipelineInputAssemblyStateCreateInfo assembly;
  VkPipelineViewportStateCreateInfo viewport_state;
  VkPipelineRasterizationStateCreateInfo raster;
  VkPipelineMultisampleStateCreateInfo multisample;
  VkPipelineDepthStencilStateCreateInfo depth;
  VkPipelineColorBlendAttachmentState blend_att;
  VkPipelineColorBlendStateCreateInfo blend;
  VkDynamicState dynamic_states[2];
  VkPipelineDynamicStateCreateInfo dynamic;
  VkGraphicsPipelineCreateInfo pi;
  VkPipeline pipeline = VK_NULL_HANDLE;
  memset(stages, 0, sizeof(stages));
  stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
  stages[0].module = vert;
  stages[0].pName = "main";
  stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
  stages[1].module = frag;
  stages[1].pName = "main";
  memset(&binding, 0, sizeof(binding));
  binding.stride = sizeof(ovl_vertex_t);
  binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
  memset(attributes, 0, sizeof(attributes));
  attributes[0].location = 0;
  attributes[0].format = VK_FORMAT_R32G32_SFLOAT;
  attributes[0].offset = 0;
  attributes[1].location = 1;
  attributes[1].format = VK_FORMAT_R32G32_SFLOAT;
  attributes[1].offset = 8;
  attributes[2].location = 2;
  attributes[2].format = VK_FORMAT_R8G8B8A8_UNORM;
  attributes[2].offset = 16;
  memset(&vertex_input, 0, sizeof(vertex_input));
  vertex_input.sType =
      VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
  vertex_input.vertexBindingDescriptionCount = 1;
  vertex_input.pVertexBindingDescriptions = &binding;
  vertex_input.vertexAttributeDescriptionCount = 3;
  vertex_input.pVertexAttributeDescriptions = attributes;
  memset(&assembly, 0, sizeof(assembly));
  assembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
  assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
  memset(&viewport_state, 0, sizeof(viewport_state));
  viewport_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
  viewport_state.viewportCount = 1;
  viewport_state.scissorCount = 1;
  memset(&raster, 0, sizeof(raster));
  raster.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
  raster.polygonMode = VK_POLYGON_MODE_FILL;
  raster.cullMode = VK_CULL_MODE_NONE;
  raster.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
  raster.lineWidth = 1.0f;
  memset(&multisample, 0, sizeof(multisample));
  multisample.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
  multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
  memset(&depth, 0, sizeof(depth));
  depth.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
  memset(&blend_att, 0, sizeof(blend_att));
  blend_att.blendEnable = VK_TRUE;
  blend_att.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
  blend_att.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
  blend_att.colorBlendOp = VK_BLEND_OP_ADD;
  blend_att.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
  blend_att.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
  blend_att.alphaBlendOp = VK_BLEND_OP_ADD;
  blend_att.colorWriteMask = 0xf;
  memset(&blend, 0, sizeof(blend));
  blend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
  blend.attachmentCount = 1;
  blend.pAttachments = &blend_att;
  dynamic_states[0] = VK_DYNAMIC_STATE_VIEWPORT;
  dynamic_states[1] = VK_DYNAMIC_STATE_SCISSOR;
  memset(&dynamic, 0, sizeof(dynamic));
  dynamic.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
  dynamic.dynamicStateCount = 2;
  dynamic.pDynamicStates = dynamic_states;
  memset(&pi, 0, sizeof(pi));
  pi.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
  pi.stageCount = 2;
  pi.pStages = stages;
  pi.pVertexInputState = &vertex_input;
  pi.pInputAssemblyState = &assembly;
  pi.pViewportState = &viewport_state;
  pi.pRasterizationState = &raster;
  pi.pMultisampleState = &multisample;
  pi.pDepthStencilState = &depth;
  pi.pColorBlendState = &blend;
  pi.pDynamicState = &dynamic;
  pi.layout = layout;
  pi.renderPass = pass;
  p_vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pi, NULL, &pipeline);
  return pipeline;
}

/* ------------------------------------------------------------------------------------------
 */
/* The same frame on the CPU, drawn as a GPU draws it */

static const float clear_colour[3] = {0.2f, 0.3f, 0.4f};

/* The atlas as sampled through the core's view: (G, G, G, R), repeating */
static void texel(const uint8_t *atlas, int x, int y, float *out) {
  const uint8_t *t;
  x &= OVL_ATLAS_SIZE - 1;
  y &= OVL_ATLAS_SIZE - 1;
  t = atlas + ((size_t)y * OVL_ATLAS_SIZE + (size_t)x) * 2;
  out[0] = out[1] = out[2] = t[1] / 255.0f;
  out[3] = t[0] / 255.0f;
}

static void sample(const uint8_t *atlas, float u, float v, float *out) {
  const float x = u * OVL_ATLAS_SIZE - 0.5f, y = v * OVL_ATLAS_SIZE - 0.5f;
  const int x0 = (int)floor(x), y0 = (int)floor(y);
  const float fx = x - (float)x0, fy = y - (float)y0;
  float a[4], b[4], c[4], d[4];
  int i;
  texel(atlas, x0, y0, a);
  texel(atlas, x0 + 1, y0, b);
  texel(atlas, x0, y0 + 1, c);
  texel(atlas, x0 + 1, y0 + 1, d);
  for (i = 0; i < 4; i++)
    out[i] = (a[i] * (1 - fx) + b[i] * fx) * (1 - fy) +
             (c[i] * (1 - fx) + d[i] * fx) * fy;
}

static float edge(const ovl_vertex_t *a, const ovl_vertex_t *b, float px,
                  float py) {
  return (b->x - a->x) * (py - a->y) - (b->y - a->y) * (px - a->x);
}

/* A pixel centre on an edge belongs to the triangle on the edge's top or left
 * side only */
static int top_left(const ovl_vertex_t *a, const ovl_vertex_t *b, float area) {
  const float dx = (b->x - a->x) * (area > 0 ? 1.0f : -1.0f);
  const float dy = (b->y - a->y) * (area > 0 ? 1.0f : -1.0f);
  return (dy == 0.0f && dx > 0.0f) || dy < 0.0f;
}

static void cpu_triangle(float *rgb, const uint8_t *atlas,
                         const ovl_vertex_t *v0, const ovl_vertex_t *v1,
                         const ovl_vertex_t *v2, const int *scissor) {
  const float area = edge(v0, v1, v2->x, v2->y);
  int x, y, i;
  float c0[4], c1[4], c2[4];
  if (area == 0.0f)
    return;
  for (i = 0; i < 4; i++) {
    c0[i] = ((v0->col >> (8 * i)) & 0xFF) / 255.0f;
    c1[i] = ((v1->col >> (8 * i)) & 0xFF) / 255.0f;
    c2[i] = ((v2->col >> (8 * i)) & 0xFF) / 255.0f;
  }
  for (y = scissor[1]; y < scissor[3]; y++)
    for (x = scissor[0]; x < scissor[2]; x++) {
      const float px = (float)x + 0.5f, py = (float)y + 0.5f;
      const float e0 = edge(v1, v2, px, py), e1 = edge(v2, v0, px, py),
                  e2 = edge(v0, v1, px, py);
      float w0, w1, w2, col[4], tex[4], *dst;
      if (area > 0 ? (e0 < 0 || e1 < 0 || e2 < 0)
                   : (e0 > 0 || e1 > 0 || e2 > 0))
        continue;
      if ((e0 == 0 && !top_left(v1, v2, area)) ||
          (e1 == 0 && !top_left(v2, v0, area)) ||
          (e2 == 0 && !top_left(v0, v1, area)))
        continue;
      w0 = e0 / area;
      w1 = e1 / area;
      w2 = e2 / area;
      sample(atlas, v0->u * w0 + v1->u * w1 + v2->u * w2,
             v0->v * w0 + v1->v * w1 + v2->v * w2, tex);
      for (i = 0; i < 4; i++)
        col[i] = (c0[i] * w0 + c1[i] * w1 + c2[i] * w2) * tex[i];
      dst = rgb + ((size_t)y * W + (size_t)x) * 3;
      for (i = 0; i < 3; i++)
        dst[i] = col[i] * col[3] + dst[i] * (1.0f - col[3]);
    }
}

static void cpu_frame(float *rgb, const ovl_draw_data_t *dd) {
  uint32_t c, i;
  for (i = 0; i < W * H; i++) {
    rgb[i * 3 + 0] = clear_colour[0];
    rgb[i * 3 + 1] = clear_colour[1];
    rgb[i * 3 + 2] = clear_colour[2];
  }
  for (c = 0; c < dd->cmd_count; c++) {
    const ovl_cmd_t *cmd = &dd->cmds[c];
    int scissor[4];
    scissor[0] = cmd->clip[0] < 0 ? 0 : (int)cmd->clip[0];
    scissor[1] = cmd->clip[1] < 0 ? 0 : (int)cmd->clip[1];
    scissor[2] = scissor[0] + (int)((cmd->clip[2] > W ? W : cmd->clip[2]) -
                                    (float)scissor[0]);
    scissor[3] = scissor[1] + (int)((cmd->clip[3] > H ? H : cmd->clip[3]) -
                                    (float)scissor[1]);
    for (i = 0; i + 2 < cmd->idx_count; i += 3)
      cpu_triangle(rgb, ovl_atlas_pixels(),
                   &dd->vtx[dd->idx[cmd->idx_offset + i]],
                   &dd->vtx[dd->idx[cmd->idx_offset + i + 1]],
                   &dd->vtx[dd->idx[cmd->idx_offset + i + 2]], scissor);
  }
}

/* ------------------------------------------------------------------------------------------
 */

static unsigned char *read_file(const char *path, size_t *size) {
  FILE *f = fopen(path, "rb");
  unsigned char *data;
  long n;
  if (!f)
    return NULL;
  fseek(f, 0, SEEK_END);
  n = ftell(f);
  fseek(f, 0, SEEK_SET);
  data = (unsigned char *)malloc((size_t)n);
  if (data && fread(data, 1, (size_t)n, f) != (size_t)n) {
    free(data);
    data = NULL;
  }
  fclose(f);
  *size = (size_t)n;
  return data;
}

/* The FPS counter, red, at 7 frames a second; and a few of ImGui's shapes under
 * it */
static const ovl_draw_data_t *build_frame(void) {
  ovl_vec4_t red;
  ovl_vec2_t pos, a, b;
  red.x = 1.0f;
  red.y = 0.0f;
  red.z = 0.0f;
  red.w = 1.0f;
  ovl_new_frame((float)W, (float)H, 1.0f / 60.0f);
  a.x = 10.5f;
  a.y = 10.0f;
  b.x = 70.25f;
  b.y = 50.75f;
  ovl_add_rect_filled(ovl_background_draw_list(), a, b,
                      OVL_COL(40, 200, 90, 180), 9.0f, 0);
  a.x = 100.0f;
  a.y = 32.0f;
  ovl_add_circle(ovl_background_draw_list(), a, 20.0f,
                 OVL_COL(255, 255, 0, 255), 0, 2.0f);
  if (ovl_begin("Video Info", OVL_WINDOW_NO_NAV | OVL_WINDOW_NO_DECORATION |
                                  OVL_WINDOW_ALWAYS_AUTO_RESIZE |
                                  OVL_WINDOW_NO_INPUTS)) {
    pos.x = ovl_display_size().x - ovl_window_size().x;
    pos.y = 0.0f;
    ovl_set_window_pos(pos);
    ovl_push_color(OVL_COLOR_TEXT, red);
    ovl_text("7 FPS (142.9 ms)", NULL);
    ovl_pop_color(1);
  }
  ovl_end();
  return ovl_render();
}

static void run(const char *font_path) {
  static uint32_t words[HOST_SPIRV_MAX_WORDS];
  unsigned char *noto;
  size_t size, n;
  ovl_face_desc_t face;
  const ovl_draw_data_t *dd = NULL;
  VkShaderModule vert, frag;
  VkDescriptorSetLayoutBinding binding;
  VkDescriptorSetLayoutCreateInfo sli;
  VkDescriptorSetLayout set_layout;
  VkPushConstantRange push;
  VkPipelineLayoutCreateInfo pli;
  VkPipelineLayout layout;
  VkSamplerCreateInfo sci;
  VkSampler sampler;
  VkDescriptorPoolSize pool_size;
  VkDescriptorPoolCreateInfo dpi;
  VkDescriptorPool dpool;
  VkDescriptorSetAllocateInfo dai;
  VkDescriptorSet set;
  VkDescriptorImageInfo image_info;
  VkWriteDescriptorSet write;
  VkImage atlas, target;
  VkImageView atlas_view, target_view;
  VkRenderPass pass;
  VkFramebufferCreateInfo fi;
  VkFramebuffer fb;
  VkPipeline pipeline;
  void *staging_map, *vtx_map, *idx_map, *out_map;
  VkBuffer staging, vtx, idx, out;
  VkImageMemoryBarrier barrier;
  VkBufferImageCopy region;
  VkRenderPassBeginInfo rb;
  VkClearValue clear;
  VkViewport viewport;
  VkDeviceSize zero = 0;
  float scale_translate[4];
  float *want;
  uint32_t c;
  int frame, i, worst = 0, off = 0, drawn = 0;

  noto = read_file(font_path, &size);
  if (!noto) {
    CHECK(0, "the font");
    return;
  }
  face.data = noto;
  face.size = size;
  face.index = 0;
  if (!ovl_create(ovl_font_add(&face, 1, 32.0f), 0.5f)) {
    CHECK(0, "the overlay");
    return;
  }
  for (frame = 0; frame < 3; frame++)
    dd = build_frame();
  CHECK(dd && dd->idx_count > 0, "a frame to draw");

  n = host_spirv_overlay_vert(words, HOST_SPIRV_MAX_WORDS);
  vert = n ? make_module(words, n) : VK_NULL_HANDLE;
  n = host_spirv_overlay_frag(words, HOST_SPIRV_MAX_WORDS, 0.0f);
  frag = n ? make_module(words, n) : VK_NULL_HANDLE;
  CHECK(vert && frag, "the overlay's shaders");

  memset(&binding, 0, sizeof(binding));
  binding.descriptorCount = 1;
  binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
  memset(&sli, 0, sizeof(sli));
  sli.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
  sli.bindingCount = 1;
  sli.pBindings = &binding;
  p_vkCreateDescriptorSetLayout(device, &sli, NULL, &set_layout);
  push.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
  push.offset = 0;
  push.size = sizeof(scale_translate);
  memset(&pli, 0, sizeof(pli));
  pli.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  pli.setLayoutCount = 1;
  pli.pSetLayouts = &set_layout;
  pli.pushConstantRangeCount = 1;
  pli.pPushConstantRanges = &push;
  p_vkCreatePipelineLayout(device, &pli, NULL, &layout);
  memset(&sci, 0, sizeof(sci));
  sci.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
  sci.magFilter = VK_FILTER_LINEAR;
  sci.minFilter = VK_FILTER_LINEAR;
  sci.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
  sci.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
  sci.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
  sci.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
  sci.maxAnisotropy = 1.0f;
  sci.minLod = -1000.0f;
  sci.maxLod = 1000.0f;
  p_vkCreateSampler(device, &sci, NULL, &sampler);
  pool_size.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  pool_size.descriptorCount = 1;
  memset(&dpi, 0, sizeof(dpi));
  dpi.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
  dpi.maxSets = 1;
  dpi.poolSizeCount = 1;
  dpi.pPoolSizes = &pool_size;
  p_vkCreateDescriptorPool(device, &dpi, NULL, &dpool);

  atlas =
      make_image(VK_FORMAT_R8G8_UNORM, OVL_ATLAS_SIZE, OVL_ATLAS_SIZE,
                 VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT);
  atlas_view =
      atlas ? make_view(atlas, VK_FORMAT_R8G8_UNORM, 1) : VK_NULL_HANDLE;
  target = make_image(VK_FORMAT_R8G8B8A8_UNORM, W, H,
                      VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                          VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
  target_view =
      target ? make_view(target, VK_FORMAT_R8G8B8A8_UNORM, 0) : VK_NULL_HANDLE;
  pass = make_pass();
  memset(&fi, 0, sizeof(fi));
  fi.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
  fi.renderPass = pass;
  fi.attachmentCount = 1;
  fi.pAttachments = &target_view;
  fi.width = W;
  fi.height = H;
  fi.layers = 1;
  fb = VK_NULL_HANDLE;
  p_vkCreateFramebuffer(device, &fi, NULL, &fb);
  pipeline = make_pipeline(pass, layout, vert, frag);
  staging = host_buffer((VkDeviceSize)OVL_ATLAS_SIZE * OVL_ATLAS_SIZE * 2,
                        VK_BUFFER_USAGE_TRANSFER_SRC_BIT, &staging_map);
  vtx = host_buffer(dd->vtx_count * sizeof(ovl_vertex_t),
                    VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, &vtx_map);
  idx = host_buffer(dd->idx_count * sizeof(uint32_t),
                    VK_BUFFER_USAGE_INDEX_BUFFER_BIT, &idx_map);
  out = host_buffer(W * H * 4, VK_BUFFER_USAGE_TRANSFER_DST_BIT, &out_map);
  memset(&dai, 0, sizeof(dai));
  dai.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
  dai.descriptorPool = dpool;
  dai.descriptorSetCount = 1;
  dai.pSetLayouts = &set_layout;
  set = VK_NULL_HANDLE;
  if (!atlas_view || !target_view || !pass || !fb || !pipeline || !staging ||
      !vtx || !idx || !out ||
      p_vkAllocateDescriptorSets(device, &dai, &set) != VK_SUCCESS) {
    CHECK(0, "the device's objects");
    return;
  }
  memset(&image_info, 0, sizeof(image_info));
  image_info.sampler = sampler;
  image_info.imageView = atlas_view;
  image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  memset(&write, 0, sizeof(write));
  write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  write.dstSet = set;
  write.descriptorCount = 1;
  write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  write.pImageInfo = &image_info;
  p_vkUpdateDescriptorSets(device, 1, &write, 0, NULL);

  memcpy(staging_map, ovl_atlas_pixels(),
         (size_t)OVL_ATLAS_SIZE * OVL_ATLAS_SIZE * 2);
  memcpy(vtx_map, dd->vtx, dd->vtx_count * sizeof(ovl_vertex_t));
  memcpy(idx_map, dd->idx, dd->idx_count * sizeof(uint32_t));

  {
    VkCommandBufferBeginInfo bi;
    memset(&bi, 0, sizeof(bi));
    bi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    p_vkBeginCommandBuffer(cb, &bi);
  }
  memset(&barrier, 0, sizeof(barrier));
  barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
  barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.image = atlas;
  barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  barrier.subresourceRange.levelCount = 1;
  barrier.subresourceRange.layerCount = 1;
  barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
  barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
  p_vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                         VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, NULL, 0, NULL, 1,
                         &barrier);
  memset(&region, 0, sizeof(region));
  region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  region.imageSubresource.layerCount = 1;
  region.imageExtent.width = OVL_ATLAS_SIZE;
  region.imageExtent.height = OVL_ATLAS_SIZE;
  region.imageExtent.depth = 1;
  p_vkCmdCopyBufferToImage(cb, staging, atlas,
                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
  barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
  barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
  barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
  barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  p_vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, NULL, 0,
                         NULL, 1, &barrier);
  memset(&clear, 0, sizeof(clear));
  clear.color.float32[0] = clear_colour[0];
  clear.color.float32[1] = clear_colour[1];
  clear.color.float32[2] = clear_colour[2];
  clear.color.float32[3] = 1.0f;
  memset(&rb, 0, sizeof(rb));
  rb.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
  rb.renderPass = pass;
  rb.framebuffer = fb;
  rb.renderArea.extent.width = W;
  rb.renderArea.extent.height = H;
  rb.clearValueCount = 1;
  rb.pClearValues = &clear;
  p_vkCmdBeginRenderPass(cb, &rb, VK_SUBPASS_CONTENTS_INLINE);
  p_vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
  p_vkCmdBindVertexBuffers(cb, 0, 1, &vtx, &zero);
  p_vkCmdBindIndexBuffer(cb, idx, 0, VK_INDEX_TYPE_UINT32);
  memset(&viewport, 0, sizeof(viewport));
  viewport.width = (float)W;
  viewport.height = (float)H;
  viewport.maxDepth = 1.0f;
  p_vkCmdSetViewport(cb, 0, 1, &viewport);
  scale_translate[0] = 2.0f / dd->display_w;
  scale_translate[1] = 2.0f / dd->display_h;
  scale_translate[2] = -1.0f;
  scale_translate[3] = -1.0f;
  p_vkCmdPushConstants(cb, layout, VK_SHADER_STAGE_VERTEX_BIT, 0,
                       sizeof(scale_translate), scale_translate);
  p_vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, 0, 1,
                            &set, 0, NULL);
  for (c = 0; c < dd->cmd_count; c++) {
    const ovl_cmd_t *cmd = &dd->cmds[c];
    const float x0 = cmd->clip[0] < 0 ? 0 : cmd->clip[0];
    const float y0 = cmd->clip[1] < 0 ? 0 : cmd->clip[1];
    const float x1 = cmd->clip[2] > W ? W : cmd->clip[2];
    const float y1 = cmd->clip[3] > H ? H : cmd->clip[3];
    VkRect2D scissor;
    if (x1 <= x0 || y1 <= y0)
      continue;
    scissor.offset.x = (int32_t)x0;
    scissor.offset.y = (int32_t)y0;
    scissor.extent.width = (uint32_t)(x1 - x0);
    scissor.extent.height = (uint32_t)(y1 - y0);
    p_vkCmdSetScissor(cb, 0, 1, &scissor);
    p_vkCmdDrawIndexed(cb, cmd->idx_count, 1, cmd->idx_offset, 0, 0);
  }
  p_vkCmdEndRenderPass(cb);
  memset(&region, 0, sizeof(region));
  region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  region.imageSubresource.layerCount = 1;
  region.imageExtent.width = W;
  region.imageExtent.height = H;
  region.imageExtent.depth = 1;
  p_vkCmdCopyImageToBuffer(cb, target, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                           out, 1, &region);
  p_vkEndCommandBuffer(cb);
  {
    VkSubmitInfo si;
    memset(&si, 0, sizeof(si));
    si.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    si.commandBufferCount = 1;
    si.pCommandBuffers = &cb;
    p_vkQueueSubmit(queue, 1, &si, VK_NULL_HANDLE);
    p_vkQueueWaitIdle(queue);
  }

  want = (float *)malloc(sizeof(float) * W * H * 3);
  cpu_frame(want, dd);
  for (i = 0; i < W * H; i++) {
    const uint8_t *got = (const uint8_t *)out_map + i * 4;
    int k, d = 0, changed = 0;
    for (k = 0; k < 3; k++) {
      const int w = (int)(want[i * 3 + k] * 255.0f + 0.5f);
      const int bg = (int)(clear_colour[k] * 255.0f + 0.5f);
      const int diff = abs((int)got[k] - w);
      if (diff > d)
        d = diff;
      if (w != bg)
        changed = 1;
    }
    drawn += changed;
    if (d > worst)
      worst = d;
    if (d > 2)
      off++;
  }
  printf("overlay_device: %d of %d drawn pixels more than 2/255 off, the "
         "largest %d/255\n",
         off, drawn, worst);
  CHECK(drawn > 1000, "the frame drawn");
  /* Pixel centres exactly on a shared edge can go either way between
   * rasterizers */
  CHECK(off * 200 <= drawn, "the device's frame as the core's CPU model of it");
  free(want);
  ovl_destroy();
  free(noto);
}

int main(int argc, char **argv) {
  void *lib = dlopen("libvulkan.so.1", RTLD_NOW);
  VkApplicationInfo app;
  VkInstanceCreateInfo instance_info;
  VkInstance instance;
  VkPhysicalDevice physical;
  VkDeviceQueueCreateInfo queue_info;
  VkDeviceCreateInfo device_info;
  VkCommandPoolCreateInfo pool_info;
  VkCommandPool pool;
  VkCommandBufferAllocateInfo cb_info;
  float priority = 1.0f;
  uint32_t count = 1;

  if (lib)
    *(void **)&get_instance_proc = dlsym(lib, "vkGetInstanceProcAddr");
  if (!lib || !get_instance_proc) {
    printf("overlay_device: no Vulkan loader, skipped\n");
    return 0;
  }
  p_vkCreateInstance =
      (PFN_vkCreateInstance)get_instance_proc(NULL, "vkCreateInstance");
  memset(&app, 0, sizeof(app));
  app.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
  app.apiVersion = VK_API_VERSION_1_1;
  memset(&instance_info, 0, sizeof(instance_info));
  instance_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
  instance_info.pApplicationInfo = &app;
  if (p_vkCreateInstance(&instance_info, NULL, &instance) != VK_SUCCESS) {
    printf("overlay_device: no Vulkan instance, skipped\n");
    return 0;
  }
#define LOADI(name) p_##name = (PFN_##name)get_instance_proc(instance, #name)
  LOADI(vkEnumeratePhysicalDevices);
  LOADI(vkGetPhysicalDeviceMemoryProperties);
  LOADI(vkCreateDevice);
  get_device_proc = (PFN_vkGetDeviceProcAddr)get_instance_proc(
      instance, "vkGetDeviceProcAddr");
  if (p_vkEnumeratePhysicalDevices(instance, &count, &physical) < 0 || !count) {
    printf("overlay_device: no device, skipped\n");
    return 0;
  }
  p_vkGetPhysicalDeviceMemoryProperties(physical, &mem);
  memset(&queue_info, 0, sizeof(queue_info));
  queue_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
  queue_info.queueCount = 1;
  queue_info.pQueuePriorities = &priority;
  memset(&device_info, 0, sizeof(device_info));
  device_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
  device_info.queueCreateInfoCount = 1;
  device_info.pQueueCreateInfos = &queue_info;
  if (p_vkCreateDevice(physical, &device_info, NULL, &device) != VK_SUCCESS) {
    printf("overlay_device: no device, skipped\n");
    return 0;
  }
#define LOADD(name) p_##name = (PFN_##name)get_device_proc(device, #name)
  LOADD(vkGetDeviceQueue);
  LOADD(vkAllocateMemory);
  LOADD(vkMapMemory);
  LOADD(vkCreateBuffer);
  LOADD(vkGetBufferMemoryRequirements);
  LOADD(vkBindBufferMemory);
  LOADD(vkCreateImage);
  LOADD(vkGetImageMemoryRequirements);
  LOADD(vkBindImageMemory);
  LOADD(vkCreateImageView);
  LOADD(vkCreateRenderPass);
  LOADD(vkCreateFramebuffer);
  LOADD(vkCreateShaderModule);
  LOADD(vkCreateDescriptorSetLayout);
  LOADD(vkCreatePipelineLayout);
  LOADD(vkCreateGraphicsPipelines);
  LOADD(vkCreateDescriptorPool);
  LOADD(vkAllocateDescriptorSets);
  LOADD(vkUpdateDescriptorSets);
  LOADD(vkCreateCommandPool);
  LOADD(vkAllocateCommandBuffers);
  LOADD(vkBeginCommandBuffer);
  LOADD(vkEndCommandBuffer);
  LOADD(vkCmdBeginRenderPass);
  LOADD(vkCmdEndRenderPass);
  LOADD(vkCmdBindPipeline);
  LOADD(vkCmdBindDescriptorSets);
  LOADD(vkCmdBindVertexBuffers);
  LOADD(vkCmdBindIndexBuffer);
  LOADD(vkCmdDrawIndexed);
  LOADD(vkCmdSetScissor);
  LOADD(vkCmdSetViewport);
  LOADD(vkCmdCopyImageToBuffer);
  LOADD(vkCmdCopyBufferToImage);
  LOADD(vkCmdPipelineBarrier);
  LOADD(vkCmdPushConstants);
  LOADD(vkCreateSampler);
  LOADD(vkQueueSubmit);
  LOADD(vkQueueWaitIdle);
  p_vkGetDeviceQueue(device, 0, 0, &queue);
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

  run(argc > 1 ? argv[1] : "src/imgui/renderer/fonts/NotoSans-Regular.ttf");

  if (failures) {
    printf("overlay_device: %d failed\n", failures);
    return 1;
  }
  printf("overlay_device: all passed\n");
  return 0;
}
