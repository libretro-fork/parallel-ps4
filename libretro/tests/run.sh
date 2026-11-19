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
# FMOD's two: no slot output before its mixer filled it, a frame starting while it mixes too.
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
# The frames handed to the core in the shared memory (libretro/frame_slots.c): claimed slots
# never the newest or held one, the core seeing only whole frames that stay as they are
for san in address,undefined thread; do
    name=frame_slots_test-$(echo $san | tr , -)
    $CC -std=c89 -O1 -g -Wall -fsanitize=$san -I$L/include libretro/tests/frame_slots_test.c \
        libretro/frame_slots.c -o "$OUT/$name" -lpthread
    "$OUT/$name"
done
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
# What is done once, where the core had std::call_once: four threads at 2000 of them
for san in address,undefined thread; do
    $CC -std=gnu89 -O1 -g -Wall -Wdeclaration-after-statement -fsanitize=$san -Isrc -I$L/include \
        libretro/tests/run_once_test.c src/common/run_once.c $L/rthreads/rthreads.c \
        $L/rthreads/retro_eventcount.c $L/features/features_cpu.c -o "$OUT/run_once_test" \
        -lpthread
    "$OUT/run_once_test"
done
# The service thread that can be asked to stop, where the core had std::jthread: 5000 started,
# stopped as they work or go into their wait, and joined
for san in address,undefined thread; do
    $CC -std=gnu89 -O1 -g -Wall -Wdeclaration-after-statement -fsanitize=$san -Isrc -I$L/include \
        libretro/tests/stop_thread_test.c src/common/stop_thread.c $L/rthreads/rthreads.c \
        $L/rthreads/retro_eventcount.c $L/features/features_cpu.c -o "$OUT/stop_thread_test" \
        -lpthread
    "$OUT/stop_thread_test"
done
# The latch that stands where the core had semaphores: every wait woken by its set, four
# threads, 20000 sets each
for san in address,undefined thread; do
    $CC -std=gnu89 -O1 -g -Wall -Wdeclaration-after-statement -fsanitize=$san -Isrc -I$L/include \
        libretro/tests/event_latch_test.c src/common/event_latch.c $L/rthreads/rthreads.c \
        $L/rthreads/retro_eventcount.c $L/features/features_cpu.c -o "$OUT/event_latch_test" \
        -lpthread
    "$OUT/event_latch_test"
done
# What the on-screen keyboard stands on: the list of keyboards on screen, on five threads, and
# text between UTF-16 and UTF-8; the text edited where it is as the same edit made of copies
for san in address,undefined thread; do
    $CC -std=gnu89 -O1 -g -Wall -Wdeclaration-after-statement -fsanitize=$san -Isrc -I$L/include \
        libretro/tests/ime_support_test.c src/core/libraries/ime/ime_support.c \
        $L/rthreads/rthreads.c $L/rthreads/retro_eventcount.c $L/encodings/encoding_utf.c \
        $L/compat/compat_strl.c $L/features/features_cpu.c -o "$OUT/ime_support_test" -lpthread
    "$OUT/ime_support_test"
done
# What of a free region of the address space can be reserved on Windows's granularity
$CC -std=c89 -pedantic -Wno-long-long -O1 -g -Wall -Wdeclaration-after-statement \
    -fsanitize=address,undefined -Isrc -I$L/include libretro/tests/reserve_span_test.c \
    src/common/reserve_span.c -o "$OUT/reserve_span_test"
"$OUT/reserve_span_test"
# The object table the network libraries keep their objects in: three threads holding and
# giving back objects while a fourth removes them and puts new ones in; none seen once freed,
# each freed exactly once
for san in address,undefined thread; do
    $CC -std=gnu89 -O1 -g -Wall -Wdeclaration-after-statement -fsanitize=$san -Isrc -I$L/include \
        libretro/tests/obj_table_test.c src/common/obj_table.c $L/rthreads/rthreads.c \
        $L/rthreads/retro_eventcount.c $L/features/features_cpu.c -o "$OUT/obj_table_test" -lpthread
    "$OUT/obj_table_test"
done
# The bounded many-to-one queue the keyboard's and the music player's events go through: four
# threads put into a queue too small for them, every item taken once, whole, in each one's order
for san in address,undefined thread; do
    $CC -std=gnu89 -O1 -g -Wall -Wdeclaration-after-statement -fsanitize=$san -Isrc -I$L/include \
        libretro/tests/mpsc_ring_test.c src/common/mpsc_ring.c $L/rthreads/rthreads.c \
        -o "$OUT/mpsc_ring_test" -lpthread
    "$OUT/mpsc_ring_test"
done
# A game thread's life: its record collected exactly once, however its end, its detach or join
# and other threads' holds fall; and its name read while another thread renames it
for san in address,undefined thread; do
    $CC -std=gnu89 -O1 -g -Wall -Wdeclaration-after-statement -fsanitize=$san -Isrc -I$L/include \
        libretro/tests/thread_life_test.c src/common/thread_life.c $L/rthreads/rthreads.c \
        -o "$OUT/thread_life_test" -lpthread
    "$OUT/thread_life_test"
    $CC -std=gnu89 -O1 -g -Wall -Wdeclaration-after-statement -fsanitize=$san -Isrc -I$L/include \
        libretro/tests/word_name_test.c src/common/word_name.c $L/rthreads/rthreads.c \
        -o "$OUT/word_name_test" -lpthread
    "$OUT/word_name_test"
done
# The game's libc file table: four threads taking and giving back slots of a table too small
# for all, never two in one slot
for san in address,undefined thread; do
    $CC -std=gnu89 -O1 -g -Wall -Wdeclaration-after-statement -fsanitize=$san -Isrc -I$L/include \
        libretro/tests/slot_table_test.c src/common/slot_table.c $L/rthreads/rthreads.c \
        -o "$OUT/slot_table_test" -lpthread
    "$OUT/slot_table_test"
done
# The game's stdout and stderr cut into lines, each thread its own buffer
$CC -std=c89 -pedantic -O1 -g -Wall -Wdeclaration-after-statement -fsanitize=address,undefined \
    -Isrc -I$L/include libretro/tests/line_buffer_test.c src/common/line_buffer.c \
    -o "$OUT/line_buffer_test"
"$OUT/line_buffer_test"
# The guest rwlocks' word: four threads reading and writing a block through it, never a
# reader beside a writer nor two writers
for san in address,undefined thread; do
    $CC -std=gnu89 -O1 -g -Wall -Wdeclaration-after-statement -fsanitize=$san -Isrc -I$L/include \
        libretro/tests/rw_word_test.c src/common/rw_word.c $L/rthreads/rthreads.c \
        -o "$OUT/rw_word_test" -lpthread
    "$OUT/rw_word_test"
done
# libSceAudio3d's ports without a lock: a ring's blocks whole and in order while a third
# thread drops what is queued; objects reserved by four threads while a fifth mixes; the mix's
# sums, stereo and 7.1
for san in address,undefined thread; do
    $CC -std=gnu89 -O1 -g -Wall -Wdeclaration-after-statement -fsanitize=$san -Isrc -I$L/include \
        libretro/tests/audio3d_port_test.c src/core/libraries/audio3d/audio3d_port.c \
        $L/rthreads/rthreads.c -o "$OUT/audio3d_port_test" -lpthread
    "$OUT/audio3d_port_test"
done
# The video player's state machine and who starts and stops it, moved by four threads at once:
# its moves the ones it had under a lock, none lost, one thread starting or stopping at a time
for san in address,undefined thread; do
    $CC -std=gnu89 -O1 -g -Wall -Wdeclaration-after-statement -fsanitize=$san -Isrc -I$L/include \
        libretro/tests/avplayer_fsm_test.c src/core/libraries/avplayer/avplayer_fsm.c \
        $L/rthreads/rthreads.c -o "$OUT/avplayer_fsm_test" -lpthread
    "$OUT/avplayer_fsm_test"
done
# The gate readers pin the video player's source through while its owner changes it: a reader
# let in never finds it half changed
for san in address,undefined thread; do
    $CC -std=gnu89 -O1 -g -Wall -Wdeclaration-after-statement -fsanitize=$san -Isrc -I$L/include \
        libretro/tests/pin_gate_test.c src/common/pin_gate.c $L/rthreads/rthreads.c \
        $L/rthreads/retro_eventcount.c $L/features/features_cpu.c -o "$OUT/pin_gate_test" -lpthread
    "$OUT/pin_gate_test"
done
# The video player's clock: the time into a file from times of the game's, pauses not counted
$CC -std=c89 -pedantic -Wno-long-long -O1 -g -Wall -Wdeclaration-after-statement \
    -fsanitize=address,undefined -Isrc libretro/tests/avplayer_clock_test.c \
    src/core/libraries/avplayer/avplayer_clock.c -o "$OUT/avplayer_clock_test"
"$OUT/avplayer_clock_test"
# A glyph into a game's surface straight from the face's bitmap, as it was written by way of a
# copy of the bitmap
$CC -std=c89 -pedantic -O1 -g -Wall -Wdeclaration-after-statement -fsanitize=address,undefined \
    -Isrc libretro/tests/font_blit_test.c src/core/libraries/font/font_blit.c \
    -o "$OUT/font_blit_test"
"$OUT/font_blit_test"
# A patch line that halves a number a game carries as text
$CC -std=c89 -pedantic -O1 -g -Wall -Wdeclaration-after-statement -fsanitize=address,undefined \
    -Isrc -I$L/include libretro/tests/patch_halve_test.c src/common/patch_halve.c \
    -o "$OUT/patch_halve_test"
"$OUT/patch_halve_test"
# Asynchronous shader compilation: a draw is left out only of an image the game drew into the
# frame before
$CC -std=c89 -pedantic -Wno-long-long -O1 -g -Wall -Wdeclaration-after-statement \
    -fsanitize=address,undefined -Isrc -I$L/include libretro/tests/target_frames_test.c \
    -o "$OUT/target_frames_test"
"$OUT/target_frames_test"
# The memory a crash report shows around an address
$CC -std=c89 -pedantic -Wno-long-long -O1 -g -Wall -Wdeclaration-after-statement \
    -fsanitize=address,undefined -Isrc -I$L/include libretro/tests/hex_dump_test.c \
    src/common/hex_dump.c -o "$OUT/hex_dump_test"
"$OUT/hex_dump_test"
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
# The overlays' library against ImGui: the FPS counter's window, layout and glyphs where ImGui
# put them, the atlas's white texel and baked lines, a notification and a trophy as ImGui drew
# them, their images' slots freed after; the splash decoded on its thread and fitted as the
# presenter's ImGui window fitted it
OVL="src/video_core/overlay/ovl_draw.c src/video_core/overlay/ovl_font.c \
     src/video_core/overlay/ovl_fps.c src/video_core/overlay/ovl_msgdialog.c \
     src/video_core/overlay/ovl_notify.c src/video_core/overlay/ovl_savedialog.c \
     src/video_core/overlay/ovl_profile.c src/video_core/overlay/ovl_commerce.c \
     src/video_core/overlay/ovl_invdialog.c \
     src/video_core/overlay/ovl_shadnet.c src/video_core/overlay/ovl_invite.c \
     src/video_core/overlay/ovl_splash.c src/video_core/overlay/ovl_tex.c \
     src/video_core/overlay/ovl_trophy.c \
     src/video_core/overlay/ovl_ttf.c src/video_core/overlay/ovl_ui.c \
     $L/formats/png/rpng.c $L/streams/trans_stream.c $L/streams/trans_stream_deflate.c \
     $L/streams/trans_stream_pipe.c $L/encodings/encoding_deflate.c \
     $L/encodings/encoding_crc32.c $L/features/features_cpu.c $L/rthreads/rthreads.c \
     $L/rthreads/retro_eventcount.c"
$CC -std=gnu89 -O1 -g -Wall -Wdeclaration-after-statement -ffp-contract=off \
    -fsanitize=address,undefined -I. -Isrc -I$L/include libretro/tests/ovl_test.c $OVL \
    -o "$OUT/ovl_test" -lm -lpthread
"$OUT/ovl_test" src/resources/fonts/NotoSans-Regular.ttf
# The overlays drawn on a device by the core's renderer (vk_overlay.c): the FPS counter, a
# notification and a trophy with their images, as the CPU model of them; the images' slots
# free again after
$CC -std=gnu89 -O1 -g -Wall -Wdeclaration-after-statement -fsanitize=address,undefined \
    -isystem externals/vulkan-headers/include -isystem externals/sirit/externals/SPIRV-Headers/include \
    -I. -Isrc -I$L/include libretro/tests/overlay_device_test.c src/video_core/overlay/vk_overlay.c \
    src/video_core/host_shaders/host_spirv.c $OVL -o "$OUT/overlay_device_test" -ldl -lm -lpthread
ASAN_OPTIONS=detect_leaks=0 "$OUT/overlay_device_test"
# The overlays' queues from four threads at once while frames are drawn: every notification
# shown once, each thread's in order, every image slot free after
for san in address,undefined thread; do
    $CC -std=gnu89 -O1 -g -Wall -Wdeclaration-after-statement -fsanitize=$san -I. -Isrc \
        -I$L/include libretro/tests/ovl_queue_test.c $OVL \
        -o "$OUT/ovl_queue_test" -lm -lpthread
    "$OUT/ovl_queue_test"
done
# The message dialog across threads: a game thread opening, changing and closing dialogs, another
# waiting for the emulator's own, the overlay's thread drawing and pressing cross; each finished
# once by one side, every wait returned
for san in address,undefined thread; do
    $CC -std=gnu89 -O1 -g -Wall -Wdeclaration-after-statement -fsanitize=$san -I. -Isrc \
        -I$L/include libretro/tests/ovl_msgdialog_test.c $OVL \
        -o "$OUT/ovl_msgdialog_test" -lm -lpthread
    "$OUT/ovl_msgdialog_test"
done
# The save data dialog across threads: a game thread building lists of saves with icons where
# the dialog says to, opening them, changing a progress bar, closing some; the overlay's thread
# drawing, pressing, uploading the icons and retiring them; each finished once by one side, a
# save chosen one of its list, every icon cell free after
for san in address,undefined thread; do
    $CC -std=gnu89 -O1 -g -Wall -Wdeclaration-after-statement -fsanitize=$san -I. -Isrc \
        -I$L/include libretro/tests/ovl_savedialog_test.c $OVL \
        -o "$OUT/ovl_savedialog_test" -lm -lpthread
    "$OUT/ovl_savedialog_test"
done
# The profile dialog across threads: a game thread opening dialogs and closing some, the
# overlay's thread drawing and pressing cross; each finished once by one side
for san in address,undefined thread; do
    $CC -std=gnu89 -O1 -g -Wall -Wdeclaration-after-statement -fsanitize=$san -I. -Isrc \
        -I$L/include libretro/tests/ovl_profile_test.c $OVL \
        -o "$OUT/ovl_profile_test" -lm -lpthread
    "$OUT/ovl_profile_test"
done
# The store's dialog and icon across threads: a game thread opening dialogs in every mode,
# closing some, moving the icon; the overlay's thread drawing and pressing cross; each finished
# once by one side, a press never a purchase
for san in address,undefined thread; do
    $CC -std=gnu89 -O1 -g -Wall -Wdeclaration-after-statement -fsanitize=$san -I. -Isrc \
        -I$L/include libretro/tests/ovl_commerce_test.c $OVL \
        -o "$OUT/ovl_commerce_test" -lm -lpthread
    "$OUT/ovl_commerce_test"
done
# The invitation dialog across threads: a game thread opening dialogs to send and to receive,
# closing some; the overlay's thread drawing and pressing cross; a server thread answering the
# requests, some refused, some twice; each finished once, a late answer changing nothing
for san in address,undefined thread; do
    $CC -std=gnu89 -O1 -g -Wall -Wdeclaration-after-statement -fsanitize=$san -I. -Isrc \
        -I$L/include libretro/tests/ovl_invdialog_test.c $OVL \
        -o "$OUT/ovl_invdialog_test" -lm -lpthread
    "$OUT/ovl_invdialog_test"
done
