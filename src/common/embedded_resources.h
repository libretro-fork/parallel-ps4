// SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <stddef.h>

#include "common/types.h"

namespace Common {

/// The resource the core carries at @path, and its size in *@size; null (and 0) when there is
/// none
const u8* GetEmbeddedResource(const char* path, size_t* size);

/// @fn(@ctx, its path, its bytes, their size) for each resource whose path starts with @prefix
void ForEachEmbeddedResource(const char* prefix,
                             void (*fn)(void* ctx, const char* path, const u8* data, size_t size),
                             void* ctx);

} // namespace Common
