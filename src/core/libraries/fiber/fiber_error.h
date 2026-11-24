/* SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef SHADPS4_FIBER_ERROR_H
#define SHADPS4_FIBER_ERROR_H

#include "core/libraries/error_codes.h"

/* Fiber library */
#define ORBIS_FIBER_ERROR_NULL ((int)0x80590001)
#define ORBIS_FIBER_ERROR_ALIGNMENT ((int)0x80590002)
#define ORBIS_FIBER_ERROR_RANGE ((int)0x80590003)
#define ORBIS_FIBER_ERROR_INVALID ((int)0x80590004)
#define ORBIS_FIBER_ERROR_PERMISSION ((int)0x80590005)
#define ORBIS_FIBER_ERROR_STATE ((int)0x80590006)

#endif
