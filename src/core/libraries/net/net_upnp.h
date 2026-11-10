// SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <atomic>
#include <thread>

#include <net/net_upnp.h>
#include <rthreads/retro_eventcount.h>

#include "common/types.h"

namespace Libraries::Net {

// Forwards the P2P port through the router with UPnP, on a thread of its own: callers only post
// what they want forwarded and never wait for the router.
class UPnPClient {
public:
    static UPnPClient& Instance();

    // Starts finding the router. Only the first call does anything.
    void Start();
    void SetP2PFeaturesEnabled(bool enabled);

    // External IP in network byte order as reported by the router. 0 if not (yet) known.
    u32 GetExternalIp() const;

    // Forwards `port` once the router is found, replacing an earlier request's mapping.
    void MapPortAsync(u16 port);
    // Removes what MapPortAsync forwarded, if it is `port`.
    void UnmapPort(u16 port);

private:
    UPnPClient();
    ~UPnPClient();
    UPnPClient(const UPnPClient&) = delete;
    UPnPClient& operator=(const UPnPClient&) = delete;

    void Run();

    std::atomic<bool> m_started{false};
    std::atomic<bool> m_p2p_features_enabled{true};
    std::atomic<bool> m_quit{false};
    std::atomic<u32> m_external_ip_nbo{0};
    // The port wanted forwarded, 0 for none. The thread alone talks to the router.
    std::atomic<u16> m_wanted_port{0};
    retro_eventcount_t m_wake{};
    bool m_wake_ok = false;
    std::thread m_thread;
};

} // namespace Libraries::Net
