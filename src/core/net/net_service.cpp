// SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <net/net_compat.h>
#include <net/net_http.h>

#include <retro_atomic.h>
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
    retro_atomic_int_t done{0};
};
} // namespace

NetService& NetService::Instance() {
    static NetService instance;
    return instance;
}

NetService::NetService() {
    for (u32 i = 0; i < Slots; ++i) {
        retro_atomic_store_relaxed_int(&m_cells[i].seq, i);
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
    retro_atomic_store_release_int(&m_stop, 1);
    retro_eventcount_notify(&m_work);
    m_thread.join();
    retro_eventcount_free(&m_work);
    retro_eventcount_free(&m_done);
}

bool NetService::Post(Job job, void* user) {
    if (!m_ok) {
        return false;
    }
    u32 pos = static_cast<u32>(retro_atomic_load_relaxed_int(&m_tail));
    Cell* cell;
    for (;;) {
        cell = &m_cells[pos % Slots];
        const u32 seq = static_cast<u32>(retro_atomic_load_acquire_int(&cell->seq));
        const s32 diff = static_cast<s32>(seq - pos);
        if (diff == 0) {
            if ((retro_atomic_cas_int(&m_tail, pos, pos + 1) ||
                 ((pos = static_cast<u32>(retro_atomic_load_acquire_int(&m_tail))), false))) {
                break;
            }
        } else if (diff < 0) {
            return false;
        } else {
            pos = static_cast<u32>(retro_atomic_load_relaxed_int(&m_tail));
        }
    }
    cell->job = job;
    cell->user = user;
    retro_atomic_store_release_int(&cell->seq, pos + 1);
    retro_eventcount_notify(&m_work);
    return true;
}

bool NetService::Run(Job job, void* user) {
    Waited waited{job, user};
    const auto trampoline = [](void* p) {
        auto* w = static_cast<Waited*>(p);
        w->job(w->user);
        retro_atomic_store_release_int(&w->done, 1);
    };
    if (!Post(trampoline, &waited)) {
        return false;
    }
    WaitFor(&m_done, [&] { return (retro_atomic_load_acquire_int(&waited.done) != 0); });
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
            if (static_cast<u32>(retro_atomic_load_acquire_int(&next.seq)) == m_head + 1) {
                cell = &next;
                return true;
            }
            return (retro_atomic_load_acquire_int(&m_stop) != 0);
        });
        if (!cell) {
            break;
        }
        const Job job = cell->job;
        void* user = cell->user;
        retro_atomic_store_release_int(&cell->seq, m_head + Slots);
        ++m_head;
        job(user);
        retro_eventcount_notify(&m_done);
    }
    net_http_deinit();
}

} // namespace Core::Net
