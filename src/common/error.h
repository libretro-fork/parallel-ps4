// SPDX-FileCopyrightText: 2013 Dolphin Emulator Project
// SPDX-FileCopyrightText: 2014 Citra Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

namespace Common {

/// The last system error, in words: in a buffer of the calling thread's, good until its next
/// call
[[nodiscard]] const char* GetLastErrorMsg();

} // namespace Common
