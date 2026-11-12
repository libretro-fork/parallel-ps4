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
    # libSceUlt's objects: C89, on libretro-common's lock-free primitives
    name=ult_sync_test-$(echo $san | tr , -)
    $CC -std=gnu89 -O1 -g -Wall -Wdeclaration-after-statement -fno-omit-frame-pointer \
        -fsanitize=$san -Isrc -I$L/include libretro/tests/ult_sync_test.c \
        src/core/libraries/ult/ult_sync.c $L/rthreads/retro_eventcount.c $L/rthreads/rthreads.c \
        $L/features/features_cpu.c -o "$OUT/$name" -lpthread
    "$OUT/$name"
    # libSceNgs2's engine: C89, the grains it renders against what they have to be
    name=ngs2_core_test-$(echo $san | tr , -)
    $CC -std=c89 -O1 -g -Wall -Wdeclaration-after-statement -fno-omit-frame-pointer \
        -fsanitize=$san -Isrc -Ilibretro/tests -I$L/include libretro/tests/ngs2_core_test.c \
        src/core/libraries/ngs2/ngs2_core.c $L/formats/atrac9/ratrac9.c -o "$OUT/$name" -lm
    "$OUT/$name"
    # libSceNgs2's waveform parsing, panning and geometry
    name=ngs2_util_test-$(echo $san | tr , -)
    $CC -std=c89 -O1 -g -Wall -Wdeclaration-after-statement -fno-omit-frame-pointer \
        -fsanitize=$san -Isrc -I$L/include libretro/tests/ngs2_util_test.c \
        src/core/libraries/ngs2/ngs2_util.c src/core/libraries/ngs2/ngs2_core.c \
        $L/formats/atrac9/ratrac9.c -o "$OUT/$name" -lm
    "$OUT/$name"
    # libSceAudiodec, as a game calls it
    name=audiodec_test-$(echo $san | tr , -)
    $CC -std=c89 -O1 -g -Wall -Wdeclaration-after-statement -fno-omit-frame-pointer \
        -fsanitize=$san -Isrc -Ilibretro/tests -I$L/include libretro/tests/audiodec_test.c \
        src/core/libraries/audiodec/audiodec.c $L/formats/atrac9/ratrac9.c \
        $L/formats/mp3/rmp3.c $L/formats/aac/raac.c -o "$OUT/$name" -lm
    "$OUT/$name"
    # What is said of a lost GPU device, against a driver made up for it
    name=vk_device_fault_test-$(echo $san | tr , -)
    $CC -std=c89 -O1 -g -Wall -Wdeclaration-after-statement -Wno-long-long \
        -fno-omit-frame-pointer -fsanitize=$san -isystem externals/vulkan-headers/include -Isrc \
        -I$L/include libretro/tests/vk_device_fault_test.c \
        src/video_core/renderer_vulkan/vk_device_fault.c -o "$OUT/$name"
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
# The tiling shader on a real device: tiled and detiled texels are the texels they were, and a
# texel with no place in the image is neither read nor written
$CC -std=c89 -O1 -g -Wall -fsanitize=address,undefined -isystem externals/vulkan-headers/include \
    -isystem externals/sirit/externals/SPIRV-Headers/include -Isrc -I$L/include \
    libretro/tests/tiling_device_test.c src/video_core/host_shaders/host_spirv.c \
    -o "$OUT/tiling_device_test" -ldl
ASAN_OPTIONS=detect_leaks=0 "$OUT/tiling_device_test"
# libretro's analog values to the pad's bytes, rounded, a stick at rest centred
$CC -std=c89 -O1 -g -Wall -Wno-unused-function -fsanitize=address,undefined \
    libretro/tests/pad_scale_test.c -o "$OUT/pad_scale_test"
# Ten-bit frames read back, put in libretro's order for HDR10
$CC -std=c89 -pedantic -O1 -g -Wall -fsanitize=address,undefined \
    libretro/tests/pixel_order_test.c -o "$OUT/pixel_order_test"
"$OUT/pixel_order_test"
"$OUT/pad_scale_test"
# Copy GPU Buffers' room: a frame's copies are kept until the GPU thread is done with them
$CC -std=c89 -O1 -g -Wall -fsanitize=address,undefined -Isrc -I$L/include \
    libretro/tests/cmd_copy_ring_test.c src/video_core/amdgpu/cmd_copy_ring.c \
    -o "$OUT/cmd_copy_ring_test"
"$OUT/cmd_copy_ring_test"
# The game's sample position registers as positions
$CC -std=c89 -O1 -g -Wall -fsanitize=address,undefined -Isrc -I$L/include \
    libretro/tests/sample_locs_test.c src/video_core/amdgpu/sample_locs.c -o "$OUT/sample_locs_test"
"$OUT/sample_locs_test"
# The frames' ten-bit format on a device, and a readback's narrowing of it to XRGB8888
$CC -std=c89 -O1 -g -Wall -fsanitize=address,undefined -isystem externals/vulkan-headers/include \
    libretro/tests/frame_narrow_device_test.c -o "$OUT/frame_narrow_device_test" -ldl
ASAN_OPTIONS=detect_leaks=0 "$OUT/frame_narrow_device_test"
# The depth-to-colour shader on a real device: each sample of a depth buffer's depth and stencil
# comes out as the colour it should
$CC -std=c89 -O1 -g -Wall -fsanitize=address,undefined -isystem externals/vulkan-headers/include \
    -isystem externals/sirit/externals/SPIRV-Headers/include -Isrc -I$L/include \
    libretro/tests/depth_to_color_device_test.c src/video_core/host_shaders/host_spirv.c \
    -o "$OUT/depth_to_color_device_test" -ldl
ASAN_OPTIONS=detect_leaks=0 "$OUT/depth_to_color_device_test"
# The output pass on a device: SDR unchanged, PQ for an HDR frontend, a game's HDR passed through
$CC -std=c89 -O1 -g -Wall -fsanitize=address,undefined -isystem externals/vulkan-headers/include \
    -isystem externals/sirit/externals/SPIRV-Headers/include -Isrc -I$L/include \
    libretro/tests/pq_output_device_test.c src/video_core/host_shaders/host_spirv.c \
    -o "$OUT/pq_output_device_test" -ldl -lm
ASAN_OPTIONS=detect_leaks=0 "$OUT/pq_output_device_test"
# The vertex capture's report on a capture it makes itself, a mesh of each kind in it
$CC -std=c89 -pedantic -O1 -g -Wall -Wno-long-long -fsanitize=address,undefined \
    tools/vertex_capture_report.c -o "$OUT/vertex_capture_report" -lm
ASAN_OPTIONS=detect_leaks=0 "$OUT/vertex_capture_report" --selftest
