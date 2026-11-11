// SPDX-FileCopyrightText: Copyright 2025-2026 shadLauncher4 Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "emulator_state.h"

std::shared_ptr<EmulatorState> EmulatorState::s_instance = nullptr;
Common::Combiner EmulatorState::s_mutex;

EmulatorState::EmulatorState() {}

EmulatorState::~EmulatorState() {}

std::shared_ptr<EmulatorState> EmulatorState::GetInstance() {
    return s_mutex.Run([&]() -> std::shared_ptr<EmulatorState> {
        if (!s_instance)
            s_instance = std::make_shared<EmulatorState>();
        return s_instance;
    });
}

void EmulatorState::SetInstance(std::shared_ptr<EmulatorState> instance) {
    s_mutex.Run([&] { s_instance = instance; });
}

bool EmulatorState::IsGameRunning() const {
    return m_running;
}
void EmulatorState::SetGameRunning(bool running) {
    m_running = running;
}

bool EmulatorState::IsAutoPatchesLoadEnabled() const {
    return m_load_patches_auto;
}
void EmulatorState::SetAutoPatchesLoadEnabled(bool enable) {
    m_load_patches_auto = enable;
}

bool EmulatorState::IsGameSpecifigConfigUsed() const {
    return m_game_specific_config_used;
}

void EmulatorState::SetGameSpecifigConfigUsed(bool used) {
    m_game_specific_config_used = used;
}
