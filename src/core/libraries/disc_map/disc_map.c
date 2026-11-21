/* SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "core/libraries/disc_map/disc_map.h"
#include "core/libraries/disc_map/disc_map_codes.h"
#include "core/libraries/error_codes.h"

int PS4_SYSV_ABI sceDiscMapGetPackageSize(s64 fflags, int* ret1, int* ret2) {
    return ORBIS_DISC_MAP_ERROR_NO_BITMAP_INFO;
}

int PS4_SYSV_ABI sceDiscMapIsRequestOnHDD(char* path, s64 offset, s64 nbytes, int* ret) {
    return ORBIS_DISC_MAP_ERROR_NO_BITMAP_INFO;
}

int PS4_SYSV_ABI DiscMap_Func_7C980FFB0AA27E7A(char* path, s64 offset, s64 nbytes, int* flags,
                                               int* ret1, int* ret2) {
    *flags = 0;
    *ret1 = 0;
    *ret2 = 0;
    return ORBIS_OK;
}

int PS4_SYSV_ABI DiscMap_Func_8A828CAEE7EDD5E9(char* path, s64 offset, s64 nbytes, int* flags,
                                               int* ret1, int* ret2) {
    return ORBIS_DISC_MAP_ERROR_NO_BITMAP_INFO;
}

int PS4_SYSV_ABI DiscMap_Func_E7EBCE96E92F91F8(void) {
    return ORBIS_DISC_MAP_ERROR_NO_BITMAP_INFO;
}
