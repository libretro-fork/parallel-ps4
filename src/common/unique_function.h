// SPDX-FileCopyrightText: Copyright 2021 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

namespace Common {

/// General purpose function wrapper similar to std::function.
/// Unlike std::function, the captured values don't have to be copyable.
/// This class can be moved but not copied.
template <typename ResultType, typename... Args>
class UniqueFunction {
    class CallableBase {
    public:
        virtual ~CallableBase() = default;
        virtual ResultType operator()(Args&&...) = 0;
    };

    template <typename Functor>
    class Callable final : public CallableBase {
    public:
        Callable(Functor&& functor_) : functor{static_cast<Functor&&>(functor_)} {}
        ~Callable() override = default;

        ResultType operator()(Args&&... args) override {
            return functor(static_cast<Args&&>(args)...);
        }

    private:
        Functor functor;
    };

public:
    UniqueFunction() = default;

    template <typename Functor>
    UniqueFunction(Functor&& functor)
        : callable{new Callable<Functor>(static_cast<Functor&&>(functor))} {}

    UniqueFunction& operator=(UniqueFunction&& rhs) noexcept {
        if (this != &rhs) {
            delete callable;
            callable = rhs.callable;
            rhs.callable = nullptr;
        }
        return *this;
    }
    UniqueFunction(UniqueFunction&& rhs) noexcept : callable{rhs.callable} {
        rhs.callable = nullptr;
    }
    ~UniqueFunction() {
        delete callable;
    }

    UniqueFunction& operator=(const UniqueFunction&) = delete;
    UniqueFunction(const UniqueFunction&) = delete;

    ResultType operator()(Args&&... args) const {
        return (*callable)(static_cast<Args&&>(args)...);
    }

    explicit operator bool() const noexcept {
        return callable != nullptr;
    }

private:
    CallableBase* callable = nullptr;
};

} // namespace Common
