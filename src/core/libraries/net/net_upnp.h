// SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <atomic>

#include <net/net_upnp.h>

#include "common/types.h"

namespace Libraries::Net {

// Forwards the P2P port through the router with UPnP, on the network service thread (the one
// that drives net_http): callers only post what they want forwarded and never wait.
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

    // Jobs on the service thread, which alone touches the fields below the atomics
    static void Discover(void* self);
    static void Sync(void* self);
    static void Shutdown(void* self);

    std::atomic<bool> m_started{false};
    std::atomic<bool> m_p2p_features_enabled{true};
    std::atomic<u32> m_external_ip_nbo{0};
    // The port wanted forwarded, 0 for none
    std::atomic<u16> m_wanted_port{0};
    net_upnp_gateway m_gateway{};
    bool m_have_gateway = false;
    u16 m_mapped = 0;
};

} // namespace Libraries::Net
