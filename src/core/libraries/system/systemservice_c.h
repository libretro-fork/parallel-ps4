/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef SHADPS4_LIBRARIES_SYSTEMSERVICE_C_H
#define SHADPS4_LIBRARIES_SYSTEMSERVICE_C_H

/* The system service's calls the libraries written in C make. Defined in systemservice.cpp. */

#include <retro_common_api.h>

#include "common/types.h"

RETRO_BEGIN_DECLS

/* OrbisSystemServiceParamId */
#define SYSTEM_SERVICE_PARAM_ID_LANG 1

/* sceSystemServiceParamGetInt */
s32 system_service_param_get_int(s32 param_id, s32* value);

/* An entitlement update for user 0 and service label 0, pushed as the system service's event */
void system_service_push_entitlement_update(void);

RETRO_END_DECLS

#endif
