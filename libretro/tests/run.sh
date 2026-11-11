#!/bin/sh
# SPDX-FileCopyrightText: 2026 shadPS4 Emulator Project
# SPDX-License-Identifier: GPL-2.0-or-later
#
# The core's own tests, each built with AddressSanitizer and UndefinedBehaviorSanitizer and
# with ThreadSanitizer. Run from the repository's root: sh libretro/tests/run.sh
set -e
CC=${CC:-cc}
CXX=${CXX:-c++}
OUT=${OUT:-build-tests}
L=libretro/libretro-common
COMMON="$L/queues/retro_spsc.c $L/queues/retro_waitable_spsc.c $L/rthreads/retro_eventcount.c
        $L/rthreads/rthreads.c $L/features/features_cpu.c $L/compat/compat_strl.c"
mkdir -p "$OUT"
for san in address,undefined thread; do
    name=audio_mix_test-$(echo $san | tr , -)
    $CC -std=gnu99 -O1 -g -Wall -fno-omit-frame-pointer -fsanitize=$san -I$L/include \
        libretro/tests/audio_mix_test.c libretro/audio_mix.c $COMMON \
        -o "$OUT/$name" -lpthread -lm
    "$OUT/$name"
    name=write_snapshots_test-$(echo $san | tr , -)
    $CC -O1 -g -Wall -fno-omit-frame-pointer -fsanitize=$san -Isrc -I$L/include \
        libretro/tests/write_snapshots_test.c src/video_core/buffer_cache/write_snapshots.c \
        $L/rthreads/rthreads.c -o "$OUT/$name" -lpthread
    "$OUT/$name"
    # libSceUlt's objects: C++, on the core's own lock-free primitives
    name=ult_sync_test-$(echo $san | tr , -)
    for c in rthreads/retro_eventcount.c rthreads/rthreads.c features/features_cpu.c; do
        $CC -O1 -g -fsanitize=$san -I$L/include -c $L/$c -o "$OUT/ult_$(basename $c).o"
    done
    $CXX -std=c++20 -O1 -g -Wall -fno-omit-frame-pointer -fsanitize=$san -Isrc -I$L/include \
        libretro/tests/ult_sync_test.cpp "$OUT"/ult_*.o -o "$OUT/$name" -lpthread
    "$OUT/$name"
    # libSceNgs2's engine: C89, the grains it renders against what they have to be
    name=ngs2_core_test-$(echo $san | tr , -)
    $CC -std=c89 -O1 -g -Wall -Wdeclaration-after-statement -fno-omit-frame-pointer \
        -fsanitize=$san -Isrc -I$L/include libretro/tests/ngs2_core_test.c \
        src/core/libraries/ngs2/ngs2_core.c $L/formats/atrac9/ratrac9.c -o "$OUT/$name" -lm
    "$OUT/$name"
    name=vk_heap_test-$(echo $san | tr , -)
    $CC -std=c89 -pedantic -O1 -g -Wall -Wdeclaration-after-statement -Wno-long-long \
        -fno-omit-frame-pointer -fsanitize=$san -isystem externals/vulkan-headers/include -Isrc \
        libretro/tests/vk_heap_test.c src/video_core/renderer_vulkan/vk_heap.c -o "$OUT/$name"
    "$OUT/$name"
done
# The heap on a real device (lavapipe where there is no GPU), with the validation layer when
# it is installed; skipped with no Vulkan
$CC -std=c89 -O1 -g -Wall -fsanitize=address,undefined -isystem externals/vulkan-headers/include \
    -Isrc libretro/tests/vk_heap_device_test.c src/video_core/renderer_vulkan/vk_heap.c \
    -o "$OUT/vk_heap_device_test" -ldl
ASAN_OPTIONS=detect_leaks=0 "$OUT/vk_heap_device_test"
# The vertex capture's report on a capture it makes itself, a mesh of each kind in it
$CC -std=c89 -pedantic -O1 -g -Wall -Wno-long-long -fsanitize=address,undefined \
    tools/vertex_capture_report.c -o "$OUT/vertex_capture_report" -lm
ASAN_OPTIONS=detect_leaks=0 "$OUT/vertex_capture_report" --selftest
