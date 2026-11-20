// SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <stddef.h>

namespace Common {

extern const char g_version[];
extern const bool g_is_release;

extern const char g_scm_rev[];
extern const char g_scm_branch[];
extern const char g_scm_desc[];
extern const char g_scm_remote_name[];
extern const char g_scm_remote_url[];
extern const char g_scm_date[];

/// The owner in g_scm_remote_url (github.com/<owner>/... or git@host:<owner>/...), into @out;
/// "unknown" when the URL is neither form
void GetRemoteNameFromLink(char* out, size_t size);

} // namespace Common
