// SPDX-FileCopyrightText: Copyright 2020 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <stddef.h>

namespace Common {

/// Whether T is a contiguous container: one with an iterator type, data() and size()
template <typename T>
concept IsContiguousContainer = requires(T& t) {
    typename T::iterator;
    t.data();
    t.size();
};

/// T with const and volatile taken off
template <typename T>
struct RemoveCV {
    using Type = T;
};
template <typename T>
struct RemoveCV<const T> {
    using Type = T;
};
template <typename T>
struct RemoveCV<volatile T> {
    using Type = T;
};
template <typename T>
struct RemoveCV<const volatile T> {
    using Type = T;
};

/// T with a reference taken off
template <typename T>
struct RemoveReference {
    using Type = T;
};
template <typename T>
struct RemoveReference<T&> {
    using Type = T;
};
template <typename T>
struct RemoveReference<T&&> {
    using Type = T;
};

template <typename A, typename B>
inline constexpr bool IsSame = false;
template <typename A>
inline constexpr bool IsSame<A, A> = true;

template <typename T>
inline constexpr bool IsIntegralBase = false;
template <>
inline constexpr bool IsIntegralBase<bool> = true;
template <>
inline constexpr bool IsIntegralBase<char> = true;
template <>
inline constexpr bool IsIntegralBase<signed char> = true;
template <>
inline constexpr bool IsIntegralBase<unsigned char> = true;
template <>
inline constexpr bool IsIntegralBase<wchar_t> = true;
template <>
inline constexpr bool IsIntegralBase<char8_t> = true;
template <>
inline constexpr bool IsIntegralBase<char16_t> = true;
template <>
inline constexpr bool IsIntegralBase<char32_t> = true;
template <>
inline constexpr bool IsIntegralBase<short> = true;
template <>
inline constexpr bool IsIntegralBase<unsigned short> = true;
template <>
inline constexpr bool IsIntegralBase<int> = true;
template <>
inline constexpr bool IsIntegralBase<unsigned int> = true;
template <>
inline constexpr bool IsIntegralBase<long> = true;
template <>
inline constexpr bool IsIntegralBase<unsigned long> = true;
template <>
inline constexpr bool IsIntegralBase<long long> = true;
template <>
inline constexpr bool IsIntegralBase<unsigned long long> = true;
#ifdef __SIZEOF_INT128__
template <>
inline constexpr bool IsIntegralBase<__int128> = true;
template <>
inline constexpr bool IsIntegralBase<unsigned __int128> = true;
#endif

/// Whether T is an integer type (bool and the character types too), as std::is_integral_v
template <typename T>
inline constexpr bool IsIntegral = IsIntegralBase<typename RemoveCV<T>::Type>;

/// Whether T is an unsigned integer type, as std::is_unsigned_v is for integers
template <typename T>
inline constexpr bool IsUnsigned = [] {
    if constexpr (IsIntegral<T>) {
        return T(0) < T(-1);
    } else {
        return false;
    }
}();

template <typename T>
inline constexpr bool IsReference = false;
template <typename T>
inline constexpr bool IsReference<T&> = true;
template <typename T>
inline constexpr bool IsReference<T&&> = true;

/// Whether T is a signed integer type, as std::is_signed_v is for integers
template <typename T>
inline constexpr bool IsSigned = [] {
    if constexpr (IsIntegral<T>) {
        return T(-1) < T(0);
    } else {
        return false;
    }
}();

/// T's underlying type for an enumeration, T itself otherwise
template <typename T, bool = __is_enum(T)>
struct UnderlyingOf {
    using Type = T;
};
template <typename T>
struct UnderlyingOf<T, true> {
    using Type = __underlying_type(T);
};

/// The unsigned integer type as wide as T
template <size_t Size>
struct UnsignedOfSize;
template <>
struct UnsignedOfSize<1> {
    using Type = unsigned char;
};
template <>
struct UnsignedOfSize<2> {
    using Type = unsigned short;
};
template <>
struct UnsignedOfSize<4> {
    using Type = unsigned int;
};
template <>
struct UnsignedOfSize<8> {
    using Type = unsigned long long;
};
template <typename T>
using MakeUnsigned = typename UnsignedOfSize<sizeof(T)>::Type;

/// The type at @I of Args
template <size_t I, typename... Args>
struct TypeAt;
template <typename First, typename... Rest>
struct TypeAt<0, First, Rest...> {
    using Type = First;
};
template <size_t I, typename First, typename... Rest>
struct TypeAt<I, First, Rest...> {
    using Type = typename TypeAt<I - 1, Rest...>::Type;
};

/// @value with its bytes the other way round
template <typename T>
    requires IsIntegral<T>
[[nodiscard]] constexpr T ByteSwap(T value) {
    if constexpr (sizeof(T) == 1) {
        return value;
    } else if constexpr (sizeof(T) == 2) {
        return static_cast<T>(__builtin_bswap16(static_cast<unsigned short>(value)));
    } else if constexpr (sizeof(T) == 4) {
        return static_cast<T>(__builtin_bswap32(static_cast<unsigned int>(value)));
    } else {
        static_assert(sizeof(T) == 8);
        return static_cast<T>(__builtin_bswap64(static_cast<unsigned long long>(value)));
    }
}

/// The bits of @from read as a To of the same size
template <typename To, typename From>
[[nodiscard]] constexpr To BitCast(const From& from) {
    static_assert(sizeof(To) == sizeof(From));
    return __builtin_bit_cast(To, from);
}

} // namespace Common
