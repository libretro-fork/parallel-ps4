// SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

// The FPS counter (the Show FPS Counter core option, F10 to show or hide it), in the top right
// corner, as the developer tools drew it
namespace ImGui::FpsCounter {

void Register();
void Unregister();

} // namespace ImGui::FpsCounter
