// SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "common/types.h"

namespace Core::Loader {
class SymbolsResolver;
}

namespace Libraries::LibcInternal {

// The library's functions are C (libc_internal_*.c); this registers them, in libs.cpp

void RegisterLib(Core::Loader::SymbolsResolver* sym);
} // namespace Libraries::LibcInternal