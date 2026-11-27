/* SPDX-FileCopyrightText: Copyright 2024-2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <stddef.h>

#include "core/emulator_settings.h"
#include "core/libraries/network/net_ctl_codes.h"
#include "core/libraries/network/net_ctl_obj.h"

/* A slot: free, being filled, or ready to be called */
enum { SLOT_FREE = 0, SLOT_FILLING, SLOT_READY };

typedef struct net_ctl_slot {
    OrbisNetCtlCallback func;
    void* arg;
    retro_atomic_int_t state;
} net_ctl_slot_t;

static net_ctl_slot_t net_ctl_slots[2][NET_CTL_CALLBACK_MAX];

int net_ctl_register(int which, OrbisNetCtlCallback func, void* arg) {
    net_ctl_slot_t* const slots = net_ctl_slots[which != NET_CTL_CALLBACKS_GAME];
    int i;
    for (i = 0; i < NET_CTL_CALLBACK_MAX; i++) {
        if (retro_atomic_cas_int(&slots[i].state, SLOT_FREE, SLOT_FILLING)) {
            slots[i].func = func;
            slots[i].arg = arg;
            retro_atomic_store_release_int(&slots[i].state, SLOT_READY);
            return i;
        }
    }
    return ORBIS_NET_CTL_ERROR_CALLBACK_MAX;
}

void net_ctl_notify(int which) {
    net_ctl_slot_t* const slots = net_ctl_slots[which != NET_CTL_CALLBACKS_GAME];
    const int event = emu_settings_get_connected_to_network()
                          ? ORBIS_NET_CTL_EVENT_TYPE_IPOBTAINED
                          : ORBIS_NET_CTL_EVENT_TYPE_DISCONNECTED;
    int i;
    for (i = 0; i < NET_CTL_CALLBACK_MAX; i++) {
        if (retro_atomic_load_acquire_int(&slots[i].state) == SLOT_READY) {
            slots[i].func(event, slots[i].arg);
        }
    }
}
