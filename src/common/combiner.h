// SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <new>

#include "common/combine.h"

#include "common/concepts.h"

namespace Common {

/// combiner_t (common/combine.h) for C++ callers that hand it a callable
class Combiner {
public:
    Combiner() {
        combiner_init(&c);
    }
    ~Combiner() {
        combiner_free(&c);
    }
    Combiner(const Combiner&) = delete;
    Combiner& operator=(const Combiner&) = delete;

    /// Runs @func with no other function of this Combiner running, and returns what it returns.
    template <typename F>
    decltype((*static_cast<F*>(nullptr))()) Run(F&& func) {
        using R = decltype((*static_cast<F*>(nullptr))());
        using Func = typename RemoveReference<F>::Type;
        if constexpr (IsSame<R, void>) {
            combiner_run(&c, &CallVoid<Func>,
                         const_cast<void*>(static_cast<const void*>(__builtin_addressof(func))));
        } else if constexpr (IsReference<R>) {
            ResultRef<Func, R> call{__builtin_addressof(func), nullptr};
            combiner_run(&c, &CallRef<Func, R>, &call);
            return static_cast<R>(*call.result);
        } else {
            // Made in place by the function's run, moved out and ended here
            alignas(R) unsigned char storage[sizeof(R)];
            ResultValue<Func> call{__builtin_addressof(func), storage};
            combiner_run(&c, &CallValue<Func, R>, &call);
            R* made = __builtin_launder(reinterpret_cast<R*>(storage));
            R result(static_cast<R&&>(*made));
            made->~R();
            return result;
        }
    }

private:
    template <typename Func, typename R>
    struct ResultRef {
        Func* func;
        typename RemoveReference<R>::Type* result;
    };
    template <typename Func>
    struct ResultValue {
        Func* func;
        unsigned char* storage;
    };

    template <typename Func>
    static void CallVoid(void* f) {
        (*static_cast<Func*>(f))();
    }
    template <typename Func, typename R>
    static void CallRef(void* p) {
        ResultRef<Func, R>* call = static_cast<ResultRef<Func, R>*>(p);
        call->result = __builtin_addressof((*call->func)());
    }
    template <typename Func, typename R>
    static void CallValue(void* p) {
        ResultValue<Func>* call = static_cast<ResultValue<Func>*>(p);
        new (call->storage) R((*call->func)());
    }

    combiner_t c;
};

} // namespace Common
