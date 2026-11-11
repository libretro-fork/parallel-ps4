// SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "common/types.h"

namespace Core::Loader {
class SymbolsResolver;
}

/// libSceUlt, the PS4's user-level thread library, for games whose system folder has no
/// libSceUlt.sprx (ult.cpp).
namespace Libraries::Ult {
void RegisterLib(Core::Loader::SymbolsResolver* sym);
} // namespace Libraries::Ult
