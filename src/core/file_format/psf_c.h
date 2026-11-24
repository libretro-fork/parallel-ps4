/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef SHADPS4_FILE_FORMAT_PSF_C_H
#define SHADPS4_FILE_FORMAT_PSF_C_H

/* The game's param.sfo (Common::Singleton<PSF>), for code written in C. Defined in psf.cpp. */

#include <stddef.h>

#include <boolean.h>
#include <retro_common_api.h>

#include "common/types.h"

RETRO_BEGIN_DECLS

/* The string @key: false if there is none. Its length is put in @length (if not NULL), and the
 * string, cut to fit and terminated, in @out (if @size is not 0). */
bool psf_game_get_string(const char* key, char* out, size_t size, size_t* length);

/* The integer @key: false if there is none */
bool psf_game_get_integer(const char* key, s32* value);

RETRO_END_DECLS

#endif
