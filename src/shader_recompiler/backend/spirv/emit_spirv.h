// SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <span>
#include "shader_recompiler/backend/bindings.h"
#include "shader_recompiler/backend/spirv/spv_module.h"
#include "shader_recompiler/ir/program.h"
#include "shader_recompiler/profile.h"

namespace Shader::Backend::SPIRV {

/// The shader @program in SPIR-V, written into @module (reset for it): its words, which lie in
/// the module until its next reset.
[[nodiscard]] std::span<const u32> EmitSPIRV(spv_module_t& module, const Profile& profile,
                                             const RuntimeInfo& runtime_info,
                                             const IR::Program& program, Bindings& binding);

} // namespace Shader::Backend::SPIRV
