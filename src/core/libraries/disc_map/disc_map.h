/* SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef SHADPS4_LIBRARIES_DISC_MAP_H
#define SHADPS4_LIBRARIES_DISC_MAP_H

/* libSceDiscMap. Registered in libs.cpp. */

#include <retro_common_api.h>

#include "common/types.h"

RETRO_BEGIN_DECLS

int PS4_SYSV_ABI sceDiscMapGetPackageSize(s64 fflags, int* ret1, int* ret2);
int PS4_SYSV_ABI sceDiscMapIsRequestOnHDD(char* path, s64 offset, s64 nbytes, int* ret);
int PS4_SYSV_ABI DiscMap_Func_7C980FFB0AA27E7A(char* path, s64 offset, s64 nbytes, int* flags,
                                               int* ret1, int* ret2);
int PS4_SYSV_ABI DiscMap_Func_8A828CAEE7EDD5E9(char* path, s64 offset, s64 nbytes, int* flags,
                                               int* ret1, int* ret2);
int PS4_SYSV_ABI DiscMap_Func_E7EBCE96E92F91F8(void);

RETRO_END_DECLS

#endif
