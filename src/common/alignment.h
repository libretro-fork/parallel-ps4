// SPDX-FileCopyrightText: 2014 Jannik Vogel <email@jannikvogel.de>
// SPDX-License-Identifier: CC0-1.0

#pragma once

#include <stddef.h>

#include "common/concepts.h"

namespace Common {

template <typename T>
[[nodiscard]] constexpr T AlignUp(T value, size_t size) {
    static_assert(IsUnsigned<T>, "T must be an unsigned value.");
    auto mod{static_cast<T>(value % size)};
    value -= mod;
    return static_cast<T>(mod == T{0} ? value : value + size);
}

template <typename T>
[[nodiscard]] constexpr T AlignDown(T value, size_t size) {
    static_assert(IsUnsigned<T>, "T must be an unsigned value.");
    return static_cast<T>(value - value % size);
}

template <typename T>
[[nodiscard]] constexpr T AlignUpPow2(T value, size_t size) {
    return static_cast<T>(value + (size - 1) & ~(size - 1));
}

template <typename T>
[[nodiscard]] constexpr T AlignDownPow2(T value, size_t size) {
    return static_cast<T>(value & ~(size - 1));
}

template <typename T>
    requires IsIntegral<T>
[[nodiscard]] constexpr bool IsAligned(T value, size_t alignment) {
    return (value & (alignment - 1)) == 0;
}

template <typename T>
    requires IsIntegral<T>
[[nodiscard]] constexpr bool Is16KBAligned(T value) {
    return (value & 0x3FFF) == 0;
}

template <typename T>
    requires IsIntegral<T>
[[nodiscard]] constexpr bool Is64KBAligned(T value) {
    return (value & 0xFFFF) == 0;
}

template <typename T>
    requires IsIntegral<T>
[[nodiscard]] constexpr bool Is2MBAligned(T value) {
    return (value & 0x1FFFFF) == 0;
}

} // namespace Common
