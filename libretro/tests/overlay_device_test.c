/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

/* The overlays drawn on a device by the core's own renderer
 * (video_core/overlay/vk_overlay.c): the FPS counter, a notification with its
 * icon and the trophy popup with both of its, over a picture, through the
 * atlas, the images' slots, the shaders and the pipeline as the core makes
 * them. What comes out is checked against the same frame drawn here on the CPU
 * as a GPU draws it (pixel centres, top-left rule, bilinear texels with repeat,
 * source-alpha blending); ovl_test and the ImGui comparison it was made with
 * tie that frame to ImGui's look. Then, frames on, the images released and
 * their slots free again. Made after a failed attempt (no font), which makes
 * nothing and says why. First, the splash screen: decoded on its thread and
 * drawn in place of the game's picture, black around it, as the CPU model of
 * it. Skipped (exit 0) with no device that does dynamic
 * rendering.
 *
 *   VK_ICD_FILENAMES=/usr/share/vulkan/icd.d/lvp_icd.json
 * build-tests/overlay_device_test */

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <dlfcn.h>

#include "../../src/core/emulator_settings.h"
#include "../../src/video_core/overlay/ovl.h"
#include "../../src/video_core/overlay/ovl_layers.h"
#include "../../src/video_core/overlay/vk_overlay.h"
#include "../audio_mix.h"

#include <formats/image.h>
#include <formats/rpng.h>

static int failures = 0;

#define CHECK(cond, what)                                                      \
  do {                                                                         \
    if (!(cond)) {                                                             \
      printf("FAIL %s (line %d)\n", what, __LINE__);                           \
      ++failures;                                                              \
    }                                                                          \
  } while (0)

/* What the overlays ask of the core's settings and mix */
bool emu_settings_get_show_fps_counter(void) { return true; }
bool emu_settings_get_hdr_allowed(void) { return false; }
unsigned emu_settings_get_hdr_paper_white(void) { return 203; }
bool emu_settings_get_vkhost_markers(void) { return false; }
bool emu_settings_get_trophy_popup_disabled(void) { return false; }
const char *emu_settings_get_trophy_notification_side(void) { return "right"; }
double emu_settings_get_trophy_notification_duration(void) { return 6.0; }
int emu_settings_get_volume_slider(void) { return 100; }
bool emu_settings_get_show_splash(void) { return true; }
void audio_mix_play_clip(const float *frames, unsigned count, float gain) {
  (void)frames;
  (void)count;
  (void)gain;
}
void ovl_invite_chosen(int user_id, const char *invitation_id, const char *from_npid,
                       int accept) {
  (void)user_id;
  (void)invitation_id;
  (void)from_npid;
  (void)accept;
}

static PFN_vkGetInstanceProcAddr get_instance_proc;
static PFN_vkGetDeviceProcAddr get_device_proc;
#define FN(name) static PFN_##name p_##name
FN(vkCreateInstance);
FN(vkEnumeratePhysicalDevices);
FN(vkGetPhysicalDeviceMemoryProperties);
FN(vkGetPhysicalDeviceProperties);
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
FN(vkCreateCommandPool);
FN(vkAllocateCommandBuffers);
FN(vkBeginCommandBuffer);
FN(vkEndCommandBuffer);
FN(vkCmdPipelineBarrier);
FN(vkCmdClearColorImage);
FN(vkCmdCopyImageToBuffer);
FN(vkQueueSubmit);
FN(vkQueueWaitIdle);
FN(vkDeviceWaitIdle);

#define W 640
#define H 360

static VkDevice device;
static VkQueue queue;
static VkCommandBuffer cb;
static VkPhysicalDeviceMemoryProperties mem;
static const float clear_colour[3] = {0.2f, 0.3f, 0.4f};

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

static void barrier(VkImage image, VkImageLayout from, VkImageLayout to,
                    VkAccessFlags src, VkAccessFlags dst) {
  VkImageMemoryBarrier b;
  memset(&b, 0, sizeof(b));
  b.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
  b.srcAccessMask = src;
  b.dstAccessMask = dst;
  b.oldLayout = from;
  b.newLayout = to;
  b.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  b.image = image;
  b.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  b.subresourceRange.levelCount = 1;
  b.subresourceRange.layerCount = 1;
  p_vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                         VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, NULL, 0,
                         NULL, 1, &b);
}

static void submit(void) {
  VkSubmitInfo si;
  p_vkEndCommandBuffer(cb);
  memset(&si, 0, sizeof(si));
  si.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  si.commandBufferCount = 1;
  si.pCommandBuffers = &cb;
  p_vkQueueSubmit(queue, 1, &si, VK_NULL_HANDLE);
  p_vkQueueWaitIdle(queue);
}

static void begin(void) {
  VkCommandBufferBeginInfo bi;
  memset(&bi, 0, sizeof(bi));
  bi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  p_vkBeginCommandBuffer(cb, &bi);
}

/* ------------------------------------------------------------------------------------------
 */
/* The same frame on the CPU, drawn as a GPU draws it */

typedef struct texture {
  const uint8_t *px;
  int w, h, bpp; /* bpp 2: the atlas, sampled (G, G, G, R); 4: an image's slot
                    or the splash, RGBA */
} texture_t;

static void texel(const texture_t *t, int x, int y, float *out) {
  const uint8_t *p;
  x = ((x % t->w) + t->w) % t->w;
  y = ((y % t->h) + t->h) % t->h;
  p = t->px + ((size_t)y * (size_t)t->w + (size_t)x) * (size_t)t->bpp;
  if (t->bpp == 2) {
    out[0] = out[1] = out[2] = p[1] / 255.0f;
    out[3] = p[0] / 255.0f;
  } else {
    out[0] = p[0] / 255.0f;
    out[1] = p[1] / 255.0f;
    out[2] = p[2] / 255.0f;
    out[3] = p[3] / 255.0f;
  }
}

static void sample(const texture_t *t, float u, float v, float *out) {
  const float x = u * (float)t->w - 0.5f, y = v * (float)t->h - 0.5f;
  const int x0 = (int)floor(x), y0 = (int)floor(y);
  const float fx = x - (float)x0, fy = y - (float)y0;
  float a[4], b[4], c[4], d[4];
  int i;
  texel(t, x0, y0, a);
  texel(t, x0 + 1, y0, b);
  texel(t, x0, y0 + 1, c);
  texel(t, x0 + 1, y0 + 1, d);
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

static void cpu_triangle(float *rgb, const texture_t *t, const ovl_vertex_t *v0,
                         const ovl_vertex_t *v1, const ovl_vertex_t *v2,
                         const int *scissor) {
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
      sample(t, v0->u * w0 + v1->u * w1 + v2->u * w2,
             v0->v * w0 + v1->v * w1 + v2->v * w2, tex);
      for (i = 0; i < 4; i++)
        col[i] = (c0[i] * w0 + c1[i] * w1 + c2[i] * w2) * tex[i];
      dst = rgb + ((size_t)y * W + (size_t)x) * 3;
      for (i = 0; i < 3; i++)
        dst[i] = col[i] * col[3] + dst[i] * (1.0f - col[3]);
    }
}

static int cpu_frame(float *rgb, const ovl_draw_data_t *dd) {
  uint32_t c, i;
  int images = 0;
  for (i = 0; i < W * H; i++) {
    rgb[i * 3 + 0] = clear_colour[0];
    rgb[i * 3 + 1] = clear_colour[1];
    rgb[i * 3 + 2] = clear_colour[2];
  }
  for (c = 0; c < dd->cmd_count; c++) {
    const ovl_cmd_t *cmd = &dd->cmds[c];
    texture_t t;
    int scissor[4];
    if (cmd->tex == OVL_TEX_ATLAS) {
      t.px = ovl_atlas_pixels();
      t.w = t.h = OVL_ATLAS_SIZE;
      t.bpp = 2;
    } else {
      t.px = ovl_tex_pixels((int)cmd->tex);
      t.w = t.h = OVL_TEX_DIM;
      t.bpp = 4;
      images++;
    }
    scissor[0] = cmd->clip[0] < 0 ? 0 : (int)cmd->clip[0];
    scissor[1] = cmd->clip[1] < 0 ? 0 : (int)cmd->clip[1];
    scissor[2] = scissor[0] + (int)((cmd->clip[2] > W ? W : cmd->clip[2]) -
                                    (float)scissor[0]);
    scissor[3] = scissor[1] + (int)((cmd->clip[3] > H ? H : cmd->clip[3]) -
                                    (float)scissor[1]);
    for (i = 0; i + 2 < cmd->idx_count; i += 3)
      cpu_triangle(rgb, &t, &dd->vtx[dd->idx[cmd->idx_offset + i]],
                   &dd->vtx[dd->idx[cmd->idx_offset + i + 1]],
                   &dd->vtx[dd->idx[cmd->idx_offset + i + 2]], scissor);
  }
  return images;
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

/* @png as rpng decodes it, R, G, B, A bytes; its size */
static uint8_t *decode_png(const unsigned char *png, size_t size, int *w,
                           int *h) {
  rpng_t *r = rpng_alloc();
  struct rpng_info info;
  uint8_t *px = NULL;
  void *out = NULL;
  unsigned ow, oh;
  int ret;
  rpng_set_buf_ptr(r, (void *)png, size);
  rpng_start(r);
  while (rpng_iterate_image(r)) {
  }
  if (rpng_get_info(r, &info)) {
    px = (uint8_t *)malloc((size_t)info.width * info.height * 4);
    rpng_set_output(r, (uint32_t *)px, info.width);
    do {
      ret = rpng_process_image(r, &out, size, &ow, &oh, true);
    } while (ret == IMAGE_PROCESS_NEXT);
    *w = (int)info.width;
    *h = (int)info.height;
  }
  rpng_free(r);
  return px;
}

/* The splash drawn in place of the game's picture, read back into @out_map:
 * as the CPU model of it, and black around it */
static void check_splash(VkImage target, VkImageView view, VkBuffer out,
                         const void *out_map, const unsigned char *png,
                         size_t png_size) {
  const uint32_t idx[6] = {0, 1, 2, 0, 2, 3};
  const int scissor[4] = {0, 0, W, H};
  ovl_vertex_t v[4];
  VkBufferImageCopy region;
  VkExtent2D extent;
  texture_t t;
  float *want;
  int i, worst = 0, off = 0, black = 1;
  t.px = decode_png(png, png_size, &t.w, &t.h);
  t.bpp = 4;
  CHECK(t.px != NULL, "the splash decoded here");
  if (!t.px)
    return;
  /* Its own thread decodes it from when the renderer is made */
  while (!ovl_vk_splash_ready()) {
  }
  extent.width = W;
  extent.height = H;
  ovl_vk_new_frame(W, H, 1.0f / 60.0f, 17.0f);
  begin();
  barrier(target, VK_IMAGE_LAYOUT_UNDEFINED,
          VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, 0,
          VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT);
  ovl_vk_render_splash(cb, view, extent);
  barrier(target, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
          VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
          VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT);
  memset(&region, 0, sizeof(region));
  region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  region.imageSubresource.layerCount = 1;
  region.imageExtent.width = W;
  region.imageExtent.height = H;
  region.imageExtent.depth = 1;
  p_vkCmdCopyImageToBuffer(cb, target, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                           out, 1, &region);
  submit();

  want = (float *)calloc((size_t)W * H * 3, sizeof(float));
  ovl_splash_quad((float)W, (float)H, v);
  for (i = 0; i < 6; i += 3)
    cpu_triangle(want, &t, &v[idx[i]], &v[idx[i + 1]], &v[idx[i + 2]], scissor);
  for (i = 0; i < W * H; i++) {
    const uint8_t *got = (const uint8_t *)out_map + i * 4;
    const int x = i % W;
    int k, d = 0;
    for (k = 0; k < 3; k++) {
      const int diff =
          abs((int)got[k] - (int)(want[i * 3 + k] * 255.0f + 0.5f));
      if (diff > d)
        d = diff;
    }
    if (d > worst)
      worst = d;
    if (d > 2)
      off++;
    /* Outside the picture, left and right of it here */
    if ((float)x < v[0].x - 1.0f || (float)x > v[2].x)
      black &= got[0] == 0 && got[1] == 0 && got[2] == 0 && got[3] == 255;
  }
  printf("overlay_device: splash %dx%d at %gx%g..%gx%g, %d pixels more than "
         "2/255 off, the largest %d/255\n",
         t.w, t.h, v[0].x, v[0].y, v[2].x, v[2].y, off, worst);
  CHECK(v[0].x > 0.0f, "a square splash fitted between bars");
  CHECK(off * 200 <= W * H, "the device's splash as the CPU model of it");
  CHECK(black, "black around it");
  free(want);
  free((void *)t.px);

  /* Hidden by the game: no longer drawn, freed frames on */
  ovl_vk_splash_done();
  CHECK(!ovl_vk_splash_ready(), "no longer shown");
}

/* How many images can be decoded now: every slot taken with @png, then given
 * back */
static int free_slots(const unsigned char *png, size_t size) {
  int handles[OVL_TEX_SLOTS + 1], n = 0, i;
  while (n <= OVL_TEX_SLOTS &&
         (handles[n] = ovl_tex_decode_png(png, size)) != 0)
    n++;
  for (i = 0; i < n; i++)
    ovl_tex_release(handles[i]);
  return n;
}

static void run(void) {
  size_t font_size, icon_size, gold_size, plat_size;
  unsigned char *font =
      read_file("src/imgui/renderer/fonts/NotoSans-Regular.ttf", &font_size);
  unsigned char *icon = read_file("src/resources/shadps4.png", &icon_size);
  unsigned char *gold = read_file("src/resources/gold.png", &gold_size);
  unsigned char *plat = read_file("src/resources/platinum.png", &plat_size);
  ovl_vk_info_t info;
  VkImageCreateInfo ii;
  VkImageViewCreateInfo vi;
  VkBufferCreateInfo bi;
  VkMemoryRequirements req;
  VkImage target;
  VkImageView view;
  VkBuffer out;
  VkDeviceMemory out_memory;
  void *out_map = NULL;
  VkClearColorValue clear;
  VkImageSubresourceRange range;
  VkBufferImageCopy region;
  VkExtent2D extent;
  float *want;
  const ovl_draw_data_t *dd;
  int frame, i, worst = 0, off = 0, drawn = 0, images;

  if (!font || !icon || !gold || !plat) {
    CHECK(0, "the font and icons");
    return;
  }
  memset(&info, 0, sizeof(info));
  info.get_device_proc = get_device_proc;
  info.device = device;
  info.memory_properties = mem;
  info.image_count = 3;
  info.surface_format = VK_FORMAT_R8G8B8A8_UNORM;
  info.font = font;
  info.font_size = font_size;
  info.splash = icon;
  info.splash_size = icon_size;
  /* Without the font: nothing made, and said why */
  info.font = NULL;
  info.font_size = 0;
  CHECK(!ovl_vk_init(&info) && strcmp(ovl_vk_error(), "the UI font") == 0,
        "no overlays without the font");
  CHECK(!ovl_vk_must_keep_drawing(),
        "nothing to keep drawing for without them");
  info.font = font;
  info.font_size = font_size;
  CHECK(ovl_vk_init(&info), "the core's renderer made");
  if (failures)
    return;

  memset(&ii, 0, sizeof(ii));
  ii.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
  ii.imageType = VK_IMAGE_TYPE_2D;
  ii.format = VK_FORMAT_R8G8B8A8_UNORM;
  ii.extent.width = W;
  ii.extent.height = H;
  ii.extent.depth = 1;
  ii.mipLevels = 1;
  ii.arrayLayers = 1;
  ii.samples = VK_SAMPLE_COUNT_1_BIT;
  ii.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
             VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
  p_vkCreateImage(device, &ii, NULL, &target);
  p_vkGetImageMemoryRequirements(device, target, &req);
  p_vkBindImageMemory(device, target, alloc(&req, 0), 0);
  memset(&vi, 0, sizeof(vi));
  vi.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
  vi.image = target;
  vi.viewType = VK_IMAGE_VIEW_TYPE_2D;
  vi.format = VK_FORMAT_R8G8B8A8_UNORM;
  vi.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  vi.subresourceRange.levelCount = 1;
  vi.subresourceRange.layerCount = 1;
  p_vkCreateImageView(device, &vi, NULL, &view);
  memset(&bi, 0, sizeof(bi));
  bi.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  bi.size = W * H * 4;
  bi.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
  p_vkCreateBuffer(device, &bi, NULL, &out);
  p_vkGetBufferMemoryRequirements(device, out, &req);
  out_memory = alloc(&req, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                               VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
  p_vkBindBufferMemory(device, out, out_memory, 0);
  p_vkMapMemory(device, out_memory, 0, VK_WHOLE_SIZE, 0, &out_map);

  check_splash(target, view, out, out_map, icon, icon_size);

  CHECK(ovl_notify_queue("Settings saved, and a second line\nto wrap", 3.0f,
                         OVL_NOTIFY_BOTTOM_LEFT, icon, icon_size, 1),
        "a notification queued");
  CHECK(ovl_trophy_queue("Lantern Lit", plat, plat_size, gold, gold_size),
        "a trophy queued");
  CHECK(ovl_vk_must_keep_drawing(), "frames to be drawn while they wait");

  /* Drawn frame by frame, as the presenter would: well into both */
  extent.width = W;
  extent.height = H;
  memset(&clear, 0, sizeof(clear));
  clear.float32[0] = clear_colour[0];
  clear.float32[1] = clear_colour[1];
  clear.float32[2] = clear_colour[2];
  clear.float32[3] = 1.0f;
  memset(&range, 0, sizeof(range));
  range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  range.levelCount = 1;
  range.layerCount = 1;
  for (frame = 0; frame < 60; frame++) {
    ovl_vk_new_frame(W, H, 1.0f / 60.0f, 17.0f);
    begin();
    barrier(target, VK_IMAGE_LAYOUT_UNDEFINED,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 0,
            VK_ACCESS_TRANSFER_WRITE_BIT);
    p_vkCmdClearColorImage(cb, target, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                           &clear, 1, &range);
    barrier(target, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT);
    ovl_vk_render(cb, view, extent);
    barrier(target, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT);
    memset(&region, 0, sizeof(region));
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.layerCount = 1;
    region.imageExtent.width = W;
    region.imageExtent.height = H;
    region.imageExtent.depth = 1;
    p_vkCmdCopyImageToBuffer(cb, target, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                             out, 1, &region);
    submit();
  }

  /* The last frame: its draws as the CPU model draws them */
  dd = ovl_draw_data();
  want = (float *)malloc(sizeof(float) * W * H * 3);
  images = cpu_frame(want, dd);
  CHECK(images == 3,
        "the notification's icon and the trophy's two, from their slots");
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
  CHECK(drawn > 10000, "the frame drawn");
  /* Pixel centres exactly on a shared edge can go either way between
   * rasterizers */
  CHECK(off * 200 <= drawn, "the device's frame as the CPU model of it");
  free(want);

  /* The notification over after its three seconds, the trophy after its six:
   * the images released, and their slots free again once the frames that drew
   * them are done */
  for (frame = 0; frame < 420; frame++) {
    ovl_vk_new_frame(W, H, 1.0f / 60.0f, 17.0f);
    begin();
    barrier(target, VK_IMAGE_LAYOUT_UNDEFINED,
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, 0,
            VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT);
    ovl_vk_render(cb, view, extent);
    submit();
  }
  CHECK(!ovl_vk_must_keep_drawing(), "nothing left to show");
  CHECK(free_slots(gold, gold_size) == OVL_TEX_SLOTS,
        "every image's slot free again");

  p_vkDeviceWaitIdle(device);
  ovl_vk_shutdown();
  free(font);
  free(icon);
  free(gold);
  free(plat);
}

int main(void) {
  void *lib = dlopen("libvulkan.so.1", RTLD_NOW);
  VkApplicationInfo app;
  VkInstanceCreateInfo instance_info;
  VkInstance instance;
  VkPhysicalDevice physical;
  VkPhysicalDeviceProperties props;
  VkPhysicalDeviceVulkan13Features features13;
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
  app.apiVersion = VK_API_VERSION_1_3;
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
  LOADI(vkGetPhysicalDeviceProperties);
  LOADI(vkCreateDevice);
  get_device_proc = (PFN_vkGetDeviceProcAddr)get_instance_proc(
      instance, "vkGetDeviceProcAddr");
  if (p_vkEnumeratePhysicalDevices(instance, &count, &physical) < 0 || !count) {
    printf("overlay_device: no device, skipped\n");
    return 0;
  }
  p_vkGetPhysicalDeviceProperties(physical, &props);
  if (props.apiVersion < VK_API_VERSION_1_3) {
    printf("overlay_device: no Vulkan 1.3 device, skipped\n");
    return 0;
  }
  p_vkGetPhysicalDeviceMemoryProperties(physical, &mem);
  memset(&features13, 0, sizeof(features13));
  features13.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
  features13.dynamicRendering = VK_TRUE;
  memset(&queue_info, 0, sizeof(queue_info));
  queue_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
  queue_info.queueCount = 1;
  queue_info.pQueuePriorities = &priority;
  memset(&device_info, 0, sizeof(device_info));
  device_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
  device_info.pNext = &features13;
  device_info.queueCreateInfoCount = 1;
  device_info.pQueueCreateInfos = &queue_info;
  if (p_vkCreateDevice(physical, &device_info, NULL, &device) != VK_SUCCESS) {
    printf("overlay_device: no device with dynamic rendering, skipped\n");
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
  LOADD(vkCreateCommandPool);
  LOADD(vkAllocateCommandBuffers);
  LOADD(vkBeginCommandBuffer);
  LOADD(vkEndCommandBuffer);
  LOADD(vkCmdPipelineBarrier);
  LOADD(vkCmdClearColorImage);
  LOADD(vkCmdCopyImageToBuffer);
  LOADD(vkQueueSubmit);
  LOADD(vkQueueWaitIdle);
  LOADD(vkDeviceWaitIdle);
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

  run();

  if (failures) {
    printf("overlay_device: %d failed\n", failures);
    return 1;
  }
  printf("overlay_device: all passed\n");
  return 0;
}
