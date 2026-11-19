// SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <filesystem>
#include <optional>
#include <thread>

#include <string>
#include <rthreads/retro_eventcount.h>
#include "common/singleton.h"
#include "common/stop_thread.h"
#include "core/linker.h"
#include "frontend_window.h"
#include "input/controller.h"

namespace Core {

using HLEInitDef = void (*)(Core::Loader::SymbolsResolver* sym);

struct SysModules {
    std::string_view module_name;
    HLEInitDef callback;
};

class Emulator {
public:
    Emulator();
    ~Emulator();

    void Run(std::filesystem::path file, std::vector<std::string> args = {},
             std::optional<std::filesystem::path> game_folder = {},
             std::vector<std::pair<std::filesystem::path, std::string>> mounts = {},
             std::vector<std::string> const& env_vars = {}, bool append_log = false);
    void UpdatePlayTime(const std::string_view serial);
    void Shutdown();

    /**
     * This will kill the current process and launch a new process with the same configuration
     * (using CLI args) but replacing the eboot image and guest arguments
     */
    void Restart(std::filesystem::path eboot_path, const std::vector<std::string>& guest_args = {});

    /**
     * Launches a new emulator process with the supplied CLI arguments, then terminates this
     * process. The new process waits for this one to exit before initializing.
     */
    [[noreturn]] void Relaunch(std::vector<std::string> args);

    const char* executableName;

private:
    void LoadSystemModules(const std::string& game_serial);

    Core::MemoryManager* memory;
    Input::GameControllers* controllers;
    Core::Linker* linker;
    std::unique_ptr<Frontend::Window> window;
    // The game's time (nanoseconds) the play time was last counted up to; 0: not counting
    u64 start_time{};
    stop_thread_t play_time_thread = STOP_THREAD_INIT;
    // What the play time thread waits on between its counts: notified to stop it
    retro_eventcount_t play_time_wake;
    std::string play_time_serial;
};

} // namespace Core
