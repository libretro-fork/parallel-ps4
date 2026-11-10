/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

/* Written by gen_messages.py from shadnet.proto; do not edit. */
/* clang-format off */

#ifndef SHADNET_MSG_H
#define SHADNET_MSG_H

#include "shadnet/pb_wire.h"

RETRO_BEGIN_DECLS

#define SHADNET_MATCHING_USER_ID_ONLINE_ID 0
#define SHADNET_MATCHING_USER_ID_ACCOUNT_ID 1

struct shadnet_LoginRequest;
struct shadnet_SetAppearOfflineRequest;
struct shadnet_ServerFeaturesReply;
struct shadnet_FriendEntry;
struct shadnet_LoginReply;
struct shadnet_RegistrationRequest;
struct shadnet_FriendCommandRequest;
struct shadnet_NotifyFriendQuery;
struct shadnet_NotifyFriendNew;
struct shadnet_NotifyFriendLost;
struct shadnet_NotifyFriendStatus;
struct shadnet_NotifyServiceUnavailable;
struct shadnet_NotifyClientRejected;
struct shadnet_BoardInfo;
struct shadnet_RecordScoreRequest;
struct shadnet_RecordScoreGameDataRequest;
struct shadnet_GetScoreGameDataRequest;
struct shadnet_GetScoreRangeRequest;
struct shadnet_ScoreNpIdPcId;
struct shadnet_GetScoreNpIdRequest;
struct shadnet_GetScoreFriendsRequest;
struct shadnet_ScoreAccountIdPcId;
struct shadnet_GetScoreAccountIdRequest;
struct shadnet_GetScoreGameDataByAccountIdRequest;
struct shadnet_ScoreRankData;
struct shadnet_ScoreInfo;
struct shadnet_GetScoreResponse;
struct shadnet_MatchingBinAttr;
struct shadnet_MatchingIntAttr;
struct shadnet_MatchingRoomGroup;
struct shadnet_MatchingRoomGroupConfig;
struct shadnet_MatchingRoomDataInternal;
struct shadnet_MatchingRoomMemberData;
struct shadnet_MatchingRoomMemberDataExternal;
struct shadnet_MatchingRoomDataExternal;
struct shadnet_CreateJoinRoomResponse;
struct shadnet_ContextStartRequest;
struct shadnet_ContextStopRequest;
struct shadnet_CreateRoomRequest;
struct shadnet_JoinRoomRequest;
struct shadnet_LeaveRoomRequest;
struct shadnet_RequestSignalingInfosRequest;
struct shadnet_SetRoomDataInternalRequest;
struct shadnet_SetRoomMemberDataInternalRequest;
struct shadnet_SetRoomDataExternalRequest;
struct shadnet_KickoutRoomMemberRequest;
struct shadnet_CreateRoomReply;
struct shadnet_JoinRoomReply;
struct shadnet_MatchingIntSearchFilter;
struct shadnet_MatchingBinSearchFilter;
struct shadnet_SearchRoomRequest;
struct shadnet_SearchRoomReply;
struct shadnet_GetRoomDataExternalListRequest;
struct shadnet_GetRoomDataExternalListReply;
struct shadnet_GetRoomMemberDataExternalListRequest;
struct shadnet_GetRoomMemberDataExternalListReply;
struct shadnet_MatchingUserInfo;
struct shadnet_SetUserInfoRequest;
struct shadnet_SetUserInfoReply;
struct shadnet_GetUserInfoListRequest;
struct shadnet_GetUserInfoListReply;
struct shadnet_MatchingWorld;
struct shadnet_GetWorldInfoListRequest;
struct shadnet_GetWorldInfoListReply;
struct shadnet_RequestSignalingInfosReply;
struct shadnet_LeaveRoomReply;
struct shadnet_SetRoomDataInternalReply;
struct shadnet_SetRoomMemberDataInternalReply;
struct shadnet_SetRoomDataExternalReply;
struct shadnet_KickoutRoomMemberReply;
struct shadnet_SendRoomMessageRequest;
struct shadnet_SendRoomMessageReply;
struct shadnet_GetTokenReply;
struct shadnet_LookupOnlineIdRequest;
struct shadnet_LookupOnlineIdReply;
struct shadnet_GetAuthorizationCodeRequest;
struct shadnet_GetAuthorizationCodeReply;
struct shadnet_NotifyRoomEvent;
struct shadnet_NotifyRoomMessage;
struct shadnet_TusVariable;
struct shadnet_TusDataStatus;
struct shadnet_TusSetMultiSlotVariableRequest;
struct shadnet_TusGetMultiSlotVariableRequest;
struct shadnet_TusAddAndGetVariableRequest;
struct shadnet_TusVariableResponse;
struct shadnet_TusSetDataRequest;
struct shadnet_TusGetDataRequest;
struct shadnet_TusGetDataResponse;
struct shadnet_TusGetMultiSlotDataStatusRequest;
struct shadnet_TusGetMultiUserDataStatusRequest;
struct shadnet_TusGetFriendsDataStatusRequest;
struct shadnet_TusGetFriendsVariableRequest;
struct shadnet_TusDataStatusResponse;
struct shadnet_TusTryAndSetVariableRequest;
struct shadnet_TusDeleteMultiSlotDataRequest;
struct shadnet_TusDeleteMultiSlotVariableRequest;
struct shadnet_TusGetMultiUserVariableRequest;
struct shadnet_TssGetDataRequest;
struct shadnet_TssGetDataResponse;
struct shadnet_TrophyEntry;
struct shadnet_UnlockTrophyRequest;
struct shadnet_SyncTrophiesRequest;
struct shadnet_SyncTrophiesReply;
struct shadnet_SetClientVersionRequest;

typedef struct shadnet_LoginRequest {
    pb_bytes_t npid;
    pb_bytes_t password;
    pb_bytes_t token;
    pb_bytes_t title_id;
    pb_bytes_t title_name;
    uint8_t appear_offline;
} shadnet_LoginRequest;
extern const pb_msg_desc_t shadnet_LoginRequest_desc;

typedef struct shadnet_SetAppearOfflineRequest {
    uint8_t appear_offline;
} shadnet_SetAppearOfflineRequest;
extern const pb_msg_desc_t shadnet_SetAppearOfflineRequest_desc;

typedef struct shadnet_ServerFeaturesReply {
    uint8_t matching2_enabled;
    uint8_t trophies_enabled;
} shadnet_ServerFeaturesReply;
extern const pb_msg_desc_t shadnet_ServerFeaturesReply_desc;

typedef struct shadnet_FriendEntry {
    pb_bytes_t npid;
    uint8_t online;
    pb_bytes_t presence;
} shadnet_FriendEntry;
extern const pb_msg_desc_t shadnet_FriendEntry_desc;

typedef struct shadnet_LoginReply {
    pb_bytes_t avatar_url;
    uint64_t user_id;
    struct {
        struct shadnet_FriendEntry* items;
        size_t count;
    } friends;
    struct {
        pb_bytes_t* items;
        size_t count;
    } friend_requests_sent;
    struct {
        pb_bytes_t* items;
        size_t count;
    } friend_requests_received;
    struct {
        pb_bytes_t* items;
        size_t count;
    } blocked;
} shadnet_LoginReply;
extern const pb_msg_desc_t shadnet_LoginReply_desc;

typedef struct shadnet_RegistrationRequest {
    pb_bytes_t npid;
    pb_bytes_t password;
    pb_bytes_t avatar_url;
    pb_bytes_t email;
    pb_bytes_t secret_key;
} shadnet_RegistrationRequest;
extern const pb_msg_desc_t shadnet_RegistrationRequest_desc;

typedef struct shadnet_FriendCommandRequest {
    pb_bytes_t npid;
} shadnet_FriendCommandRequest;
extern const pb_msg_desc_t shadnet_FriendCommandRequest_desc;

typedef struct shadnet_NotifyFriendQuery {
    pb_bytes_t from_npid;
} shadnet_NotifyFriendQuery;
extern const pb_msg_desc_t shadnet_NotifyFriendQuery_desc;

typedef struct shadnet_NotifyFriendNew {
    pb_bytes_t npid;
    uint8_t online;
} shadnet_NotifyFriendNew;
extern const pb_msg_desc_t shadnet_NotifyFriendNew_desc;

typedef struct shadnet_NotifyFriendLost {
    pb_bytes_t npid;
} shadnet_NotifyFriendLost;
extern const pb_msg_desc_t shadnet_NotifyFriendLost_desc;

typedef struct shadnet_NotifyFriendStatus {
    pb_bytes_t npid;
    uint8_t online;
    uint64_t timestamp;
} shadnet_NotifyFriendStatus;
extern const pb_msg_desc_t shadnet_NotifyFriendStatus_desc;

typedef struct shadnet_NotifyServiceUnavailable {
    uint32_t service;
    uint32_t reason;
    uint32_t in_use;
    uint32_t limit;
} shadnet_NotifyServiceUnavailable;
extern const pb_msg_desc_t shadnet_NotifyServiceUnavailable_desc;

typedef struct shadnet_NotifyClientRejected {
    uint32_t reason;
    pb_bytes_t message;
    pb_bytes_t version;
} shadnet_NotifyClientRejected;
extern const pb_msg_desc_t shadnet_NotifyClientRejected_desc;

typedef struct shadnet_BoardInfo {
    uint32_t rankLimit;
    uint32_t updateMode;
    uint32_t sortMode;
    uint32_t uploadNumLimit;
    uint64_t uploadSizeLimit;
} shadnet_BoardInfo;
extern const pb_msg_desc_t shadnet_BoardInfo_desc;

typedef struct shadnet_RecordScoreRequest {
    uint32_t boardId;
    int32_t pcId;
    int64_t score;
    pb_bytes_t comment;
    pb_bytes_t data;
} shadnet_RecordScoreRequest;
extern const pb_msg_desc_t shadnet_RecordScoreRequest_desc;

typedef struct shadnet_RecordScoreGameDataRequest {
    uint32_t boardId;
    int32_t pcId;
    int64_t score;
} shadnet_RecordScoreGameDataRequest;
extern const pb_msg_desc_t shadnet_RecordScoreGameDataRequest_desc;

typedef struct shadnet_GetScoreGameDataRequest {
    uint32_t boardId;
    pb_bytes_t npId;
    int32_t pcId;
} shadnet_GetScoreGameDataRequest;
extern const pb_msg_desc_t shadnet_GetScoreGameDataRequest_desc;

typedef struct shadnet_GetScoreRangeRequest {
    uint32_t boardId;
    uint32_t startRank;
    uint32_t numRanks;
    uint8_t withComment;
    uint8_t withGameInfo;
} shadnet_GetScoreRangeRequest;
extern const pb_msg_desc_t shadnet_GetScoreRangeRequest_desc;

typedef struct shadnet_ScoreNpIdPcId {
    pb_bytes_t npid;
    int32_t pcId;
} shadnet_ScoreNpIdPcId;
extern const pb_msg_desc_t shadnet_ScoreNpIdPcId_desc;

typedef struct shadnet_GetScoreNpIdRequest {
    uint32_t boardId;
    struct {
        struct shadnet_ScoreNpIdPcId* items;
        size_t count;
    } npids;
    uint8_t withComment;
    uint8_t withGameInfo;
} shadnet_GetScoreNpIdRequest;
extern const pb_msg_desc_t shadnet_GetScoreNpIdRequest_desc;

typedef struct shadnet_GetScoreFriendsRequest {
    uint32_t boardId;
    uint8_t includeSelf;
    uint32_t max;
    uint8_t withComment;
    uint8_t withGameInfo;
} shadnet_GetScoreFriendsRequest;
extern const pb_msg_desc_t shadnet_GetScoreFriendsRequest_desc;

typedef struct shadnet_ScoreAccountIdPcId {
    int64_t accountId;
    int32_t pcId;
} shadnet_ScoreAccountIdPcId;
extern const pb_msg_desc_t shadnet_ScoreAccountIdPcId_desc;

typedef struct shadnet_GetScoreAccountIdRequest {
    uint32_t boardId;
    struct {
        struct shadnet_ScoreAccountIdPcId* items;
        size_t count;
    } ids;
    uint8_t withComment;
    uint8_t withGameInfo;
} shadnet_GetScoreAccountIdRequest;
extern const pb_msg_desc_t shadnet_GetScoreAccountIdRequest_desc;

typedef struct shadnet_GetScoreGameDataByAccountIdRequest {
    uint32_t boardId;
    int64_t accountId;
    int32_t pcId;
} shadnet_GetScoreGameDataByAccountIdRequest;
extern const pb_msg_desc_t shadnet_GetScoreGameDataByAccountIdRequest_desc;

typedef struct shadnet_ScoreRankData {
    pb_bytes_t npId;
    int32_t pcId;
    uint32_t rank;
    int64_t score;
    uint8_t hasGameData;
    uint64_t recordDate;
    int64_t accountId;
} shadnet_ScoreRankData;
extern const pb_msg_desc_t shadnet_ScoreRankData_desc;

typedef struct shadnet_ScoreInfo {
    pb_bytes_t data;
} shadnet_ScoreInfo;
extern const pb_msg_desc_t shadnet_ScoreInfo_desc;

typedef struct shadnet_GetScoreResponse {
    struct {
        struct shadnet_ScoreRankData* items;
        size_t count;
    } rankArray;
    struct {
        pb_bytes_t* items;
        size_t count;
    } commentArray;
    struct {
        struct shadnet_ScoreInfo* items;
        size_t count;
    } infoArray;
    uint64_t lastSortDate;
    uint32_t totalRecord;
} shadnet_GetScoreResponse;
extern const pb_msg_desc_t shadnet_GetScoreResponse_desc;

typedef struct shadnet_MatchingBinAttr {
    uint32_t attr_id;
    pb_bytes_t data;
    uint64_t update_date;
    uint32_t update_member_id;
} shadnet_MatchingBinAttr;
extern const pb_msg_desc_t shadnet_MatchingBinAttr_desc;

typedef struct shadnet_MatchingIntAttr {
    uint32_t attr_id;
    uint32_t attr_value;
} shadnet_MatchingIntAttr;
extern const pb_msg_desc_t shadnet_MatchingIntAttr_desc;

typedef struct shadnet_MatchingRoomGroup {
    uint32_t group_id;
    uint8_t has_passwd;
    uint8_t has_label;
    pb_bytes_t label;
    uint32_t slot_count;
    uint32_t num_members;
} shadnet_MatchingRoomGroup;
extern const pb_msg_desc_t shadnet_MatchingRoomGroup_desc;

typedef struct shadnet_MatchingRoomGroupConfig {
    uint32_t slot_count;
    uint8_t has_label;
    pb_bytes_t label;
    uint8_t has_passwd;
} shadnet_MatchingRoomGroupConfig;
extern const pb_msg_desc_t shadnet_MatchingRoomGroupConfig_desc;

typedef struct shadnet_MatchingRoomDataInternal {
    uint32_t public_slots;
    uint32_t private_slots;
    uint32_t open_public_slots;
    uint32_t open_private_slots;
    uint32_t max_slot;
    uint32_t server_id;
    uint32_t world_id;
    uint32_t lobby_id;
    uint64_t room_id;
    uint64_t passwd_slot_mask;
    uint32_t joined_slot_mask;
    struct {
        struct shadnet_MatchingRoomGroup* items;
        size_t count;
    } groups;
    uint32_t flags;
    struct {
        struct shadnet_MatchingBinAttr* items;
        size_t count;
    } bin_attrs_internal;
} shadnet_MatchingRoomDataInternal;
extern const pb_msg_desc_t shadnet_MatchingRoomDataInternal_desc;

typedef struct shadnet_MatchingRoomMemberData {
    pb_bytes_t npid;
    uint32_t member_id;
    uint32_t team_id;
    uint8_t is_owner;
    struct shadnet_MatchingRoomGroup group;
    uint8_t has_group;
    struct {
        struct shadnet_MatchingBinAttr* items;
        size_t count;
    } bin_attrs_internal;
    uint64_t join_date;
    uint32_t nat_type;
    uint32_t flag_attr;
    pb_bytes_t addr;
    uint32_t port;
    uint32_t group_id;
    uint64_t account_id;
    uint32_t platform;
} shadnet_MatchingRoomMemberData;
extern const pb_msg_desc_t shadnet_MatchingRoomMemberData_desc;

typedef struct shadnet_MatchingRoomMemberDataExternal {
    pb_bytes_t npid;
    uint32_t member_id;
    uint64_t account_id;
    uint32_t platform;
    uint64_t join_date;
    uint32_t role;
} shadnet_MatchingRoomMemberDataExternal;
extern const pb_msg_desc_t shadnet_MatchingRoomMemberDataExternal_desc;

typedef struct shadnet_MatchingRoomDataExternal {
    uint32_t max_slot;
    uint32_t cur_members;
    uint32_t flags;
    uint32_t server_id;
    uint32_t world_id;
    uint64_t lobby_id;
    uint64_t room_id;
    uint64_t passwd_slot_mask;
    uint64_t joined_slot_mask;
    uint32_t public_slots;
    uint32_t private_slots;
    uint32_t open_public_slots;
    uint32_t open_private_slots;
    struct {
        struct shadnet_MatchingRoomGroup* items;
        size_t count;
    } groups;
    struct {
        struct shadnet_MatchingIntAttr* items;
        size_t count;
    } external_search_int_attrs;
    struct {
        struct shadnet_MatchingBinAttr* items;
        size_t count;
    } external_search_bin_attrs;
    struct {
        struct shadnet_MatchingBinAttr* items;
        size_t count;
    } external_bin_attrs;
    pb_bytes_t owner_npid;
    uint64_t owner_account_id;
    uint32_t owner_platform;
} shadnet_MatchingRoomDataExternal;
extern const pb_msg_desc_t shadnet_MatchingRoomDataExternal_desc;

typedef struct shadnet_CreateJoinRoomResponse {
    struct shadnet_MatchingRoomDataInternal room_data;
    uint8_t has_room_data;
    struct {
        struct shadnet_MatchingRoomMemberData* items;
        size_t count;
    } members;
    uint32_t me_member_id;
    uint32_t owner_member_id;
} shadnet_CreateJoinRoomResponse;
extern const pb_msg_desc_t shadnet_CreateJoinRoomResponse_desc;

typedef struct shadnet_ContextStartRequest {
    uint32_t ctx_id;
} shadnet_ContextStartRequest;
extern const pb_msg_desc_t shadnet_ContextStartRequest_desc;

typedef struct shadnet_ContextStopRequest {
    uint32_t ctx_id;
} shadnet_ContextStopRequest;
extern const pb_msg_desc_t shadnet_ContextStopRequest_desc;

typedef struct shadnet_CreateRoomRequest {
    uint32_t req_id;
    uint32_t max_slots;
    uint32_t team_id;
    uint32_t world_id;
    uint32_t lobby_id;
    uint32_t flags;
    uint32_t group_config_count;
    uint32_t allowed_user_count;
    uint32_t blocked_user_count;
    uint32_t internal_bin_attr_count;
    struct {
        struct shadnet_MatchingIntAttr* items;
        size_t count;
    } external_search_int_attrs;
    struct {
        struct shadnet_MatchingBinAttr* items;
        size_t count;
    } external_search_bin_attrs;
    struct {
        struct shadnet_MatchingBinAttr* items;
        size_t count;
    } external_bin_attrs;
    struct {
        struct shadnet_MatchingBinAttr* items;
        size_t count;
    } member_bin_attrs;
    uint8_t join_group_label_present;
    uint8_t room_password_present;
    uint32_t sig_type;
    uint32_t sig_flag;
    uint32_t sig_main_member;
    struct {
        struct shadnet_MatchingBinAttr* items;
        size_t count;
    } internal_bin_attrs;
    int32_t user_id_kind;
    struct {
        pb_bytes_t* items;
        size_t count;
    } allowed_online_ids;
    struct {
        pb_bytes_t* items;
        size_t count;
    } blocked_online_ids;
    struct {
        uint64_t* items;
        size_t count;
    } allowed_account_ids;
    struct {
        uint64_t* items;
        size_t count;
    } blocked_account_ids;
    struct {
        struct shadnet_MatchingRoomGroupConfig* items;
        size_t count;
    } group_configs;
    pb_bytes_t room_password;
    uint8_t has_passwd_slot_mask;
    uint64_t passwd_slot_mask;
    pb_bytes_t join_group_label;
} shadnet_CreateRoomRequest;
extern const pb_msg_desc_t shadnet_CreateRoomRequest_desc;

typedef struct shadnet_JoinRoomRequest {
    uint64_t room_id;
    uint32_t req_id;
    uint32_t team_id;
    uint32_t join_flags;
    uint32_t blocked_user_count;
    struct {
        struct shadnet_MatchingBinAttr* items;
        size_t count;
    } member_bin_attrs;
    uint8_t room_password_present;
    uint8_t join_group_label_present;
    int32_t user_id_kind;
    struct {
        pb_bytes_t* items;
        size_t count;
    } blocked_online_ids;
    struct {
        uint64_t* items;
        size_t count;
    } blocked_account_ids;
} shadnet_JoinRoomRequest;
extern const pb_msg_desc_t shadnet_JoinRoomRequest_desc;

typedef struct shadnet_LeaveRoomRequest {
    uint64_t room_id;
    uint32_t req_id;
} shadnet_LeaveRoomRequest;
extern const pb_msg_desc_t shadnet_LeaveRoomRequest_desc;

typedef struct shadnet_RequestSignalingInfosRequest {
    pb_bytes_t target_npid;
} shadnet_RequestSignalingInfosRequest;
extern const pb_msg_desc_t shadnet_RequestSignalingInfosRequest_desc;

typedef struct shadnet_SetRoomDataInternalRequest {
    uint32_t req_id;
    uint64_t room_id;
    uint32_t flag_filter;
    uint32_t flag_attr;
    struct {
        struct shadnet_MatchingBinAttr* items;
        size_t count;
    } bin_attrs;
    uint8_t has_passwd_mask;
    uint64_t passwd_slot_mask;
} shadnet_SetRoomDataInternalRequest;
extern const pb_msg_desc_t shadnet_SetRoomDataInternalRequest_desc;

typedef struct shadnet_SetRoomMemberDataInternalRequest {
    uint32_t req_id;
    uint64_t room_id;
    uint32_t member_id;
    uint32_t team_id;
    uint32_t flag_filter;
    uint32_t flag_attr;
    struct {
        struct shadnet_MatchingBinAttr* items;
        size_t count;
    } bin_attrs;
} shadnet_SetRoomMemberDataInternalRequest;
extern const pb_msg_desc_t shadnet_SetRoomMemberDataInternalRequest_desc;

typedef struct shadnet_SetRoomDataExternalRequest {
    uint32_t req_id;
    uint64_t room_id;
    struct {
        struct shadnet_MatchingIntAttr* items;
        size_t count;
    } search_int_attrs;
    struct {
        struct shadnet_MatchingBinAttr* items;
        size_t count;
    } search_bin_attrs;
    struct {
        struct shadnet_MatchingBinAttr* items;
        size_t count;
    } ext_bin_attrs;
} shadnet_SetRoomDataExternalRequest;
extern const pb_msg_desc_t shadnet_SetRoomDataExternalRequest_desc;

typedef struct shadnet_KickoutRoomMemberRequest {
    uint64_t room_id;
    uint32_t req_id;
    uint32_t target_member_id;
    uint32_t block_kick_flag;
} shadnet_KickoutRoomMemberRequest;
extern const pb_msg_desc_t shadnet_KickoutRoomMemberRequest_desc;

typedef struct shadnet_CreateRoomReply {
    uint64_t room_id;
    uint32_t server_id;
    uint32_t world_id;
    uint32_t lobby_id;
    uint32_t member_id;
    uint32_t max_slots;
    uint32_t flags;
    uint32_t cur_members;
    struct shadnet_CreateJoinRoomResponse details;
    uint8_t has_details;
} shadnet_CreateRoomReply;
extern const pb_msg_desc_t shadnet_CreateRoomReply_desc;

typedef struct shadnet_JoinRoomReply {
    uint64_t room_id;
    uint32_t member_id;
    uint32_t max_slots;
    uint32_t flags;
    uint32_t cur_members;
    struct shadnet_CreateJoinRoomResponse details;
    uint8_t has_details;
} shadnet_JoinRoomReply;
extern const pb_msg_desc_t shadnet_JoinRoomReply_desc;

typedef struct shadnet_MatchingIntSearchFilter {
    uint32_t op;
    uint32_t attr_id;
    uint32_t attr_value;
} shadnet_MatchingIntSearchFilter;
extern const pb_msg_desc_t shadnet_MatchingIntSearchFilter_desc;

typedef struct shadnet_MatchingBinSearchFilter {
    uint32_t op;
    uint32_t attr_id;
    pb_bytes_t data;
} shadnet_MatchingBinSearchFilter;
extern const pb_msg_desc_t shadnet_MatchingBinSearchFilter_desc;

typedef struct shadnet_SearchRoomRequest {
    uint32_t world_id;
    uint64_t lobby_id;
    uint32_t range_filter_start;
    uint32_t range_filter_max;
    uint32_t flag_filter;
    uint32_t flag_attr;
    struct {
        struct shadnet_MatchingIntSearchFilter* items;
        size_t count;
    } int_filters;
    struct {
        struct shadnet_MatchingBinSearchFilter* items;
        size_t count;
    } bin_filters;
    struct {
        uint32_t* items;
        size_t count;
    } attr_ids;
} shadnet_SearchRoomRequest;
extern const pb_msg_desc_t shadnet_SearchRoomRequest_desc;

typedef struct shadnet_SearchRoomReply {
    struct {
        struct shadnet_MatchingRoomDataExternal* items;
        size_t count;
    } rooms;
    uint32_t range_start;
    uint32_t range_total;
    uint32_t range_result;
} shadnet_SearchRoomReply;
extern const pb_msg_desc_t shadnet_SearchRoomReply_desc;

typedef struct shadnet_GetRoomDataExternalListRequest {
    struct {
        uint64_t* items;
        size_t count;
    } room_ids;
    struct {
        uint32_t* items;
        size_t count;
    } attr_ids;
} shadnet_GetRoomDataExternalListRequest;
extern const pb_msg_desc_t shadnet_GetRoomDataExternalListRequest_desc;

typedef struct shadnet_GetRoomDataExternalListReply {
    struct {
        struct shadnet_MatchingRoomDataExternal* items;
        size_t count;
    } rooms;
} shadnet_GetRoomDataExternalListReply;
extern const pb_msg_desc_t shadnet_GetRoomDataExternalListReply_desc;

typedef struct shadnet_GetRoomMemberDataExternalListRequest {
    uint64_t room_id;
} shadnet_GetRoomMemberDataExternalListRequest;
extern const pb_msg_desc_t shadnet_GetRoomMemberDataExternalListRequest_desc;

typedef struct shadnet_GetRoomMemberDataExternalListReply {
    struct {
        struct shadnet_MatchingRoomMemberDataExternal* items;
        size_t count;
    } members;
} shadnet_GetRoomMemberDataExternalListReply;
extern const pb_msg_desc_t shadnet_GetRoomMemberDataExternalListReply_desc;

typedef struct shadnet_MatchingUserInfo {
    pb_bytes_t npid;
    uint64_t account_id;
    uint32_t platform;
    struct {
        struct shadnet_MatchingBinAttr* items;
        size_t count;
    } user_bin_attrs;
} shadnet_MatchingUserInfo;
extern const pb_msg_desc_t shadnet_MatchingUserInfo_desc;

typedef struct shadnet_SetUserInfoRequest {
    uint32_t server_id;
    struct {
        struct shadnet_MatchingBinAttr* items;
        size_t count;
    } user_bin_attrs;
} shadnet_SetUserInfoRequest;
extern const pb_msg_desc_t shadnet_SetUserInfoRequest_desc;

typedef struct shadnet_SetUserInfoReply {
    uint8_t unused_; /* no fields */
} shadnet_SetUserInfoReply;
extern const pb_msg_desc_t shadnet_SetUserInfoReply_desc;

typedef struct shadnet_GetUserInfoListRequest {
    struct {
        pb_bytes_t* items;
        size_t count;
    } npids;
    struct {
        uint32_t* items;
        size_t count;
    } attr_ids;
    int32_t option;
} shadnet_GetUserInfoListRequest;
extern const pb_msg_desc_t shadnet_GetUserInfoListRequest_desc;

typedef struct shadnet_GetUserInfoListReply {
    struct {
        struct shadnet_MatchingUserInfo* items;
        size_t count;
    } users;
} shadnet_GetUserInfoListReply;
extern const pb_msg_desc_t shadnet_GetUserInfoListReply_desc;

typedef struct shadnet_MatchingWorld {
    uint32_t world_id;
    uint32_t lobbies_num;
    uint32_t max_lobby_members;
    uint32_t lobby_members_num;
    uint32_t rooms_num;
    uint32_t room_members_num;
} shadnet_MatchingWorld;
extern const pb_msg_desc_t shadnet_MatchingWorld_desc;

typedef struct shadnet_GetWorldInfoListRequest {
    uint32_t server_id;
} shadnet_GetWorldInfoListRequest;
extern const pb_msg_desc_t shadnet_GetWorldInfoListRequest_desc;

typedef struct shadnet_GetWorldInfoListReply {
    struct {
        struct shadnet_MatchingWorld* items;
        size_t count;
    } worlds;
} shadnet_GetWorldInfoListReply;
extern const pb_msg_desc_t shadnet_GetWorldInfoListReply_desc;

typedef struct shadnet_RequestSignalingInfosReply {
    pb_bytes_t target_npid;
    pb_bytes_t target_ip;
    uint32_t target_port;
    uint32_t target_member_id;
} shadnet_RequestSignalingInfosReply;
extern const pb_msg_desc_t shadnet_RequestSignalingInfosReply_desc;

typedef struct shadnet_LeaveRoomReply {
    uint64_t room_id;
} shadnet_LeaveRoomReply;
extern const pb_msg_desc_t shadnet_LeaveRoomReply_desc;

typedef struct shadnet_SetRoomDataInternalReply {
    uint64_t room_id;
} shadnet_SetRoomDataInternalReply;
extern const pb_msg_desc_t shadnet_SetRoomDataInternalReply_desc;

typedef struct shadnet_SetRoomMemberDataInternalReply {
    uint64_t room_id;
} shadnet_SetRoomMemberDataInternalReply;
extern const pb_msg_desc_t shadnet_SetRoomMemberDataInternalReply_desc;

typedef struct shadnet_SetRoomDataExternalReply {
    uint64_t room_id;
} shadnet_SetRoomDataExternalReply;
extern const pb_msg_desc_t shadnet_SetRoomDataExternalReply_desc;

typedef struct shadnet_KickoutRoomMemberReply {
    uint64_t room_id;
} shadnet_KickoutRoomMemberReply;
extern const pb_msg_desc_t shadnet_KickoutRoomMemberReply_desc;

typedef struct shadnet_SendRoomMessageRequest {
    uint64_t room_id;
    uint32_t cast_type;
    struct {
        uint32_t* items;
        size_t count;
    } dst_member_ids;
    pb_bytes_t msg;
    int32_t option;
} shadnet_SendRoomMessageRequest;
extern const pb_msg_desc_t shadnet_SendRoomMessageRequest_desc;

typedef struct shadnet_SendRoomMessageReply {
    uint64_t room_id;
} shadnet_SendRoomMessageReply;
extern const pb_msg_desc_t shadnet_SendRoomMessageReply_desc;

typedef struct shadnet_GetTokenReply {
    pb_bytes_t token;
    uint64_t user_id;
    pb_bytes_t npid;
} shadnet_GetTokenReply;
extern const pb_msg_desc_t shadnet_GetTokenReply_desc;

typedef struct shadnet_LookupOnlineIdRequest {
    pb_bytes_t npid;
} shadnet_LookupOnlineIdRequest;
extern const pb_msg_desc_t shadnet_LookupOnlineIdRequest_desc;

typedef struct shadnet_LookupOnlineIdReply {
    uint64_t account_id;
    pb_bytes_t npid;
} shadnet_LookupOnlineIdReply;
extern const pb_msg_desc_t shadnet_LookupOnlineIdReply_desc;

typedef struct shadnet_GetAuthorizationCodeRequest {
    pb_bytes_t client_id;
    pb_bytes_t scope;
} shadnet_GetAuthorizationCodeRequest;
extern const pb_msg_desc_t shadnet_GetAuthorizationCodeRequest_desc;

typedef struct shadnet_GetAuthorizationCodeReply {
    pb_bytes_t code;
    uint32_t expires_in;
} shadnet_GetAuthorizationCodeReply;
extern const pb_msg_desc_t shadnet_GetAuthorizationCodeReply_desc;

typedef struct shadnet_NotifyRoomEvent {
    uint32_t ctx_id;
    uint64_t room_id;
    uint32_t event;
    uint32_t event_cause;
    int32_t error_code;
    struct shadnet_MatchingRoomMemberData member;
    uint8_t has_member;
    struct {
        struct shadnet_MatchingBinAttr* items;
        size_t count;
    } bin_attrs;
    uint32_t flags;
    uint8_t has_passwd_mask;
    uint64_t passwd_slot_mask;
} shadnet_NotifyRoomEvent;
extern const pb_msg_desc_t shadnet_NotifyRoomEvent_desc;

typedef struct shadnet_NotifyRoomMessage {
    uint32_t ctx_id;
    uint64_t room_id;
    uint32_t src_member_id;
    uint32_t event;
    uint32_t cast_type;
    struct {
        uint32_t* items;
        size_t count;
    } dst_member_ids;
    pb_bytes_t src_npid;
    uint64_t src_account_id;
    uint32_t src_platform;
    pb_bytes_t msg;
} shadnet_NotifyRoomMessage;
extern const pb_msg_desc_t shadnet_NotifyRoomMessage_desc;

typedef struct shadnet_TusVariable {
    pb_bytes_t ownerNpId;
    uint8_t set;
    int64_t variable;
    int64_t oldVariable;
    uint64_t lastChangedDate;
    pb_bytes_t lastChangedAuthorNpId;
    int64_t ownerAccountId;
    int64_t lastChangedAuthorAccountId;
} shadnet_TusVariable;
extern const pb_msg_desc_t shadnet_TusVariable_desc;

typedef struct shadnet_TusDataStatus {
    pb_bytes_t ownerNpId;
    uint8_t set;
    uint64_t lastChangedDate;
    pb_bytes_t lastChangedAuthorNpId;
    uint64_t dataSize;
    pb_bytes_t info;
    int64_t ownerAccountId;
    int64_t lastChangedAuthorAccountId;
} shadnet_TusDataStatus;
extern const pb_msg_desc_t shadnet_TusDataStatus_desc;

typedef struct shadnet_TusSetMultiSlotVariableRequest {
    pb_bytes_t ownerNpId;
    struct {
        int32_t* items;
        size_t count;
    } slotIds;
    struct {
        int64_t* items;
        size_t count;
    } values;
    int64_t ownerAccountId;
    pb_bytes_t virtualUser;
} shadnet_TusSetMultiSlotVariableRequest;
extern const pb_msg_desc_t shadnet_TusSetMultiSlotVariableRequest_desc;

typedef struct shadnet_TusGetMultiSlotVariableRequest {
    pb_bytes_t ownerNpId;
    struct {
        int32_t* items;
        size_t count;
    } slotIds;
    int64_t ownerAccountId;
    pb_bytes_t virtualUser;
} shadnet_TusGetMultiSlotVariableRequest;
extern const pb_msg_desc_t shadnet_TusGetMultiSlotVariableRequest_desc;

typedef struct shadnet_TusAddAndGetVariableRequest {
    pb_bytes_t ownerNpId;
    int32_t slotId;
    int64_t inValue;
    int64_t ownerAccountId;
    pb_bytes_t virtualUser;
    uint8_t hasAuthorCheck;
    int64_t isLastChangedAuthor;
    uint8_t hasDateCheck;
    uint64_t isLastChangedDate;
    pb_bytes_t isLastChangedAuthorNpId;
} shadnet_TusAddAndGetVariableRequest;
extern const pb_msg_desc_t shadnet_TusAddAndGetVariableRequest_desc;

typedef struct shadnet_TusVariableResponse {
    struct {
        struct shadnet_TusVariable* items;
        size_t count;
    } variables;
    uint32_t total;
} shadnet_TusVariableResponse;
extern const pb_msg_desc_t shadnet_TusVariableResponse_desc;

typedef struct shadnet_TusSetDataRequest {
    pb_bytes_t ownerNpId;
    int32_t slotId;
    pb_bytes_t data;
    pb_bytes_t info;
    int64_t ownerAccountId;
    pb_bytes_t virtualUser;
    uint8_t hasAuthorCheck;
    int64_t isLastChangedAuthor;
    uint8_t hasDateCheck;
    uint64_t isLastChangedDate;
    pb_bytes_t isLastChangedAuthorNpId;
} shadnet_TusSetDataRequest;
extern const pb_msg_desc_t shadnet_TusSetDataRequest_desc;

typedef struct shadnet_TusGetDataRequest {
    pb_bytes_t ownerNpId;
    int32_t slotId;
    int64_t ownerAccountId;
    pb_bytes_t virtualUser;
} shadnet_TusGetDataRequest;
extern const pb_msg_desc_t shadnet_TusGetDataRequest_desc;

typedef struct shadnet_TusGetDataResponse {
    struct shadnet_TusDataStatus status;
    uint8_t has_status;
    pb_bytes_t data;
} shadnet_TusGetDataResponse;
extern const pb_msg_desc_t shadnet_TusGetDataResponse_desc;

typedef struct shadnet_TusGetMultiSlotDataStatusRequest {
    pb_bytes_t ownerNpId;
    struct {
        int32_t* items;
        size_t count;
    } slotIds;
    int64_t ownerAccountId;
    pb_bytes_t virtualUser;
} shadnet_TusGetMultiSlotDataStatusRequest;
extern const pb_msg_desc_t shadnet_TusGetMultiSlotDataStatusRequest_desc;

typedef struct shadnet_TusGetMultiUserDataStatusRequest {
    int32_t slotId;
    struct {
        pb_bytes_t* items;
        size_t count;
    } ownerNpIds;
    struct {
        pb_bytes_t* items;
        size_t count;
    } virtualUsers;
    struct {
        int64_t* items;
        size_t count;
    } ownerAccountIds;
} shadnet_TusGetMultiUserDataStatusRequest;
extern const pb_msg_desc_t shadnet_TusGetMultiUserDataStatusRequest_desc;

typedef struct shadnet_TusGetFriendsDataStatusRequest {
    int32_t slotId;
    uint8_t includeSelf;
    uint32_t max;
    int32_t sortType;
    uint32_t startOffset;
} shadnet_TusGetFriendsDataStatusRequest;
extern const pb_msg_desc_t shadnet_TusGetFriendsDataStatusRequest_desc;

typedef struct shadnet_TusGetFriendsVariableRequest {
    int32_t slotId;
    uint8_t includeSelf;
    uint32_t max;
    int32_t sortType;
    uint32_t startOffset;
} shadnet_TusGetFriendsVariableRequest;
extern const pb_msg_desc_t shadnet_TusGetFriendsVariableRequest_desc;

typedef struct shadnet_TusDataStatusResponse {
    struct {
        struct shadnet_TusDataStatus* items;
        size_t count;
    } statuses;
    uint32_t total;
} shadnet_TusDataStatusResponse;
extern const pb_msg_desc_t shadnet_TusDataStatusResponse_desc;

typedef struct shadnet_TusTryAndSetVariableRequest {
    pb_bytes_t ownerNpId;
    int64_t ownerAccountId;
    pb_bytes_t virtualUser;
    int32_t slotId;
    int32_t opeType;
    int64_t value;
    uint8_t hasCompare;
    int64_t compareValue;
    uint8_t hasAuthorCheck;
    int64_t isLastChangedAuthor;
    uint8_t hasDateCheck;
    uint64_t isLastChangedDate;
    pb_bytes_t isLastChangedAuthorNpId;
} shadnet_TusTryAndSetVariableRequest;
extern const pb_msg_desc_t shadnet_TusTryAndSetVariableRequest_desc;

typedef struct shadnet_TusDeleteMultiSlotDataRequest {
    pb_bytes_t ownerNpId;
    struct {
        int32_t* items;
        size_t count;
    } slotIds;
    int64_t ownerAccountId;
    pb_bytes_t virtualUser;
} shadnet_TusDeleteMultiSlotDataRequest;
extern const pb_msg_desc_t shadnet_TusDeleteMultiSlotDataRequest_desc;

typedef struct shadnet_TusDeleteMultiSlotVariableRequest {
    pb_bytes_t ownerNpId;
    pb_bytes_t virtualUser;
    struct {
        int32_t* items;
        size_t count;
    } slotIds;
    int64_t ownerAccountId;
} shadnet_TusDeleteMultiSlotVariableRequest;
extern const pb_msg_desc_t shadnet_TusDeleteMultiSlotVariableRequest_desc;

typedef struct shadnet_TusGetMultiUserVariableRequest {
    int32_t slotId;
    struct {
        int64_t* items;
        size_t count;
    } ownerAccountIds;
    struct {
        pb_bytes_t* items;
        size_t count;
    } virtualUsers;
    struct {
        pb_bytes_t* items;
        size_t count;
    } ownerNpIds;
} shadnet_TusGetMultiUserVariableRequest;
extern const pb_msg_desc_t shadnet_TusGetMultiUserVariableRequest_desc;

typedef struct shadnet_TssGetDataRequest {
    int32_t slotId;
    uint8_t hasOffset;
    uint64_t offset;
    uint8_t hasLastByte;
    uint64_t lastByte;
    uint8_t hasIfParam;
    int32_t ifType;
    uint64_t ifLastModified;
} shadnet_TssGetDataRequest;
extern const pb_msg_desc_t shadnet_TssGetDataRequest_desc;

typedef struct shadnet_TssGetDataResponse {
    int32_t statusCodeType;
    uint64_t lastModified;
    uint64_t contentLength;
    pb_bytes_t data;
} shadnet_TssGetDataResponse;
extern const pb_msg_desc_t shadnet_TssGetDataResponse_desc;

typedef struct shadnet_TrophyEntry {
    int32_t trophyId;
    uint64_t timestamp;
} shadnet_TrophyEntry;
extern const pb_msg_desc_t shadnet_TrophyEntry_desc;

typedef struct shadnet_UnlockTrophyRequest {
    int32_t trophyId;
    uint64_t timestamp;
} shadnet_UnlockTrophyRequest;
extern const pb_msg_desc_t shadnet_UnlockTrophyRequest_desc;

typedef struct shadnet_SyncTrophiesRequest {
    struct {
        struct shadnet_TrophyEntry* items;
        size_t count;
    } trophies;
} shadnet_SyncTrophiesRequest;
extern const pb_msg_desc_t shadnet_SyncTrophiesRequest_desc;

typedef struct shadnet_SyncTrophiesReply {
    struct {
        struct shadnet_TrophyEntry* items;
        size_t count;
    } trophies;
} shadnet_SyncTrophiesReply;
extern const pb_msg_desc_t shadnet_SyncTrophiesReply_desc;

typedef struct shadnet_SetClientVersionRequest {
    pb_bytes_t version;
} shadnet_SetClientVersionRequest;
extern const pb_msg_desc_t shadnet_SetClientVersionRequest_desc;

/* Every message, for X-macro use: X(LoginRequest) ... */
#define SHADNET_MESSAGES(X) \
    X(LoginRequest) \
    X(SetAppearOfflineRequest) \
    X(ServerFeaturesReply) \
    X(FriendEntry) \
    X(LoginReply) \
    X(RegistrationRequest) \
    X(FriendCommandRequest) \
    X(NotifyFriendQuery) \
    X(NotifyFriendNew) \
    X(NotifyFriendLost) \
    X(NotifyFriendStatus) \
    X(NotifyServiceUnavailable) \
    X(NotifyClientRejected) \
    X(BoardInfo) \
    X(RecordScoreRequest) \
    X(RecordScoreGameDataRequest) \
    X(GetScoreGameDataRequest) \
    X(GetScoreRangeRequest) \
    X(ScoreNpIdPcId) \
    X(GetScoreNpIdRequest) \
    X(GetScoreFriendsRequest) \
    X(ScoreAccountIdPcId) \
    X(GetScoreAccountIdRequest) \
    X(GetScoreGameDataByAccountIdRequest) \
    X(ScoreRankData) \
    X(ScoreInfo) \
    X(GetScoreResponse) \
    X(MatchingBinAttr) \
    X(MatchingIntAttr) \
    X(MatchingRoomGroup) \
    X(MatchingRoomGroupConfig) \
    X(MatchingRoomDataInternal) \
    X(MatchingRoomMemberData) \
    X(MatchingRoomMemberDataExternal) \
    X(MatchingRoomDataExternal) \
    X(CreateJoinRoomResponse) \
    X(ContextStartRequest) \
    X(ContextStopRequest) \
    X(CreateRoomRequest) \
    X(JoinRoomRequest) \
    X(LeaveRoomRequest) \
    X(RequestSignalingInfosRequest) \
    X(SetRoomDataInternalRequest) \
    X(SetRoomMemberDataInternalRequest) \
    X(SetRoomDataExternalRequest) \
    X(KickoutRoomMemberRequest) \
    X(CreateRoomReply) \
    X(JoinRoomReply) \
    X(MatchingIntSearchFilter) \
    X(MatchingBinSearchFilter) \
    X(SearchRoomRequest) \
    X(SearchRoomReply) \
    X(GetRoomDataExternalListRequest) \
    X(GetRoomDataExternalListReply) \
    X(GetRoomMemberDataExternalListRequest) \
    X(GetRoomMemberDataExternalListReply) \
    X(MatchingUserInfo) \
    X(SetUserInfoRequest) \
    X(SetUserInfoReply) \
    X(GetUserInfoListRequest) \
    X(GetUserInfoListReply) \
    X(MatchingWorld) \
    X(GetWorldInfoListRequest) \
    X(GetWorldInfoListReply) \
    X(RequestSignalingInfosReply) \
    X(LeaveRoomReply) \
    X(SetRoomDataInternalReply) \
    X(SetRoomMemberDataInternalReply) \
    X(SetRoomDataExternalReply) \
    X(KickoutRoomMemberReply) \
    X(SendRoomMessageRequest) \
    X(SendRoomMessageReply) \
    X(GetTokenReply) \
    X(LookupOnlineIdRequest) \
    X(LookupOnlineIdReply) \
    X(GetAuthorizationCodeRequest) \
    X(GetAuthorizationCodeReply) \
    X(NotifyRoomEvent) \
    X(NotifyRoomMessage) \
    X(TusVariable) \
    X(TusDataStatus) \
    X(TusSetMultiSlotVariableRequest) \
    X(TusGetMultiSlotVariableRequest) \
    X(TusAddAndGetVariableRequest) \
    X(TusVariableResponse) \
    X(TusSetDataRequest) \
    X(TusGetDataRequest) \
    X(TusGetDataResponse) \
    X(TusGetMultiSlotDataStatusRequest) \
    X(TusGetMultiUserDataStatusRequest) \
    X(TusGetFriendsDataStatusRequest) \
    X(TusGetFriendsVariableRequest) \
    X(TusDataStatusResponse) \
    X(TusTryAndSetVariableRequest) \
    X(TusDeleteMultiSlotDataRequest) \
    X(TusDeleteMultiSlotVariableRequest) \
    X(TusGetMultiUserVariableRequest) \
    X(TssGetDataRequest) \
    X(TssGetDataResponse) \
    X(TrophyEntry) \
    X(UnlockTrophyRequest) \
    X(SyncTrophiesRequest) \
    X(SyncTrophiesReply) \
    X(SetClientVersionRequest)

RETRO_END_DECLS

#endif
