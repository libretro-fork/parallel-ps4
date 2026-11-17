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
# The sound's whole path on threads: a game's port writing into its input in the shared pool,
# retro_run mixing exactly a frame of it at 60 Hz with jitter, the frame thread telling the
# sources there is room: every sample once, in order, from the first run (three seconds of
# real time: it runs the frontend's cadence)
for san in address,undefined thread; do
    $CC -std=gnu89 -O1 -g -Wall -Wdeclaration-after-statement -fsanitize=$san -I$L/include \
        libretro/tests/audio_pipeline_test.c libretro/audio_mix.c $COMMON $L/time/rtime.c \
        -o "$OUT/audio_pipeline_test" -lpthread -lm
    "$OUT/audio_pipeline_test"
done
# A game's sound on the game's time against the frame-locked mix (the real game_clock and
# audio_mix), retro_run never waiting on the game: exactly 800 of the game's samples every run,
# in order (two seconds a case). Four cases that still crackle are reported, not counted.
for san in address,undefined thread; do
    $CC -std=gnu89 -O1 -g -Wall -Wdeclaration-after-statement -fsanitize=$san -Isrc -I$L/include \
        libretro/tests/audio_game_model_test.c libretro/audio_mix.c src/common/game_clock.c \
        $COMMON $L/time/rtime.c -o "$OUT/audio_game_model_test" -lpthread -lm
    "$OUT/audio_game_model_test"
done
# The frame-locked mix: exactly a frame of every source's sound a frame, in order, none
# queued past a frame and a buffer, a source that stops going silent
$CC -std=gnu89 -O1 -g -Wall -Wdeclaration-after-statement -fsanitize=address,undefined \
    -I$L/include libretro/tests/audio_frame_test.c libretro/audio_mix.c $COMMON \
    -o "$OUT/audio_frame_test" -lpthread -lm
"$OUT/audio_frame_test"
for san in address,undefined thread; do
    name=audio_mix_test-$(echo $san | tr , -)
    $CC -std=gnu99 -O1 -g -Wall -fno-omit-frame-pointer -fsanitize=$san -I$L/include \
        libretro/tests/audio_mix_test.c libretro/audio_mix.c $COMMON \
        -o "$OUT/$name" -lpthread -lm
    "$OUT/$name"
    # The mix made for surround: 7.1 and stereo inputs, the result's width, both scales
    name=audio_mix_wide_test-$(echo $san | tr , -)
    $CC -std=gnu99 -O1 -g -Wall -fno-omit-frame-pointer -fsanitize=$san -I$L/include \
        libretro/tests/audio_mix_wide_test.c libretro/audio_mix.c $COMMON \
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
# The game's memory lent to the GPU (buffer_cache/guest_import.c) on a real device: copied
# from, written into where a scattered address space maps it, chunks imported once, as wanted
# or as mapped by threads racing
for san in address,undefined thread; do
    name=guest_import_device_test-$(echo $san | tr , -)
    $CC -std=c89 -O1 -g -Wall -fsanitize=$san -isystem externals/vulkan-headers/include \
        -Isrc -I$L/include libretro/tests/guest_import_device_test.c \
        src/video_core/buffer_cache/guest_import.c -o "$OUT/$name" -ldl -lpthread
    ASAN_OPTIONS=detect_leaks=0 "$OUT/$name"
done
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
"$OUT/pad_scale_test"
# The touchpad's fingers into the pad's two slots
$CC -std=c89 -pedantic -O1 -g -Wall -fsanitize=address,undefined \
    libretro/tests/touch_track_test.c -o "$OUT/touch_track_test"
# The lock-free pool of free nodes, on four threads
for san in address,undefined thread; do
    $CC -std=gnu89 -O1 -g -Wall -Wdeclaration-after-statement -fsanitize=$san -Isrc -I$L/include \
        libretro/tests/node_pool_test.c src/common/node_pool.c $L/rthreads/rthreads.c \
        $L/features/features_cpu.c -o "$OUT/node_pool_test" -lpthread
    "$OUT/node_pool_test"
done
# The Patch Manager's catalog: a game's patches from two repositories
$CC -std=c89 -O1 -g -Wall -Wdeclaration-after-statement -fsanitize=address,undefined \
    -Isrc -I$L/include libretro/tests/patch_catalog_test.c src/common/patch_catalog.c \
    $L/formats/xml/rxml.c -o "$OUT/patch_catalog_test"
# The console's vblanks from the frontend's frames, at every rate, a stall, a rate change, the
# wrap
$CC -std=c89 -pedantic -O1 -g -Wall -Wdeclaration-after-statement -fsanitize=address,undefined \
    -Isrc libretro/tests/vblank_clock_test.c src/common/vblank_clock.c -o "$OUT/vblank_clock_test"
"$OUT/vblank_clock_test"
# The runs of mapped memory the GPU thread looks up: lookups, overflow, and three readers
# against a writer republishing as fast as it can (a seqlock: not under ThreadSanitizer)
$CC -std=gnu89 -O1 -g -Wall -Wdeclaration-after-statement -fsanitize=address,undefined -Isrc \
    -I$L/include libretro/tests/range_runs_test.c src/common/range_runs.c $L/rthreads/rthreads.c \
    $L/features/features_cpu.c -o "$OUT/range_runs_test" -lpthread
"$OUT/range_runs_test"
# The game's time from the frontend's frames: exact over a minute (60 and 59.94), advances
# within a frame and never past it, four threads advancing while frames start
for san in address,undefined thread; do
    $CC -std=gnu89 -O1 -g -Wall -Wdeclaration-after-statement -fsanitize=$san -Isrc -I$L/include \
        libretro/tests/game_clock_test.c src/common/game_clock.c $L/rthreads/rthreads.c \
        $L/rthreads/retro_eventcount.c \
        $L/features/features_cpu.c -o "$OUT/game_clock_test" -lpthread
    "$OUT/game_clock_test"
done
# A thread ended from anywhere in it, as a guest thread by pthread_exit: 2000 of them,
# released while running
for san in address,undefined thread; do
    $CC -std=gnu89 -O1 -g -Wall -Wdeclaration-after-statement -fsanitize=$san -Isrc -I$L/include \
        libretro/tests/host_thread_test.c src/common/host_thread.c $L/rthreads/rthreads.c \
        $L/features/features_cpu.c -o "$OUT/host_thread_test" -lpthread
    "$OUT/host_thread_test"
done
# A thread's name, its own, on four threads
for san in address,undefined thread; do
    $CC -std=gnu89 -O1 -g -Wall -Wdeclaration-after-statement -fsanitize=$san -Isrc -I$L/include \
        libretro/tests/thread_name_test.c src/common/thread_name.c $L/rthreads/rthreads.c \
        $L/features/features_cpu.c -o "$OUT/thread_name_test" -lpthread
    "$OUT/thread_name_test"
done
"$OUT/patch_catalog_test"
"$OUT/touch_track_test"
# The music player's block decoder, on a sine made by ffmpeg in each format it plays (an MP3
# with its tag frame and one without); skipped with no ffmpeg
$CC -std=c89 -D_POSIX_C_SOURCE=200809L -DHAVE_RAAC -O1 -g -Wall -fsanitize=address,undefined \
    -Isrc -I$L/include libretro/tests/audio_stream_test.c src/common/audio_stream.c \
    $L/formats/mp3/rmp3.c $L/formats/flac/rflac.c $L/formats/aac/raac.c \
    $L/formats/mp4/rmp4_audio.c $L/formats/mp4/rmp4.c $L/features/features_cpu.c \
    -o "$OUT/audio_stream_test" -lm
if command -v ffmpeg >/dev/null 2>&1; then
    sine="sine=frequency=440:duration=7"
    ffmpeg -loglevel error -y -f lavfi -i "$sine:sample_rate=44100" -ac 2 -sample_fmt s32 \
        -c:a flac "$OUT/sine.flac"
    ffmpeg -loglevel error -y -f lavfi -i "$sine:sample_rate=44100" -ac 2 -c:a libmp3lame \
        -b:a 192k "$OUT/sine.mp3"
    ffmpeg -loglevel error -y -f lavfi -i "$sine:sample_rate=32000" -ac 1 -c:a libmp3lame \
        -write_xing 0 "$OUT/sine_notag.mp3"
    ffmpeg -loglevel error -y -f lavfi -i "$sine:sample_rate=48000" -ac 2 -c:a aac -b:a 160k \
        -f adts "$OUT/sine.aac"
    ffmpeg -loglevel error -y -f lavfi -i "$sine:sample_rate=48000" -ac 1 -c:a aac \
        "$OUT/sine.m4a"
    # 22.05 kHz AAC: decoded at twice the rate, as HE-AAC (SBR)
    ffmpeg -loglevel error -y -f lavfi -i "$sine:sample_rate=22050" -ac 2 -c:a aac \
        -f adts "$OUT/sine_sbr.aac"
    ffmpeg -loglevel error -y -f lavfi -i "$sine:sample_rate=22050" -ac 2 -c:a aac \
        "$OUT/sine_sbr.m4a"
    files="$OUT/sine.flac $OUT/sine.mp3 $OUT/sine_notag.mp3 $OUT/sine.aac $OUT/sine.m4a"
    files="$files $OUT/sine_sbr.aac $OUT/sine_sbr.m4a"
    # A FLAC with a seek table, which seeks take a shortcut through (ffmpeg writes none)
    if command -v flac >/dev/null 2>&1 && command -v metaflac >/dev/null 2>&1; then
        ffmpeg -loglevel error -y -f lavfi -i "$sine:sample_rate=48000" -ac 2 -c:a pcm_s24le \
            -fflags +bitexact "$OUT/sine24.wav"
        flac --silent --force -S- -o "$OUT/sine_seektable.flac" "$OUT/sine24.wav"
        metaflac --add-seekpoint=20x "$OUT/sine_seektable.flac"
        files="$files $OUT/sine_seektable.flac"
    fi
    "$OUT/audio_stream_test" $files
else
    "$OUT/audio_stream_test"
fi
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
