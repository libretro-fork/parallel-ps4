#!/bin/sh
# SPDX-FileCopyrightText: 2026 shadPS4 Emulator Project
# SPDX-License-Identifier: GPL-2.0-or-later
#
# The core's own tests, each built with AddressSanitizer and UndefinedBehaviorSanitizer and
# with ThreadSanitizer. Run from the repository's root: sh libretro/tests/run.sh
set -e
CC=${CC:-cc}
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
done
