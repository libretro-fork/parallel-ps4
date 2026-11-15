// SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <functional>
#include <span>
#include <string_view>

#include "common/types.h"

namespace Common {

/// A file carried in the core (src/resources), named by its path from the top of the source
/// tree, lent in place; empty when there is none by that name.
std::span<const u8> GetEmbeddedResource(std::string_view path);

/// Each file carried whose path starts with @prefix, in the order the build embedded them.
void ForEachEmbeddedResource(std::string_view prefix,
                             const std::function<void(std::string_view, std::span<const u8>)>& fn);

} // namespace Common
