/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef SHADPS4_NP_INVITATION_JSON_H
#define SHADPS4_NP_INVITATION_JSON_H

#include <stddef.h>

#include <retro_common_api.h>

RETRO_BEGIN_DECLS

/* Writes the shadNet invitation-request body,
 *   {"to":["npid",...],"message":"..."}
 * into s (the message is left out when NULL or empty). Like snprintf it
 * returns the length the whole body needs, writing at most len - 1 octets
 * and a terminator; 0 means the body could not be made. */
size_t np_invitation_request_json(char* s, size_t len, const char* const* to, size_t count,
                                  const char* message);

RETRO_END_DECLS

#endif
