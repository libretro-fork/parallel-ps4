/* SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <ctype.h>
#include <string.h>

#include "core/libraries/avplayer/avplayer_common.h"

static bool avplayer_iequals(const char* l, size_t length, const char* r) {
    size_t i;
    if (strlen(r) != length) {
        return false;
    }
    for (i = 0; i < length; i++) {
        if (tolower((unsigned char)l[i]) != tolower((unsigned char)r[i])) {
            return false;
        }
    }
    return true;
}

s32 avplayer_get_source_type(const char* path, size_t length) {
    size_t name_length = length;
    size_t i;
    size_t dot_pos;
    size_t ext_length;
    const char* ext;
    bool found = false;

    if (length == 0) {
        return AV_PLAYER_SOURCE_TYPE_UNKNOWN;
    }

    for (i = 0; i + 3 <= length; i++) {
        if (path[i] == ':' && path[i + 1] == '/' && path[i + 2] == '/') {
            found = true;
            break;
        }
    }
    if (found) {
        /* This path is a URI. Strip HTTP parameters from it.
         * schema://server.domain/path/file.ext/and/beyond?param=value#paragraph ->
         * -> schema://server.domain/path/to/file.ext/and/beyond */
        for (i = 0; i < length; i++) {
            if (path[i] == '?' || path[i] == '#') {
                break;
            }
        }
        name_length = i;
        if (name_length == 0) {
            return AV_PLAYER_SOURCE_TYPE_UNKNOWN;
        }
    }

    /* schema://server.domain/path/to/file.ext/and/beyond -> .ext/and/beyond
     * Find extension dot */
    found = false;
    dot_pos = 0;
    for (i = name_length; i-- > 0;) {
        if (path[i] == '.') {
            dot_pos = i;
            found = true;
            break;
        }
    }
    if (!found) {
        return AV_PLAYER_SOURCE_TYPE_UNKNOWN;
    }

    /* Extract extension (".ext/anything" or ".ext"), then .ext/and/beyond -> .ext */
    ext = path + dot_pos;
    ext_length = name_length - dot_pos;
    for (i = 0; i < ext_length; i++) {
        if (ext[i] == '/') {
            ext_length = i;
            break;
        }
    }

    if (avplayer_iequals(ext, ext_length, ".mp4") || avplayer_iequals(ext, ext_length, ".m4v") ||
        avplayer_iequals(ext, ext_length, ".m3d") || avplayer_iequals(ext, ext_length, ".m4a") ||
        avplayer_iequals(ext, ext_length, ".mov")) {
        return AV_PLAYER_SOURCE_TYPE_FILE_MP4;
    }
    if (avplayer_iequals(ext, ext_length, ".m3u8")) {
        return AV_PLAYER_SOURCE_TYPE_HLS;
    }
    return AV_PLAYER_SOURCE_TYPE_UNKNOWN;
}
