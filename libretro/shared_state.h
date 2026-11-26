// SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <string>

#include <retro_atomic.h>
#include <retro_timers.h>
#include <rthreads/retro_shared_eventcount.h>

#include "audio_mix.h"
#include "common/patch_catalog.h"
#include "frame_slots.h"
#include "libretro_bridge.h"

// The memory the libretro core (libretro_core.cpp, in the frontend's process) shares with the
// emulator it runs in a process of its own (host.cpp). One block: this header, then three
// frames.
namespace Libretro {

inline constexpr std::uint32_t SHARED_MAGIC = 0x34505368; // "hSP4"
inline constexpr std::uint32_t SHARED_VERSION = 31;

// MAX_WIDTH, MAX_HEIGHT and FRAME_BYTES, a frame's size at most, are in
// libretro_bridge.h

// FRAME_SLOTS, the frames in this memory, is in libretro_bridge.h
/// The emulator's images when the picture is handed over on the GPU
/// (gpu_frames.h): the same three, two more for frames that are drawn but not
/// finished by the GPU yet, and one for a frame the frontend may still be
/// drawing from (frames_busy).
inline constexpr std::uint32_t FRAME_IMAGES = 6;
static_assert(FRAME_SLOTS <= FRAME_IMAGES,
              "frame_size has a place for every slot");
static_assert(FRAME_SLOTS <= FRAME_SLOTS_MAX && FRAME_IMAGES < FRAME_SLOTS_NONE,
              "frame_slots.h counts the slots");
/// SharedState::gpu_frames_state
inline constexpr std::uint32_t GPU_FRAMES_PENDING = 0;
inline constexpr std::uint32_t GPU_FRAMES_SHOWN = 1;
inline constexpr std::uint32_t GPU_FRAMES_FAILED = 2;
/// frame_held when the core is showing none.
inline constexpr std::uint32_t FRAME_SLOT_NONE = FRAME_SLOTS_NONE;
/// frame_latest is the number of frames published shifted by this, plus the newest one's slot.
inline constexpr std::uint32_t FRAME_SLOT_BITS = FRAME_SLOTS_BITS;
inline constexpr std::uint32_t FRAME_SLOT_MASK = (1u << FRAME_SLOT_BITS) - 1;

/// What the emulator tells the core once its images exist - over the socket on Linux, with one
/// descriptor of an image's memory for each of them, and in this memory on Windows, with a
/// handle for each: what the core needs to make the same images in the frontend's Vulkan
/// device. The formats and flags are Vulkan's values.
inline constexpr std::uint32_t GPU_IMAGES_MAGIC = 0x47505368; // "hSPG"
struct GpuImagesMessage {
    std::uint32_t magic;
    std::uint32_t count;
    std::uint32_t width;
    std::uint32_t height;
    std::uint32_t format;
    std::uint32_t usage;
    std::uint32_t memory_type;
    std::uint32_t reserved;
    std::uint64_t allocation_size[FRAME_IMAGES];
    std::uint8_t device_uuid[16];
    std::uint8_t driver_uuid[16];
};


inline constexpr std::size_t PATH_BYTES = 4096;
/// The paths of a PKG game's additional content, one a line.
inline constexpr std::size_t ADDCONT_BYTES = 64 * 1024;

/// The frontend's ports that can be pads of the console.
inline constexpr std::uint32_t MAX_PADS = 4;

/// A pad's axes, 0 to 255. Its buttons are OrbisPadButtonDataOffset bits.
enum SharedAxis : std::uint32_t {
    SHARED_AXIS_LEFT_X,
    SHARED_AXIS_LEFT_Y,
    SHARED_AXIS_RIGHT_X,
    SHARED_AXIS_RIGHT_Y,
    SHARED_AXIS_TRIGGER_LEFT,
    SHARED_AXIS_TRIGGER_RIGHT,
    SHARED_AXIS_COUNT,
};

/// The core options as they were when the game was loaded.
struct SharedOptions {
    std::uint32_t neo_mode;
    std::uint32_t circle_enter;
    std::uint32_t network;
    std::uint32_t show_splash;
    std::uint32_t width;
    std::uint32_t height;
    /// The width the game is rendered at when below its own, or 0 (Resolution: Default).
    std::uint32_t render_width;
    /// Resolution: Default - the picture is the game's own size, once the game has said it
    /// (its video out buffers); width and height are what it is until then.
    std::uint32_t follow_game_size;
    std::uint32_t vblank_frequency;
    std::uint32_t readbacks;
    /// When the game's memory is lent to the GPU for readbacks: 0 never, 1 as a
    /// readback first goes there, 2 as the game maps it (emu_readback_import)
    std::uint32_t readback_import;
    std::uint32_t direct_memory_access;
    /// The game's red zone is kept from Windows (emu_settings redzone_patches)
    std::uint32_t red_zone;
    std::uint32_t show_fps;
    std::uint32_t vertex_capture;
    std::int32_t volume;
    /// How many of the frontend's ports are connected as pads, 1 to MAX_PADS.
    std::uint32_t pads;
    std::int32_t console_language;
    std::uint32_t dev_kit;
    std::int32_t extra_dmem_mbytes;
    std::uint32_t readback_linear_images;
    std::uint32_t shader_cache;
    std::uint32_t dump_shaders;
    std::uint32_t sample_positions;
    std::uint32_t copy_gpu_buffers;
    /// Shader Compilation: Asynchronous (1), a draw whose pipeline is being made is skipped;
    /// Wait (0)
    std::uint32_t async_shaders;
    /// The frontend takes float sound: the core hands it the mix unrounded. Fixed for the
    /// session.
    std::uint32_t audio_float;
    /// 8: the frontend takes surround (RETRO_ENVIRONMENT_GET_AUDIO_SAMPLE_BATCH_MULTI): the mix
    /// is 7.1 wide. 2 otherwise. Fixed for the session.
    std::uint32_t audio_channels;
    /// The frontend takes HDR10 (PQ Rec.2020,
    /// RETRO_PIXEL_FORMAT_HDR10_2101010): the picture goes out as PQ, and
    /// frames read back are XRGB2101010. Fixed for the session.
    std::uint32_t hdr_output;
    /// Where SDR white goes in it, in nits (the frontend's paper white).
    std::uint32_t hdr_paper_white;
    /// The brightest and darkest the frontend shows, in nits: what a game is told to tone map
    /// to (sceSystemServiceGetHdrToneMapLuminance).
    float hdr_max_nits;
    float hdr_min_nits;
    /// The picture is handed over on the GPU: the frontend gave the core a Vulkan context.
    std::uint32_t gpu_frames;
    /// Linux: the emulator's end of the socket its images' descriptors go over, as a
    /// descriptor in the emulator's process; meaningful with gpu_frames only.
    std::int32_t gpu_socket;
    std::uint32_t trophy_popup;
    /// 0 left, 1 right, 2 top, 3 bottom.
    std::uint32_t trophy_side;
    std::uint32_t trophy_seconds;
    std::uint32_t log_enable;
    std::uint32_t log_separate;
    std::uint32_t log_sync;
    /// The Patch Manager: the serial the core listed the game's patches for, and those of
    /// them switched on, by their index in its catalog (common/patch_catalog.h), which the
    /// emulator builds again for the same serial.
    char patch_serial[16];
    std::uint32_t patches_on[PATCH_CATALOG_WORDS];
};

struct SharedState {
    std::uint32_t magic;
    std::uint32_t version;

    /// What each side parks on until the other has written (retro_shared_eventcount.h), in
    /// place of waking on a timer. Each is notified after what it names is written.
    /// wake_control: quit, options_serial and frontend_gpu (the core).
    /// wake_input: buttons and axes (the core, when they change).
    /// wake_frame: frame_held (the core), an image no longer being drawn (the emulator).
    /// wake_audio: not used.
    /// wake_vblank: vblank_ticks (the core).
    retro_shared_eventcount_t wake_control;
    retro_shared_eventcount_t wake_input;
    retro_shared_eventcount_t wake_frame;
    retro_shared_eventcount_t wake_audio;
    retro_shared_eventcount_t wake_vblank;

    /// The frontend's frames: one more each retro_run, after its input is written. The
    /// console's vblanks are counted from these (src/common/vblank_clock.h), not from a clock.
    /// Written by the core only; it wraps.
    retro_atomic_int_t vblank_ticks;

    // Written by the core before the emulator is started.
    char eboot_path[PATH_BYTES]; // UTF-8
    char user_dir[PATH_BYTES];   // UTF-8
    // A game run from its PKG: the PKG of its update and those of its additional content
    // the core found beside it, or empty.
    char update_path[PATH_BYTES];        // UTF-8
    char addcont_paths[ADDCONT_BYTES];   // UTF-8, '\n' after each
    SharedOptions options;

    /// Set by the core when content is closed: the emulator ends itself.
    retro_atomic_int_t quit;

    /// The options that take effect while the game runs, written again by the core whenever
    /// the frontend's user changes one; options_serial counts the changes, and the emulator
    /// applies them when it moves. The rest of options is read when the game starts.
    struct {
        retro_atomic_int_t show_fps;
        retro_atomic_int_t volume;
        retro_atomic_int_t trophy_popup;
        retro_atomic_int_t trophy_side;
        retro_atomic_int_t trophy_seconds;
        /// Diagnostics: switched on, the emulator captures a frame's vertex inputs
        retro_atomic_int_t vertex_capture;
    } live;
    retro_atomic_int_t options_serial;

    // Core to emulator, per pad.
    retro_atomic_int_t buttons[MAX_PADS];
    retro_atomic_int_t axes[MAX_PADS][SHARED_AXIS_COUNT];
    /// The touchpad's two touch slots (libretro/touch_track.h: down, x and y in one word)
    retro_atomic_int_t touch[MAX_PADS][2];

    // Emulator to core.
    /// A pad's motors, the small one in the low byte and the large one above it.
    retro_atomic_int_t rumble[MAX_PADS];

    /// The newest frame: frames published so far, shifted by FRAME_SLOT_BITS, plus the slot the
    /// newest is in. Zero while there is none. Written by the emulator only. A slot is one of
    /// the FRAME_SLOTS frames in this memory, or with gpu_frames one of the emulator's
    /// FRAME_IMAGES images.
    retro_atomic_int_t frame_latest;
    /// The slot the core is showing, which it hands to the frontend without a copy, or
    /// FRAME_SLOT_NONE. Written by the core only. The emulator writes a frame into a slot that
    /// is neither this one nor the newest. The core sets this to the newest slot and then reads
    /// frame_latest again: if it still names that slot, the emulator either saw the slot held or
    /// had not published anything newer, so it is not writing there. Both sides use the default,
    /// sequentially consistent ordering for the two; the argument needs it.
    retro_atomic_int_t frame_held;
    /// With gpu_frames: the images the frontend may still be drawing from on the GPU, a bit
    /// each, written by the core only. The core lets go of the image it held when it shows a
    /// newer one, but the frontend's frames that sampled it can still be in flight, as many as
    /// the frontend has sync indices: an image shown in one of those frames is not drawn into
    /// by the emulator until the frontend has waited for that frame.
    retro_atomic_int_t frames_busy;
    struct {
        std::uint32_t width;
        std::uint32_t height;
    } frame_size[FRAME_IMAGES];

    /// With gpu_frames: the GPU the frontend's Vulkan device is on, for the emulator to make
    /// its own on, as an image's memory cannot be handed from one GPU to another.
    /// frontend_gpu is 0 until the core knows, then 1 with the device's UUID in
    /// frontend_gpu_uuid, or 2 when it could not find out.
    retro_atomic_int_t frontend_gpu;
    std::uint8_t frontend_gpu_uuid[16];

    /// Windows, with gpu_frames: the emulator's images, as the socket carries them elsewhere.
    /// gpu_images_ready is set once the rest is written; the handles are the emulator's, and
    /// the core duplicates them into its own process.
    retro_atomic_int_t gpu_images_ready;
    GpuImagesMessage gpu_images;
    std::uint64_t gpu_image_handles[FRAME_IMAGES];

    /// With gpu_frames: GPU_FRAMES_PENDING until the core has imported the emulator's images
    /// (GPU_FRAMES_SHOWN), or either side found they cannot be shared (GPU_FRAMES_FAILED): the
    /// emulator then reads each frame back into this memory, as without gpu_frames, and the
    /// core uploads it into an image of the frontend's device.
    retro_atomic_int_t gpu_frames_state;

    /// The audio mix's inputs (libretro/audio_mix.h): the emulator's sources write them, and
    /// the core mixes exactly one frame of every one in retro_run, straight to the frontend.
    /// No ring between, no mixing thread.
    alignas(64) audio_mix_shared_t mix;
};

inline constexpr std::size_t SHARED_FRAMES_OFFSET = (sizeof(SharedState) + 4095) & ~std::size_t{4095};
inline constexpr std::size_t SHARED_BYTES = SHARED_FRAMES_OFFSET + FRAME_SLOTS * FRAME_BYTES;

/// One process's handle onto a wake in the shared memory.
class SharedWake {
public:
    SharedWake() = default;
    SharedWake(const SharedWake&) = delete;
    SharedWake& operator=(const SharedWake&) = delete;
    ~SharedWake() {
        Detach();
    }

    /// @name is the same in both processes (Windows names a semaphore with it).
    bool Attach(retro_shared_eventcount_t* state, const std::string& name) {
        Detach();
        attached = retro_shared_eventcount_attach(&handle, state, name.c_str());
        return attached;
    }
    void Detach() {
        if (attached) {
            retro_shared_eventcount_detach(&handle);
            attached = false;
        }
    }

    void Notify() {
        if (attached) {
            retro_shared_eventcount_notify(&handle);
        }
    }
    /// The wait's window opens here: look at what is waited for after this, then Cancel or
    /// Wait.
    int Prepare() {
        return attached ? retro_shared_eventcount_prepare_wait(&handle) : 0;
    }
    void Cancel() {
        if (attached) {
            retro_shared_eventcount_cancel_wait(&handle);
        }
    }
    /// Until a Notify after the Prepare, or @timeout_us (negative: no limit). Without a wake
    /// between the processes on this platform it sleeps a millisecond at most, and the caller
    /// looks again, as before there were wakes.
    void Wait(int key, std::int64_t timeout_us = -1) {
        if (!attached) {
            const std::int64_t us = timeout_us < 0 ? 1000 : std::min<std::int64_t>(timeout_us, 1000);
            retro_sleep_us(static_cast<unsigned>(us));
        } else if (timeout_us < 0) {
            retro_shared_eventcount_commit_wait(&handle, key);
        } else {
            retro_shared_eventcount_commit_wait_timeout(&handle, key, timeout_us);
        }
    }

private:
    retro_shared_eventcount_handle_t handle{};
    bool attached = false;
};

/// The five wakes of a SharedState, attached in this process. @name is the shared memory's
/// name on Windows (one per session); elsewhere it is not used.
struct SharedWakes {
    SharedWake control;
    SharedWake input;
    SharedWake frame;
    SharedWake audio;
    SharedWake vblank;

    void Attach(SharedState* state, const std::string& name) {
        control.Attach(&state->wake_control, name + "_control");
        input.Attach(&state->wake_input, name + "_input");
        frame.Attach(&state->wake_frame, name + "_frame");
        audio.Attach(&state->wake_audio, name + "_audio");
        vblank.Attach(&state->wake_vblank, name + "_vblank");
    }
    void Detach() {
        control.Detach();
        input.Detach();
        frame.Detach();
        audio.Detach();
        vblank.Detach();
    }
};

inline std::uint8_t* SharedFrame(SharedState* state, std::uint32_t slot) {
    return reinterpret_cast<std::uint8_t*>(state) + SHARED_FRAMES_OFFSET + slot * FRAME_BYTES;
}

} // namespace Libretro
