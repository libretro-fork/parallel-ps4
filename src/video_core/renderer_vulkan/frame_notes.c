/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <stdio.h>
#include <string.h>

#include <streams/file_stream.h>

#include "video_core/renderer_vulkan/frame_notes.h"
#include "video_core/renderer_vulkan/vk_device_fault.h"

#define BUSY 400u  /* draws in a frame from which it counts as one of the game proper */
#define APART 300u /* frames from one written frame to the next */
#define AT_MOST 3u /* frames written in all */

static RFILE* file;
static unsigned long flips;
static unsigned written;
static unsigned long draws, last_written;
static const char* frame_dir;

static void note(const char* what, const uint64_t* v) {
    filestream_printf(file, "%s | %lx%08lx %lx%08lx %lx%08lx %lx%08lx\n", what,
                      (unsigned long)(v[0] >> 32), (unsigned long)(v[0] & 0xffffffffu),
                      (unsigned long)(v[1] >> 32), (unsigned long)(v[1] & 0xffffffffu),
                      (unsigned long)(v[2] >> 32), (unsigned long)(v[2] & 0xffffffffu),
                      (unsigned long)(v[3] >> 32), (unsigned long)(v[3] & 0xffffffffu));
}

void frame_notes_flip(const char* dir) {
    if (file) {
        vk_device_fault_sink(NULL);
        filestream_close(file);
        file = NULL;
    }
    ++flips;
    /* The frame after a busy one, so not a logo or a loading screen */
    if (dir && draws >= BUSY && written < AT_MOST && (!written || flips - last_written >= APART) &&
        strlen(dir) < 3900) {
        char path[4096];
        sprintf(path, "%s/frame_%05lu.txt", dir, flips);
        file = filestream_open(path, RETRO_VFS_FILE_ACCESS_WRITE, RETRO_VFS_FILE_ACCESS_HINT_NONE);
        if (file) {
            ++written;
            last_written = flips;
            frame_dir = dir;
            vk_device_fault_sink(note);
        }
    }
    draws = 0;
}

void frame_notes_draw(void) {
    ++draws;
}

unsigned long frame_notes_frame(void) {
    return file ? flips : 0;
}

int frame_notes_image_path(char* path, size_t size, unsigned seq, unsigned long address_high,
                           unsigned long address_low, unsigned format, unsigned width,
                           unsigned height, unsigned step) {
    if (!file || !frame_dir || strlen(frame_dir) + 128 > size)
        return 0;
    sprintf(path, "%s/frame_%05lu_img%03u_%lx%08lx_f%u_%ux%u_s%u.raw", frame_dir, flips, seq,
            address_high, address_low, format, width, height, step);
    return 1;
}
