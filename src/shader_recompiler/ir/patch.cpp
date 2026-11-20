// SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "common/string_util.h"
#include "shader_recompiler/ir/patch.h"

namespace Shader::IR {

std::string NameOf(Patch patch) {
    switch (patch) {
    case Patch::TessellationLodLeft:
        return "TessellationLodLeft";
    case Patch::TessellationLodTop:
        return "TessellationLodTop";
    case Patch::TessellationLodRight:
        return "TessellationLodRight";
    case Patch::TessellationLodBottom:
        return "TessellationLodBottom";
    case Patch::TessellationLodInteriorU:
        return "TessellationLodInteriorU";
    case Patch::TessellationLodInteriorV:
        return "TessellationLodInteriorV";
    default:
        const u32 index = u32(patch) - u32(Patch::Component0);
        return Common::StringFromFormat("Component%u", index);
    }
}

} // namespace Shader::IR
