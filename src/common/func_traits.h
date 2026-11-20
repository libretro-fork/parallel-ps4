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

} // namespace Common
