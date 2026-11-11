// SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <string>
#include <thread>

#include <rthreads/retro_shared_eventcount.h>
#include <retro_atomic.h>

// The memory the libretro core (libretro_core.cpp, in the frontend's process) shares with the
// emulator it runs in a process of its own (host.cpp). One block: this header, then three
// frames.
namespace Libretro {

inline constexpr std::uint32_t SHARED_MAGIC = 0x34505368; // "hSP4"
inline constexpr std::uint32_t SHARED_VERSION = 15;

inline constexpr std::uint32_t MAX_WIDTH = 3840;
inline constexpr std::uint32_t MAX_HEIGHT = 2160;
inline constexpr std::size_t FRAME_BYTES = static_cast<std::size_t>(MAX_WIDTH) * MAX_HEIGHT * 4;

/// The slots a frame can be in: the newest one published, the one the core is showing, and one
/// for the emulator to write the next into.
inline constexpr std::uint32_t FRAME_SLOTS = 3;
/// The emulator's images when the picture is handed over on the GPU (gpu_frames.h): the same
/// three, and two more for frames that are drawn but not finished by the GPU yet.
inline constexpr std::uint32_t FRAME_IMAGES = 5;
/// SharedState::gpu_frames_state
inline constexpr std::uint32_t GPU_FRAMES_PENDING = 0;
inline constexpr std::uint32_t GPU_FRAMES_SHOWN = 1;
inline constexpr std::uint32_t GPU_FRAMES_FAILED = 2;
/// frame_held when the core is showing none.
inline constexpr std::uint32_t FRAME_SLOT_NONE = 7;
/// frame_latest is the number of frames published shifted by this, plus the newest one's slot.
inline constexpr std::uint32_t FRAME_SLOT_BITS = 3;
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

/// Frames of 16-bit stereo the audio ring holds. A power of two.
inline constexpr std::uint32_t AUDIO_RING_FRAMES = 16384;
/// Frames the emulator keeps in the ring: two of the frontend's frames' worth at 60 per
/// second, about 33 ms. The frontend takes one at a time, so less would run dry; more would
/// only be latency.
inline constexpr std::uint32_t AUDIO_RING_TARGET_FRAMES = 1600;

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
    std::uint32_t direct_memory_access;
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
};

struct SharedState {
    std::uint32_t magic;
    std::uint32_t version;

    /// What each side parks on until the other has written (retro_shared_eventcount.h), in
    /// place of waking on a timer. Each is notified after what it names is written.
    /// wake_control: quit, options_serial and frontend_gpu (the core).
    /// wake_input: buttons and axes (the core, when they change).
    /// wake_frame: frame_held (the core), an image no longer being drawn (the emulator).
    /// wake_audio: audio_read (the core), a source's sound handed to the mix (the emulator).
    retro_shared_eventcount_t wake_control;
    retro_shared_eventcount_t wake_input;
    retro_shared_eventcount_t wake_frame;
    retro_shared_eventcount_t wake_audio;

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

    /// A ring of interleaved 16-bit stereo, one writer (the emulator) and one reader (the core).
    /// The indices count frames and only grow; a position is index & (AUDIO_RING_FRAMES - 1).
    retro_atomic_int_t audio_written;
    retro_atomic_int_t audio_read;
    std::int16_t audio[AUDIO_RING_FRAMES * 2];
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
            std::this_thread::sleep_for(std::chrono::microseconds(us));
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

/// The four wakes of a SharedState, attached in this process. @name is the shared memory's
/// name on Windows (one per session); elsewhere it is not used.
struct SharedWakes {
    SharedWake control;
    SharedWake input;
    SharedWake frame;
    SharedWake audio;

    void Attach(SharedState* state, const std::string& name) {
        control.Attach(&state->wake_control, name + "_control");
        input.Attach(&state->wake_input, name + "_input");
        frame.Attach(&state->wake_frame, name + "_frame");
        audio.Attach(&state->wake_audio, name + "_audio");
    }
    void Detach() {
        control.Detach();
        input.Detach();
        frame.Detach();
        audio.Detach();
    }
};

inline std::uint8_t* SharedFrame(SharedState* state, std::uint32_t slot) {
    return reinterpret_cast<std::uint8_t*>(state) + SHARED_FRAMES_OFFSET + slot * FRAME_BYTES;
}

} // namespace Libretro
