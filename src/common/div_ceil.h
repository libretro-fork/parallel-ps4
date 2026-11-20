// SPDX-FileCopyrightText: Copyright 2020 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "common/concepts.h"

namespace Common {

/// Ceiled integer division.
template <typename N, typename D>
    requires IsIntegral<N> && IsUnsigned<D>
[[nodiscard]] constexpr N DivCeil(N number, D divisor) {
    return static_cast<N>((static_cast<D>(number) + divisor - 1) / divisor);
}

/// Ceiled integer division with logarithmic divisor in base 2
template <typename N, typename D>
    requires IsIntegral<N> && IsUnsigned<D>
[[nodiscard]] constexpr N DivCeilLog2(N value, D alignment_log2) {
    return static_cast<N>((static_cast<D>(value) + (D(1) << alignment_log2) - 1) >> alignment_log2);
}

} // namespace Common
