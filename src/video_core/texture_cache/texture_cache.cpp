// SPDX-FileCopyrightText: Copyright 2024-2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <string.h>

#include <encodings/xxh3.h>

#include <retro_atomic.h>
#include <streams/file_stream.h>
#include "common/alignment.h"
#include "common/assert.h"
#include "common/div_ceil.h"
#include "common/hash.h"
#include "core/emulator_settings.h"
#include "core/memory.h"
#include "video_core/buffer_cache/buffer_cache.h"
#include "video_core/page_manager.h"
#include "video_core/renderer_vulkan/frame_notes.h"
#include "video_core/renderer_vulkan/liverpool_to_vk.h"
#include "video_core/renderer_vulkan/vk_instance.h"
#include "video_core/renderer_vulkan/vk_runtime.h"
#include "video_core/renderer_vulkan/vk_scheduler.h"
#include "video_core/texture_cache/host_compatibility.h"
#include "video_core/texture_cache/texture_cache.h"
#include "video_core/texture_cache/tile_manager.h"

namespace VideoCore {

static constexpr u32 MAX_IMAGES = 0xffff;
static constexpr u32 MAX_IMAGE_VIEWS = 0xffff;
static constexpr u32 MAX_SAMPLERS = 0xffff;

/// The copy of mip @mip of @image between it and its layout in the guest's memory (offset 0),
/// all its layers, the stencil aspect left out
static VkBufferImageCopy MipCopy(const Image& image, u32 mip) {
    const u32 width = Common::Max(image.info.size.width >> mip, 1u);
    const u32 height = Common::Max(image.info.size.height >> mip, 1u);
    const u32 depth =
        image.info.props.is_volume ? Common::Max(image.info.size.depth >> mip, 1u) : 1u;
    const MipInfo& layout = image.info.mips_layout[mip];
    const u32 mip_pitch = layout.pitch;
    const u32 mip_height = layout.height;
    const u32 extent_width = mip_pitch ? Common::Min<u32>(mip_pitch, width) : width;
    const u32 extent_height = mip_height ? Common::Min<u32>(mip_height, height) : height;
    VkBufferImageCopy copy{};
    copy.bufferOffset = layout.offset;
    copy.bufferRowLength = mip_pitch;
    copy.bufferImageHeight = mip_height;
    copy.imageSubresource.aspectMask =
        static_cast<VkImageAspectFlags>(image.aspect_mask) & ~VK_IMAGE_ASPECT_STENCIL_BIT;
    copy.imageSubresource.mipLevel = mip;
    copy.imageSubresource.baseArrayLayer = 0;
    copy.imageSubresource.layerCount = image.info.resources.layers;
    copy.imageOffset = {0, 0, 0};
    copy.imageExtent = {extent_width, extent_height, depth};
    return copy;
}

static u64 HashBackingMemory(VAddr virtual_addr, u64 size) {
    static constexpr u8 zeros[Core::GUEST_PAGE_SIZE]{};
    xxh3_state_t state;
    const Core::AddressSpace::BackingPageTable& backing_pages =
        Core::Memory::Instance()->GetAddressSpace().BackingPages();
    xxh3_reset(&state, 0);
    while (size) {
        const u64 page = virtual_addr >> Core::GUEST_PAGE_BITS;
        const u64 offset_in_page = virtual_addr & (Core::GUEST_PAGE_SIZE - 1);
        const u64 hash_size = Common::Min<u64>(Core::GUEST_PAGE_SIZE - offset_in_page, size);
        if (u8* const* entry = backing_pages.find(page); entry && *entry) {
            xxh3_update(&state, *entry + offset_in_page, hash_size);
        } else {
            xxh3_update(&state, zeros, hash_size);
        }
        size -= hash_size;
        virtual_addr += hash_size;
    }
    return xxh3_digest(&state);
}

TextureCache::TextureCache(const Vulkan::Instance& instance_, Vulkan::Scheduler& scheduler_,
                           Vulkan::Runtime& runtime_, AmdGpu::Liverpool* liverpool_,
                           BufferCache& buffer_cache_, PageManager& page_manager_)
    : instance{instance_}, scheduler{scheduler_}, runtime{runtime_}, liverpool{liverpool_},
      buffer_cache{buffer_cache_}, page_manager{page_manager_}, slot_images{MAX_IMAGES},
      slot_image_views{MAX_IMAGE_VIEWS}, slot_samplers{MAX_SAMPLERS},
      blit_helper{instance, scheduler},
      tile_manager{instance, scheduler, runtime, buffer_cache.GetStreamBuffer()},
      readback_linear_images{emu_settings_get_readback_linear_images_enabled()} {

    u32 max_samplers = instance.GetMaxSamplerAllocationCount();

    // Made once at the most the GPU can hold, so no frame ever grows them; the other two
    // double only past what a frame normally holds.
    static_assert(__is_trivially_copyable(MetaDataInfo));
    u64_map_init(&samplers, sizeof(SamplerId), Common::Min(max_samplers, MAX_SAMPLERS));
    u64_map_init(&download_images, 0, 256);
    u64_map_init(&surface_metas, sizeof(MetaDataInfo), 4096);
    trigger_gc_samplers = max_samplers * 3 / 4;
    pressure_gc_samplers = max_samplers * 7 / 8;
    critical_gc_samplers = max_samplers * 15 / 16;

    // Set up garbage collection parameters.
    if (!instance.CanReportMemoryUsage()) {
        trigger_gc_memory = 0;
        pressure_gc_memory = DEFAULT_PRESSURE_GC_MEMORY;
        critical_gc_memory = DEFAULT_CRITICAL_GC_MEMORY;
        return;
    }

    const s64 device_local_memory = static_cast<s64>(instance.GetTotalMemoryBudget());
    const s64 min_spacing_expected = device_local_memory - 1_GB;
    const s64 min_spacing_critical = device_local_memory - 512_MB;
    const s64 mem_threshold = Common::Min<s64>(device_local_memory, TARGET_GC_THRESHOLD);
    const s64 min_vacancy_expected = (6 * mem_threshold) / 10;
    const s64 min_vacancy_critical = (2 * mem_threshold) / 10;
    const s64 min_pressure_floor = Common::Min<s64>(
        Common::Max<s64>(device_local_memory / 4, 256_MB), DEFAULT_PRESSURE_GC_MEMORY);
    const s64 min_critical_floor = Common::Min<s64>(
        Common::Max<s64>(device_local_memory / 2, 512_MB), DEFAULT_CRITICAL_GC_MEMORY);
    pressure_gc_memory = static_cast<u64>(Common::Max<s64>(
        Common::Min(device_local_memory - min_vacancy_expected, min_spacing_expected),
        min_pressure_floor));
    critical_gc_memory = static_cast<u64>(Common::Max<s64>(
        Common::Min(device_local_memory - min_vacancy_critical, min_spacing_critical),
        min_critical_floor));
    trigger_gc_memory = static_cast<u64>((device_local_memory - mem_threshold) / 2);
}

TextureCache::~TextureCache() {
    u64_map_free(&samplers);
    u64_map_free(&download_images);
    u64_map_free(&surface_metas);
}

void TextureCache::ProcessDownloadImages() {
    download_images_combiner.Run([&] {
        size_t pos = 0;
        u64 index;
        while (u64_map_next(&download_images, &pos, &index)) {
            DownloadImageMemory(ImageId{static_cast<u32>(index)}, DownloadMemoryFlags::Sync);
        }
        u64_map_clear(&download_images);
    });
}

bool TextureCache::DumpImagePixels(ImageId image_id, u32 seq) {
    Image& image = slot_images[image_id];
    const ImageInfo& info = image.info;
    if (!image.backing || info.num_samples != 1 || info.props.is_block || info.props.is_volume ||
        False(image.flags & ImageFlagBits::GpuModified)) {
        return false;
    }
    const bool is_depth = info.props.is_depth;
    const u32 bpp =
        is_depth ? (info.pixel_format == VK_FORMAT_D16_UNORM ? 2 : 4) : info.num_bits / 8;
    const u32 width = image.HostSize().width;
    const u32 height = image.HostSize().height;
    if (bpp == 0 || bpp > 16 || width == 0 || height == 0) {
        return false;
    }
    const u32 step = (Common::Max(width, height) + 127) / 128;
    char path[4096];
    if (!frame_notes_image_path(path, sizeof(path), seq,
                                static_cast<unsigned long>(info.guest_address >> 32),
                                static_cast<unsigned long>(info.guest_address & 0xffffffffu),
                                static_cast<unsigned>(info.pixel_format), width, height, step)) {
        return false;
    }
    // Room to spare: a depth and stencil format counts more bytes for a texel than are copied
    const Vulkan::StagingBufferRef download = runtime.GetStagingPool().Request(
        u64(width) * height * (is_depth ? 8 : bpp), MemoryType::HostCached, 16, true);
    VkBufferImageCopy copy{};
    copy.bufferOffset = download.offset;
    copy.imageSubresource.aspectMask =
        is_depth ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
    copy.imageSubresource.mipLevel = 0;
    copy.imageSubresource.baseArrayLayer = 0;
    copy.imageSubresource.layerCount = 1;
    copy.imageOffset = {0, 0, 0};
    copy.imageExtent = {width, height, 1};
    runtime.DownloadImage(&image, download.buffer, &copy, 1);

    // Written once the GPU has done the copy, in its own time. Waiting for it here would end
    // the command buffer after every image: a frame of a hundred of them then counts as a
    // hundred frames to everything that ages by submissions, and images still in use are
    // taken for stale and dropped (Sekiro lost its picture for good after a written frame).
    // Opened now, under the name the frame has now, and written when the copy is done
    RFILE* const file =
        filestream_open(path, RETRO_VFS_FILE_ACCESS_WRITE, RETRO_VFS_FILE_ACCESS_HINT_NONE);
    scheduler.DeferOperation([this, download, width, height, bpp, step, file] {
        download.Invalidate();
        if (file) {
            u8 row[256 * 16];
            for (u32 y = 0; y < height; y += step) {
                u32 used = 0;
                for (u32 x = 0; x < width; x += step) {
                    memcpy(row + used, download.mapped + (u64(y) * width + x) * bpp, bpp);
                    used += bpp;
                }
                filestream_write(file, row, used);
            }
            filestream_close(file);
        }
        runtime.GetStagingPool().FreeDeferred(download);
    });
    return true;
}

void TextureCache::DownloadImageMemory(ImageId image_id, DownloadMemoryFlags flags) {
    Image& image = slot_images[image_id];
    if (False(image.flags & ImageFlagBits::GpuModified) || image.IsScaled()) {
        return;
    }
    const VAddr image_addr = image.info.guest_address;
    const u32 image_mips = image.info.resources.levels;
    const bool is_sync = True(flags & DownloadMemoryFlags::Sync);
    const bool is_priority = True(flags & DownloadMemoryFlags::Priority);
    const bool discard_buffer_cache = True(flags & DownloadMemoryFlags::DiscardBufferCache);
    ASSERT(!(is_sync && is_priority));

    u32 copy_size = 0;
    Common::SmallVector<VkBufferImageCopy, 8> buffer_copies;
    for (u32 mip = 0; mip < image_mips; ++mip) {
        buffer_copies.push_back(MipCopy(image, mip));
        copy_size += image.info.mips_layout[mip].size;
    }
    if (buffer_copies.empty()) {
        return;
    }
    ASSERT(copy_size <= image.info.guest_size);

    // Straight into the game's memory where it is lent to the GPU (guest_import.h). The tiler
    // writes all of the image's memory, so only when all of it is read back; a copy writes the
    // levels' texels, at offsets a texel's size divides.
    const bool tiled = image.info.props.is_tiled;
    if ((!tiled || copy_size == image.info.guest_size) &&
        Common::HasSingleBit(image.info.num_bits)) {
        const u64 alignment = tiled ? Common::Max<u64>(256, instance.StorageMinAlignment()) : 256;
        u64 target_offset = 0;
        const Buffer* target = buffer_cache.GuestTarget(
            image_addr, tiled ? image.info.guest_size : copy_size, alignment, &target_offset);
        if (target) {
            // Landing when the GPU is done: until then the CPU waits for it there
            const int writeback =
                is_sync ? -1
                        : buffer_cache.BeginWriteback(image_addr,
                                                      tiled ? image.info.guest_size : copy_size);
            tile_manager.TileImage(image, buffer_copies.data(),
                                   static_cast<u32>(buffer_copies.size()), target, target_offset);
            if (is_sync) {
                scheduler.Finish();
                EndImageWriteback(image_addr, copy_size, discard_buffer_cache, writeback);
            } else if (is_priority) {
                scheduler.DeferPriorityOperation(
                    [this, image_addr, copy_size, discard_buffer_cache, writeback] {
                        EndImageWriteback(image_addr, copy_size, discard_buffer_cache, writeback);
                    });
            } else {
                scheduler.DeferOperation(
                    [this, image_addr, copy_size, discard_buffer_cache, writeback] {
                        EndImageWriteback(image_addr, copy_size, discard_buffer_cache, writeback);
                    });
            }
            return;
        }
    }

    const int writeback = is_sync ? -1 : buffer_cache.BeginWriteback(image_addr, copy_size);
    const Vulkan::StagingBufferRef download =
        runtime.GetStagingPool().Request(copy_size, MemoryType::HostCached, 16, !is_sync);

    tile_manager.TileImage(image, buffer_copies.data(), static_cast<u32>(buffer_copies.size()),
                           download.buffer, download.offset);

    if (is_sync) {
        scheduler.Finish();
        LandImageDownload(image_addr, copy_size, download, discard_buffer_cache, true, writeback);
    } else if (is_priority) {
        scheduler.DeferPriorityOperation(
            [this, image_addr, copy_size, download, discard_buffer_cache, writeback] {
                LandImageDownload(image_addr, copy_size, download, discard_buffer_cache, false,
                                  writeback);
            });
    } else {
        scheduler.DeferOperation(
            [this, image_addr, copy_size, download, discard_buffer_cache, writeback] {
                LandImageDownload(image_addr, copy_size, download, discard_buffer_cache, false,
                                  writeback);
            });
    }
}

void TextureCache::EndImageWriteback(VAddr image_addr, u32 copy_size, bool discard_buffer_cache,
                                     int writeback) {
    if (discard_buffer_cache) {
        buffer_cache.UnmarkRegionAsGpuModified(image_addr, copy_size, true);
    }
    buffer_cache.EndWriteback(writeback);
}

void TextureCache::LandImageDownload(VAddr image_addr, u32 copy_size,
                                     const Vulkan::StagingBufferRef& download,
                                     bool discard_buffer_cache, bool is_sync, int writeback) {
    download.Invalidate();
    Core::Memory::Instance()->TryWriteBacking(reinterpret_cast<u8*>(image_addr), download.mapped,
                                              copy_size);
    if (discard_buffer_cache) {
        buffer_cache.UnmarkRegionAsGpuModified(image_addr, copy_size, true);
    }
    if (!is_sync) {
        runtime.GetStagingPool().FreeDeferred(download);
    }
    buffer_cache.EndWriteback(writeback);
}

void TextureCache::MarkAsMaybeDirty(ImageId image_id, Image& image) {
    if (image.cpu_hash == 0) {
        // Initialize hash
        const u8* addr = reinterpret_cast<const u8*>(image.info.guest_address);
        image.cpu_hash = xxh3_64(addr, image.info.guest_size, 0);
    }
    image.flags |= ImageFlagBits::MaybeCpuDirty;
    UntrackImage(image_id);
}

void TextureCache::InvalidateMemory(VAddr addr, size_t size) {
    const VAddr pages_start = PageManager::GetPageAddr(addr);
    const VAddr pages_end = PageManager::GetNextPageAddr(addr + size - 1);

    Common::SmallVector<ImageId, 8> image_ids;
    ForEachImageInRegion(pages_start, pages_end - pages_start,
                         [&](ImageId image_id, Image&) { image_ids.push_back(image_id); });

    for (const ImageId image_id : image_ids) {
        Image& image = slot_images[image_id];
        image.combiner.Run([&] {
            const VAddr image_begin = image.info.guest_address;
            const VAddr image_end = image.info.guest_address + image.info.guest_size;
            if (image.Overlaps(addr, size)) {
                // Modified region overlaps image, so the image was definitely accessed by this
                // fault. Untrack the image, so that the range is unprotected and the guest can
                // write freely.
                image.flags |= ImageFlagBits::CpuDirty;
                UntrackImage(image_id);
            } else if (pages_end < image_end) {
                // This page access may or may not modify the image.
                // We should not mark it as dirty now. If it really was modified
                // it will receive more invalidations on its other pages.
                // Remove tracking from this page only.
                UntrackImageHead(image_id);
            } else if (image_begin < pages_start) {
                // This page access does not modify the image but the page should be untracked.
                // We should not mark this image as dirty now. If it really was modified
                // it will receive more invalidations on its other pages.
                UntrackImageTail(image_id);
            } else {
                // Image begins and ends on this page so it can not receive any more invalidations.
                // We will check it's hash later to see if it really was modified.
                MarkAsMaybeDirty(image_id, image);
            }
        });
    }
}

void TextureCache::InvalidateMemoryFromGPU(VAddr address, size_t max_size) {
    ForEachImageInRegion(address, max_size, [&](ImageId image_id, Image& image) {
        // Only consider images that match base address.
        // TODO: Maybe also consider subresources
        if (image.info.guest_address != address) {
            return;
        }
        // Ensure image is reuploaded when accessed again.
        image.flags |= ImageFlagBits::GpuDirty;
    });
}

void TextureCache::MarkAsMaybeGpuDirty(VAddr addr, size_t size) {
    ForEachImageInRegion(addr, size, [&](ImageId image_id, Image& image) {
        if (image.gpu_hash == 0) {
            // Read backing pages directly, without faulting through the guest readback path.
            image.gpu_hash = HashBackingMemory(image.info.guest_address, image.info.guest_size);
        }
    });
}

void TextureCache::UnmapMemory(VAddr cpu_addr, size_t size) {
    Common::SmallVector<ImageId, 16> deleted_images;
    ForEachImageInRegion(cpu_addr, size, [&](ImageId id, Image&) { deleted_images.push_back(id); });
    for (const ImageId id : deleted_images) {
        // TODO: Download image data back to host.
        FreeImage(id);
    }
}

ImageId TextureCache::ResolveDepthOverlap(const ImageInfo& requested_info, BindingType binding,
                                          ImageId cache_image_id) {
    Image& cache_image = slot_images[cache_image_id];

    if (!cache_image.info.props.is_depth && !requested_info.props.is_depth) {
        return {};
    }

    const bool stencil_match =
        requested_info.props.has_stencil == cache_image.info.props.has_stencil;
    const bool bpp_match = requested_info.num_bits == cache_image.info.num_bits;

    // If an image in the cache has less slices we need to expand it
    bool recreate = cache_image.info.resources < requested_info.resources;

    switch (binding) {
    case BindingType::Texture:
        // The guest requires a depth sampled texture, but cache can offer only Rxf. Need to
        // recreate the image.
        recreate |= requested_info.props.is_depth && !cache_image.info.props.is_depth;
        break;
    case BindingType::Storage:
        // If the guest is going to use previously created depth as storage, the image needs to be
        // recreated. (TODO: Probably a case with linear rgba8 aliasing is legit)
        recreate |= cache_image.info.props.is_depth;
        break;
    case BindingType::RenderTarget:
        // Render target can have only Rxf format. If the cache contains only Dx[S8] we need to
        // re-create the image.
        ASSERT(!requested_info.props.is_depth);
        recreate |= cache_image.info.props.is_depth;
        break;
    case BindingType::DepthTarget:
        // The guest has requested previously allocated texture to be bound as a depth target.
        // In this case we need to convert Rx float to a Dx[S8] as requested
        recreate |= !cache_image.info.props.is_depth;

        // The guest is trying to bind a depth target and cache has it. Need to be sure that aspects
        // and bpp match
        recreate |= cache_image.info.props.is_depth && !(stencil_match && bpp_match);
        break;
    default:
        break;
    }

    if (recreate) {
        ImageInfo new_info = requested_info;
        // The larger of the two, as Common::Max took it: the requested one when they are equal
        new_info.resources = requested_info.resources < cache_image.info.resources
                                 ? cache_image.info.resources
                                 : requested_info.resources;
        new_info.UpdateSize();
        const ImageId new_image_id =
            slot_images.Insert(instance, runtime, slot_image_views, new_info);
        RegisterImage(new_image_id);

        // Inherit image usage
        Image& new_image = slot_images[new_image_id];
        new_image.usage = cache_image.usage;
        if (new_info.num_samples == 1 &&
            (new_info.resources.layers > cache_image.info.resources.layers ||
             new_info.resources.levels > cache_image.info.resources.levels)) {
            RefreshImage(new_image);
        }
        new_image.flags &= ~ImageFlagBits::Dirty;
        // When creating a depth buffer through overlap resolution don't clear it on first use.
        new_image.info.meta_info.htile_clear_mask = 0;
        runtime.CopyColorAndDepth(&cache_image, &new_image);

        // Free the cache image.
        FreeImage(cache_image_id);
        return new_image_id;
    }

    // Will be handled by view
    return cache_image_id;
}

TextureCache::OverlapResult TextureCache::ResolveOverlap(const ImageInfo& image_info,
                                                         BindingType binding,
                                                         ImageId cache_image_id,
                                                         ImageId merged_image_id) {
    static constexpr u64 NUM_FRAMES_BEFORE_REMOVAL = 32;

    Image& cache_image = slot_images[cache_image_id];
    const bool safe_to_delete =
        scheduler.CurrentTick() - cache_image.tick_accessed_last > NUM_FRAMES_BEFORE_REMOVAL;

    // Equal address
    if (image_info.guest_address == cache_image.info.guest_address) {
        const u32 lhs_block_size = image_info.num_bits * image_info.num_samples;
        const u32 rhs_block_size = cache_image.info.num_bits * cache_image.info.num_samples;
        if (image_info.BlockDim() != cache_image.info.BlockDim() ||
            lhs_block_size != rhs_block_size) {
            // Very likely this kind of overlap is caused by allocation from a pool.
            if (safe_to_delete) {
                FreeImage(cache_image_id);
            }
            return {merged_image_id, -1, -1};
        }

        if (const ImageId depth_image_id =
                ResolveDepthOverlap(image_info, binding, cache_image_id)) {
            return {depth_image_id, -1, -1};
        }

        // Compressed view of uncompressed image with same block size.
        if (image_info.props.is_block && !cache_image.info.props.is_block) {
            return {ExpandImage(image_info, cache_image_id), -1, -1};
        }

        if (image_info.guest_size == cache_image.info.guest_size &&
            (image_info.type == AmdGpu::ImageType::Color3D ||
             cache_image.info.type == AmdGpu::ImageType::Color3D)) {
            return {ExpandImage(image_info, cache_image_id), -1, -1};
        }

        const bool pow2_padding_only =
            image_info.props.is_pow2 != cache_image.info.props.is_pow2 &&
            image_info.tile_mode == cache_image.info.tile_mode &&
            image_info.size == cache_image.info.size &&
            image_info.pitch == cache_image.info.pitch && image_info.resources.levels == 1 &&
            cache_image.info.resources.levels == 1 && image_info.resources.layers == 1 &&
            cache_image.info.resources.layers == 1;

        // Size and resources are less than or equal, use image view.
        if (image_info.pixel_format != cache_image.info.pixel_format ||
            image_info.guest_size <= cache_image.info.guest_size || pow2_padding_only) {
            const ImageId result_id = merged_image_id ? merged_image_id : cache_image_id;
            const Image& result_image = slot_images[result_id];
            const bool is_compatible =
                IsVulkanFormatCompatible(result_image.info.pixel_format, image_info.pixel_format);
            return {is_compatible ? result_id : ImageId{}, -1, -1};
        }

        // Size and resources are greater, expand the image.
        if (image_info.type == cache_image.info.type &&
            image_info.resources > cache_image.info.resources) {
            return {ExpandImage(image_info, cache_image_id), -1, -1};
        }

        // Size is greater but resources are not, because the tiling mode is different.
        // Likely the address is reused for a image with a different tiling mode or alternate
        // tiling: Shadow of the Colossus asks for its 1920x1080 target again with the alternate
        // tiling, which is larger in the game's memory (as its PC port has it)
        if (image_info.tile_mode != cache_image.info.tile_mode ||
            image_info.alt_tile != cache_image.info.alt_tile) {
            if (safe_to_delete) {
                FreeImage(cache_image_id);
            }
            return {merged_image_id, -1, -1};
        }

        // Enhanced debug logging for unreachable case
        // Calculate expected size based on format and dimensions
        u64 expected_size =
            (static_cast<u64>(image_info.size.width) * static_cast<u64>(image_info.size.height) *
             static_cast<u64>(image_info.size.depth) * static_cast<u64>(image_info.num_bits) / 8);
        LOG_ERROR(
            Render_Vulkan,
            "Unresolvable image overlap with equal memory address:\n"
            "=== OLD IMAGE (cached) ===\n"
            "  Address:        0x%" PRIxPTR "\n"
            "  Size:           0x%x bytes\n"
            "  Format:         %d\n"
            "  Type:           %d\n"
            "  Width:          %u\n"
            "  Height:         %u\n"
            "  Depth:          %u\n"
            "  Pitch:          %u\n"
            "  Mip levels:     %u\n"
            "  Array layers:   %u\n"
            "  Samples:        %u\n"
            "  Tile mode:      0x%x\n"
            "  Block size:     %u bits\n"
            "  Is block-comp:  %d\n"
            "  Guest size:     0x%x\n"
            "  Last accessed:  tick %" PRIu64 "\n"
            "  Safe to delete: %s\n"
            "  isPow2:         %s\n"
            "  Alt tile:       %s\n"
            "\n"
            "=== NEW IMAGE (requested) ===\n"
            "  Address:        0x%" PRIxPTR "\n"
            "  Size:           0x%x bytes\n"
            "  Format:         %d\n"
            "  Type:           %d\n"
            "  Width:          %u\n"
            "  Height:         %u\n"
            "  Depth:          %u\n"
            "  Pitch:          %u\n"
            "  Mip levels:     %u\n"
            "  Array layers:   %u\n"
            "  Samples:        %u\n"
            "  Tile mode:      0x%x\n"
            "  Block size:     %u bits\n"
            "  Is block-comp:  %u\n"
            "  Guest size:     0x%x\n"
            "  isPow2:         %s\n"
            "  Alt tile:       %s\n"
            "\n"
            "=== COMPARISON ===\n"
            "  Same format:           %s\n"
            "  Same type:             %s\n"
            "  Same tile mode:        %s\n"
            "  Same block size:       %s\n"
            "  Same BlockDim:         %s\n"
            "  Same pitch:            %s\n"
            "  Same pow2:             %s\n"
            "  Same alt tile:         %s\n"
            "  Old resources <= new:  %s (old: %u, new: %u)\n"
            "  Old size <= new size:  %s\n"
            "  Expected size (calc):  %" PRIu64 " bytes\n"
            "  Size ratio (new/expected): %.2fx\n"
            "  Size ratio (new/old):  %.2fx\n"
            "  Old vs expected diff:  %" PRId64 " bytes (%+.2f%%)\n"
            "  New vs expected diff:  %" PRId64 " bytes (%+.2f%%)\n"
            "  Merged image ID:       %u\n"
            "  Binding type:          %d\n"
            "  Current tick:          %" PRIu64 "\n"
            "  Age (ticks since last access): %" PRIu64,

            // Old image details
            cache_image.info.guest_address, cache_image.info.guest_size,
            static_cast<int>(cache_image.info.pixel_format),
            static_cast<int>(cache_image.info.type), cache_image.info.size.width,
            cache_image.info.size.height, cache_image.info.size.depth, cache_image.info.pitch,
            cache_image.info.resources.levels, cache_image.info.resources.layers,
            cache_image.info.num_samples, static_cast<u32>(cache_image.info.tile_mode),
            cache_image.info.num_bits, +cache_image.info.props.is_block,
            cache_image.info.guest_size, cache_image.tick_accessed_last,
            (safe_to_delete) ? "true" : "false",
            (bool(cache_image.info.props.is_pow2)) ? "true" : "false",
            (cache_image.info.alt_tile) ? "true" : "false", // New image details
            image_info.guest_address, image_info.guest_size,
            static_cast<int>(image_info.pixel_format), static_cast<int>(image_info.type),
            image_info.size.width, image_info.size.height, image_info.size.depth, image_info.pitch,
            image_info.resources.levels, image_info.resources.layers, image_info.num_samples,
            static_cast<u32>(image_info.tile_mode), image_info.num_bits, image_info.props.is_block,
            image_info.guest_size, (bool(image_info.props.is_pow2)) ? "true" : "false",
            (image_info.alt_tile) ? "true" : "false",
            ( // Comparison
                (image_info.pixel_format == cache_image.info.pixel_format))
                ? "true"
                : "false",
            ((image_info.type == cache_image.info.type)) ? "true" : "false",
            ((image_info.tile_mode == cache_image.info.tile_mode)) ? "true" : "false",
            ((image_info.num_bits == cache_image.info.num_bits)) ? "true" : "false",
            ((image_info.BlockDim() == cache_image.info.BlockDim())) ? "true" : "false",
            ((image_info.pitch == cache_image.info.pitch)) ? "true" : "false",
            ((image_info.props.is_pow2 == cache_image.info.props.is_pow2)) ? "true" : "false",
            ((image_info.alt_tile == cache_image.info.alt_tile)) ? "true" : "false",
            ((cache_image.info.resources <= image_info.resources)) ? "true" : "false",
            cache_image.info.resources.levels, image_info.resources.levels,
            ((cache_image.info.guest_size <= image_info.guest_size)) ? "true" : "false",
            expected_size, // Size ratios
            static_cast<double>(image_info.guest_size) / expected_size,
            static_cast<double>(image_info.guest_size) /
                cache_image.info
                    .guest_size, // Difference between actual and expected sizes with percentages
            static_cast<s64>(cache_image.info.guest_size) - static_cast<s64>(expected_size),
            (static_cast<double>(cache_image.info.guest_size) / expected_size - 1.0) * 100.0,
            static_cast<s64>(image_info.guest_size) - static_cast<s64>(expected_size),
            (static_cast<double>(image_info.guest_size) / expected_size - 1.0) * 100.0,
            merged_image_id.index, static_cast<int>(binding), scheduler.CurrentTick(),
            scheduler.CurrentTick() - cache_image.tick_accessed_last);

        UNREACHABLE_MSG("Encountered unresolvable image overlap with equal memory address.");
    }

    // Right overlap, the image requested is a possible subresource of the image from cache.
    if (image_info.guest_address > cache_image.info.guest_address) {
        if (const s32 mip = image_info.MipOf(cache_image.info); mip >= 0) {
            if (const s32 slice = image_info.SliceOf(cache_image.info, mip); slice >= 0) {
                return {cache_image_id, mip, slice};
            }
        }

        // Image isn't a subresource but a chance overlap.
        if (safe_to_delete) {
            FreeImage(cache_image_id);
        }

        return {{}, -1, -1};
    } else {
        // Left overlap, the image from cache is a possible subresource of the image requested
        if (const s32 mip = cache_image.info.MipOf(image_info); mip >= 0) {
            if (const s32 slice = cache_image.info.SliceOf(image_info, mip); slice >= 0) {
                // We have a larger image created and a separate one, representing a subres of it
                // bound as render target. In this case we need to rebind render target.
                if (cache_image.binding.is_target) {
                    cache_image.binding.needs_rebind = 1u;
                    if (merged_image_id) {
                        GetImage(merged_image_id).binding.is_target = 1u;
                    }

                    FreeImage(cache_image_id);
                    return {merged_image_id, -1, -1};
                }

                // If we don't have an allocated image, we create one here
                // Otherwise it's later created from guest memory and the GPU data is lost
                // And reads/writes go to different images.
                if (!merged_image_id) {
                    merged_image_id =
                        slot_images.Insert(instance, runtime, slot_image_views, image_info);
                    RegisterImage(merged_image_id);
                    RefreshImage(slot_images[merged_image_id]);
                    TrackImage(merged_image_id);
                }
                Image& merged_image = slot_images[merged_image_id];
                runtime.CopyMip(&cache_image, &merged_image, mip, slice);
                FreeImage(cache_image_id);
            }
        }
    }

    return {merged_image_id, -1, -1};
}

ImageId TextureCache::ExpandImage(const ImageInfo& info, ImageId image_id) {
    const ImageId new_image_id = slot_images.Insert(instance, runtime, slot_image_views, info);
    RegisterImage(new_image_id);

    Image& src_image = slot_images[image_id];
    Image& new_image = slot_images[new_image_id];

    // Only carry over the old contents when they hold GPU-written data that guest memory lacks.
    // A CPU-dirty source is stale (e.g. a streamed texture rewritten in a reused pool slot);
    // copying it would overwrite the fresh upload with old data.
    const bool src_has_gpu_data = src_image.HasGpuValidData();

    RefreshImage(new_image);

    if (src_image.binding.is_bound || src_image.binding.is_target) {
        src_image.binding.needs_rebind = 1u;
    }

    TrackImage(new_image_id);
    new_image.flags &= ~ImageFlagBits::Dirty;
    if (src_has_gpu_data) {
        runtime.CopyImage(&src_image, &new_image);
    }
    FreeImage(image_id);

    return new_image_id;
}

ImageId TextureCache::FindImage(ImageDesc& desc, bool exact_fmt) {
    const ImageInfo& info = desc.info;

    ASSERT(info.guest_address != 0);

    if (desc.type == BindingType::VideoOut) {
        frame_extent = info.size;
    }

    Common::SmallVector<ImageId, 8> image_ids;
    ForEachImageInRegion(info.guest_address, info.guest_size,
                         [&](ImageId image_id, Image& image) { image_ids.push_back(image_id); });

    ImageId image_id{};

    // Check for a perfect match first
    for (const ImageId cache_id : image_ids) {
        Image& cache_image = slot_images[cache_id];
        if (cache_image.info.guest_address != info.guest_address) {
            continue;
        }
        if (cache_image.info.guest_size != info.guest_size) {
            continue;
        }
        if (cache_image.info.size != info.size) {
            continue;
        }
        if (!IsVulkanFormatCompatible(cache_image.info.pixel_format, info.pixel_format) ||
            (cache_image.info.type != info.type && info.size != Extent3D{1, 1, 1})) {
            continue;
        }
        if (exact_fmt && info.pixel_format != cache_image.info.pixel_format) {
            continue;
        }
        image_id = cache_id;
    }

    // Try to resolve overlaps (if any)
    int view_mip{-1};
    int view_slice{-1};
    if (!image_id) {
        for (const ImageId cache_id : image_ids) {
            view_mip = -1;
            view_slice = -1;

            const ImageInfo& merged_info = image_id ? slot_images[image_id].info : info;
            const OverlapResult overlap =
                ResolveOverlap(merged_info, desc.type, cache_id, image_id);
            if (overlap.image_id) {
                image_id = overlap.image_id;
                view_mip = overlap.view_mip;
                view_slice = overlap.view_slice;
            }
        }
    }

    if (image_id) {
        Image& image_resolved = slot_images[image_id];
        if (exact_fmt && info.pixel_format != image_resolved.info.pixel_format) {
            // Cannot reuse this image as we need the exact requested format.
            image_id = {};
        } else if (image_resolved.info.resources < info.resources) {
            // The image was clearly picked up wrong.
            FreeImage(image_id);
            image_id = {};
            LOG_WARNING(Render_Vulkan, "Image overlap resolve failed");
        }
    }
    // Create and register a new image
    if (!image_id) {
        u32 scale_num = 1;
        u32 scale_den = 1;
        // Resolution below the game's: a target of the frame's size, which the game draws
        // into and samples back, and its depth buffer; one mip and layer, as the frame has
        const bool target = desc.type == BindingType::RenderTarget ||
                            desc.type == BindingType::DepthTarget ||
                            desc.type == BindingType::VideoOut;
        const u32 width = static_cast<u32>(retro_atomic_load_seq_cst_int(&render_width));
        if (target && width != 0 && width < frame_extent.width && info.size == frame_extent &&
            info.resources.levels == 1 && info.resources.layers == 1) {
            scale_num = width;
            scale_den = frame_extent.width;
        }
        image_id =
            slot_images.Insert(instance, runtime, slot_image_views, info, scale_num, scale_den);
        RegisterImage(image_id);
    }

    Image& image = slot_images[image_id];
    image.tick_accessed_last = scheduler.CurrentTick();
    TouchImage(image);

    // If the image requested is a subresource of the image from cache record its location.
    if (view_mip > 0) {
        desc.view_info.range.base.level = view_mip;
    }
    if (view_slice > 0) {
        desc.view_info.range.base.layer = view_slice;
    }

    return image_id;
}

ImageId TextureCache::FindImageFromRange(VAddr address, size_t size, bool ensure_valid) {
    Common::SmallVector<ImageId, 4> image_ids;
    ForEachImageWithAddress(address, [&](ImageId image_id, Image& image) {
        if (ensure_valid && !image.HasGpuValidData()) {
            return;
        }
        image_ids.push_back(image_id);
    });
    if (image_ids.size() == 1) {
        // Sometimes image size might not exactly match with requested buffer size
        // If we only found 1 candidate image use it without too many questions.
        Image& image = slot_images[image_ids[0]];
        TouchImage(image);
        return image_ids.back();
    }
    if (!image_ids.empty()) {
        for (s32 i = 0; i < image_ids.size(); ++i) {
            Image& image = slot_images[image_ids[i]];
            if (image.info.guest_size == size) {
                TouchImage(image);
                return image_ids[i];
            }
        }
        LOG_WARNING(Render_Vulkan,
                    "Failed to find exact image match for copy addr=0x%" PRIxPTR ", size=0x%zx",
                    address, size);
    }
    return {};
}

ImageView& TextureCache::FindTexture(ImageId image_id, const ImageDesc& desc) {
    Image& image = slot_images[image_id];
    if (desc.type == BindingType::Storage) {
        image.flags |= ImageFlagBits::GpuModified;
        if (readback_linear_images && (!image.info.props.is_tiled || image.info.size.width <= 8) &&
            image.info.guest_address != 0) {
            download_images_combiner.Run(
                [&] { u64_map_insert(&download_images, image_id.index, nullptr); });
        }
    }
    UpdateImage(image_id);
    return image.FindView(desc.view_info);
}

void TextureCache::RegisterMeta(VAddr address, MetaDataInfo info) {
    bool inserted = false;
    MetaDataInfo* slot =
        static_cast<MetaDataInfo*>(u64_map_insert(&surface_metas, address, &inserted));
    if (inserted) {
        *slot = info;
        return;
    }

    MetaDataInfo& registered = *slot;
    if (registered.type != info.type || registered.owner_uid != info.owner_uid) {
        registered = info;
        return;
    }

    // Preserve the current clear state when the same render target is rebound.
    registered.owner_id = info.owner_id;
    registered.num_samples = info.num_samples;
}

void TextureCache::UnregisterMeta(VAddr address, u64 owner_uid) {
    const MetaDataInfo* meta = FindMeta(address);
    if (meta && meta->owner_uid == owner_uid) {
        u64_map_erase(&surface_metas, address);
    }
}

bool TextureCache::DrawnLastFrame(VAddr address) {
    bool found = false;
    bool recurring = true;
    ForEachImageInRegion(address, 1, [&](ImageId, Image& image) {
        if (image.info.guest_address != address) {
            return;
        }
        found = true;
        recurring &= target_frames_recurring(&image.drawn_frames, draw_frame) != 0;
        target_frames_draw(&image.drawn_frames, draw_frame);
    });
    // No image yet: the game has never drawn into it
    return found && recurring;
}

ImageView& TextureCache::FindRenderTarget(ImageId image_id, const ImageDesc& desc) {
    Image& image = slot_images[image_id];
    image.flags |= ImageFlagBits::GpuModified;
    target_frames_draw(&image.drawn_frames, draw_frame);
    if (readback_linear_images && (!image.info.props.is_tiled || image.info.size.width <= 8)) {
        download_images_combiner.Run(
            [&] { u64_map_insert(&download_images, image_id.index, nullptr); });
    }
    image.usage.render_target = 1u;
    UpdateImage(image_id);

    // Register meta data for this color buffer
    const VAddr cmask_addr = desc.info.meta_info.cmask_addr;
    if (image.info.meta_info.cmask_addr != cmask_addr) {
        UnregisterMeta(image.info.meta_info.cmask_addr, image.image_uid);
        image.info.meta_info.cmask_addr = cmask_addr;
    }
    if (cmask_addr) {
        RegisterMeta(cmask_addr, MetaDataInfo{
                                     .type = MetaType::CMask,
                                     .owner_id = image_id,
                                     .owner_uid = image.image_uid,
                                     .num_samples = image.info.num_samples,
                                 });
    }

    const VAddr fmask_addr = desc.info.meta_info.fmask_addr;
    if (image.info.meta_info.fmask_addr != fmask_addr) {
        UnregisterMeta(image.info.meta_info.fmask_addr, image.image_uid);
        image.info.meta_info.fmask_addr = fmask_addr;
    }
    if (fmask_addr) {
        RegisterMeta(fmask_addr, MetaDataInfo{
                                     .type = MetaType::FMask,
                                     .owner_id = image_id,
                                     .owner_uid = image.image_uid,
                                     .num_samples = image.info.num_samples,
                                 });
    }

    return image.FindView(desc.view_info, false);
}

ImageView& TextureCache::FindDepthTarget(ImageId image_id, const ImageDesc& desc) {
    Image& image = slot_images[image_id];
    image.flags |= ImageFlagBits::GpuModified;
    target_frames_draw(&image.drawn_frames, draw_frame);
    image.usage.depth_target = 1u;
    UpdateImage(image_id);

    // Register meta data for this depth buffer
    const VAddr htile_addr = desc.info.meta_info.htile_addr;
    if (image.info.meta_info.htile_addr != htile_addr) {
        UnregisterMeta(image.info.meta_info.htile_addr, image.image_uid);
        image.info.meta_info.htile_addr = htile_addr;
    }
    if (htile_addr) {
        RegisterMeta(htile_addr, MetaDataInfo{
                                     .type = MetaType::HTile,
                                     .owner_id = image_id,
                                     .owner_uid = image.image_uid,
                                     .num_samples = image.info.num_samples,
                                     .clear_mask = image.info.meta_info.htile_clear_mask,
                                 });
    }

    AssociateStencil(image_id, desc.info);

    return image.FindView(desc.view_info, false);
}

void TextureCache::AssociateStencil(ImageId depth_id, const ImageInfo& depth_info) {
    if (depth_info.stencil_addr == 0 || depth_info.stencil_addr == depth_info.guest_address) {
        return;
    }
    ImageId stencil_id{};
    ForEachImageInRegion(
        depth_info.stencil_addr, depth_info.stencil_size, [&](ImageId image_id, Image& image) {
            if (image.info.guest_address != depth_info.stencil_addr) {
                return;
            }
            if (image.info.pixel_format == VK_FORMAT_UNDEFINED ||
                Vulkan::LiverpoolToVK::IsFormatStencilCompatible(image.info.pixel_format)) {
                stencil_id = image_id;
            }
        });
    if (!stencil_id) {
        ImageInfo info{};
        info.guest_address = depth_info.stencil_addr;
        info.guest_size = depth_info.stencil_size;
        info.size = depth_info.size;
        stencil_id = slot_images.Insert(instance, runtime, slot_image_views, info);
        RegisterImage(stencil_id);
    }
    Image& stencil_image = slot_images[stencil_id];
    TouchImage(stencil_image);
    stencil_image.AssociateDepth(depth_id, slot_images[depth_id].image_uid);
}

void TextureCache::RefreshImage(Image& image) {
    if (False(image.flags & ImageFlagBits::Dirty)) {
        return;
    }
    if (image.info.num_samples > 1) {
        // A multisampled image is not read from the game's memory. One nothing has drawn into
        // yet holds whatever the GPU's memory held: zero is what a shader then reads from it
        if (!image.info.props.is_depth && image.backing &&
            False(image.flags & ImageFlagBits::GpuModified)) {
            SubresourceRange range{};
            range.extent.levels = 1;
            range.extent.layers = image.info.resources.layers;
            runtime.ClearImage(&image, range, VkClearValue{});
        }
        return;
    }
    if (image.IsScaled()) {
        // Smaller than the guest's memory of it: the game's own draws fill it
        image.flags &= ~ImageFlagBits::Dirty;
        return;
    }

    if (True(image.flags & ImageFlagBits::MaybeCpuDirty) &&
        False(image.flags & ImageFlagBits::CpuDirty)) {
        // The image size should be less than page size to be considered MaybeCpuDirty
        // So this calculation should be very uncommon and reasonably fast
        // For now we'll just check up to 64 first pixels
        const u8* addr = reinterpret_cast<const u8*>(image.info.guest_address);
        const u32 w = Common::Min(image.info.size.width, u32(8));
        const u32 h = Common::Min(image.info.size.height, u32(8));

        const u32 s_w = image.info.props.is_block ? Common::DivCeil(w, 4u) : w;
        const u32 s_h = image.info.props.is_block ? Common::DivCeil(h, 4u) : h;
        const u32 size = s_w * s_h * (image.info.num_bits / 8);
        const u64 hash = xxh3_64(addr, size, 0);
        if (image.cpu_hash == hash) {
            image.flags &= ~ImageFlagBits::MaybeCpuDirty;
            return;
        }
        image.cpu_hash = hash;
    }

    const u32 num_layers = image.info.resources.layers;
    const u32 num_mips = image.info.resources.levels;
    const bool is_gpu_modified = True(image.flags & ImageFlagBits::GpuModified);
    const bool is_gpu_dirty = True(image.flags & ImageFlagBits::GpuDirty);

    Common::SmallVector<VkBufferImageCopy, 14> image_copies;
    for (u32 m = 0; m < num_mips; m++) {
        image_copies.push_back(MipCopy(image, m));
    }

    if (image_copies.empty()) {
        image.flags &= ~ImageFlagBits::Dirty;
        return;
    }

    scheduler.EndRendering();

    u64 in_offset = 0;
    const Buffer* in_buffer = buffer_cache.ObtainBufferForImage(image.info.guest_address,
                                                                image.info.guest_size, &in_offset);
    // A tiled image the GPU can write as a storage image is detiled straight into it; the rest
    // are detiled into a buffer and copied in
    if (tile_manager.DetileIntoImage(in_buffer, in_offset, image)) {
        return;
    }
    u64 offset = 0;
    const Buffer* const buffer =
        tile_manager.DetileImage(in_buffer, in_offset, image.info, &offset);
    for (VkBufferImageCopy& copy : image_copies) {
        copy.bufferOffset += offset;
    }

    runtime.UploadImage(&image, buffer, image_copies.data(), static_cast<u32>(image_copies.size()));
}

VkSampler TextureCache::GetSampler(const AmdGpu::Sampler& sharp,
                                   AmdGpu::BorderColorBuffer border_color_base,
                                   const bool is_depth) {
    // Compare and plain uses of one S# need separate samplers.
    const u64 hash = HashCombine(xxh3_64(&sharp, sizeof(sharp), 0), is_depth);
    bool new_sampler = false;
    SamplerId* id = static_cast<SamplerId*>(u64_map_insert(&samplers, hash, &new_sampler));
    if (new_sampler) {
        *id = slot_samplers.Insert(instance, sharp, border_color_base, is_depth);
        Sampler& sampler = slot_samplers[*id];
        sampler.hash = hash;
        sampler_lru_cache.Insert(sampler, gc_tick);
    }

    Sampler& sampler = slot_samplers[*id];
    sampler_lru_cache.Touch(sampler, gc_tick);
    return sampler.Handle();
}

void TextureCache::RegisterImage(ImageId image_id) {
    Image& image = slot_images[image_id];
    ASSERT_MSG(False(image.flags & ImageFlagBits::Registered),
               "Trying to register an already registered image");
    image.flags |= ImageFlagBits::Registered;
    image.memory_usage_counter = &total_used_memory;
    total_used_memory += Common::AlignUp(image.SizeBytes(), 1024);
    image_lru_cache.Insert(image, gc_tick);
    const ImageInfo& info = image.info;
    ASSERT_MSG((info.guest_address & 0xff) == 0, "Trying to register an unaligned image");
    ForEachPage(info.guest_address, info.guest_size, [this, image_id, info](u64 page) {
        page_table[page].entries.emplace_back(BucketEntry{
            .key = u32(info.guest_address >> 8),
            .size = info.guest_size,
            .id = image_id,
        });
    });
}

void TextureCache::UnregisterImage(ImageId image_id) {
    Image& image = slot_images[image_id];
    ASSERT_MSG(True(image.flags & ImageFlagBits::Registered),
               "Trying to unregister an already unregistered image");
    image.flags &= ~ImageFlagBits::Registered;
    image.memory_usage_counter = nullptr;
    image_lru_cache.Free(image);
    total_used_memory -= Common::AlignUp(image.SizeBytes(), 1024);
    ForEachPage(image.info.guest_address, image.info.guest_size, [this, image_id](u64 page) {
        Bucket* const page_it = page_table.find(page);
        ASSERT_MSG(page_it, "Unregistering unregistered page=0x%" PRIx64,
                   page << Traits::PAGE_BITS);
        Common::SmallVector<BucketEntry, 4, u32>& entries = page_it->entries;
        BucketEntry* entry = entries.begin();
        while (entry != entries.end() && entry->id != image_id) {
            ++entry;
        }
        ASSERT_MSG(entry != entries.end(), "Unregistering unregistered image in page=0x%" PRIx64,
                   page << Traits::PAGE_BITS);
        entries.erase(entry);
    });
}

void TextureCache::TrackImage(ImageId image_id) {
    Image& image = slot_images[image_id];
    if (False(image.flags & ImageFlagBits::Registered)) {
        return;
    }
    const VAddr image_begin = image.info.guest_address;
    const VAddr image_end = image.info.guest_address + image.info.guest_size;
    if (image_begin == image.track_addr && image_end == image.track_addr_end) {
        return;
    }

    image.combiner.Run([&] {
        if (image.IsUntracked()) {
            page_manager.UpdatePageWatchers(image_begin, image.info.guest_size, PageOp::Track);
        } else {
            if (image_begin < image.track_addr) {
                page_manager.UpdatePageWatchers(image_begin, image.track_addr - image_begin,
                                                PageOp::Track);
            }
            if (image.track_addr_end < image_end) {
                page_manager.UpdatePageWatchers(image.track_addr_end,
                                                image_end - image.track_addr_end, PageOp::Track);
            }
        }
        image.track_addr = image_begin;
        image.track_addr_end = image_end;
    });
}

void TextureCache::UntrackImage(ImageId image_id) {
    Image& image = slot_images[image_id];
    if (image.IsUntracked()) {
        return;
    }
    const VAddr addr = image.track_addr;
    const u64 size = image.track_addr_end - image.track_addr;
    if (size != 0) {
        page_manager.UpdatePageWatchers(addr, size, PageOp::Untrack);
    }
    image.track_addr = 0;
    image.track_addr_end = 0;
}

void TextureCache::UntrackImageHead(ImageId image_id) {
    Image& image = slot_images[image_id];
    const VAddr image_begin = image.info.guest_address;
    if (image.IsUntracked() || image_begin < image.track_addr) {
        return;
    }
    const VAddr addr = page_manager.GetNextPageAddr(image_begin);
    const u64 size = addr - image_begin;
    page_manager.UpdatePageWatchers(image_begin, size, PageOp::Untrack);

    image.track_addr = addr;
    if (image.track_addr == image.track_addr_end) {
        // This image spans only 2 pages and both are modified,
        // but the image itself was not directly affected.
        // Cehck its hash later.
        MarkAsMaybeDirty(image_id, image);
    }
}

void TextureCache::UntrackImageTail(ImageId image_id) {
    Image& image = slot_images[image_id];
    const VAddr image_end = image.info.guest_address + image.info.guest_size;
    if (image.IsUntracked() || image.track_addr_end < image_end) {
        return;
    }
    ASSERT(image.track_addr_end != 0);
    const VAddr addr = page_manager.GetPageAddr(image_end);
    const u64 size = image_end - addr;
    image.track_addr_end = addr;
    if (image.track_addr == image.track_addr_end) {
        // This image spans only 2 pages and both are modified,
        // but the image itself was not directly affected.
        // Cehck its hash later.
        MarkAsMaybeDirty(image_id, image);
    }
    page_manager.UpdatePageWatchers(addr, size, PageOp::Untrack);
}

void TextureCache::GarbageCollectImages() {
    if (instance.CanReportMemoryUsage()) {
        total_used_memory = instance.GetDeviceMemoryUsage();
    }
    if (total_used_memory < trigger_gc_memory) {
        return;
    }
    Common::SmallVector<ImageId, 8> download_queue;
    Common::SmallVector<ImageId, 8> maybe_download;

    /// One pass over the images old enough: frees, or queues for reading back, as many as the
    /// pressure on memory asks for
    struct Collect {
        TextureCache& cache;
        Common::SmallVector<ImageId, 8>& download_queue;
        Common::SmallVector<ImageId, 8>& maybe_download;
        bool pressured = false;
        bool aggresive = false;
        u64 ticks_to_destroy = 0;
        size_t num_deletions = 0;

        void Configure(bool allow_aggressive) {
            pressured = cache.total_used_memory >= cache.pressure_gc_memory;
            aggresive = allow_aggressive && cache.total_used_memory >= cache.critical_gc_memory;
            ticks_to_destroy = aggresive ? 160 : pressured ? 80 : 16;
            ticks_to_destroy = Common::Min(ticks_to_destroy, cache.gc_tick);
            num_deletions = aggresive ? 40 : pressured ? 20 : 10;
        }

        bool operator()(Image& image) {
            if (num_deletions == 0) {
                return true;
            }
            const bool download = image.HasGpuValidData();
            if (download && (!pressured || !image.CanBeDownloaded())) {
                return false;
            }
            --num_deletions;
            const ImageId image_id = cache.slot_images.GetSlotId(image);
            if (download) {
                cache.UntrackImage(image_id);
                cache.UnregisterImage(image_id);
                if (image.gpu_hash != 0) {
                    cache.buffer_cache.ReadMemory(image.info.guest_address, image.info.guest_size,
                                                  false, true, true);
                    maybe_download.push_back(image_id);
                } else {
                    download_queue.push_back(image_id);
                }
            } else {
                cache.FreeImage(image_id);
            }
            if (cache.total_used_memory < cache.critical_gc_memory) {
                if (aggresive) {
                    num_deletions >>= 2;
                    aggresive = false;
                    return false;
                }
                if (pressured && cache.total_used_memory < cache.pressure_gc_memory) {
                    num_deletions >>= 1;
                    pressured = false;
                }
            }
            return false;
        }
    } collect{*this, download_queue, maybe_download};

    // Try to remove anything old enough and not high priority.
    collect.Configure(false);
    image_lru_cache.ForEachItemBelow(gc_tick - collect.ticks_to_destroy, collect);

    if (total_used_memory >= critical_gc_memory) {
        // If we are still over the critical limit, run an aggressive GC
        collect.Configure(true);
        image_lru_cache.ForEachItemBelow(gc_tick - collect.ticks_to_destroy, collect);
    }

    if (!maybe_download.empty()) {
        // wait for memory reads to finish
        scheduler.Finish();
        scheduler.PopPendingOperations();

        for (const ImageId image_id : maybe_download) {
            const Image& image = slot_images[image_id];
            const u64 hash = HashBackingMemory(image.info.guest_address, image.info.guest_size);
            if (image.gpu_hash == hash) {
                download_queue.push_back(image_id);
            } else {
                DeleteImage(image_id);
            }
        }
    }

    for (const ImageId image_id : download_queue) {
        buffer_cache.ReadEdgeImagePages(slot_images[image_id]);
    }
    for (const ImageId image_id : download_queue) {
        DownloadImageMemory(image_id, DownloadMemoryFlags::DiscardBufferCache);
    }

    if (!download_queue.empty()) {
        // We need to make downloads synchronous. It is possible that the contents
        // of the image are requested before they are downloaded in which case
        // outdated buffer cache contents are used instead.
        scheduler.Finish();
        scheduler.PopPendingOperations();
    }

    for (const ImageId image_id : download_queue) {
        DeleteImage(image_id);
    }
}

void TextureCache::GarbageCollectSamplers() {
    total_used_samplers = samplers.count;
    if (total_used_samplers < trigger_gc_samplers) {
        return;
    }
    /// One pass over the samplers old enough: frees as many as the pressure asks for
    struct Collect {
        TextureCache& cache;
        u64 ticks_to_destroy = 0;
        size_t num_deletions = 0;

        void Configure(bool allow_aggressive) {
            const bool pressured = cache.total_used_samplers >= cache.pressure_gc_samplers;
            const bool aggresive =
                allow_aggressive && cache.total_used_samplers >= cache.critical_gc_samplers;
            ticks_to_destroy = aggresive ? 160 : pressured ? 80 : 16;
            ticks_to_destroy = Common::Min(ticks_to_destroy, cache.gc_tick);
            num_deletions = aggresive ? 40 : pressured ? 20 : 10;
        }

        bool operator()(Sampler& sampler) {
            if (num_deletions == 0) {
                return true;
            }
            --num_deletions;
            cache.sampler_lru_cache.Free(sampler);
            u64_map_erase(&cache.samplers, sampler.hash);
            const SamplerId sampler_id = cache.slot_samplers.GetSlotId(sampler);
            cache.slot_samplers.Erase(sampler_id);
            return false;
        }
    } collect{*this};

    // Try to remove anything old enough and not high priority.
    collect.Configure(false);
    sampler_lru_cache.ForEachItemBelow(gc_tick - collect.ticks_to_destroy, collect);

    if (total_used_samplers >= critical_gc_samplers) {
        // If we are still over the critical limit, run an aggressive GC
        collect.Configure(true);
        sampler_lru_cache.ForEachItemBelow(gc_tick - collect.ticks_to_destroy, collect);
    }
}

void TextureCache::RunGarbageCollector() {
    GarbageCollectImages();
    GarbageCollectSamplers();
    ++gc_tick;
}

void TextureCache::TouchImage(Image& image) {
    image_lru_cache.Touch(image, gc_tick);

    // Image is still valid
    image.gpu_hash = 0;
}

void TextureCache::DeleteImage(ImageId image_id) {
    Image& image = slot_images[image_id];
    ASSERT_MSG(image.IsUntracked(), "Image was not untracked");
    ASSERT_MSG(False(image.flags & ImageFlagBits::Registered), "Image was not unregistered");

    // Remove any registered meta areas.
    const ImageInfo::MetaInfo& meta_info = image.info.meta_info;
    if (meta_info.cmask_addr) {
        UnregisterMeta(meta_info.cmask_addr, image.image_uid);
    }
    if (meta_info.fmask_addr) {
        UnregisterMeta(meta_info.fmask_addr, image.image_uid);
    }
    if (meta_info.htile_addr) {
        UnregisterMeta(meta_info.htile_addr, image.image_uid);
    }

    download_images_combiner.Run([&] { u64_map_erase(&download_images, image_id.index); });

    // Reclaim image and any image views it references.
    scheduler.DeferOperation([this, image_id] {
        Image& image = slot_images[image_id];
        for (Image::BackingImage& backing : image.backing_images) {
            for (const ImageViewId image_view_id : backing.image_view_ids) {
                slot_image_views.Erase(image_view_id);
            }
        }
        slot_images.Erase(image_id);
    });
}

} // namespace VideoCore
