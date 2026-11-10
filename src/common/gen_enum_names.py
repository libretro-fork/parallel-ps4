#!/usr/bin/env python3
# SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
# SPDX-License-Identifier: GPL-2.0-or-later
"""Writes enum_names.h and enum_names.c: C89 tables of the names of the
enums the core prints, with the values the compiler gives them.

    python3 src/common/gen_enum_names.py BUILD_DIR

BUILD_DIR holds compile_commands.json (configure with
CMAKE_EXPORT_COMPILE_COMMANDS=ON). Needs Python's libclang module
(pip install libclang). Add an enum to ENUMS, and its header to HEADERS,
to give it a table.
"""

import json
import os
import shlex
import sys

import clang.cindex as ci

HERE = os.path.dirname(os.path.abspath(__file__))
SRC = os.path.dirname(HERE)

# The headers that define the enums, and a source file for an enum of its own
HEADERS = [
    "core/debug_state.h",
    "core/file_sys/fs.h",
    "core/libraries/ajm/ajm.h",
    "core/libraries/audio/audioout.h",
    "core/libraries/audio3d/audio3d.h",
    "core/libraries/avplayer/avplayer.h",
    "core/libraries/avplayer/avplayer_common.h",
    "core/libraries/ime/ime_common.h",
    "core/libraries/jpeg/jpegenc.h",
    "core/libraries/kernel/equeue.h",
    "core/libraries/kernel/memory.h",
    "core/libraries/np/np_web_api/np_web_api.h",
    "core/libraries/save_data/dialog/savedatadialog_ui.h",
    "core/libraries/save_data/save_backup.h",
    "core/libraries/save_data/savedata.h",
    "core/libraries/system/commondialog.h",
    "core/libraries/system/msgdialog_ui.h",
    "core/memory.h",
    "core/platform.h",
    "shader_recompiler/frontend/instruction.h",
    "shader_recompiler/frontend/opcodes.h",
    "shader_recompiler/ir/ssa.h",
    "shader_recompiler/runtime_info.h",
    "video_core/amdgpu/pixel_format.h",
    "video_core/amdgpu/pm4_cmds.h",
    "video_core/amdgpu/regs_color.h",
    "video_core/amdgpu/regs_depth.h",
    "video_core/amdgpu/regs_primitive.h",
    "video_core/amdgpu/regs_shader.h",
    "video_core/amdgpu/regs_vertex.h",
    "video_core/amdgpu/resource.h",
    "video_core/amdgpu/tiling.h",
    "video_core/texture_cache/texture_cache.h",
]

ENUMS = [
    "AmdGpu::BlendControl::BlendFactor",
    "AmdGpu::BlendControl::BlendFunc",
    "AmdGpu::ClipSpace",
    "AmdGpu::ColorBuffer::EndianSwap",
    "AmdGpu::ColorBuffer::RoundMode",
    "AmdGpu::ColorBuffer::SwapMode",
    "AmdGpu::ColorControl::OperationMode",
    "AmdGpu::CompareFunc",
    "AmdGpu::CullMode",
    "AmdGpu::DataFormat",
    "AmdGpu::DepthBuffer::StencilFormat",
    "AmdGpu::DepthBuffer::ZFormat",
    "AmdGpu::EventIndex",
    "AmdGpu::EventType",
    "AmdGpu::FpRoundMode",
    "AmdGpu::FrontFace",
    "AmdGpu::ImageType",
    "AmdGpu::NumberFormat",
    "AmdGpu::PolygonMode",
    "AmdGpu::PrimitiveType",
    "AmdGpu::ProvokingVtxLast",
    "AmdGpu::SourceSelect",
    "AmdGpu::TessellationPartitioning",
    "AmdGpu::TessellationType",
    "AmdGpu::TileMode",
    "Core::FileSys::FileType",
    "Core::MemoryProt",
    "Platform::InterruptId",
    "Core::VMAType",
    "DebugStateType::QueueType",
    "Libraries::Ajm::AjmCodecType",
    "Libraries::Audio3d::OrbisAudio3dBlocking",
    "Libraries::Audio3d::OrbisAudio3dFormat",
    "Libraries::Audio3d::OrbisAudio3dOutputRoute",
    "Libraries::AudioOut::OrbisAudioOutParamAttr",
    "Libraries::AudioOut::OrbisAudioOutParamFormat",
    "Libraries::AudioOut::OrbisAudioOutPort",
    "Libraries::AvPlayer::AvPlayerEvents",
    "Libraries::AvPlayer::AvState",
    "Libraries::CommonDialog::Result",
    "Libraries::CommonDialog::Status",
    "Libraries::JpegEnc::OrbisJpegEncEncodeParamPixelFormat",
    "Libraries::JpegEnc::OrbisJpengEncEncodeParamColorSpace",
    "Libraries::JpegEnc::OrbisJpengEncEncodeParamEncodeMode",
    "Libraries::JpegEnc::OrbisJpengEncEncodeParamSamplingType",
    "Libraries::Kernel::MemoryTypes",
    "Libraries::Kernel::OrbisKernelEvent::Filter",
    "Libraries::MsgDialog::MsgDialogMode",
    "Libraries::Np::NpWebApi::OrbisNpWebApiHttpMethod",
    "Libraries::SaveData::Backup::WorkerStatus",
    "Libraries::SaveData::Dialog::SaveDataDialogMode",
    "Libraries::SaveData::Dialog::SystemMessageType",
    "Libraries::SaveData::OrbisSaveDataParamType",
    "OrbisImeDisableDevice",
    "OrbisImeExtOption",
    "OrbisImeInitExtKeyboardMode",
    "OrbisImeInputMethodState",
    "OrbisImeKeyboardOption",
    "OrbisImeKeycodeState",
    "OrbisImeLanguage",
    "OrbisImeOption",
    "Shader::Gcn::DppCtrl",
    "Shader::Gcn::InstCategory",
    "Shader::Gcn::Opcode",
    "Shader::IR::RegType",
    "Shader::Output",
    "VideoCore::TextureCache::MetaType",
]


def qualified(c):
    parts = []
    while c is not None and c.kind != ci.CursorKind.TRANSLATION_UNIT:
        if c.spelling:
            parts.append(c.spelling)
        c = c.semantic_parent
    return "::".join(reversed(parts))


def compile_args(build_dir, any_source):
    db = json.load(open(os.path.join(build_dir, "compile_commands.json")))
    for e in db:
        if os.path.realpath(e["file"]) == os.path.realpath(any_source):
            a = shlex.split(e["command"])[1:]
            i = a.index("-o")
            del a[i : i + 2]
            return [x for x in a if not x.startswith("-MD") and x not in ("-MF", "-c")
                    and not x.endswith(os.path.basename(any_source))], e["directory"]
    sys.exit("no compile command for " + any_source)


def collect(build_dir):
    # One TU that includes every header; savedata.cpp keeps an enum to itself
    probe = os.path.join(build_dir, "enum_names_probe.cpp")
    with open(probe, "w") as f:
        for h in HEADERS:
            f.write(f'#include "{h}"\n')
    args, _ = compile_args(build_dir, os.path.join(SRC, "core/libraries/save_data/savedata.cpp"))
    args += ["-isystem", "/usr/lib/llvm-19/lib/clang/19/include"]
    found = {}
    index = ci.Index.create()
    for tu_path in (probe, os.path.join(SRC, "core/libraries/save_data/savedata.cpp")):
        tu = index.parse(tu_path, args=args)
        for d in tu.diagnostics:
            if d.severity >= 3:
                sys.exit(f"{tu_path}: {d}")

        def visit(c):
            try:
                kind = c.kind
            except ValueError:
                kind = None
            if kind == ci.CursorKind.ENUM_DECL and c.is_definition():
                q = qualified(c)
                if q in ENUMS and q not in found:
                    found[q] = [(k.spelling, k.enum_value) for k in c.get_children()
                                if k.kind == ci.CursorKind.ENUM_CONSTANT_DECL]
            for ch in c.get_children():
                f = ch.location.file
                if f is None or str(f).startswith(SRC):
                    visit(ch)

        visit(tu.cursor)
    os.remove(probe)
    missing = [e for e in ENUMS if e not in found]
    if missing:
        sys.exit("not found: " + ", ".join(missing))
    return found


def c_name(q):
    return "enum_names_" + q.replace("::", "_")


def main():
    if len(sys.argv) != 2:
        sys.exit(__doc__)
    found = collect(sys.argv[1])

    head = [
        "/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project",
        " * SPDX-License-Identifier: GPL-2.0-or-later */",
        "",
        "/* Written by gen_enum_names.py; do not edit. */",
    ]
    h = head + [
        "/* clang-format off */",
        "",
        "#ifndef SHADPS4_COMMON_ENUM_NAMES_H",
        "#define SHADPS4_COMMON_ENUM_NAMES_H",
        "",
        "/* The names of the enums the core prints, as C89 tables sorted by value.",
        " * enum_name() looks a value up; a value with no name gives \"\". Where",
        " * two names share a value, the one declared first is kept. */",
        "",
        "#include <stddef.h>",
        "#include <stdint.h>",
        "",
        "#include <retro_common_api.h>",
        "",
        "RETRO_BEGIN_DECLS",
        "",
        "typedef struct enum_name_entry {",
        "    int64_t value;",
        "    const char* name;",
        "} enum_name_entry_t;",
        "",
        "typedef struct enum_names {",
        "    const enum_name_entry_t* entries;",
        "    size_t count;",
        "} enum_names_t;",
        "",
        "const char* enum_name(const enum_names_t* names, int64_t value);",
        "/* Nonzero when @value has a name */",
        "int enum_name_exists(const enum_names_t* names, int64_t value);",
        "/* Every value or'ed together, for flag enums */",
        "uint64_t enum_names_mask(const enum_names_t* names);",
        "",
    ]
    for q in ENUMS:
        h.append(f"extern const enum_names_t {c_name(q)};")
    h += ["", "RETRO_END_DECLS", "", "#endif", ""]

    c = head + [
        "/* clang-format off */",
        "",
        '#include "common/enum_names.h"',
        "",
        "const char* enum_name(const enum_names_t* names, int64_t value) {",
        "    size_t lo = 0;",
        "    size_t hi = names->count;",
        "    while (lo < hi) {",
        "        size_t mid = (lo + hi) / 2;",
        "        if (names->entries[mid].value < value)",
        "            lo = mid + 1;",
        "        else",
        "            hi = mid;",
        "    }",
        "    if (lo < names->count && names->entries[lo].value == value)",
        "        return names->entries[lo].name;",
        '    return "";',
        "}",
        "",
        "int enum_name_exists(const enum_names_t* names, int64_t value) {",
        "    return enum_name(names, value)[0] != '\\0';",
        "}",
        "",
        "uint64_t enum_names_mask(const enum_names_t* names) {",
        "    uint64_t mask = 0;",
        "    size_t i;",
        "    for (i = 0; i < names->count; i++)",
        "        mask |= (uint64_t)names->entries[i].value;",
        "    return mask;",
        "}",
        "",
    ]
    for q in ENUMS:
        seen = {}
        for name, value in found[q]:
            if value >= 2**63:
                value -= 2**64  # as the int64_t a caller's cast makes it
            seen.setdefault(value, name)
        entries = sorted(seen.items())
        lit = lambda v: f"(int64_t){v}" if -2**31 <= v < 2**31 else (
            f"(int64_t)((uint64_t){v >> 32}u << 32 | {v & 0xFFFFFFFF}u)" if v >= 0 else
            f"-(int64_t)((uint64_t){(-v) >> 32}u << 32 | {(-v) & 0xFFFFFFFF}u)")
        c.append(f"static const enum_name_entry_t {c_name(q)}_entries[] = {{")
        for value, name in entries:
            c.append(f'    {{{lit(value)}, "{name}"}},')
        c.append("};")
        c.append(f"const enum_names_t {c_name(q)} = {{{c_name(q)}_entries,")
        c.append(f"    sizeof({c_name(q)}_entries) / sizeof({c_name(q)}_entries[0])}};")
        c.append("")

    with open(os.path.join(HERE, "enum_names.h"), "w") as f:
        f.write("\n".join(h))
    with open(os.path.join(HERE, "enum_names.c"), "w") as f:
        f.write("\n".join(c).rstrip("\n") + "\n")
    return 0


if __name__ == "__main__":
    sys.exit(main())
