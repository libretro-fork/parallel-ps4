// SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "common/types.h"
#include "video_core/renderer_vulkan/vk_common.h"

namespace Vulkan {

class Instance;
class Scheduler;

/// The images the presenter draws each finished frame into, taken in turn. The libretro core
/// has no surface to present to: the frontend draws from these (shared with it on the GPU), or
/// the presenter reads each back for it.
class OutputImages {
public:
    /// How many there are when they are not shared with the frontend
    static constexpr u32 OffscreenCount = 3;
    /// The most there can be (as many as the frontend shares)
    static constexpr u32 MaxCount = 6;

    explicit OutputImages(const Instance& instance);
    ~OutputImages();

    OutputImages(const OutputImages&) = delete;
    OutputImages& operator=(const OutputImages&) = delete;

    /// Makes the images @width by @height. Those the frontend has are kept: it was sent them
    /// once, and draws from them. Images made before are destroyed once @scheduler's GPU work
    /// so far is done, as it is what last drew into them.
    void Create(u32 width, u32 height, Scheduler& scheduler);

    /// Whether they have been made
    bool Made() const {
        return count != 0;
    }

    /// Takes the next image to draw the frame into
    void Acquire();

    VkImage Image() const {
        return images[index];
    }

    VkImageView ImageView() const {
        return views[index];
    }

    VkFormat Format() const {
        return format;
    }

    VkExtent2D Extent() const {
        return extent;
    }

    u32 Count() const {
        return count;
    }

    /// Which of them the frame is drawn into
    u32 Index() const {
        return index;
    }

    /// The frame's generation in the image shared with the frontend (Libretro::AcquireImage)
    int Generation() const {
        return generation;
    }

private:
    /// What the images were, to destroy once the GPU is done with them
    struct Set {
        VkImage images[MaxCount];
        VkImageView views[MaxCount];
        VkDeviceMemory memory[MaxCount];
        u32 count;
    };

    /// Destroys the images, views and memory of @set
    static void DestroySet(VkDevice device, const Set& set);

    /// The images as a set, and none left here
    Set Take();

    /// Makes the image views and names the images
    void CreateViews();

    /// Makes images of the device's own memory
    void CreateOffscreenImages();

    /// Makes them of exportable memory and sends them to the frontend, when the picture is to
    /// be handed over on the GPU. False when that cannot be done: the images are then to be
    /// made the plain way
    bool CreateSharedImages();

    /// Makes images[@i] from @image_ci on device-local memory of its own, the allocation
    /// chained to @allocate_next, its size into *@out_size. False when that cannot be done.
    bool CreateImage(u32 i, const VkImageCreateInfo& image_ci, const void* allocate_next,
                     VkDeviceSize* out_size);

    const Instance& instance;
    VkDevice device;
    VkFormat format;
    VkExtent2D extent{};
    VkImage images[MaxCount]{};
    VkImageView views[MaxCount]{};
    VkDeviceMemory memory[MaxCount]{};
    u32 count = 0;
    u32 index = 0;
    int generation = 0;
    /// The memory type the images are made in, the same for each (the frontend is told it)
    u32 memory_type = ~0u;
};

} // namespace Vulkan
