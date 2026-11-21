/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef SHADPS4_LIBRARIES_PNGENC_ERROR_H
#define SHADPS4_LIBRARIES_PNGENC_ERROR_H

#include "core/libraries/error_codes.h"

/* PngEnc library */
#define ORBIS_PNG_ENC_ERROR_INVALID_ADDR ((int32_t)0x80690101)
#define ORBIS_PNG_ENC_ERROR_INVALID_SIZE ((int32_t)0x80690102)
#define ORBIS_PNG_ENC_ERROR_INVALID_PARAM ((int32_t)0x80690103)
#define ORBIS_PNG_ENC_ERROR_INVALID_HANDLE ((int32_t)0x80690104)
#define ORBIS_PNG_ENC_ERROR_DATA_OVERFLOW ((int32_t)0x80690110)
#define ORBIS_PNG_ENC_ERROR_FATAL ((int32_t)0x80690120)

#endif
