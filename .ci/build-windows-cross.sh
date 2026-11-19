#!/bin/bash
# SPDX-FileCopyrightText: 2026 shadPS4 Emulator Project
# SPDX-License-Identifier: GPL-2.0-or-later
#
# The Windows x64 core, cross-built on Linux with the static Makefile: clang
# and lld (LLVM 19, as the Linux job) for MinGW-w64, against the C runtime
# (UCRT), the Windows headers and the libstdc++ of MSYS2's UCRT64 environment.
# That is the build `make` gives in an MSYS2 UCRT64 shell on Windows, made
# here because the libretro buildbot's own Windows runner has a MinGW too old
# for the sources. Run from the repository root in ubuntu:24.04, as root:
#
#   NUMPROC=8 .ci/build-windows-cross.sh
#
# leaves the core at parallel_ps4_libretro.dll in the repository root.
#
# MSYS2 keeps only the current version of a package, so the versions are not
# named here: they are whatever its package list has, each file checked
# against the SHA-256 the list gives for it.
#
#   SHAD_CROSS_TOOLS   where the sysroot is kept (the buildbot's cache)
#   SHAD_CROSS_NO_APT  set: the tools are there already, install nothing
#   MSYS2_REPO         the package repository (a file:// one for a mirror)

set -euo pipefail

NUMPROC=${NUMPROC:-$(nproc)}
[ "$NUMPROC" -gt 0 ] || NUMPROC=1
TOOLS=${SHAD_CROSS_TOOLS:-/opt/shad-cross}
MSYS2_REPO=${MSYS2_REPO:-https://repo.msys2.org/mingw/ucrt64}
# The C runtime and Windows headers, libstdc++ and libgcc (in the gcc
# package, with their headers), and the pthread library libstdc++ stands on
PACKAGES="crt headers gcc winpthreads libwinpthread"

if [ -z "${SHAD_CROSS_NO_APT:-}" ]; then
  export DEBIAN_FRONTEND=noninteractive
  apt-get update -qq
  apt-get install -y --no-install-recommends \
    ca-certificates wget curl gnupg git python3 make file zstd
  wget -qO- https://apt.llvm.org/llvm-snapshot.gpg.key > /etc/apt/trusted.gpg.d/apt.llvm.org.asc
  echo "deb http://apt.llvm.org/noble/ llvm-toolchain-noble-19 main" > /etc/apt/sources.list.d/llvm-19.list
  apt-get update -qq
  apt-get install -y --no-install-recommends clang-19 lld-19 llvm-19
fi

# The sysroot: fetched when the package list names other files than the ones
# it was made from; kept when TOOLS comes back from the buildbot's cache
# (.gitlab-ci.yml)
SYSROOT="$TOOLS/ucrt64"
mkdir -p "$TOOLS"
curl -fsSL -o "$TOOLS/ucrt64.db" "$MSYS2_REPO/ucrt64.db"
python3 - "$TOOLS/ucrt64.db" $PACKAGES > "$TOOLS/packages.txt" <<'PY'
import io, subprocess, sys, tarfile
db, wanted = sys.argv[1], ["mingw-w64-ucrt-x86_64-" + n for n in sys.argv[2:]]
# The list is a tar of one desc file a package: "%KEY%" lines, each followed by its value
data = subprocess.run(["zstd", "-dc", db], check=True, capture_output=True).stdout
found = {}
with tarfile.open(fileobj=io.BytesIO(data)) as tar:
    for member in tar:
        if not member.name.endswith("/desc"):
            continue
        lines = tar.extractfile(member).read().decode().split("\n")
        desc = {lines[i]: lines[i + 1] for i in range(len(lines) - 1) if lines[i].startswith("%")}
        if desc.get("%NAME%") in wanted:
            found[desc["%NAME%"]] = (desc["%FILENAME%"], desc["%SHA256SUM%"])
missing = [n for n in wanted if n not in found]
if missing:
    sys.exit("not in MSYS2's package list: " + " ".join(missing))
for name in wanted:
    print(*found[name])
PY
if ! cmp -s "$TOOLS/packages.txt" "$SYSROOT/.packages"; then
  rm -rf "$SYSROOT" "$TOOLS/fetch"
  mkdir -p "$SYSROOT" "$TOOLS/fetch"
  while read -r file sum; do
    curl -fsSL -o "$TOOLS/fetch/$file" "$MSYS2_REPO/$file"
    echo "$sum  $TOOLS/fetch/$file" | sha256sum -c --quiet -
    # Every package's files are under ucrt64/
    zstd -dc "$TOOLS/fetch/$file" | tar -x -C "$SYSROOT" --strip-components=1 ucrt64
  done < "$TOOLS/packages.txt"
  rm -rf "$TOOLS/fetch"
  cp "$TOOLS/packages.txt" "$SYSROOT/.packages"
fi
cat "$SYSROOT/.packages"

# Headers the sources name in another case than MinGW's files (<Windows.h>
# for windows.h): a link of that spelling next to each, on a case-sensitive
# file system. Every run: the sources change more often than the sysroot.
python3 - "$SYSROOT/include" <<'PY'
import os, re, sys
include = sys.argv[1]
files = {name.lower(): name for name in os.listdir(include)}
wanted = set()
pattern = re.compile(rb'#\s*include\s*[<"]([A-Za-z0-9_]+\.[hH])[>"]')
for top in ("src", "libretro", "externals"):
    for base, _, names in os.walk(top):
        for n in names:
            if n.endswith((".h", ".hpp", ".c", ".cc", ".cpp", ".inl", ".ipp")):
                try:
                    data = open(os.path.join(base, n), "rb").read()
                except OSError:
                    continue
                wanted.update(m.decode() for m in pattern.findall(data))
made = 0
for name in sorted(wanted):
    real = files.get(name.lower())
    if real and real != name and not os.path.lexists(os.path.join(include, name)):
        os.symlink(real, os.path.join(include, name))
        made += 1
print(f"case links: {made}")
PY

# The Makefile takes the platform from the compiler's name, which says
# windows-gnu here and not mingw: named
CROSS="--target=x86_64-w64-mingw32 --sysroot=$SYSROOT -fuse-ld=lld"
make -j "$NUMPROC" platform=win CC="clang-19 $CROSS" CXX="clang++-19 $CROSS" \
  ${SHAD_CROSS_KEEP_GOING:+-k}
file parallel_ps4_libretro.dll
# Nothing of MinGW's is needed beside the core: the Makefile links its
# runtime in
! llvm-objdump-19 -p parallel_ps4_libretro.dll | grep -i "DLL Name" |
  grep -i -E "libstdc|libgcc|libwinpthread"
