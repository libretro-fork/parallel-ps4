// SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <stddef.h>

#include "common/concepts.h"

namespace Common {

template <class Func>
struct FuncTraits {};

template <class ReturnType_, class... Args>
struct FuncTraits<ReturnType_ (*)(Args...)> {
    using ReturnType = ReturnType_;

    static constexpr size_t NUM_ARGS = sizeof...(Args);

    template <size_t I>
    using ArgType = typename TypeAt<I, Args...>::Type;
};

template <typename Func>
struct LambdaTraits : LambdaTraits<decltype(&RemoveReference<Func>::Type::operator())> {};

template <typename ReturnType, typename LambdaType, typename... Args>
struct LambdaTraits<ReturnType (LambdaType::*)(Args...) const> {
    template <size_t I>
    using ArgType = typename TypeAt<I, Args...>::Type;

    static constexpr size_t NUM_ARGS{sizeof...(Args)};
};

/// The indices 0 to N - 1 as a pack (what std::index_sequence was)
template <size_t... I>
struct IndexSequence {};

template <size_t N, size_t... I>
struct MakeIndexSequenceT : MakeIndexSequenceT<N - 1, N - 1, I...> {};

template <size_t... I>
struct MakeIndexSequenceT<0, I...> {
    using Type = IndexSequence<I...>;
};

template <size_t N>
using MakeIndexSequence = typename MakeIndexSequenceT<N>::Type;

} // namespace Common
