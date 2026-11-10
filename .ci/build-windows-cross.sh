#!/bin/bash
# SPDX-FileCopyrightText: 2026 shadPS4 Emulator Project
# SPDX-License-Identifier: GPL-2.0-or-later
#
# The Windows x64 core, cross-built on Linux for the libretro buildbot, whose
# own Windows runner has MSVC 2019 16.8 - too old for shadPS4, which builds
# with clang-cl against a current MSVC. Here it is clang-cl and lld-link (LLVM
# 19, as the Linux job) against the MSVC CRT and Windows SDK that xwin
# downloads, so the result links against the same runtime as the project's own
# Windows build. Run from the repository root in ubuntu:24.04, as root:
#
#   BUILD_DIR=build/windows-x86_64 NUMPROC=8 .ci/build-windows-cross.sh
#
# leaves the core at $BUILD_DIR/shadps4_libretro.dll.

set -euo pipefail

BUILD_DIR=${BUILD_DIR:-build/windows-x86_64}
NUMPROC=${NUMPROC:-$(nproc)}
[ "$NUMPROC" -gt 0 ] || NUMPROC=1
TOOLS=${SHAD_CROSS_TOOLS:-/opt/shad-cross}
LLVM=/usr/lib/llvm-19/bin
XWIN_VERSION=0.10.0
LLVM_WINDOWS=https://github.com/llvm/llvm-project/releases/download/llvmorg-19.1.7/clang+llvm-19.1.7-x86_64-pc-windows-msvc.tar.xz

export DEBIAN_FRONTEND=noninteractive
apt-get update -qq
apt-get install -y --no-install-recommends \
  ca-certificates wget gnupg git python3 python3-venv ninja-build make file xz-utils nasm
wget -qO- https://apt.llvm.org/llvm-snapshot.gpg.key > /etc/apt/trusted.gpg.d/apt.llvm.org.asc
echo "deb http://apt.llvm.org/noble/ llvm-toolchain-noble-19 main" > /etc/apt/sources.list.d/llvm-19.list
apt-get update -qq
apt-get install -y --no-install-recommends \
  clang-19 clang-tools-19 lld-19 llvm-19 libclang-rt-19-dev libstdc++-14-dev

# A current CMake; 24.04's pip wants a venv
mkdir -p "$TOOLS"
if [ ! -x "$TOOLS/venv/bin/cmake" ]; then
  python3 -m venv "$TOOLS/venv"
  "$TOOLS/venv/bin/pip" install -q --upgrade cmake
fi
export PATH="$TOOLS/venv/bin:$PATH"

# The MSVC CRT and the Windows SDK
# Fetched once for this xwin version; kept when TOOLS comes back from the
# buildbot's cache (.gitlab-ci.yml)
MSVC="$TOOLS/msvc"
if [ ! -f "$MSVC/.splat-$XWIN_VERSION" ]; then
  mkdir -p "$TOOLS"
  rm -rf "$MSVC"
  wget -q -O "$TOOLS/xwin.tar.gz" \
    "https://github.com/Jake-Shadle/xwin/releases/download/$XWIN_VERSION/xwin-$XWIN_VERSION-x86_64-unknown-linux-musl.tar.gz"
  tar -xzf "$TOOLS/xwin.tar.gz" -C "$TOOLS" --strip-components=1
  "$TOOLS/xwin" --accept-license --arch x86_64 --cache-dir "$TOOLS/xwin-cache" \
    splat --output "$MSVC"
  rm -rf "$TOOLS/xwin-cache" "$TOOLS/xwin.tar.gz"
  touch "$MSVC/.splat-$XWIN_VERSION"
fi

# compiler-rt's builtins for Windows, which the project links by name
# (128-bit division and the like, which MSVC's runtime does not have). The
# Linux packages carry the Linux ones only: taken out of LLVM's Windows
# release as it streams by, without keeping the archive.
wget -q -O - "$LLVM_WINDOWS" | tar -xJ --wildcards --strip-components=6 \
  -C "$MSVC/crt/lib/x86_64" '*/lib/clang/19/lib/windows/clang_rt.builtins-x86_64.lib'
test -f "$MSVC/crt/lib/x86_64/clang_rt.builtins-x86_64.lib"

# Headers the sources name in another case than the SDK's files (<Windows.h>
# for windows.h): a link of that spelling next to each, on a case-sensitive
# file system. Libraries asked for with a capital (Version.lib) likewise.
python3 - "$MSVC" <<'PY'
import os, re, sys
root = sys.argv[1]
dirs = [os.path.join(root, d) for d in ("crt/include", "sdk/include/ucrt",
        "sdk/include/um", "sdk/include/shared", "sdk/include/winrt")]
files = {}
for d in dirs:
    for name in os.listdir(d):
        files.setdefault(name.lower(), (d, name))
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
for name in wanted:
    hit = files.get(name.lower())
    if hit and hit[1] != name and not os.path.exists(os.path.join(hit[0], name)):
        os.symlink(hit[1], os.path.join(hit[0], name))
        made += 1
print(f"case links: {made}")
libs = 0
for d in ("sdk/lib/um/x86_64", "sdk/lib/ucrt/x86_64", "crt/lib/x86_64"):
    d = os.path.join(root, d)
    for name in os.listdir(d):
        if name.endswith(".lib") and name == name.lower():
            cap = name[0].upper() + name[1:]
            if not os.path.lexists(os.path.join(d, cap)):
                os.symlink(name, os.path.join(d, cap))
                libs += 1
print(f"library case links: {libs}")
PY

# The tools the build runs: glslang, which compiles the host shaders, the tool
# that turns the fonts into sources, and protoc. Built for Linux here, from
# the same sources, and handed to the Windows build, which cannot run its own.
HOST="$TOOLS/host"
CC=clang-19 CXX=clang++-19 cmake -S externals/glslang -B "$HOST/glslang" -G Ninja \
  -DCMAKE_BUILD_TYPE=Release -DENABLE_OPT=OFF -DGLSLANG_TESTS=OFF -DBUILD_SHARED_LIBS=OFF
cmake --build "$HOST/glslang" --target glslang-standalone -- -j "$NUMPROC"
HOST_GLSLANG=$(find "$HOST/glslang" -type f \( -name glslang -o -name glslangValidator \) -perm -u+x | head -1)
clang++-19 -O1 -o "$HOST/font-embed" externals/imgui/misc/fonts/binary_to_compressed_c.cpp
test -x "$HOST_GLSLANG"
# protoc with abseil beside it, set up as externals/CMakeLists.txt does
mkdir -p "$HOST/protoc-src"
cat > "$HOST/protoc-src/CMakeLists.txt" <<PROTOC_EOF
cmake_minimum_required(VERSION 3.24)
project(host_protoc C CXX)
set(CMAKE_CXX_STANDARD 17)
set(ABSL_PROPAGATE_CXX_STD ON)
add_subdirectory("$PWD/externals/abseil-cpp" abseil EXCLUDE_FROM_ALL)
set(protobuf_BUILD_TESTS OFF CACHE INTERNAL "")
set(protobuf_BUILD_EXAMPLES OFF CACHE INTERNAL "")
set(protobuf_BUILD_PROTOC_BINARIES ON CACHE INTERNAL "")
set(protobuf_INSTALL OFF CACHE INTERNAL "")
set(protobuf_WITH_ZLIB OFF CACHE INTERNAL "")
set(protobuf_BUILD_SHARED_LIBS OFF CACHE INTERNAL "")
set(protobuf_LOCAL_DEPENDENCIES_ONLY ON CACHE INTERNAL "")
set(protobuf_FORCE_FETCH_DEPENDENCIES OFF CACHE INTERNAL "")
add_subdirectory("$PWD/externals/protobuf" protobuf)
PROTOC_EOF
CC=clang-19 CXX=clang++-19 cmake -S "$HOST/protoc-src" -B "$HOST/protoc" -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build "$HOST/protoc" --target protoc -- -j "$NUMPROC"
HOST_PROTOC=$(find "$HOST/protoc" -type f -name 'protoc*' -perm -u+x | head -1)
test -x "$HOST_PROTOC"

TOOLCHAIN="$TOOLS/clang-cl-x86_64.cmake"
cat > "$TOOLCHAIN" <<TOOLCHAIN_EOF
set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_VERSION 10.0)
set(CMAKE_SYSTEM_PROCESSOR AMD64)
set(CMAKE_C_COMPILER "$LLVM/clang-cl")
set(CMAKE_CXX_COMPILER "$LLVM/clang-cl")
set(CMAKE_C_COMPILER_TARGET x86_64-pc-windows-msvc)
set(CMAKE_CXX_COMPILER_TARGET x86_64-pc-windows-msvc)
set(CMAKE_ASM_NASM_COMPILER nasm)
set(CMAKE_LINKER "$LLVM/lld-link")
set(CMAKE_AR "$LLVM/llvm-lib")
set(CMAKE_RC_COMPILER "$LLVM/llvm-rc")
set(CMAKE_MT "$LLVM/llvm-mt")
set(SHAD_CLANG_CL_INCLUDES
  "$MSVC/crt/include" "$MSVC/sdk/include/ucrt" "$MSVC/sdk/include/um"
  "$MSVC/sdk/include/shared" "$MSVC/sdk/include/winrt")
set(SHAD_CLANG_CL_LIBS
  "$MSVC/crt/lib/x86_64" "$MSVC/sdk/lib/um/x86_64" "$MSVC/sdk/lib/ucrt/x86_64")
set(_shad_includes "")
set(_shad_rc_includes "")
foreach(_shad_dir IN LISTS SHAD_CLANG_CL_INCLUDES)
  string(APPEND _shad_includes " /imsvc \"\${_shad_dir}\"")
  string(APPEND _shad_rc_includes " /I \"\${_shad_dir}\"")
endforeach()
set(_shad_flags "-Wno-unused-command-line-argument\${_shad_includes}")
set(_shad_link "/manifest:no")
foreach(_shad_dir IN LISTS SHAD_CLANG_CL_LIBS)
  string(APPEND _shad_link " /libpath:\"\${_shad_dir}\"")
endforeach()
set(CMAKE_C_FLAGS_INIT "\${_shad_flags}")
set(CMAKE_CXX_FLAGS_INIT "\${_shad_flags}")
set(CMAKE_RC_FLAGS_INIT "\${_shad_rc_includes}")
set(CMAKE_EXE_LINKER_FLAGS_INIT "\${_shad_link}")
set(CMAKE_SHARED_LINKER_FLAGS_INIT "\${_shad_link}")
set(CMAKE_MODULE_LINKER_FLAGS_INIT "\${_shad_link}")
# The release CRT only: xwin fetches no debug runtime, and the compiler
# checks would otherwise link msvcrtd
set(CMAKE_TRY_COMPILE_CONFIGURATION Release)
set(CMAKE_MSVC_RUNTIME_LIBRARY MultiThreadedDLL)
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
TOOLCHAIN_EOF

cmake -S . -B "$BUILD_DIR" -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_TOOLCHAIN_FILE="$TOOLCHAIN" \
  -DENABLE_DISCORD_RPC=OFF -DENABLE_UPDATER=OFF \
  -DSHADPS4_HOST_GLSLANG="$HOST_GLSLANG" -DSHADPS4_HOST_FONT_EMBED="$HOST/font-embed" \
  -DSHADPS4_HOST_PROTOC="$HOST_PROTOC"
export PATH="$LLVM:$PATH"
export LIB="$MSVC/crt/lib/x86_64;$MSVC/sdk/lib/um/x86_64;$MSVC/sdk/lib/ucrt/x86_64"
cmake --build "$BUILD_DIR" -- -j "$NUMPROC" ${SHAD_CROSS_KEEP_GOING:+-k 0}
# CMake leaves the core in the build directory itself; moved there if a later
# layout puts it deeper
BUILT=$(find "$BUILD_DIR" -name shadps4_libretro.dll | head -1)
[ "$BUILT" -ef "$BUILD_DIR/shadps4_libretro.dll" ] || mv "$BUILT" "$BUILD_DIR/shadps4_libretro.dll"
file "$BUILD_DIR/shadps4_libretro.dll"
