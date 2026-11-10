// SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <net/net_compat.h>

#include "common/logging/log.h"
#include "core/net/net_service.h"
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
    // Made first, so it is destroyed after this and can still run the shutdown job
    Core::Net::NetService::Instance();
}

UPnPClient::~UPnPClient() {
    if (m_started.load()) {
        // Leave nothing on the router: the mappings have no lease time
        Core::Net::NetService::Instance().Run(&UPnPClient::Shutdown, this);
    }
}

void UPnPClient::Start() {
    if (!m_p2p_features_enabled.load()) {
        LOG_INFO(Lib_Net, "UPNP: skipped because Matching2 is disabled");
        return;
    }
    bool expected = false;
    if (!m_started.compare_exchange_strong(expected, true)) {
        return;
    }
    LOG_INFO(Lib_Net, "UPNP: looking for the router");
    Core::Net::NetService::Instance().Post(&UPnPClient::Discover, this);
}

void UPnPClient::SetP2PFeaturesEnabled(bool enabled) {
    m_p2p_features_enabled.store(enabled);
}

u32 UPnPClient::GetExternalIp() const {
    return m_external_ip_nbo.load();
}

void UPnPClient::MapPortAsync(u16 port) {
    m_wanted_port.store(port);
    if (m_started.load()) {
        Core::Net::NetService::Instance().Post(&UPnPClient::Sync, this);
    }
}

void UPnPClient::UnmapPort(u16 port) {
    u16 expected = port;
    if (m_wanted_port.compare_exchange_strong(expected, 0) && m_started.load()) {
        Core::Net::NetService::Instance().Post(&UPnPClient::Sync, this);
    }
}

void UPnPClient::Discover(void* self_) {
    auto* self = static_cast<UPnPClient*>(self_);
    char external_ip[64];
    if (!net_upnp_discover(&self->m_gateway, DiscoverTimeoutMs)) {
        LOG_INFO(Lib_Net, "UPNP: no IGD found");
        return;
    }
    if (!net_upnp_external_address(&self->m_gateway, external_ip, sizeof(external_ip),
                                   RequestTimeoutMs)) {
        LOG_WARNING(Lib_Net, "UPNP: IGD found but failed to get external IP");
        return;
    }
    u32 ip_nbo = 0;
    inet_pton(AF_INET, external_ip, &ip_nbo);
    self->m_external_ip_nbo.store(ip_nbo);
    self->m_have_gateway = true;
    LOG_INFO(Lib_Net, "UPNP: IGD found - external IP={} lan={} (port mapping available)",
             external_ip, self->m_gateway.lan_addr);
    Sync(self);
}

void UPnPClient::Sync(void* self_) {
    auto* self = static_cast<UPnPClient*>(self_);
    if (!self->m_have_gateway) {
        return; // Discover syncs once it has found the router
    }
    const u16 wanted = self->m_wanted_port.load();
    if (wanted == self->m_mapped) {
        return;
    }
    if (self->m_mapped != 0) {
        net_upnp_delete_port_mapping(&self->m_gateway, self->m_mapped, true, RequestTimeoutMs);
        LOG_INFO(Lib_Net, "UPNP: removed UDP port mapping {}", self->m_mapped);
    }
    self->m_mapped = wanted;
    if (wanted != 0) {
        if (net_upnp_add_port_mapping(&self->m_gateway, wanted, true, MappingDescription,
                                      RequestTimeoutMs)) {
            LOG_INFO(Lib_Net, "UPNP: mapped UDP port {}", wanted);
        } else {
            LOG_WARNING(Lib_Net, "UPNP: failed to map port {}", wanted);
        }
    }
}

void UPnPClient::Shutdown(void* self_) {
    auto* self = static_cast<UPnPClient*>(self_);
    self->m_wanted_port.store(0);
    Sync(self);
}

} // namespace Libraries::Net
