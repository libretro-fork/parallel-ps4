// SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <net/net_compat.h>
#include <net/net_http.h>

#include "common/logging/log.h"
#include "common/thread.h"
#include "core/net/net_service.h"

namespace Core::Net {

namespace {
template <class F>
void WaitFor(retro_eventcount_t* ec, F&& ready) {
    while (!ready()) {
        const int key = retro_eventcount_prepare_wait(ec);
        if (ready()) {
            retro_eventcount_cancel_wait(ec);
            return;
        }
        retro_eventcount_commit_wait(ec, key);
    }
}

struct Waited {
    NetService::Job job;
    void* user;
    std::atomic<bool> done{false};
};
} // namespace

NetService& NetService::Instance() {
    static NetService instance;
    return instance;
}

NetService::NetService() {
    for (u32 i = 0; i < Slots; ++i) {
        m_cells[i].seq.store(i, std::memory_order_relaxed);
    }
    const bool work = retro_eventcount_init(&m_work);
    const bool done = retro_eventcount_init(&m_done);
    m_ok = work && done;
    if (!m_ok) {
        LOG_ERROR(Lib_Net, "No eventcount: network requests cannot run");
        if (work) {
            retro_eventcount_free(&m_work);
        }
        if (done) {
            retro_eventcount_free(&m_done);
        }
        return;
    }
    m_thread = std::thread([this] { Loop(); });
}

NetService::~NetService() {
    if (!m_ok) {
        return;
    }
    m_stop.store(true, std::memory_order_release);
    retro_eventcount_notify(&m_work);
    m_thread.join();
    retro_eventcount_free(&m_work);
    retro_eventcount_free(&m_done);
}

bool NetService::Post(Job job, void* user) {
    if (!m_ok) {
        return false;
    }
    u32 pos = m_tail.load(std::memory_order_relaxed);
    Cell* cell;
    for (;;) {
        cell = &m_cells[pos % Slots];
        const u32 seq = cell->seq.load(std::memory_order_acquire);
        const s32 diff = static_cast<s32>(seq - pos);
        if (diff == 0) {
            if (m_tail.compare_exchange_weak(pos, pos + 1, std::memory_order_relaxed)) {
                break;
            }
        } else if (diff < 0) {
            return false;
        } else {
            pos = m_tail.load(std::memory_order_relaxed);
        }
    }
    cell->job = job;
    cell->user = user;
    cell->seq.store(pos + 1, std::memory_order_release);
    retro_eventcount_notify(&m_work);
    return true;
}

bool NetService::Run(Job job, void* user) {
    Waited waited{job, user};
    const auto trampoline = [](void* p) {
        auto* w = static_cast<Waited*>(p);
        w->job(w->user);
        w->done.store(true, std::memory_order_release);
    };
    if (!Post(trampoline, &waited)) {
        return false;
    }
    WaitFor(&m_done, [&] { return waited.done.load(std::memory_order_acquire); });
    return true;
}

void NetService::Loop() {
    Common::SetCurrentThreadName("shadPS4:NetService");
    const bool network = network_init();
    if (!network) {
        LOG_WARNING(Lib_Net, "No network: network requests will fail");
    }
    net_http_init();
    for (;;) {
        Cell* cell = nullptr;
        WaitFor(&m_work, [&] {
            Cell& next = m_cells[m_head % Slots];
            if (next.seq.load(std::memory_order_acquire) == m_head + 1) {
                cell = &next;
                return true;
            }
            return m_stop.load(std::memory_order_acquire);
        });
        if (!cell) {
            break;
        }
        const Job job = cell->job;
        void* user = cell->user;
        cell->seq.store(m_head + Slots, std::memory_order_release);
        ++m_head;
        job(user);
        retro_eventcount_notify(&m_done);
    }
    net_http_deinit();
}

} // namespace Core::Net
