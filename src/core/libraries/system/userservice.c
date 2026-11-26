/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

/* libSceUserService: who is logged in, their names and colours, and the queue of login events
 * the game polls. The rest of the library's functions are not implemented. */

#include <string.h>

#include <retro_atomic.h>

#include "common/c_bridge.h"
#include "core/emulator_settings.h"
#include "core/libraries/error_codes.h"
#include "core/libraries/np/np_handler/np_handler_c.h"
#include "core/libraries/system/userservice.h"
#include "core/libraries/system/userservice_error.h"
#include "core/user_manager.h"
#include "input/controller.h"

#define LOG_CLASS C_LOG_LIB_USER_SERVICE

/* The events the game has not taken yet. The frontend's thread and the controllers add, the
 * game's thread takes: an adder claims a slot by the count of events added, fills it and marks
 * it ready, and the game takes the slots in that order as they become ready. There are four
 * users to log in or out; an event that finds every slot taken is dropped and said so. */
#define EVENT_SLOTS 32

typedef struct event_slot {
    retro_atomic_int_t ready;
    s32 event;
    s32 user_id;
} event_slot_t;

static event_slot_t g_events[EVENT_SLOTS];
static retro_atomic_int_t g_events_added;
static retro_atomic_int_t g_events_taken;

void userservice_add_event(OrbisUserServiceEventType event, OrbisUserServiceUserId user_id) {
    unsigned added;
    event_slot_t* slot;
    if ((unsigned)retro_atomic_load_acquire_int(&g_events_added) -
            (unsigned)retro_atomic_load_acquire_int(&g_events_taken) >=
        EVENT_SLOTS - ORBIS_USER_SERVICE_MAX_REGISTER_USERS) {
        c_log(LOG_CLASS, C_LOG_LEVEL_ERROR, "userservice_add_event",
              "The game takes no events: %d for user %d dropped", (int)event, user_id);
        return;
    }
    added = (unsigned)retro_atomic_fetch_add_int(&g_events_added, 1);
    slot = &g_events[added % EVENT_SLOTS];
    slot->event = (s32)event;
    slot->user_id = user_id;
    retro_atomic_store_release_int(&slot->ready, 1);
    c_log(LOG_CLASS, C_LOG_LEVEL_DEBUG, "userservice_add_event", "Event added to queue: %d %d",
          (int)event, user_id);
}

/* The controllers' (input/controller.c) way to tell the game a user logged in */
void userservice_push_login(int32_t user_id) {
    userservice_add_event(ORBIS_USER_SERVICE_EVENT_TYPE_LOGIN, user_id);
}

s32 PS4_SYSV_ABI sceUserServiceGetEvent(OrbisUserServiceEvent* event) {
    unsigned taken = (unsigned)retro_atomic_load_relaxed_int(&g_events_taken);
    event_slot_t* slot = &g_events[taken % EVENT_SLOTS];
    if (!retro_atomic_load_acquire_int(&slot->ready)) {
        return ORBIS_USER_SERVICE_ERROR_NO_EVENT;
    }
    event->event = slot->event;
    event->userId = slot->user_id;
    retro_atomic_store_release_int(&slot->ready, 0);
    retro_atomic_store_release_int(&g_events_taken, (int)(taken + 1));
    np_notify_user_service_event(event->event, event->userId);
    c_log(LOG_CLASS, C_LOG_LEVEL_INFO, "sceUserServiceGetEvent",
          "Event processed by the game: %d %d", event->event, event->userId);
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceUserServiceGetInitialUser(int* user_id) {
    c_log(LOG_CLASS, C_LOG_LEVEL_DEBUG, "sceUserServiceGetInitialUser", "called");
    if (user_id == NULL) {
        c_log(LOG_CLASS, C_LOG_LEVEL_ERROR, "sceUserServiceGetInitialUser", "user_id is null");
        return ORBIS_USER_SERVICE_ERROR_INVALID_ARGUMENT;
    }
    *user_id = user_manager_get_default_user()->user_id;
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceUserServiceGetLoginUserIdList(OrbisUserServiceLoginUserIdList* userIdList) {
    int i;
    if (userIdList == NULL) {
        c_log(LOG_CLASS, C_LOG_LEVEL_ERROR, "sceUserServiceGetLoginUserIdList",
              "userIdList is null");
        return ORBIS_USER_SERVICE_ERROR_INVALID_ARGUMENT;
    }
    for (i = 0; i < ORBIS_USER_SERVICE_MAX_LOGIN_USERS; i++) {
        const struct user* u = user_manager_get_logged_in((unsigned)i);
        userIdList->user_id[i] = u ? u->user_id : ORBIS_USER_SERVICE_USER_ID_INVALID;
        c_log(LOG_CLASS, C_LOG_LEVEL_DEBUG, "sceUserServiceGetLoginUserIdList",
              "Slot %d: User ID %d (port %d)", i, userIdList->user_id[i], u ? u->player_index : -1);
    }
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceUserServiceGetUserColor(int user_id, s32* color) {
    const struct user* u;
    c_log(LOG_CLASS, C_LOG_LEVEL_DEBUG, "sceUserServiceGetUserColor", "called user_id = %d",
          user_id);
    if (color == NULL) {
        c_log(LOG_CLASS, C_LOG_LEVEL_ERROR, "sceUserServiceGetUserColor", "color is null");
        return ORBIS_USER_SERVICE_ERROR_INVALID_ARGUMENT;
    }
    u = user_manager_get_by_id(user_id);
    if (u == NULL) {
        c_log(LOG_CLASS, C_LOG_LEVEL_ERROR, "sceUserServiceGetUserColor", "No user found");
        return ORBIS_USER_SERVICE_ERROR_NOT_LOGGED_IN;
    }
    *color = (s32)u->user_color;
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceUserServiceGetUserName(int user_id, char* user_name, size_t size) {
    /* An NP online ID is 16 characters */
    char handle[32];
    const char* name = "shadPS4";
    const struct user* u;
    size_t length;
    c_log(LOG_CLASS, C_LOG_LEVEL_DEBUG, "sceUserServiceGetUserName",
          "called user_id = %d, size = %lu ", user_id, (unsigned long)size);
    if (user_id == ORBIS_USER_SERVICE_USER_ID_INVALID) {
        c_log(LOG_CLASS, C_LOG_LEVEL_ERROR, "sceUserServiceGetUserName", "invalid user_id");
        return ORBIS_USER_SERVICE_ERROR_INVALID_ARGUMENT;
    }
    if (user_name == NULL) {
        c_log(LOG_CLASS, C_LOG_LEVEL_ERROR, "sceUserServiceGetUserName", "user_name is null");
        return ORBIS_USER_SERVICE_ERROR_INVALID_ARGUMENT;
    }
    u = user_manager_get_by_id(user_id);
    if (u != NULL) {
        name = u->user_name;
    } else {
        c_log(LOG_CLASS, C_LOG_LEVEL_ERROR, "sceUserServiceGetUserName", "No user found");
    }
    /* Once signed in to shadnet the online ID is the user's name */
    if (emu_settings_is_shadnet_enabled() &&
        np_signed_in_online_id(user_id, handle, sizeof(handle)) > 0) {
        name = handle;
    }
    length = strlen(name);
    if (size < length + 1) {
        c_log(LOG_CLASS, C_LOG_LEVEL_ERROR, "sceUserServiceGetUserName", "buffer is too short");
        return ORBIS_USER_SERVICE_ERROR_BUFFER_TOO_SHORT;
    }
    memcpy(user_name, name, length + 1);
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceUserServiceInitialize(const OrbisUserServiceInitializeParams* initParams) {
    (void)initParams;
    c_log(LOG_CLASS, C_LOG_LEVEL_WARNING, "sceUserServiceInitialize", "(dummy) called");
    return ORBIS_OK;
}

/* The functions that are not implemented */
#define USER_SERVICE_STUB(name)                                                                    \
    int PS4_SYSV_ABI name(void) {                                                                  \
        c_log(LOG_CLASS, C_LOG_LEVEL_ERROR, #name, "(STUBBED) called");                            \
        return ORBIS_OK;                                                                           \
    }

USER_SERVICE_STUB(sceUserServiceInitializeForShellCore)
USER_SERVICE_STUB(sceUserServiceTerminateForShellCore)
USER_SERVICE_STUB(sceUserServiceDestroyUser)
USER_SERVICE_STUB(sceUserServiceGetAccessibilityKeyremapData)
USER_SERVICE_STUB(sceUserServiceGetAccessibilityKeyremapEnable)
USER_SERVICE_STUB(sceUserServiceGetAccessibilityPressAndHoldDelay)
USER_SERVICE_STUB(sceUserServiceGetAccessibilityVibration)
USER_SERVICE_STUB(sceUserServiceGetAccessibilityZoom)
USER_SERVICE_STUB(sceUserServiceGetAccessibilityZoomEnabled)
USER_SERVICE_STUB(sceUserServiceGetAccountRemarks)
USER_SERVICE_STUB(sceUserServiceGetAgeVerified)
USER_SERVICE_STUB(sceUserServiceGetAppearOfflineSetting)
USER_SERVICE_STUB(sceUserServiceGetAppSortOrder)
USER_SERVICE_STUB(sceUserServiceGetAutoLoginEnabled)
USER_SERVICE_STUB(sceUserServiceGetCreatedVersion)
USER_SERVICE_STUB(sceUserServiceGetCurrentUserGroupIndex)
USER_SERVICE_STUB(sceUserServiceGetDefaultNewUserGroupName)
USER_SERVICE_STUB(sceUserServiceGetDeletedUserInfo)
USER_SERVICE_STUB(sceUserServiceGetDiscPlayerFlag)
USER_SERVICE_STUB(sceUserServiceGetEventCalendarType)
USER_SERVICE_STUB(sceUserServiceGetEventFilterTeamEvent)
USER_SERVICE_STUB(sceUserServiceGetEventSortEvent)
USER_SERVICE_STUB(sceUserServiceGetEventSortTitle)
USER_SERVICE_STUB(sceUserServiceGetEventUiFlag)
USER_SERVICE_STUB(sceUserServiceGetEventVsh)
USER_SERVICE_STUB(sceUserServiceGetFaceRecognitionDeleteCount)
USER_SERVICE_STUB(sceUserServiceGetFaceRecognitionRegisterCount)
USER_SERVICE_STUB(sceUserServiceGetFileBrowserFilter)
USER_SERVICE_STUB(sceUserServiceGetFileBrowserSortContent)
USER_SERVICE_STUB(sceUserServiceGetFileBrowserSortTitle)
USER_SERVICE_STUB(sceUserServiceGetFileSelectorFilter)
USER_SERVICE_STUB(sceUserServiceGetFileSelectorSortContent)
USER_SERVICE_STUB(sceUserServiceGetFileSelectorSortTitle)
USER_SERVICE_STUB(sceUserServiceGetForegroundUser)
USER_SERVICE_STUB(sceUserServiceGetFriendCustomListLastFocus)
USER_SERVICE_STUB(sceUserServiceGetFriendFlag)
USER_SERVICE_STUB(sceUserServiceGetGlsAccessTokenNiconicoLive)
USER_SERVICE_STUB(sceUserServiceGetGlsAccessTokenTwitch)
USER_SERVICE_STUB(sceUserServiceGetGlsAccessTokenUstream)
USER_SERVICE_STUB(sceUserServiceGetGlsAnonymousUserId)
USER_SERVICE_STUB(sceUserServiceGetGlsBcTags)
USER_SERVICE_STUB(sceUserServiceGetGlsBcTitle)
USER_SERVICE_STUB(sceUserServiceGetGlsBroadcastChannel)
USER_SERVICE_STUB(sceUserServiceGetGlsBroadcastersComment)
USER_SERVICE_STUB(sceUserServiceGetGlsBroadcastersCommentColor)
USER_SERVICE_STUB(sceUserServiceGetGlsBroadcastService)
USER_SERVICE_STUB(sceUserServiceGetGlsBroadcastUiLayout)
USER_SERVICE_STUB(sceUserServiceGetGlsCamCrop)
USER_SERVICE_STUB(sceUserServiceGetGlsCameraBgFilter)
USER_SERVICE_STUB(sceUserServiceGetGlsCameraBrightness)
USER_SERVICE_STUB(sceUserServiceGetGlsCameraChromaKeyLevel)
USER_SERVICE_STUB(sceUserServiceGetGlsCameraContrast)
USER_SERVICE_STUB(sceUserServiceGetGlsCameraDepthLevel)
USER_SERVICE_STUB(sceUserServiceGetGlsCameraEdgeLevel)
USER_SERVICE_STUB(sceUserServiceGetGlsCameraEffect)
USER_SERVICE_STUB(sceUserServiceGetGlsCameraEliminationLevel)
USER_SERVICE_STUB(sceUserServiceGetGlsCameraPosition)
USER_SERVICE_STUB(sceUserServiceGetGlsCameraReflection)
USER_SERVICE_STUB(sceUserServiceGetGlsCameraSize)
USER_SERVICE_STUB(sceUserServiceGetGlsCameraTransparency)
USER_SERVICE_STUB(sceUserServiceGetGlsCommunityId)
USER_SERVICE_STUB(sceUserServiceGetGlsFloatingMessage)
USER_SERVICE_STUB(sceUserServiceGetGlsHintFlag)
USER_SERVICE_STUB(sceUserServiceGetGlsInitSpectating)
USER_SERVICE_STUB(sceUserServiceGetGlsIsCameraHidden)
USER_SERVICE_STUB(sceUserServiceGetGlsIsFacebookEnabled)
USER_SERVICE_STUB(sceUserServiceGetGlsIsMuteEnabled)
USER_SERVICE_STUB(sceUserServiceGetGlsIsRecDisabled)
USER_SERVICE_STUB(sceUserServiceGetGlsIsRecievedMessageHidden)
USER_SERVICE_STUB(sceUserServiceGetGlsIsTwitterEnabled)
USER_SERVICE_STUB(sceUserServiceGetGlsLanguageFilter)
USER_SERVICE_STUB(sceUserServiceGetGlsLfpsSortOrder)
USER_SERVICE_STUB(sceUserServiceGetGlsLiveQuality)
USER_SERVICE_STUB(sceUserServiceGetGlsLiveQuality2)
USER_SERVICE_STUB(sceUserServiceGetGlsLiveQuality3)
USER_SERVICE_STUB(sceUserServiceGetGlsLiveQuality4)
USER_SERVICE_STUB(sceUserServiceGetGlsLiveQuality5)
USER_SERVICE_STUB(sceUserServiceGetGlsMessageFilterLevel)
USER_SERVICE_STUB(sceUserServiceGetGlsTtsFlags)
USER_SERVICE_STUB(sceUserServiceGetGlsTtsPitch)
USER_SERVICE_STUB(sceUserServiceGetGlsTtsSpeed)
USER_SERVICE_STUB(sceUserServiceGetGlsTtsVolume)
USER_SERVICE_STUB(sceUserServiceGetHmuBrightness)
USER_SERVICE_STUB(sceUserServiceGetHmuZoom)
USER_SERVICE_STUB(sceUserServiceGetHoldAudioOutDevice)
USER_SERVICE_STUB(sceUserServiceGetHomeDirectory)
USER_SERVICE_STUB(sceUserServiceGetImeAutoCapitalEnabled)
USER_SERVICE_STUB(sceUserServiceGetImeInitFlag)
USER_SERVICE_STUB(sceUserServiceGetImeInputType)
USER_SERVICE_STUB(sceUserServiceGetImeLastUnit)
USER_SERVICE_STUB(sceUserServiceGetImePointerMode)
USER_SERVICE_STUB(sceUserServiceGetImePredictiveTextEnabled)
USER_SERVICE_STUB(sceUserServiceGetImeRunCount)
USER_SERVICE_STUB(sceUserServiceGetIPDLeft)
USER_SERVICE_STUB(sceUserServiceGetIPDRight)
USER_SERVICE_STUB(sceUserServiceGetIsFakePlus)
USER_SERVICE_STUB(sceUserServiceGetIsQuickSignup)
USER_SERVICE_STUB(sceUserServiceGetIsRemotePlayAllowed)
USER_SERVICE_STUB(sceUserServiceGetJapaneseInputType)
USER_SERVICE_STUB(sceUserServiceGetKeyboardType)
USER_SERVICE_STUB(sceUserServiceGetKeyRepeatSpeed)
USER_SERVICE_STUB(sceUserServiceGetKeyRepeatStartingTime)
USER_SERVICE_STUB(sceUserServiceGetKratosPrimaryUser)
USER_SERVICE_STUB(sceUserServiceGetLastLoginOrder)
USER_SERVICE_STUB(sceUserServiceGetLightBarBaseBrightness)
USER_SERVICE_STUB(sceUserServiceGetLoginFlag)
USER_SERVICE_STUB(sceUserServiceGetMicLevel)
USER_SERVICE_STUB(sceUserServiceGetMouseHandType)
USER_SERVICE_STUB(sceUserServiceGetMousePointerSpeed)
USER_SERVICE_STUB(sceUserServiceGetNotificationBehavior)
USER_SERVICE_STUB(sceUserServiceGetNotificationSettings)
USER_SERVICE_STUB(sceUserServiceGetNpAccountId)
USER_SERVICE_STUB(sceUserServiceGetNpAccountUpgradeFlag)
USER_SERVICE_STUB(sceUserServiceGetNpAge)
USER_SERVICE_STUB(sceUserServiceGetNpAuthErrorFlag)
USER_SERVICE_STUB(sceUserServiceGetNpCountryCode)
USER_SERVICE_STUB(sceUserServiceGetNpDateOfBirth)
USER_SERVICE_STUB(sceUserServiceGetNpEnv)
USER_SERVICE_STUB(sceUserServiceGetNpLanguageCode)
USER_SERVICE_STUB(sceUserServiceGetNpLanguageCode2)
USER_SERVICE_STUB(sceUserServiceGetNpLoginId)
USER_SERVICE_STUB(sceUserServiceGetNpMAccountId)
USER_SERVICE_STUB(sceUserServiceGetNpNpId)
USER_SERVICE_STUB(sceUserServiceGetNpOfflineAccountAdult)
USER_SERVICE_STUB(sceUserServiceGetNpOfflineAccountId)
USER_SERVICE_STUB(sceUserServiceGetNpOnlineId)
USER_SERVICE_STUB(sceUserServiceGetNpSubAccount)
USER_SERVICE_STUB(sceUserServiceGetPadSpeakerVolume)
USER_SERVICE_STUB(sceUserServiceGetParentalBdAge)
USER_SERVICE_STUB(sceUserServiceGetParentalBrowser)
USER_SERVICE_STUB(sceUserServiceGetParentalDvd)
USER_SERVICE_STUB(sceUserServiceGetParentalDvdRegion)
USER_SERVICE_STUB(sceUserServiceGetParentalGame)
USER_SERVICE_STUB(sceUserServiceGetParentalGameAgeLevel)
USER_SERVICE_STUB(sceUserServiceGetParentalMorpheus)
USER_SERVICE_STUB(sceUserServiceGetPartyMuteList)
USER_SERVICE_STUB(sceUserServiceGetPartyMuteListA)
USER_SERVICE_STUB(sceUserServiceGetPartySettingFlags)
USER_SERVICE_STUB(sceUserServiceGetPasscode)
USER_SERVICE_STUB(sceUserServiceGetPbtcAdditionalTime)
USER_SERVICE_STUB(sceUserServiceGetPbtcFlag)
USER_SERVICE_STUB(sceUserServiceGetPbtcFridayDuration)
USER_SERVICE_STUB(sceUserServiceGetPbtcFridayHoursEnd)
USER_SERVICE_STUB(sceUserServiceGetPbtcFridayHoursStart)
USER_SERVICE_STUB(sceUserServiceGetPbtcMode)
USER_SERVICE_STUB(sceUserServiceGetPbtcMondayDuration)
USER_SERVICE_STUB(sceUserServiceGetPbtcMondayHoursEnd)
USER_SERVICE_STUB(sceUserServiceGetPbtcMondayHoursStart)
USER_SERVICE_STUB(sceUserServiceGetPbtcPlayTime)
USER_SERVICE_STUB(sceUserServiceGetPbtcPlayTimeLastUpdated)
USER_SERVICE_STUB(sceUserServiceGetPbtcSaturdayDuration)
USER_SERVICE_STUB(sceUserServiceGetPbtcSaturdayHoursEnd)
USER_SERVICE_STUB(sceUserServiceGetPbtcSaturdayHoursStart)
USER_SERVICE_STUB(sceUserServiceGetPbtcSundayDuration)
USER_SERVICE_STUB(sceUserServiceGetPbtcSundayHoursEnd)
USER_SERVICE_STUB(sceUserServiceGetPbtcSundayHoursStart)
USER_SERVICE_STUB(sceUserServiceGetPbtcThursdayDuration)
USER_SERVICE_STUB(sceUserServiceGetPbtcThursdayHoursEnd)
USER_SERVICE_STUB(sceUserServiceGetPbtcThursdayHoursStart)
USER_SERVICE_STUB(sceUserServiceGetPbtcTuesdayDuration)
USER_SERVICE_STUB(sceUserServiceGetPbtcTuesdayHoursEnd)
USER_SERVICE_STUB(sceUserServiceGetPbtcTuesdayHoursStart)
USER_SERVICE_STUB(sceUserServiceGetPbtcTzOffset)
USER_SERVICE_STUB(sceUserServiceGetPbtcWednesdayDuration)
USER_SERVICE_STUB(sceUserServiceGetPbtcWednesdayHoursEnd)
USER_SERVICE_STUB(sceUserServiceGetPbtcWednesdayHoursStart)
USER_SERVICE_STUB(sceUserServiceGetPlayTogetherFlags)
USER_SERVICE_STUB(sceUserServiceGetPsnPasswordForDebug)
USER_SERVICE_STUB(sceUserServiceGetRegisteredHomeUserIdList)
USER_SERVICE_STUB(sceUserServiceGetRegisteredUserIdList)
USER_SERVICE_STUB(sceUserServiceGetSaveDataAutoUpload)
USER_SERVICE_STUB(sceUserServiceGetSaveDataSort)
USER_SERVICE_STUB(sceUserServiceGetSaveDataTutorialFlag)
USER_SERVICE_STUB(sceUserServiceGetSecureHomeDirectory)
USER_SERVICE_STUB(sceUserServiceGetShareButtonAssign)
USER_SERVICE_STUB(sceUserServiceGetShareDailymotionAccessToken)
USER_SERVICE_STUB(sceUserServiceGetShareDailymotionRefreshToken)
USER_SERVICE_STUB(sceUserServiceGetSharePlayFlags)
USER_SERVICE_STUB(sceUserServiceGetSharePlayFramerateHost)
USER_SERVICE_STUB(sceUserServiceGetSharePlayResolutionHost)
USER_SERVICE_STUB(sceUserServiceGetShareStatus)
USER_SERVICE_STUB(sceUserServiceGetShareStatus2)
USER_SERVICE_STUB(sceUserServiceGetSystemLoggerHashedAccountId)
USER_SERVICE_STUB(sceUserServiceGetSystemLoggerHashedAccountIdClockType)
USER_SERVICE_STUB(sceUserServiceGetSystemLoggerHashedAccountIdParam)
USER_SERVICE_STUB(sceUserServiceGetSystemLoggerHashedAccountIdTtl)
USER_SERVICE_STUB(sceUserServiceGetTeamShowAboutTeam)
USER_SERVICE_STUB(sceUserServiceGetThemeBgImageDimmer)
USER_SERVICE_STUB(sceUserServiceGetThemeBgImageWaveColor)
USER_SERVICE_STUB(sceUserServiceGetThemeBgImageZoom)
USER_SERVICE_STUB(sceUserServiceGetThemeEntitlementId)
USER_SERVICE_STUB(sceUserServiceGetThemeHomeShareOwner)
USER_SERVICE_STUB(sceUserServiceGetThemeTextShadow)
USER_SERVICE_STUB(sceUserServiceGetThemeWaveColor)
USER_SERVICE_STUB(sceUserServiceGetTopMenuLimitItem)
USER_SERVICE_STUB(sceUserServiceGetTopMenuNotificationFlag)
USER_SERVICE_STUB(sceUserServiceGetTopMenuTutorialFlag)
USER_SERVICE_STUB(sceUserServiceGetTraditionalChineseInputType)
USER_SERVICE_STUB(sceUserServiceGetUserGroupName)
USER_SERVICE_STUB(sceUserServiceGetUserGroupNameList)
USER_SERVICE_STUB(sceUserServiceGetUserGroupNum)
USER_SERVICE_STUB(sceUserServiceGetUserStatus)
USER_SERVICE_STUB(sceUserServiceGetVibrationEnabled)
USER_SERVICE_STUB(sceUserServiceGetVoiceRecognitionLastUsedOsk)
USER_SERVICE_STUB(sceUserServiceGetVoiceRecognitionTutorialState)
USER_SERVICE_STUB(sceUserServiceGetVolumeForController)
USER_SERVICE_STUB(sceUserServiceGetVolumeForGenericUSB)
USER_SERVICE_STUB(sceUserServiceGetVolumeForMorpheusSidetone)
USER_SERVICE_STUB(sceUserServiceGetVolumeForSidetone)
USER_SERVICE_STUB(sceUserServiceInitialize2)
USER_SERVICE_STUB(sceUserServiceIsGuestUser)
USER_SERVICE_STUB(sceUserServiceIsKratosPrimaryUser)
USER_SERVICE_STUB(sceUserServiceIsKratosUser)
USER_SERVICE_STUB(sceUserServiceIsLoggedIn)
USER_SERVICE_STUB(sceUserServiceIsLoggedInWithoutLock)
USER_SERVICE_STUB(sceUserServiceIsSharePlayClientUser)
USER_SERVICE_STUB(sceUserServiceIsUserStorageAccountBound)
USER_SERVICE_STUB(sceUserServiceLogin)
USER_SERVICE_STUB(sceUserServiceLogout)
USER_SERVICE_STUB(sceUserServiceRegisterEventCallback)
USER_SERVICE_STUB(sceUserServiceSetAccessibilityKeyremapData)
USER_SERVICE_STUB(sceUserServiceSetAccessibilityKeyremapEnable)
USER_SERVICE_STUB(sceUserServiceSetAccessibilityZoom)
USER_SERVICE_STUB(sceUserServiceSetAccountRemarks)
USER_SERVICE_STUB(sceUserServiceSetAgeVerified)
USER_SERVICE_STUB(sceUserServiceSetAppearOfflineSetting)
USER_SERVICE_STUB(sceUserServiceSetAppSortOrder)
USER_SERVICE_STUB(sceUserServiceSetAutoLoginEnabled)
USER_SERVICE_STUB(sceUserServiceSetCreatedVersion)
USER_SERVICE_STUB(sceUserServiceSetDiscPlayerFlag)
USER_SERVICE_STUB(sceUserServiceSetEventCalendarType)
USER_SERVICE_STUB(sceUserServiceSetEventFilterTeamEvent)
USER_SERVICE_STUB(sceUserServiceSetEventSortEvent)
USER_SERVICE_STUB(sceUserServiceSetEventSortTitle)
USER_SERVICE_STUB(sceUserServiceSetEventUiFlag)
USER_SERVICE_STUB(sceUserServiceSetFaceRecognitionDeleteCount)
USER_SERVICE_STUB(sceUserServiceSetFaceRecognitionRegisterCount)
USER_SERVICE_STUB(sceUserServiceSetFileBrowserFilter)
USER_SERVICE_STUB(sceUserServiceSetFileBrowserSortContent)
USER_SERVICE_STUB(sceUserServiceSetFileBrowserSortTitle)
USER_SERVICE_STUB(sceUserServiceSetFileSelectorFilter)
USER_SERVICE_STUB(sceUserServiceSetFileSelectorSortContent)
USER_SERVICE_STUB(sceUserServiceSetFileSelectorSortTitle)
USER_SERVICE_STUB(sceUserServiceSetForegroundUser)
USER_SERVICE_STUB(sceUserServiceSetFriendCustomListLastFocus)
USER_SERVICE_STUB(sceUserServiceSetFriendFlag)
USER_SERVICE_STUB(sceUserServiceSetGlsAccessTokenNiconicoLive)
USER_SERVICE_STUB(sceUserServiceSetGlsAccessTokenTwitch)
USER_SERVICE_STUB(sceUserServiceSetGlsAccessTokenUstream)
USER_SERVICE_STUB(sceUserServiceSetGlsAnonymousUserId)
USER_SERVICE_STUB(sceUserServiceSetGlsBcTags)
USER_SERVICE_STUB(sceUserServiceSetGlsBcTitle)
USER_SERVICE_STUB(sceUserServiceSetGlsBroadcastChannel)
USER_SERVICE_STUB(sceUserServiceSetGlsBroadcastersComment)
USER_SERVICE_STUB(sceUserServiceSetGlsBroadcastersCommentColor)
USER_SERVICE_STUB(sceUserServiceSetGlsBroadcastService)
USER_SERVICE_STUB(sceUserServiceSetGlsBroadcastUiLayout)
USER_SERVICE_STUB(sceUserServiceSetGlsCamCrop)
USER_SERVICE_STUB(sceUserServiceSetGlsCameraBgFilter)
USER_SERVICE_STUB(sceUserServiceSetGlsCameraBrightness)
USER_SERVICE_STUB(sceUserServiceSetGlsCameraChromaKeyLevel)
USER_SERVICE_STUB(sceUserServiceSetGlsCameraContrast)
USER_SERVICE_STUB(sceUserServiceSetGlsCameraDepthLevel)
USER_SERVICE_STUB(sceUserServiceSetGlsCameraEdgeLevel)
USER_SERVICE_STUB(sceUserServiceSetGlsCameraEffect)
USER_SERVICE_STUB(sceUserServiceSetGlsCameraEliminationLevel)
USER_SERVICE_STUB(sceUserServiceSetGlsCameraPosition)
USER_SERVICE_STUB(sceUserServiceSetGlsCameraReflection)
USER_SERVICE_STUB(sceUserServiceSetGlsCameraSize)
USER_SERVICE_STUB(sceUserServiceSetGlsCameraTransparency)
USER_SERVICE_STUB(sceUserServiceSetGlsCommunityId)
USER_SERVICE_STUB(sceUserServiceSetGlsFloatingMessage)
USER_SERVICE_STUB(sceUserServiceSetGlsHintFlag)
USER_SERVICE_STUB(sceUserServiceSetGlsInitSpectating)
USER_SERVICE_STUB(sceUserServiceSetGlsIsCameraHidden)
USER_SERVICE_STUB(sceUserServiceSetGlsIsFacebookEnabled)
USER_SERVICE_STUB(sceUserServiceSetGlsIsMuteEnabled)
USER_SERVICE_STUB(sceUserServiceSetGlsIsRecDisabled)
USER_SERVICE_STUB(sceUserServiceSetGlsIsRecievedMessageHidden)
USER_SERVICE_STUB(sceUserServiceSetGlsIsTwitterEnabled)
USER_SERVICE_STUB(sceUserServiceSetGlsLanguageFilter)
USER_SERVICE_STUB(sceUserServiceSetGlsLfpsSortOrder)
USER_SERVICE_STUB(sceUserServiceSetGlsLiveQuality)
USER_SERVICE_STUB(sceUserServiceSetGlsLiveQuality2)
USER_SERVICE_STUB(sceUserServiceSetGlsLiveQuality3)
USER_SERVICE_STUB(sceUserServiceSetGlsLiveQuality4)
USER_SERVICE_STUB(sceUserServiceSetGlsLiveQuality5)
USER_SERVICE_STUB(sceUserServiceSetGlsMessageFilterLevel)
USER_SERVICE_STUB(sceUserServiceSetGlsTtsFlags)
USER_SERVICE_STUB(sceUserServiceSetGlsTtsPitch)
USER_SERVICE_STUB(sceUserServiceSetGlsTtsSpeed)
USER_SERVICE_STUB(sceUserServiceSetGlsTtsVolume)
USER_SERVICE_STUB(sceUserServiceSetHmuBrightness)
USER_SERVICE_STUB(sceUserServiceSetHmuZoom)
USER_SERVICE_STUB(sceUserServiceSetHoldAudioOutDevice)
USER_SERVICE_STUB(sceUserServiceSetImeAutoCapitalEnabled)
USER_SERVICE_STUB(sceUserServiceSetImeInitFlag)
USER_SERVICE_STUB(sceUserServiceSetImeInputType)
USER_SERVICE_STUB(sceUserServiceSetImeLastUnit)
USER_SERVICE_STUB(sceUserServiceSetImePointerMode)
USER_SERVICE_STUB(sceUserServiceSetImePredictiveTextEnabled)
USER_SERVICE_STUB(sceUserServiceSetImeRunCount)
USER_SERVICE_STUB(sceUserServiceSetIPDLeft)
USER_SERVICE_STUB(sceUserServiceSetIPDRight)
USER_SERVICE_STUB(sceUserServiceSetIsFakePlus)
USER_SERVICE_STUB(sceUserServiceSetIsQuickSignup)
USER_SERVICE_STUB(sceUserServiceSetIsRemotePlayAllowed)
USER_SERVICE_STUB(sceUserServiceSetJapaneseInputType)
USER_SERVICE_STUB(sceUserServiceSetKeyboardType)
USER_SERVICE_STUB(sceUserServiceSetKeyRepeatSpeed)
USER_SERVICE_STUB(sceUserServiceSetKeyRepeatStartingTime)
USER_SERVICE_STUB(sceUserServiceSetLightBarBaseBrightness)
USER_SERVICE_STUB(sceUserServiceSetLoginFlag)
USER_SERVICE_STUB(sceUserServiceSetMicLevel)
USER_SERVICE_STUB(sceUserServiceSetMouseHandType)
USER_SERVICE_STUB(sceUserServiceSetMousePointerSpeed)
USER_SERVICE_STUB(sceUserServiceSetNotificationBehavior)
USER_SERVICE_STUB(sceUserServiceSetNotificationSettings)
USER_SERVICE_STUB(sceUserServiceSetNpAccountUpgradeFlag)
USER_SERVICE_STUB(sceUserServiceSetNpAge)
USER_SERVICE_STUB(sceUserServiceSetNpAuthErrorFlag)
USER_SERVICE_STUB(sceUserServiceSetNpCountryCode)
USER_SERVICE_STUB(sceUserServiceSetNpDateOfBirth)
USER_SERVICE_STUB(sceUserServiceSetNpEnv)
USER_SERVICE_STUB(sceUserServiceSetNpLanguageCode)
USER_SERVICE_STUB(sceUserServiceSetNpLanguageCode2)
USER_SERVICE_STUB(sceUserServiceSetNpLoginId)
USER_SERVICE_STUB(sceUserServiceSetNpMAccountId)
USER_SERVICE_STUB(sceUserServiceSetNpNpId)
USER_SERVICE_STUB(sceUserServiceSetNpOfflineAccountAdult)
USER_SERVICE_STUB(sceUserServiceSetNpOnlineId)
USER_SERVICE_STUB(sceUserServiceSetNpSubAccount)
USER_SERVICE_STUB(sceUserServiceSetPadSpeakerVolume)
USER_SERVICE_STUB(sceUserServiceSetParentalBdAge)
USER_SERVICE_STUB(sceUserServiceSetParentalBrowser)
USER_SERVICE_STUB(sceUserServiceSetParentalDvd)
USER_SERVICE_STUB(sceUserServiceSetParentalDvdRegion)
USER_SERVICE_STUB(sceUserServiceSetParentalGame)
USER_SERVICE_STUB(sceUserServiceSetParentalGameAgeLevel)
USER_SERVICE_STUB(sceUserServiceSetParentalMorpheus)
USER_SERVICE_STUB(sceUserServiceSetPartyMuteList)
USER_SERVICE_STUB(sceUserServiceSetPartyMuteListA)
USER_SERVICE_STUB(sceUserServiceSetPartySettingFlags)
USER_SERVICE_STUB(sceUserServiceSetPasscode)
USER_SERVICE_STUB(sceUserServiceSetPbtcAdditionalTime)
USER_SERVICE_STUB(sceUserServiceSetPbtcFlag)
USER_SERVICE_STUB(sceUserServiceSetPbtcFridayDuration)
USER_SERVICE_STUB(sceUserServiceSetPbtcFridayHoursEnd)
USER_SERVICE_STUB(sceUserServiceSetPbtcFridayHoursStart)
USER_SERVICE_STUB(sceUserServiceSetPbtcMode)
USER_SERVICE_STUB(sceUserServiceSetPbtcMondayDuration)
USER_SERVICE_STUB(sceUserServiceSetPbtcMondayHoursEnd)
USER_SERVICE_STUB(sceUserServiceSetPbtcMondayHoursStart)
USER_SERVICE_STUB(sceUserServiceSetPbtcPlayTime)
USER_SERVICE_STUB(sceUserServiceSetPbtcPlayTimeLastUpdated)
USER_SERVICE_STUB(sceUserServiceSetPbtcSaturdayDuration)
USER_SERVICE_STUB(sceUserServiceSetPbtcSaturdayHoursEnd)
USER_SERVICE_STUB(sceUserServiceSetPbtcSaturdayHoursStart)
USER_SERVICE_STUB(sceUserServiceSetPbtcSundayDuration)
USER_SERVICE_STUB(sceUserServiceSetPbtcSundayHoursEnd)
USER_SERVICE_STUB(sceUserServiceSetPbtcSundayHoursStart)
USER_SERVICE_STUB(sceUserServiceSetPbtcThursdayDuration)
USER_SERVICE_STUB(sceUserServiceSetPbtcThursdayHoursEnd)
USER_SERVICE_STUB(sceUserServiceSetPbtcThursdayHoursStart)
USER_SERVICE_STUB(sceUserServiceSetPbtcTuesdayDuration)
USER_SERVICE_STUB(sceUserServiceSetPbtcTuesdayHoursEnd)
USER_SERVICE_STUB(sceUserServiceSetPbtcTuesdayHoursStart)
USER_SERVICE_STUB(sceUserServiceSetPbtcTzOffset)
USER_SERVICE_STUB(sceUserServiceSetPbtcWednesdayDuration)
USER_SERVICE_STUB(sceUserServiceSetPbtcWednesdayHoursEnd)
USER_SERVICE_STUB(sceUserServiceSetPbtcWednesdayHoursStart)
USER_SERVICE_STUB(sceUserServiceSetPlayTogetherFlags)
USER_SERVICE_STUB(sceUserServiceSetPsnPasswordForDebug)
USER_SERVICE_STUB(sceUserServiceSetSaveDataAutoUpload)
USER_SERVICE_STUB(sceUserServiceSetSaveDataSort)
USER_SERVICE_STUB(sceUserServiceSetSaveDataTutorialFlag)
USER_SERVICE_STUB(sceUserServiceSetShareButtonAssign)
USER_SERVICE_STUB(sceUserServiceSetShareDailymotionAccessToken)
USER_SERVICE_STUB(sceUserServiceSetShareDailymotionRefreshToken)
USER_SERVICE_STUB(sceUserServiceSetSharePlayFlags)
USER_SERVICE_STUB(sceUserServiceSetSharePlayFramerateHost)
USER_SERVICE_STUB(sceUserServiceSetSharePlayResolutionHost)
USER_SERVICE_STUB(sceUserServiceSetShareStatus)
USER_SERVICE_STUB(sceUserServiceSetShareStatus2)
USER_SERVICE_STUB(sceUserServiceSetSystemLoggerHashedAccountId)
USER_SERVICE_STUB(sceUserServiceSetSystemLoggerHashedAccountIdClockType)
USER_SERVICE_STUB(sceUserServiceSetSystemLoggerHashedAccountIdParam)
USER_SERVICE_STUB(sceUserServiceSetSystemLoggerHashedAccountIdTtl)
USER_SERVICE_STUB(sceUserServiceSetTeamShowAboutTeam)
USER_SERVICE_STUB(sceUserServiceSetThemeBgImageDimmer)
USER_SERVICE_STUB(sceUserServiceSetThemeBgImageWaveColor)
USER_SERVICE_STUB(sceUserServiceSetThemeBgImageZoom)
USER_SERVICE_STUB(sceUserServiceSetThemeEntitlementId)
USER_SERVICE_STUB(sceUserServiceSetThemeHomeShareOwner)
USER_SERVICE_STUB(sceUserServiceSetThemeTextShadow)
USER_SERVICE_STUB(sceUserServiceSetThemeWaveColor)
USER_SERVICE_STUB(sceUserServiceSetTopMenuLimitItem)
USER_SERVICE_STUB(sceUserServiceSetTopMenuNotificationFlag)
USER_SERVICE_STUB(sceUserServiceSetTopMenuTutorialFlag)
USER_SERVICE_STUB(sceUserServiceSetTraditionalChineseInputType)
USER_SERVICE_STUB(sceUserServiceSetUserGroupIndex)
USER_SERVICE_STUB(sceUserServiceSetUserGroupName)
USER_SERVICE_STUB(sceUserServiceSetUserName)
USER_SERVICE_STUB(sceUserServiceSetUserStatus)
USER_SERVICE_STUB(sceUserServiceSetVibrationEnabled)
USER_SERVICE_STUB(sceUserServiceSetVoiceRecognitionLastUsedOsk)
USER_SERVICE_STUB(sceUserServiceSetVoiceRecognitionTutorialState)
USER_SERVICE_STUB(sceUserServiceSetVolumeForController)
USER_SERVICE_STUB(sceUserServiceSetVolumeForGenericUSB)
USER_SERVICE_STUB(sceUserServiceSetVolumeForMorpheusSidetone)
USER_SERVICE_STUB(sceUserServiceSetVolumeForSidetone)
USER_SERVICE_STUB(sceUserServiceTerminate)
USER_SERVICE_STUB(sceUserServiceUnregisterEventCallback)
USER_SERVICE_STUB(UserService_Func_8AC6DC4168D5FEA5)
USER_SERVICE_STUB(UserService_Func_A6BDC9DFDAFD02B4)
USER_SERVICE_STUB(UserService_Func_BB9491DFE6B4953C)
USER_SERVICE_STUB(UserService_Func_D2B814603E7B4477)
