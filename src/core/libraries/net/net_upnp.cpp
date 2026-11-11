// SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <net/net_compat.h>

#include <retro_atomic.h>
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
    if (retro_atomic_load_seq_cst_int(&m_started) != 0) {
        // Leave nothing on the router: the mappings have no lease time
        Core::Net::NetService::Instance().Run(&UPnPClient::Shutdown, this);
    }
}

void UPnPClient::Start() {
    if (retro_atomic_load_seq_cst_int(&m_p2p_features_enabled) == 0) {
        LOG_INFO(Lib_Net, "UPNP: skipped because Matching2 is disabled");
        return;
    }
    bool expected = false;
    if (!(retro_atomic_cas_int(&m_started, expected, true) ||
          ((expected = (retro_atomic_load_acquire_int(&m_started) != 0)), false))) {
        return;
    }
    LOG_INFO(Lib_Net, "UPNP: looking for the router");
    Core::Net::NetService::Instance().Post(&UPnPClient::Discover, this);
}

void UPnPClient::SetP2PFeaturesEnabled(bool enabled) {
    (void)retro_atomic_exchange_int(&m_p2p_features_enabled, enabled);
}

u32 UPnPClient::GetExternalIp() const {
    return static_cast<u32>(retro_atomic_load_seq_cst_int(&m_external_ip_nbo));
}

void UPnPClient::MapPortAsync(u16 port) {
    (void)retro_atomic_exchange_int(&m_wanted_port, port);
    if (retro_atomic_load_seq_cst_int(&m_started) != 0) {
        Core::Net::NetService::Instance().Post(&UPnPClient::Sync, this);
    }
}

void UPnPClient::UnmapPort(u16 port) {
    if (retro_atomic_cas_int(&m_wanted_port, port, 0) &&
        retro_atomic_load_seq_cst_int(&m_started) != 0) {
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
    (void)retro_atomic_exchange_int(&self->m_external_ip_nbo, ip_nbo);
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
    const u16 wanted = static_cast<u16>(retro_atomic_load_acquire_int(&self->m_wanted_port));
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
    (void)retro_atomic_exchange_int(&self->m_wanted_port, 0);
    Sync(self);
}

} // namespace Libraries::Net
