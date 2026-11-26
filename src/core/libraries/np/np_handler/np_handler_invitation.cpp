// SPDX-FileCopyrightText: Copyright 2019-2026 rpcs3 Project
// SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <algorithm>
#include <chrono>
#include <cstring>
#include <net/net_http.h>

#include <rthreads/rthreads.h>
#include "common/logging/log.h"
#include "core/emulator_settings.h"
#include "core/libraries/invitation_dialog/invitation_dialog.h"
#include "core/libraries/np/np_handler/np_handler.h"
#include "core/libraries/np/np_handler/np_handler_c.h"
#include "core/libraries/np/np_handler/np_invitation_json.h"
#include "core/libraries/system/systemservice.h"
#include "core/net/net_service.h"

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
        LOG_ERROR(NpHandler, "no bearer token for user_id=%d", user_id);
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
        LOG_ERROR(NpHandler, "POST %s failed (no response)", (path).c_str());
        return false;
    }
    if (post.status != 200 && post.status != 204) {
        LOG_ERROR(NpHandler, "POST %s -> HTTP %d", (path).c_str(), post.status);
        return false;
    }
    LOG_INFO(NpHandler, "sent invite to %zu recipient(s) for session '%s'", to.size(),
             (session_id).c_str());
    return true;
}

void NpHandler::PostSessionInvitationEvent(s32 user_id, const std::string& session_id,
                                           const std::string& invitation_id,
                                           const std::string& accepter_online_id,
                                           const std::string& inviter_online_id,
                                           OrbisNpAccountId inviter_account_id) {
    // Written where it waits for the game: an event is 8 KiB
    OrbisSystemServiceEvent* event = system_service_event_claim();
    if (!event) {
        return;
    }
    event->event_type = ORBIS_SYSTEM_SERVICE_EVENT_SESSION_INVITATION;

    auto* param = reinterpret_cast<OrbisNpSessionInvitationEventParam*>(event->param);
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

    system_service_event_post(event);
    LOG_INFO(NpHandler,
             "Posted SESSION_INVITATION user_id=%d session='%s' invitation='%s' flag=%d "
             "onlineId='%s' referral='%s'(%" PRIu64 ")",
             user_id, (session_id).c_str(), (invitation_id).c_str(), param->flag,
             (accepter_online_id).c_str(), (inviter_online_id).c_str(), inviter_account_id);
}

std::vector<NpHandler::PendingInvitation> NpHandler::GetPendingInvitations(s32 user_id) const {
    return m_mutex_pending_invites.Run([&]() -> std::vector<NpHandler::PendingInvitation> {
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
    });
}

bool NpHandler::AcceptSessionInvitation(s32 user_id, const std::string& invitation_id) {
    // Pull the matching stashed invite (and drop it).
    PendingInvitation inv;
    bool found = false;
    m_mutex_pending_invites.Run([&] {
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
    });
    // Whatever surface handled it (RECV dialog or the emulator prompt), retire the other one.
    ovl_invite_remove(invitation_id.c_str());
    if (!found) {
        LOG_ERROR(NpHandler, "no pending invite '%s' for user_id=%d", (invitation_id).c_str(),
                  user_id);
        return false;
    }
    // Raise the join event now that the user has explicitly accepted (via the RECV dialog or the
    // emulator's system-UI equivalent).
    PostSessionInvitationEvent(user_id, inv.session_id, invitation_id, inv.to_npid, inv.from_npid,
                               inv.from_account_id);
    LOG_INFO(NpHandler, "accepted '%s' session='%s'", (invitation_id).c_str(),
             (inv.session_id).c_str());
    return true;
}

void NpHandler::DeclineSessionInvitation(s32 user_id, const std::string& invitation_id) {
    ovl_invite_remove(invitation_id.c_str());
    m_mutex_pending_invites.Run([&] {
        auto uit = m_pending_invites.find(user_id);
        if (uit == m_pending_invites.end()) {
            return;
        }
        auto& v = uit->second;
        v.erase(std::remove_if(
                    v.begin(), v.end(),
                    [&](const PendingInvitation& p) { return p.invitation_id == invitation_id; }),
                v.end());
        LOG_INFO(NpHandler, "dismissed '%s' for user_id=%d", (invitation_id).c_str(), user_id);
    });
}

} // namespace Libraries::Np

// An invitation accepted or declined on the overlay's prompt (ovl_invite.c), on the overlay's
// thread. Answering hits the shadNet server (blocking HTTP inside NpHandler), so it runs on a
// thread of its own; the prompt shows it busy until it is removed, and an acceptance's outcome
// is told as a shadNet notification.
extern "C" void ovl_invite_chosen(int user_id, const char* invitation_id, const char* from_npid,
                                  int accept) {
    struct Job {
        int user_id;
        std::string id;
        std::string from;
        int accept;

        static void Run(void* arg) {
            auto* const job = static_cast<Job*>(arg);
            auto& np = Libraries::Np::NpHandler::GetInstance();
            if (job->accept) {
                const bool ok = np.AcceptSessionInvitation(job->user_id, job->id);
                ovl_invite_remove(job->id.c_str());
                ShadNetNotify(OVL_SHADNET_INFO,
                              ok ? "Accepted " + job->from + "'s invitation, joining session"
                                 : "Could not accept " + job->from +
                                       "'s invitation (expired or already used)");
            } else {
                np.DeclineSessionInvitation(job->user_id, job->id);
                ovl_invite_remove(job->id.c_str());
            }
            delete job;
        }
    };
    auto* const job = new Job{user_id, invitation_id, from_npid, accept};
    if (sthread_t* thread = sthread_create(&Job::Run, job)) {
        sthread_detach(thread);
    } else {
        delete job;
    }
}

extern "C" void np_lend_pending_invitations(s32 user_id,
                                            void (*use)(const ovl_invdlg_invite_t* invites,
                                                        int count, size_t pending, void* ctx),
                                            void* ctx) {
    const auto pend = Libraries::Np::NpHandler::GetInstance().GetPendingInvitations(user_id);
    ovl_invdlg_invite_t invites[OVL_INVDLG_INVITES];
    int n = 0;
    for (; n < static_cast<int>(pend.size()) && n < OVL_INVDLG_INVITES; n++) {
        invites[n].invitation_id = pend[n].invitation_id.c_str();
        invites[n].from_npid = pend[n].from_npid.c_str();
    }
    use(invites, n, pend.size(), ctx);
}

// What the overlay's dialog asks of the core, on the overlay's thread. Sending, accepting and
// declining hit the shadNet server (blocking HTTP inside NpHandler), so each runs on a thread
// of its own; the dialog shows itself busy until a send or an acceptance says how it went.
extern "C" int ovl_invitation_dialog_friends(int user_id, ovl_invdlg_friend_t* out, int max) {
    const auto list = Libraries::Np::NpHandler::GetInstance().GetFriendList(user_id);
    int n = 0;
    for (const auto& f : list.friends) {
        if (n == max) {
            break;
        }
        const size_t len = std::min(f.npid.size(), sizeof(out[n].npid) - 1);
        std::memcpy(out[n].npid, f.npid.data(), len);
        out[n].npid[len] = '\0';
        out[n].online = f.online;
        n++;
    }
    return n;
}

// A request of the overlay's dialog, sent to the shadNet server (blocking HTTP inside
// NpHandler) on a thread of its own that nobody joins
struct InvitationJob {
    enum Kind { Send, Accept, Decline } kind;
    int user_id;
    std::string id; // the session's, or the invitation's
    std::string message;
    std::vector<std::string> recipients;
    unsigned token;

    static void Run(void* arg) {
        auto* const job = static_cast<InvitationJob*>(arg);
        auto& np = Libraries::Np::NpHandler::GetInstance();
        switch (job->kind) {
        case Send:
            ovl_invitation_dialog_request_done(
                job->token,
                np.SendSessionInvitation(job->user_id, job->id, job->recipients, job->message));
            break;
        case Accept:
            ovl_invitation_dialog_request_done(job->token,
                                               np.AcceptSessionInvitation(job->user_id, job->id));
            break;
        case Decline:
            np.DeclineSessionInvitation(job->user_id, job->id);
            break;
        }
        delete job;
    }
};

static void StartInvitationJob(InvitationJob* job) {
    if (sthread_t* thread = sthread_create(&InvitationJob::Run, job)) {
        sthread_detach(thread);
    } else {
        delete job;
    }
}

extern "C" void ovl_invitation_dialog_send(int user_id, const char* session_id, const char* message,
                                           const char* const* to, int count, unsigned token) {
    StartInvitationJob(new InvitationJob{InvitationJob::Send, user_id, session_id, message,
                                         std::vector<std::string>(to, to + count), token});
}

extern "C" void ovl_invitation_dialog_accept(int user_id, const char* invitation_id,
                                             unsigned token) {
    StartInvitationJob(
        new InvitationJob{InvitationJob::Accept, user_id, invitation_id, {}, {}, token});
}

extern "C" void ovl_invitation_dialog_decline(int user_id, const char* invitation_id) {
    StartInvitationJob(
        new InvitationJob{InvitationJob::Decline, user_id, invitation_id, {}, {}, 0});
}
