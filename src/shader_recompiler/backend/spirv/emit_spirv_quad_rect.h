// SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "common/concepts.h"
#include "common/types.h"

namespace Shader {
struct HwFragmentRuntimeInfo;
}

namespace Shader::Backend::SPIRV {

enum class AuxShaderType : u32 {
    RectListTCS,
    QuadListTCS,
    PassthroughTES,
};

[[nodiscard]] Common::HeapArray<u32> EmitAuxilaryTessShader(AuxShaderType type,
                                                            const HwFragmentRuntimeInfo& fs_info);

} // namespace Shader::Backend::SPIRV
