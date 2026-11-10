/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

/* Written by gen_messages.py from shadnet.proto; do not edit. */
/* clang-format off */

#include <stddef.h>

#include "shadnet/shadnet_msg.h"

static const pb_field_t shadnet_LoginRequest_fields[] = {
    {1, PB_STRING, 0, offsetof(shadnet_LoginRequest, npid), 0, NULL},
    {2, PB_STRING, 0, offsetof(shadnet_LoginRequest, password), 0, NULL},
    {3, PB_STRING, 0, offsetof(shadnet_LoginRequest, token), 0, NULL},
    {4, PB_STRING, 0, offsetof(shadnet_LoginRequest, title_id), 0, NULL},
    {5, PB_STRING, 0, offsetof(shadnet_LoginRequest, title_name), 0, NULL},
    {6, PB_BOOL, 0, offsetof(shadnet_LoginRequest, appear_offline), 0, NULL},
};
const pb_msg_desc_t shadnet_LoginRequest_desc = {sizeof(shadnet_LoginRequest), shadnet_LoginRequest_fields,
    sizeof(shadnet_LoginRequest_fields) / sizeof(shadnet_LoginRequest_fields[0])};

static const pb_field_t shadnet_SetAppearOfflineRequest_fields[] = {
    {1, PB_BOOL, 0, offsetof(shadnet_SetAppearOfflineRequest, appear_offline), 0, NULL},
};
const pb_msg_desc_t shadnet_SetAppearOfflineRequest_desc = {sizeof(shadnet_SetAppearOfflineRequest), shadnet_SetAppearOfflineRequest_fields,
    sizeof(shadnet_SetAppearOfflineRequest_fields) / sizeof(shadnet_SetAppearOfflineRequest_fields[0])};

static const pb_field_t shadnet_ServerFeaturesReply_fields[] = {
    {1, PB_BOOL, 0, offsetof(shadnet_ServerFeaturesReply, matching2_enabled), 0, NULL},
    {2, PB_BOOL, 0, offsetof(shadnet_ServerFeaturesReply, trophies_enabled), 0, NULL},
};
const pb_msg_desc_t shadnet_ServerFeaturesReply_desc = {sizeof(shadnet_ServerFeaturesReply), shadnet_ServerFeaturesReply_fields,
    sizeof(shadnet_ServerFeaturesReply_fields) / sizeof(shadnet_ServerFeaturesReply_fields[0])};

static const pb_field_t shadnet_FriendEntry_fields[] = {
    {1, PB_STRING, 0, offsetof(shadnet_FriendEntry, npid), 0, NULL},
    {2, PB_BOOL, 0, offsetof(shadnet_FriendEntry, online), 0, NULL},
    {3, PB_BYTES, 0, offsetof(shadnet_FriendEntry, presence), 0, NULL},
};
const pb_msg_desc_t shadnet_FriendEntry_desc = {sizeof(shadnet_FriendEntry), shadnet_FriendEntry_fields,
    sizeof(shadnet_FriendEntry_fields) / sizeof(shadnet_FriendEntry_fields[0])};

static const pb_field_t shadnet_LoginReply_fields[] = {
    {1, PB_STRING, 0, offsetof(shadnet_LoginReply, avatar_url), 0, NULL},
    {2, PB_UINT64, 0, offsetof(shadnet_LoginReply, user_id), 0, NULL},
    {3, PB_MESSAGE, 1, offsetof(shadnet_LoginReply, friends), 0, &shadnet_FriendEntry_desc},
    {4, PB_STRING, 1, offsetof(shadnet_LoginReply, friend_requests_sent), 0, NULL},
    {5, PB_STRING, 1, offsetof(shadnet_LoginReply, friend_requests_received), 0, NULL},
    {6, PB_STRING, 1, offsetof(shadnet_LoginReply, blocked), 0, NULL},
};
const pb_msg_desc_t shadnet_LoginReply_desc = {sizeof(shadnet_LoginReply), shadnet_LoginReply_fields,
    sizeof(shadnet_LoginReply_fields) / sizeof(shadnet_LoginReply_fields[0])};

static const pb_field_t shadnet_RegistrationRequest_fields[] = {
    {1, PB_STRING, 0, offsetof(shadnet_RegistrationRequest, npid), 0, NULL},
    {2, PB_STRING, 0, offsetof(shadnet_RegistrationRequest, password), 0, NULL},
    {3, PB_STRING, 0, offsetof(shadnet_RegistrationRequest, avatar_url), 0, NULL},
    {4, PB_STRING, 0, offsetof(shadnet_RegistrationRequest, email), 0, NULL},
    {5, PB_STRING, 0, offsetof(shadnet_RegistrationRequest, secret_key), 0, NULL},
};
const pb_msg_desc_t shadnet_RegistrationRequest_desc = {sizeof(shadnet_RegistrationRequest), shadnet_RegistrationRequest_fields,
    sizeof(shadnet_RegistrationRequest_fields) / sizeof(shadnet_RegistrationRequest_fields[0])};

static const pb_field_t shadnet_FriendCommandRequest_fields[] = {
    {1, PB_STRING, 0, offsetof(shadnet_FriendCommandRequest, npid), 0, NULL},
};
const pb_msg_desc_t shadnet_FriendCommandRequest_desc = {sizeof(shadnet_FriendCommandRequest), shadnet_FriendCommandRequest_fields,
    sizeof(shadnet_FriendCommandRequest_fields) / sizeof(shadnet_FriendCommandRequest_fields[0])};

static const pb_field_t shadnet_NotifyFriendQuery_fields[] = {
    {1, PB_STRING, 0, offsetof(shadnet_NotifyFriendQuery, from_npid), 0, NULL},
};
const pb_msg_desc_t shadnet_NotifyFriendQuery_desc = {sizeof(shadnet_NotifyFriendQuery), shadnet_NotifyFriendQuery_fields,
    sizeof(shadnet_NotifyFriendQuery_fields) / sizeof(shadnet_NotifyFriendQuery_fields[0])};

static const pb_field_t shadnet_NotifyFriendNew_fields[] = {
    {1, PB_STRING, 0, offsetof(shadnet_NotifyFriendNew, npid), 0, NULL},
    {2, PB_BOOL, 0, offsetof(shadnet_NotifyFriendNew, online), 0, NULL},
};
const pb_msg_desc_t shadnet_NotifyFriendNew_desc = {sizeof(shadnet_NotifyFriendNew), shadnet_NotifyFriendNew_fields,
    sizeof(shadnet_NotifyFriendNew_fields) / sizeof(shadnet_NotifyFriendNew_fields[0])};

static const pb_field_t shadnet_NotifyFriendLost_fields[] = {
    {1, PB_STRING, 0, offsetof(shadnet_NotifyFriendLost, npid), 0, NULL},
};
const pb_msg_desc_t shadnet_NotifyFriendLost_desc = {sizeof(shadnet_NotifyFriendLost), shadnet_NotifyFriendLost_fields,
    sizeof(shadnet_NotifyFriendLost_fields) / sizeof(shadnet_NotifyFriendLost_fields[0])};

static const pb_field_t shadnet_NotifyFriendStatus_fields[] = {
    {1, PB_STRING, 0, offsetof(shadnet_NotifyFriendStatus, npid), 0, NULL},
    {2, PB_BOOL, 0, offsetof(shadnet_NotifyFriendStatus, online), 0, NULL},
    {3, PB_UINT64, 0, offsetof(shadnet_NotifyFriendStatus, timestamp), 0, NULL},
};
const pb_msg_desc_t shadnet_NotifyFriendStatus_desc = {sizeof(shadnet_NotifyFriendStatus), shadnet_NotifyFriendStatus_fields,
    sizeof(shadnet_NotifyFriendStatus_fields) / sizeof(shadnet_NotifyFriendStatus_fields[0])};

static const pb_field_t shadnet_NotifyServiceUnavailable_fields[] = {
    {1, PB_UINT32, 0, offsetof(shadnet_NotifyServiceUnavailable, service), 0, NULL},
    {2, PB_UINT32, 0, offsetof(shadnet_NotifyServiceUnavailable, reason), 0, NULL},
    {3, PB_UINT32, 0, offsetof(shadnet_NotifyServiceUnavailable, in_use), 0, NULL},
    {4, PB_UINT32, 0, offsetof(shadnet_NotifyServiceUnavailable, limit), 0, NULL},
};
const pb_msg_desc_t shadnet_NotifyServiceUnavailable_desc = {sizeof(shadnet_NotifyServiceUnavailable), shadnet_NotifyServiceUnavailable_fields,
    sizeof(shadnet_NotifyServiceUnavailable_fields) / sizeof(shadnet_NotifyServiceUnavailable_fields[0])};

static const pb_field_t shadnet_NotifyClientRejected_fields[] = {
    {1, PB_UINT32, 0, offsetof(shadnet_NotifyClientRejected, reason), 0, NULL},
    {2, PB_STRING, 0, offsetof(shadnet_NotifyClientRejected, message), 0, NULL},
    {3, PB_STRING, 0, offsetof(shadnet_NotifyClientRejected, version), 0, NULL},
};
const pb_msg_desc_t shadnet_NotifyClientRejected_desc = {sizeof(shadnet_NotifyClientRejected), shadnet_NotifyClientRejected_fields,
    sizeof(shadnet_NotifyClientRejected_fields) / sizeof(shadnet_NotifyClientRejected_fields[0])};

static const pb_field_t shadnet_BoardInfo_fields[] = {
    {1, PB_UINT32, 0, offsetof(shadnet_BoardInfo, rankLimit), 0, NULL},
    {2, PB_UINT32, 0, offsetof(shadnet_BoardInfo, updateMode), 0, NULL},
    {3, PB_UINT32, 0, offsetof(shadnet_BoardInfo, sortMode), 0, NULL},
    {4, PB_UINT32, 0, offsetof(shadnet_BoardInfo, uploadNumLimit), 0, NULL},
    {5, PB_UINT64, 0, offsetof(shadnet_BoardInfo, uploadSizeLimit), 0, NULL},
};
const pb_msg_desc_t shadnet_BoardInfo_desc = {sizeof(shadnet_BoardInfo), shadnet_BoardInfo_fields,
    sizeof(shadnet_BoardInfo_fields) / sizeof(shadnet_BoardInfo_fields[0])};

static const pb_field_t shadnet_RecordScoreRequest_fields[] = {
    {1, PB_UINT32, 0, offsetof(shadnet_RecordScoreRequest, boardId), 0, NULL},
    {2, PB_INT32, 0, offsetof(shadnet_RecordScoreRequest, pcId), 0, NULL},
    {3, PB_INT64, 0, offsetof(shadnet_RecordScoreRequest, score), 0, NULL},
    {4, PB_STRING, 0, offsetof(shadnet_RecordScoreRequest, comment), 0, NULL},
    {5, PB_BYTES, 0, offsetof(shadnet_RecordScoreRequest, data), 0, NULL},
};
const pb_msg_desc_t shadnet_RecordScoreRequest_desc = {sizeof(shadnet_RecordScoreRequest), shadnet_RecordScoreRequest_fields,
    sizeof(shadnet_RecordScoreRequest_fields) / sizeof(shadnet_RecordScoreRequest_fields[0])};

static const pb_field_t shadnet_RecordScoreGameDataRequest_fields[] = {
    {1, PB_UINT32, 0, offsetof(shadnet_RecordScoreGameDataRequest, boardId), 0, NULL},
    {2, PB_INT32, 0, offsetof(shadnet_RecordScoreGameDataRequest, pcId), 0, NULL},
    {3, PB_INT64, 0, offsetof(shadnet_RecordScoreGameDataRequest, score), 0, NULL},
};
const pb_msg_desc_t shadnet_RecordScoreGameDataRequest_desc = {sizeof(shadnet_RecordScoreGameDataRequest), shadnet_RecordScoreGameDataRequest_fields,
    sizeof(shadnet_RecordScoreGameDataRequest_fields) / sizeof(shadnet_RecordScoreGameDataRequest_fields[0])};

static const pb_field_t shadnet_GetScoreGameDataRequest_fields[] = {
    {1, PB_UINT32, 0, offsetof(shadnet_GetScoreGameDataRequest, boardId), 0, NULL},
    {2, PB_STRING, 0, offsetof(shadnet_GetScoreGameDataRequest, npId), 0, NULL},
    {3, PB_INT32, 0, offsetof(shadnet_GetScoreGameDataRequest, pcId), 0, NULL},
};
const pb_msg_desc_t shadnet_GetScoreGameDataRequest_desc = {sizeof(shadnet_GetScoreGameDataRequest), shadnet_GetScoreGameDataRequest_fields,
    sizeof(shadnet_GetScoreGameDataRequest_fields) / sizeof(shadnet_GetScoreGameDataRequest_fields[0])};

static const pb_field_t shadnet_GetScoreRangeRequest_fields[] = {
    {1, PB_UINT32, 0, offsetof(shadnet_GetScoreRangeRequest, boardId), 0, NULL},
    {2, PB_UINT32, 0, offsetof(shadnet_GetScoreRangeRequest, startRank), 0, NULL},
    {3, PB_UINT32, 0, offsetof(shadnet_GetScoreRangeRequest, numRanks), 0, NULL},
    {4, PB_BOOL, 0, offsetof(shadnet_GetScoreRangeRequest, withComment), 0, NULL},
    {5, PB_BOOL, 0, offsetof(shadnet_GetScoreRangeRequest, withGameInfo), 0, NULL},
};
const pb_msg_desc_t shadnet_GetScoreRangeRequest_desc = {sizeof(shadnet_GetScoreRangeRequest), shadnet_GetScoreRangeRequest_fields,
    sizeof(shadnet_GetScoreRangeRequest_fields) / sizeof(shadnet_GetScoreRangeRequest_fields[0])};

static const pb_field_t shadnet_ScoreNpIdPcId_fields[] = {
    {1, PB_STRING, 0, offsetof(shadnet_ScoreNpIdPcId, npid), 0, NULL},
    {2, PB_INT32, 0, offsetof(shadnet_ScoreNpIdPcId, pcId), 0, NULL},
};
const pb_msg_desc_t shadnet_ScoreNpIdPcId_desc = {sizeof(shadnet_ScoreNpIdPcId), shadnet_ScoreNpIdPcId_fields,
    sizeof(shadnet_ScoreNpIdPcId_fields) / sizeof(shadnet_ScoreNpIdPcId_fields[0])};

static const pb_field_t shadnet_GetScoreNpIdRequest_fields[] = {
    {1, PB_UINT32, 0, offsetof(shadnet_GetScoreNpIdRequest, boardId), 0, NULL},
    {2, PB_MESSAGE, 1, offsetof(shadnet_GetScoreNpIdRequest, npids), 0, &shadnet_ScoreNpIdPcId_desc},
    {3, PB_BOOL, 0, offsetof(shadnet_GetScoreNpIdRequest, withComment), 0, NULL},
    {4, PB_BOOL, 0, offsetof(shadnet_GetScoreNpIdRequest, withGameInfo), 0, NULL},
};
const pb_msg_desc_t shadnet_GetScoreNpIdRequest_desc = {sizeof(shadnet_GetScoreNpIdRequest), shadnet_GetScoreNpIdRequest_fields,
    sizeof(shadnet_GetScoreNpIdRequest_fields) / sizeof(shadnet_GetScoreNpIdRequest_fields[0])};

static const pb_field_t shadnet_GetScoreFriendsRequest_fields[] = {
    {1, PB_UINT32, 0, offsetof(shadnet_GetScoreFriendsRequest, boardId), 0, NULL},
    {2, PB_BOOL, 0, offsetof(shadnet_GetScoreFriendsRequest, includeSelf), 0, NULL},
    {3, PB_UINT32, 0, offsetof(shadnet_GetScoreFriendsRequest, max), 0, NULL},
    {4, PB_BOOL, 0, offsetof(shadnet_GetScoreFriendsRequest, withComment), 0, NULL},
    {5, PB_BOOL, 0, offsetof(shadnet_GetScoreFriendsRequest, withGameInfo), 0, NULL},
};
const pb_msg_desc_t shadnet_GetScoreFriendsRequest_desc = {sizeof(shadnet_GetScoreFriendsRequest), shadnet_GetScoreFriendsRequest_fields,
    sizeof(shadnet_GetScoreFriendsRequest_fields) / sizeof(shadnet_GetScoreFriendsRequest_fields[0])};

static const pb_field_t shadnet_ScoreAccountIdPcId_fields[] = {
    {1, PB_INT64, 0, offsetof(shadnet_ScoreAccountIdPcId, accountId), 0, NULL},
    {2, PB_INT32, 0, offsetof(shadnet_ScoreAccountIdPcId, pcId), 0, NULL},
};
const pb_msg_desc_t shadnet_ScoreAccountIdPcId_desc = {sizeof(shadnet_ScoreAccountIdPcId), shadnet_ScoreAccountIdPcId_fields,
    sizeof(shadnet_ScoreAccountIdPcId_fields) / sizeof(shadnet_ScoreAccountIdPcId_fields[0])};

static const pb_field_t shadnet_GetScoreAccountIdRequest_fields[] = {
    {1, PB_UINT32, 0, offsetof(shadnet_GetScoreAccountIdRequest, boardId), 0, NULL},
    {2, PB_MESSAGE, 1, offsetof(shadnet_GetScoreAccountIdRequest, ids), 0, &shadnet_ScoreAccountIdPcId_desc},
    {3, PB_BOOL, 0, offsetof(shadnet_GetScoreAccountIdRequest, withComment), 0, NULL},
    {4, PB_BOOL, 0, offsetof(shadnet_GetScoreAccountIdRequest, withGameInfo), 0, NULL},
};
const pb_msg_desc_t shadnet_GetScoreAccountIdRequest_desc = {sizeof(shadnet_GetScoreAccountIdRequest), shadnet_GetScoreAccountIdRequest_fields,
    sizeof(shadnet_GetScoreAccountIdRequest_fields) / sizeof(shadnet_GetScoreAccountIdRequest_fields[0])};

static const pb_field_t shadnet_GetScoreGameDataByAccountIdRequest_fields[] = {
    {1, PB_UINT32, 0, offsetof(shadnet_GetScoreGameDataByAccountIdRequest, boardId), 0, NULL},
    {2, PB_INT64, 0, offsetof(shadnet_GetScoreGameDataByAccountIdRequest, accountId), 0, NULL},
    {3, PB_INT32, 0, offsetof(shadnet_GetScoreGameDataByAccountIdRequest, pcId), 0, NULL},
};
const pb_msg_desc_t shadnet_GetScoreGameDataByAccountIdRequest_desc = {sizeof(shadnet_GetScoreGameDataByAccountIdRequest), shadnet_GetScoreGameDataByAccountIdRequest_fields,
    sizeof(shadnet_GetScoreGameDataByAccountIdRequest_fields) / sizeof(shadnet_GetScoreGameDataByAccountIdRequest_fields[0])};

static const pb_field_t shadnet_ScoreRankData_fields[] = {
    {1, PB_STRING, 0, offsetof(shadnet_ScoreRankData, npId), 0, NULL},
    {2, PB_INT32, 0, offsetof(shadnet_ScoreRankData, pcId), 0, NULL},
    {3, PB_UINT32, 0, offsetof(shadnet_ScoreRankData, rank), 0, NULL},
    {4, PB_INT64, 0, offsetof(shadnet_ScoreRankData, score), 0, NULL},
    {5, PB_BOOL, 0, offsetof(shadnet_ScoreRankData, hasGameData), 0, NULL},
    {6, PB_UINT64, 0, offsetof(shadnet_ScoreRankData, recordDate), 0, NULL},
    {7, PB_INT64, 0, offsetof(shadnet_ScoreRankData, accountId), 0, NULL},
};
const pb_msg_desc_t shadnet_ScoreRankData_desc = {sizeof(shadnet_ScoreRankData), shadnet_ScoreRankData_fields,
    sizeof(shadnet_ScoreRankData_fields) / sizeof(shadnet_ScoreRankData_fields[0])};

static const pb_field_t shadnet_ScoreInfo_fields[] = {
    {1, PB_BYTES, 0, offsetof(shadnet_ScoreInfo, data), 0, NULL},
};
const pb_msg_desc_t shadnet_ScoreInfo_desc = {sizeof(shadnet_ScoreInfo), shadnet_ScoreInfo_fields,
    sizeof(shadnet_ScoreInfo_fields) / sizeof(shadnet_ScoreInfo_fields[0])};

static const pb_field_t shadnet_GetScoreResponse_fields[] = {
    {1, PB_MESSAGE, 1, offsetof(shadnet_GetScoreResponse, rankArray), 0, &shadnet_ScoreRankData_desc},
    {2, PB_STRING, 1, offsetof(shadnet_GetScoreResponse, commentArray), 0, NULL},
    {3, PB_MESSAGE, 1, offsetof(shadnet_GetScoreResponse, infoArray), 0, &shadnet_ScoreInfo_desc},
    {4, PB_UINT64, 0, offsetof(shadnet_GetScoreResponse, lastSortDate), 0, NULL},
    {5, PB_UINT32, 0, offsetof(shadnet_GetScoreResponse, totalRecord), 0, NULL},
};
const pb_msg_desc_t shadnet_GetScoreResponse_desc = {sizeof(shadnet_GetScoreResponse), shadnet_GetScoreResponse_fields,
    sizeof(shadnet_GetScoreResponse_fields) / sizeof(shadnet_GetScoreResponse_fields[0])};

static const pb_field_t shadnet_MatchingBinAttr_fields[] = {
    {1, PB_UINT32, 0, offsetof(shadnet_MatchingBinAttr, attr_id), 0, NULL},
    {2, PB_BYTES, 0, offsetof(shadnet_MatchingBinAttr, data), 0, NULL},
    {3, PB_UINT64, 0, offsetof(shadnet_MatchingBinAttr, update_date), 0, NULL},
    {4, PB_UINT32, 0, offsetof(shadnet_MatchingBinAttr, update_member_id), 0, NULL},
};
const pb_msg_desc_t shadnet_MatchingBinAttr_desc = {sizeof(shadnet_MatchingBinAttr), shadnet_MatchingBinAttr_fields,
    sizeof(shadnet_MatchingBinAttr_fields) / sizeof(shadnet_MatchingBinAttr_fields[0])};

static const pb_field_t shadnet_MatchingIntAttr_fields[] = {
    {1, PB_UINT32, 0, offsetof(shadnet_MatchingIntAttr, attr_id), 0, NULL},
    {2, PB_UINT32, 0, offsetof(shadnet_MatchingIntAttr, attr_value), 0, NULL},
};
const pb_msg_desc_t shadnet_MatchingIntAttr_desc = {sizeof(shadnet_MatchingIntAttr), shadnet_MatchingIntAttr_fields,
    sizeof(shadnet_MatchingIntAttr_fields) / sizeof(shadnet_MatchingIntAttr_fields[0])};

static const pb_field_t shadnet_MatchingRoomGroup_fields[] = {
    {1, PB_UINT32, 0, offsetof(shadnet_MatchingRoomGroup, group_id), 0, NULL},
    {2, PB_BOOL, 0, offsetof(shadnet_MatchingRoomGroup, has_passwd), 0, NULL},
    {3, PB_BOOL, 0, offsetof(shadnet_MatchingRoomGroup, has_label), 0, NULL},
    {4, PB_BYTES, 0, offsetof(shadnet_MatchingRoomGroup, label), 0, NULL},
    {5, PB_UINT32, 0, offsetof(shadnet_MatchingRoomGroup, slot_count), 0, NULL},
    {6, PB_UINT32, 0, offsetof(shadnet_MatchingRoomGroup, num_members), 0, NULL},
};
const pb_msg_desc_t shadnet_MatchingRoomGroup_desc = {sizeof(shadnet_MatchingRoomGroup), shadnet_MatchingRoomGroup_fields,
    sizeof(shadnet_MatchingRoomGroup_fields) / sizeof(shadnet_MatchingRoomGroup_fields[0])};

static const pb_field_t shadnet_MatchingRoomGroupConfig_fields[] = {
    {1, PB_UINT32, 0, offsetof(shadnet_MatchingRoomGroupConfig, slot_count), 0, NULL},
    {2, PB_BOOL, 0, offsetof(shadnet_MatchingRoomGroupConfig, has_label), 0, NULL},
    {3, PB_BYTES, 0, offsetof(shadnet_MatchingRoomGroupConfig, label), 0, NULL},
    {4, PB_BOOL, 0, offsetof(shadnet_MatchingRoomGroupConfig, has_passwd), 0, NULL},
};
const pb_msg_desc_t shadnet_MatchingRoomGroupConfig_desc = {sizeof(shadnet_MatchingRoomGroupConfig), shadnet_MatchingRoomGroupConfig_fields,
    sizeof(shadnet_MatchingRoomGroupConfig_fields) / sizeof(shadnet_MatchingRoomGroupConfig_fields[0])};

static const pb_field_t shadnet_MatchingRoomDataInternal_fields[] = {
    {1, PB_UINT32, 0, offsetof(shadnet_MatchingRoomDataInternal, public_slots), 0, NULL},
    {2, PB_UINT32, 0, offsetof(shadnet_MatchingRoomDataInternal, private_slots), 0, NULL},
    {3, PB_UINT32, 0, offsetof(shadnet_MatchingRoomDataInternal, open_public_slots), 0, NULL},
    {4, PB_UINT32, 0, offsetof(shadnet_MatchingRoomDataInternal, open_private_slots), 0, NULL},
    {5, PB_UINT32, 0, offsetof(shadnet_MatchingRoomDataInternal, max_slot), 0, NULL},
    {6, PB_UINT32, 0, offsetof(shadnet_MatchingRoomDataInternal, server_id), 0, NULL},
    {7, PB_UINT32, 0, offsetof(shadnet_MatchingRoomDataInternal, world_id), 0, NULL},
    {8, PB_UINT32, 0, offsetof(shadnet_MatchingRoomDataInternal, lobby_id), 0, NULL},
    {9, PB_UINT64, 0, offsetof(shadnet_MatchingRoomDataInternal, room_id), 0, NULL},
    {10, PB_UINT64, 0, offsetof(shadnet_MatchingRoomDataInternal, passwd_slot_mask), 0, NULL},
    {11, PB_UINT32, 0, offsetof(shadnet_MatchingRoomDataInternal, joined_slot_mask), 0, NULL},
    {12, PB_MESSAGE, 1, offsetof(shadnet_MatchingRoomDataInternal, groups), 0, &shadnet_MatchingRoomGroup_desc},
    {13, PB_UINT32, 0, offsetof(shadnet_MatchingRoomDataInternal, flags), 0, NULL},
    {14, PB_MESSAGE, 1, offsetof(shadnet_MatchingRoomDataInternal, bin_attrs_internal), 0, &shadnet_MatchingBinAttr_desc},
};
const pb_msg_desc_t shadnet_MatchingRoomDataInternal_desc = {sizeof(shadnet_MatchingRoomDataInternal), shadnet_MatchingRoomDataInternal_fields,
    sizeof(shadnet_MatchingRoomDataInternal_fields) / sizeof(shadnet_MatchingRoomDataInternal_fields[0])};

static const pb_field_t shadnet_MatchingRoomMemberData_fields[] = {
    {1, PB_STRING, 0, offsetof(shadnet_MatchingRoomMemberData, npid), 0, NULL},
    {2, PB_UINT32, 0, offsetof(shadnet_MatchingRoomMemberData, member_id), 0, NULL},
    {3, PB_UINT32, 0, offsetof(shadnet_MatchingRoomMemberData, team_id), 0, NULL},
    {4, PB_BOOL, 0, offsetof(shadnet_MatchingRoomMemberData, is_owner), 0, NULL},
    {5, PB_MESSAGE, 0, offsetof(shadnet_MatchingRoomMemberData, group), offsetof(shadnet_MatchingRoomMemberData, has_group), &shadnet_MatchingRoomGroup_desc},
    {6, PB_MESSAGE, 1, offsetof(shadnet_MatchingRoomMemberData, bin_attrs_internal), 0, &shadnet_MatchingBinAttr_desc},
    {7, PB_UINT64, 0, offsetof(shadnet_MatchingRoomMemberData, join_date), 0, NULL},
    {8, PB_UINT32, 0, offsetof(shadnet_MatchingRoomMemberData, nat_type), 0, NULL},
    {9, PB_UINT32, 0, offsetof(shadnet_MatchingRoomMemberData, flag_attr), 0, NULL},
    {10, PB_STRING, 0, offsetof(shadnet_MatchingRoomMemberData, addr), 0, NULL},
    {11, PB_UINT32, 0, offsetof(shadnet_MatchingRoomMemberData, port), 0, NULL},
    {12, PB_UINT32, 0, offsetof(shadnet_MatchingRoomMemberData, group_id), 0, NULL},
    {13, PB_UINT64, 0, offsetof(shadnet_MatchingRoomMemberData, account_id), 0, NULL},
    {14, PB_UINT32, 0, offsetof(shadnet_MatchingRoomMemberData, platform), 0, NULL},
};
const pb_msg_desc_t shadnet_MatchingRoomMemberData_desc = {sizeof(shadnet_MatchingRoomMemberData), shadnet_MatchingRoomMemberData_fields,
    sizeof(shadnet_MatchingRoomMemberData_fields) / sizeof(shadnet_MatchingRoomMemberData_fields[0])};

static const pb_field_t shadnet_MatchingRoomMemberDataExternal_fields[] = {
    {1, PB_STRING, 0, offsetof(shadnet_MatchingRoomMemberDataExternal, npid), 0, NULL},
    {2, PB_UINT32, 0, offsetof(shadnet_MatchingRoomMemberDataExternal, member_id), 0, NULL},
    {3, PB_UINT64, 0, offsetof(shadnet_MatchingRoomMemberDataExternal, account_id), 0, NULL},
    {4, PB_UINT32, 0, offsetof(shadnet_MatchingRoomMemberDataExternal, platform), 0, NULL},
    {5, PB_UINT64, 0, offsetof(shadnet_MatchingRoomMemberDataExternal, join_date), 0, NULL},
    {6, PB_UINT32, 0, offsetof(shadnet_MatchingRoomMemberDataExternal, role), 0, NULL},
};
const pb_msg_desc_t shadnet_MatchingRoomMemberDataExternal_desc = {sizeof(shadnet_MatchingRoomMemberDataExternal), shadnet_MatchingRoomMemberDataExternal_fields,
    sizeof(shadnet_MatchingRoomMemberDataExternal_fields) / sizeof(shadnet_MatchingRoomMemberDataExternal_fields[0])};

static const pb_field_t shadnet_MatchingRoomDataExternal_fields[] = {
    {1, PB_UINT32, 0, offsetof(shadnet_MatchingRoomDataExternal, max_slot), 0, NULL},
    {2, PB_UINT32, 0, offsetof(shadnet_MatchingRoomDataExternal, cur_members), 0, NULL},
    {3, PB_UINT32, 0, offsetof(shadnet_MatchingRoomDataExternal, flags), 0, NULL},
    {4, PB_UINT32, 0, offsetof(shadnet_MatchingRoomDataExternal, server_id), 0, NULL},
    {5, PB_UINT32, 0, offsetof(shadnet_MatchingRoomDataExternal, world_id), 0, NULL},
    {6, PB_UINT64, 0, offsetof(shadnet_MatchingRoomDataExternal, lobby_id), 0, NULL},
    {7, PB_UINT64, 0, offsetof(shadnet_MatchingRoomDataExternal, room_id), 0, NULL},
    {8, PB_UINT64, 0, offsetof(shadnet_MatchingRoomDataExternal, passwd_slot_mask), 0, NULL},
    {9, PB_UINT64, 0, offsetof(shadnet_MatchingRoomDataExternal, joined_slot_mask), 0, NULL},
    {10, PB_UINT32, 0, offsetof(shadnet_MatchingRoomDataExternal, public_slots), 0, NULL},
    {11, PB_UINT32, 0, offsetof(shadnet_MatchingRoomDataExternal, private_slots), 0, NULL},
    {12, PB_UINT32, 0, offsetof(shadnet_MatchingRoomDataExternal, open_public_slots), 0, NULL},
    {13, PB_UINT32, 0, offsetof(shadnet_MatchingRoomDataExternal, open_private_slots), 0, NULL},
    {14, PB_MESSAGE, 1, offsetof(shadnet_MatchingRoomDataExternal, groups), 0, &shadnet_MatchingRoomGroup_desc},
    {15, PB_MESSAGE, 1, offsetof(shadnet_MatchingRoomDataExternal, external_search_int_attrs), 0, &shadnet_MatchingIntAttr_desc},
    {16, PB_MESSAGE, 1, offsetof(shadnet_MatchingRoomDataExternal, external_search_bin_attrs), 0, &shadnet_MatchingBinAttr_desc},
    {17, PB_MESSAGE, 1, offsetof(shadnet_MatchingRoomDataExternal, external_bin_attrs), 0, &shadnet_MatchingBinAttr_desc},
    {18, PB_STRING, 0, offsetof(shadnet_MatchingRoomDataExternal, owner_npid), 0, NULL},
    {19, PB_UINT64, 0, offsetof(shadnet_MatchingRoomDataExternal, owner_account_id), 0, NULL},
    {20, PB_UINT32, 0, offsetof(shadnet_MatchingRoomDataExternal, owner_platform), 0, NULL},
};
const pb_msg_desc_t shadnet_MatchingRoomDataExternal_desc = {sizeof(shadnet_MatchingRoomDataExternal), shadnet_MatchingRoomDataExternal_fields,
    sizeof(shadnet_MatchingRoomDataExternal_fields) / sizeof(shadnet_MatchingRoomDataExternal_fields[0])};

static const pb_field_t shadnet_CreateJoinRoomResponse_fields[] = {
    {1, PB_MESSAGE, 0, offsetof(shadnet_CreateJoinRoomResponse, room_data), offsetof(shadnet_CreateJoinRoomResponse, has_room_data), &shadnet_MatchingRoomDataInternal_desc},
    {2, PB_MESSAGE, 1, offsetof(shadnet_CreateJoinRoomResponse, members), 0, &shadnet_MatchingRoomMemberData_desc},
    {3, PB_UINT32, 0, offsetof(shadnet_CreateJoinRoomResponse, me_member_id), 0, NULL},
    {4, PB_UINT32, 0, offsetof(shadnet_CreateJoinRoomResponse, owner_member_id), 0, NULL},
};
const pb_msg_desc_t shadnet_CreateJoinRoomResponse_desc = {sizeof(shadnet_CreateJoinRoomResponse), shadnet_CreateJoinRoomResponse_fields,
    sizeof(shadnet_CreateJoinRoomResponse_fields) / sizeof(shadnet_CreateJoinRoomResponse_fields[0])};

static const pb_field_t shadnet_ContextStartRequest_fields[] = {
    {1, PB_UINT32, 0, offsetof(shadnet_ContextStartRequest, ctx_id), 0, NULL},
};
const pb_msg_desc_t shadnet_ContextStartRequest_desc = {sizeof(shadnet_ContextStartRequest), shadnet_ContextStartRequest_fields,
    sizeof(shadnet_ContextStartRequest_fields) / sizeof(shadnet_ContextStartRequest_fields[0])};

static const pb_field_t shadnet_ContextStopRequest_fields[] = {
    {1, PB_UINT32, 0, offsetof(shadnet_ContextStopRequest, ctx_id), 0, NULL},
};
const pb_msg_desc_t shadnet_ContextStopRequest_desc = {sizeof(shadnet_ContextStopRequest), shadnet_ContextStopRequest_fields,
    sizeof(shadnet_ContextStopRequest_fields) / sizeof(shadnet_ContextStopRequest_fields[0])};

static const pb_field_t shadnet_CreateRoomRequest_fields[] = {
    {1, PB_UINT32, 0, offsetof(shadnet_CreateRoomRequest, req_id), 0, NULL},
    {2, PB_UINT32, 0, offsetof(shadnet_CreateRoomRequest, max_slots), 0, NULL},
    {3, PB_UINT32, 0, offsetof(shadnet_CreateRoomRequest, team_id), 0, NULL},
    {4, PB_UINT32, 0, offsetof(shadnet_CreateRoomRequest, world_id), 0, NULL},
    {5, PB_UINT32, 0, offsetof(shadnet_CreateRoomRequest, lobby_id), 0, NULL},
    {6, PB_UINT32, 0, offsetof(shadnet_CreateRoomRequest, flags), 0, NULL},
    {7, PB_UINT32, 0, offsetof(shadnet_CreateRoomRequest, group_config_count), 0, NULL},
    {8, PB_UINT32, 0, offsetof(shadnet_CreateRoomRequest, allowed_user_count), 0, NULL},
    {9, PB_UINT32, 0, offsetof(shadnet_CreateRoomRequest, blocked_user_count), 0, NULL},
    {10, PB_UINT32, 0, offsetof(shadnet_CreateRoomRequest, internal_bin_attr_count), 0, NULL},
    {11, PB_MESSAGE, 1, offsetof(shadnet_CreateRoomRequest, external_search_int_attrs), 0, &shadnet_MatchingIntAttr_desc},
    {12, PB_MESSAGE, 1, offsetof(shadnet_CreateRoomRequest, external_search_bin_attrs), 0, &shadnet_MatchingBinAttr_desc},
    {13, PB_MESSAGE, 1, offsetof(shadnet_CreateRoomRequest, external_bin_attrs), 0, &shadnet_MatchingBinAttr_desc},
    {14, PB_MESSAGE, 1, offsetof(shadnet_CreateRoomRequest, member_bin_attrs), 0, &shadnet_MatchingBinAttr_desc},
    {15, PB_BOOL, 0, offsetof(shadnet_CreateRoomRequest, join_group_label_present), 0, NULL},
    {16, PB_BOOL, 0, offsetof(shadnet_CreateRoomRequest, room_password_present), 0, NULL},
    {17, PB_UINT32, 0, offsetof(shadnet_CreateRoomRequest, sig_type), 0, NULL},
    {18, PB_UINT32, 0, offsetof(shadnet_CreateRoomRequest, sig_flag), 0, NULL},
    {19, PB_UINT32, 0, offsetof(shadnet_CreateRoomRequest, sig_main_member), 0, NULL},
    {20, PB_MESSAGE, 1, offsetof(shadnet_CreateRoomRequest, internal_bin_attrs), 0, &shadnet_MatchingBinAttr_desc},
    {21, PB_INT32, 0, offsetof(shadnet_CreateRoomRequest, user_id_kind), 0, NULL},
    {22, PB_STRING, 1, offsetof(shadnet_CreateRoomRequest, allowed_online_ids), 0, NULL},
    {23, PB_STRING, 1, offsetof(shadnet_CreateRoomRequest, blocked_online_ids), 0, NULL},
    {24, PB_UINT64, 1, offsetof(shadnet_CreateRoomRequest, allowed_account_ids), 0, NULL},
    {25, PB_UINT64, 1, offsetof(shadnet_CreateRoomRequest, blocked_account_ids), 0, NULL},
    {26, PB_MESSAGE, 1, offsetof(shadnet_CreateRoomRequest, group_configs), 0, &shadnet_MatchingRoomGroupConfig_desc},
    {27, PB_BYTES, 0, offsetof(shadnet_CreateRoomRequest, room_password), 0, NULL},
    {28, PB_BOOL, 0, offsetof(shadnet_CreateRoomRequest, has_passwd_slot_mask), 0, NULL},
    {29, PB_UINT64, 0, offsetof(shadnet_CreateRoomRequest, passwd_slot_mask), 0, NULL},
    {30, PB_BYTES, 0, offsetof(shadnet_CreateRoomRequest, join_group_label), 0, NULL},
};
const pb_msg_desc_t shadnet_CreateRoomRequest_desc = {sizeof(shadnet_CreateRoomRequest), shadnet_CreateRoomRequest_fields,
    sizeof(shadnet_CreateRoomRequest_fields) / sizeof(shadnet_CreateRoomRequest_fields[0])};

static const pb_field_t shadnet_JoinRoomRequest_fields[] = {
    {1, PB_UINT64, 0, offsetof(shadnet_JoinRoomRequest, room_id), 0, NULL},
    {2, PB_UINT32, 0, offsetof(shadnet_JoinRoomRequest, req_id), 0, NULL},
    {3, PB_UINT32, 0, offsetof(shadnet_JoinRoomRequest, team_id), 0, NULL},
    {4, PB_UINT32, 0, offsetof(shadnet_JoinRoomRequest, join_flags), 0, NULL},
    {5, PB_UINT32, 0, offsetof(shadnet_JoinRoomRequest, blocked_user_count), 0, NULL},
    {6, PB_MESSAGE, 1, offsetof(shadnet_JoinRoomRequest, member_bin_attrs), 0, &shadnet_MatchingBinAttr_desc},
    {7, PB_BOOL, 0, offsetof(shadnet_JoinRoomRequest, room_password_present), 0, NULL},
    {8, PB_BOOL, 0, offsetof(shadnet_JoinRoomRequest, join_group_label_present), 0, NULL},
    {9, PB_INT32, 0, offsetof(shadnet_JoinRoomRequest, user_id_kind), 0, NULL},
    {10, PB_STRING, 1, offsetof(shadnet_JoinRoomRequest, blocked_online_ids), 0, NULL},
    {11, PB_UINT64, 1, offsetof(shadnet_JoinRoomRequest, blocked_account_ids), 0, NULL},
};
const pb_msg_desc_t shadnet_JoinRoomRequest_desc = {sizeof(shadnet_JoinRoomRequest), shadnet_JoinRoomRequest_fields,
    sizeof(shadnet_JoinRoomRequest_fields) / sizeof(shadnet_JoinRoomRequest_fields[0])};

static const pb_field_t shadnet_LeaveRoomRequest_fields[] = {
    {1, PB_UINT64, 0, offsetof(shadnet_LeaveRoomRequest, room_id), 0, NULL},
    {2, PB_UINT32, 0, offsetof(shadnet_LeaveRoomRequest, req_id), 0, NULL},
};
const pb_msg_desc_t shadnet_LeaveRoomRequest_desc = {sizeof(shadnet_LeaveRoomRequest), shadnet_LeaveRoomRequest_fields,
    sizeof(shadnet_LeaveRoomRequest_fields) / sizeof(shadnet_LeaveRoomRequest_fields[0])};

static const pb_field_t shadnet_RequestSignalingInfosRequest_fields[] = {
    {1, PB_STRING, 0, offsetof(shadnet_RequestSignalingInfosRequest, target_npid), 0, NULL},
};
const pb_msg_desc_t shadnet_RequestSignalingInfosRequest_desc = {sizeof(shadnet_RequestSignalingInfosRequest), shadnet_RequestSignalingInfosRequest_fields,
    sizeof(shadnet_RequestSignalingInfosRequest_fields) / sizeof(shadnet_RequestSignalingInfosRequest_fields[0])};

static const pb_field_t shadnet_SetRoomDataInternalRequest_fields[] = {
    {1, PB_UINT32, 0, offsetof(shadnet_SetRoomDataInternalRequest, req_id), 0, NULL},
    {2, PB_UINT64, 0, offsetof(shadnet_SetRoomDataInternalRequest, room_id), 0, NULL},
    {3, PB_UINT32, 0, offsetof(shadnet_SetRoomDataInternalRequest, flag_filter), 0, NULL},
    {4, PB_UINT32, 0, offsetof(shadnet_SetRoomDataInternalRequest, flag_attr), 0, NULL},
    {5, PB_MESSAGE, 1, offsetof(shadnet_SetRoomDataInternalRequest, bin_attrs), 0, &shadnet_MatchingBinAttr_desc},
    {6, PB_BOOL, 0, offsetof(shadnet_SetRoomDataInternalRequest, has_passwd_mask), 0, NULL},
    {7, PB_UINT64, 0, offsetof(shadnet_SetRoomDataInternalRequest, passwd_slot_mask), 0, NULL},
};
const pb_msg_desc_t shadnet_SetRoomDataInternalRequest_desc = {sizeof(shadnet_SetRoomDataInternalRequest), shadnet_SetRoomDataInternalRequest_fields,
    sizeof(shadnet_SetRoomDataInternalRequest_fields) / sizeof(shadnet_SetRoomDataInternalRequest_fields[0])};

static const pb_field_t shadnet_SetRoomMemberDataInternalRequest_fields[] = {
    {1, PB_UINT32, 0, offsetof(shadnet_SetRoomMemberDataInternalRequest, req_id), 0, NULL},
    {2, PB_UINT64, 0, offsetof(shadnet_SetRoomMemberDataInternalRequest, room_id), 0, NULL},
    {3, PB_UINT32, 0, offsetof(shadnet_SetRoomMemberDataInternalRequest, member_id), 0, NULL},
    {4, PB_UINT32, 0, offsetof(shadnet_SetRoomMemberDataInternalRequest, team_id), 0, NULL},
    {5, PB_UINT32, 0, offsetof(shadnet_SetRoomMemberDataInternalRequest, flag_filter), 0, NULL},
    {6, PB_UINT32, 0, offsetof(shadnet_SetRoomMemberDataInternalRequest, flag_attr), 0, NULL},
    {7, PB_MESSAGE, 1, offsetof(shadnet_SetRoomMemberDataInternalRequest, bin_attrs), 0, &shadnet_MatchingBinAttr_desc},
};
const pb_msg_desc_t shadnet_SetRoomMemberDataInternalRequest_desc = {sizeof(shadnet_SetRoomMemberDataInternalRequest), shadnet_SetRoomMemberDataInternalRequest_fields,
    sizeof(shadnet_SetRoomMemberDataInternalRequest_fields) / sizeof(shadnet_SetRoomMemberDataInternalRequest_fields[0])};

static const pb_field_t shadnet_SetRoomDataExternalRequest_fields[] = {
    {1, PB_UINT32, 0, offsetof(shadnet_SetRoomDataExternalRequest, req_id), 0, NULL},
    {2, PB_UINT64, 0, offsetof(shadnet_SetRoomDataExternalRequest, room_id), 0, NULL},
    {3, PB_MESSAGE, 1, offsetof(shadnet_SetRoomDataExternalRequest, search_int_attrs), 0, &shadnet_MatchingIntAttr_desc},
    {4, PB_MESSAGE, 1, offsetof(shadnet_SetRoomDataExternalRequest, search_bin_attrs), 0, &shadnet_MatchingBinAttr_desc},
    {5, PB_MESSAGE, 1, offsetof(shadnet_SetRoomDataExternalRequest, ext_bin_attrs), 0, &shadnet_MatchingBinAttr_desc},
};
const pb_msg_desc_t shadnet_SetRoomDataExternalRequest_desc = {sizeof(shadnet_SetRoomDataExternalRequest), shadnet_SetRoomDataExternalRequest_fields,
    sizeof(shadnet_SetRoomDataExternalRequest_fields) / sizeof(shadnet_SetRoomDataExternalRequest_fields[0])};

static const pb_field_t shadnet_KickoutRoomMemberRequest_fields[] = {
    {1, PB_UINT64, 0, offsetof(shadnet_KickoutRoomMemberRequest, room_id), 0, NULL},
    {2, PB_UINT32, 0, offsetof(shadnet_KickoutRoomMemberRequest, req_id), 0, NULL},
    {3, PB_UINT32, 0, offsetof(shadnet_KickoutRoomMemberRequest, target_member_id), 0, NULL},
    {4, PB_UINT32, 0, offsetof(shadnet_KickoutRoomMemberRequest, block_kick_flag), 0, NULL},
};
const pb_msg_desc_t shadnet_KickoutRoomMemberRequest_desc = {sizeof(shadnet_KickoutRoomMemberRequest), shadnet_KickoutRoomMemberRequest_fields,
    sizeof(shadnet_KickoutRoomMemberRequest_fields) / sizeof(shadnet_KickoutRoomMemberRequest_fields[0])};

static const pb_field_t shadnet_CreateRoomReply_fields[] = {
    {1, PB_UINT64, 0, offsetof(shadnet_CreateRoomReply, room_id), 0, NULL},
    {2, PB_UINT32, 0, offsetof(shadnet_CreateRoomReply, server_id), 0, NULL},
    {3, PB_UINT32, 0, offsetof(shadnet_CreateRoomReply, world_id), 0, NULL},
    {4, PB_UINT32, 0, offsetof(shadnet_CreateRoomReply, lobby_id), 0, NULL},
    {5, PB_UINT32, 0, offsetof(shadnet_CreateRoomReply, member_id), 0, NULL},
    {6, PB_UINT32, 0, offsetof(shadnet_CreateRoomReply, max_slots), 0, NULL},
    {7, PB_UINT32, 0, offsetof(shadnet_CreateRoomReply, flags), 0, NULL},
    {8, PB_UINT32, 0, offsetof(shadnet_CreateRoomReply, cur_members), 0, NULL},
    {9, PB_MESSAGE, 0, offsetof(shadnet_CreateRoomReply, details), offsetof(shadnet_CreateRoomReply, has_details), &shadnet_CreateJoinRoomResponse_desc},
};
const pb_msg_desc_t shadnet_CreateRoomReply_desc = {sizeof(shadnet_CreateRoomReply), shadnet_CreateRoomReply_fields,
    sizeof(shadnet_CreateRoomReply_fields) / sizeof(shadnet_CreateRoomReply_fields[0])};

static const pb_field_t shadnet_JoinRoomReply_fields[] = {
    {1, PB_UINT64, 0, offsetof(shadnet_JoinRoomReply, room_id), 0, NULL},
    {2, PB_UINT32, 0, offsetof(shadnet_JoinRoomReply, member_id), 0, NULL},
    {3, PB_UINT32, 0, offsetof(shadnet_JoinRoomReply, max_slots), 0, NULL},
    {4, PB_UINT32, 0, offsetof(shadnet_JoinRoomReply, flags), 0, NULL},
    {5, PB_UINT32, 0, offsetof(shadnet_JoinRoomReply, cur_members), 0, NULL},
    {6, PB_MESSAGE, 0, offsetof(shadnet_JoinRoomReply, details), offsetof(shadnet_JoinRoomReply, has_details), &shadnet_CreateJoinRoomResponse_desc},
};
const pb_msg_desc_t shadnet_JoinRoomReply_desc = {sizeof(shadnet_JoinRoomReply), shadnet_JoinRoomReply_fields,
    sizeof(shadnet_JoinRoomReply_fields) / sizeof(shadnet_JoinRoomReply_fields[0])};

static const pb_field_t shadnet_MatchingIntSearchFilter_fields[] = {
    {1, PB_UINT32, 0, offsetof(shadnet_MatchingIntSearchFilter, op), 0, NULL},
    {2, PB_UINT32, 0, offsetof(shadnet_MatchingIntSearchFilter, attr_id), 0, NULL},
    {3, PB_UINT32, 0, offsetof(shadnet_MatchingIntSearchFilter, attr_value), 0, NULL},
};
const pb_msg_desc_t shadnet_MatchingIntSearchFilter_desc = {sizeof(shadnet_MatchingIntSearchFilter), shadnet_MatchingIntSearchFilter_fields,
    sizeof(shadnet_MatchingIntSearchFilter_fields) / sizeof(shadnet_MatchingIntSearchFilter_fields[0])};

static const pb_field_t shadnet_MatchingBinSearchFilter_fields[] = {
    {1, PB_UINT32, 0, offsetof(shadnet_MatchingBinSearchFilter, op), 0, NULL},
    {2, PB_UINT32, 0, offsetof(shadnet_MatchingBinSearchFilter, attr_id), 0, NULL},
    {3, PB_BYTES, 0, offsetof(shadnet_MatchingBinSearchFilter, data), 0, NULL},
};
const pb_msg_desc_t shadnet_MatchingBinSearchFilter_desc = {sizeof(shadnet_MatchingBinSearchFilter), shadnet_MatchingBinSearchFilter_fields,
    sizeof(shadnet_MatchingBinSearchFilter_fields) / sizeof(shadnet_MatchingBinSearchFilter_fields[0])};

static const pb_field_t shadnet_SearchRoomRequest_fields[] = {
    {1, PB_UINT32, 0, offsetof(shadnet_SearchRoomRequest, world_id), 0, NULL},
    {2, PB_UINT64, 0, offsetof(shadnet_SearchRoomRequest, lobby_id), 0, NULL},
    {3, PB_UINT32, 0, offsetof(shadnet_SearchRoomRequest, range_filter_start), 0, NULL},
    {4, PB_UINT32, 0, offsetof(shadnet_SearchRoomRequest, range_filter_max), 0, NULL},
    {5, PB_UINT32, 0, offsetof(shadnet_SearchRoomRequest, flag_filter), 0, NULL},
    {6, PB_UINT32, 0, offsetof(shadnet_SearchRoomRequest, flag_attr), 0, NULL},
    {7, PB_MESSAGE, 1, offsetof(shadnet_SearchRoomRequest, int_filters), 0, &shadnet_MatchingIntSearchFilter_desc},
    {8, PB_MESSAGE, 1, offsetof(shadnet_SearchRoomRequest, bin_filters), 0, &shadnet_MatchingBinSearchFilter_desc},
    {9, PB_UINT32, 1, offsetof(shadnet_SearchRoomRequest, attr_ids), 0, NULL},
};
const pb_msg_desc_t shadnet_SearchRoomRequest_desc = {sizeof(shadnet_SearchRoomRequest), shadnet_SearchRoomRequest_fields,
    sizeof(shadnet_SearchRoomRequest_fields) / sizeof(shadnet_SearchRoomRequest_fields[0])};

static const pb_field_t shadnet_SearchRoomReply_fields[] = {
    {1, PB_MESSAGE, 1, offsetof(shadnet_SearchRoomReply, rooms), 0, &shadnet_MatchingRoomDataExternal_desc},
    {2, PB_UINT32, 0, offsetof(shadnet_SearchRoomReply, range_start), 0, NULL},
    {3, PB_UINT32, 0, offsetof(shadnet_SearchRoomReply, range_total), 0, NULL},
    {4, PB_UINT32, 0, offsetof(shadnet_SearchRoomReply, range_result), 0, NULL},
};
const pb_msg_desc_t shadnet_SearchRoomReply_desc = {sizeof(shadnet_SearchRoomReply), shadnet_SearchRoomReply_fields,
    sizeof(shadnet_SearchRoomReply_fields) / sizeof(shadnet_SearchRoomReply_fields[0])};

static const pb_field_t shadnet_GetRoomDataExternalListRequest_fields[] = {
    {1, PB_UINT64, 1, offsetof(shadnet_GetRoomDataExternalListRequest, room_ids), 0, NULL},
    {2, PB_UINT32, 1, offsetof(shadnet_GetRoomDataExternalListRequest, attr_ids), 0, NULL},
};
const pb_msg_desc_t shadnet_GetRoomDataExternalListRequest_desc = {sizeof(shadnet_GetRoomDataExternalListRequest), shadnet_GetRoomDataExternalListRequest_fields,
    sizeof(shadnet_GetRoomDataExternalListRequest_fields) / sizeof(shadnet_GetRoomDataExternalListRequest_fields[0])};

static const pb_field_t shadnet_GetRoomDataExternalListReply_fields[] = {
    {1, PB_MESSAGE, 1, offsetof(shadnet_GetRoomDataExternalListReply, rooms), 0, &shadnet_MatchingRoomDataExternal_desc},
};
const pb_msg_desc_t shadnet_GetRoomDataExternalListReply_desc = {sizeof(shadnet_GetRoomDataExternalListReply), shadnet_GetRoomDataExternalListReply_fields,
    sizeof(shadnet_GetRoomDataExternalListReply_fields) / sizeof(shadnet_GetRoomDataExternalListReply_fields[0])};

static const pb_field_t shadnet_GetRoomMemberDataExternalListRequest_fields[] = {
    {1, PB_UINT64, 0, offsetof(shadnet_GetRoomMemberDataExternalListRequest, room_id), 0, NULL},
};
const pb_msg_desc_t shadnet_GetRoomMemberDataExternalListRequest_desc = {sizeof(shadnet_GetRoomMemberDataExternalListRequest), shadnet_GetRoomMemberDataExternalListRequest_fields,
    sizeof(shadnet_GetRoomMemberDataExternalListRequest_fields) / sizeof(shadnet_GetRoomMemberDataExternalListRequest_fields[0])};

static const pb_field_t shadnet_GetRoomMemberDataExternalListReply_fields[] = {
    {1, PB_MESSAGE, 1, offsetof(shadnet_GetRoomMemberDataExternalListReply, members), 0, &shadnet_MatchingRoomMemberDataExternal_desc},
};
const pb_msg_desc_t shadnet_GetRoomMemberDataExternalListReply_desc = {sizeof(shadnet_GetRoomMemberDataExternalListReply), shadnet_GetRoomMemberDataExternalListReply_fields,
    sizeof(shadnet_GetRoomMemberDataExternalListReply_fields) / sizeof(shadnet_GetRoomMemberDataExternalListReply_fields[0])};

static const pb_field_t shadnet_MatchingUserInfo_fields[] = {
    {1, PB_STRING, 0, offsetof(shadnet_MatchingUserInfo, npid), 0, NULL},
    {2, PB_UINT64, 0, offsetof(shadnet_MatchingUserInfo, account_id), 0, NULL},
    {3, PB_UINT32, 0, offsetof(shadnet_MatchingUserInfo, platform), 0, NULL},
    {4, PB_MESSAGE, 1, offsetof(shadnet_MatchingUserInfo, user_bin_attrs), 0, &shadnet_MatchingBinAttr_desc},
};
const pb_msg_desc_t shadnet_MatchingUserInfo_desc = {sizeof(shadnet_MatchingUserInfo), shadnet_MatchingUserInfo_fields,
    sizeof(shadnet_MatchingUserInfo_fields) / sizeof(shadnet_MatchingUserInfo_fields[0])};

static const pb_field_t shadnet_SetUserInfoRequest_fields[] = {
    {1, PB_UINT32, 0, offsetof(shadnet_SetUserInfoRequest, server_id), 0, NULL},
    {2, PB_MESSAGE, 1, offsetof(shadnet_SetUserInfoRequest, user_bin_attrs), 0, &shadnet_MatchingBinAttr_desc},
};
const pb_msg_desc_t shadnet_SetUserInfoRequest_desc = {sizeof(shadnet_SetUserInfoRequest), shadnet_SetUserInfoRequest_fields,
    sizeof(shadnet_SetUserInfoRequest_fields) / sizeof(shadnet_SetUserInfoRequest_fields[0])};

const pb_msg_desc_t shadnet_SetUserInfoReply_desc = {sizeof(shadnet_SetUserInfoReply), NULL, 0};

static const pb_field_t shadnet_GetUserInfoListRequest_fields[] = {
    {1, PB_STRING, 1, offsetof(shadnet_GetUserInfoListRequest, npids), 0, NULL},
    {2, PB_UINT32, 1, offsetof(shadnet_GetUserInfoListRequest, attr_ids), 0, NULL},
    {3, PB_INT32, 0, offsetof(shadnet_GetUserInfoListRequest, option), 0, NULL},
};
const pb_msg_desc_t shadnet_GetUserInfoListRequest_desc = {sizeof(shadnet_GetUserInfoListRequest), shadnet_GetUserInfoListRequest_fields,
    sizeof(shadnet_GetUserInfoListRequest_fields) / sizeof(shadnet_GetUserInfoListRequest_fields[0])};

static const pb_field_t shadnet_GetUserInfoListReply_fields[] = {
    {1, PB_MESSAGE, 1, offsetof(shadnet_GetUserInfoListReply, users), 0, &shadnet_MatchingUserInfo_desc},
};
const pb_msg_desc_t shadnet_GetUserInfoListReply_desc = {sizeof(shadnet_GetUserInfoListReply), shadnet_GetUserInfoListReply_fields,
    sizeof(shadnet_GetUserInfoListReply_fields) / sizeof(shadnet_GetUserInfoListReply_fields[0])};

static const pb_field_t shadnet_MatchingWorld_fields[] = {
    {1, PB_UINT32, 0, offsetof(shadnet_MatchingWorld, world_id), 0, NULL},
    {2, PB_UINT32, 0, offsetof(shadnet_MatchingWorld, lobbies_num), 0, NULL},
    {3, PB_UINT32, 0, offsetof(shadnet_MatchingWorld, max_lobby_members), 0, NULL},
    {4, PB_UINT32, 0, offsetof(shadnet_MatchingWorld, lobby_members_num), 0, NULL},
    {5, PB_UINT32, 0, offsetof(shadnet_MatchingWorld, rooms_num), 0, NULL},
    {6, PB_UINT32, 0, offsetof(shadnet_MatchingWorld, room_members_num), 0, NULL},
};
const pb_msg_desc_t shadnet_MatchingWorld_desc = {sizeof(shadnet_MatchingWorld), shadnet_MatchingWorld_fields,
    sizeof(shadnet_MatchingWorld_fields) / sizeof(shadnet_MatchingWorld_fields[0])};

static const pb_field_t shadnet_GetWorldInfoListRequest_fields[] = {
    {1, PB_UINT32, 0, offsetof(shadnet_GetWorldInfoListRequest, server_id), 0, NULL},
};
const pb_msg_desc_t shadnet_GetWorldInfoListRequest_desc = {sizeof(shadnet_GetWorldInfoListRequest), shadnet_GetWorldInfoListRequest_fields,
    sizeof(shadnet_GetWorldInfoListRequest_fields) / sizeof(shadnet_GetWorldInfoListRequest_fields[0])};

static const pb_field_t shadnet_GetWorldInfoListReply_fields[] = {
    {1, PB_MESSAGE, 1, offsetof(shadnet_GetWorldInfoListReply, worlds), 0, &shadnet_MatchingWorld_desc},
};
const pb_msg_desc_t shadnet_GetWorldInfoListReply_desc = {sizeof(shadnet_GetWorldInfoListReply), shadnet_GetWorldInfoListReply_fields,
    sizeof(shadnet_GetWorldInfoListReply_fields) / sizeof(shadnet_GetWorldInfoListReply_fields[0])};

static const pb_field_t shadnet_RequestSignalingInfosReply_fields[] = {
    {1, PB_STRING, 0, offsetof(shadnet_RequestSignalingInfosReply, target_npid), 0, NULL},
    {2, PB_STRING, 0, offsetof(shadnet_RequestSignalingInfosReply, target_ip), 0, NULL},
    {3, PB_UINT32, 0, offsetof(shadnet_RequestSignalingInfosReply, target_port), 0, NULL},
    {4, PB_UINT32, 0, offsetof(shadnet_RequestSignalingInfosReply, target_member_id), 0, NULL},
};
const pb_msg_desc_t shadnet_RequestSignalingInfosReply_desc = {sizeof(shadnet_RequestSignalingInfosReply), shadnet_RequestSignalingInfosReply_fields,
    sizeof(shadnet_RequestSignalingInfosReply_fields) / sizeof(shadnet_RequestSignalingInfosReply_fields[0])};

static const pb_field_t shadnet_LeaveRoomReply_fields[] = {
    {1, PB_UINT64, 0, offsetof(shadnet_LeaveRoomReply, room_id), 0, NULL},
};
const pb_msg_desc_t shadnet_LeaveRoomReply_desc = {sizeof(shadnet_LeaveRoomReply), shadnet_LeaveRoomReply_fields,
    sizeof(shadnet_LeaveRoomReply_fields) / sizeof(shadnet_LeaveRoomReply_fields[0])};

static const pb_field_t shadnet_SetRoomDataInternalReply_fields[] = {
    {1, PB_UINT64, 0, offsetof(shadnet_SetRoomDataInternalReply, room_id), 0, NULL},
};
const pb_msg_desc_t shadnet_SetRoomDataInternalReply_desc = {sizeof(shadnet_SetRoomDataInternalReply), shadnet_SetRoomDataInternalReply_fields,
    sizeof(shadnet_SetRoomDataInternalReply_fields) / sizeof(shadnet_SetRoomDataInternalReply_fields[0])};

static const pb_field_t shadnet_SetRoomMemberDataInternalReply_fields[] = {
    {1, PB_UINT64, 0, offsetof(shadnet_SetRoomMemberDataInternalReply, room_id), 0, NULL},
};
const pb_msg_desc_t shadnet_SetRoomMemberDataInternalReply_desc = {sizeof(shadnet_SetRoomMemberDataInternalReply), shadnet_SetRoomMemberDataInternalReply_fields,
    sizeof(shadnet_SetRoomMemberDataInternalReply_fields) / sizeof(shadnet_SetRoomMemberDataInternalReply_fields[0])};

static const pb_field_t shadnet_SetRoomDataExternalReply_fields[] = {
    {1, PB_UINT64, 0, offsetof(shadnet_SetRoomDataExternalReply, room_id), 0, NULL},
};
const pb_msg_desc_t shadnet_SetRoomDataExternalReply_desc = {sizeof(shadnet_SetRoomDataExternalReply), shadnet_SetRoomDataExternalReply_fields,
    sizeof(shadnet_SetRoomDataExternalReply_fields) / sizeof(shadnet_SetRoomDataExternalReply_fields[0])};

static const pb_field_t shadnet_KickoutRoomMemberReply_fields[] = {
    {1, PB_UINT64, 0, offsetof(shadnet_KickoutRoomMemberReply, room_id), 0, NULL},
};
const pb_msg_desc_t shadnet_KickoutRoomMemberReply_desc = {sizeof(shadnet_KickoutRoomMemberReply), shadnet_KickoutRoomMemberReply_fields,
    sizeof(shadnet_KickoutRoomMemberReply_fields) / sizeof(shadnet_KickoutRoomMemberReply_fields[0])};

static const pb_field_t shadnet_SendRoomMessageRequest_fields[] = {
    {1, PB_UINT64, 0, offsetof(shadnet_SendRoomMessageRequest, room_id), 0, NULL},
    {2, PB_UINT32, 0, offsetof(shadnet_SendRoomMessageRequest, cast_type), 0, NULL},
    {3, PB_UINT32, 1, offsetof(shadnet_SendRoomMessageRequest, dst_member_ids), 0, NULL},
    {4, PB_BYTES, 0, offsetof(shadnet_SendRoomMessageRequest, msg), 0, NULL},
    {5, PB_INT32, 0, offsetof(shadnet_SendRoomMessageRequest, option), 0, NULL},
};
const pb_msg_desc_t shadnet_SendRoomMessageRequest_desc = {sizeof(shadnet_SendRoomMessageRequest), shadnet_SendRoomMessageRequest_fields,
    sizeof(shadnet_SendRoomMessageRequest_fields) / sizeof(shadnet_SendRoomMessageRequest_fields[0])};

static const pb_field_t shadnet_SendRoomMessageReply_fields[] = {
    {1, PB_UINT64, 0, offsetof(shadnet_SendRoomMessageReply, room_id), 0, NULL},
};
const pb_msg_desc_t shadnet_SendRoomMessageReply_desc = {sizeof(shadnet_SendRoomMessageReply), shadnet_SendRoomMessageReply_fields,
    sizeof(shadnet_SendRoomMessageReply_fields) / sizeof(shadnet_SendRoomMessageReply_fields[0])};

static const pb_field_t shadnet_GetTokenReply_fields[] = {
    {1, PB_STRING, 0, offsetof(shadnet_GetTokenReply, token), 0, NULL},
    {2, PB_UINT64, 0, offsetof(shadnet_GetTokenReply, user_id), 0, NULL},
    {3, PB_STRING, 0, offsetof(shadnet_GetTokenReply, npid), 0, NULL},
};
const pb_msg_desc_t shadnet_GetTokenReply_desc = {sizeof(shadnet_GetTokenReply), shadnet_GetTokenReply_fields,
    sizeof(shadnet_GetTokenReply_fields) / sizeof(shadnet_GetTokenReply_fields[0])};

static const pb_field_t shadnet_LookupOnlineIdRequest_fields[] = {
    {1, PB_STRING, 0, offsetof(shadnet_LookupOnlineIdRequest, npid), 0, NULL},
};
const pb_msg_desc_t shadnet_LookupOnlineIdRequest_desc = {sizeof(shadnet_LookupOnlineIdRequest), shadnet_LookupOnlineIdRequest_fields,
    sizeof(shadnet_LookupOnlineIdRequest_fields) / sizeof(shadnet_LookupOnlineIdRequest_fields[0])};

static const pb_field_t shadnet_LookupOnlineIdReply_fields[] = {
    {1, PB_UINT64, 0, offsetof(shadnet_LookupOnlineIdReply, account_id), 0, NULL},
    {2, PB_STRING, 0, offsetof(shadnet_LookupOnlineIdReply, npid), 0, NULL},
};
const pb_msg_desc_t shadnet_LookupOnlineIdReply_desc = {sizeof(shadnet_LookupOnlineIdReply), shadnet_LookupOnlineIdReply_fields,
    sizeof(shadnet_LookupOnlineIdReply_fields) / sizeof(shadnet_LookupOnlineIdReply_fields[0])};

static const pb_field_t shadnet_GetAuthorizationCodeRequest_fields[] = {
    {1, PB_STRING, 0, offsetof(shadnet_GetAuthorizationCodeRequest, client_id), 0, NULL},
    {2, PB_STRING, 0, offsetof(shadnet_GetAuthorizationCodeRequest, scope), 0, NULL},
};
const pb_msg_desc_t shadnet_GetAuthorizationCodeRequest_desc = {sizeof(shadnet_GetAuthorizationCodeRequest), shadnet_GetAuthorizationCodeRequest_fields,
    sizeof(shadnet_GetAuthorizationCodeRequest_fields) / sizeof(shadnet_GetAuthorizationCodeRequest_fields[0])};

static const pb_field_t shadnet_GetAuthorizationCodeReply_fields[] = {
    {1, PB_STRING, 0, offsetof(shadnet_GetAuthorizationCodeReply, code), 0, NULL},
    {2, PB_UINT32, 0, offsetof(shadnet_GetAuthorizationCodeReply, expires_in), 0, NULL},
};
const pb_msg_desc_t shadnet_GetAuthorizationCodeReply_desc = {sizeof(shadnet_GetAuthorizationCodeReply), shadnet_GetAuthorizationCodeReply_fields,
    sizeof(shadnet_GetAuthorizationCodeReply_fields) / sizeof(shadnet_GetAuthorizationCodeReply_fields[0])};

static const pb_field_t shadnet_NotifyRoomEvent_fields[] = {
    {1, PB_UINT32, 0, offsetof(shadnet_NotifyRoomEvent, ctx_id), 0, NULL},
    {2, PB_UINT64, 0, offsetof(shadnet_NotifyRoomEvent, room_id), 0, NULL},
    {3, PB_UINT32, 0, offsetof(shadnet_NotifyRoomEvent, event), 0, NULL},
    {4, PB_UINT32, 0, offsetof(shadnet_NotifyRoomEvent, event_cause), 0, NULL},
    {5, PB_INT32, 0, offsetof(shadnet_NotifyRoomEvent, error_code), 0, NULL},
    {6, PB_MESSAGE, 0, offsetof(shadnet_NotifyRoomEvent, member), offsetof(shadnet_NotifyRoomEvent, has_member), &shadnet_MatchingRoomMemberData_desc},
    {7, PB_MESSAGE, 1, offsetof(shadnet_NotifyRoomEvent, bin_attrs), 0, &shadnet_MatchingBinAttr_desc},
    {8, PB_UINT32, 0, offsetof(shadnet_NotifyRoomEvent, flags), 0, NULL},
    {9, PB_BOOL, 0, offsetof(shadnet_NotifyRoomEvent, has_passwd_mask), 0, NULL},
    {10, PB_UINT64, 0, offsetof(shadnet_NotifyRoomEvent, passwd_slot_mask), 0, NULL},
};
const pb_msg_desc_t shadnet_NotifyRoomEvent_desc = {sizeof(shadnet_NotifyRoomEvent), shadnet_NotifyRoomEvent_fields,
    sizeof(shadnet_NotifyRoomEvent_fields) / sizeof(shadnet_NotifyRoomEvent_fields[0])};

static const pb_field_t shadnet_NotifyRoomMessage_fields[] = {
    {1, PB_UINT32, 0, offsetof(shadnet_NotifyRoomMessage, ctx_id), 0, NULL},
    {2, PB_UINT64, 0, offsetof(shadnet_NotifyRoomMessage, room_id), 0, NULL},
    {3, PB_UINT32, 0, offsetof(shadnet_NotifyRoomMessage, src_member_id), 0, NULL},
    {4, PB_UINT32, 0, offsetof(shadnet_NotifyRoomMessage, event), 0, NULL},
    {5, PB_UINT32, 0, offsetof(shadnet_NotifyRoomMessage, cast_type), 0, NULL},
    {6, PB_UINT32, 1, offsetof(shadnet_NotifyRoomMessage, dst_member_ids), 0, NULL},
    {7, PB_STRING, 0, offsetof(shadnet_NotifyRoomMessage, src_npid), 0, NULL},
    {8, PB_UINT64, 0, offsetof(shadnet_NotifyRoomMessage, src_account_id), 0, NULL},
    {9, PB_UINT32, 0, offsetof(shadnet_NotifyRoomMessage, src_platform), 0, NULL},
    {10, PB_BYTES, 0, offsetof(shadnet_NotifyRoomMessage, msg), 0, NULL},
};
const pb_msg_desc_t shadnet_NotifyRoomMessage_desc = {sizeof(shadnet_NotifyRoomMessage), shadnet_NotifyRoomMessage_fields,
    sizeof(shadnet_NotifyRoomMessage_fields) / sizeof(shadnet_NotifyRoomMessage_fields[0])};

static const pb_field_t shadnet_TusVariable_fields[] = {
    {1, PB_STRING, 0, offsetof(shadnet_TusVariable, ownerNpId), 0, NULL},
    {2, PB_BOOL, 0, offsetof(shadnet_TusVariable, set), 0, NULL},
    {3, PB_INT64, 0, offsetof(shadnet_TusVariable, variable), 0, NULL},
    {4, PB_INT64, 0, offsetof(shadnet_TusVariable, oldVariable), 0, NULL},
    {5, PB_UINT64, 0, offsetof(shadnet_TusVariable, lastChangedDate), 0, NULL},
    {6, PB_STRING, 0, offsetof(shadnet_TusVariable, lastChangedAuthorNpId), 0, NULL},
    {7, PB_INT64, 0, offsetof(shadnet_TusVariable, ownerAccountId), 0, NULL},
    {8, PB_INT64, 0, offsetof(shadnet_TusVariable, lastChangedAuthorAccountId), 0, NULL},
};
const pb_msg_desc_t shadnet_TusVariable_desc = {sizeof(shadnet_TusVariable), shadnet_TusVariable_fields,
    sizeof(shadnet_TusVariable_fields) / sizeof(shadnet_TusVariable_fields[0])};

static const pb_field_t shadnet_TusDataStatus_fields[] = {
    {1, PB_STRING, 0, offsetof(shadnet_TusDataStatus, ownerNpId), 0, NULL},
    {2, PB_BOOL, 0, offsetof(shadnet_TusDataStatus, set), 0, NULL},
    {3, PB_UINT64, 0, offsetof(shadnet_TusDataStatus, lastChangedDate), 0, NULL},
    {4, PB_STRING, 0, offsetof(shadnet_TusDataStatus, lastChangedAuthorNpId), 0, NULL},
    {5, PB_UINT64, 0, offsetof(shadnet_TusDataStatus, dataSize), 0, NULL},
    {6, PB_BYTES, 0, offsetof(shadnet_TusDataStatus, info), 0, NULL},
    {7, PB_INT64, 0, offsetof(shadnet_TusDataStatus, ownerAccountId), 0, NULL},
    {8, PB_INT64, 0, offsetof(shadnet_TusDataStatus, lastChangedAuthorAccountId), 0, NULL},
};
const pb_msg_desc_t shadnet_TusDataStatus_desc = {sizeof(shadnet_TusDataStatus), shadnet_TusDataStatus_fields,
    sizeof(shadnet_TusDataStatus_fields) / sizeof(shadnet_TusDataStatus_fields[0])};

static const pb_field_t shadnet_TusSetMultiSlotVariableRequest_fields[] = {
    {1, PB_STRING, 0, offsetof(shadnet_TusSetMultiSlotVariableRequest, ownerNpId), 0, NULL},
    {2, PB_INT32, 1, offsetof(shadnet_TusSetMultiSlotVariableRequest, slotIds), 0, NULL},
    {3, PB_INT64, 1, offsetof(shadnet_TusSetMultiSlotVariableRequest, values), 0, NULL},
    {4, PB_INT64, 0, offsetof(shadnet_TusSetMultiSlotVariableRequest, ownerAccountId), 0, NULL},
    {5, PB_STRING, 0, offsetof(shadnet_TusSetMultiSlotVariableRequest, virtualUser), 0, NULL},
};
const pb_msg_desc_t shadnet_TusSetMultiSlotVariableRequest_desc = {sizeof(shadnet_TusSetMultiSlotVariableRequest), shadnet_TusSetMultiSlotVariableRequest_fields,
    sizeof(shadnet_TusSetMultiSlotVariableRequest_fields) / sizeof(shadnet_TusSetMultiSlotVariableRequest_fields[0])};

static const pb_field_t shadnet_TusGetMultiSlotVariableRequest_fields[] = {
    {1, PB_STRING, 0, offsetof(shadnet_TusGetMultiSlotVariableRequest, ownerNpId), 0, NULL},
    {2, PB_INT32, 1, offsetof(shadnet_TusGetMultiSlotVariableRequest, slotIds), 0, NULL},
    {3, PB_INT64, 0, offsetof(shadnet_TusGetMultiSlotVariableRequest, ownerAccountId), 0, NULL},
    {4, PB_STRING, 0, offsetof(shadnet_TusGetMultiSlotVariableRequest, virtualUser), 0, NULL},
};
const pb_msg_desc_t shadnet_TusGetMultiSlotVariableRequest_desc = {sizeof(shadnet_TusGetMultiSlotVariableRequest), shadnet_TusGetMultiSlotVariableRequest_fields,
    sizeof(shadnet_TusGetMultiSlotVariableRequest_fields) / sizeof(shadnet_TusGetMultiSlotVariableRequest_fields[0])};

static const pb_field_t shadnet_TusAddAndGetVariableRequest_fields[] = {
    {1, PB_STRING, 0, offsetof(shadnet_TusAddAndGetVariableRequest, ownerNpId), 0, NULL},
    {2, PB_INT32, 0, offsetof(shadnet_TusAddAndGetVariableRequest, slotId), 0, NULL},
    {3, PB_INT64, 0, offsetof(shadnet_TusAddAndGetVariableRequest, inValue), 0, NULL},
    {4, PB_INT64, 0, offsetof(shadnet_TusAddAndGetVariableRequest, ownerAccountId), 0, NULL},
    {5, PB_STRING, 0, offsetof(shadnet_TusAddAndGetVariableRequest, virtualUser), 0, NULL},
    {6, PB_BOOL, 0, offsetof(shadnet_TusAddAndGetVariableRequest, hasAuthorCheck), 0, NULL},
    {7, PB_INT64, 0, offsetof(shadnet_TusAddAndGetVariableRequest, isLastChangedAuthor), 0, NULL},
    {8, PB_BOOL, 0, offsetof(shadnet_TusAddAndGetVariableRequest, hasDateCheck), 0, NULL},
    {9, PB_UINT64, 0, offsetof(shadnet_TusAddAndGetVariableRequest, isLastChangedDate), 0, NULL},
    {10, PB_STRING, 0, offsetof(shadnet_TusAddAndGetVariableRequest, isLastChangedAuthorNpId), 0, NULL},
};
const pb_msg_desc_t shadnet_TusAddAndGetVariableRequest_desc = {sizeof(shadnet_TusAddAndGetVariableRequest), shadnet_TusAddAndGetVariableRequest_fields,
    sizeof(shadnet_TusAddAndGetVariableRequest_fields) / sizeof(shadnet_TusAddAndGetVariableRequest_fields[0])};

static const pb_field_t shadnet_TusVariableResponse_fields[] = {
    {1, PB_MESSAGE, 1, offsetof(shadnet_TusVariableResponse, variables), 0, &shadnet_TusVariable_desc},
    {2, PB_UINT32, 0, offsetof(shadnet_TusVariableResponse, total), 0, NULL},
};
const pb_msg_desc_t shadnet_TusVariableResponse_desc = {sizeof(shadnet_TusVariableResponse), shadnet_TusVariableResponse_fields,
    sizeof(shadnet_TusVariableResponse_fields) / sizeof(shadnet_TusVariableResponse_fields[0])};

static const pb_field_t shadnet_TusSetDataRequest_fields[] = {
    {1, PB_STRING, 0, offsetof(shadnet_TusSetDataRequest, ownerNpId), 0, NULL},
    {2, PB_INT32, 0, offsetof(shadnet_TusSetDataRequest, slotId), 0, NULL},
    {3, PB_BYTES, 0, offsetof(shadnet_TusSetDataRequest, data), 0, NULL},
    {4, PB_BYTES, 0, offsetof(shadnet_TusSetDataRequest, info), 0, NULL},
    {5, PB_INT64, 0, offsetof(shadnet_TusSetDataRequest, ownerAccountId), 0, NULL},
    {6, PB_STRING, 0, offsetof(shadnet_TusSetDataRequest, virtualUser), 0, NULL},
    {7, PB_BOOL, 0, offsetof(shadnet_TusSetDataRequest, hasAuthorCheck), 0, NULL},
    {8, PB_INT64, 0, offsetof(shadnet_TusSetDataRequest, isLastChangedAuthor), 0, NULL},
    {9, PB_BOOL, 0, offsetof(shadnet_TusSetDataRequest, hasDateCheck), 0, NULL},
    {10, PB_UINT64, 0, offsetof(shadnet_TusSetDataRequest, isLastChangedDate), 0, NULL},
    {11, PB_STRING, 0, offsetof(shadnet_TusSetDataRequest, isLastChangedAuthorNpId), 0, NULL},
};
const pb_msg_desc_t shadnet_TusSetDataRequest_desc = {sizeof(shadnet_TusSetDataRequest), shadnet_TusSetDataRequest_fields,
    sizeof(shadnet_TusSetDataRequest_fields) / sizeof(shadnet_TusSetDataRequest_fields[0])};

static const pb_field_t shadnet_TusGetDataRequest_fields[] = {
    {1, PB_STRING, 0, offsetof(shadnet_TusGetDataRequest, ownerNpId), 0, NULL},
    {2, PB_INT32, 0, offsetof(shadnet_TusGetDataRequest, slotId), 0, NULL},
    {3, PB_INT64, 0, offsetof(shadnet_TusGetDataRequest, ownerAccountId), 0, NULL},
    {4, PB_STRING, 0, offsetof(shadnet_TusGetDataRequest, virtualUser), 0, NULL},
};
const pb_msg_desc_t shadnet_TusGetDataRequest_desc = {sizeof(shadnet_TusGetDataRequest), shadnet_TusGetDataRequest_fields,
    sizeof(shadnet_TusGetDataRequest_fields) / sizeof(shadnet_TusGetDataRequest_fields[0])};

static const pb_field_t shadnet_TusGetDataResponse_fields[] = {
    {1, PB_MESSAGE, 0, offsetof(shadnet_TusGetDataResponse, status), offsetof(shadnet_TusGetDataResponse, has_status), &shadnet_TusDataStatus_desc},
    {2, PB_BYTES, 0, offsetof(shadnet_TusGetDataResponse, data), 0, NULL},
};
const pb_msg_desc_t shadnet_TusGetDataResponse_desc = {sizeof(shadnet_TusGetDataResponse), shadnet_TusGetDataResponse_fields,
    sizeof(shadnet_TusGetDataResponse_fields) / sizeof(shadnet_TusGetDataResponse_fields[0])};

static const pb_field_t shadnet_TusGetMultiSlotDataStatusRequest_fields[] = {
    {1, PB_STRING, 0, offsetof(shadnet_TusGetMultiSlotDataStatusRequest, ownerNpId), 0, NULL},
    {2, PB_INT32, 1, offsetof(shadnet_TusGetMultiSlotDataStatusRequest, slotIds), 0, NULL},
    {3, PB_INT64, 0, offsetof(shadnet_TusGetMultiSlotDataStatusRequest, ownerAccountId), 0, NULL},
    {4, PB_STRING, 0, offsetof(shadnet_TusGetMultiSlotDataStatusRequest, virtualUser), 0, NULL},
};
const pb_msg_desc_t shadnet_TusGetMultiSlotDataStatusRequest_desc = {sizeof(shadnet_TusGetMultiSlotDataStatusRequest), shadnet_TusGetMultiSlotDataStatusRequest_fields,
    sizeof(shadnet_TusGetMultiSlotDataStatusRequest_fields) / sizeof(shadnet_TusGetMultiSlotDataStatusRequest_fields[0])};

static const pb_field_t shadnet_TusGetMultiUserDataStatusRequest_fields[] = {
    {1, PB_INT32, 0, offsetof(shadnet_TusGetMultiUserDataStatusRequest, slotId), 0, NULL},
    {2, PB_STRING, 1, offsetof(shadnet_TusGetMultiUserDataStatusRequest, ownerNpIds), 0, NULL},
    {3, PB_STRING, 1, offsetof(shadnet_TusGetMultiUserDataStatusRequest, virtualUsers), 0, NULL},
    {4, PB_INT64, 1, offsetof(shadnet_TusGetMultiUserDataStatusRequest, ownerAccountIds), 0, NULL},
};
const pb_msg_desc_t shadnet_TusGetMultiUserDataStatusRequest_desc = {sizeof(shadnet_TusGetMultiUserDataStatusRequest), shadnet_TusGetMultiUserDataStatusRequest_fields,
    sizeof(shadnet_TusGetMultiUserDataStatusRequest_fields) / sizeof(shadnet_TusGetMultiUserDataStatusRequest_fields[0])};

static const pb_field_t shadnet_TusGetFriendsDataStatusRequest_fields[] = {
    {1, PB_INT32, 0, offsetof(shadnet_TusGetFriendsDataStatusRequest, slotId), 0, NULL},
    {2, PB_BOOL, 0, offsetof(shadnet_TusGetFriendsDataStatusRequest, includeSelf), 0, NULL},
    {3, PB_UINT32, 0, offsetof(shadnet_TusGetFriendsDataStatusRequest, max), 0, NULL},
    {4, PB_INT32, 0, offsetof(shadnet_TusGetFriendsDataStatusRequest, sortType), 0, NULL},
    {5, PB_UINT32, 0, offsetof(shadnet_TusGetFriendsDataStatusRequest, startOffset), 0, NULL},
};
const pb_msg_desc_t shadnet_TusGetFriendsDataStatusRequest_desc = {sizeof(shadnet_TusGetFriendsDataStatusRequest), shadnet_TusGetFriendsDataStatusRequest_fields,
    sizeof(shadnet_TusGetFriendsDataStatusRequest_fields) / sizeof(shadnet_TusGetFriendsDataStatusRequest_fields[0])};

static const pb_field_t shadnet_TusGetFriendsVariableRequest_fields[] = {
    {1, PB_INT32, 0, offsetof(shadnet_TusGetFriendsVariableRequest, slotId), 0, NULL},
    {2, PB_BOOL, 0, offsetof(shadnet_TusGetFriendsVariableRequest, includeSelf), 0, NULL},
    {3, PB_UINT32, 0, offsetof(shadnet_TusGetFriendsVariableRequest, max), 0, NULL},
    {4, PB_INT32, 0, offsetof(shadnet_TusGetFriendsVariableRequest, sortType), 0, NULL},
    {5, PB_UINT32, 0, offsetof(shadnet_TusGetFriendsVariableRequest, startOffset), 0, NULL},
};
const pb_msg_desc_t shadnet_TusGetFriendsVariableRequest_desc = {sizeof(shadnet_TusGetFriendsVariableRequest), shadnet_TusGetFriendsVariableRequest_fields,
    sizeof(shadnet_TusGetFriendsVariableRequest_fields) / sizeof(shadnet_TusGetFriendsVariableRequest_fields[0])};

static const pb_field_t shadnet_TusDataStatusResponse_fields[] = {
    {1, PB_MESSAGE, 1, offsetof(shadnet_TusDataStatusResponse, statuses), 0, &shadnet_TusDataStatus_desc},
    {2, PB_UINT32, 0, offsetof(shadnet_TusDataStatusResponse, total), 0, NULL},
};
const pb_msg_desc_t shadnet_TusDataStatusResponse_desc = {sizeof(shadnet_TusDataStatusResponse), shadnet_TusDataStatusResponse_fields,
    sizeof(shadnet_TusDataStatusResponse_fields) / sizeof(shadnet_TusDataStatusResponse_fields[0])};

static const pb_field_t shadnet_TusTryAndSetVariableRequest_fields[] = {
    {1, PB_STRING, 0, offsetof(shadnet_TusTryAndSetVariableRequest, ownerNpId), 0, NULL},
    {2, PB_INT64, 0, offsetof(shadnet_TusTryAndSetVariableRequest, ownerAccountId), 0, NULL},
    {3, PB_STRING, 0, offsetof(shadnet_TusTryAndSetVariableRequest, virtualUser), 0, NULL},
    {4, PB_INT32, 0, offsetof(shadnet_TusTryAndSetVariableRequest, slotId), 0, NULL},
    {5, PB_INT32, 0, offsetof(shadnet_TusTryAndSetVariableRequest, opeType), 0, NULL},
    {6, PB_INT64, 0, offsetof(shadnet_TusTryAndSetVariableRequest, value), 0, NULL},
    {7, PB_BOOL, 0, offsetof(shadnet_TusTryAndSetVariableRequest, hasCompare), 0, NULL},
    {8, PB_INT64, 0, offsetof(shadnet_TusTryAndSetVariableRequest, compareValue), 0, NULL},
    {9, PB_BOOL, 0, offsetof(shadnet_TusTryAndSetVariableRequest, hasAuthorCheck), 0, NULL},
    {10, PB_INT64, 0, offsetof(shadnet_TusTryAndSetVariableRequest, isLastChangedAuthor), 0, NULL},
    {11, PB_BOOL, 0, offsetof(shadnet_TusTryAndSetVariableRequest, hasDateCheck), 0, NULL},
    {12, PB_UINT64, 0, offsetof(shadnet_TusTryAndSetVariableRequest, isLastChangedDate), 0, NULL},
    {13, PB_STRING, 0, offsetof(shadnet_TusTryAndSetVariableRequest, isLastChangedAuthorNpId), 0, NULL},
};
const pb_msg_desc_t shadnet_TusTryAndSetVariableRequest_desc = {sizeof(shadnet_TusTryAndSetVariableRequest), shadnet_TusTryAndSetVariableRequest_fields,
    sizeof(shadnet_TusTryAndSetVariableRequest_fields) / sizeof(shadnet_TusTryAndSetVariableRequest_fields[0])};

static const pb_field_t shadnet_TusDeleteMultiSlotDataRequest_fields[] = {
    {1, PB_STRING, 0, offsetof(shadnet_TusDeleteMultiSlotDataRequest, ownerNpId), 0, NULL},
    {2, PB_INT32, 1, offsetof(shadnet_TusDeleteMultiSlotDataRequest, slotIds), 0, NULL},
    {3, PB_INT64, 0, offsetof(shadnet_TusDeleteMultiSlotDataRequest, ownerAccountId), 0, NULL},
    {4, PB_STRING, 0, offsetof(shadnet_TusDeleteMultiSlotDataRequest, virtualUser), 0, NULL},
};
const pb_msg_desc_t shadnet_TusDeleteMultiSlotDataRequest_desc = {sizeof(shadnet_TusDeleteMultiSlotDataRequest), shadnet_TusDeleteMultiSlotDataRequest_fields,
    sizeof(shadnet_TusDeleteMultiSlotDataRequest_fields) / sizeof(shadnet_TusDeleteMultiSlotDataRequest_fields[0])};

static const pb_field_t shadnet_TusDeleteMultiSlotVariableRequest_fields[] = {
    {1, PB_STRING, 0, offsetof(shadnet_TusDeleteMultiSlotVariableRequest, ownerNpId), 0, NULL},
    {2, PB_STRING, 0, offsetof(shadnet_TusDeleteMultiSlotVariableRequest, virtualUser), 0, NULL},
    {3, PB_INT32, 1, offsetof(shadnet_TusDeleteMultiSlotVariableRequest, slotIds), 0, NULL},
    {4, PB_INT64, 0, offsetof(shadnet_TusDeleteMultiSlotVariableRequest, ownerAccountId), 0, NULL},
};
const pb_msg_desc_t shadnet_TusDeleteMultiSlotVariableRequest_desc = {sizeof(shadnet_TusDeleteMultiSlotVariableRequest), shadnet_TusDeleteMultiSlotVariableRequest_fields,
    sizeof(shadnet_TusDeleteMultiSlotVariableRequest_fields) / sizeof(shadnet_TusDeleteMultiSlotVariableRequest_fields[0])};

static const pb_field_t shadnet_TusGetMultiUserVariableRequest_fields[] = {
    {1, PB_INT32, 0, offsetof(shadnet_TusGetMultiUserVariableRequest, slotId), 0, NULL},
    {2, PB_INT64, 1, offsetof(shadnet_TusGetMultiUserVariableRequest, ownerAccountIds), 0, NULL},
    {3, PB_STRING, 1, offsetof(shadnet_TusGetMultiUserVariableRequest, virtualUsers), 0, NULL},
    {4, PB_STRING, 1, offsetof(shadnet_TusGetMultiUserVariableRequest, ownerNpIds), 0, NULL},
};
const pb_msg_desc_t shadnet_TusGetMultiUserVariableRequest_desc = {sizeof(shadnet_TusGetMultiUserVariableRequest), shadnet_TusGetMultiUserVariableRequest_fields,
    sizeof(shadnet_TusGetMultiUserVariableRequest_fields) / sizeof(shadnet_TusGetMultiUserVariableRequest_fields[0])};

static const pb_field_t shadnet_TssGetDataRequest_fields[] = {
    {1, PB_INT32, 0, offsetof(shadnet_TssGetDataRequest, slotId), 0, NULL},
    {2, PB_BOOL, 0, offsetof(shadnet_TssGetDataRequest, hasOffset), 0, NULL},
    {3, PB_UINT64, 0, offsetof(shadnet_TssGetDataRequest, offset), 0, NULL},
    {4, PB_BOOL, 0, offsetof(shadnet_TssGetDataRequest, hasLastByte), 0, NULL},
    {5, PB_UINT64, 0, offsetof(shadnet_TssGetDataRequest, lastByte), 0, NULL},
    {6, PB_BOOL, 0, offsetof(shadnet_TssGetDataRequest, hasIfParam), 0, NULL},
    {7, PB_INT32, 0, offsetof(shadnet_TssGetDataRequest, ifType), 0, NULL},
    {8, PB_UINT64, 0, offsetof(shadnet_TssGetDataRequest, ifLastModified), 0, NULL},
};
const pb_msg_desc_t shadnet_TssGetDataRequest_desc = {sizeof(shadnet_TssGetDataRequest), shadnet_TssGetDataRequest_fields,
    sizeof(shadnet_TssGetDataRequest_fields) / sizeof(shadnet_TssGetDataRequest_fields[0])};

static const pb_field_t shadnet_TssGetDataResponse_fields[] = {
    {1, PB_INT32, 0, offsetof(shadnet_TssGetDataResponse, statusCodeType), 0, NULL},
    {2, PB_UINT64, 0, offsetof(shadnet_TssGetDataResponse, lastModified), 0, NULL},
    {3, PB_UINT64, 0, offsetof(shadnet_TssGetDataResponse, contentLength), 0, NULL},
    {4, PB_BYTES, 0, offsetof(shadnet_TssGetDataResponse, data), 0, NULL},
};
const pb_msg_desc_t shadnet_TssGetDataResponse_desc = {sizeof(shadnet_TssGetDataResponse), shadnet_TssGetDataResponse_fields,
    sizeof(shadnet_TssGetDataResponse_fields) / sizeof(shadnet_TssGetDataResponse_fields[0])};

static const pb_field_t shadnet_TrophyEntry_fields[] = {
    {1, PB_INT32, 0, offsetof(shadnet_TrophyEntry, trophyId), 0, NULL},
    {2, PB_UINT64, 0, offsetof(shadnet_TrophyEntry, timestamp), 0, NULL},
};
const pb_msg_desc_t shadnet_TrophyEntry_desc = {sizeof(shadnet_TrophyEntry), shadnet_TrophyEntry_fields,
    sizeof(shadnet_TrophyEntry_fields) / sizeof(shadnet_TrophyEntry_fields[0])};

static const pb_field_t shadnet_UnlockTrophyRequest_fields[] = {
    {1, PB_INT32, 0, offsetof(shadnet_UnlockTrophyRequest, trophyId), 0, NULL},
    {2, PB_UINT64, 0, offsetof(shadnet_UnlockTrophyRequest, timestamp), 0, NULL},
};
const pb_msg_desc_t shadnet_UnlockTrophyRequest_desc = {sizeof(shadnet_UnlockTrophyRequest), shadnet_UnlockTrophyRequest_fields,
    sizeof(shadnet_UnlockTrophyRequest_fields) / sizeof(shadnet_UnlockTrophyRequest_fields[0])};

static const pb_field_t shadnet_SyncTrophiesRequest_fields[] = {
    {1, PB_MESSAGE, 1, offsetof(shadnet_SyncTrophiesRequest, trophies), 0, &shadnet_TrophyEntry_desc},
};
const pb_msg_desc_t shadnet_SyncTrophiesRequest_desc = {sizeof(shadnet_SyncTrophiesRequest), shadnet_SyncTrophiesRequest_fields,
    sizeof(shadnet_SyncTrophiesRequest_fields) / sizeof(shadnet_SyncTrophiesRequest_fields[0])};

static const pb_field_t shadnet_SyncTrophiesReply_fields[] = {
    {1, PB_MESSAGE, 1, offsetof(shadnet_SyncTrophiesReply, trophies), 0, &shadnet_TrophyEntry_desc},
};
const pb_msg_desc_t shadnet_SyncTrophiesReply_desc = {sizeof(shadnet_SyncTrophiesReply), shadnet_SyncTrophiesReply_fields,
    sizeof(shadnet_SyncTrophiesReply_fields) / sizeof(shadnet_SyncTrophiesReply_fields[0])};

static const pb_field_t shadnet_SetClientVersionRequest_fields[] = {
    {1, PB_STRING, 0, offsetof(shadnet_SetClientVersionRequest, version), 0, NULL},
};
const pb_msg_desc_t shadnet_SetClientVersionRequest_desc = {sizeof(shadnet_SetClientVersionRequest), shadnet_SetClientVersionRequest_fields,
    sizeof(shadnet_SetClientVersionRequest_fields) / sizeof(shadnet_SetClientVersionRequest_fields[0])};
