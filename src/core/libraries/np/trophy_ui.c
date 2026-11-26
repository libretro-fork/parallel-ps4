/* SPDX-FileCopyrightText: Copyright 2025-2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <stdlib.h>
#include <string.h>

#include <compat/strl.h>
#include <file/file_path.h>
#include <formats/rmp3.h>
#include <streams/file_stream.h>

#include "common/c_bridge.h"
#include "common/types.h"
#include "core/emulator_settings.h"
#include "core/libraries/np/trophy_ui.h"
#include "libretro/audio_mix.h"
#include "video_core/overlay/ovl_layers.h"

/* The trophy sound, as the core's mix plays it: stereo at 48 kHz in the mix's format, made once
 * when the emulator starts and lent to the overlay, which plays it each time a trophy unlocks. */
static float* trophy_sound = NULL;
static unsigned trophy_sound_frames = 0;

/* A file mapped where it lies, or bytes lent */
typedef struct trophy_view {
    RFILE* file;
    const u8* data;
    size_t size;
} trophy_view_t;

static bool trophy_view_map(trophy_view_t* view, const char* path) {
    int64_t len = 0;
    view->file = NULL;
    view->data = NULL;
    view->size = 0;
    /* A missing file is an answer, not an error */
    if (!path_is_valid(path)) {
        return false;
    }
    view->file = filestream_open(path, RETRO_VFS_FILE_ACCESS_READ,
                                 RETRO_VFS_FILE_ACCESS_HINT_FREQUENT_ACCESS);
    if (view->file == NULL) {
        return false;
    }
    view->data = filestream_get_mapped_ptr(view->file, &len);
    if (view->data == NULL || len <= 0) {
        filestream_close(view->file);
        view->file = NULL;
        view->data = NULL;
        return false;
    }
    view->size = (size_t)len;
    return true;
}

static void trophy_view_close(trophy_view_t* view) {
    if (view->file != NULL) {
        filestream_close(view->file);
    }
    view->file = NULL;
    view->data = NULL;
    view->size = 0;
}

static u32 trophy_le16(const u8* p) {
    return (u32)(p[0] | (p[1] << 8));
}

static u32 trophy_le32(const u8* p) {
    return (u32)p[0] | ((u32)p[1] << 8) | ((u32)p[2] << 16) | ((u32)p[3] << 24);
}

/* 16-bit PCM in a RIFF/WAVE file, 1 or 2 channels: where its samples are, read in place. */
static bool trophy_parse_wav(const u8* data, size_t size, const s16** samples, u64* frames,
                             u32* channels, u32* hz) {
    bool have_fmt = false;
    size_t at;
    if (data == NULL || size < 12 || memcmp(data, "RIFF", 4) != 0 ||
        memcmp(data + 8, "WAVE", 4) != 0) {
        return false;
    }
    for (at = 12; at + 8 <= size;) {
        const u32 chunk = trophy_le32(data + at + 4);
        const u8* body = data + at + 8;
        if (chunk > size - at - 8) {
            return false;
        }
        if (memcmp(data + at, "fmt ", 4) == 0 && chunk >= 16) {
            if (trophy_le16(body) != 1 || trophy_le16(body + 14) != 16) {
                return false; /* only 16-bit PCM */
            }
            *channels = trophy_le16(body + 2);
            *hz = trophy_le32(body + 4);
            have_fmt = *channels == 1 || *channels == 2;
        } else if (memcmp(data + at, "data", 4) == 0 && have_fmt && *hz > 0) {
            *samples = (const s16*)(const void*)body;
            *frames = chunk / (2 * *channels);
            return true;
        }
        at += 8 + chunk + (chunk & 1);
    }
    return false;
}

/* Room for the sound resampled to 48 kHz stereo; false when there is none */
static bool trophy_sound_alloc(u64 frames, u32 hz, unsigned* room) {
    const u64 out_frames = (frames * 48000) / hz + 1;
    free(trophy_sound);
    trophy_sound = (float*)malloc((size_t)out_frames * 2 * sizeof(float));
    trophy_sound_frames = 0;
    *room = (unsigned)out_frames;
    return trophy_sound != NULL;
}

/* A 16-bit file (a WAV) resampled into the sound once */
static void trophy_resample_s16(const s16* samples, u64 frames, u32 channels, u32 hz) {
    audio_mix_resampler_t r;
    u64 pos = 0;
    unsigned room;
    audio_mix_resampler_init(&r, hz, channels);
    if (trophy_sound_alloc(frames, hz, &room)) {
        trophy_sound_frames =
            audio_mix_resample(&r, samples, frames, &pos, trophy_sound, room, 1.0f);
    }
}

/* A decoder's float output resampled into the sound once */
static void trophy_resample_f32(const float* samples, u64 frames, u32 channels, u32 hz) {
    audio_mix_resampler_t r;
    u64 pos = 0;
    unsigned room;
    audio_mix_resampler_init(&r, hz, channels);
    if (trophy_sound_alloc(frames, hz, &room)) {
        trophy_sound_frames =
            audio_mix_resample_f32(&r, samples, frames, &pos, trophy_sound, room, 1.0f);
    }
}

/* An MP3 decoded to float, as rmp3 makes it, so that the sound is rounded to 16 bits once, in
 * the mix, and not first here as well. *@pcm is the caller's to free. */
static bool trophy_decode_mp3_f32(const u8* data, size_t size, float** pcm, u64* out_frames,
                                  u32* channels, u32* hz) {
    rmp3 mp3;
    bool ok;
    *pcm = NULL;
    *out_frames = 0;
    if (!rmp3_init_memory(&mp3, data, size)) {
        return false;
    }
    *channels = mp3.channels;
    *hz = mp3.sampleRate;
    ok = (*channels == 1 || *channels == 2) && *hz > 0;
    if (ok) {
        /* Short: the length at 128 kbit/s, grown while the stream goes on */
        u64 room;
        u64 frames = 0;
        if (mp3.totalFrames) {
            room = mp3.totalFrames + 1;
        } else {
            room = (u64)size * *hz / 16000;
            if (room < 4096) {
                room = 4096;
            }
        }
        for (;;) {
            float* grown = (float*)realloc(*pcm, (size_t)(room * *channels) * sizeof(float));
            if (grown == NULL) {
                ok = false;
                break;
            }
            *pcm = grown;
            frames += rmp3_read_f32(&mp3, room - frames, *pcm + frames * *channels);
            if (frames < room) {
                break;
            }
            room *= 2;
        }
        *out_frames = frames;
        ok = ok && frames > 0;
    }
    rmp3_uninit(&mp3);
    if (!ok) {
        free(*pcm);
        *pcm = NULL;
    }
    return ok;
}

static void trophy_preload_sound_into(void) {
    char dir[4096];
    char path[4096];
    trophy_view_t view;
    const s16* samples = NULL;
    u64 frames = 0;
    u32 channels = 0;
    u32 hz = 0;
    size_t file_size = 0;
    const u8* file;

    free(trophy_sound);
    trophy_sound = NULL;
    trophy_sound_frames = 0;
    c_get_user_path(C_USER_PATH_CUSTOM_TROPHY, dir, sizeof(dir));

    fill_pathname_join_special(path, dir, "trophy.mp3", sizeof(path));
    if (path_is_valid(path)) {
        float* pcm = NULL;
        if (trophy_view_map(&view, path) &&
            trophy_decode_mp3_f32(view.data, view.size, &pcm, &frames, &channels, &hz)) {
            trophy_resample_f32(pcm, frames, channels, hz);
        } else {
            c_log(C_LOG_LIB_NP_TROPHY, C_LOG_LEVEL_ERROR, "trophy_ui_preload_sound",
                  "Could not decode the custom trophy sound %s", path);
        }
        free(pcm);
        trophy_view_close(&view);
        return;
    }
    fill_pathname_join_special(path, dir, "trophy.wav", sizeof(path));
    if (path_is_valid(path)) {
        if (trophy_view_map(&view, path) &&
            trophy_parse_wav(view.data, view.size, &samples, &frames, &channels, &hz)) {
            trophy_resample_s16(samples, frames, channels, hz);
        } else {
            c_log(C_LOG_LIB_NP_TROPHY, C_LOG_LEVEL_ERROR, "trophy_ui_preload_sound",
                  "The custom trophy sound %s is not 16-bit PCM", path);
        }
        trophy_view_close(&view);
        return;
    }
    file = c_get_embedded_resource("src/resources/trophy.wav", &file_size);
    if (trophy_parse_wav(file, file_size, &samples, &frames, &channels, &hz)) {
        trophy_resample_s16(samples, frames, channels, hz);
    }
}

void trophy_ui_preload_sound(void) {
    trophy_preload_sound_into();
    /* Lent to the overlay, which plays it as a trophy shows */
    ovl_trophy_set_sound(trophy_sound, trophy_sound_frames);
}

void trophy_ui_queue(const char* icon_path, const char* name, const char* rarity, bool after) {
    trophy_view_t icon;
    trophy_view_t type_icon;
    const char* type_name = NULL;

    if (emu_settings_get_trophy_popup_disabled()) {
        return;
    }
    /* The trophy's icon, mapped where it lies */
    if (!trophy_view_map(&icon, icon_path)) {
        c_log(C_LOG_LIB_NP_TROPHY, C_LOG_LEVEL_ERROR, "trophy_ui_queue",
              "Couldnt load trophy icon at %s", icon_path);
    }
    /* The type's, the user's own in the custom trophy folder or the core's */
    if (rarity != NULL && rarity[0] != '\0' && rarity[1] == '\0') {
        switch (rarity[0]) {
        case 'P':
            type_name = "platinum.png";
            break;
        case 'G':
            type_name = "gold.png";
            break;
        case 'S':
            type_name = "silver.png";
            break;
        case 'B':
            type_name = "bronze.png";
            break;
        default:
            break;
        }
    }
    type_icon.file = NULL;
    type_icon.data = NULL;
    type_icon.size = 0;
    if (type_name != NULL) {
        char dir[4096];
        char custom[4096];
        c_get_user_path(C_USER_PATH_CUSTOM_TROPHY, dir, sizeof(dir));
        fill_pathname_join_special(custom, dir, type_name, sizeof(custom));
        if (!trophy_view_map(&type_icon, custom)) {
            /* Embedded in the core: lent, not copied */
            char resource[64];
            strlcpy(resource, "src/resources/", sizeof(resource));
            strlcat(resource, type_name, sizeof(resource));
            type_icon.data = c_get_embedded_resource(resource, &type_icon.size);
        }
    }
    /* Decoded here, on the game's thread, before the overlay shows it */
    ovl_trophy_queue_after(name, icon.data, icon.size, type_icon.data, type_icon.size,
                           after ? 1 : 0);
    trophy_view_close(&icon);
    trophy_view_close(&type_icon);
}
