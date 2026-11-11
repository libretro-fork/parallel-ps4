// SPDX-FileCopyrightText: Copyright 2019-2026 rpcs3 Project
// SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <algorithm>
#include <cstring>
#include "common/logging/log.h"
#include "common/string_util.h"
#include "core/libraries/np/np_error.h"
#include "core/libraries/np/np_handler/np_handler.h"
#include "shadnet/messages.h"

using ShadNet::Bytes;
using ShadNet::Decode;
using ShadNet::Encode;
using ShadNet::Items;
using ShadNet::Scratch;
using ShadNet::SetItems;
using ShadNet::Str;
using ShadNet::View;

namespace Libraries::Np {

std::vector<u8> BuildTusPayload(const std::string& com_id, const std::string& proto_bytes) {
    std::vector<u8> payload;
    payload.reserve(12 + 4 + proto_bytes.size());
    payload.insert(payload.end(), com_id.begin(), com_id.end());
    const u32 sz = static_cast<u32>(proto_bytes.size());
    payload.push_back(static_cast<u8>(sz));
    payload.push_back(static_cast<u8>(sz >> 8));
    payload.push_back(static_cast<u8>(sz >> 16));
    payload.push_back(static_cast<u8>(sz >> 24));
    payload.insert(payload.end(), proto_bytes.begin(), proto_bytes.end());
    return payload;
}

void CopyNpHandle(OrbisNpId& dst, std::string_view src) {
    dst = OrbisNpId{};
    const size_t cp = std::min(src.size(), sizeof(dst.handle.data) - 1);
    std::memcpy(dst.handle.data, src.data(), cp);
}

s32 NpHandler::TusGetMultiSlotVariable(
    s32 user_id, s32 service_label, const std::string& ownerNpId, const std::string& virtualUser,
    const std::vector<s32>& slotIds, NpTus::OrbisNpTusVariable* variablesOut, u64 arrayNum,
    std::shared_ptr<NpTus::TusRequestCtx> ctx, NpTus::OrbisNpTusVariableA* variablesAOut,
    s64 ownerAccountId, NpTus::OrbisNpTusVariableForCrossSave* variablesCSOut) {
    std::shared_ptr<ShadNet::ShadNetClient> client;
    if (auto returned = m_mutex_clients.Run([&]() -> std::optional<s32> {
            auto it = m_clients.find(user_id);
            if (it == m_clients.end()) {
                return std::optional<s32>{ORBIS_NP_ERROR_SIGNED_OUT};
            }
            client = it->second;
            return std::nullopt;
        })) {
        return *returned;
    }
    shadnet_TusGetMultiSlotVariableRequest proto{};
    ShadNet::Builder proto_build;
    proto.ownerNpId = Bytes(ownerNpId);
    if (!virtualUser.empty()) {
        proto.virtualUser = Bytes(virtualUser);
    } else if (ownerAccountId != 0) {
        proto.ownerAccountId = ownerAccountId;
    } else if (ownerNpId.empty() ||
               Common::ToLower(ownerNpId) == Common::ToLower(client->GetNpid())) {
        proto.ownerAccountId = static_cast<s64>(client->GetUserId());
    }
    for (s32 s : slotIds) {
        proto_build.Push(proto.slotIds, s);
    }
    const std::string com_id = GetNpCommId(service_label);
    if (!IsValidNpCommId(com_id)) {
        return ORBIS_NP_COMMUNITY_ERROR_INVALID_ARGUMENT;
    }
    const u64 pkt_id = client->SubmitRequest(ShadNet::CommandType::TusGetMultiSlotVariable,
                                             BuildTusPayload(com_id, Encode(proto)));
    return m_mutex_pending_tus.Run([&]() -> s32 {
        PendingTusRequest p;
        p.req = std::move(ctx);
        p.cmd = ShadNet::CommandType::TusGetMultiSlotVariable;
        p.variableArray = variablesOut;
        p.variableArrayA = variablesAOut;
        p.variableArrayCS = variablesCSOut;
        p.arrayNum = arrayNum;
        p.user_id = user_id;
        m_pending_tus.emplace(pkt_id, std::move(p));
        return ORBIS_OK;
    });
}

s32 NpHandler::TusSetMultiSlotVariable(s32 user_id, s32 service_label, const std::string& ownerNpId,
                                       const std::string& virtualUser,
                                       const std::vector<s32>& slotIds,
                                       const std::vector<s64>& values,
                                       std::shared_ptr<NpTus::TusRequestCtx> ctx,
                                       s64 ownerAccountId) {
    std::shared_ptr<ShadNet::ShadNetClient> client;
    if (auto returned = m_mutex_clients.Run([&]() -> std::optional<s32> {
            auto it = m_clients.find(user_id);
            if (it == m_clients.end()) {
                return std::optional<s32>{ORBIS_NP_ERROR_SIGNED_OUT};
            }
            client = it->second;
            return std::nullopt;
        })) {
        return *returned;
    }
    shadnet_TusSetMultiSlotVariableRequest proto{};
    ShadNet::Builder proto_build;
    proto.ownerNpId = Bytes(ownerNpId);
    if (!virtualUser.empty()) {
        proto.virtualUser = Bytes(virtualUser);
    } else if (ownerAccountId != 0) {
        proto.ownerAccountId = ownerAccountId;
    } else if (ownerNpId.empty() ||
               Common::ToLower(ownerNpId) == Common::ToLower(client->GetNpid())) {
        proto.ownerAccountId = static_cast<s64>(client->GetUserId());
    }

    for (s32 s : slotIds) {
        proto_build.Push(proto.slotIds, s);
    }
    for (s64 v : values) {
        proto_build.Push(proto.values, v);
    }
    const std::string com_id = GetNpCommId(service_label);
    if (!IsValidNpCommId(com_id)) {
        return ORBIS_NP_COMMUNITY_ERROR_INVALID_ARGUMENT;
    }
    const u64 pkt_id = client->SubmitRequest(ShadNet::CommandType::TusSetMultiSlotVariable,
                                             BuildTusPayload(com_id, Encode(proto)));
    return m_mutex_pending_tus.Run([&]() -> s32 {
        PendingTusRequest p;
        p.req = std::move(ctx);
        p.cmd = ShadNet::CommandType::TusSetMultiSlotVariable;
        p.user_id = user_id;
        m_pending_tus.emplace(pkt_id, std::move(p));
        return ORBIS_OK;
    });
}

s32 NpHandler::TssGetData(s32 user_id, s32 service_label, s32 slotId, bool hasOffset, u64 offset,
                          bool hasLastByte, u64 lastByte, bool hasIfParam, s32 ifType,
                          u64 ifLastModified, NpTus::OrbisNpTssDataStatus* statusOut, void* dataOut,
                          u64 dataCap, std::shared_ptr<NpTus::TusRequestCtx> ctx,
                          u64* contentLengthOut) {
    std::shared_ptr<ShadNet::ShadNetClient> client;
    if (auto returned = m_mutex_clients.Run([&]() -> std::optional<s32> {
            auto it = m_clients.find(user_id);
            if (it == m_clients.end()) {
                return std::optional<s32>{ORBIS_NP_ERROR_SIGNED_OUT};
            }
            client = it->second;
            return std::nullopt;
        })) {
        return *returned;
    }
    shadnet_TssGetDataRequest proto{};
    proto.slotId = slotId;
    if (hasOffset) {
        proto.hasOffset = true;
        proto.offset = offset;
    }
    if (hasLastByte) {
        proto.hasLastByte = true;
        proto.lastByte = lastByte;
    }
    if (hasIfParam) {
        proto.hasIfParam = true;
        proto.ifType = ifType;
        proto.ifLastModified = ifLastModified;
    }
    const std::string com_id = GetNpCommId(service_label);
    if (!IsValidNpCommId(com_id)) {
        return ORBIS_NP_COMMUNITY_ERROR_INVALID_ARGUMENT;
    }
    const u64 pkt_id = client->SubmitRequest(ShadNet::CommandType::TssGetData,
                                             BuildTusPayload(com_id, Encode(proto)));
    return m_mutex_pending_tus.Run([&]() -> s32 {
        PendingTusRequest p;
        p.req = std::move(ctx);
        p.cmd = ShadNet::CommandType::TssGetData;
        p.user_id = user_id;
        p.tssStatusOut = statusOut;
        p.tssContentLengthOut = contentLengthOut;
        p.dataOut = dataOut;
        p.dataCap = dataCap;
        m_pending_tus.emplace(pkt_id, std::move(p));
        return ORBIS_OK;
    });
}

s32 NpHandler::TusGetData(s32 user_id, s32 service_label, const std::string& ownerNpId,
                          const std::string& virtualUser, s64 ownerAccountId, s32 slotId,
                          NpTus::OrbisNpTusDataStatusA* statusAOut, u64 statusCap, void* dataOut,
                          u64 dataCap, u64 dataOffset, std::shared_ptr<NpTus::TusRequestCtx> ctx,
                          NpTus::OrbisNpTusDataStatus* statusOut,
                          NpTus::OrbisNpTusDataStatusForCrossSave* statusCSOut) {
    std::shared_ptr<ShadNet::ShadNetClient> client;
    if (auto returned = m_mutex_clients.Run([&]() -> std::optional<s32> {
            auto it = m_clients.find(user_id);
            if (it == m_clients.end()) {
                return std::optional<s32>{ORBIS_NP_ERROR_SIGNED_OUT};
            }
            client = it->second;
            return std::nullopt;
        })) {
        return *returned;
    }
    shadnet_TusGetDataRequest proto{};
    proto.ownerNpId = Bytes(ownerNpId);
    proto.slotId = slotId;
    // Same targeting precedence as the variable paths.
    if (!virtualUser.empty()) {
        proto.virtualUser = Bytes(virtualUser);
    } else if (ownerAccountId != 0) {
        proto.ownerAccountId = ownerAccountId;
    } else if (ownerNpId.empty() ||
               Common::ToLower(ownerNpId) == Common::ToLower(client->GetNpid())) {
        proto.ownerAccountId = static_cast<s64>(client->GetUserId());
    }
    const std::string com_id = GetNpCommId(service_label);
    if (!IsValidNpCommId(com_id)) {
        return ORBIS_NP_COMMUNITY_ERROR_INVALID_ARGUMENT;
    }
    const u64 pkt_id = client->SubmitRequest(ShadNet::CommandType::TusGetData,
                                             BuildTusPayload(com_id, Encode(proto)));
    return m_mutex_pending_tus.Run([&]() -> s32 {
        PendingTusRequest p;
        p.req = std::move(ctx);
        p.cmd = ShadNet::CommandType::TusGetData;
        p.user_id = user_id;
        p.statusArrayA = statusAOut;
        p.statusArray = statusOut;
        p.statusArrayCS = statusCSOut;
        p.statusCap = statusCap;
        p.dataOut = dataOut;
        p.dataCap = dataCap;
        p.dataOffset = dataOffset;
        p.arrayNum = 1;
        m_pending_tus.emplace(pkt_id, std::move(p));
        return ORBIS_OK;
    });
}

s32 NpHandler::TusDeleteMultiSlotData(s32 user_id, s32 service_label, const std::string& ownerNpId,
                                      const std::string& virtualUser, s64 ownerAccountId,
                                      const std::vector<s32>& slotIds,
                                      std::shared_ptr<NpTus::TusRequestCtx> ctx) {
    std::shared_ptr<ShadNet::ShadNetClient> client;
    if (auto returned = m_mutex_clients.Run([&]() -> std::optional<s32> {
            auto it = m_clients.find(user_id);
            if (it == m_clients.end()) {
                return std::optional<s32>{ORBIS_NP_ERROR_SIGNED_OUT};
            }
            client = it->second;
            return std::nullopt;
        })) {
        return *returned;
    }
    shadnet_TusDeleteMultiSlotDataRequest proto{};
    ShadNet::Builder proto_build;
    proto.ownerNpId = Bytes(ownerNpId);
    if (!virtualUser.empty()) {
        proto.virtualUser = Bytes(virtualUser);
    } else if (ownerAccountId != 0) {
        proto.ownerAccountId = ownerAccountId;
    } else if (ownerNpId.empty() ||
               Common::ToLower(ownerNpId) == Common::ToLower(client->GetNpid())) {
        proto.ownerAccountId = static_cast<s64>(client->GetUserId());
    }
    for (s32 s : slotIds) {
        proto_build.Push(proto.slotIds, s);
    }
    const std::string com_id = GetNpCommId(service_label);
    if (!IsValidNpCommId(com_id)) {
        return ORBIS_NP_COMMUNITY_ERROR_INVALID_ARGUMENT;
    }
    const u64 pkt_id = client->SubmitRequest(ShadNet::CommandType::TusDeleteMultiSlotData,
                                             BuildTusPayload(com_id, Encode(proto)));
    return m_mutex_pending_tus.Run([&]() -> s32 {
        PendingTusRequest p;
        p.req = std::move(ctx);
        p.cmd = ShadNet::CommandType::TusDeleteMultiSlotData;
        p.user_id = user_id;
        m_pending_tus.emplace(pkt_id, std::move(p));
        return ORBIS_OK;
    });
}

s32 NpHandler::TusDeleteMultiSlotVariable(s32 user_id, s32 service_label,
                                          const std::string& ownerNpId,
                                          const std::string& virtualUser, s64 ownerAccountId,
                                          const std::vector<s32>& slotIds,
                                          std::shared_ptr<NpTus::TusRequestCtx> ctx) {
    std::shared_ptr<ShadNet::ShadNetClient> client;
    if (auto returned = m_mutex_clients.Run([&]() -> std::optional<s32> {
            auto it = m_clients.find(user_id);
            if (it == m_clients.end()) {
                return std::optional<s32>{ORBIS_NP_ERROR_SIGNED_OUT};
            }
            client = it->second;
            return std::nullopt;
        })) {
        return *returned;
    }
    shadnet_TusDeleteMultiSlotVariableRequest proto{};
    ShadNet::Builder proto_build;
    proto.ownerNpId = Bytes(ownerNpId);
    if (!virtualUser.empty()) {
        proto.virtualUser = Bytes(virtualUser);
    } else if (ownerAccountId != 0) {
        proto.ownerAccountId = ownerAccountId;
    } else if (ownerNpId.empty() ||
               Common::ToLower(ownerNpId) == Common::ToLower(client->GetNpid())) {
        proto.ownerAccountId = static_cast<s64>(client->GetUserId());
    }
    for (s32 s : slotIds) {
        proto_build.Push(proto.slotIds, s);
    }
    const std::string com_id = GetNpCommId(service_label);
    if (!IsValidNpCommId(com_id)) {
        return ORBIS_NP_COMMUNITY_ERROR_INVALID_ARGUMENT;
    }
    const u64 pkt_id = client->SubmitRequest(ShadNet::CommandType::TusDeleteMultiSlotVariable,
                                             BuildTusPayload(com_id, Encode(proto)));
    return m_mutex_pending_tus.Run([&]() -> s32 {
        PendingTusRequest p;
        p.req = std::move(ctx);
        p.cmd = ShadNet::CommandType::TusDeleteMultiSlotVariable;
        p.user_id = user_id;
        m_pending_tus.emplace(pkt_id, std::move(p));
        return ORBIS_OK;
    });
}

s32 NpHandler::TusGetMultiUserDataStatus(s32 user_id, s32 service_label, s32 slotId,
                                         const std::vector<std::string>& ownerNpIds,
                                         const std::vector<std::string>& virtualUsers,
                                         const std::vector<s64>& ownerAccountIds,
                                         NpTus::OrbisNpTusDataStatus* statusOut,
                                         NpTus::OrbisNpTusDataStatusA* statusAOut, u64 arrayNum,
                                         std::shared_ptr<NpTus::TusRequestCtx> ctx,
                                         NpTus::OrbisNpTusDataStatusForCrossSave* statusCSOut) {
    std::shared_ptr<ShadNet::ShadNetClient> client;
    if (auto returned = m_mutex_clients.Run([&]() -> std::optional<s32> {
            auto it = m_clients.find(user_id);
            if (it == m_clients.end()) {
                return std::optional<s32>{ORBIS_NP_ERROR_SIGNED_OUT};
            }
            client = it->second;
            return std::nullopt;
        })) {
        return *returned;
    }
    shadnet_TusGetMultiUserDataStatusRequest proto{};
    ShadNet::Builder proto_build;
    proto.slotId = slotId;
    for (const auto& vu : virtualUsers) {
        proto_build.Push(proto.virtualUsers, vu);
    }
    for (const auto& np : ownerNpIds) {
        proto_build.Push(proto.ownerNpIds, np);
    }
    for (s64 acc : ownerAccountIds) {
        proto_build.Push(proto.ownerAccountIds, acc);
    }
    const std::string com_id = GetNpCommId(service_label);
    if (!IsValidNpCommId(com_id)) {
        return ORBIS_NP_COMMUNITY_ERROR_INVALID_ARGUMENT;
    }
    const u64 pkt_id = client->SubmitRequest(ShadNet::CommandType::TusGetMultiUserDataStatus,
                                             BuildTusPayload(com_id, Encode(proto)));
    return m_mutex_pending_tus.Run([&]() -> s32 {
        PendingTusRequest p;
        p.req = std::move(ctx);
        p.cmd = ShadNet::CommandType::TusGetMultiUserDataStatus;
        p.user_id = user_id;
        p.statusArray = statusOut;
        p.statusArrayA = statusAOut;
        p.statusArrayCS = statusCSOut;
        p.arrayNum = arrayNum;
        m_pending_tus.emplace(pkt_id, std::move(p));
        return ORBIS_OK;
    });
}

s32 NpHandler::TusGetMultiUserVariable(s32 user_id, s32 service_label, s32 slotId,
                                       const std::vector<std::string>& ownerNpIds,
                                       const std::vector<std::string>& virtualUsers,
                                       const std::vector<s64>& ownerAccountIds,
                                       NpTus::OrbisNpTusVariable* variablesOut,
                                       NpTus::OrbisNpTusVariableA* variablesAOut, u64 arrayNum,
                                       std::shared_ptr<NpTus::TusRequestCtx> ctx,
                                       NpTus::OrbisNpTusVariableForCrossSave* variablesCSOut) {
    std::shared_ptr<ShadNet::ShadNetClient> client;
    if (auto returned = m_mutex_clients.Run([&]() -> std::optional<s32> {
            auto it = m_clients.find(user_id);
            if (it == m_clients.end()) {
                return std::optional<s32>{ORBIS_NP_ERROR_SIGNED_OUT};
            }
            client = it->second;
            return std::nullopt;
        })) {
        return *returned;
    }
    shadnet_TusGetMultiUserVariableRequest proto{};
    ShadNet::Builder proto_build;
    proto.slotId = slotId;
    for (const auto& vu : virtualUsers) {
        proto_build.Push(proto.virtualUsers, vu);
    }
    for (const auto& np : ownerNpIds) {
        proto_build.Push(proto.ownerNpIds, np);
    }
    for (s64 acc : ownerAccountIds) {
        proto_build.Push(proto.ownerAccountIds, acc);
    }
    const std::string com_id = GetNpCommId(service_label);
    if (!IsValidNpCommId(com_id)) {
        return ORBIS_NP_COMMUNITY_ERROR_INVALID_ARGUMENT;
    }
    const u64 pkt_id = client->SubmitRequest(ShadNet::CommandType::TusGetMultiUserVariable,
                                             BuildTusPayload(com_id, Encode(proto)));
    return m_mutex_pending_tus.Run([&]() -> s32 {
        PendingTusRequest p;
        p.req = std::move(ctx);
        p.cmd = ShadNet::CommandType::TusGetMultiUserVariable;
        p.user_id = user_id;
        p.variableArray = variablesOut;
        p.variableArrayA = variablesAOut;
        p.variableArrayCS = variablesCSOut;
        p.arrayNum = arrayNum;
        m_pending_tus.emplace(pkt_id, std::move(p));
        return ORBIS_OK;
    });
}

s32 NpHandler::TusTryAndSetVariable(s32 user_id, s32 service_label, const std::string& ownerNpId,
                                    const std::string& virtualUser, s64 ownerAccountId, s32 slotId,
                                    s32 opeType, s64 value, bool hasCompare, s64 compareValue,
                                    bool hasAuthorCheck, s64 isLastChangedAuthor,
                                    const std::string& isLastChangedAuthorNpId, bool hasDateCheck,
                                    u64 isLastChangedDate, NpTus::OrbisNpTusVariable* variableOut,
                                    NpTus::OrbisNpTusVariableA* variableAOut,
                                    std::shared_ptr<NpTus::TusRequestCtx> ctx,
                                    NpTus::OrbisNpTusVariableForCrossSave* variableCSOut) {
    std::shared_ptr<ShadNet::ShadNetClient> client;
    if (auto returned = m_mutex_clients.Run([&]() -> std::optional<s32> {
            auto it = m_clients.find(user_id);
            if (it == m_clients.end()) {
                return std::optional<s32>{ORBIS_NP_ERROR_SIGNED_OUT};
            }
            client = it->second;
            return std::nullopt;
        })) {
        return *returned;
    }
    shadnet_TusTryAndSetVariableRequest proto{};
    proto.ownerNpId = Bytes(ownerNpId);
    proto.slotId = slotId;
    proto.opeType = opeType;
    proto.value = value;
    // compareValue is optional: when absent the server compares against value
    if (hasCompare) {
        proto.hasCompare = true;
        proto.compareValue = compareValue;
    }
    if (!virtualUser.empty()) {
        proto.virtualUser = Bytes(virtualUser);
    } else if (ownerAccountId != 0) {
        proto.ownerAccountId = ownerAccountId;
    } else if (ownerNpId.empty() ||
               Common::ToLower(ownerNpId) == Common::ToLower(client->GetNpid())) {
        proto.ownerAccountId = static_cast<s64>(client->GetUserId());
    }
    if (hasAuthorCheck) {
        proto.hasAuthorCheck = true;
        proto.isLastChangedAuthor = isLastChangedAuthor;
        if (!isLastChangedAuthorNpId.empty()) {
            proto.isLastChangedAuthorNpId = Bytes(isLastChangedAuthorNpId);
        }
    }
    if (hasDateCheck) {
        proto.hasDateCheck = true;
        proto.isLastChangedDate = isLastChangedDate;
    }
    const std::string com_id = GetNpCommId(service_label);
    if (!IsValidNpCommId(com_id)) {
        return ORBIS_NP_COMMUNITY_ERROR_INVALID_ARGUMENT;
    }
    const u64 pkt_id = client->SubmitRequest(ShadNet::CommandType::TusTryAndSetVariable,
                                             BuildTusPayload(com_id, Encode(proto)));
    return m_mutex_pending_tus.Run([&]() -> s32 {
        PendingTusRequest p;
        p.req = std::move(ctx);
        p.cmd = ShadNet::CommandType::TusTryAndSetVariable;
        p.user_id = user_id;
        p.variableArray = variableOut;
        p.variableArrayA = variableAOut;
        p.variableArrayCS = variableCSOut;
        p.arrayNum = 1;
        m_pending_tus.emplace(pkt_id, std::move(p));
        return ORBIS_OK;
    });
}

s32 NpHandler::TusAddAndGetVariable(s32 user_id, s32 service_label, const std::string& ownerNpId,
                                    const std::string& virtualUser, s64 ownerAccountId, s32 slotId,
                                    s64 inVariable, bool hasAuthorCheck, s64 isLastChangedAuthor,
                                    const std::string& isLastChangedAuthorNpId, bool hasDateCheck,
                                    u64 isLastChangedDate, NpTus::OrbisNpTusVariable* variableOut,
                                    NpTus::OrbisNpTusVariableA* variableAOut,
                                    std::shared_ptr<NpTus::TusRequestCtx> ctx,
                                    NpTus::OrbisNpTusVariableForCrossSave* variableCSOut) {
    std::shared_ptr<ShadNet::ShadNetClient> client;
    if (auto returned = m_mutex_clients.Run([&]() -> std::optional<s32> {
            auto it = m_clients.find(user_id);
            if (it == m_clients.end()) {
                return std::optional<s32>{ORBIS_NP_ERROR_SIGNED_OUT};
            }
            client = it->second;
            return std::nullopt;
        })) {
        return *returned;
    }
    shadnet_TusAddAndGetVariableRequest proto{};
    proto.ownerNpId = Bytes(ownerNpId);
    proto.slotId = slotId;
    proto.inValue = inVariable;
    if (!virtualUser.empty()) {
        proto.virtualUser = Bytes(virtualUser);
    } else if (ownerAccountId != 0) {
        proto.ownerAccountId = ownerAccountId;
    } else if (ownerNpId.empty() ||
               Common::ToLower(ownerNpId) == Common::ToLower(client->GetNpid())) {
        proto.ownerAccountId = static_cast<s64>(client->GetUserId());
    }
    if (hasAuthorCheck) {
        proto.hasAuthorCheck = true;
        proto.isLastChangedAuthor = isLastChangedAuthor;
        if (!isLastChangedAuthorNpId.empty()) {
            proto.isLastChangedAuthorNpId = Bytes(isLastChangedAuthorNpId);
        }
    }
    if (hasDateCheck) {
        proto.hasDateCheck = true;
        proto.isLastChangedDate = isLastChangedDate;
    }
    const std::string com_id = GetNpCommId(service_label);
    if (!IsValidNpCommId(com_id)) {
        return ORBIS_NP_COMMUNITY_ERROR_INVALID_ARGUMENT;
    }
    const u64 pkt_id = client->SubmitRequest(ShadNet::CommandType::TusAddAndGetVariable,
                                             BuildTusPayload(com_id, Encode(proto)));
    return m_mutex_pending_tus.Run([&]() -> s32 {
        PendingTusRequest p;
        p.req = std::move(ctx);
        p.cmd = ShadNet::CommandType::TusAddAndGetVariable;
        p.user_id = user_id;
        p.variableArray = variableOut;
        p.variableArrayA = variableAOut;
        p.variableArrayCS = variableCSOut;
        p.arrayNum = 1;
        m_pending_tus.emplace(pkt_id, std::move(p));
        return ORBIS_OK;
    });
}

s32 NpHandler::TusGetFriendsDataStatus(s32 user_id, s32 service_label, s32 slotId, bool includeSelf,
                                       s32 sortType, u32 max,
                                       NpTus::OrbisNpTusDataStatus* statusOut,
                                       NpTus::OrbisNpTusDataStatusA* statusAOut, u64 arrayNum,
                                       std::shared_ptr<NpTus::TusRequestCtx> ctx, u32 startOffset,
                                       u32* hitsOut,
                                       NpTus::OrbisNpTusDataStatusForCrossSave* statusCSOut) {
    std::shared_ptr<ShadNet::ShadNetClient> client;
    if (auto returned = m_mutex_clients.Run([&]() -> std::optional<s32> {
            auto it = m_clients.find(user_id);
            if (it == m_clients.end()) {
                return std::optional<s32>{ORBIS_NP_ERROR_SIGNED_OUT};
            }
            client = it->second;
            return std::nullopt;
        })) {
        return *returned;
    }
    // Always targets the caller's own friend list, so there is no owner field.
    shadnet_TusGetFriendsDataStatusRequest proto{};
    proto.slotId = slotId;
    proto.includeSelf = includeSelf;
    proto.sortType = sortType;
    proto.max = max;
    proto.startOffset = startOffset;
    const std::string com_id = GetNpCommId(service_label);
    if (!IsValidNpCommId(com_id)) {
        return ORBIS_NP_COMMUNITY_ERROR_INVALID_ARGUMENT;
    }
    const u64 pkt_id = client->SubmitRequest(ShadNet::CommandType::TusGetFriendsDataStatus,
                                             BuildTusPayload(com_id, Encode(proto)));
    return m_mutex_pending_tus.Run([&]() -> s32 {
        PendingTusRequest p;
        p.req = std::move(ctx);
        p.cmd = ShadNet::CommandType::TusGetFriendsDataStatus;
        p.user_id = user_id;
        p.statusArray = statusOut;
        p.statusArrayA = statusAOut;
        p.statusArrayCS = statusCSOut;
        p.arrayNum = arrayNum;
        p.totalOut = hitsOut;
        m_pending_tus.emplace(pkt_id, std::move(p));
        return ORBIS_OK;
    });
}

s32 NpHandler::TusGetFriendsVariable(s32 user_id, s32 service_label, s32 slotId, bool includeSelf,
                                     s32 sortType, u32 max, NpTus::OrbisNpTusVariable* variablesOut,
                                     NpTus::OrbisNpTusVariableA* variablesAOut, u64 arrayNum,
                                     std::shared_ptr<NpTus::TusRequestCtx> ctx, u32 startOffset,
                                     u32* hitsOut,
                                     NpTus::OrbisNpTusVariableForCrossSave* variablesCSOut) {
    std::shared_ptr<ShadNet::ShadNetClient> client;
    if (auto returned = m_mutex_clients.Run([&]() -> std::optional<s32> {
            auto it = m_clients.find(user_id);
            if (it == m_clients.end()) {
                return std::optional<s32>{ORBIS_NP_ERROR_SIGNED_OUT};
            }
            client = it->second;
            return std::nullopt;
        })) {
        return *returned;
    }
    // Always targets the caller's own friend list, so there is no owner field.
    shadnet_TusGetFriendsVariableRequest proto{};
    proto.slotId = slotId;
    proto.includeSelf = includeSelf;
    proto.sortType = sortType;
    proto.max = max;
    proto.startOffset = startOffset;
    const std::string com_id = GetNpCommId(service_label);
    if (!IsValidNpCommId(com_id)) {
        return ORBIS_NP_COMMUNITY_ERROR_INVALID_ARGUMENT;
    }
    const u64 pkt_id = client->SubmitRequest(ShadNet::CommandType::TusGetFriendsVariable,
                                             BuildTusPayload(com_id, Encode(proto)));
    return m_mutex_pending_tus.Run([&]() -> s32 {
        PendingTusRequest p;
        p.req = std::move(ctx);
        p.cmd = ShadNet::CommandType::TusGetFriendsVariable;
        p.user_id = user_id;
        p.variableArray = variablesOut;
        p.variableArrayA = variablesAOut;
        p.variableArrayCS = variablesCSOut;
        p.arrayNum = arrayNum;
        p.totalOut = hitsOut;
        m_pending_tus.emplace(pkt_id, std::move(p));
        return ORBIS_OK;
    });
}

s32 NpHandler::TusGetMultiSlotDataStatus(s32 user_id, s32 service_label,
                                         const std::string& ownerNpId,
                                         const std::string& virtualUser, s64 ownerAccountId,
                                         const std::vector<s32>& slotIds,
                                         NpTus::OrbisNpTusDataStatus* statusOut,
                                         NpTus::OrbisNpTusDataStatusA* statusAOut, u64 arrayNum,
                                         std::shared_ptr<NpTus::TusRequestCtx> ctx,
                                         NpTus::OrbisNpTusDataStatusForCrossSave* statusCSOut) {
    std::shared_ptr<ShadNet::ShadNetClient> client;
    if (auto returned = m_mutex_clients.Run([&]() -> std::optional<s32> {
            auto it = m_clients.find(user_id);
            if (it == m_clients.end()) {
                return std::optional<s32>{ORBIS_NP_ERROR_SIGNED_OUT};
            }
            client = it->second;
            return std::nullopt;
        })) {
        return *returned;
    }
    shadnet_TusGetMultiSlotDataStatusRequest proto{};
    ShadNet::Builder proto_build;
    proto.ownerNpId = Bytes(ownerNpId);
    if (!virtualUser.empty()) {
        proto.virtualUser = Bytes(virtualUser);
    } else if (ownerAccountId != 0) {
        proto.ownerAccountId = ownerAccountId;
    } else if (ownerNpId.empty() ||
               Common::ToLower(ownerNpId) == Common::ToLower(client->GetNpid())) {
        proto.ownerAccountId = static_cast<s64>(client->GetUserId());
    }
    for (s32 s : slotIds) {
        proto_build.Push(proto.slotIds, s);
    }
    const std::string com_id = GetNpCommId(service_label);
    if (!IsValidNpCommId(com_id)) {
        return ORBIS_NP_COMMUNITY_ERROR_INVALID_ARGUMENT;
    }
    const u64 pkt_id = client->SubmitRequest(ShadNet::CommandType::TusGetMultiSlotDataStatus,
                                             BuildTusPayload(com_id, Encode(proto)));
    return m_mutex_pending_tus.Run([&]() -> s32 {
        PendingTusRequest p;
        p.req = std::move(ctx);
        p.cmd = ShadNet::CommandType::TusGetMultiSlotDataStatus;
        p.user_id = user_id;
        p.statusArray = statusOut;
        p.statusArrayA = statusAOut;
        p.statusArrayCS = statusCSOut;
        p.arrayNum = arrayNum;
        m_pending_tus.emplace(pkt_id, std::move(p));
        return ORBIS_OK;
    });
}

s32 NpHandler::TusSetData(s32 user_id, s32 service_label, const std::string& ownerNpId,
                          const std::string& virtualUser, s64 ownerAccountId, s32 slotId,
                          const std::vector<u8>& blob, const std::vector<u8>& info,
                          bool hasAuthorCheck, s64 isLastChangedAuthor,
                          const std::string& isLastChangedAuthorNpId, bool hasDateCheck,
                          u64 isLastChangedDate, std::shared_ptr<NpTus::TusRequestCtx> ctx) {
    std::shared_ptr<ShadNet::ShadNetClient> client;
    if (auto returned = m_mutex_clients.Run([&]() -> std::optional<s32> {
            auto it = m_clients.find(user_id);
            if (it == m_clients.end()) {
                return std::optional<s32>{ORBIS_NP_ERROR_SIGNED_OUT};
            }
            client = it->second;
            return std::nullopt;
        })) {
        return *returned;
    }
    shadnet_TusSetDataRequest proto{};
    proto.ownerNpId = Bytes(ownerNpId);
    proto.slotId = slotId;
    proto.data = Bytes(blob.data(), blob.size());
    if (!info.empty()) {
        proto.info = Bytes(info.data(), info.size());
    }
    if (!virtualUser.empty()) {
        proto.virtualUser = Bytes(virtualUser);
    } else if (ownerAccountId != 0) {
        proto.ownerAccountId = ownerAccountId;
    } else if (ownerNpId.empty() ||
               Common::ToLower(ownerNpId) == Common::ToLower(client->GetNpid())) {
        proto.ownerAccountId = static_cast<s64>(client->GetUserId());
    }
    if (hasAuthorCheck) {
        proto.hasAuthorCheck = true;
        proto.isLastChangedAuthor = isLastChangedAuthor;
        if (!isLastChangedAuthorNpId.empty()) {
            proto.isLastChangedAuthorNpId = Bytes(isLastChangedAuthorNpId);
        }
    }
    if (hasDateCheck) {
        proto.hasDateCheck = true;
        proto.isLastChangedDate = isLastChangedDate;
    }
    const std::string com_id = GetNpCommId(service_label);
    if (!IsValidNpCommId(com_id)) {
        return ORBIS_NP_COMMUNITY_ERROR_INVALID_ARGUMENT;
    }
    const u64 pkt_id = client->SubmitRequest(ShadNet::CommandType::TusSetData,
                                             BuildTusPayload(com_id, Encode(proto)));
    return m_mutex_pending_tus.Run([&]() -> s32 {
        PendingTusRequest p;
        p.req = std::move(ctx);
        p.cmd = ShadNet::CommandType::TusSetData;
        p.user_id = user_id;
        m_pending_tus.emplace(pkt_id, std::move(p));
        return ORBIS_OK;
    });
}

void NpHandler::OnTusReply(s32 user_id, ShadNet::CommandType cmd, u64 pkt_id,
                           ShadNet::ErrorType error, const std::vector<u8>& body) {
    PendingTusRequest pending;
    if (m_mutex_pending_tus.Run([&] {
            auto it = m_pending_tus.find(pkt_id);
            if (it == m_pending_tus.end()) {
                LOG_WARNING(NpHandler, "no pending request for pkt_id={} cmd={}", pkt_id,
                            static_cast<int>(cmd));
                return true;
            }
            pending = std::move(it->second);
            m_pending_tus.erase(it);
            return false;
        })) {
        return;
    }
    auto& req = pending.req;

    if (error != ShadNet::ErrorType::NoError) {
        s32 orbis_err = ORBIS_NP_COMMUNITY_ERROR_BAD_RESPONSE;
        switch (error) {
        case ShadNet::ErrorType::Unauthorized:
            orbis_err = ORBIS_NP_COMMUNITY_SERVER_ERROR_FORBIDDEN;
            break;
        case ShadNet::ErrorType::NotFound:
            // Owner npid did not resolve to a registered user.
            orbis_err = ORBIS_NP_COMMUNITY_SERVER_ERROR_USER_PROFILE_NOT_FOUND;
            break;
        case ShadNet::ErrorType::DbFail:
            orbis_err = ORBIS_NP_COMMUNITY_SERVER_ERROR_INTERNAL_SERVER_ERROR;
            break;
        case ShadNet::ErrorType::CondFail:
            orbis_err = ORBIS_NP_COMMUNITY_SERVER_ERROR_CONDITIONS_NOT_SATISFIED;
            break;
        default:
            break;
        }
        LOG_WARNING(NpHandler, "user_id={} pkt_id={} cmd={} server_error={} -> orbis {:#x}",
                    user_id, pkt_id, static_cast<int>(cmd), static_cast<int>(error),
                    static_cast<u32>(orbis_err));
        req->SetResult(orbis_err);
        return;
    }

    auto parseTusBody = [&](auto& msg, Scratch& msg_scratch) -> bool {
        if (body.size() < 4) {
            LOG_ERROR(NpHandler, "OnTusReply: cmd={} body too small ({})", static_cast<int>(cmd),
                      body.size());
            return false;
        }
        const u32 proto_size = static_cast<u32>(body[0]) | (static_cast<u32>(body[1]) << 8) |
                               (static_cast<u32>(body[2]) << 16) |
                               (static_cast<u32>(body[3]) << 24);
        if (static_cast<size_t>(4) + proto_size > body.size()) {
            LOG_ERROR(NpHandler, "OnTusReply: cmd={} proto size {} exceeds body size {}",
                      static_cast<int>(cmd), proto_size, body.size());
            return false;
        }
        if (!Decode(msg, body.data() + 4, proto_size, msg_scratch)) {
            LOG_ERROR(NpHandler, "OnTusReply: cmd={} proto parse failed", static_cast<int>(cmd));
            return false;
        }
        return true;
    };

    auto fillVariable = [](NpTus::OrbisNpTusVariable& v, const shadnet_TusVariable& s) {
        v = NpTus::OrbisNpTusVariable{};
        CopyNpHandle(v.ownerId, View(s.ownerNpId));
        v.hasData = s.set ? 1 : 0;
        v.lastChangedDate.tick = s.lastChangedDate;
        CopyNpHandle(v.lastChangedAuthorId, View(s.lastChangedAuthorNpId));
        v.variable = s.variable;
        v.oldVariable = s.oldVariable;
        v.ownerAccountId = static_cast<OrbisNpAccountId>(s.ownerAccountId);
        v.lastChangedAuthorAccountId = static_cast<OrbisNpAccountId>(s.lastChangedAuthorAccountId);
    };
    auto fillVariableA = [](NpTus::OrbisNpTusVariableA& v, const shadnet_TusVariable& s) {
        v = NpTus::OrbisNpTusVariableA{};
        std::memcpy(v.ownerId.data, s.ownerNpId.data,
                    std::min(s.ownerNpId.size, sizeof(v.ownerId.data)));
        v.hasData = s.set ? 1 : 0;
        v.lastChangedDate.tick = s.lastChangedDate;
        std::memcpy(v.lastChangedAuthorId.data, s.lastChangedAuthorNpId.data,
                    std::min(s.lastChangedAuthorNpId.size, sizeof(v.lastChangedAuthorId.data)));
        v.variable = s.variable;
        v.oldVariable = s.oldVariable;
        v.ownerAccountId = static_cast<OrbisNpAccountId>(s.ownerAccountId);
        v.lastChangedAuthorAccountId = static_cast<OrbisNpAccountId>(s.lastChangedAuthorAccountId);
    };
    auto fillVariableForCrossSave = [](NpTus::OrbisNpTusVariableForCrossSave& v,
                                       const shadnet_TusVariable& s) {
        v = NpTus::OrbisNpTusVariableForCrossSave{};
        CopyNpHandle(v.ownerId, View(s.ownerNpId));
        v.hasData = s.set ? 1 : 0;
        v.lastChangedDate.tick = s.lastChangedDate;
        CopyNpHandle(v.lastChangedAuthorId, View(s.lastChangedAuthorNpId));
        v.variable = s.variable;
        v.oldVariable = s.oldVariable;
        v.ownerAccountId = static_cast<OrbisNpAccountId>(s.ownerAccountId);
        v.lastChangedAuthorAccountId = static_cast<OrbisNpAccountId>(s.lastChangedAuthorAccountId);
    };
    auto fillStatus = [](NpTus::OrbisNpTusDataStatus& d, const shadnet_TusDataStatus& s) {
        d = NpTus::OrbisNpTusDataStatus{};
        CopyNpHandle(d.ownerId, View(s.ownerNpId));
        d.hasData = s.set ? 1 : 0;
        d.lastChangedDate.tick = s.lastChangedDate;
        CopyNpHandle(d.lastChangedAuthorId, View(s.lastChangedAuthorNpId));
        d.dataSize = s.dataSize;
        d.info.size = s.info.size;
        const size_t cp = std::min(s.info.size, sizeof(d.info.data));
        std::memcpy(d.info.data, s.info.data, cp);
    };
    auto fillStatusA = [](NpTus::OrbisNpTusDataStatusA& d, const shadnet_TusDataStatus& s) {
        d = NpTus::OrbisNpTusDataStatusA{};
        std::memcpy(d.ownerId.data, s.ownerNpId.data,
                    std::min(s.ownerNpId.size, sizeof(d.ownerId.data)));
        d.hasData = s.set ? 1 : 0;
        d.lastChangedDate.tick = s.lastChangedDate;
        std::memcpy(d.lastChangedAuthorId.data, s.lastChangedAuthorNpId.data,
                    std::min(s.lastChangedAuthorNpId.size, sizeof(d.lastChangedAuthorId.data)));
        d.dataSize = s.dataSize;
        d.info.size = s.info.size;
        const size_t cp = std::min(s.info.size, sizeof(d.info.data));
        std::memcpy(d.info.data, s.info.data, cp);
        d.ownerAccountId = static_cast<OrbisNpAccountId>(s.ownerAccountId);
        d.lastChangedAuthorAccountId = static_cast<OrbisNpAccountId>(s.lastChangedAuthorAccountId);
    };
    auto fillStatusForCrossSave = [](NpTus::OrbisNpTusDataStatusForCrossSave& d,
                                     const shadnet_TusDataStatus& s) {
        d = NpTus::OrbisNpTusDataStatusForCrossSave{};
        CopyNpHandle(d.ownerId, View(s.ownerNpId));
        d.hasData = s.set ? 1 : 0;
        d.lastChangedDate.tick = s.lastChangedDate;
        CopyNpHandle(d.lastChangedAuthorId, View(s.lastChangedAuthorNpId));
        d.dataSize = s.dataSize;
        d.info.size = s.info.size;
        const size_t cp = std::min(s.info.size, sizeof(d.info.data));
        std::memcpy(d.info.data, s.info.data, cp);
        d.ownerAccountId = static_cast<OrbisNpAccountId>(s.ownerAccountId);
        d.lastChangedAuthorAccountId = static_cast<OrbisNpAccountId>(s.lastChangedAuthorAccountId);
    };

    switch (cmd) {
    case ShadNet::CommandType::TusGetMultiSlotVariable:
    case ShadNet::CommandType::TusTryAndSetVariable:
    case ShadNet::CommandType::TusGetMultiUserVariable:
    case ShadNet::CommandType::TusGetFriendsVariable:
    case ShadNet::CommandType::TusAddAndGetVariable: {
        shadnet_TusVariableResponse resp{};
        Scratch resp_scratch;
        if (!parseTusBody(resp, resp_scratch)) {
            req->SetResult(ORBIS_NP_COMMUNITY_ERROR_BAD_RESPONSE);
            return;
        }
        u64 filled = 0;
        if (pending.variableArrayA) {
            filled = std::min<u64>(pending.arrayNum, resp.variables.count);
            for (u64 i = 0; i < filled; ++i) {
                fillVariableA(pending.variableArrayA[i], resp.variables.items[i]);
            }
        } else if (pending.variableArrayCS) {
            filled = std::min<u64>(pending.arrayNum, resp.variables.count);
            for (u64 i = 0; i < filled; ++i) {
                fillVariableForCrossSave(pending.variableArrayCS[i], resp.variables.items[i]);
            }
        } else if (pending.variableArray) {
            filled = std::min<u64>(pending.arrayNum, resp.variables.count);
            for (u64 i = 0; i < filled; ++i) {
                fillVariable(pending.variableArray[i], resp.variables.items[i]);
            }
        }
        // GetMultiSlotVariable / GetMultiUserVariable return the count of variables read;
        // AddAndGet / TryAndSet return 0 on normal termination (value is in the struct).
        if (pending.totalOut) {
            // OrbisNpTusGetFriendsVariableOptParam::hits - total registered
            // friends before startSerialRank/arrayNum are applied.
            *pending.totalOut = resp.total ? resp.total : static_cast<u32>(resp.variables.count);
        }
        if (cmd == ShadNet::CommandType::TusGetMultiSlotVariable ||
            cmd == ShadNet::CommandType::TusGetMultiUserVariable ||
            cmd == ShadNet::CommandType::TusGetFriendsVariable) {
            req->SetResult(static_cast<s32>(filled));
        }
        break;
    }
    case ShadNet::CommandType::TssGetData: {
        shadnet_TssGetDataResponse resp{};
        Scratch resp_scratch;
        if (!parseTusBody(resp, resp_scratch)) {
            req->SetResult(ORBIS_NP_COMMUNITY_ERROR_BAD_RESPONSE);
            return;
        }
        u64 recv = 0;
        if (pending.dataOut && !(resp.data.size == 0)) {
            recv = std::min<u64>(pending.dataCap, resp.data.size);
            std::memcpy(pending.dataOut, resp.data.data, recv);
        }
        if (pending.tssContentLengthOut) {
            *pending.tssContentLengthOut = resp.contentLength;
        }
        if (pending.tssStatusOut) {
            *pending.tssStatusOut = NpTus::OrbisNpTssDataStatus{};
            pending.tssStatusOut->modified.tick = resp.lastModified;
            pending.tssStatusOut->status =
                static_cast<NpTus::OrbisNpTssStatus>(resp.statusCodeType);
            pending.tssStatusOut->contentLength = resp.contentLength;
        }
        req->SetResult(ORBIS_OK);
        return;
    }
    case ShadNet::CommandType::TusGetData: {
        shadnet_TusGetDataResponse resp{};
        Scratch resp_scratch;
        if (!parseTusBody(resp, resp_scratch)) {
            req->SetResult(ORBIS_NP_COMMUNITY_ERROR_BAD_RESPONSE);
            return;
        }
        u64 recv = 0;
        const u64 blob_size = resp.data.size;
        if (pending.dataOut && pending.dataOffset < blob_size) {
            const u64 avail = blob_size - pending.dataOffset;
            recv = std::min<u64>(pending.dataCap, avail);
            std::memcpy(pending.dataOut, resp.data.data + pending.dataOffset, recv);
        }
        if (pending.statusArrayA) {
            NpTus::OrbisNpTusDataStatusA tmp{};
            fillStatusA(tmp, resp.status);
            if (recv) {
                tmp.data = pending.dataOut;
            }
            const u64 cap =
                pending.statusCap ? std::min<u64>(pending.statusCap, sizeof(tmp)) : sizeof(tmp);
            std::memcpy(pending.statusArrayA, &tmp, cap);
        } else if (pending.statusArrayCS) {
            NpTus::OrbisNpTusDataStatusForCrossSave tmp{};
            fillStatusForCrossSave(tmp, resp.status);
            if (recv) {
                tmp.data = pending.dataOut;
            }
            const u64 cap =
                pending.statusCap ? std::min<u64>(pending.statusCap, sizeof(tmp)) : sizeof(tmp);
            std::memcpy(pending.statusArrayCS, &tmp, cap);
        } else if (pending.statusArray) {
            NpTus::OrbisNpTusDataStatus tmp{};
            fillStatus(tmp, resp.status);
            if (recv) {
                tmp.data = pending.dataOut;
            }
            const u64 cap =
                pending.statusCap ? std::min<u64>(pending.statusCap, sizeof(tmp)) : sizeof(tmp);
            std::memcpy(pending.statusArray, &tmp, cap);
        }
        req->SetResult(static_cast<s32>(recv));
        return;
    }
    case ShadNet::CommandType::TusGetMultiSlotDataStatus:
    case ShadNet::CommandType::TusGetMultiUserDataStatus:
    case ShadNet::CommandType::TusGetFriendsDataStatus: {
        shadnet_TusDataStatusResponse resp{};
        Scratch resp_scratch;
        if (!parseTusBody(resp, resp_scratch)) {
            req->SetResult(ORBIS_NP_COMMUNITY_ERROR_BAD_RESPONSE);
            return;
        }
        if (pending.statusArrayA) {
            const u64 n = std::min<u64>(pending.arrayNum, resp.statuses.count);
            for (u64 i = 0; i < n; ++i) {
                fillStatusA(pending.statusArrayA[i], resp.statuses.items[i]);
            }
        } else if (pending.statusArrayCS) {
            const u64 n = std::min<u64>(pending.arrayNum, resp.statuses.count);
            for (u64 i = 0; i < n; ++i) {
                fillStatusForCrossSave(pending.statusArrayCS[i], resp.statuses.items[i]);
            }
        } else if (pending.statusArray) {
            const u64 n = std::min<u64>(pending.arrayNum, resp.statuses.count);
            for (u64 i = 0; i < n; ++i) {
                fillStatus(pending.statusArray[i], resp.statuses.items[i]);
            }
        }
        if (pending.totalOut) {
            // hits = total registered friends before offset/cap
            *pending.totalOut = resp.total ? resp.total : static_cast<u32>(resp.statuses.count);
        }
        if (cmd == ShadNet::CommandType::TusGetFriendsDataStatus ||
            cmd == ShadNet::CommandType::TusGetMultiSlotDataStatus ||
            cmd == ShadNet::CommandType::TusGetMultiUserDataStatus) {
            // These calls return the number of statuses read (>= 0), not ORBIS_OK.
            const s32 cnt = static_cast<s32>(std::min<u64>(pending.arrayNum, resp.statuses.count));
            req->SetResult(cnt);
            return;
        }
        break;
    }
    default:
        // Set*/Delete* carry no payload to fill.
        LOG_DEBUG(NpHandler, "cmd={} ok (no payload)", static_cast<int>(cmd));
        break;
    }
    req->SetResult(ORBIS_OK);
}

} // namespace Libraries::Np
