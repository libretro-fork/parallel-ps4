// SPDX-FileCopyrightText: Copyright 2019-2026 rpcs3 Project
// SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <algorithm>
#include <chrono>
#include <cstring>
#include <net/net_http.h>

#include "common/logging/log.h"
#include "core/emulator_settings.h"
#include "core/libraries/invitation_dialog/invitation_dialog.h"
#include "core/libraries/np/np_handler/np_handler.h"
#include "core/libraries/np/np_handler/np_invitation_json.h"
#include "core/libraries/system/systemservice.h"
#include "core/net/net_service.h"
#include "imgui/invitation_prompt_layer.h"

namespace Libraries::Np {

bool NpHandler::SendSessionInvitation(s32 user_id, const std::string& session_id,
                                      const std::vector<std::string>& to,
                                      const std::string& message) {
    if (session_id.empty() || to.empty()) {
        LOG_ERROR(NpHandler, "empty session_id or recipient list");
        return false;
    }
    const std::string base_url = emu_settings_get_shadnet_webapi_server();
    if (base_url.empty()) {
        LOG_ERROR(NpHandler, "WebAPI server address is empty");
        return false;
    }
    const std::string token = GetBearerToken(user_id);
    if (token.empty()) {
        LOG_ERROR(NpHandler, "no bearer token for user_id={}", user_id);
        return false;
    }

    // invitation-request JSON: {"to":[...],"message":"..."}
    std::vector<const char*> recipients;
    recipients.reserve(to.size());
    for (const auto& npid : to) {
        recipients.push_back(npid.c_str());
    }
    std::string json;
    json.resize(np_invitation_request_json(nullptr, 0, recipients.data(), recipients.size(),
                                           message.c_str()));
    if (json.empty() ||
        np_invitation_request_json(json.data(), json.size() + 1, recipients.data(),
                                   recipients.size(), message.c_str()) != json.size()) {
        LOG_ERROR(NpHandler, "could not build the invitation request");
        return false;
    }

    // multipart/mixed with a single JSON part; shadNet keys on the Content-Description.
    const std::string boundary = "shadps4invite" + std::to_string(user_id);
    std::string body;
    body += "--" + boundary + "\r\n";
    body += "Content-Type: application/json; charset=utf-8\r\n";
    body += "Content-Description: invitation-request\r\n\r\n";
    body += json;
    body += "\r\n--" + boundary + "--\r\n";

    const std::string path = "/v1/sessions/" + session_id + "/invitations";
    const std::string content_type = "multipart/mixed; boundary=" + boundary;

    // A POST on the network service thread, which owns net_http
    struct Post {
        std::string url, headers, body, content_type;
        int status = -1;
    } post{base_url + path, "Authorization: Bearer " + token + "\r\n", body, content_type};
    const auto run = [](void* p) {
        auto* post = static_cast<Post*>(p);
        http_connection_t* conn = net_http_connection_new(post->url.c_str(), "POST", nullptr);
        if (!conn) {
            return;
        }
        while (!net_http_connection_iterate(conn)) {
        }
        if (!net_http_connection_done(conn)) {
            net_http_connection_free(conn);
            return;
        }
        net_http_connection_set_headers(conn, post->headers.c_str());
        net_http_connection_set_content(conn, post->content_type.c_str(), post->body.size(),
                                        post->body.data());
        http_t* http = net_http_new(conn);
        net_http_connection_free(conn);
        if (!http) {
            return;
        }
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(15);
        bool done = false;
        while (!(done = net_http_update(http, nullptr, nullptr)) &&
               std::chrono::steady_clock::now() < deadline) {
            net_http_wait(http, 50);
        }
        if (done && !net_http_failure(http, nullptr) && net_http_status(http) > 0) {
            post->status = net_http_status(http);
        }
        net_http_delete(http);
    };
    if (!Core::Net::NetService::Instance().Run(run, &post) || post.status < 0) {
        LOG_ERROR(NpHandler, "POST {} failed (no response)", path);
        return false;
    }
    if (post.status != 200 && post.status != 204) {
        LOG_ERROR(NpHandler, "POST {} -> HTTP {}", path, post.status);
        return false;
    }
    LOG_INFO(NpHandler, "sent invite to {} recipient(s) for session '{}'", to.size(), session_id);
    return true;
}

void NpHandler::PostSessionInvitationEvent(s32 user_id, const std::string& session_id,
                                           const std::string& invitation_id,
                                           const std::string& accepter_online_id,
                                           const std::string& inviter_online_id,
                                           OrbisNpAccountId inviter_account_id) {
    using Libraries::InvitationDialog::ORBIS_NP_SESSION_INVITATION_EVENT_FLAG_INVITATION;
    using Libraries::InvitationDialog::OrbisNpSessionInvitationEventParam;

    Libraries::SystemService::OrbisSystemServiceEvent event{};
    event.event_type = Libraries::SystemService::OrbisSystemServiceEventType::SessionInvitation;

    auto* param = reinterpret_cast<OrbisNpSessionInvitationEventParam*>(event.param);
    std::memset(param, 0, sizeof(*param));
    std::strncpy(param->sessionId.data, session_id.c_str(), sizeof(param->sessionId.data) - 1);
    if (!invitation_id.empty()) {
        std::strncpy(param->invitationId.data, invitation_id.c_str(),
                     sizeof(param->invitationId.data) - 1);
        param->flag = ORBIS_NP_SESSION_INVITATION_EVENT_FLAG_INVITATION;
    } else {
        param->flag = 0; // join from session info (no invitation id in the push)
    }
    // data[16] is followed by its own 'term' byte, so a full 16-char online id must not be cut.
    std::strncpy(param->onlineId.data, accepter_online_id.c_str(), sizeof(param->onlineId.data));
    param->userId = user_id;
    std::strncpy(param->referralOnlineId.data, inviter_online_id.c_str(),
                 sizeof(param->referralOnlineId.data));
    param->referralAccountId = inviter_account_id;

    Libraries::SystemService::PushSystemServiceEvent(event);
    LOG_INFO(NpHandler,
             "Posted SESSION_INVITATION user_id={} session='{}' invitation='{}' flag={} "
             "onlineId='{}' referral='{}'({})",
             user_id, session_id, invitation_id, param->flag, accepter_online_id, inviter_online_id,
             inviter_account_id);
}

std::vector<NpHandler::PendingInvitation> NpHandler::GetPendingInvitations(s32 user_id) const {
    std::lock_guard lk(m_mutex_pending_invites);
    const auto it = m_pending_invites.find(user_id);
    if (it == m_pending_invites.end()) {
        return {};
    }
    const auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
                         std::chrono::system_clock::now().time_since_epoch())
                         .count();
    std::vector<PendingInvitation> out;
    out.reserve(it->second.size());
    for (const auto& p : it->second) {
        if (p.valid_until == 0 || now <= p.valid_until) {
            out.push_back(p);
        }
    }
    return out;
}

bool NpHandler::AcceptSessionInvitation(s32 user_id, const std::string& invitation_id) {
    // Pull the matching stashed invite (and drop it).
    PendingInvitation inv;
    bool found = false;
    {
        std::lock_guard lk(m_mutex_pending_invites);
        const auto mit = m_pending_invites.find(user_id);
        if (mit != m_pending_invites.end()) {
            auto& v = mit->second;
            for (auto it = v.begin(); it != v.end(); ++it) {
                if (it->invitation_id == invitation_id) {
                    inv = *it;
                    v.erase(it);
                    found = true;
                    break;
                }
            }
        }
    }
    // Whatever surface handled it (RECV dialog or the emulator prompt), retire the other one.
    ImGui::InvitationPrompt::Dismiss(invitation_id);
    if (!found) {
        LOG_ERROR(NpHandler, "no pending invite '{}' for user_id={}", invitation_id, user_id);
        return false;
    }
    // Raise the join event now that the user has explicitly accepted (via the RECV dialog or the
    // emulator's system-UI equivalent).
    PostSessionInvitationEvent(user_id, inv.session_id, invitation_id, inv.to_npid, inv.from_npid,
                               inv.from_account_id);
    LOG_INFO(NpHandler, "accepted '{}' session='{}'", invitation_id, inv.session_id);
    return true;
}

void NpHandler::DeclineSessionInvitation(s32 user_id, const std::string& invitation_id) {
    ImGui::InvitationPrompt::Dismiss(invitation_id);
    std::lock_guard lock(m_mutex_pending_invites);
    auto uit = m_pending_invites.find(user_id);
    if (uit == m_pending_invites.end()) {
        return;
    }
    auto& v = uit->second;
    v.erase(std::remove_if(
                v.begin(), v.end(),
                [&](const PendingInvitation& p) { return p.invitation_id == invitation_id; }),
            v.end());
    LOG_INFO(NpHandler, "dismissed '{}' for user_id={}", invitation_id, user_id);
}

} // namespace Libraries::Np
