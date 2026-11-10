// SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

// The emulator's side of the libretro core. The core (libretro_core.cpp) lives
// in the frontend's process and starts this module again in a process of its
// own - through rundll32 on Windows, through the small launcher built into the
// core elsewhere (host_main.c) - which enters here, in retro_host_main. The
// emulator then runs as it does standalone, in a process whose address space is
// its own and which can be ended when content is closed, and trades with the
// core through the shared memory of shared_state.h:
//
//   picture  each finished frame, read back by the renderer (PushFrame)
//   sound    the sources' inputs of the mix, summed into a ring (PumpAudio,
//   audio_mix.c) pads     the frontend's ports, put on the controllers
//   (PumpInput), and their motors options  the core options, put on top of the
//   emulator's configuration (ApplyCoreOptions)

#include <algorithm>
#include <atomic>
#include <cerrno>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#include "audio_mix.h"
#include "common/key_manager.h"
#include "common/logging/log.h"
#include "common/path_util.h"
#include "common/singleton.h"
#include "core/devtools/layer.h"
#include "core/emulator_settings.h"
#include "core/file_sys/fs.h"
#include "core/ipc/ipc.h"
#include "core/libraries/pad/pad.h"
#include "core/user_manager.h"
#include "core/user_settings.h"
#include "emulator.h"
#include "input/controller.h"
#include "libretro.h"
#include "libretro_bridge.h"
#include "shared_state.h"
#include "video_core/renderer_vulkan/vk_presenter.h"
#include "video_core/texture_cache/texture_cache.h"

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

// The presenter, as upstream's IPC and devtools reach it
extern std::unique_ptr<Vulkan::Presenter> presenter;

namespace {

Libretro::SharedState* shared = nullptr;

// The renderer has drawn: the emulator is up, its controllers made.
std::atomic<bool> frame_seen{false};

// The picture handed over on the GPU: whether the images were sent to the core, and which of
// them have a frame being drawn that is not published yet.
std::atomic<bool> gpu_frames_active{false};

// Resolution: Default (SetGameSize): the game's size, packed as width << 16 | height, and
// whether the presenter has taken it
std::atomic<std::uint32_t> game_size{0};
std::atomic<bool> game_size_new{false};
std::atomic<bool> image_in_flight[Libretro::FRAME_IMAGES];

Libretro::SharedState* OpenShared(const char* argument) {
#ifdef _WIN32
    // The name of the core's file mapping.
    const HANDLE mapping = OpenFileMappingA(FILE_MAP_ALL_ACCESS, FALSE, argument);
    if (!mapping) {
        return nullptr;
    }
    void* view = MapViewOfFile(mapping, FILE_MAP_ALL_ACCESS, 0, 0, Libretro::SHARED_BYTES);
#else
    // The number of the descriptor inherited from the core.
    const int fd = std::atoi(argument);
    void* view =
        mmap(nullptr, Libretro::SHARED_BYTES, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (view == MAP_FAILED) {
        view = nullptr;
    }
#endif
    auto* state = static_cast<Libretro::SharedState*>(view);
    if (!state || state->magic != Libretro::SHARED_MAGIC ||
        state->version != Libretro::SHARED_VERSION) {
        return nullptr;
    }
    return state;
}

// Content closed in the frontend: the emulator ends the way it does when its window is closed,
// which runs its at_quick_exit work (play time, logs, save data backups).
constexpr const char* trophy_sides[] = {"left", "right", "top", "bottom"};

// The options changed while the game runs, where the emulator reads them as it goes: the
// volume and the trophy notifications from its settings, FSR from the presenter's own copy and
// the FPS counter from the overlay's, which upstream's IPC changes the same way.
void ApplyLiveOptions() {
    const auto& live = shared->live;
    EmulatorSettings.SetVolumeSlider(live.volume.load());
    EmulatorSettings.SetFsrEnabled(live.fsr.load() != 0);
    EmulatorSettings.SetRcasEnabled(live.rcas.load() != 0);
    EmulatorSettings.SetRcasAttenuation(live.rcas_attenuation.load());
    if (presenter) {
        auto& fsr = presenter->GetFsrSettingsRef();
        fsr.enable = live.fsr.load() != 0;
        fsr.use_rcas = live.rcas.load() != 0;
        fsr.rcas_attenuation = static_cast<float>(live.rcas_attenuation.load() / 1000.0f);
    }
    EmulatorSettings.SetShowFpsCounter(live.show_fps.load() != 0);
    Overlay::SetSimpleFps(live.show_fps.load() != 0);
    EmulatorSettings.SetTrophyPopupDisabled(live.trophy_popup.load() == 0);
    EmulatorSettings.SetTrophyNotificationSide(trophy_sides[std::min(live.trophy_side.load(), 3u)]);
    EmulatorSettings.SetTrophyNotificationDuration(static_cast<double>(live.trophy_seconds.load()));
}

// Ends the process on the core's word, and applies the options changed meanwhile
void WatchQuit() {
    std::uint32_t serial = 0;
    while (!shared->quit.load(std::memory_order_acquire)) {
        const std::uint32_t now = shared->options_serial.load(std::memory_order_acquire);
        if (now != serial) {
            serial = now;
            ApplyLiveOptions();
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    std::quick_exit(0);
}

// What the sources have, summed, into the ring the core plays from. A source
// that has less than another contributes silence for the difference.
void PumpAudio() {
  for (;;) {
    std::this_thread::sleep_for(std::chrono::milliseconds(2));

    const std::uint32_t written =
        shared->audio_written.load(std::memory_order_relaxed);
    const std::uint32_t read =
        shared->audio_read.load(std::memory_order_acquire);
    // Up to the target and no further: what the sources have beyond it stays in
    // their inputs, where the game's audio threads wait for it to be taken.
    const std::uint32_t queued = written - read;
    const std::uint32_t wanted =
        queued < Libretro::AUDIO_RING_TARGET_FRAMES
            ? Libretro::AUDIO_RING_TARGET_FRAMES - queued
            : 0;
    if (wanted == 0) {
      continue;
    }
    const std::uint32_t frames = audio_mix_pump(
        shared->audio, Libretro::AUDIO_RING_FRAMES - 1, written, wanted);
    shared->audio_written.store(written + frames, std::memory_order_release);
  }
}

// The frontend's ports onto the controllers. Only what changed: every call
// queues a state for the game to read.
//
// The first controller is connected by the emulator itself when the game loads
// the pad library (GameControllers::ConnectFirstController). The others, as
// many as the Controllers option says, are connected here the same way: the
// user of that player number is logged in and the controller marked connected.
//
// Each connected controller also reports its state every 4 ms whether or not it
// changed, as a pad does: a game reading several states at once gets the
// history it expects.
void PumpInput() {
    using Libraries::Pad::OrbisPadButtonDataOffset;
    static constexpr OrbisPadButtonDataOffset buttons[] = {
        OrbisPadButtonDataOffset::L3,       OrbisPadButtonDataOffset::R3,
        OrbisPadButtonDataOffset::Options,  OrbisPadButtonDataOffset::Up,
        OrbisPadButtonDataOffset::Right,    OrbisPadButtonDataOffset::Down,
        OrbisPadButtonDataOffset::Left,     OrbisPadButtonDataOffset::L2,
        OrbisPadButtonDataOffset::R2,       OrbisPadButtonDataOffset::L1,
        OrbisPadButtonDataOffset::R1,       OrbisPadButtonDataOffset::Triangle,
        OrbisPadButtonDataOffset::Circle,   OrbisPadButtonDataOffset::Cross,
        OrbisPadButtonDataOffset::Square,   OrbisPadButtonDataOffset::TouchPad,
    };
    static constexpr Input::Axis axes[Libretro::SHARED_AXIS_COUNT] = {
        Input::Axis::LeftX,  Input::Axis::LeftY,       Input::Axis::RightX,
        Input::Axis::RightY, Input::Axis::TriggerLeft, Input::Axis::TriggerRight,
    };

    std::uint32_t held[Libretro::MAX_PADS] = {};
    std::uint32_t axis_value[Libretro::MAX_PADS][Libretro::SHARED_AXIS_COUNT];
    for (auto& pad : axis_value) {
        std::fill(std::begin(pad), std::end(pad), ~0u);
    }
    const std::uint32_t pads = std::clamp<std::uint32_t>(shared->options.pads, 1, Libretro::MAX_PADS);
    bool others_connected = false;
    bool report = false;

    for (;;) {
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
        report = !report;
        if (!frame_seen) {
            continue;
        }
        auto& controllers = *Common::Singleton<Input::GameControllers>::Instance();
        // Not before the emulator has logged the first controller in.
        if (!controllers[0] || controllers[0]->user_id < 0) {
            continue;
        }
        if (!others_connected) {
            others_connected = true;
            for (std::uint32_t pad = 1; pad < pads; ++pad) {
                auto* user = UserManagement.GetUserByPlayerIndex(static_cast<s32>(pad) + 1);
                if (!user) {
                    LOG_WARNING(Input, "No user for player {}: controller {} stays disconnected",
                                pad + 1, pad + 1);
                    continue;
                }
                controllers[pad]->user_id = user->user_id;
                UserManagement.LoginUser(user, static_cast<s32>(pad) + 1);
                controllers[pad]->ConnectController();
                LOG_INFO(Input, "Controller {} connected from the frontend's port {}", pad + 1,
                         pad + 1);
            }
        }

        for (std::uint32_t pad = 0; pad < pads; ++pad) {
            auto* controller = controllers[pad];
            if (!controller || controller->user_id < 0) {
                continue;
            }
            const std::uint32_t now = shared->buttons[pad].load(std::memory_order_acquire);
            for (const auto button : buttons) {
                const auto bit = static_cast<std::uint32_t>(button);
                if ((now ^ held[pad]) & bit) {
                    controller->Button(button, (now & bit) != 0);
                }
            }
            held[pad] = now;

            for (std::uint32_t i = 0; i < Libretro::SHARED_AXIS_COUNT; ++i) {
                const std::uint32_t value = shared->axes[pad][i].load(std::memory_order_acquire);
                if (value != axis_value[pad][i]) {
                    axis_value[pad][i] = value;
                    controller->Axis(axes[i], static_cast<int>(value), false);
                }
            }
            if (report) {
              controller->PollState();
            }
        }
    }
}

// What main() does between its command line and Emulator::Run. Run does not return: it ends
// in the window's event loop and std::quick_exit.
int RunEmulator(const char* argument) {
    shared = OpenShared(argument);
    if (!shared) {
        return 2;
    }

    const std::filesystem::path eboot{
        std::u8string_view(reinterpret_cast<const char8_t*>(shared->eboot_path))};
    const std::filesystem::path user_dir{
        std::u8string_view(reinterpret_cast<const char8_t*>(shared->user_dir))};
    Common::FS::SetLibretroUserDir(user_dir);
    // The update and additional content of a game run from its PKG (libretro_core.cpp).
    Core::FileSys::MntPoints::libretro_update =
        std::u8string_view(reinterpret_cast<const char8_t*>(shared->update_path));
    for (std::string_view paths{shared->addcont_paths}; !paths.empty();) {
        const auto end = paths.find('\n');
        const auto line = paths.substr(0, end);
        if (!line.empty()) {
            Core::FileSys::MntPoints::libretro_addcont.emplace_back(
                std::u8string_view(reinterpret_cast<const char8_t*>(line.data()), line.size()));
        }
        paths = end == std::string_view::npos ? std::string_view{} : paths.substr(end + 1);
    }

    std::thread(WatchQuit).detach();
    if (!audio_mix_init()) {
      LOG_CRITICAL(Lib_AudioOut, "No memory for the core's audio mix");
    }
    std::thread(PumpAudio).detach();
    std::thread(PumpInput).detach();

    Common::Log::Setup("shadps4.log");
    IPC::Instance().Init();
    key_manager_load();
    EmulatorSettings.Load();
    UserSettings.Load();

    auto* emulator = Common::Singleton<Core::Emulator>::Instance();
    emulator->executableName = "shadps4_libretro";
    emulator->Run(eboot);
    return 0;
}

} // namespace

namespace Libretro {

void SetGameSize(std::uint32_t width, std::uint32_t height) {
    if (!shared || shared->options.follow_game_size == 0 || width == 0 || height == 0) {
        return;
    }
    width = std::min(width, MAX_WIDTH);
    height = std::min(height, MAX_HEIGHT);
    const std::uint32_t packed = width << 16 | height;
    if (game_size.exchange(packed) != packed) {
        game_size_new.store(true);
    }
}

bool TakeGameSize(std::uint32_t& width, std::uint32_t& height) {
    if (!game_size_new.exchange(false)) {
        return false;
    }
    const std::uint32_t packed = game_size.load();
    width = packed >> 16;
    height = packed & 0xFFFF;
    return true;
}

bool GameSizePending() {
    return shared && shared->options.follow_game_size != 0 && game_size.load() == 0;
}

void PushFrame(const void* pixels, std::uint32_t width, std::uint32_t height) {
    if (!shared || !pixels || width == 0 || height == 0 || width > MAX_WIDTH ||
        height > MAX_HEIGHT) {
        return;
    }
    // Until the game's size is known the images for the frontend are not made yet, and it
    // expects those, not frames in the shared memory
    if (GpuFramesWanted() && GameSizePending()) {
        return;
    }
    // Into the slot that is neither the newest nor the one the core is showing
    // (shared_state.h, frame_held).
    const std::uint32_t latest = shared->frame_latest.load();
    const std::uint32_t newest = latest == 0 ? FRAME_SLOT_NONE : latest & FRAME_SLOT_MASK;
    const std::uint32_t held = shared->frame_held.load();
    std::uint32_t slot = 0;
    while (slot == newest || slot == held) {
        ++slot;
    }
    shared->frame_size[slot].width = width;
    shared->frame_size[slot].height = height;
    std::memcpy(SharedFrame(shared, slot), pixels, static_cast<std::size_t>(width) * height * 4);
    shared->frame_latest.store((((latest >> FRAME_SLOT_BITS) + 1) << FRAME_SLOT_BITS) | slot);
    frame_seen = true;
}

bool GpuFramesWanted() {
#if defined(__linux__)
    return shared && shared->options.gpu_frames != 0 && shared->options.gpu_socket >= 0;
#elif defined(_WIN32)
    return shared && shared->options.gpu_frames != 0;
#else
    return false;
#endif
}

bool FrontendGpu(std::uint8_t* device_uuid) {
    if (!GpuFramesWanted()) {
        return false;
    }
    // The frontend makes its device right after the game is loaded, and the emulator gets here
    // about as soon: five seconds is far more than the frontend takes.
    for (int attempt = 0; attempt < 500 && shared->frontend_gpu.load() == 0; ++attempt) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    if (shared->frontend_gpu.load() != 1) {
        LOG_WARNING(Render_Vulkan, "The frontend's GPU is not known");
        return false;
    }
    std::memcpy(device_uuid, shared->frontend_gpu_uuid, sizeof(shared->frontend_gpu_uuid));
    return true;
}

std::uint32_t GpuFrameImages() {
    return FRAME_IMAGES;
}

bool ShareImages(const std::uint64_t* memory_handles, std::uint32_t count, std::uint32_t width,
                 std::uint32_t height, std::uint32_t format, std::uint32_t usage,
                 std::uint32_t memory_type, const std::uint64_t* allocation_sizes,
                 const std::uint8_t* device_uuid, const std::uint8_t* driver_uuid) {
#if defined(__linux__) || defined(_WIN32)
    if (!GpuFramesWanted() || count == 0 || count > FRAME_IMAGES || gpu_frames_active) {
        return false;
    }
    GpuImagesMessage message{};
    message.magic = GPU_IMAGES_MAGIC;
    message.count = count;
    message.width = width;
    message.height = height;
    message.format = format;
    message.usage = usage;
    message.memory_type = memory_type;
    std::memcpy(message.allocation_size, allocation_sizes, count * sizeof(std::uint64_t));
    std::memcpy(message.device_uuid, device_uuid, sizeof(message.device_uuid));
    std::memcpy(message.driver_uuid, driver_uuid, sizeof(message.driver_uuid));

#if defined(_WIN32)
    // In the shared memory; the core duplicates the handles into its own process.
    shared->gpu_images = message;
    std::memcpy(shared->gpu_image_handles, memory_handles, count * sizeof(std::uint64_t));
    shared->gpu_images_ready.store(1);
#else
    int descriptors[FRAME_IMAGES];
    for (std::uint32_t i = 0; i < count; ++i) {
        descriptors[i] = static_cast<int>(memory_handles[i]);
    }
    iovec data{&message, sizeof(message)};
    alignas(cmsghdr) char control[CMSG_SPACE(sizeof(int) * FRAME_IMAGES)]{};
    msghdr header{};
    header.msg_iov = &data;
    header.msg_iovlen = 1;
    header.msg_control = control;
    header.msg_controllen = CMSG_SPACE(sizeof(int) * count);
    cmsghdr* part = CMSG_FIRSTHDR(&header);
    part->cmsg_level = SOL_SOCKET;
    part->cmsg_type = SCM_RIGHTS;
    part->cmsg_len = CMSG_LEN(sizeof(int) * count);
    std::memcpy(CMSG_DATA(part), descriptors, sizeof(int) * count);
    if (sendmsg(shared->options.gpu_socket, &header, MSG_NOSIGNAL) !=
        static_cast<ssize_t>(sizeof(message))) {
        LOG_ERROR(Render_Vulkan, "The images could not be sent to the frontend: {}",
                  std::strerror(errno));
        return false;
    }
#endif
    for (auto& in_flight : image_in_flight) {
        in_flight = false;
    }
    gpu_frames_active = true;
    LOG_INFO(Render_Vulkan, "The picture is handed to the frontend on the GPU: {} images of {}x{}",
             count, width, height);
    return true;
#else
    return false;
#endif
}

bool GpuFramesActive() {
    return gpu_frames_active;
}

std::uint32_t AcquireImage() {
    // Up to a tenth of a second: the frontend holds one image and the newest is kept for it,
    // which leaves three for frames being drawn. After that the wait is given up, and an image
    // still being drawn is drawn over rather than the picture stopping.
    for (int attempt = 0; attempt < 100; ++attempt) {
        const std::uint32_t latest = shared->frame_latest.load();
        const std::uint32_t newest = latest == 0 ? FRAME_SLOT_NONE : latest & FRAME_SLOT_MASK;
        const std::uint32_t held = shared->frame_held.load();
        for (std::uint32_t image = 0; image < FRAME_IMAGES; ++image) {
            if (image != newest && image != held && !image_in_flight[image].exchange(true)) {
                return image;
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    const std::uint32_t latest = shared->frame_latest.load();
    const std::uint32_t newest = latest == 0 ? FRAME_SLOT_NONE : latest & FRAME_SLOT_MASK;
    const std::uint32_t held = shared->frame_held.load();
    std::uint32_t image = 0;
    while (image == newest || image == held) {
        ++image;
    }
    image_in_flight[image] = true;
    return image;
}

void PushImage(std::uint32_t image, std::uint32_t width, std::uint32_t height) {
    if (!shared || image >= FRAME_IMAGES) {
        return;
    }
    shared->frame_size[image].width = width;
    shared->frame_size[image].height = height;
    const std::uint32_t latest = shared->frame_latest.load();
    shared->frame_latest.store((((latest >> FRAME_SLOT_BITS) + 1) << FRAME_SLOT_BITS) | image);
    image_in_flight[image] = false;
    frame_seen = true;
}

void ApplyCoreOptions() {
    if (!shared) {
        return;
    }
    const SharedOptions& options = shared->options;
    EmulatorSettings.SetNeo(options.neo_mode != 0);
    EmulatorSettings.SetCircleEnter(options.circle_enter != 0);
    EmulatorSettings.SetConnectedToNetwork(options.network != 0);
    EmulatorSettings.SetShowSplash(options.show_splash != 0);
    EmulatorSettings.SetWindowWidth(options.width);
    EmulatorSettings.SetWindowHeight(options.height);
    VideoCore::render_width.store(options.render_width);
    EmulatorSettings.SetFsrEnabled(options.fsr != 0);
    EmulatorSettings.SetRcasEnabled(options.rcas != 0);
    EmulatorSettings.SetVblankFrequency(options.vblank_frequency);
    EmulatorSettings.SetReadbacksMode(options.readbacks);
    EmulatorSettings.SetDirectMemoryAccessEnabled(options.direct_memory_access != 0);
    EmulatorSettings.SetShowFpsCounter(options.show_fps != 0);
    EmulatorSettings.SetVolumeSlider(options.volume);
    EmulatorSettings.SetConsoleLanguage(options.console_language);
    EmulatorSettings.SetDevKit(options.dev_kit != 0);
    EmulatorSettings.SetExtraDmemInMBytes(options.extra_dmem_mbytes);
    EmulatorSettings.SetRcasAttenuation(options.rcas_attenuation);
    EmulatorSettings.SetReadbackLinearImagesEnabled(options.readback_linear_images != 0);
    EmulatorSettings.SetPipelineCacheEnabled(options.shader_cache != 0);
    EmulatorSettings.SetCopyGpuBuffers(options.copy_gpu_buffers != 0);
    EmulatorSettings.SetTrophyPopupDisabled(options.trophy_popup == 0);
    EmulatorSettings.SetTrophyNotificationSide(trophy_sides[std::min(options.trophy_side, 3u)]);
    EmulatorSettings.SetTrophyNotificationDuration(static_cast<double>(options.trophy_seconds));
    EmulatorSettings.SetLogEnable(options.log_enable != 0);
    EmulatorSettings.SetLogSeparate(options.log_separate != 0);
    EmulatorSettings.SetLogSync(options.log_sync != 0);
}

void SetRumble(std::uint32_t pad, std::uint8_t small_motor, std::uint8_t large_motor) {
    if (shared && pad < MAX_PADS) {
        shared->rumble[pad].store(small_motor | (static_cast<std::uint32_t>(large_motor) << 8),
                                  std::memory_order_release);
    }
}

} // namespace Libretro

#ifdef _WIN32

namespace {

DWORD WINAPI EmulatorThread(LPVOID argument) {
    return static_cast<DWORD>(RunEmulator(static_cast<const char*>(argument)));
}

} // namespace

// rundll32's entry: rundll32.exe "<core>",retro_host_main <name of the file mapping>. The
// emulator gets a thread with the stack upstream gives its executable's main thread - 2 MB,
// committed, where rundll32's own is the default - and this one waits for it.
extern "C" __declspec(dllexport) void CALLBACK retro_host_main(HWND, HINSTANCE, LPSTR command_line,
                                                               int) {
    static std::string argument;
    argument = command_line ? command_line : "";
    const auto first = argument.find_first_not_of(" \t\"");
    const auto last = argument.find_last_not_of(" \t\"\r\n");
    argument = first == std::string::npos ? "" : argument.substr(first, last - first + 1);

    const HANDLE thread =
        CreateThread(nullptr, 0x200000, EmulatorThread, argument.data(), 0, nullptr);
    if (!thread) {
        ExitProcess(2);
    }
    WaitForSingleObject(thread, INFINITE);
    DWORD code = 0;
    GetExitCodeThread(thread, &code);
    ExitProcess(code);
}

#else

// The launcher's entry (host_main.c): the number of the shared memory's descriptor.
extern "C" RETRO_API int retro_host_main(const char* argument) {
    return RunEmulator(argument ? argument : "");
}

#endif
