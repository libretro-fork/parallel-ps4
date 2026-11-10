#!/usr/bin/env python3
# SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
# SPDX-License-Identifier: GPL-2.0-or-later
"""Writes shadnet_msg.h and shadnet_msg.c from shadnet.proto.

Each message becomes a plain C89 struct and a field table that pb_wire.c
encodes and decodes with. Run it after changing shadnet.proto:

    python3 src/shadnet/gen_messages.py

Only what shadnet.proto uses is understood: proto3 messages without
nesting, scalar, string, bytes, enum and message fields, repeated or not.
"""

import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))

SCALARS = {
    # proto type: (C type, wire type constant)
    "uint32": ("uint32_t", "PB_UINT32"),
    "int32": ("int32_t", "PB_INT32"),
    "uint64": ("uint64_t", "PB_UINT64"),
    "int64": ("int64_t", "PB_INT64"),
    "bool": ("uint8_t", "PB_BOOL"),
    "string": ("pb_bytes_t", "PB_STRING"),
    "bytes": ("pb_bytes_t", "PB_BYTES"),
}


def parse(text):
    text = re.sub(r"//[^\n]*", "", text)
    enums, messages = {}, []
    for m in re.finditer(r"\benum\s+(\w+)\s*\{([^}]*)\}", text):
        values = re.findall(r"(\w+)\s*=\s*(-?\d+)\s*;", m.group(2))
        enums[m.group(1)] = values
    for m in re.finditer(r"\bmessage\s+(\w+)\s*\{([^}]*)\}", text):
        fields = []
        for f in re.finditer(r"(repeated\s+)?(\w+)\s+(\w+)\s*=\s*(\d+)\s*;", m.group(2)):
            fields.append((bool(f.group(1)), f.group(2), f.group(3), int(f.group(4))))
        messages.append((m.group(1), fields))
    return enums, messages


def order(messages):
    """Messages after the ones they contain by value"""
    by_name = dict(messages)
    done, out = set(), []

    def visit(name):
        if name in done:
            return
        done.add(name)
        for repeated, ftype, _, _ in by_name[name]:
            if ftype in by_name and not repeated:
                visit(ftype)
        out.append((name, by_name[name]))

    for name, _ in messages:
        visit(name)
    return out


def main():
    with open(os.path.join(HERE, "shadnet.proto")) as f:
        enums, messages = parse(f.read())
    names = {n for n, _ in messages}

    h = []
    h.append("/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project")
    h.append(" * SPDX-License-Identifier: GPL-2.0-or-later */")
    h.append("")
    h.append("/* Written by gen_messages.py from shadnet.proto; do not edit. */")
    h.append("/* clang-format off */")
    h.append("")
    h.append("#ifndef SHADNET_MSG_H")
    h.append("#define SHADNET_MSG_H")
    h.append("")
    h.append('#include "shadnet/pb_wire.h"')
    h.append("")
    h.append("RETRO_BEGIN_DECLS")
    h.append("")
    for ename, values in enums.items():
        for vname, v in values:
            h.append(f"#define SHADNET_{vname} {v}")
        h.append("")
    for name, _ in messages:
        h.append(f"struct shadnet_{name};")
    h.append("")
    for name, fields in order(messages):
        h.append(f"typedef struct shadnet_{name} {{")
        if not fields:
            h.append("    uint8_t unused_; /* no fields */")
        for repeated, ftype, fname, _ in fields:
            if ftype in names:
                ctype = f"struct shadnet_{ftype}"
            elif ftype in enums:
                ctype = "int32_t"
            else:
                ctype = SCALARS[ftype][0]
            if repeated:
                h.append(f"    struct {{")
                h.append(f"        {ctype}* items;")
                h.append(f"        size_t count;")
                h.append(f"    }} {fname};")
            else:
                h.append(f"    {ctype} {fname};")
                if ftype in names:
                    h.append(f"    uint8_t has_{fname};")
        h.append(f"}} shadnet_{name};")
        h.append(f"extern const pb_msg_desc_t shadnet_{name}_desc;")
        h.append("")
    h.append("/* Every message, for X-macro use: X(LoginRequest) ... */")
    h.append("#define SHADNET_MESSAGES(X) \\")
    for i, (name, _) in enumerate(messages):
        h.append(f"    X({name})" + (" \\" if i + 1 < len(messages) else ""))
    h.append("")
    h.append("RETRO_END_DECLS")
    h.append("")
    h.append("#endif")

    c = []
    c.append("/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project")
    c.append(" * SPDX-License-Identifier: GPL-2.0-or-later */")
    c.append("")
    c.append("/* Written by gen_messages.py from shadnet.proto; do not edit. */")
    c.append("/* clang-format off */")
    c.append("")
    c.append("#include <stddef.h>")
    c.append("")
    c.append('#include "shadnet/shadnet_msg.h"')
    c.append("")
    for name, fields in order(messages):
        s = f"shadnet_{name}"
        if fields:
            c.append(f"static const pb_field_t {s}_fields[] = {{")
            for repeated, ftype, fname, num in sorted(fields, key=lambda x: x[3]):
                if ftype in names:
                    wt, sub = "PB_MESSAGE", f"&shadnet_{ftype}_desc"
                elif ftype in enums:
                    wt, sub = "PB_INT32", "NULL"
                else:
                    wt, sub = SCALARS[ftype][1], "NULL"
                has = f"offsetof({s}, has_{fname})" if (ftype in names and not repeated) else "0"
                rep = "1" if repeated else "0"
                c.append(f"    {{{num}, {wt}, {rep}, offsetof({s}, {fname}), {has}, {sub}}},")
            c.append("};")
            c.append(f"const pb_msg_desc_t {s}_desc = {{sizeof({s}), {s}_fields,")
            c.append(f"    sizeof({s}_fields) / sizeof({s}_fields[0])}};")
        else:
            c.append(f"const pb_msg_desc_t {s}_desc = {{sizeof({s}), NULL, 0}};")
        c.append("")

    with open(os.path.join(HERE, "shadnet_msg.h"), "w") as f:
        f.write("\n".join(h) + "\n")
    with open(os.path.join(HERE, "shadnet_msg.c"), "w") as f:
        f.write("\n".join(c).rstrip("\n") + "\n")
    return 0


if __name__ == "__main__":
    sys.exit(main())
