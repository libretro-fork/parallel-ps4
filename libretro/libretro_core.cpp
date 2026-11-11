// SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

// The libretro core, the part in the frontend's process. It does not run the emulator: it
// starts this same module again in a process of its own (host.cpp) and passes the frontend's
// side of things through the shared memory of shared_state.h.
//
// The emulator runs the game's code directly on the host CPU, at the addresses the game was
// built for, and has no way to stop a running game short of ending its process - upstream's
// own exit is std::quick_exit. In a process of its own it can have its address range to itself,
// closing content is ending that process, the next game is a new one, and a game that crashes
// does not take the frontend with it.
//
//   Windows   rundll32.exe "<this dll>",retro_host_main <name of the file mapping>
//   elsewhere a small launcher, built into the core and run from memory (host_main.c), which
//             loads the core and calls retro_host_main with the shared memory's descriptor
//
// Picture: the emulator's renderer reads each finished frame back and puts it in the shared
// memory; retro_run hands the newest one over as a software frame, from where it is.
// Sound: the emulator mixes its audio ports into a ring; retro_run takes one frame's worth and
// fills what is missing with silence.
// Pads: the buttons, sticks and triggers of as many ports as the Controllers option says go
// in, the motors come back.
// Core options: read when a game is loaded and passed along; the emulator puts them on top of
// its own configuration. The texts are in libretro_core_options.h, translated through Crowdin.
//
// Not there: save states (the emulator has none), and the emulator's picture without the copy
// through system memory.

#include <algorithm>
#include <atomic>
#include <cctype>
#include <cerrno>
#include <climits>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <string>
#include <system_error>
#include <vector>

#include <rthreads/rthreads.h>
#include <streams/file_stream.h>

#include "common/io_file.h"
#include "common/scm_rev.h"
#include "core/libraries/pad/pad.h"
#include "libretro.h"
#include "gpu_frames.h"
#include "libretro_core_options.h"
#include "pkg_reader.h"
#include "shared_state.h"

#ifdef _WIN32
#include <windows.h>
#else
#include <csignal>
#include <dlfcn.h>
#include <fcntl.h>
#include <spawn.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

extern char** environ;

// The launcher (host_main.c), embedded by embed_host.cmake.
extern "C" const unsigned char g_libretro_host[];
extern "C" const unsigned long g_libretro_host_size;
#endif

namespace {

using Libretro::SharedState;

retro_environment_t environ_cb = nullptr;
retro_video_refresh_t video_cb = nullptr;
retro_audio_sample_batch_t audio_batch_cb = nullptr;
retro_input_poll_t input_poll_cb = nullptr;
retro_input_state_t input_state_cb = nullptr;
retro_log_printf_t log_cb = nullptr;

// Resolution: Default, the PS4's own
constexpr unsigned kWidth = 1920;
constexpr unsigned kHeight = 1080;
constexpr double kFps = 60.0;
constexpr int kSampleRate = 48000;
constexpr int kAudioFrames = kSampleRate / 60;

SharedState* shared = nullptr;
bool emulator_running = false;
bool shutdown_requested = false;

#ifdef _WIN32
HANDLE shared_mapping = nullptr;
HANDLE emulator_process = nullptr;
HANDLE emulator_job = nullptr;
#else
int shared_fd = -1;
pid_t emulator_pid = -1;
#endif

// The frame handed to the frontend: a slot of the shared memory, held for as long as it is
// shown (shared_state.h, frame_held), or blank_frame until the emulator has drawn one.
const void* shown_frame = nullptr;
std::vector<uint32_t> blank_frame;
unsigned shown_width = 0;
unsigned shown_height = 0;
uint32_t shown_latest = 0;

// The picture handed over on the GPU (gpu_frames.h): the frontend gave a Vulkan context, and
// the frame shown is one of the emulator's images in place of a slot of the shared memory.
bool gpu_frames = false;
bool gpu_failure_told = false;
uint32_t shown_image = 0;
// The core's end of the socket the images' descriptors come over, and the emulator's until it
// is started.
int gpu_socket = -1;
int gpu_socket_emulator = -1;

// The Resolution the game was loaded with: the size frames come at, and so the largest
// the frontend is told to expect. No larger - a frontend sizes its shader passes by it.
unsigned output_width = kWidth;
unsigned output_height = kHeight;
// The largest picture the frontend was told to expect (retro_get_system_av_info, or
// SET_SYSTEM_AV_INFO since). A larger one needs SET_SYSTEM_AV_INFO: SET_GEOMETRY cannot raise
// it, and the frontend's buffers (a software video filter's among them) are sized by it.
unsigned declared_max_width = kWidth;
unsigned declared_max_height = kHeight;
// The size the frontend was last told a picture has
unsigned announced_width = kWidth;
unsigned announced_height = kHeight;

// The frontend told of a picture of another size, from retro_run, before it is handed over.
// Larger than it was told to expect (Resolution changed before a Reset, or Default taking the
// game's size): it sizes its buffers again with SET_SYSTEM_AV_INFO - SET_GEOMETRY cannot raise
// the max, and a software video filter (scanline2x) wrote past its buffer and crashed it.
void AnnounceSize(unsigned width, unsigned height) {
    if (width == announced_width && height == announced_height) {
        return;
    }
    announced_width = width;
    announced_height = height;
    if (width > declared_max_width || height > declared_max_height) {
        declared_max_width = std::max(declared_max_width, width);
        declared_max_height = std::max(declared_max_height, height);
        struct retro_system_av_info av_info {};
        av_info.geometry.base_width = width;
        av_info.geometry.base_height = height;
        av_info.geometry.max_width = declared_max_width;
        av_info.geometry.max_height = declared_max_height;
        av_info.geometry.aspect_ratio = static_cast<float>(width) / static_cast<float>(height);
        av_info.timing.fps = kFps;
        av_info.timing.sample_rate = kSampleRate;
        environ_cb(RETRO_ENVIRONMENT_SET_SYSTEM_AV_INFO, &av_info);
        return;
    }
    struct retro_game_geometry geometry {};
    geometry.base_width = width;
    geometry.base_height = height;
    geometry.max_width = declared_max_width;
    geometry.max_height = declared_max_height;
    geometry.aspect_ratio = static_cast<float>(width) / static_cast<float>(height);
    environ_cb(RETRO_ENVIRONMENT_SET_GEOMETRY, &geometry);
}

struct retro_rumble_interface rumble {};

void Log(enum retro_log_level level, const char* fmt, ...) {
    char text[2048];
    va_list args;
    va_start(args, fmt);
    std::vsnprintf(text, sizeof(text), fmt, args);
    va_end(args);
    if (log_cb) {
        log_cb(level, "[shadPS4] %s\n", text);
    } else {
        std::fprintf(stderr, "[shadPS4] %s\n", text);
    }
}

std::string SystemDirectory() {
    const char* dir = nullptr;
    if (environ_cb && environ_cb(RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY, &dir) && dir && *dir) {
        std::string path = dir;
        if (path.back() != '/' && path.back() != '\\') {
            path += '/';
        }
        return path + "shadPS4";
    }
    return {};
}

std::string OptionValue(const char* key, const char* fallback) {
    struct retro_variable variable = {key, nullptr};
    if (environ_cb && environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &variable) && variable.value) {
        return variable.value;
    }
    return fallback;
}

void ReadCoreOptions(Libretro::SharedOptions& options) {
    const auto enabled = [](const char* key, const char* fallback) -> uint32_t {
        return OptionValue(key, fallback) == "enabled" ? 1 : 0;
    };
    options.neo_mode = enabled("shadps4_neo_mode", "disabled");
    options.circle_enter = OptionValue("shadps4_confirm_button", "cross") == "circle" ? 1 : 0;
    options.network = enabled("shadps4_network", "disabled");
    options.show_splash = enabled("shadps4_show_splash", "disabled");

    // Resolution: the picture handed over at that size and, where it is below the game's
    // own, the game rendered at it too (the emulator works out how much smaller)
    options.width = kWidth;
    options.height = kHeight;
    options.render_width = 0;
    options.follow_game_size = 1;
    unsigned width = 0;
    unsigned height = 0;
    const std::string resolution = OptionValue("shadps4_game_resolution", "default");
    if (std::sscanf(resolution.c_str(), "%ux%u", &width, &height) == 2 && width >= 320 &&
        width <= Libretro::MAX_WIDTH && height >= 180 && height <= Libretro::MAX_HEIGHT) {
        options.width = width;
        options.height = height;
        options.render_width = width;
        options.follow_game_size = 0;
    }

    options.vblank_frequency =
        static_cast<uint32_t>(std::atoi(OptionValue("shadps4_vblank_frequency", "60").c_str()));
    const std::string readbacks = OptionValue("shadps4_readbacks", "disabled");
    options.readbacks = readbacks == "precise" ? 2 : readbacks == "relaxed" ? 1 : 0;
    options.direct_memory_access = enabled("shadps4_direct_memory_access", "disabled");
    options.show_fps = enabled("shadps4_show_fps", "disabled");
    options.volume = std::clamp(std::atoi(OptionValue("shadps4_volume", "100").c_str()), 0, 500);
    options.pads = static_cast<uint32_t>(std::clamp(
        std::atoi(OptionValue("shadps4_controllers", "1").c_str()), 1,
        static_cast<int>(Libretro::MAX_PADS)));

    options.console_language =
        std::clamp(std::atoi(OptionValue("shadps4_console_language", "1").c_str()), 0, 30);
    options.dev_kit = enabled("shadps4_dev_kit", "disabled");
    options.extra_dmem_mbytes =
        std::clamp(std::atoi(OptionValue("shadps4_extra_memory", "0").c_str()), 0, 20000);
    options.readback_linear_images = enabled("shadps4_readback_linear_images", "disabled");
    options.shader_cache = enabled("shadps4_shader_cache", "disabled");
    options.copy_gpu_buffers = enabled("shadps4_copy_gpu_buffers", "enabled");
    options.gpu_frames = enabled("shadps4_gpu_frames", "disabled");
    options.gpu_socket = -1;
    options.trophy_popup = enabled("shadps4_trophy_notifications", "enabled");
    const std::string side = OptionValue("shadps4_trophy_position", "right");
    options.trophy_side = side == "left" ? 0 : side == "top" ? 2 : side == "bottom" ? 3 : 1;
    options.trophy_seconds = static_cast<uint32_t>(
        std::clamp(std::atoi(OptionValue("shadps4_trophy_duration", "6").c_str()), 1, 10));
    options.log_enable = enabled("shadps4_log", "enabled");
    options.log_separate = enabled("shadps4_log_separate", "disabled");
    options.log_sync = enabled("shadps4_log_sync", "enabled");
}

void Notify(const char* text) {
    struct retro_message_ext message {};
    message.msg = text;
    message.duration = 6000;
    message.priority = 3;
    message.level = RETRO_LOG_ERROR;
    message.target = RETRO_MESSAGE_TARGET_ALL;
    message.type = RETRO_MESSAGE_TYPE_NOTIFICATION;
    message.progress = -1;
    unsigned version = 0;
    if (environ_cb(RETRO_ENVIRONMENT_GET_MESSAGE_INTERFACE_VERSION, &version) && version >= 1) {
        environ_cb(RETRO_ENVIRONMENT_SET_MESSAGE_EXT, &message);
    } else {
        struct retro_message old = {text, 360};
        environ_cb(RETRO_ENVIRONMENT_SET_MESSAGE, &old);
    }
    Log(RETRO_LOG_ERROR, "%s", text);
}

// A message with a progress bar, where the frontend draws one.
void NotifyProgress(const char* text, int percent) {
    struct retro_message_ext message {};
    message.msg = text;
    message.duration = 3000;
    message.priority = 2;
    message.level = RETRO_LOG_INFO;
    message.target = RETRO_MESSAGE_TARGET_ALL;
    message.type = RETRO_MESSAGE_TYPE_PROGRESS;
    message.progress = static_cast<int8_t>(percent);
    unsigned version = 0;
    if (environ_cb(RETRO_ENVIRONMENT_GET_MESSAGE_INTERFACE_VERSION, &version) && version >= 1) {
        environ_cb(RETRO_ENVIRONMENT_SET_MESSAGE_EXT, &message);
    } else {
        struct retro_message old = {text, 180};
        environ_cb(RETRO_ENVIRONMENT_SET_MESSAGE, &old);
    }
}

// The frontend's paths are UTF-8.
std::filesystem::path Utf8Path(const std::string& text) {
    return std::filesystem::path(
        std::u8string_view(reinterpret_cast<const char8_t*>(text.data()), text.size()));
}

std::string Utf8String(const std::filesystem::path& path) {
    const std::u8string text = path.u8string();
    return std::string(reinterpret_cast<const char*>(text.data()), text.size());
}

bool IsPkgPath(const std::string& path) {
    const auto dot = path.find_last_of('.');
    if (dot == std::string::npos) {
        return false;
    }
    std::string extension = path.substr(dot);
    std::transform(extension.begin(), extension.end(), extension.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return extension == ".pkg";
}

// PKG Content's Install, on a thread of its own: extracting a game takes minutes, and the
// frontend's thread must not wait on it. retro_run starts the game once it is done. The two
// threads share nothing but this, through the atomics: what the worker writes before it sets
// done, the frontend's thread reads after it sees it set.
struct PkgInstallJob {
    sthread_t* thread = nullptr;
    std::string game;
    std::string user_dir;
    std::string title_id;
    std::string update;
    std::string addcont;
    // Written by the worker
    std::string boot;
    std::string error;
    std::atomic<int> percent{0};
    std::atomic<bool> cancel{false};
    std::atomic<bool> succeeded{false};
    std::atomic<bool> done{false};
    // Read by the frontend's thread only
    int told_percent = -1;
};
PkgInstallJob* pkg_install = nullptr;

// The core option PKG Content's Install: a PKG's files extracted into dir. A dir that holds
// the same PKG already (by its marker file) is kept as it is; one that holds another is
// replaced.
bool InstallPkg(PkgInstallJob& job, const std::filesystem::path& pkg,
                const std::filesystem::path& dir, const std::string& user_dir) {
    std::error_code ec;
    const auto marker = dir / ".libretro_pkg";
    const std::string stamp = Utf8String(pkg.filename()) + "\n" +
                              std::to_string(std::filesystem::file_size(pkg, ec)) + "\n";
    if (filestream_matches_buf(Utf8String(marker).c_str(), stamp.data(), stamp.size())) {
        Log(RETRO_LOG_INFO, "%s is installed in %s", Utf8String(pkg).c_str(),
            Utf8String(dir).c_str());
        return true;
    }

    Libretro::PkgReader reader;
    std::string error;
    if (!reader.Open(pkg, Libretro::PkgKeysFile(Utf8Path(user_dir)), error)) {
        job.error = "shadPS4: the PKG cannot be read: " + error;
        return false;
    }
    const auto files = reader.Files();
    u64 total = 0;
    for (const auto& rel : files) {
        total += reader.Find(rel)->size;
    }
    std::filesystem::create_directories(dir.parent_path(), ec);
    const auto space = std::filesystem::space(dir.parent_path(), ec);
    if (!ec && space.available < total) {
        job.error = "shadPS4: installing " + Utf8String(pkg.filename()) + " needs " +
                    std::to_string(total >> 20) + " MiB, and there are " +
                    std::to_string(space.available >> 20) + " MiB free";
        return false;
    }

    Log(RETRO_LOG_INFO, "Installing %s into %s: %zu files, %llu MiB", Utf8String(pkg).c_str(),
        Utf8String(dir).c_str(), files.size(), static_cast<unsigned long long>(total >> 20));
    Common::FS::RemoveAll(dir);
    std::vector<u8> buffer(4 << 20);
    u64 done = 0;
    int told = 0;
    for (const auto& rel : files) {
        const auto* node = reader.Find(rel);
        const auto target = dir / Utf8Path(rel);
        std::filesystem::create_directories(target.parent_path(), ec);
        // Written front to back once, with no stdio buffer between
        const Common::FS::IOFile out(target, Common::FS::FileAccessMode::Create,
                                     Common::FS::FileHint::Sequential);
        bool written = out.IsOpen();
        for (u64 at = 0; written && at < node->size;) {
            if (job.cancel.load(std::memory_order_relaxed)) {
                job.error = "shadPS4: the install was stopped";
                return false;
            }
            const u64 read = reader.Read(*node, at, buffer.size(), buffer.data());
            if (read == 0) {
                written = false;
                break;
            }
            written = out.WriteRaw<u8>(buffer.data(), read) == read;
            at += read;
            done += read;
        }
        if (!written) {
            job.error = "shadPS4: " + Utf8String(target) + " could not be written";
            return false;
        }
        if (total) {
            job.percent.store(static_cast<int>(done * 100 / total), std::memory_order_relaxed);
        }
        if (total && done * 10 / total > static_cast<u64>(told)) {
            told = static_cast<int>(done * 10 / total);
            Log(RETRO_LOG_INFO, "Installing: %d %%", told * 10);
        }
    }
    filestream_write_file(Utf8String(marker).c_str(), stamp.data(), static_cast<s64>(stamp.size()));
    return true;
}

// Install mode: the game into games/<title ID>, its update into games/<title ID>-UPDATE and
// each additional content into addcont/<title ID>/<entitlement label>, the folders the
// emulator looks in; boot is then the installed eboot.bin.
bool InstallPkgContent(PkgInstallJob& job) {
    const std::string& game = job.game;
    const std::string& user_dir = job.user_dir;
    const std::string& title_id = job.title_id;
    const std::string& update = job.update;
    const std::string& addcont = job.addcont;
    std::string& boot = job.boot;
    const auto user = Utf8Path(user_dir);
    const auto game_dir = user / "games" / Utf8Path(title_id);
    const auto update_dir = user / "games" / Utf8Path(title_id + "-UPDATE");
    if (!InstallPkg(job, Utf8Path(game), game_dir, user_dir)) {
        return false;
    }
    std::error_code ec;
    if (!update.empty()) {
        if (!InstallPkg(job, Utf8Path(update), update_dir, user_dir)) {
            return false;
        }
    } else if (std::filesystem::exists(update_dir / ".libretro_pkg", ec)) {
        // An update this core installed whose PKG is gone.
        Log(RETRO_LOG_INFO, "The update's PKG is gone: %s removed", Utf8String(update_dir).c_str());
        Common::FS::RemoveAll(update_dir);
    }
    for (std::string_view paths{addcont}; !paths.empty();) {
        const auto end = paths.find('\n');
        const std::string path{paths.substr(0, end)};
        paths = end == std::string_view::npos ? std::string_view{} : paths.substr(end + 1);
        Libretro::PkgSummary summary;
        std::string error;
        if (path.empty() || !Libretro::ReadPkgSummary(Utf8Path(path), summary, error)) {
            continue;
        }
        // The content ID is UPxxxx-CUSAxxxxx_xx-<entitlement label>.
        const auto dash = summary.content_id.find_last_of('-');
        if (dash == std::string::npos || dash + 1 >= summary.content_id.size()) {
            continue;
        }
        const auto label = summary.content_id.substr(dash + 1);
        if (!InstallPkg(job, Utf8Path(path),
                        user / "addcont" / Utf8Path(title_id) / Utf8Path(label), user_dir)) {
            return false;
        }
    }
    boot = Utf8String(game_dir / "eboot.bin");
    if (boot.size() >= Libretro::PATH_BYTES) {
        job.error = "shadPS4: the installed game's path is too long";
        return false;
    }
    return true;
}

void PkgInstallThread(void* data) {
    auto& job = *static_cast<PkgInstallJob*>(data);
    job.succeeded.store(InstallPkgContent(job), std::memory_order_relaxed);
    job.done.store(true, std::memory_order_release);
}

// Stops an install, waiting at most for the block being written.
void StopPkgInstall() {
    if (!pkg_install) {
        return;
    }
    pkg_install->cancel.store(true, std::memory_order_relaxed);
    sthread_join(pkg_install->thread);
    delete pkg_install;
    pkg_install = nullptr;
}

// A game run from its PKG: the keys are there, the PKG is a game's, and its update (the
// highest version) and additional content are the PKGs of the same title beside it.
bool FindPkgContent(const std::string& game, const std::string& user_dir, std::string& title_id,
                    std::string& update, std::string& addcont) {
    Libretro::PkgKeys keys;
    std::string error;
    if (!Libretro::LoadPkgKeys(Libretro::PkgKeysFile(Utf8Path(user_dir)), keys,
                               error)) {
        Notify(("shadPS4: " + error).c_str());
        return false;
    }
    const std::filesystem::path game_path = Utf8Path(game);
    Libretro::PkgSummary summary;
    if (!Libretro::ReadPkgSummary(game_path, summary, error)) {
        Notify(("shadPS4: the PKG cannot be read: " + error).c_str());
        return false;
    }
    if (summary.category == "gp" || summary.category == "ac") {
        Notify("shadPS4: this PKG is an update or additional content; load the game's PKG, the "
               "others are taken from beside it");
        return false;
    }
    Log(RETRO_LOG_INFO, "PKG %s: %s, version %s", game.c_str(), summary.title_id.c_str(),
        summary.app_ver.c_str());
    title_id = summary.title_id;

    std::string update_version;
    std::error_code ec;
    for (const auto& entry : std::filesystem::directory_iterator(game_path.parent_path(), ec)) {
        const std::string path = Utf8String(entry.path());
        if (!entry.is_regular_file(ec) || !IsPkgPath(path) || entry.path() == game_path) {
            continue;
        }
        Libretro::PkgSummary other;
        if (!Libretro::ReadPkgSummary(entry.path(), other, error) ||
            other.title_id != summary.title_id) {
            continue;
        }
        if (other.category == "gp" && (update.empty() || other.app_ver > update_version)) {
            update = path;
            update_version = other.app_ver;
        } else if (other.category == "ac") {
            if (addcont.size() + path.size() + 1 < Libretro::ADDCONT_BYTES) {
                addcont += path + "\n";
                Log(RETRO_LOG_INFO, "Additional content %s", path.c_str());
            } else {
                Log(RETRO_LOG_WARN, "Too many additional contents: %s left out", path.c_str());
            }
        }
    }
    if (!update.empty()) {
        if (update.size() >= Libretro::PATH_BYTES) {
            Log(RETRO_LOG_WARN, "The update's path is too long: %s left out", update.c_str());
            update.clear();
        } else {
            Log(RETRO_LOG_INFO, "Update %s, version %s", update.c_str(), update_version.c_str());
        }
    }
    return true;
}

// ---- The shared memory and the emulator's process, per platform ----

#ifdef _WIN32

std::wstring ModulePath() {
    HMODULE module = nullptr;
    if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                                GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                            reinterpret_cast<LPCWSTR>(&retro_api_version), &module)) {
        return {};
    }
    std::wstring path(32768, L'\0');
    const DWORD length = GetModuleFileNameW(module, path.data(), static_cast<DWORD>(path.size()));
    path.resize(length);
    return path;
}

bool CreateShared(std::string& argument) {
    static unsigned counter = 0;
    char name[96];
    std::snprintf(name, sizeof(name), "Local\\shadps4_libretro_%lu_%u",
                  static_cast<unsigned long>(GetCurrentProcessId()), ++counter);
    const ULONGLONG size = Libretro::SHARED_BYTES;
    shared_mapping =
        CreateFileMappingA(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE,
                           static_cast<DWORD>(size >> 32), static_cast<DWORD>(size), name);
    if (!shared_mapping) {
        return false;
    }
    shared = static_cast<SharedState*>(
        MapViewOfFile(shared_mapping, FILE_MAP_ALL_ACCESS, 0, 0, Libretro::SHARED_BYTES));
    argument = name;
    return shared != nullptr;
}

void DestroyShared() {
    if (shared) {
        UnmapViewOfFile(shared);
        shared = nullptr;
    }
    if (shared_mapping) {
        CloseHandle(shared_mapping);
        shared_mapping = nullptr;
    }
}

bool StartEmulator(const std::string& argument) {
    const std::wstring module = ModulePath();
    if (module.empty()) {
        Log(RETRO_LOG_ERROR, "The core's own path could not be found");
        return false;
    }
    std::wstring rundll(MAX_PATH, L'\0');
    rundll.resize(GetSystemDirectoryW(rundll.data(), MAX_PATH));
    rundll += L"\\rundll32.exe";

    std::wstring command = L"\"" + rundll + L"\" \"" + module + L"\",retro_host_main ";
    command.append(argument.begin(), argument.end());

    // Ended with the frontend, however the frontend ends.
    emulator_job = CreateJobObjectW(nullptr, nullptr);
    if (emulator_job) {
        JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};
        limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
        SetInformationJobObject(emulator_job, JobObjectExtendedLimitInformation, &limits,
                                sizeof(limits));
    }

    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION process{};
    if (!CreateProcessW(rundll.c_str(), command.data(), nullptr, nullptr, FALSE,
                        CREATE_NO_WINDOW | CREATE_SUSPENDED, nullptr, nullptr, &startup,
                        &process)) {
        Log(RETRO_LOG_ERROR, "rundll32 could not be started (error %lu)",
            static_cast<unsigned long>(GetLastError()));
        return false;
    }
    if (emulator_job) {
        AssignProcessToJobObject(emulator_job, process.hProcess);
    }
    ResumeThread(process.hThread);
    CloseHandle(process.hThread);
    emulator_process = process.hProcess;
    return true;
}

// Whether the emulator's process has ended, with what, and whether it was killed rather than
// ended by itself. An unhandled exception leaves the status of the exception behind, which has
// one of the two top severity bits set; a status a program exits with is a small number.
bool EmulatorEnded(int& code, bool& crashed) {
    if (!emulator_process || WaitForSingleObject(emulator_process, 0) != WAIT_OBJECT_0) {
        return false;
    }
    DWORD exit_code = 0;
    GetExitCodeProcess(emulator_process, &exit_code);
    crashed = (exit_code >> 30) >= 2;
    code = static_cast<int>(exit_code);
    return true;
}

void StopEmulator() {
    if (emulator_process) {
        // It ends itself on the quit flag; what has not within three seconds is ended.
        if (WaitForSingleObject(emulator_process, 3000) != WAIT_OBJECT_0) {
            TerminateProcess(emulator_process, 1);
            WaitForSingleObject(emulator_process, 2000);
        }
        CloseHandle(emulator_process);
        emulator_process = nullptr;
    }
    if (emulator_job) {
        CloseHandle(emulator_job);
        emulator_job = nullptr;
    }
}

#else

std::string ModulePath() {
    Dl_info info{};
    if (!dladdr(reinterpret_cast<void*>(&retro_api_version), &info) || !info.dli_fname) {
        return {};
    }
    char resolved[PATH_MAX];
    if (realpath(info.dli_fname, resolved)) {
        return resolved;
    }
    return info.dli_fname;
}

bool CreateShared(std::string& argument) {
    // Without MFD_CLOEXEC: the emulator's process inherits the descriptor.
    shared_fd = memfd_create("shadps4_libretro_shared", 0);
    if (shared_fd < 0 || ftruncate(shared_fd, static_cast<off_t>(Libretro::SHARED_BYTES)) != 0) {
        return false;
    }
    void* view = mmap(nullptr, Libretro::SHARED_BYTES, PROT_READ | PROT_WRITE, MAP_SHARED,
                      shared_fd, 0);
    if (view == MAP_FAILED) {
        return false;
    }
    shared = static_cast<SharedState*>(view);
    argument = std::to_string(shared_fd);
    return true;
}

void DestroyShared() {
    if (shared) {
        munmap(shared, Libretro::SHARED_BYTES);
        shared = nullptr;
    }
    if (shared_fd >= 0) {
        close(shared_fd);
        shared_fd = -1;
    }
}

// The launcher as a file in memory, opened only for reading - a file open for writing cannot
// be run.
int LauncherDescriptor() {
    const int fd = memfd_create("shadps4_libretro_host", 0);
    if (fd < 0) {
        return -1;
    }
    unsigned long done = 0;
    while (done < g_libretro_host_size) {
        const ssize_t wrote = write(fd, g_libretro_host + done, g_libretro_host_size - done);
        if (wrote <= 0) {
            close(fd);
            return -1;
        }
        done += static_cast<unsigned long>(wrote);
    }
    fchmod(fd, 0700);
    char path[64];
    std::snprintf(path, sizeof(path), "/proc/self/fd/%d", fd);
    const int read_only = open(path, O_RDONLY);
    close(fd);
    return read_only;
}

// With the FEXCore backend the aarch64 emulator runs the game's code itself, in a process of
// this core's own kind as on x86-64
#if defined(__aarch64__) && !defined(SHADPS4_ENABLE_FEX_GUEST_CPU)
#ifdef LIBRETRO_X86_64_PARTS
// box64 and the x86-64 build of the core, carried in this one (CMakeLists.txt)
extern "C" const unsigned char g_part_box64[], g_part_box64_end[];
extern "C" const unsigned char g_part_shadps4_libretro_host[], g_part_shadps4_libretro_host_end[];
extern "C" const unsigned char g_part_shadps4_libretro_so[], g_part_shadps4_libretro_so_end[];
#endif

// Writes what the core carries into dir, unless the files there are from this very build,
// as the "build" file there says. Each is written beside its place and moved into it.
bool WriteParts(const std::string& dir) {
#ifdef LIBRETRO_X86_64_PARTS
    const std::string stamp = dir + "build";
    if (filestream_matches_buf(stamp.c_str(), Common::g_scm_rev, std::strlen(Common::g_scm_rev))) {
        return true;
    }
    mkdir(dir.c_str(), 0755);
    static const struct {
        const char* name;
        const unsigned char* begin;
        const unsigned char* end;
    } parts[] = {
        {"box64", g_part_box64, g_part_box64_end},
        {"shadps4_libretro_host", g_part_shadps4_libretro_host, g_part_shadps4_libretro_host_end},
        {"shadps4_libretro.so", g_part_shadps4_libretro_so, g_part_shadps4_libretro_so_end},
    };
    for (const auto& part : parts) {
        const std::string path = dir + part.name;
        const std::string temporary = path + ".new";
        // Straight from where the core carries it
        const bool written = filestream_write_file(temporary.c_str(), part.begin,
                                                   static_cast<s64>(part.end - part.begin));
        if (!written || chmod(temporary.c_str(), 0755) != 0 ||
            filestream_rename(temporary.c_str(), path.c_str()) != 0) {
            Log(RETRO_LOG_ERROR, "%s could not be written: %s", path.c_str(), std::strerror(errno));
            filestream_delete(temporary.c_str());
            return false;
        }
    }
    if (!filestream_write_file(stamp.c_str(), Common::g_scm_rev,
                               static_cast<s64>(std::strlen(Common::g_scm_rev)))) {
        Log(RETRO_LOG_ERROR, "%s could not be written", stamp.c_str());
        return false;
    }
    Log(RETRO_LOG_INFO, "box64 and the x86-64 emulator written to %s", dir.c_str());
    return true;
#else
    Log(RETRO_LOG_ERROR, "This aarch64 build carries no x86-64 emulator: it was built "
                         "without LIBRETRO_X86_64_PARTS");
    return false;
#endif
}

// On an ARM machine the emulator cannot run in a process of this core's own kind: it runs the
// game's x86-64 code on the CPU. Its process is the x86-64 build of the core instead, started
// through box64, which translates it; the two halves trade through the same shared memory.
// The core carries both and writes them into the frontend's system directory, in
// shadPS4/x86_64/, as an executable has to be a file there to be started.
bool StartEmulator(const std::string& argument) {
    const std::string dir = SystemDirectory() + "/x86_64/";
    if (!WriteParts(dir)) {
        return false;
    }
    std::string box64 = dir + "box64";
    std::string launcher = dir + "shadps4_libretro_host";
    std::string module = dir + "shadps4_libretro.so";

    // The frontend's environment. With Exact CPU Flags box64 computes the flags exactly
    // (SAFEFLAGS=2): with its faster default Sonic Mania's music, which the game decodes
    // itself, stayed silent while its sound effects played. Costs some speed.
    std::vector<std::string> variables;
    for (char** variable = environ; variable && *variable; ++variable) {
        if (std::strncmp(*variable, "BOX64_DYNAREC_SAFEFLAGS=", 24) != 0) {
            variables.emplace_back(*variable);
        }
    }
    if (OptionValue("shadps4_exact_flags", "enabled") == "enabled") {
        variables.emplace_back("BOX64_DYNAREC_SAFEFLAGS=2");
    }
    std::vector<char*> environment;
    for (std::string& variable : variables) {
        environment.push_back(variable.data());
    }
    environment.push_back(nullptr);

    std::string shared_argument = argument;
    char* const arguments[] = {box64.data(), launcher.data(), module.data(),
                               shared_argument.data(), nullptr};
    pid_t pid = -1;
    const int result =
        posix_spawn(&pid, box64.c_str(), nullptr, nullptr, arguments, environment.data());
    if (result != 0) {
        Log(RETRO_LOG_ERROR, "box64 could not be started: %s", std::strerror(result));
        return false;
    }
    Log(RETRO_LOG_INFO, "The emulator runs as an x86-64 process through %s", box64.c_str());
    emulator_pid = pid;
    return true;
}
#else
bool StartEmulator(const std::string& argument) {
    const std::string module = ModulePath();
    if (module.empty()) {
        Log(RETRO_LOG_ERROR, "The core's own path could not be found");
        return false;
    }
    const int launcher = LauncherDescriptor();
    if (launcher < 0) {
        Log(RETRO_LOG_ERROR, "The launcher could not be put in memory: %s", std::strerror(errno));
        return false;
    }
    char launcher_path[64];
    std::snprintf(launcher_path, sizeof(launcher_path), "/proc/self/fd/%d", launcher);

    std::string name = "shadps4_libretro_host";
    std::string module_argument = module;
    std::string shared_argument = argument;
    char* const arguments[] = {name.data(), module_argument.data(), shared_argument.data(),
                               nullptr};
    pid_t pid = -1;
    const int result = posix_spawn(&pid, launcher_path, nullptr, nullptr, arguments, environ);
    close(launcher);
    if (result != 0) {
        Log(RETRO_LOG_ERROR, "The launcher could not be started: %s", std::strerror(result));
        return false;
    }
    emulator_pid = pid;
    return true;
}
#endif

// Whether the emulator's process has ended, with what, and whether it was killed rather than
// ended by itself: a process that ends by itself leaves the status it exited with, one that is
// killed leaves the signal that killed it.
bool EmulatorEnded(int& code, bool& crashed) {
    if (emulator_pid <= 0) {
        return false;
    }
    int status = 0;
    if (waitpid(emulator_pid, &status, WNOHANG) != emulator_pid) {
        return false;
    }
    crashed = !WIFEXITED(status);
    code = crashed ? WTERMSIG(status) : WEXITSTATUS(status);
    emulator_pid = -1;
    return true;
}

void StopEmulator() {
    if (emulator_pid <= 0) {
        return;
    }
    // It ends itself on the quit flag; what has not within three seconds is ended.
    int status = 0;
    for (int i = 0; i < 300; ++i) {
        if (waitpid(emulator_pid, &status, WNOHANG) == emulator_pid) {
            emulator_pid = -1;
            return;
        }
        usleep(10 * 1000);
    }
    kill(emulator_pid, SIGKILL);
    waitpid(emulator_pid, &status, 0);
    emulator_pid = -1;
}

#endif

// ---- The frontend's side of the shared memory ----

void SendInput() {
    using Libraries::Pad::OrbisPadButtonDataOffset;

    if (!input_state_cb) {
        return;
    }
    static const struct {
        unsigned id;
        OrbisPadButtonDataOffset button;
    } buttons[] = {
        {RETRO_DEVICE_ID_JOYPAD_B, OrbisPadButtonDataOffset::Cross},
        {RETRO_DEVICE_ID_JOYPAD_A, OrbisPadButtonDataOffset::Circle},
        {RETRO_DEVICE_ID_JOYPAD_Y, OrbisPadButtonDataOffset::Square},
        {RETRO_DEVICE_ID_JOYPAD_X, OrbisPadButtonDataOffset::Triangle},
        {RETRO_DEVICE_ID_JOYPAD_UP, OrbisPadButtonDataOffset::Up},
        {RETRO_DEVICE_ID_JOYPAD_DOWN, OrbisPadButtonDataOffset::Down},
        {RETRO_DEVICE_ID_JOYPAD_LEFT, OrbisPadButtonDataOffset::Left},
        {RETRO_DEVICE_ID_JOYPAD_RIGHT, OrbisPadButtonDataOffset::Right},
        {RETRO_DEVICE_ID_JOYPAD_L, OrbisPadButtonDataOffset::L1},
        {RETRO_DEVICE_ID_JOYPAD_R, OrbisPadButtonDataOffset::R1},
        {RETRO_DEVICE_ID_JOYPAD_L2, OrbisPadButtonDataOffset::L2},
        {RETRO_DEVICE_ID_JOYPAD_R2, OrbisPadButtonDataOffset::R2},
        {RETRO_DEVICE_ID_JOYPAD_L3, OrbisPadButtonDataOffset::L3},
        {RETRO_DEVICE_ID_JOYPAD_R3, OrbisPadButtonDataOffset::R3},
        {RETRO_DEVICE_ID_JOYPAD_START, OrbisPadButtonDataOffset::Options},
        {RETRO_DEVICE_ID_JOYPAD_SELECT, OrbisPadButtonDataOffset::TouchPad},
    };
    for (unsigned pad = 0; pad < shared->options.pads && pad < Libretro::MAX_PADS; ++pad) {
        uint32_t held = 0;
        for (const auto& entry : buttons) {
            if (input_state_cb(pad, RETRO_DEVICE_JOYPAD, 0, entry.id)) {
                held |= static_cast<uint32_t>(entry.button);
            }
        }
        shared->buttons[pad].store(held, std::memory_order_release);

        // libretro's -0x8000..0x7fff to the pad's 0..255.
        const auto stick = [pad](unsigned index, unsigned id) -> uint32_t {
            const int raw = input_state_cb(pad, RETRO_DEVICE_ANALOG, index, id);
            return static_cast<uint32_t>(std::clamp((raw + 0x8000) * 255 / 0xffff, 0, 255));
        };
        auto& axes = shared->axes[pad];
        axes[Libretro::SHARED_AXIS_LEFT_X] =
            stick(RETRO_DEVICE_INDEX_ANALOG_LEFT, RETRO_DEVICE_ID_ANALOG_X);
        axes[Libretro::SHARED_AXIS_LEFT_Y] =
            stick(RETRO_DEVICE_INDEX_ANALOG_LEFT, RETRO_DEVICE_ID_ANALOG_Y);
        axes[Libretro::SHARED_AXIS_RIGHT_X] =
            stick(RETRO_DEVICE_INDEX_ANALOG_RIGHT, RETRO_DEVICE_ID_ANALOG_X);
        axes[Libretro::SHARED_AXIS_RIGHT_Y] =
            stick(RETRO_DEVICE_INDEX_ANALOG_RIGHT, RETRO_DEVICE_ID_ANALOG_Y);

        // Triggers: analog where the frontend has it, else the digital button.
        const auto trigger = [pad](unsigned id) -> uint32_t {
            int raw =
                input_state_cb(pad, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_BUTTON, id);
            if (raw == 0 && input_state_cb(pad, RETRO_DEVICE_JOYPAD, 0, id)) {
                raw = 0x7fff;
            }
            return static_cast<uint32_t>(std::clamp(raw * 255 / 0x7fff, 0, 255));
        };
        axes[Libretro::SHARED_AXIS_TRIGGER_LEFT] = trigger(RETRO_DEVICE_ID_JOYPAD_L2);
        axes[Libretro::SHARED_AXIS_TRIGGER_RIGHT] = trigger(RETRO_DEVICE_ID_JOYPAD_R2);
    }
}

void ApplyRumble() {
    static uint32_t applied[Libretro::MAX_PADS] = {};
    if (!rumble.set_rumble_state) {
        return;
    }
    for (unsigned pad = 0; pad < Libretro::MAX_PADS; ++pad) {
        const uint32_t wanted = shared->rumble[pad].load(std::memory_order_acquire);
        if (wanted == applied[pad]) {
            continue;
        }
        applied[pad] = wanted;
        const uint32_t small_motor = wanted & 0xff;
        const uint32_t large_motor = (wanted >> 8) & 0xff;
        rumble.set_rumble_state(pad, RETRO_RUMBLE_WEAK,
                                static_cast<uint16_t>(small_motor * 0x101));
        rumble.set_rumble_state(pad, RETRO_RUMBLE_STRONG,
                                static_cast<uint16_t>(large_motor * 0x101));
    }
}

// The newest frame, if there is one newer than the one shown. A frame stays whole until the
// emulator begins the second one after it, so the copy is taken only if nothing was published
// while it was made; else the newer one is tried.
void TakeFrame() {
    for (int attempt = 0; attempt < 3; ++attempt) {
        const uint32_t latest = shared->frame_latest.load();
        if (latest == 0 || latest == shown_latest) {
            return;
        }
        // Held first, then looked at again: once the slot is held and still the newest, the
        // emulator does not write into it, and it is shown from where it is.
        const uint32_t slot = latest & Libretro::FRAME_SLOT_MASK;
        if (slot >= (gpu_frames ? Libretro::FRAME_IMAGES : Libretro::FRAME_SLOTS)) {
            return;
        }
        shared->frame_held.store(slot);
        if (shared->frame_latest.load() != latest) {
            continue;
        }
        const uint32_t width = shared->frame_size[slot].width;
        const uint32_t height = shared->frame_size[slot].height;
        if (width == 0 || height == 0 || width > Libretro::MAX_WIDTH ||
            height > Libretro::MAX_HEIGHT) {
            return;
        }
        if (gpu_frames) {
            shown_image = slot;
        } else {
            shown_frame = Libretro::SharedFrame(shared, slot);
        }
        shown_latest = latest;
        if (width != shown_width || height != shown_height) {
            shown_width = width;
            shown_height = height;
            AnnounceSize(width, height);
        }
        return;
    }
}

// One frame's worth of sound out of the ring; what is not there is silence.
void TakeAudio(int16_t* out) {
    const uint32_t read = shared->audio_read.load(std::memory_order_relaxed);
    const uint32_t written = shared->audio_written.load(std::memory_order_acquire);
    const uint32_t frames = std::min<uint32_t>(written - read, kAudioFrames);
    for (uint32_t i = 0; i < frames; ++i) {
        const uint32_t at = (read + i) & (Libretro::AUDIO_RING_FRAMES - 1);
        out[i * 2 + 0] = shared->audio[at * 2 + 0];
        out[i * 2 + 1] = shared->audio[at * 2 + 1];
    }
    std::memset(out + frames * 2, 0, (kAudioFrames - frames) * 2 * sizeof(int16_t));
    shared->audio_read.store(read + frames, std::memory_order_release);
}

// The names of the pad's buttons and sticks, for the ports that are controllers: what the
// frontend shows on its remapping screen. They follow SendInput.
void DescribeInput(std::uint32_t pads) {
    static constexpr struct {
        unsigned device;
        unsigned index;
        unsigned id;
        const char* name;
    } controls[] = {
        {RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_B, "Cross"},
        {RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_A, "Circle"},
        {RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_Y, "Square"},
        {RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_X, "Triangle"},
        {RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_UP, "D-Pad Up"},
        {RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_DOWN, "D-Pad Down"},
        {RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_LEFT, "D-Pad Left"},
        {RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_RIGHT, "D-Pad Right"},
        {RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L, "L1"},
        {RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R, "R1"},
        {RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L2, "L2"},
        {RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R2, "R2"},
        {RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L3, "L3"},
        {RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R3, "R3"},
        {RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_START, "Options"},
        {RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_SELECT, "Touchpad Button"},
        {RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_LEFT, RETRO_DEVICE_ID_ANALOG_X,
         "Left Stick X"},
        {RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_LEFT, RETRO_DEVICE_ID_ANALOG_Y,
         "Left Stick Y"},
        {RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_RIGHT, RETRO_DEVICE_ID_ANALOG_X,
         "Right Stick X"},
        {RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_RIGHT, RETRO_DEVICE_ID_ANALOG_Y,
         "Right Stick Y"},
    };
    // Kept: the frontend may hold on to the pointer.
    static std::vector<retro_input_descriptor> descriptors;
    descriptors.clear();
    for (unsigned pad = 0; pad < pads && pad < Libretro::MAX_PADS; ++pad) {
        for (const auto& control : controls) {
            descriptors.push_back({pad, control.device, control.index, control.id, control.name});
        }
    }
    descriptors.push_back({});
    environ_cb(RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS, descriptors.data());
}

} // namespace

extern "C" {

RETRO_API unsigned retro_api_version(void) {
    return RETRO_API_VERSION;
}

RETRO_API void retro_set_environment(retro_environment_t cb) {
    environ_cb = cb;

    struct retro_log_callback logging {};
    if (cb(RETRO_ENVIRONMENT_GET_LOG_INTERFACE, &logging)) {
        log_cb = logging.log;
    }

    bool categories_supported = false;
    libretro_set_core_options(cb, &categories_supported);
}

RETRO_API void retro_set_video_refresh(retro_video_refresh_t cb) {
    video_cb = cb;
}

RETRO_API void retro_set_audio_sample(retro_audio_sample_t) {}

RETRO_API void retro_set_audio_sample_batch(retro_audio_sample_batch_t cb) {
    audio_batch_cb = cb;
}

RETRO_API void retro_set_input_poll(retro_input_poll_t cb) {
    input_poll_cb = cb;
}

RETRO_API void retro_set_input_state(retro_input_state_t cb) {
    input_state_cb = cb;
}

RETRO_API void retro_init(void) {}

RETRO_API void retro_deinit(void) {}

RETRO_API void retro_get_system_info(struct retro_system_info* info) {
    std::memset(info, 0, sizeof(*info));
    info->library_name = "shadPS4";
    info->library_version = Common::g_version;
    info->valid_extensions = "bin|pkg";
    info->need_fullpath = true;
    info->block_extract = true;
}

RETRO_API void retro_get_system_av_info(struct retro_system_av_info* info) {
    std::memset(info, 0, sizeof(*info));
    info->geometry.base_width = output_width;
    info->geometry.base_height = output_height;
    info->geometry.max_width = output_width;
    info->geometry.max_height = output_height;
    declared_max_width = output_width;
    declared_max_height = output_height;
    announced_width = output_width;
    announced_height = output_height;
    info->geometry.aspect_ratio = 16.0f / 9.0f;
    info->timing.fps = kFps;
    info->timing.sample_rate = kSampleRate;
}

RETRO_API void retro_set_controller_port_device(unsigned, unsigned) {}

// A reset is an unload and a load: the emulator stops only with its process, so the game
// starts again in a new one. The game's path is taken from the shared memory before the
// unload frees it.
RETRO_API void retro_reset(void) {
    if (!shared) {
        return;
    }
    const std::string path = shared->eboot_path;
    retro_unload_game();
    retro_game_info game{};
    game.path = path.c_str();
    if (!retro_load_game(&game)) {
        Notify("shadPS4: the game could not be started again; see the log");
    }
}

// The game started in the emulator's process, over the memory shared with it.
static bool StartGame(const std::string& boot_path, const std::string& user_dir,
               const std::string& update_path, const std::string& addcont_paths) {
    std::string argument;
    if (!CreateShared(argument)) {
        Log(RETRO_LOG_ERROR, "The memory shared with the emulator could not be made");
        DestroyShared();
        return false;
    }
    // New memory is zeroed: the counters and flags start there.
    shared->magic = Libretro::SHARED_MAGIC;
    shared->version = Libretro::SHARED_VERSION;
    std::strcpy(shared->eboot_path, boot_path.c_str());
    std::strcpy(shared->user_dir, user_dir.c_str());
    std::strcpy(shared->update_path, update_path.c_str());
    std::strcpy(shared->addcont_paths, addcont_paths.c_str());
    ReadCoreOptions(shared->options);
    DescribeInput(shared->options.pads);
    // The picture on the GPU, when the option asks for it and the frontend gives a Vulkan
    // context: the emulator is told, and given its end of the socket its images come over.
    gpu_frames = false;
    gpu_failure_told = false;
    shown_image = 0;
#ifdef _WIN32
    // The images come through the shared memory.
    if (shared->options.gpu_frames != 0 && Libretro::GpuFrames::Request(environ_cb, log_cb)) {
        gpu_frames = true;
        Libretro::GpuFrames::SetShared(shared);
    }
    shared->options.gpu_frames = gpu_frames ? 1 : 0;
#else
    if (shared->options.gpu_frames != 0 && Libretro::GpuFrames::Request(environ_cb, log_cb)) {
        gpu_frames = true;
        Libretro::GpuFrames::SetShared(shared);
        int pair[2];
        if (socketpair(AF_UNIX, SOCK_SEQPACKET, 0, pair) == 0) {
            gpu_socket = pair[0];
            gpu_socket_emulator = pair[1];
            fcntl(gpu_socket, F_SETFD, FD_CLOEXEC);
            shared->options.gpu_socket = gpu_socket_emulator;
        } else {
            Log(RETRO_LOG_ERROR, "No socket for the emulator's images: %s", std::strerror(errno));
        }
    }
    shared->options.gpu_frames = gpu_frames && shared->options.gpu_socket >= 0;
#endif
    output_width = shared->options.width;
    output_height = shared->options.height;
    for (auto& axes : shared->axes) {
        for (auto& axis : axes) {
            axis = 128;
        }
        axes[Libretro::SHARED_AXIS_TRIGGER_LEFT] = 0;
        axes[Libretro::SHARED_AXIS_TRIGGER_RIGHT] = 0;
    }

    shared->frame_held.store(Libretro::FRAME_SLOT_NONE);
    shown_frame = nullptr;
    shown_width = 0;
    shown_height = 0;
    shown_latest = 0;
    shutdown_requested = false;

    Log(RETRO_LOG_INFO, "shadPS4 %s (%s), user directory %s", Common::g_version,
        Common::g_scm_rev, user_dir.c_str());
    Log(RETRO_LOG_INFO, "Starting %s", boot_path.c_str());
    const bool started = StartEmulator(argument);
#ifndef _WIN32
    // The emulator has its end now, or was not started.
    if (gpu_socket_emulator >= 0) {
        close(gpu_socket_emulator);
        gpu_socket_emulator = -1;
    }
#endif
    if (!started) {
        DestroyShared();
        return false;
    }
    emulator_running = true;
    return true;
}

RETRO_API bool retro_load_game(const struct retro_game_info* game) {
    if (!game || !game->path || !*game->path) {
        Log(RETRO_LOG_ERROR, "No content: load a game's eboot.bin or PKG");
        return false;
    }
    const std::string user_dir = SystemDirectory();
    if (user_dir.empty()) {
        Log(RETRO_LOG_ERROR, "The frontend gave no system directory");
        return false;
    }
    if (std::strlen(game->path) >= Libretro::PATH_BYTES ||
        user_dir.size() >= Libretro::PATH_BYTES) {
        Log(RETRO_LOG_ERROR, "The game's path or the system directory is too long");
        return false;
    }

    enum retro_pixel_format format = RETRO_PIXEL_FORMAT_XRGB8888;
    environ_cb(RETRO_ENVIRONMENT_SET_PIXEL_FORMAT, &format);
    if (!environ_cb(RETRO_ENVIRONMENT_GET_RUMBLE_INTERFACE, &rumble)) {
        rumble = {};
    }

    // A PKG is run from as it is, or installed first and its eboot.bin run (PKG Content).
    std::string update_path;
    std::string addcont_paths;
    if (IsPkgPath(game->path)) {
        std::string title_id;
        if (!FindPkgContent(game->path, user_dir, title_id, update_path, addcont_paths)) {
            return false;
        }
        if (OptionValue("shadps4_pkg_content", "run") == "install") {
            // Installed on a thread of its own; retro_run starts the game when it is done,
            // and shows black until then, at the size the game will have.
            Libretro::SharedOptions options{};
            ReadCoreOptions(options);
            output_width = options.width;
            output_height = options.height;
            auto* job = new PkgInstallJob;
            job->game = game->path;
            job->user_dir = user_dir;
            job->title_id = title_id;
            job->update = update_path;
            job->addcont = addcont_paths;
            job->thread = sthread_create(PkgInstallThread, job);
            if (!job->thread) {
                delete job;
                Log(RETRO_LOG_ERROR, "No thread for the PKG install");
                return false;
            }
            pkg_install = job;
            NotifyProgress("shadPS4: installing the PKG", 0);
            return true;
        }
    }
    return StartGame(game->path, user_dir, update_path, addcont_paths);
}

RETRO_API bool retro_load_game_special(unsigned, const struct retro_game_info*, size_t) {
    return false;
}

RETRO_API void retro_unload_game(void) {
    StopPkgInstall();
    if (shared) {
        shared->quit.store(1, std::memory_order_release);
    }
    StopEmulator();
    // The frame shown was in the shared memory, or one of the emulator's images.
    shown_frame = nullptr;
    Libretro::GpuFrames::Close();
    Libretro::GpuFrames::SetShared(nullptr);
#ifndef _WIN32
    if (gpu_socket >= 0) {
        close(gpu_socket);
        gpu_socket = -1;
    }
#endif
    gpu_frames = false;
    DestroyShared();
    emulator_running = false;
}

// While a PKG is installed: its progress, then the game started, or the frontend asked to close
// the content when the install failed.
static void PollPkgInstall() {
    PkgInstallJob& job = *pkg_install;
    if (!job.done.load(std::memory_order_acquire)) {
        const int percent = job.percent.load(std::memory_order_relaxed);
        if (percent / 5 != job.told_percent / 5) {
            job.told_percent = percent;
            char text[96];
            std::snprintf(text, sizeof(text), "shadPS4: installing the PKG, %d %%", percent);
            NotifyProgress(text, percent);
        }
        return;
    }
    // Done: the thread has ended, so joining it does not wait
    sthread_join(job.thread);
    const bool succeeded = job.succeeded.load(std::memory_order_relaxed);
    const std::string boot = job.boot;
    const std::string user_dir = job.user_dir;
    const std::string error = job.error;
    delete pkg_install;
    pkg_install = nullptr;
    if (!succeeded || !StartGame(boot, user_dir, {}, {})) {
        if (!error.empty()) {
            Notify(error.c_str());
        }
        shutdown_requested = true;
        environ_cb(RETRO_ENVIRONMENT_SHUTDOWN, nullptr);
    }
}

RETRO_API void retro_run(void) {
    static int16_t sound[kAudioFrames * 2];

    if (input_poll_cb) {
        input_poll_cb();
    }

    if (pkg_install) {
        PollPkgInstall();
    }

    // An option changed: those that take effect while the game runs go to the emulator
    bool options_updated = false;
    if (shared && environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE, &options_updated) &&
        options_updated) {
        Libretro::SharedOptions options{};
        ReadCoreOptions(options);
        auto& live = shared->live;
        live.show_fps.store(options.show_fps);
        live.volume.store(options.volume);
        live.trophy_popup.store(options.trophy_popup);
        live.trophy_side.store(options.trophy_side);
        live.trophy_seconds.store(options.trophy_seconds);
        shared->options_serial.fetch_add(1, std::memory_order_release);
    }

    if (shared) {
        // The emulator ended on its own: the game quit, or it crashed. Either way there is
        // nothing left to show, and the frontend is asked to close the content.
        int code = 0;
        bool crashed = false;
        if (emulator_running && EmulatorEnded(code, crashed)) {
            emulator_running = false;
            char text[160];
            if (crashed) {
                std::snprintf(text, sizeof(text),
                              "shadPS4: the emulator stopped unexpectedly (%d); see shadps4.log",
                              code);
                Notify(text);
            } else if (code != 0) {
                // The game ended itself, with an error of its own: not the emulator's crash.
                std::snprintf(text, sizeof(text),
                              "shadPS4: the game ended with an error (%d); see shadps4.log", code);
                Notify(text);
            } else {
                Log(RETRO_LOG_INFO, "The emulator ended");
            }
        }
        if (!emulator_running && !shutdown_requested) {
            shutdown_requested = true;
            environ_cb(RETRO_ENVIRONMENT_SHUTDOWN, nullptr);
        }

        SendInput();
        ApplyRumble();
        if (gpu_frames) {
#ifdef _WIN32
            Libretro::GpuFrames::Receive(-1, emulator_process);
#else
            Libretro::GpuFrames::Receive(gpu_socket, nullptr);
#endif
            if (const char* failure = Libretro::GpuFrames::Failure();
                failure && !gpu_failure_told) {
                gpu_failure_told = true;
                char text[320];
                std::snprintf(text, sizeof(text),
                              "shadPS4: no picture on the GPU (%s). Turn the core option GPU "
                              "Frame Hand-over off and start the game again.",
                              failure);
                Notify(text);
            }
        }
        TakeFrame();
        TakeAudio(sound);
    } else {
        std::memset(sound, 0, sizeof(sound));
    }

    if (video_cb && gpu_frames) {
        if (shown_width == 0 || shown_height == 0) {
            shown_width = output_width;
            shown_height = output_height;
        }
        AnnounceSize(shown_width, shown_height);
        Libretro::GpuFrames::Show(video_cb, shown_image, shown_latest != 0, shown_width,
                                  shown_height);
    } else if (video_cb) {
        if (!shown_frame) {
            // Nothing drawn yet: black, at the size declared.
            blank_frame.assign(static_cast<std::size_t>(output_width) * output_height, 0);
            shown_frame = blank_frame.data();
            shown_width = output_width;
            shown_height = output_height;
        }
        AnnounceSize(shown_width, shown_height);
        video_cb(shown_frame, shown_width, shown_height, shown_width * sizeof(uint32_t));
    }
    if (audio_batch_cb) {
        audio_batch_cb(sound, kAudioFrames);
    }
}

RETRO_API unsigned retro_get_region(void) {
    return RETRO_REGION_NTSC;
}

RETRO_API size_t retro_serialize_size(void) {
    return 0;
}

RETRO_API bool retro_serialize(void*, size_t) {
    return false;
}

RETRO_API bool retro_unserialize(const void*, size_t) {
    return false;
}

RETRO_API void retro_cheat_reset(void) {}

RETRO_API void retro_cheat_set(unsigned, bool, const char*) {}

RETRO_API void* retro_get_memory_data(unsigned) {
    return nullptr;
}

RETRO_API size_t retro_get_memory_size(unsigned) {
    return 0;
}

} // extern "C"
