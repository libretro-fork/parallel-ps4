/* SPDX-FileCopyrightText: Copyright 2024-2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef SHADPS4_NET_CTL_OBJ_H
#define SHADPS4_NET_CTL_OBJ_H

/* libSceNetCtl's callbacks: the game's, and the NP toolkit's, eight of each.
 * Lock-free: a slot is claimed with a compare-and-swap and its function
 * published after its argument, so a caller of net_ctl_notify sees a whole
 * slot or none. */

#include <retro_atomic.h>
#include <retro_common_api.h>

#include "common/types.h"

RETRO_BEGIN_DECLS

#define NET_CTL_CALLBACK_MAX 8

typedef void(PS4_SYSV_ABI* OrbisNetCtlCallback)(int eventType, void* arg);
typedef void(PS4_SYSV_ABI* OrbisNetCtlCallbackForNpToolkit)(int eventType, void* arg);

enum net_ctl_callbacks {
    NET_CTL_CALLBACKS_GAME = 0,  /* sceNetCtlRegisterCallback */
    NET_CTL_CALLBACKS_NP_TOOLKIT /* sceNetCtlRegisterCallbackForNpToolkit */
};

/* Keeps @func with @arg in the first free slot of @which (enum
 * net_ctl_callbacks): its id, or ORBIS_NET_CTL_ERROR_CALLBACK_MAX when all
 * eight are taken */
int net_ctl_register(int which, OrbisNetCtlCallback func, void* arg);

/* Calls every callback of @which with the event the network setting gives:
 * IP obtained when connected, disconnected when not */
void net_ctl_notify(int which);

RETRO_END_DECLS

#endif
