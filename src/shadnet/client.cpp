// SPDX-FileCopyrightText: Copyright 2019-2026 rpcs3 Project
// SPDX-FileCopyrightText: Copyright 2024-2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <algorithm>
#include <chrono>

#include <retro_atomic.h>
#include "client.h"
#include "common/elf_info.h"
#include "common/logging/log.h"
#include "common/scm_rev.h"
#include "common/string_util.h"
#include "common/thread_name.h"
#include "core/libraries/kernel/time.h"
#include "shadnet/messages.h"

#ifdef _WIN32
#pragma comment(lib, "Ws2_32.lib")
static void PlatformInit() {
    static bool done = false;
    if (!done) {
        WSADATA wd;
        WSAStartup(MAKEWORD(2, 2), &wd);
        done = true;
    }
}
#else
static void PlatformInit() {}
#endif

namespace ShadNet {

// Build a u32-LE-prefixed proto blob payload for a request packet.
template <typename T>
static std::vector<u8> MakeProtoPayload(const T& msg) {
    const u32 len = static_cast<u32>(EncodedSize(msg));
    std::vector<u8> out;
    out.reserve(4 + len);
    out.push_back(static_cast<u8>(len));
    out.push_back(static_cast<u8>(len >> 8));
    out.push_back(static_cast<u8>(len >> 16));
    out.push_back(static_cast<u8>(len >> 24));
    EncodeAppend(msg, out);
    return out;
}

// Read a u32-LE-prefixed proto blob starting at pos in p.
// Returns the raw bytes ready for Decode.
std::string ShadNetClient::ExtractBlob(const std::vector<u8>& p, int pos) {
    if (pos + 4 > static_cast<int>(p.size()))
        return {};
    const u32 len = GetLE32(p.data() + pos);
    pos += 4;
    if (pos + static_cast<int>(len) > static_cast<int>(p.size()))
        return {};
    return std::string(reinterpret_cast<const char*>(p.data() + pos), len);
}

ShadNetClient::ShadNetClient() {
    event_latch_init(&m_connect_settled);
    event_latch_init(&m_login_settled);
    mpsc_stack_init(&m_send_stack);
    retro_eventcount_init(&m_send_ec);
    PlatformInit();
}

ShadNetClient::~ShadNetClient() {
    Stop();
    // Packets never sent
    for (mpsc_stack_node_t* node = mpsc_stack_drain(&m_send_stack); node != nullptr;) {
        auto* const packet = reinterpret_cast<SendPacket*>(node);
        node = node->next;
        delete packet;
    }
    retro_eventcount_free(&m_send_ec);
    event_latch_free(&m_connect_settled);
    event_latch_free(&m_login_settled);
}

void ShadNetClient::Start(const std::string& host, u16 port, const std::string& npid,
                          const std::string& password, const std::string& token) {
    m_host = host;
    m_port = port;
    m_npid = npid;
    m_password = password;
    m_token = token;
    (void)retro_atomic_exchange_int(&m_terminate, 0);
    event_latch_clear(&m_connect_settled);
    event_latch_clear(&m_login_settled);
    stop_thread_start(
        &m_thread_connect, [](void* self) { static_cast<ShadNetClient*>(self)->ConnectThread(); },
        this);
}

void ShadNetClient::Stop() {
    (void)retro_atomic_exchange_int(&m_terminate, 1);
    event_latch_set(&m_connect_settled);
    event_latch_set(&m_login_settled);
    retro_eventcount_notify(&m_send_ec);
    DoDisconnect();
    stop_thread_join(&m_thread_connect);
    stop_thread_join(&m_thread_reader);
    stop_thread_join(&m_thread_writer);
}

ShadNetState ShadNetClient::WaitForConnection() {
    if (auto returned = m_mutex_connected.Run([&]() -> std::optional<ShadNetState> {
            if (retro_atomic_load_seq_cst_int(&m_connected) != 0)
                return std::optional<ShadNetState>{ShadNetState::Ok};
            return std::nullopt;
        })) {
        return *returned;
    }
    event_latch_wait(&m_connect_settled);
    return (retro_atomic_load_seq_cst_int(&m_connected) != 0)
               ? ShadNetState::Ok
               : static_cast<ShadNetState>(retro_atomic_load_seq_cst_int(&m_state));
}

ShadNetState ShadNetClient::WaitForAuthenticated() {
    if (auto returned = m_mutex_authenticated.Run([&]() -> std::optional<ShadNetState> {
            if (retro_atomic_load_seq_cst_int(&m_authenticated) != 0)
                return std::optional<ShadNetState>{ShadNetState::Ok};
            return std::nullopt;
        })) {
        return *returned;
    }
    event_latch_wait(&m_login_settled);
    return (retro_atomic_load_seq_cst_int(&m_authenticated) != 0)
               ? ShadNetState::Ok
               : static_cast<ShadNetState>(retro_atomic_load_seq_cst_int(&m_state));
}

bool ShadNetClient::IsConnected() const {
    return (retro_atomic_load_seq_cst_int(&m_connected) != 0);
}
bool ShadNetClient::IsAuthenticated() const {
    return (retro_atomic_load_seq_cst_int(&m_authenticated) != 0);
}
ShadNetState ShadNetClient::GetState() const {
    return static_cast<ShadNetState>(retro_atomic_load_seq_cst_int(&m_state));
}
const std::string& ShadNetClient::GetAvatarUrl() const {
    return m_avatar_url;
}
u64 ShadNetClient::GetUserId() const {
    return m_user_id;
}
const std::string& ShadNetClient::GetNpid() const {
    return m_npid;
}
u32 ShadNetClient::GetAddrLocal() const {
    return static_cast<u32>(retro_atomic_load_seq_cst_int(&m_addr_local));
}
u32 ShadNetClient::GetAddrServer() const {
    return static_cast<u32>(retro_atomic_load_seq_cst_int(&m_addr_server));
}
bool ShadNetClient::IsMatching2Enabled() const {
    return (retro_atomic_load_seq_cst_int(&m_matching2_enabled) != 0);
}

u32 ShadNetClient::GetNumFriends() const {
    return m_mutex_friends.Run([&]() -> u32 { return static_cast<u32>(m_friends.size()); });
}

std::optional<std::string> ShadNetClient::GetFriendNpid(u32 index) const {
    return m_mutex_friends.Run([&]() -> std::optional<std::string> {
        if (index >= m_friends.size())
            return std::nullopt;
        return m_friends[index].npid;
    });
}

std::string ShadNetClient::GetBearerToken() const {
    return m_mutex_bearer.Run([&]() -> std::string { return m_bearer_token; });
}

// Threading

void ShadNetClient::ConnectThread() {
    thread_name_set("ShadNet:Connect");
    bool connected = false;
    u32 backoff_ms = SHAD_CONNECT_RETRY_BACKOFF_MS;
    for (u32 attempt = 1;
         attempt <= SHAD_CONNECT_MAX_ATTEMPTS && retro_atomic_load_seq_cst_int(&m_terminate) == 0;
         ++attempt) {
        if (DoConnect()) {
            connected = true;
            break;
        }
        if (static_cast<ShadNetState>(retro_atomic_load_seq_cst_int(&m_state)) ==
            ShadNetState::FailureProtocol)
            break;
        if ((retro_atomic_load_seq_cst_int(&m_terminate) != 0) ||
            attempt == SHAD_CONNECT_MAX_ATTEMPTS)
            break;
        LOG_WARNING(ShadNet, "ShadNet: connect attempt %u/%u to %s:%u failed, retrying in %u ms",
                    attempt, SHAD_CONNECT_MAX_ATTEMPTS, (m_host).c_str(), m_port, backoff_ms);
        // The backoff is of the game's time; Stop() sets the latch waited on, and ends it
        Libraries::Kernel::GameWait(
            &m_connect_settled.waiters,
            Libraries::Kernel::GameDeadlineIn(static_cast<u64>(backoff_ms) * 1'000'000),
            [this] { return retro_atomic_load_seq_cst_int(&m_terminate) != 0; });
        backoff_ms = std::min<u32>(backoff_ms * 2, 8000);
    }

    if (!connected) {
        // m_state holds the last failure reason from DoConnect.
        event_latch_set(&m_connect_settled);
        event_latch_set(&m_login_settled);
        return;
    }

    stop_thread_start(
        &m_thread_reader, [](void* self) { static_cast<ShadNetClient*>(self)->ReaderThread(); },
        this);
    stop_thread_start(
        &m_thread_writer, [](void* self) { static_cast<ShadNetClient*>(self)->WriterThread(); },
        this);
    event_latch_set(&m_connect_settled);

    // Build Login request as protobuf
    const std::string title_id(Common::ElfInfo::Instance().GameSerial());
    const std::string title_name(Common::ElfInfo::Instance().Title());
    shadnet_LoginRequest req{};
    req.npid = Bytes(m_npid);
    req.password = Bytes(m_password);
    req.token = Bytes(m_token);
    req.title_id = Bytes(title_id);
    req.title_name = Bytes(title_name);
    // Appear-Offline preference. shadNet handles us as offline for everyone else while set.
    req.appear_offline = m_appear_offline;

    const u64 id = static_cast<u64>(retro_atomic_fetch_add_64(&m_pkt_counter, 1));
    if (!SendAll(BuildPacket(CommandType::Login, id, MakeProtoPayload(req)))) {
        LOG_ERROR(ShadNet, "ShadNet: Failed to send Login packet");
        (void)retro_atomic_exchange_int(&m_state, static_cast<int>(ShadNetState::FailureOther));
        return;
    }
    LOG_INFO(ShadNet, "Login packet sent for '%s'", (m_npid).c_str());
}

namespace {
thread_local bool t_reader_thread = false;
}

bool ShadNetClient::OnReaderThread() {
    return t_reader_thread;
}

void ShadNetClient::ReaderThread() {
    thread_name_set("ShadNet:Reader");
    t_reader_thread = true;
    while (retro_atomic_load_seq_cst_int(&m_terminate) == 0) {
        u8 hdr[SHAD_HEADER_SIZE];
        if (!RecvN(hdr, SHAD_HEADER_SIZE)) {
            if (retro_atomic_load_seq_cst_int(&m_terminate) == 0)
                LOG_WARNING(ShadNet, "Reader header recv failed, disconnecting");
            break;
        }
        const auto ptype = static_cast<PacketType>(hdr[0]);
        const u16 cmd_raw = GetLE16(hdr + 1);
        const u32 total_sz = GetLE32(hdr + 3);
        const u64 pkt_id = GetLE64(hdr + 7);

        if (total_sz < SHAD_HEADER_SIZE || total_sz > SHAD_MAX_PACKET_SIZE) {
            LOG_ERROR(ShadNet, "Corrupt packet (total_sz=%u)", total_sz);
            (void)retro_atomic_exchange_int(&m_state,
                                            static_cast<int>(ShadNetState::FailureProtocol));
            break;
        }
        std::vector<u8> payload;
        const u32 payload_sz = total_sz - static_cast<u32>(SHAD_HEADER_SIZE);
        if (payload_sz > 0) {
            payload.resize(payload_sz);
            if (!RecvN(payload.data(), payload_sz)) {
                if (retro_atomic_load_seq_cst_int(&m_terminate) == 0)
                    LOG_WARNING(ShadNet, "Reader payload recv failed");
                break;
            }
        }
        DispatchPacket(ptype, cmd_raw, pkt_id, payload);
    }

    if (retro_atomic_load_seq_cst_int(&m_authenticated) == 0) {
        if (static_cast<ShadNetState>(retro_atomic_load_seq_cst_int(&m_state)) == ShadNetState::Ok)
            (void)retro_atomic_exchange_int(&m_state, static_cast<int>(ShadNetState::FailureOther));
        event_latch_set(&m_login_settled);
    }
    (void)retro_atomic_exchange_int(&m_connected, 0);
    (void)retro_atomic_exchange_int(&m_authenticated, 0);
    LOG_INFO(ShadNet, "ReaderThread exiting");
}

void ShadNetClient::QueuePacket(std::vector<u8>&& pkt) {
    auto* const packet = new SendPacket{{}, std::move(pkt)};
    mpsc_stack_push(&m_send_stack, &packet->node);
    retro_eventcount_notify(&m_send_ec);
}

void ShadNetClient::WriterThread() {
    thread_name_set("ShadNet:Writer");
    while (retro_atomic_load_seq_cst_int(&m_terminate) == 0) {
        // Everything pushed so far, oldest first
        mpsc_stack_node_t* node = mpsc_stack_reverse(mpsc_stack_drain(&m_send_stack));
        if (node == nullptr) {
            const int key = retro_eventcount_prepare_wait(&m_send_ec);
            if (retro_atomic_load_seq_cst_int(&m_terminate) != 0 ||
                !mpsc_stack_empty(&m_send_stack)) {
                retro_eventcount_cancel_wait(&m_send_ec);
                continue;
            }
            retro_eventcount_commit_wait(&m_send_ec, key);
            continue;
        }
        bool failed = false;
        while (node != nullptr) {
            auto* const packet = reinterpret_cast<SendPacket*>(node);
            node = node->next;
            if (!failed && retro_atomic_load_seq_cst_int(&m_connected) != 0 &&
                !SendAll(packet->bytes)) {
                LOG_ERROR(ShadNet, "WriterThread send failed");
                failed = true;
            }
            delete packet;
        }
        if (failed) {
            return;
        }
    }
}

// Connect / Disconnect

bool ShadNetClient::DoConnect() {
    (void)retro_atomic_exchange_int(
        &m_state, static_cast<int>(
                      ShadNetState::Ok)); // reset; this attempt sets a failure code only on error
    struct addrinfo hints{}, *res_list = nullptr;
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    if (::getaddrinfo(m_host.c_str(), std::to_string(m_port).c_str(), &hints, &res_list) != 0 ||
        !res_list) {
        LOG_ERROR(ShadNet, "DNS resolution failed for '%s'", (m_host).c_str());
        (void)retro_atomic_exchange_int(&m_state, static_cast<int>(ShadNetState::FailureResolve));
        return false;
    }

    m_sock = ::socket(res_list->ai_family, SOCK_STREAM, IPPROTO_TCP);
    if (m_sock == SHAD_INVALID_SOCK) {
        ::freeaddrinfo(res_list);
        (void)retro_atomic_exchange_int(&m_state, static_cast<int>(ShadNetState::FailureConnect));
        return false;
    }

    // Use non-blocking connect + select() so we time out instead of hanging
    // for the OS default TCP timeout (75 s on Linux, >20 s on Windows).
#ifdef _WIN32
    {
        u_long nb = 1;
        ::ioctlsocket(m_sock, FIONBIO, &nb);
    }
#else
    {
        int fl = ::fcntl(m_sock, F_GETFL, 0);
        ::fcntl(m_sock, F_SETFL, fl | O_NONBLOCK);
    }
#endif

    const int cr = ::connect(m_sock, res_list->ai_addr, static_cast<int>(res_list->ai_addrlen));
    ::freeaddrinfo(res_list);

    bool connected = false;
#ifdef _WIN32
    const bool in_progress = (cr < 0 && WSAGetLastError() == WSAEWOULDBLOCK);
#else
    const bool in_progress = (cr < 0 && errno == EINPROGRESS);
#endif
    if (cr == 0 || in_progress) {
        fd_set wfds;
        FD_ZERO(&wfds);
        FD_SET(m_sock, &wfds);
        struct timeval tv{static_cast<long>(SHAD_CONNECT_TIMEOUT_MS / 1000),
                          static_cast<long>((SHAD_CONNECT_TIMEOUT_MS % 1000) * 1000)};
        if (::select(static_cast<int>(m_sock) + 1, nullptr, &wfds, nullptr, &tv) > 0) {
            int err = 0;
            socklen_t len = sizeof(err);
            ::getsockopt(m_sock, SOL_SOCKET, SO_ERROR, reinterpret_cast<char*>(&err), &len);
            connected = (err == 0);
        }
    }
    if (!connected) {
        LOG_ERROR(ShadNet, "connect() timed out or failed for %s:%u", (m_host).c_str(), m_port);
        SHAD_CLOSE(m_sock);
        m_sock = SHAD_INVALID_SOCK;
        (void)retro_atomic_exchange_int(&m_state, static_cast<int>(ShadNetState::FailureConnect));
        return false;
    }

    // Restore blocking mode for RecvN / SendAll
#ifdef _WIN32
    {
        u_long nb = 0;
        ::ioctlsocket(m_sock, FIONBIO, &nb);
    }
#else
    {
        int fl = ::fcntl(m_sock, F_GETFL, 0);
        ::fcntl(m_sock, F_SETFL, fl & ~O_NONBLOCK);
    }
#endif

    struct sockaddr_in local{};
    socklen_t alen = sizeof(local);
    if (::getsockname(m_sock, reinterpret_cast<struct sockaddr*>(&local), &alen) == 0)
        (void)retro_atomic_exchange_int(&m_addr_local, local.sin_addr.s_addr);

    struct sockaddr_in peer{};
    socklen_t plen = sizeof(peer);
    if (::getpeername(m_sock, reinterpret_cast<struct sockaddr*>(&peer), &plen) == 0)
        (void)retro_atomic_exchange_int(&m_addr_server, peer.sin_addr.s_addr);

    LOG_INFO(ShadNet, "TCP connected to %s:%u", (m_host).c_str(), m_port);

    // Apply receive timeout for the ServerInfo handshake.
    // Cleared after success so ReaderThread's RecvN blocks indefinitely as intended.
#ifdef _WIN32
    DWORD so_rcv = static_cast<DWORD>(SHAD_CONNECT_TIMEOUT_MS);
#else
    struct timeval so_rcv{static_cast<long>(SHAD_CONNECT_TIMEOUT_MS / 1000),
                          static_cast<long>((SHAD_CONNECT_TIMEOUT_MS % 1000) * 1000)};
#endif
    ::setsockopt(m_sock, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&so_rcv),
                 sizeof(so_rcv));

    // ServerInfo handshake
    u8 hdr[SHAD_HEADER_SIZE];
    if (!RecvN(hdr, SHAD_HEADER_SIZE)) {
        LOG_ERROR(ShadNet, "Timeout reading ServerInfo header");
        DoDisconnect();
        (void)retro_atomic_exchange_int(&m_state,
                                        static_cast<int>(ShadNetState::FailureServerInfo));
        return false;
    }
    if (static_cast<PacketType>(hdr[0]) != PacketType::ServerInfo) {
        LOG_ERROR(ShadNet, "Expected ServerInfo, got packet type %02x", hdr[0]);
        DoDisconnect();
        (void)retro_atomic_exchange_int(&m_state,
                                        static_cast<int>(ShadNetState::FailureServerInfo));
        return false;
    }
    const u32 total_sz = GetLE32(hdr + 3);
    const u32 payload_sz = (total_sz > SHAD_HEADER_SIZE) ? total_sz - SHAD_HEADER_SIZE : 0;
    std::vector<u8> si_payload(payload_sz);
    if (payload_sz > 0 && !RecvN(si_payload.data(), payload_sz)) {
        LOG_ERROR(ShadNet, "Timeout reading ServerInfo payload");
        DoDisconnect();
        (void)retro_atomic_exchange_int(&m_state,
                                        static_cast<int>(ShadNetState::FailureServerInfo));
        return false;
    }
    if (payload_sz >= 4) {
        const u32 server_ver = GetLE32(si_payload.data());
        (void)retro_atomic_exchange_int(&m_server_protocol_version, server_ver);
        if (server_ver != SHAD_PROTOCOL_VERSION) {
            LOG_ERROR(ShadNet, "Protocol version mismatch server=%u client=%u", server_ver,
                      SHAD_PROTOCOL_VERSION);
            DoDisconnect();
            (void)retro_atomic_exchange_int(&m_state,
                                            static_cast<int>(ShadNetState::FailureProtocol));
            return false;
        }
    }
    LOG_INFO(ShadNet, "ServerInfo OK (protocol v%u)", SHAD_PROTOCOL_VERSION);

    // Clear the receive timeout ReaderThread handles the socket from here.
#ifdef _WIN32
    DWORD no_timeout = 0;
#else
    struct timeval no_timeout{0, 0};
#endif
    ::setsockopt(m_sock, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&no_timeout),
                 sizeof(no_timeout));

    (void)retro_atomic_exchange_int(&m_connected, 1);
    return true;
}

void ShadNetClient::DoDisconnect() {
    if (m_sock != SHAD_INVALID_SOCK) {
#ifdef _WIN32
        ::shutdown(m_sock, SD_BOTH);
#else
        ::shutdown(m_sock, SHUT_RDWR);
#endif
        SHAD_CLOSE(m_sock);
        m_sock = SHAD_INVALID_SOCK;
    }
}

bool ShadNetClient::RecvN(u8* buf, u32 n) {
    u32 received = 0;
    while (received < n) {
        if (retro_atomic_load_seq_cst_int(&m_terminate) != 0)
            return false;
        const int r = static_cast<int>(::recv(m_sock, reinterpret_cast<char*>(buf + received),
                                              static_cast<int>(n - received), 0));
        if (r <= 0)
            return false;
        received += static_cast<u32>(r);
    }
    return true;
}

bool ShadNetClient::SendAll(const std::vector<u8>& data) {
    return m_mutex_send_direct.Run([&]() -> bool {
        int sent = 0;
        const int total = static_cast<int>(data.size());
        while (sent < total) {
            const int r = static_cast<int>(
                ::send(m_sock, reinterpret_cast<const char*>(data.data() + sent), total - sent, 0));
            if (r < 0) {
                LOG_ERROR(ShadNet, "send() failed");
                return false;
            }
            sent += r;
        }
        return true;
    });
}

std::vector<u8> ShadNetClient::BuildPacket(CommandType cmd, u64 id,
                                           const std::vector<u8>& payload) const {
    const u32 total = static_cast<u32>(SHAD_HEADER_SIZE + payload.size());
    std::vector<u8> out(SHAD_HEADER_SIZE);
    out[0] = static_cast<u8>(PacketType::Request);
    PutLE16(out, 1, static_cast<u16>(cmd));
    PutLE32(out, 3, total);
    PutLE64(out, 7, id);
    out.insert(out.end(), payload.begin(), payload.end());
    return out;
}

u64 ShadNetClient::SubmitRequest(CommandType cmd, const std::vector<u8>& payload) {
    const u64 pkt_id = static_cast<u64>(retro_atomic_fetch_add_64(&m_pkt_counter, 1));
    auto pkt = BuildPacket(cmd, pkt_id, payload);
    QueuePacket(std::move(pkt));
    return pkt_id;
}

u64 ShadNetClient::AddFriend(const std::string& npid) {
    shadnet_FriendCommandRequest req{};
    req.npid = Bytes(npid);
    return SubmitRequest(CommandType::AddFriend, MakeProtoPayload(req));
}

u64 ShadNetClient::RemoveFriend(const std::string& npid) {
    shadnet_FriendCommandRequest req{};
    req.npid = Bytes(npid);
    return SubmitRequest(CommandType::RemoveFriend, MakeProtoPayload(req));
}

u64 ShadNetClient::AddBlock(const std::string& npid) {
    shadnet_FriendCommandRequest req{};
    req.npid = Bytes(npid);
    return SubmitRequest(CommandType::AddBlock, MakeProtoPayload(req));
}

u64 ShadNetClient::RemoveBlock(const std::string& npid) {
    shadnet_FriendCommandRequest req{};
    req.npid = Bytes(npid);
    return SubmitRequest(CommandType::RemoveBlock, MakeProtoPayload(req));
}

u64 ShadNetClient::SetAppearOffline(bool enable) {
    m_appear_offline = enable; // cache so the (re)login packet carries the current state
    if (!IsAuthenticated())
        return 0; // not connected yet -> login will carry the cached value
    shadnet_SetAppearOfflineRequest req{};
    req.appear_offline = enable;
    return SubmitRequest(CommandType::SetAppearOffline, MakeProtoPayload(req));
}

u64 ShadNetClient::LookupOnlineId(const std::string& npid) {
    shadnet_LookupOnlineIdRequest req{};
    req.npid = Bytes(npid);
    return SubmitRequest(CommandType::LookupOnlineId, MakeProtoPayload(req));
}

u64 ShadNetClient::GetAuthorizationCode(const std::string& client_id, const std::string& scope) {
    shadnet_GetAuthorizationCodeRequest req{};
    req.client_id = Bytes(client_id);
    req.scope = Bytes(scope);
    return SubmitRequest(CommandType::GetAuthorizationCode, MakeProtoPayload(req));
}

template <typename T>
static std::vector<u8> MakeComIdPayload(const std::string& com_id, const T& msg) {
    const u32 sz = static_cast<u32>(EncodedSize(msg));
    std::vector<u8> payload;
    payload.reserve(com_id.size() + 4 + sz);
    payload.insert(payload.end(), com_id.begin(), com_id.end());
    payload.push_back(static_cast<u8>(sz));
    payload.push_back(static_cast<u8>(sz >> 8));
    payload.push_back(static_cast<u8>(sz >> 16));
    payload.push_back(static_cast<u8>(sz >> 24));
    EncodeAppend(msg, payload);
    return payload;
}

std::string ShadNetClient::BuildVersionString() {
    const std::string remote_url(Common::g_scm_remote_url);
    const std::string remote_host = Common::GetRemoteNameFromLink();
    const bool official = (remote_host == "shadps4-emu" || remote_url.empty());

    if (Common::g_is_release) {
        return official ? Common::StringFromFormat("shadPS4 v%s", Common::g_version)
                        : Common::StringFromFormat("shadPS4 %s/v%s", (remote_host).c_str(),
                                                   Common::g_version);
    }
    return official ? Common::StringFromFormat("shadPS4 v%s %s %s", Common::g_version,
                                               Common::g_scm_branch, Common::g_scm_desc)
                    : Common::StringFromFormat("shadPS4 v%s %s/%s %s", Common::g_version,
                                               (remote_host).c_str(), Common::g_scm_branch,
                                               Common::g_scm_desc);
}

u64 ShadNetClient::ReportClientVersion() {
    const std::string version = BuildVersionString();
    shadnet_SetClientVersionRequest req{};
    req.version = Bytes(version);
    return SubmitRequest(CommandType::SetClientVersion, MakeProtoPayload(req));
}

u64 ShadNetClient::UnlockTrophy(const std::string& com_id, s32 trophy_id, u64 timestamp) {
    shadnet_UnlockTrophyRequest req{};
    req.trophyId = trophy_id;
    req.timestamp = timestamp;
    return SubmitRequest(CommandType::UnlockTrophy, MakeComIdPayload(com_id, req));
}

u64 ShadNetClient::SyncTrophies(const std::string& com_id,
                                const std::vector<std::pair<s32, u64>>& local_trophies) {
    std::vector<shadnet_TrophyEntry> entries(local_trophies.size());
    for (std::size_t i = 0; i < local_trophies.size(); ++i) {
        entries[i].trophyId = local_trophies[i].first;
        entries[i].timestamp = local_trophies[i].second;
    }
    shadnet_SyncTrophiesRequest req{};
    SetItems(req.trophies, entries);
    return SubmitRequest(CommandType::SyncTrophies, MakeComIdPayload(com_id, req));
}

bool ShadNetClient::IsTrophiesEnabled() const {
    return (retro_atomic_load_seq_cst_int(&m_trophies_enabled) != 0);
}

bool ShadNetClient::RequestServerFeatures() {
    const u64 pkt_id = static_cast<u64>(retro_atomic_fetch_add_64(&m_pkt_counter, 1));
    std::vector<u8> empty_payload;
    std::vector<u8> pkt = BuildPacket(CommandType::GetServerFeatures, pkt_id, empty_payload);
    QueuePacket(std::move(pkt));
    LOG_DEBUG(ShadNet, "GetServerFeatures request fired pkt_id=%" PRIu64, pkt_id);
    return true;
}

// Packet dispatch

void ShadNetClient::DispatchPacket(PacketType type, u16 cmd_raw, u64 pkt_id,
                                   const std::vector<u8>& payload) {
    switch (type) {
    case PacketType::Reply:
        switch (static_cast<CommandType>(cmd_raw)) {
        case CommandType::Login:
            HandleLoginReply(payload);
            break;
        case CommandType::GetToken:
            HandleGetTokenReply(payload);
            break;
        case CommandType::GetServerFeatures:
            HandleServerFeaturesReply(payload);
            break;
        default:
            if (onAsyncReply) {
                // Every reply body starts with an ErrorType byte.
                ErrorType err =
                    payload.empty() ? ErrorType::Malformed : static_cast<ErrorType>(payload[0]);
                std::vector<u8> body;
                if (payload.size() > 1) {
                    body.assign(payload.begin() + 1, payload.end());
                }
                onAsyncReply(static_cast<CommandType>(cmd_raw), pkt_id, err, body);
            } else {
                LOG_DEBUG(ShadNet, "Unhandled reply cmd=%u pkt_id=%" PRIu64, cmd_raw, pkt_id);
            }
            break;
        }
        break;
    case PacketType::Notification:
        HandleNotification(cmd_raw, payload);
        break;
    case PacketType::ServerInfo:
        LOG_DEBUG(ShadNet, "ServerInfo update received");
        break;
    case PacketType::Request:
        LOG_WARNING(ShadNet, "Unexpected Request from server");
        break;
    }
}

// Login reply

void ShadNetClient::HandleLoginReply(const std::vector<u8>& payload) {
    LoginResult res;

    if (payload.empty()) {
        res.error = ErrorType::Malformed;
        LOG_ERROR(ShadNet, "Empty Login reply");
    } else {
        res.error = static_cast<ErrorType>(payload[0]);

        if (res.error == ErrorType::NoError) {
            // payload[0]   = ErrorType byte
            // payload[1..] = u32 LE blob size + LoginReply proto bytes
            shadnet_LoginReply pb{};
            Scratch pb_scratch;
            const std::string blob = ExtractBlob(payload, 1);
            if (!blob.empty() && Decode(pb, blob, pb_scratch)) {
                res.avatarUrl = Str(pb.avatar_url);
                res.userId = pb.user_id;
                for (const auto& f : Items(pb.friends)) {
                    FriendEntry fe;
                    fe.npid = Str(f.npid);
                    fe.online = f.online;
                    res.friends.push_back(std::move(fe));
                }
                for (const auto& n : Items(pb.friend_requests_sent))
                    res.requestsSent.push_back(Str(n));
                for (const auto& n : Items(pb.friend_requests_received))
                    res.requestsReceived.push_back(Str(n));
                for (const auto& n : Items(pb.blocked))
                    res.blocked.push_back(Str(n));

                m_avatar_url = res.avatarUrl;
                m_user_id = res.userId;
                m_mutex_friends.Run([&] { m_friends = res.friends; });
                (void)retro_atomic_exchange_int(&m_authenticated, 1);
                LOG_INFO(ShadNet, "Logged in npid='%s' userId=%" PRIu64 " friends=%zu",
                         (m_npid).c_str(), m_user_id, m_friends.size());

                const u64 pkt_id = static_cast<u64>(retro_atomic_fetch_add_64(&m_pkt_counter, 1));
                std::vector<u8> empty_payload;
                std::vector<u8> pkt = BuildPacket(CommandType::GetToken, pkt_id, empty_payload);
                QueuePacket(std::move(pkt));
                LOG_DEBUG(ShadNet, "GetToken request fired pkt_id=%" PRIu64, pkt_id);
            } else {
                res.error = ErrorType::Malformed;
                LOG_ERROR(ShadNet, "Failed to parse LoginReply proto");
                (void)retro_atomic_exchange_int(&m_state,
                                                static_cast<int>(ShadNetState::FailureProtocol));
                event_latch_set(&m_login_settled);
                DoDisconnect();
            }
        } else {
            switch (res.error) {
            case ErrorType::LoginAlreadyLoggedIn:
                (void)retro_atomic_exchange_int(&m_state,
                                                static_cast<int>(ShadNetState::FailureAlreadyIn));
                break;
            case ErrorType::LoginInvalidUsername:
                (void)retro_atomic_exchange_int(&m_state,
                                                static_cast<int>(ShadNetState::FailureUsername));
                break;
            case ErrorType::LoginInvalidPassword:
                (void)retro_atomic_exchange_int(&m_state,
                                                static_cast<int>(ShadNetState::FailurePassword));
                break;
            case ErrorType::LoginInvalidToken:
                (void)retro_atomic_exchange_int(&m_state,
                                                static_cast<int>(ShadNetState::FailureToken));
                break;
            default:
                (void)retro_atomic_exchange_int(&m_state,
                                                static_cast<int>(ShadNetState::FailureAuth));
                break;
            }
            LOG_ERROR(ShadNet, "Login rejected error code %u", static_cast<u8>(res.error));
            event_latch_set(&m_login_settled);
            DoDisconnect();
        }
    }

    if (onLoginResult)
        onLoginResult(res);
}

void ShadNetClient::HandleGetTokenReply(const std::vector<u8>& payload) {
    if (payload.empty()) {
        LOG_ERROR(ShadNet, "Empty GetToken reply");
        RequestServerFeatures();
        return;
    }
    const ErrorType err = static_cast<ErrorType>(payload[0]);
    if (err != ErrorType::NoError) {
        LOG_WARNING(ShadNet, "GetToken returned error=%d — WebAPI calls will be unauthenticated",
                    static_cast<int>(err));
        RequestServerFeatures();
        return;
    }
    shadnet_GetTokenReply pb{};
    Scratch pb_scratch;
    const std::string blob = ExtractBlob(payload, 1);
    if (blob.empty() || !Decode(pb, blob, pb_scratch)) {
        LOG_ERROR(ShadNet, "Failed to parse GetTokenReply proto");
        RequestServerFeatures();
        return;
    }
    m_mutex_bearer.Run([&] { m_bearer_token = Str(pb.token); });
    LOG_INFO(ShadNet,
             "Bearer token captured (%zu chars) for accountID=%" PRIu64 " canonical npid='%.*s'",
             pb.token.size, pb.user_id, static_cast<int>((View(pb.npid)).size()),
             (View(pb.npid)).data());
    RequestServerFeatures();
}

void ShadNetClient::HandleServerFeaturesReply(const std::vector<u8>& payload) {
    bool matching2_enabled = false;
    bool trophies_enabled = false;
    bool parsed = false;

    if (!payload.empty()) {
        const ErrorType err = static_cast<ErrorType>(payload[0]);
        if (err == ErrorType::NoError) {
            shadnet_ServerFeaturesReply pb{};
            Scratch pb_scratch;
            const std::string blob = ExtractBlob(payload, 1);
            if (!blob.empty() && Decode(pb, blob, pb_scratch)) {
                matching2_enabled = pb.matching2_enabled;
                trophies_enabled = pb.trophies_enabled;
                parsed = true;
            }
        } else {
            LOG_WARNING(ShadNet,
                        "GetServerFeatures returned error=%d - assuming Matching2 disabled",
                        static_cast<int>(err));
        }
    }

    (void)retro_atomic_exchange_int(&m_matching2_enabled, matching2_enabled);
    (void)retro_atomic_exchange_int(&m_trophies_enabled, trophies_enabled);
    (void)retro_atomic_exchange_int(&m_server_features_received, parsed);
    ReportClientVersion();
    LOG_INFO(ShadNet, "Server features: matching2_enabled=%s trophies_enabled=%s%s",
             matching2_enabled ? "true" : "false", trophies_enabled ? "true" : "false",
             parsed ? "" : " (defaulted)");
    event_latch_set(&m_login_settled);
}

// Notifications

void ShadNetClient::HandleNotification(u16 cmd_raw, const std::vector<u8>& payload) {
    // Notification payload = u32 LE blob size + proto bytes
    const std::string blob = ExtractBlob(payload, 0);
    if (blob.empty() &&
        static_cast<NotificationType>(cmd_raw) != NotificationType::WebApiPushEvent) {
        // WebApiPushEvent is multi-field,its first field (service name) may be empty
        // legitimately, so it parses its own payload below rather than relying on blob.
        LOG_WARNING(ShadNet, "Empty notification payload type=%u", cmd_raw);
        return;
    }

    switch (static_cast<NotificationType>(cmd_raw)) {
    case NotificationType::FriendQuery: {
        shadnet_NotifyFriendQuery pb{};
        Scratch pb_scratch;
        if (!Decode(pb, blob, pb_scratch)) {
            LOG_WARNING(ShadNet, "FriendQuery parse error");
            break;
        }
        NotifyFriendQuery n;
        n.fromNpid = Str(pb.from_npid);
        LOG_DEBUG(ShadNet, "FriendQuery from '%s'", (n.fromNpid).c_str());
        if (onFriendQuery)
            onFriendQuery(n);
        break;
    }
    case NotificationType::FriendNew: {
        shadnet_NotifyFriendNew pb{};
        Scratch pb_scratch;
        if (!Decode(pb, blob, pb_scratch)) {
            LOG_WARNING(ShadNet, "FriendNew parse error");
            break;
        }
        NotifyFriendNew n;
        n.npid = Str(pb.npid);
        n.online = pb.online;
        LOG_DEBUG(ShadNet, "FriendNew '%s' (%s)", (n.npid).c_str(),
                  n.online ? "online" : "offline");
        if (onFriendNew)
            onFriendNew(n);
        break;
    }
    case NotificationType::FriendLost: {
        shadnet_NotifyFriendLost pb{};
        Scratch pb_scratch;
        if (!Decode(pb, blob, pb_scratch)) {
            LOG_WARNING(ShadNet, "FriendLost parse error");
            break;
        }
        NotifyFriendLost n;
        n.npid = Str(pb.npid);
        LOG_DEBUG(ShadNet, "FriendLost '%s'", (n.npid).c_str());
        if (onFriendLost)
            onFriendLost(n);
        break;
    }
    case NotificationType::FriendStatus: {
        shadnet_NotifyFriendStatus pb{};
        Scratch pb_scratch;
        if (!Decode(pb, blob, pb_scratch)) {
            LOG_WARNING(ShadNet, "FriendStatus parse error");
            break;
        }
        NotifyFriendStatus n;
        n.npid = Str(pb.npid);
        n.online = pb.online;
        n.timestamp = pb.timestamp;
        LOG_DEBUG(ShadNet, "FriendStatus '%s' is %s", (n.npid).c_str(),
                  n.online ? "online" : "offline");
        if (onFriendStatus)
            onFriendStatus(n);
        break;
    }
    case NotificationType::RoomEvent: {
        shadnet_NotifyRoomEvent pb{};
        Scratch pb_scratch;
        if (!Decode(pb, blob, pb_scratch)) {
            LOG_WARNING(ShadNet, "RoomEvent parse error");
            break;
        }
        NotifyRoomEvent n;
        n.ctx_id = pb.ctx_id;
        n.room_id = pb.room_id;
        n.event = pb.event;
        n.event_cause = pb.event_cause;
        n.error_code = pb.error_code;
        n.flags = pb.flags;
        n.has_passwd_mask = pb.has_passwd_mask;
        n.passwd_slot_mask = pb.passwd_slot_mask;
        if (pb.has_member) {
            const auto& m = pb.member;
            n.member_npid = Str(m.npid);
            n.member_account_id = m.account_id;
            n.member_platform = m.platform;
            n.member_id = m.member_id;
            n.member_team_id = m.team_id;
            n.member_is_owner = m.is_owner;
            n.member_join_date = m.join_date;
            n.member_nat_type = m.nat_type;
            n.member_flag_attr = m.flag_attr;
            n.member_group_id = m.group_id;
            n.member_addr = Str(m.addr);
            n.member_port = m.port;
            for (const auto& a : Items(m.bin_attrs_internal)) {
                MatchingBinAttr ba;
                ba.attr_id = a.attr_id;
                ba.update_date = a.update_date;
                ba.update_member_id = a.update_member_id;
                ba.data.assign(a.data.data, a.data.data + a.data.size);
                n.member_bin_attrs.push_back(std::move(ba));
            }
        }
        for (const auto& a : Items(pb.bin_attrs)) {
            MatchingBinAttr ba;
            ba.attr_id = a.attr_id;
            ba.update_date = a.update_date;
            ba.update_member_id = a.update_member_id;
            ba.data.assign(a.data.data, a.data.data + a.data.size);
            n.bin_attrs.push_back(std::move(ba));
        }
        LOG_DEBUG(ShadNet, "RoomEvent room_id=%" PRIu64 " event=0x%x cause=%u", n.room_id, n.event,
                  n.event_cause);
        if (onRoomEvent)
            onRoomEvent(n);
        break;
    }
    case NotificationType::RoomMessage: {
        shadnet_NotifyRoomMessage pb{};
        Scratch pb_scratch;
        if (!Decode(pb, blob, pb_scratch)) {
            LOG_WARNING(ShadNet, "RoomMessage parse error");
            break;
        }
        NotifyRoomMessage n;
        n.ctx_id = pb.ctx_id;
        n.room_id = pb.room_id;
        n.src_member_id = pb.src_member_id;
        n.event = pb.event;
        n.cast_type = pb.cast_type;
        for (const u32 member_id : Items(pb.dst_member_ids)) {
            n.dst_member_ids.push_back(member_id);
        }
        n.src_npid = Str(pb.src_npid);
        n.src_account_id = pb.src_account_id;
        n.src_platform = pb.src_platform;
        n.msg.assign(pb.msg.data, pb.msg.data + pb.msg.size);
        LOG_DEBUG(ShadNet, "RoomMessage room_id=%" PRIu64 " src=%u event=0x%x bytes=%zu", n.room_id,
                  n.src_member_id, n.event, n.msg.size());
        if (onRoomMessage)
            onRoomMessage(n);
        break;
    }
    case NotificationType::WebApiPushEvent: {
        // Length-prefixed fields: npServiceName, npServiceLabel(u32 LE), dataType,
        // data, fromNpid, toNpid. ExtractBlob returns {} past the end, so a short
        // payload degrades to empty trailing fields rather than reading OOB.
        NotifyWebApiPushEvent n;
        int off = 0;
        n.npServiceName = ExtractBlob(payload, off);
        off += 4 + static_cast<int>(n.npServiceName.size());
        if (off + 4 <= static_cast<int>(payload.size())) {
            n.npServiceLabel = GetLE32(payload.data() + off);
            off += 4;
        }
        n.dataType = ExtractBlob(payload, off);
        off += 4 + static_cast<int>(n.dataType.size());
        n.data = ExtractBlob(payload, off);
        off += 4 + static_cast<int>(n.data.size());
        n.fromNpid = ExtractBlob(payload, off);
        off += 4 + static_cast<int>(n.fromNpid.size());
        n.toNpid = ExtractBlob(payload, off);
        off += 4 + static_cast<int>(n.toNpid.size());
        // Optional extended-data section appended after toNpid: u32 LE count, then
        // (blob key, blob value) per pair. Absent on older servers -> no bytes left ->
        // zero pairs. Length-guarded throughout; count capped to avoid runaway on a
        // malformed packet.
        if (off + 4 <= static_cast<int>(payload.size())) {
            const u32 count = GetLE32(payload.data() + off);
            off += 4;
            bool extd_complete = count <= 256;
            for (u32 i = 0; i < count && i < 256; ++i) {
                if (off + 4 > static_cast<int>(payload.size())) {
                    extd_complete = false;
                    break;
                }
                std::string key = ExtractBlob(payload, off);
                off += 4 + static_cast<int>(key.size());
                if (off + 4 > static_cast<int>(payload.size())) {
                    extd_complete = false;
                    break;
                }
                std::string val = ExtractBlob(payload, off);
                off += 4 + static_cast<int>(val.size());
                n.extdData.emplace_back(std::move(key), std::move(val));
            }
            if (extd_complete && off + 16 <= static_cast<int>(payload.size())) {
                n.fromAccountId = GetLE64(payload.data() + off);
                n.toAccountId = GetLE64(payload.data() + off + 8);
            }
        }
        LOG_INFO(ShadNet,
                 "WebApiPushEvent svc='%s' type='%s' from='%s'(%" PRIu64 ") to='%s'(%" PRIu64
                 ") bytes=%zu extd=%zu",
                 (n.npServiceName).c_str(), (n.dataType).c_str(), (n.fromNpid).c_str(),
                 n.fromAccountId, (n.toNpid).c_str(), n.toAccountId, n.data.size(),
                 n.extdData.size());
        if (onWebApiPushEvent)
            onWebApiPushEvent(n);
        break;
    }
    case NotificationType::ServiceUnavailable: {
        shadnet_NotifyServiceUnavailable pb{};
        Scratch pb_scratch;
        if (!Decode(pb, blob, pb_scratch)) {
            LOG_WARNING(ShadNet, "ServiceUnavailable parse error");
            break;
        }
        NotifyServiceUnavailable n;
        n.service = pb.service;
        n.reason = pb.reason;
        n.inUse = pb.in_use;
        n.limit = pb.limit;
        LOG_INFO(ShadNet, "ServiceUnavailable service=%u reason=%u in_use=%u limit=%u", n.service,
                 n.reason, n.inUse, n.limit);
        if (onServiceUnavailable)
            onServiceUnavailable(n);
        break;
    }
    case NotificationType::ClientRejected: {
        shadnet_NotifyClientRejected pb{};
        Scratch pb_scratch;
        if (!Decode(pb, blob, pb_scratch)) {
            LOG_WARNING(ShadNet, "ClientRejected parse error");
            break;
        }
        NotifyClientRejected n;
        n.reason = pb.reason;
        n.message = Str(pb.message);
        n.version = Str(pb.version);
        LOG_ERROR(ShadNet, "Server rejected this client (reason=%u, version='%s'): %s", n.reason,
                  (n.version).c_str(), (n.message).c_str());
        if (onClientRejected)
            onClientRejected(n);
        break;
    }
    default:
        LOG_DEBUG(ShadNet, "Unknown notification type %u", cmd_raw);
        break;
    }
}

void ShadNetClient::PutLE16(std::vector<u8>& b, size_t off, u16 v) {
    b[off] = static_cast<u8>(v);
    b[off + 1] = static_cast<u8>(v >> 8);
}
void ShadNetClient::PutLE32(std::vector<u8>& b, size_t off, u32 v) {
    b[off] = static_cast<u8>(v);
    b[off + 1] = static_cast<u8>(v >> 8);
    b[off + 2] = static_cast<u8>(v >> 16);
    b[off + 3] = static_cast<u8>(v >> 24);
}
void ShadNetClient::PutLE64(std::vector<u8>& b, size_t off, u64 v) {
    for (int i = 0; i < 8; ++i)
        b[off + i] = static_cast<u8>(v >> (8 * i));
}
u16 ShadNetClient::GetLE16(const u8* p) {
    return static_cast<u16>(p[0]) | (static_cast<u16>(p[1]) << 8);
}
u32 ShadNetClient::GetLE32(const u8* p) {
    return static_cast<u32>(p[0]) | (static_cast<u32>(p[1]) << 8) | (static_cast<u32>(p[2]) << 16) |
           (static_cast<u32>(p[3]) << 24);
}
u64 ShadNetClient::GetLE64(const u8* p) {
    u64 v = 0;
    for (int i = 0; i < 8; ++i)
        v |= static_cast<u64>(p[i]) << (8 * i);
    return v;
}

} // namespace ShadNet
