/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef SHADPS4_LIBRARIES_NP_HANDLER_C_H
#define SHADPS4_LIBRARIES_NP_HANDLER_C_H

/* The NP handler's calls the libraries written in C make. Defined in np_handler_invitation.cpp. */

#include <stddef.h>

#include <retro_common_api.h>

#include "common/types.h"
#include "video_core/overlay/ovl_layers.h"

RETRO_BEGIN_DECLS

/* The invitations waiting for @user_id that have not expired (NpHandler::GetPendingInvitations),
 * the first OVL_INVDLG_INVITES of them lent to @use for as long as it runs: @count of them, of
 * @pending in all. */
void np_lend_pending_invitations(s32 user_id,
                                 void (*use)(const ovl_invdlg_invite_t* invites, int count,
                                             size_t pending, void* ctx),
                                 void* ctx);

/* Tells NP a user logged in or out (@event is an OrbisUserServiceEventType), as the game takes
 * the event. Defined in np_manager.cpp. */
void np_notify_user_service_event(s32 event, s32 user_id);

/* @user_id's online ID when signed in to PSN, copied to @out with its terminator: its length,
 * or 0 when not signed in, there is none or it does not fit. Defined in np_manager.cpp. */
size_t np_signed_in_online_id(s32 user_id, char* out, size_t size);

RETRO_END_DECLS

#endif
