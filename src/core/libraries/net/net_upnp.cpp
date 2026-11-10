// SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <net/net_compat.h>
#include <net/net_http.h>

#include "common/logging/log.h"
#include "net_upnp.h"

namespace Libraries::Net {

namespace {
constexpr int DiscoverTimeoutMs = 2000;
constexpr int RequestTimeoutMs = 5000;
constexpr const char* MappingDescription = "shadPS4";
} // namespace

UPnPClient& UPnPClient::Instance() {
    static UPnPClient instance;
    return instance;
}

UPnPClient::UPnPClient() {
    m_wake_ok = retro_eventcount_init(&m_wake);
}

UPnPClient::~UPnPClient() {
    m_quit.store(true);
    if (m_wake_ok) {
        retro_eventcount_notify(&m_wake);
    }
    if (m_thread.joinable()) {
        m_thread.join();
    }
    if (m_wake_ok) {
        retro_eventcount_free(&m_wake);
    }
}

void UPnPClient::Start() {
    if (!m_p2p_features_enabled.load()) {
        LOG_INFO(Lib_Net, "UPNP: skipped because Matching2 is disabled");
        return;
    }
    bool expected = false;
    if (!m_wake_ok || !m_started.compare_exchange_strong(expected, true)) {
        return;
    }
    LOG_INFO(Lib_Net, "UPNP: starting discovery thread");
    m_thread = std::thread(&UPnPClient::Run, this);
}

void UPnPClient::SetP2PFeaturesEnabled(bool enabled) {
    m_p2p_features_enabled.store(enabled);
}

u32 UPnPClient::GetExternalIp() const {
    return m_external_ip_nbo.load();
}

void UPnPClient::MapPortAsync(u16 port) {
    m_wanted_port.store(port);
    if (m_wake_ok) {
        retro_eventcount_notify(&m_wake);
    }
}

void UPnPClient::UnmapPort(u16 port) {
    u16 expected = port;
    if (m_wanted_port.compare_exchange_strong(expected, 0) && m_wake_ok) {
        retro_eventcount_notify(&m_wake);
    }
}

void UPnPClient::Run() {
    net_upnp_gateway gateway;
    char external_ip[64];

    if (!network_init()) {
        LOG_WARNING(Lib_Net, "UPNP: no network");
        return;
    }
    net_http_init();
    if (!net_upnp_discover(&gateway, DiscoverTimeoutMs)) {
        LOG_INFO(Lib_Net, "UPNP: no IGD found");
        net_http_deinit();
        return;
    }
    if (!net_upnp_external_address(&gateway, external_ip, sizeof(external_ip), RequestTimeoutMs)) {
        LOG_WARNING(Lib_Net, "UPNP: IGD found but failed to get external IP");
        net_http_deinit();
        return;
    }
    u32 ip_nbo = 0;
    inet_pton(AF_INET, external_ip, &ip_nbo);
    m_external_ip_nbo.store(ip_nbo);
    LOG_INFO(Lib_Net, "UPNP: IGD found - external IP={} lan={} (port mapping available)",
             external_ip, gateway.lan_addr);

    // Follow the wanted port until shutdown, then leave nothing on the router: the mappings
    // have no lease time.
    u16 mapped = 0;
    for (;;) {
        const bool quit = m_quit.load();
        const u16 wanted = quit ? u16{0} : m_wanted_port.load();
        if (wanted != mapped) {
            if (mapped != 0) {
                net_upnp_delete_port_mapping(&gateway, mapped, true, RequestTimeoutMs);
                LOG_INFO(Lib_Net, "UPNP: removed UDP port mapping {}", mapped);
            }
            mapped = wanted;
            if (wanted != 0) {
                if (net_upnp_add_port_mapping(&gateway, wanted, true, MappingDescription,
                                              RequestTimeoutMs)) {
                    LOG_INFO(Lib_Net, "UPNP: mapped UDP port {}", wanted);
                } else {
                    LOG_WARNING(Lib_Net, "UPNP: failed to map port {}", wanted);
                }
            }
            continue;
        }
        if (quit) {
            break;
        }
        const int key = retro_eventcount_prepare_wait(&m_wake);
        if (m_quit.load() || m_wanted_port.load() != mapped) {
            retro_eventcount_cancel_wait(&m_wake);
            continue;
        }
        retro_eventcount_commit_wait(&m_wake, key);
    }
    net_http_deinit();
}

} // namespace Libraries::Net
