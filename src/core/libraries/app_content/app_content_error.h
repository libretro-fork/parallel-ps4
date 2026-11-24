/* SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef SHADPS4_LIBRARIES_APP_CONTENT_ERROR_H
#define SHADPS4_LIBRARIES_APP_CONTENT_ERROR_H

#include <stdint.h>

#include "core/libraries/error_codes.h"

/* AppContent library */
#define ORBIS_APP_CONTENT_ERROR_NOT_INITIALIZED ((int32_t)0x80D90001)
#define ORBIS_APP_CONTENT_ERROR_PARAMETER ((int32_t)0x80D90002)
#define ORBIS_APP_CONTENT_ERROR_BUSY ((int32_t)0x80D90003)
#define ORBIS_APP_CONTENT_ERROR_DRM_NO_ENTITLEMENT ((int32_t)0x80D90007)
#define ORBIS_APP_CONTENT_ERROR_NOT_FOUND ((int32_t)0x80D90005)

#endif
