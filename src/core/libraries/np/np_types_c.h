/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef SHADPS4_LIBRARIES_NP_TYPES_C_H
#define SHADPS4_LIBRARIES_NP_TYPES_C_H

/* The NP structures the libraries written in C share, as np_types.h has them for C++ (which
 * checks they match). */

#include "common/types.h"

/* Libraries::Np::OrbisNpAccountId */
typedef u64 np_account_id_t;

/* ORBIS_NP_ONLINEID_MAX_LENGTH */
#define NP_ONLINEID_MAX_LENGTH 16

/* Libraries::Np::OrbisNpOnlineId */
typedef struct np_online_id {
    char data[NP_ONLINEID_MAX_LENGTH];
    s8 term;
    s8 dummy[3];
} np_online_id_t;

typedef char np_online_id_size_check[sizeof(np_online_id_t) == 20 ? 1 : -1];

#endif
