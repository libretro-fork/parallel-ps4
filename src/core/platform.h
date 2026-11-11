// SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "common/assert.h"
#include "common/logging/log.h"
#include "common/singleton.h"
#include "common/types.h"

#include "common/enum_names.h"

#include "common/combiner.h"

#include <array>
#include <functional>
#include <unordered_map>
#include <queue>

namespace Platform {

enum class InterruptId : u32 {
    Compute0RelMem = 0x00,
    Compute1RelMem = 0x01,
    Compute2RelMem = 0x02,
    Compute3RelMem = 0x03,
    Compute4RelMem = 0x04,
    Compute5RelMem = 0x05,
    Compute6RelMem = 0x06,
    GfxEop = 0x40,
    GfxFlip = 0x08,
    GpuIdle = 0x09,

    InterruptIdMax = 0x40, ///< Max possible value (GfxEop)
};

using IrqHandler = std::function<void(InterruptId)>;

/// The handlers of each interrupt, registered, unregistered and run one at a time in its own
/// Common::Combiner: no lock, and a signal on one interrupt never waits on another's.
struct IrqController {
    void RegisterOnce(InterruptId irq, IrqHandler handler) {
        auto& ctx = Context(irq);
        ctx.combiner.Run([&] { ctx.one_time_subscribers.emplace(std::move(handler)); });
    }

    void Register(InterruptId irq, IrqHandler handler, void* uid) {
        auto& ctx = Context(irq);
        ctx.combiner.Run([&] {
            ASSERT_MSG(ctx.persistent_handlers.find(uid) == ctx.persistent_handlers.cend(),
                       "The handler is already registered!");
            ctx.persistent_handlers.emplace(uid, std::move(handler));
        });
    }

    void Unregister(InterruptId irq, void* uid) {
        auto& ctx = Context(irq);
        ctx.combiner.Run([&] { ctx.persistent_handlers.erase(uid); });
    }

    void Signal(InterruptId irq) {
        auto& ctx = Context(irq);
        ctx.combiner.Run([&] {
            LOG_TRACE(Core, "IRQ signaled: {}",
                      enum_name(&enum_names_Platform_InterruptId, static_cast<s64>(irq)));

            for (auto& [uid, h] : ctx.persistent_handlers) {
                h(irq);
            }

            if (!ctx.one_time_subscribers.empty()) {
                const auto& h = ctx.one_time_subscribers.front();
                h(irq);

                ctx.one_time_subscribers.pop();
            }
        });
    }

private:
    struct IrqContext {
        std::unordered_map<void*, IrqHandler> persistent_handlers{};
        std::queue<IrqHandler> one_time_subscribers{};
        Common::Combiner combiner;
    };

    IrqContext& Context(InterruptId irq) {
        ASSERT_MSG(static_cast<u32>(irq) <= static_cast<u32>(InterruptId::InterruptIdMax),
                   "Invalid IRQ number");
        return irq_contexts[static_cast<u32>(irq)];
    }

    // One per interrupt number, made up front: none is added while another is signalled
    std::array<IrqContext, static_cast<u32>(InterruptId::InterruptIdMax) + 1> irq_contexts{};
};

using IrqC = Common::Singleton<IrqController>;

} // namespace Platform
