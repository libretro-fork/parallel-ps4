// SPDX-FileCopyrightText: Copyright 2024-2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "common/concepts.h"
#include "common/types.h"
#include "shader_recompiler/runtime_info.h"

namespace Shader::Backend::SPIRV {

[[nodiscard]] Common::HeapArray<u32> EmitDiscardFragmentShader(const Shader::OutputMap* outputs);

} // namespace Shader::Backend::SPIRV
