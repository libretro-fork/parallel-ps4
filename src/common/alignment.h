// SPDX-FileCopyrightText: 2014 Jannik Vogel <email@jannikvogel.de>
// SPDX-License-Identifier: CC0-1.0

#pragma once

#include <stddef.h>

#include "common/concepts.h"
#include "common/types.h"

namespace Common {

/// The larger of @a and @b
template <typename T>
[[nodiscard]] constexpr T Max(T a, T b) {
    return a < b ? b : a;
}

/// The smaller of @a and @b
template <typename T>
[[nodiscard]] constexpr T Min(T a, T b) {
    return b < a ? b : a;
}

/// The number of bits @value takes, 0 for 0 (std::bit_width)
/// The set bits of @value (std::popcount)
[[nodiscard]] constexpr u32 PopCount(unsigned long long value) {
    return static_cast<u32>(__builtin_popcountll(value));
}

[[nodiscard]] constexpr u32 BitWidth(unsigned long long value) {
    return value ? 64u - static_cast<u32>(__builtin_clzll(value)) : 0u;
}

/// The smallest power of two not below @value, 1 for 0 (std::bit_ceil)
[[nodiscard]] constexpr u32 BitCeil(u32 value) {
    return value <= 1 ? 1u : 1u << BitWidth(value - 1);
}

/// Whether @value is a power of two (std::has_single_bit)
[[nodiscard]] constexpr bool HasSingleBit(unsigned long long value) {
    return value != 0 && (value & (value - 1)) == 0;
}

template <typename T>
[[nodiscard]] constexpr T AlignUp(T value, size_t size) {
    static_assert(IsUnsigned<T>, "T must be an unsigned value.");
    const T mod{static_cast<T>(value % size)};
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
