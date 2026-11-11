// SPDX-FileCopyrightText: Copyright 2025-2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <algorithm>
#include <cstdio>
#include <ctime>
#include <filesystem>
#include <iostream>
#include <set>
#include <sstream>
#include <features/features_cpu.h>
#include <fmt/core.h>
#include <fmt/xchar.h>
#include <streams/file_stream.h>

#include "common/elf_info.h"
#include "common/io_file.h"
#include "common/logging/log.h"
#include "common/memory_patcher.h"
#include "common/ntapi.h"
#include "common/path_util.h"
#include "common/polyfill_thread.h"
#include "common/scm_rev.h"
#include "common/singleton.h"
#include "common/string_util.h"
#include "common/thread.h"
#include "core/cpu_patches.h" // Windows static guest red-zone protection
#include "core/debugger.h"
#include "core/devtools/widget/module_list.h"
#include "core/emulator_settings.h"
#include "core/emulator_state.h"
#include "core/file_format/psf.h"
#include "core/file_format/trp.h"
#include "core/file_sys/fs.h"
#include "core/ipc/ipc.h"
#include "core/libraries/kernel/kernel.h"
#include "core/libraries/libs.h"
#include "core/libraries/np/np_handler/np_handler.h"
#include "core/libraries/np/np_trophy.h"
#include "core/libraries/np/trophy_ui.h"
#include "core/libraries/save_data/save_backup.h"
#include "core/libraries/usbd/usbstorage.h"
#include "core/linker.h"
#include "core/memory.h"
#include "core/user_manager.h"
#include "emulator.h"
#include "libretro/libretro_bridge.h"
#include "video_core/cache_storage.h"

#ifdef _WIN32
#include <WinSock2.h>
#endif

#ifndef _WIN32
#include <sys/wait.h>
#include <unistd.h>
#endif
#include <core/file_format/npbind.h>

Frontend::Window* g_window = nullptr;

namespace Libraries::Kernel {
extern char const* g_environment[64];
}

namespace Core {

std::mutex exit_mutex{};

Emulator::Emulator() {
    // Initialize NT API functions, set high priority and disable WER
#ifdef _WIN32
    Common::NtApi::Initialize();
    SetPriorityClass(GetCurrentProcess(), ABOVE_NORMAL_PRIORITY_CLASS);
    SetErrorMode(SetErrorMode(0) | SEM_NOGPFAULTERRORBOX);
    // need to init this in order for winsock2 to work
    WORD versionWanted = MAKEWORD(2, 2);
    WSADATA wsaData;
    WSAStartup(versionWanted, &wsaData);
#endif
    std::at_quick_exit([]() { Common::Singleton<Core::Emulator>::Instance()->Shutdown(); });
}

Emulator::~Emulator() {}

void Emulator::Shutdown() {
    static bool exit_done = false;
    std::scoped_lock l{exit_mutex};
    if (exit_done) {
        return;
    }
    Common::Log::Flush();
    Libraries::SaveData::Backup::StopThread();
    Storage::DataBase::Instance().Close();
    play_time_thread.request_stop();
    if (play_time_thread.joinable()) {
        play_time_thread.join();
    }
    UpdatePlayTime(Common::Singleton<Common::ElfInfo>::Instance()->GameSerial());
    exit_done = true;
}

s32 ReadCompiledSdkVersion(const std::string& guest_or_host_path) {
    auto* mnt = Common::Singleton<Core::FileSys::MntPoints>::Instance();
    std::unique_ptr<Core::FileSys::IFile> handle;
    if (!guest_or_host_path.empty() && guest_or_host_path.front() == '/') {
        handle = mnt->Open(guest_or_host_path, /*writable=*/false);
    }

    Core::Loader::Elf elf;
    if (handle) {
        elf.Open(std::move(handle));
    } else {
        elf.Open(std::filesystem::path{guest_or_host_path});
    }

    if (!elf.IsElfFile()) {
        return 0;
    }
    const auto elf_pheader = elf.GetProgramHeader();
    auto i_procparam = std::find_if(elf_pheader.begin(), elf_pheader.end(), [](const auto& entry) {
        return entry.p_type == PT_SCE_PROCPARAM;
    });

    if (i_procparam != elf_pheader.end()) {
        Core::OrbisProcParam param{};
        elf.LoadSegment(u64(&param), i_procparam->p_offset, i_procparam->p_filesz);
        return param.sdk_version;
    }
    return 0;
}

std::map<s32, std::string> ExtractTrophies(std::string_view npbind_guest,
                                           std::string_view trophy_dir_guest) {
    std::map<s32, std::string> trophy_index_map{};

    auto* mnt = Common::Singleton<Core::FileSys::MntPoints>::Instance();

    NPBindFile npbind;
    const auto npbind_bytes = mnt->ReadFile(npbind_guest);
    if (!npbind_bytes || !npbind.Load(npbind_bytes->Bytes())) {
        LOG_WARNING(Common_Filesystem, "Failed to load npbind.dat file");
        return trophy_index_map;
    }

    auto np_comm_ids = npbind.GetNpCommIds();
    if (np_comm_ids.empty()) {
        LOG_WARNING(Common_Filesystem, "No NPCommIDs in npbind.dat");
        return trophy_index_map;
    }
    auto& game_info = Common::ElfInfo::Instance();
    game_info.SetNpCommIds(np_comm_ids);

    if (!mnt->IsDirectory(trophy_dir_guest)) {
        LOG_WARNING(Common_Filesystem, "Game does not contain a trophy directory");
        return trophy_index_map;
    }

    auto dir = mnt->OpenDir(trophy_dir_guest);
    if (!dir) {
        return trophy_index_map;
    }

    const std::string pattern = "trophy";
    Core::FileSys::DirEntry entry;
    while (dir->Next(entry)) {
        if (entry.is_directory) {
            continue;
        }
        // Extension check: match TROPHY00.TRP as well as trophy00.trp.
        const std::string name_lower = Common::ToLower(entry.name);
        if (!name_lower.ends_with(".trp")) {
            continue;
        }
        const std::string stem = name_lower.substr(0, name_lower.size() - 4);
        if (!stem.starts_with(pattern)) {
            continue;
        }

        // Extract the number part
        const std::string num_str = stem.substr(pattern.length());
        s32 trophy_index;
        try {
            trophy_index = std::stoi(num_str);
        } catch (...) {
            continue;
        }

        if (static_cast<s32>(np_comm_ids.size()) <= trophy_index) {
            LOG_WARNING(Common_Filesystem, "Trophy index {} does not have a corresponding NPCommId",
                        trophy_index);
            continue;
        }

        const std::string np_comm_id = np_comm_ids[trophy_index];
        trophy_index_map[trophy_index] = np_comm_id;
        LOG_DEBUG(Loader, "Mapped trophy index {} to NPCommID: {}", trophy_index, np_comm_id);

        // Extract the actual trophies if they're not extracted yet.
        const auto& trophy_output_dir =
            Common::FS::GetUserPath(Common::FS::PathType::TrophyDir) / np_comm_id;
        if (!Common::FS::Exists(trophy_output_dir)) {
            const std::string entry_guest = std::string(trophy_dir_guest) + "/" + entry.name;
            // Mapped where it lies, or decoded out of an archive
            const auto trp_bytes = mnt->ReadFile(entry_guest);
            if (!trp_bytes) {
                LOG_ERROR(Loader, "Couldn't read trophy file {}", entry.name);
                continue;
            }
            const auto trp_source = mnt->GetHostPath(entry_guest);
            TRP trp;
            bool ok = trp.Extract(trp_bytes->Bytes(), np_comm_id, trophy_output_dir);
            if (!ok) {
                // if it's an update and doesn't contain trophies fallback to base
                const auto base_source = mnt->GetHostPath(
                    entry_guest, nullptr, Core::FileSys::MntPoints::HostPathType::Base);
                if (!base_source.empty() && base_source != trp_source &&
                    Common::FS::IsFile(base_source)) {
                    LOG_WARNING(Loader, "Retrying trophy extraction with base game file {}",
                                base_source.string());
                    ok = trp.Extract(base_source, np_comm_id, trophy_output_dir);
                }
            }
            if (!ok) {
                LOG_ERROR(Loader, "Couldn't extract trophy file {}", entry.name);
                continue;
            }
        }

        // Move extracted trophy contents into each user's folder
        for (size_t user_index = 0; user_index < user_manager_count(); ++user_index) {
            const struct user& user = *user_manager_at(user_index);
            if (!user_manager_has_home_dir(&user)) {
                continue;
            }
            auto const user_trophy_file = Common::FS::PathFromCString(emu_settings_get_home_dir) /
                                          std::to_string(user.user_id) / "trophy" /
                                          (np_comm_id + ".xml");
            if (!Common::FS::Exists(user_trophy_file)) {
                auto temp = user_trophy_file.parent_path();
                Common::FS::CreateDirectories(temp);
                const auto tropconf = trophy_output_dir / "Xml" / "TROPCONF.XML";
                if (!Common::FS::CopyFile(tropconf, user_trophy_file)) {
                    LOG_ERROR(Loader, "Failed to copy {} to {}", tropconf.string(),
                              user_trophy_file.string());
                }
            }
        }
    }

    if (trophy_index_map.empty()) {
        LOG_WARNING(Common_Filesystem, "No usable trophy files found in {}", trophy_dir_guest);
    }

    return trophy_index_map;
}

void Emulator::Run(std::filesystem::path file, std::vector<std::string> args,
                   std::optional<std::filesystem::path> p_game_folder,
                   std::vector<std::pair<std::filesystem::path, std::string>> mounts,
                   std::vector<std::string> const& env_vars, bool append_log) {
    Common::SetCurrentThreadName("shadPS4:Main");
    if (waitForDebuggerBeforeRun) {
        Debugger::WaitForDebuggerAttach();
    }

    if (Common::FS::IsDirectory(file)) {
        file /= "eboot.bin";
    }

    std::filesystem::path game_folder;

    // Archive detection.
    std::filesystem::path archive_path;
    std::filesystem::path archive_inner;
    {
        std::filesystem::path accum;
        bool found = false;
        for (const auto& comp : file) {
            if (!found) {
                accum /= comp;
                // The libretro core runs a game from its PKG as from a .zar.
                if (comp.extension() == ".zar" || comp.extension() == ".pkg") {
                    found = true;
                    archive_path = accum;
                }
            } else {
                archive_inner /= comp;
            }
        }
        // Only treat it as an archive if the .zar element is a real file.
        if (found && !Common::FS::IsFile(archive_path)) {
            found = false;
        }
        if (found && archive_inner.empty()) {
            archive_inner = "eboot.bin";
        }
    }
    const bool from_archive = !archive_path.empty();

    const auto rebase_to_base_game = [](std::filesystem::path& folder) {
        if (const auto base = FileSys::BaseGameFromOverlay(folder)) {
            if (const auto resolved = FileSys::ResolveGameRoot(*base)) {
                LOG_INFO(Loader, "Launched from overlay {}, using base game {} as /app0",
                         folder.string(), resolved->string());
                folder = *resolved;
            } else {
                LOG_WARNING(Loader, "Launched from overlay {} but no base game was found",
                            folder.string());
            }
        }
    };

    std::filesystem::path eboot_name;

    if (from_archive) {
        game_folder = archive_path;
        file = archive_path / archive_inner;
        eboot_name = archive_inner;
        rebase_to_base_game(game_folder);
    }

    const auto resolve_relative_path = [](const std::filesystem::path& path,
                                          const std::filesystem::path& base) {
        // WinFSP-backed mounts can reject canonical path queries while normal reads still work.
        std::error_code relative_error;
        auto relative_path = std::filesystem::relative(path, base, relative_error);
        if (relative_error) {
            LOG_WARNING(Common_Filesystem,
                        "Failed to canonicalize executable path {} relative to {}: {}. Falling "
                        "back to lexical path resolution.",
                        Common::FS::PathToUTF8String(path), Common::FS::PathToUTF8String(base),
                        relative_error.message());

            relative_path = path.lexically_relative(base);
        }
        return relative_path;
    };

    if (!from_archive) {
        if (p_game_folder.has_value()) {
            game_folder = p_game_folder.value();
            eboot_name = resolve_relative_path(file, game_folder);
        } else {
            game_folder = file.parent_path();
            eboot_name = resolve_relative_path(file, game_folder);
            rebase_to_base_game(game_folder);
        }
    }

    if (eboot_name.empty()) {
        LOG_ERROR(Common_Filesystem, "Failed to derive executable path {} relative to {}",
                  Common::FS::PathToUTF8String(file), Common::FS::PathToUTF8String(game_folder));
        return;
    }

    // Applications expect to be run from /app0 so mount the file's parent path as app0.
    auto* mnt = Common::Singleton<Core::FileSys::MntPoints>::Instance();
    mnt->Mount(game_folder, "/app0", true);
    // Certain games may use /hostapp as well such as CUSA001100
    mnt->Mount(game_folder, "/hostapp", true);

    // Load param.sfo details if it exists
    std::string id;
    std::string title;
    std::string app_version;
    u32 sdk_version;
    u32 fw_version;
    bool param_sfo_exists = false;
    Common::PSFAttributes psf_attributes{};

    if (auto psf_handle = mnt->Open("/app0/sce_sys/param.sfo", /*writable=*/false)) {
        auto* param_sfo = Common::Singleton<PSF>::Instance();
        ASSERT_MSG(param_sfo->Open(psf_handle), "Failed to open param.sfo");
        param_sfo_exists = true;

        const auto content_id = param_sfo->GetString("CONTENT_ID");
        const auto title_id = param_sfo->GetString("TITLE_ID");
        if (content_id.has_value() && !content_id->empty()) {
            id = std::string(*content_id, 7, 9);
        } else if (title_id.has_value()) {
            id = *title_id;
        }
        title = param_sfo->GetString("TITLE").value_or("Unknown title");
        fw_version = param_sfo->GetInteger("SYSTEM_VER").value_or(0x4700000);
        app_version = param_sfo->GetString("APP_VER").value_or("Unknown version");
        if (const auto raw_attributes = param_sfo->GetInteger("ATTRIBUTE")) {
            psf_attributes.raw = *raw_attributes;
        }

        // Extract sdk version from pubtool info.
        std::string_view pubtool_info =
            param_sfo->GetString("PUBTOOLINFO").value_or("Unknown value");
        u64 sdk_ver_offset = pubtool_info.find("sdk_ver");

        if (sdk_ver_offset == pubtool_info.npos) {
            // Default to using firmware version if SDK version is not found.
            sdk_version = fw_version;
        } else {
            // Increment offset to account for sdk_ver= part of string.
            sdk_ver_offset += 8;
            u64 sdk_ver_len = pubtool_info.find(",", sdk_ver_offset);
            if (sdk_ver_len == pubtool_info.npos) {
                // If there's no more commas, this is likely the last entry of pubtool info.
                // Use string length instead.
                sdk_ver_len = pubtool_info.size();
            }
            sdk_ver_len -= sdk_ver_offset;
            std::string sdk_ver_string = pubtool_info.substr(sdk_ver_offset, sdk_ver_len).data();
            // Number is stored in base 16.
            sdk_version = std::stoi(sdk_ver_string, nullptr, 16);
        }
    }

    if (id.empty()) {
        emu_settings_load();
    } else if (emu_settings_load_game(id.c_str())) {
        EmulatorState::GetInstance()->SetGameSpecifigConfigUsed(true);
    }
    // The frontend's core options, on top of what the config files say.
    Libretro::ApplyCoreOptions();
    // Before the game runs: a trophy unlocked in play decodes nothing.
    Libraries::Np::NpTrophy::PreloadTrophySound();
    // Switch to configured log
    Common::Log::Switch((!id.empty() && emu_settings_get_log_separate()) ? id + ".log"
                                                                         : "shad_log.txt",
                        append_log);

    auto guest_eboot_path = "/app0/" + eboot_name.generic_string();

    auto& game_info = Common::ElfInfo::Instance();
    game_info.initialized = true;
    game_info.game_serial = id;
    game_info.title = title;
    game_info.app_ver = app_version;
    game_info.firmware_ver = fw_version & 0xFFF00000;
    game_info.raw_firmware_ver = fw_version;
    game_info.sdk_ver = ReadCompiledSdkVersion(guest_eboot_path);
    game_info.psf_attributes = psf_attributes;

    if (auto splash = mnt->ReadFile("/app0/sce_sys/pic1.png")) {
        game_info.splash_data = std::move(*splash);
    } else {
        LOG_INFO(Loader, "No splash image found at /app0/sce_sys/pic1.png");
    }

    game_info.game_folder = game_folder;

    ASSERT_MSG(mnt->Exists(guest_eboot_path), "Guest app's main executable {} does not exist",
               guest_eboot_path);

    LOG_INFO(Loader, "Starting shadps4 emulator v{} ", Common::g_version);
    LOG_INFO(Loader, "Revision {}", Common::g_scm_rev);
    LOG_INFO(Loader, "Branch {}", Common::g_scm_branch);
    LOG_INFO(Loader, "Description {}", Common::g_scm_desc);
    LOG_INFO(Loader, "Remote {}", Common::g_scm_remote_url);

    LOG_INFO(Config, "Game-specific config used: {}",
             EmulatorState::GetInstance()->IsGameSpecifigConfigUsed());

    LOG_INFO(Config, "General isNeo: {}", emu_settings_get_neo_mode());
    LOG_INFO(Config, "General isDevKit: {}", emu_settings_get_dev_kit_mode());
    LOG_INFO(Config, "General isConnectedToNetwork: {}", emu_settings_get_connected_to_network());
    LOG_INFO(Config, "General isShadNetEnabled: {}", emu_settings_is_shadnet_enabled());
#ifdef _WIN32
    LOG_INFO(Config, "General isRedZonePatchingEnabled: {}", emu_settings_get_redzone_patches());
#endif
    LOG_INFO(Config, "General p2pPort: {}", emu_settings_get_p2p_port());
    LOG_INFO(Config, "Log sync: {}", emu_settings_get_log_sync());
    LOG_INFO(Config, "Log skipDuplicate: {}", emu_settings_get_log_skip_duplicate());
#ifdef _WIN32
    LOG_INFO(Config, "Log type: {}", emu_settings_get_log_type());
#endif
    LOG_INFO(Config, "GPU isNullGpu: {}", emu_settings_get_null_gpu());
    LOG_INFO(Config, "GPU readbacksMode: {}", emu_settings_get_readbacks_mode());
    LOG_INFO(Config, "GPU readbackLinearImages: {}",
             emu_settings_get_readback_linear_images_enabled());
    LOG_INFO(Config, "GPU directMemoryAccess: {}", emu_settings_get_direct_memory_access_enabled());
    LOG_INFO(Config, "GPU shouldDumpShaders: {}", emu_settings_get_dump_shaders());
    LOG_INFO(Config, "GPU vblankFrequency: {}", emu_settings_get_vblank_frequency());
    LOG_INFO(Config, "GPU shouldCopyGPUBuffers: {}", emu_settings_get_copy_gpu_buffers());
#ifdef __linux__
    LOG_INFO(Config, "GPU userfaultfdTracking: {}", emu_settings_get_userfaultfd());
#endif
    LOG_INFO(Config, "GPU inlineFetchShader: {}", emu_settings_get_inline_fetch_shader());
    LOG_INFO(Config, "GPU fastShaderMath: {}", emu_settings_get_fast_shader_math());
    LOG_INFO(Config, "Vulkan gpuId: {}", emu_settings_get_gpu_id());
    LOG_INFO(Config, "Vulkan vkValidation: {}", emu_settings_get_vkvalidation_enabled());
    LOG_INFO(Config, "Vulkan vkValidationCore: {}", emu_settings_get_vkvalidation_core_enabled());
    LOG_INFO(Config, "Vulkan vkValidationSync: {}", emu_settings_get_vkvalidation_sync_enabled());
    LOG_INFO(Config, "Vulkan vkValidationGpu: {}", emu_settings_get_vkvalidation_gpu_enabled());
    LOG_INFO(Config, "Vulkan crashDiagnostics: {}", emu_settings_get_vkcrash_diagnostic_enabled());
    LOG_INFO(Config, "Vulkan hostMarkers: {}", emu_settings_get_vkhost_markers());
    LOG_INFO(Config, "Vulkan guestMarkers: {}", emu_settings_get_vkguest_markers());
    LOG_INFO(Config, "Vulkan PipelineCacheEnabled: {}", emu_settings_get_pipeline_cache_enabled());

    char cpu_model[128] = {};
    cpu_features_get_model_name(cpu_model, sizeof(cpu_model));
    LOG_INFO(Config, "CPU Model: {}", cpu_model);
    LOG_INFO(Config, "CPU Physical Cores: {}, Logical Cores: {}",
             cpu_features_get_core_amount_physical(), cpu_features_get_core_amount());

    if (param_sfo_exists) {
        LOG_INFO(Loader, "Game id: {} Title: {}", id, title);
        LOG_INFO(Loader, "Fw: {:#x} App Version: {}", fw_version, app_version);
        LOG_INFO(Loader, "param.sfo SDK version: {:#x}", sdk_version);
        LOG_INFO(Loader, "eboot SDK version: {:#x}", game_info.sdk_ver);
        LOG_INFO(Loader, "PSVR Supported: {}", (bool)psf_attributes.support_ps_vr.Value());
        LOG_INFO(Loader, "PSVR Required: {}", (bool)psf_attributes.require_ps_vr.Value());
    }
    if (!args.empty()) {
        const auto argc = std::min<size_t>(args.size(), 32);
        for (auto i = 0; i < argc; i++) {
            LOG_INFO(Loader, "Game argument {}: {}", i, args[i]);
        }
        if (args.size() > 32) {
            LOG_ERROR(Loader, "Too many game arguments, only passing the first 32");
        }
    } else {
        args.insert(args.begin(), guest_eboot_path);
    }

    const auto mods_folder = FileSys::OverlayPath(game_folder, "-mods");

    if (Common::FS::Exists(mods_folder) && !Common::FS::IsEmptyDirectory(mods_folder)) {
        LOG_INFO(Loader, "Files found in game mods folder");
    }

#ifdef _WIN32
    // Enable red-zone patching if the setting is enabled
    if (emu_settings_get_redzone_patches()) {
        WindowsGuestRedZoneProtection::SetActiveMode(
            WindowsGuestRedZoneProtectionMode::StaticPatching);
    }
#endif

    // Create stdin/stdout/stderr
    Common::Singleton<FileSys::HandleTable>::Instance()->CreateStdHandles();

    // Initialize components
    memory = Core::Memory::Instance();
    controllers = Common::Singleton<Input::GameControllers>::Instance();
    linker = Common::Singleton<Core::Linker>::Instance();

    // Initialize patcher
    if (!id.empty()) {
        MemoryPatcher::g_game_serial = id;
    }

    // Extract and load trophies.
    game_info.trophy_index_map =
        ExtractTrophies("/app0/sce_sys/npbind.dat", "/app0/sce_sys/trophy");

    window = std::make_unique<Frontend::Window>(emu_settings_get_window_width(),
                                                emu_settings_get_window_height());

    g_window = window.get();

    const auto& mount_data_dir = Common::FS::GetUserPath(Common::FS::PathType::GameDataDir);
    mnt->Mount(mount_data_dir, "/data");

    // Mounting temp folders
    const auto& mount_temp_dir = Common::FS::GetUserPath(Common::FS::PathType::TempDataDir) / id;
    Common::FS::CreateDirectories(mount_temp_dir);
    mnt->Mount(mount_temp_dir, "/temp");

    const auto& mount_download_dir =
        Common::FS::GetUserPath(Common::FS::PathType::DownloadDir) / id;
    if (!Common::FS::Exists(mount_download_dir)) {
        Common::FS::CreateDirectories(mount_download_dir);
    }
    mnt->Mount(mount_download_dir, "/download0");

    const auto mount_usb_dir = Libraries::UsbStorage::GetUsbMountPath();
    std::error_code ec_usb;
    Common::FS::CreateDirectories(mount_usb_dir);
    mnt->Mount(mount_usb_dir, "/mnt/usb0");

    // Mount system fonts
    const auto& fonts_dir = Common::FS::PathFromCString(emu_settings_get_font_dir);
    if (!Common::FS::Exists(fonts_dir)) {
        Common::FS::CreateDirectories(fonts_dir);
    }

    // Fonts are mounted into the sandboxed system directory, construct the appropriate path.
    const char* sandbox_root = Libraries::Kernel::sceKernelGetFsSandboxRandomWord();
    std::string guest_font_dir = "/";
    guest_font_dir.append(sandbox_root).append("/common/font");
    const auto& host_font_dir = fonts_dir / "font";
    if (!Common::FS::Exists(host_font_dir)) {
        Common::FS::CreateDirectories(host_font_dir);
    }
    mnt->Mount(host_font_dir, guest_font_dir);

    // There is a second font directory, mount that too.
    guest_font_dir.append("2");
    const auto& host_font2_dir = fonts_dir / "font2";
    if (!Common::FS::Exists(host_font2_dir)) {
        Common::FS::CreateDirectories(host_font2_dir);
    }
    mnt->Mount(host_font2_dir, guest_font_dir);

    for (auto const& mount_pair : mounts) {
        LOG_INFO(Loader, "Mounting {} to {}", mount_pair.first.string(), mount_pair.second);
        mnt->Mount(mount_pair.first, mount_pair.second);
    }

    if (Common::FS::IsEmptyDirectory(host_font_dir) ||
        Common::FS::IsEmptyDirectory(host_font2_dir)) {
        LOG_WARNING(Loader, "No dumped system fonts, expect missing text or instability");
    }

    auto env_max = std::min<u64>(env_vars.size(), 63);
    for (int i = 0; i < env_max; i++) {
        LOG_INFO(Loader, "Env {:02}: {}", i, env_vars[i]);
        Libraries::Kernel::g_environment[i] = env_vars[i].c_str();
    }
    Libraries::Kernel::g_environment[env_max] = nullptr;

    // Initialize kernel and library facilities.
    Libraries::InitHLELibs(&linker->GetHLESymbols());

    // Load the module with the linker.
    if (linker->LoadModule(guest_eboot_path) == -1) {
        LOG_CRITICAL(Loader, "Failed to load game's eboot.bin: {}", guest_eboot_path);
        std::quick_exit(0);
    }

    if (!id.empty()) {
        start_time = std::chrono::steady_clock::now();

        play_time_thread = std::jthread([this, id](std::stop_token stop) {
            while (Common::StoppableTimedWait(stop, std::chrono::seconds(60))) {
                UpdatePlayTime(id);
            }
        });
    }

    linker->Execute(args);

    Frontend::WaitForQuit();
    std::quick_exit(0);
}

void Emulator::Restart(std::filesystem::path eboot_path,
                       const std::vector<std::string>& guest_args) {
    std::vector<std::string> args;

    auto& game_info = Common::ElfInfo::Instance();
    const auto& game_folder = game_info.GetGameFolder();
    const bool from_archive =
        Common::FS::IsFile(game_folder) &&
        (game_folder.extension() == ".zar" || game_folder.extension() == ".pkg");

    args.push_back("--log-append");

    if (from_archive) {
        // Archive-backed base game: relaunch by pointing --game at the
        // .zar itself. Run() re-detects the extension and re-mounts it.
        std::filesystem::path relaunch = game_folder;
        std::string guest = Common::FS::PathToUTF8String(eboot_path);
        for (const std::string_view prefix : {"/app0/", "/hostapp/"}) {
            if (guest.starts_with(prefix)) {
                guest.erase(0, prefix.size());
                break;
            }
        }
        if (!guest.empty() && guest != "eboot.bin") {
            relaunch /= guest;
        }

        args.push_back("--game");
        args.push_back(Common::FS::PathToUTF8String(relaunch));
    } else {
        auto mnt = Common::Singleton<Core::FileSys::MntPoints>::Instance();
        auto game_path = mnt->GetHostPath("/app0");

        args.push_back("--game");
        args.push_back(Common::FS::PathToUTF8String(eboot_path));

        args.push_back("--override-root");
        args.push_back(Common::FS::PathToUTF8String(game_path));
    }

    if (FileSys::MntPoints::ignore_game_patches) {
        args.push_back("--ignore-game-patch");
    }

    if (!MemoryPatcher::patch_file.empty()) {
        args.push_back("--patch");
        args.push_back(MemoryPatcher::patch_file);
    }

    if (waitForDebuggerBeforeRun) {
        args.push_back("--wait-for-debugger");
    }

    if (guest_args.size() > 0) {
        args.push_back("--");
        for (const auto& arg : guest_args) {
            args.push_back(arg);
        }
    }

    Relaunch(std::move(args));
}

[[noreturn]] void Emulator::Relaunch(std::vector<std::string> args) {
    const auto guest_args = std::find(args.begin(), args.end(), "--");
    args.insert(guest_args, {"--wait-for-pid", std::to_string(Debugger::GetCurrentPid())});

    LOG_INFO(Common, "Relaunching the emulator with args: {}", fmt::join(args, " "));
    Libraries::Np::NpHandler::GetInstance().Shutdown();
    Common::Log::Shutdown();

    auto& ipc = IPC::Instance();

    if (ipc.IsEnabled()) {
        ipc.SendRestart(args);
        while (true) {
            std::this_thread::sleep_for(std::chrono::minutes(1));
        }
    }
#if defined(_WIN32)
    std::wstring cmdline;
    // Emulator executable
    const auto executable = Common::UTF8ToUTF16W(executableName);
    cmdline += L"\"";
    cmdline += executable;
    cmdline += L"\"";
    for (const auto& arg : args) {
        cmdline += L" \"";
        cmdline += Common::UTF8ToUTF16W(arg);
        cmdline += L"\"";
    }
    cmdline += L'\0';

    STARTUPINFOW si{};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi{};
    bool success = CreateProcessW(executable.c_str(), cmdline.data(), nullptr, nullptr, TRUE, 0,
                                  nullptr, nullptr, &si, &pi);

    if (!success) {
        std::cerr << "Failed to restart game: {}" << GetLastError() << std::endl;
        std::quick_exit(1);
    }

    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
#else
    std::vector<char*> argv;

    // Emulator executable
    argv.push_back(const_cast<char*>(executableName));

    for (const auto& arg : args) {
        argv.push_back(const_cast<char*>(arg.c_str()));
    }
    argv.push_back(nullptr);

    pid_t pid = fork();
    if (pid == 0) {
        // Child process - execute the new instance
        execvp(executableName, argv.data());
        std::cerr << "Failed to restart game: execvp failed" << std::endl;
        std::quick_exit(1);
    } else if (pid < 0) {
        std::cerr << "Failed to restart game: fork failed" << std::endl;
        std::quick_exit(1);
    }
#endif

    std::quick_exit(0);
}

void Emulator::UpdatePlayTime(const std::string_view serial) {
    if (serial.empty() || start_time == std::chrono::steady_clock::time_point{}) {
        return;
    }

    const auto user_dir = Common::FS::GetUserPath(Common::FS::PathType::UserDir);
    const auto file_path = user_dir / "play_time.txt";

    const auto end_time = std::chrono::steady_clock::now();
    const auto duration = std::chrono::duration_cast<std::chrono::seconds>(end_time - start_time);
    start_time = end_time;
    const int total_seconds = static_cast<int>(duration.count());

    // One line a game, "serial h:mm:ss last-played", read where it lies (mapped)
    auto file = Common::FS::FileView::Map(file_path);
    const std::string_view text =
        file ? std::string_view{reinterpret_cast<const char*>(file->data()), file->size()}
             : std::string_view{};
    const auto first_word = [](std::string_view line) {
        return line.substr(0, line.find_first_of(" \t\r"));
    };

    int accumulated_seconds = 0;
    for (size_t pos = 0; pos < text.size();) {
        const size_t end = std::min(text.find('\n', pos), text.size());
        const std::string_view line = text.substr(pos, end - pos);
        pos = end + 1;
        if (first_word(line) != serial) {
            continue;
        }
        const std::string fields{line.substr(serial.size())};
        int h, m, s_;
        if (std::sscanf(fields.c_str(), " %d:%d:%d", &h, &m, &s_) == 3) {
            accumulated_seconds = h * 3600 + m * 60 + s_;
        }
        break;
    }

    accumulated_seconds += total_seconds;
    int hours = accumulated_seconds / 3600;
    int minutes = (accumulated_seconds % 3600) / 60;
    int seconds = accumulated_seconds % 60;

    std::string playTimeSaved = fmt::format("{:d}:{:02d}:{:02d}", hours, minutes, seconds);

    const std::time_t last_time_played = std::time(nullptr);

    // The other games' lines as they were, this one's replaced or added
    std::string out;
    out.reserve(text.size() + 64);
    bool lineUpdated = false;
    for (size_t pos = 0; pos < text.size();) {
        const size_t end = std::min(text.find('\n', pos), text.size());
        const std::string_view line = text.substr(pos, end - pos);
        pos = end + 1;
        if (first_word(line) == serial) {
            out += fmt::format("{} {} {}\n", serial, playTimeSaved, last_time_played);
            lineUpdated = true;
        } else {
            out += line;
            out += '\n';
        }
    }
    if (!lineUpdated) {
        out += fmt::format("{} {} {}\n", serial, playTimeSaved, last_time_played);
    }

    // Written whole, replacing the old file only once it is complete; the mapping goes first, as
    // Windows will not replace a mapped file
    file.reset();
    if (!filestream_write_file_atomic(Common::FS::PathToUTF8String(file_path).c_str(), out.data(),
                                      static_cast<s64>(out.size()))) {
        LOG_INFO(Loader, "Error writing play_time.txt");
        return;
    }

    LOG_INFO(Loader, "Playing time for {}: {} {}", serial, playTimeSaved, last_time_played);
}

} // namespace Core
