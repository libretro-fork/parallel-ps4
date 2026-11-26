/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

/* libSceUserService. Reference:
 * https://github.com/OpenOrbis/OpenOrbis-PS4-Toolchain/blob/master/include/orbis/_types/user.h */

#ifndef SHADPS4_LIBRARIES_USERSERVICE_H
#define SHADPS4_LIBRARIES_USERSERVICE_H

#include <stddef.h>

#include "common/types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Users that can be logged in at once */
#define ORBIS_USER_SERVICE_MAX_LOGIN_USERS 4
/* Users that can be registered in the system */
#define ORBIS_USER_SERVICE_MAX_REGISTER_USERS 16
/* The longest user name */
#define ORBIS_USER_SERVICE_MAX_USER_NAME_LENGTH 16

#define ORBIS_USER_SERVICE_USER_ID_SYSTEM 0xFF
#define ORBIS_USER_SERVICE_USER_ID_INVALID (-1)

typedef s32 OrbisUserServiceUserId;

typedef struct OrbisUserServiceInitializeParams {
    s32 priority;
} OrbisUserServiceInitializeParams;

typedef struct OrbisUserServiceLoginUserIdList {
    int user_id[ORBIS_USER_SERVICE_MAX_LOGIN_USERS];
} OrbisUserServiceLoginUserIdList;

typedef struct OrbisUserServiceRegisteredUserIdList {
    OrbisUserServiceUserId userId[ORBIS_USER_SERVICE_MAX_REGISTER_USERS];
} OrbisUserServiceRegisteredUserIdList;

/* s32 in the game's structures */
typedef enum OrbisUserServiceUserColor {
    ORBIS_USER_SERVICE_USER_COLOR_BLUE = 0,
    ORBIS_USER_SERVICE_USER_COLOR_RED = 1,
    ORBIS_USER_SERVICE_USER_COLOR_GREEN = 2,
    ORBIS_USER_SERVICE_USER_COLOR_PINK = 3
} OrbisUserServiceUserColor;

typedef enum OrbisUserServiceEventType {
    ORBIS_USER_SERVICE_EVENT_TYPE_LOGIN = 0,
    ORBIS_USER_SERVICE_EVENT_TYPE_LOGOUT = 1
} OrbisUserServiceEventType;

typedef struct OrbisUserServiceEvent {
    s32 event; /* OrbisUserServiceEventType */
    OrbisUserServiceUserId userId;
} OrbisUserServiceEvent;

/* Queues an event for the game's next sceUserServiceGetEvent. From any thread. */
void userservice_add_event(OrbisUserServiceEventType event, OrbisUserServiceUserId user_id);

s32 PS4_SYSV_ABI sceUserServiceGetEvent(OrbisUserServiceEvent* event);
s32 PS4_SYSV_ABI sceUserServiceGetInitialUser(int* user_id);
s32 PS4_SYSV_ABI sceUserServiceGetLoginUserIdList(OrbisUserServiceLoginUserIdList* userIdList);
s32 PS4_SYSV_ABI sceUserServiceGetUserColor(int user_id, s32* color);
s32 PS4_SYSV_ABI sceUserServiceGetUserName(int user_id, char* user_name, size_t size);
s32 PS4_SYSV_ABI sceUserServiceInitialize(const OrbisUserServiceInitializeParams* initParams);

/* Not implemented: they log and return ORBIS_OK */
int PS4_SYSV_ABI sceUserServiceInitializeForShellCore(void);
int PS4_SYSV_ABI sceUserServiceTerminateForShellCore(void);
int PS4_SYSV_ABI sceUserServiceDestroyUser(void);
int PS4_SYSV_ABI sceUserServiceGetAccessibilityKeyremapData(void);
int PS4_SYSV_ABI sceUserServiceGetAccessibilityKeyremapEnable(void);
int PS4_SYSV_ABI sceUserServiceGetAccessibilityPressAndHoldDelay(void);
int PS4_SYSV_ABI sceUserServiceGetAccessibilityVibration(void);
int PS4_SYSV_ABI sceUserServiceGetAccessibilityZoom(void);
int PS4_SYSV_ABI sceUserServiceGetAccessibilityZoomEnabled(void);
int PS4_SYSV_ABI sceUserServiceGetAccountRemarks(void);
int PS4_SYSV_ABI sceUserServiceGetAgeVerified(void);
int PS4_SYSV_ABI sceUserServiceGetAppearOfflineSetting(void);
int PS4_SYSV_ABI sceUserServiceGetAppSortOrder(void);
int PS4_SYSV_ABI sceUserServiceGetAutoLoginEnabled(void);
int PS4_SYSV_ABI sceUserServiceGetCreatedVersion(void);
int PS4_SYSV_ABI sceUserServiceGetCurrentUserGroupIndex(void);
int PS4_SYSV_ABI sceUserServiceGetDefaultNewUserGroupName(void);
int PS4_SYSV_ABI sceUserServiceGetDeletedUserInfo(void);
int PS4_SYSV_ABI sceUserServiceGetDiscPlayerFlag(void);
int PS4_SYSV_ABI sceUserServiceGetEventCalendarType(void);
int PS4_SYSV_ABI sceUserServiceGetEventFilterTeamEvent(void);
int PS4_SYSV_ABI sceUserServiceGetEventSortEvent(void);
int PS4_SYSV_ABI sceUserServiceGetEventSortTitle(void);
int PS4_SYSV_ABI sceUserServiceGetEventUiFlag(void);
int PS4_SYSV_ABI sceUserServiceGetEventVsh(void);
int PS4_SYSV_ABI sceUserServiceGetFaceRecognitionDeleteCount(void);
int PS4_SYSV_ABI sceUserServiceGetFaceRecognitionRegisterCount(void);
int PS4_SYSV_ABI sceUserServiceGetFileBrowserFilter(void);
int PS4_SYSV_ABI sceUserServiceGetFileBrowserSortContent(void);
int PS4_SYSV_ABI sceUserServiceGetFileBrowserSortTitle(void);
int PS4_SYSV_ABI sceUserServiceGetFileSelectorFilter(void);
int PS4_SYSV_ABI sceUserServiceGetFileSelectorSortContent(void);
int PS4_SYSV_ABI sceUserServiceGetFileSelectorSortTitle(void);
int PS4_SYSV_ABI sceUserServiceGetForegroundUser(void);
int PS4_SYSV_ABI sceUserServiceGetFriendCustomListLastFocus(void);
int PS4_SYSV_ABI sceUserServiceGetFriendFlag(void);
int PS4_SYSV_ABI sceUserServiceGetGlsAccessTokenNiconicoLive(void);
int PS4_SYSV_ABI sceUserServiceGetGlsAccessTokenTwitch(void);
int PS4_SYSV_ABI sceUserServiceGetGlsAccessTokenUstream(void);
int PS4_SYSV_ABI sceUserServiceGetGlsAnonymousUserId(void);
int PS4_SYSV_ABI sceUserServiceGetGlsBcTags(void);
int PS4_SYSV_ABI sceUserServiceGetGlsBcTitle(void);
int PS4_SYSV_ABI sceUserServiceGetGlsBroadcastChannel(void);
int PS4_SYSV_ABI sceUserServiceGetGlsBroadcastersComment(void);
int PS4_SYSV_ABI sceUserServiceGetGlsBroadcastersCommentColor(void);
int PS4_SYSV_ABI sceUserServiceGetGlsBroadcastService(void);
int PS4_SYSV_ABI sceUserServiceGetGlsBroadcastUiLayout(void);
int PS4_SYSV_ABI sceUserServiceGetGlsCamCrop(void);
int PS4_SYSV_ABI sceUserServiceGetGlsCameraBgFilter(void);
int PS4_SYSV_ABI sceUserServiceGetGlsCameraBrightness(void);
int PS4_SYSV_ABI sceUserServiceGetGlsCameraChromaKeyLevel(void);
int PS4_SYSV_ABI sceUserServiceGetGlsCameraContrast(void);
int PS4_SYSV_ABI sceUserServiceGetGlsCameraDepthLevel(void);
int PS4_SYSV_ABI sceUserServiceGetGlsCameraEdgeLevel(void);
int PS4_SYSV_ABI sceUserServiceGetGlsCameraEffect(void);
int PS4_SYSV_ABI sceUserServiceGetGlsCameraEliminationLevel(void);
int PS4_SYSV_ABI sceUserServiceGetGlsCameraPosition(void);
int PS4_SYSV_ABI sceUserServiceGetGlsCameraReflection(void);
int PS4_SYSV_ABI sceUserServiceGetGlsCameraSize(void);
int PS4_SYSV_ABI sceUserServiceGetGlsCameraTransparency(void);
int PS4_SYSV_ABI sceUserServiceGetGlsCommunityId(void);
int PS4_SYSV_ABI sceUserServiceGetGlsFloatingMessage(void);
int PS4_SYSV_ABI sceUserServiceGetGlsHintFlag(void);
int PS4_SYSV_ABI sceUserServiceGetGlsInitSpectating(void);
int PS4_SYSV_ABI sceUserServiceGetGlsIsCameraHidden(void);
int PS4_SYSV_ABI sceUserServiceGetGlsIsFacebookEnabled(void);
int PS4_SYSV_ABI sceUserServiceGetGlsIsMuteEnabled(void);
int PS4_SYSV_ABI sceUserServiceGetGlsIsRecDisabled(void);
int PS4_SYSV_ABI sceUserServiceGetGlsIsRecievedMessageHidden(void);
int PS4_SYSV_ABI sceUserServiceGetGlsIsTwitterEnabled(void);
int PS4_SYSV_ABI sceUserServiceGetGlsLanguageFilter(void);
int PS4_SYSV_ABI sceUserServiceGetGlsLfpsSortOrder(void);
int PS4_SYSV_ABI sceUserServiceGetGlsLiveQuality(void);
int PS4_SYSV_ABI sceUserServiceGetGlsLiveQuality2(void);
int PS4_SYSV_ABI sceUserServiceGetGlsLiveQuality3(void);
int PS4_SYSV_ABI sceUserServiceGetGlsLiveQuality4(void);
int PS4_SYSV_ABI sceUserServiceGetGlsLiveQuality5(void);
int PS4_SYSV_ABI sceUserServiceGetGlsMessageFilterLevel(void);
int PS4_SYSV_ABI sceUserServiceGetGlsTtsFlags(void);
int PS4_SYSV_ABI sceUserServiceGetGlsTtsPitch(void);
int PS4_SYSV_ABI sceUserServiceGetGlsTtsSpeed(void);
int PS4_SYSV_ABI sceUserServiceGetGlsTtsVolume(void);
int PS4_SYSV_ABI sceUserServiceGetHmuBrightness(void);
int PS4_SYSV_ABI sceUserServiceGetHmuZoom(void);
int PS4_SYSV_ABI sceUserServiceGetHoldAudioOutDevice(void);
int PS4_SYSV_ABI sceUserServiceGetHomeDirectory(void);
int PS4_SYSV_ABI sceUserServiceGetImeAutoCapitalEnabled(void);
int PS4_SYSV_ABI sceUserServiceGetImeInitFlag(void);
int PS4_SYSV_ABI sceUserServiceGetImeInputType(void);
int PS4_SYSV_ABI sceUserServiceGetImeLastUnit(void);
int PS4_SYSV_ABI sceUserServiceGetImePointerMode(void);
int PS4_SYSV_ABI sceUserServiceGetImePredictiveTextEnabled(void);
int PS4_SYSV_ABI sceUserServiceGetImeRunCount(void);
int PS4_SYSV_ABI sceUserServiceGetIPDLeft(void);
int PS4_SYSV_ABI sceUserServiceGetIPDRight(void);
int PS4_SYSV_ABI sceUserServiceGetIsFakePlus(void);
int PS4_SYSV_ABI sceUserServiceGetIsQuickSignup(void);
int PS4_SYSV_ABI sceUserServiceGetIsRemotePlayAllowed(void);
int PS4_SYSV_ABI sceUserServiceGetJapaneseInputType(void);
int PS4_SYSV_ABI sceUserServiceGetKeyboardType(void);
int PS4_SYSV_ABI sceUserServiceGetKeyRepeatSpeed(void);
int PS4_SYSV_ABI sceUserServiceGetKeyRepeatStartingTime(void);
int PS4_SYSV_ABI sceUserServiceGetKratosPrimaryUser(void);
int PS4_SYSV_ABI sceUserServiceGetLastLoginOrder(void);
int PS4_SYSV_ABI sceUserServiceGetLightBarBaseBrightness(void);
int PS4_SYSV_ABI sceUserServiceGetLoginFlag(void);
int PS4_SYSV_ABI sceUserServiceGetMicLevel(void);
int PS4_SYSV_ABI sceUserServiceGetMouseHandType(void);
int PS4_SYSV_ABI sceUserServiceGetMousePointerSpeed(void);
int PS4_SYSV_ABI sceUserServiceGetNotificationBehavior(void);
int PS4_SYSV_ABI sceUserServiceGetNotificationSettings(void);
int PS4_SYSV_ABI sceUserServiceGetNpAccountId(void);
int PS4_SYSV_ABI sceUserServiceGetNpAccountUpgradeFlag(void);
int PS4_SYSV_ABI sceUserServiceGetNpAge(void);
int PS4_SYSV_ABI sceUserServiceGetNpAuthErrorFlag(void);
int PS4_SYSV_ABI sceUserServiceGetNpCountryCode(void);
int PS4_SYSV_ABI sceUserServiceGetNpDateOfBirth(void);
int PS4_SYSV_ABI sceUserServiceGetNpEnv(void);
int PS4_SYSV_ABI sceUserServiceGetNpLanguageCode(void);
int PS4_SYSV_ABI sceUserServiceGetNpLanguageCode2(void);
int PS4_SYSV_ABI sceUserServiceGetNpLoginId(void);
int PS4_SYSV_ABI sceUserServiceGetNpMAccountId(void);
int PS4_SYSV_ABI sceUserServiceGetNpNpId(void);
int PS4_SYSV_ABI sceUserServiceGetNpOfflineAccountAdult(void);
int PS4_SYSV_ABI sceUserServiceGetNpOfflineAccountId(void);
int PS4_SYSV_ABI sceUserServiceGetNpOnlineId(void);
int PS4_SYSV_ABI sceUserServiceGetNpSubAccount(void);
int PS4_SYSV_ABI sceUserServiceGetPadSpeakerVolume(void);
int PS4_SYSV_ABI sceUserServiceGetParentalBdAge(void);
int PS4_SYSV_ABI sceUserServiceGetParentalBrowser(void);
int PS4_SYSV_ABI sceUserServiceGetParentalDvd(void);
int PS4_SYSV_ABI sceUserServiceGetParentalDvdRegion(void);
int PS4_SYSV_ABI sceUserServiceGetParentalGame(void);
int PS4_SYSV_ABI sceUserServiceGetParentalGameAgeLevel(void);
int PS4_SYSV_ABI sceUserServiceGetParentalMorpheus(void);
int PS4_SYSV_ABI sceUserServiceGetPartyMuteList(void);
int PS4_SYSV_ABI sceUserServiceGetPartyMuteListA(void);
int PS4_SYSV_ABI sceUserServiceGetPartySettingFlags(void);
int PS4_SYSV_ABI sceUserServiceGetPasscode(void);
int PS4_SYSV_ABI sceUserServiceGetPbtcAdditionalTime(void);
int PS4_SYSV_ABI sceUserServiceGetPbtcFlag(void);
int PS4_SYSV_ABI sceUserServiceGetPbtcFridayDuration(void);
int PS4_SYSV_ABI sceUserServiceGetPbtcFridayHoursEnd(void);
int PS4_SYSV_ABI sceUserServiceGetPbtcFridayHoursStart(void);
int PS4_SYSV_ABI sceUserServiceGetPbtcMode(void);
int PS4_SYSV_ABI sceUserServiceGetPbtcMondayDuration(void);
int PS4_SYSV_ABI sceUserServiceGetPbtcMondayHoursEnd(void);
int PS4_SYSV_ABI sceUserServiceGetPbtcMondayHoursStart(void);
int PS4_SYSV_ABI sceUserServiceGetPbtcPlayTime(void);
int PS4_SYSV_ABI sceUserServiceGetPbtcPlayTimeLastUpdated(void);
int PS4_SYSV_ABI sceUserServiceGetPbtcSaturdayDuration(void);
int PS4_SYSV_ABI sceUserServiceGetPbtcSaturdayHoursEnd(void);
int PS4_SYSV_ABI sceUserServiceGetPbtcSaturdayHoursStart(void);
int PS4_SYSV_ABI sceUserServiceGetPbtcSundayDuration(void);
int PS4_SYSV_ABI sceUserServiceGetPbtcSundayHoursEnd(void);
int PS4_SYSV_ABI sceUserServiceGetPbtcSundayHoursStart(void);
int PS4_SYSV_ABI sceUserServiceGetPbtcThursdayDuration(void);
int PS4_SYSV_ABI sceUserServiceGetPbtcThursdayHoursEnd(void);
int PS4_SYSV_ABI sceUserServiceGetPbtcThursdayHoursStart(void);
int PS4_SYSV_ABI sceUserServiceGetPbtcTuesdayDuration(void);
int PS4_SYSV_ABI sceUserServiceGetPbtcTuesdayHoursEnd(void);
int PS4_SYSV_ABI sceUserServiceGetPbtcTuesdayHoursStart(void);
int PS4_SYSV_ABI sceUserServiceGetPbtcTzOffset(void);
int PS4_SYSV_ABI sceUserServiceGetPbtcWednesdayDuration(void);
int PS4_SYSV_ABI sceUserServiceGetPbtcWednesdayHoursEnd(void);
int PS4_SYSV_ABI sceUserServiceGetPbtcWednesdayHoursStart(void);
int PS4_SYSV_ABI sceUserServiceGetPlayTogetherFlags(void);
int PS4_SYSV_ABI sceUserServiceGetPsnPasswordForDebug(void);
int PS4_SYSV_ABI sceUserServiceGetRegisteredHomeUserIdList(void);
int PS4_SYSV_ABI sceUserServiceGetRegisteredUserIdList(void);
int PS4_SYSV_ABI sceUserServiceGetSaveDataAutoUpload(void);
int PS4_SYSV_ABI sceUserServiceGetSaveDataSort(void);
int PS4_SYSV_ABI sceUserServiceGetSaveDataTutorialFlag(void);
int PS4_SYSV_ABI sceUserServiceGetSecureHomeDirectory(void);
int PS4_SYSV_ABI sceUserServiceGetShareButtonAssign(void);
int PS4_SYSV_ABI sceUserServiceGetShareDailymotionAccessToken(void);
int PS4_SYSV_ABI sceUserServiceGetShareDailymotionRefreshToken(void);
int PS4_SYSV_ABI sceUserServiceGetSharePlayFlags(void);
int PS4_SYSV_ABI sceUserServiceGetSharePlayFramerateHost(void);
int PS4_SYSV_ABI sceUserServiceGetSharePlayResolutionHost(void);
int PS4_SYSV_ABI sceUserServiceGetShareStatus(void);
int PS4_SYSV_ABI sceUserServiceGetShareStatus2(void);
int PS4_SYSV_ABI sceUserServiceGetSystemLoggerHashedAccountId(void);
int PS4_SYSV_ABI sceUserServiceGetSystemLoggerHashedAccountIdClockType(void);
int PS4_SYSV_ABI sceUserServiceGetSystemLoggerHashedAccountIdParam(void);
int PS4_SYSV_ABI sceUserServiceGetSystemLoggerHashedAccountIdTtl(void);
int PS4_SYSV_ABI sceUserServiceGetTeamShowAboutTeam(void);
int PS4_SYSV_ABI sceUserServiceGetThemeBgImageDimmer(void);
int PS4_SYSV_ABI sceUserServiceGetThemeBgImageWaveColor(void);
int PS4_SYSV_ABI sceUserServiceGetThemeBgImageZoom(void);
int PS4_SYSV_ABI sceUserServiceGetThemeEntitlementId(void);
int PS4_SYSV_ABI sceUserServiceGetThemeHomeShareOwner(void);
int PS4_SYSV_ABI sceUserServiceGetThemeTextShadow(void);
int PS4_SYSV_ABI sceUserServiceGetThemeWaveColor(void);
int PS4_SYSV_ABI sceUserServiceGetTopMenuLimitItem(void);
int PS4_SYSV_ABI sceUserServiceGetTopMenuNotificationFlag(void);
int PS4_SYSV_ABI sceUserServiceGetTopMenuTutorialFlag(void);
int PS4_SYSV_ABI sceUserServiceGetTraditionalChineseInputType(void);
int PS4_SYSV_ABI sceUserServiceGetUserGroupName(void);
int PS4_SYSV_ABI sceUserServiceGetUserGroupNameList(void);
int PS4_SYSV_ABI sceUserServiceGetUserGroupNum(void);
int PS4_SYSV_ABI sceUserServiceGetUserStatus(void);
int PS4_SYSV_ABI sceUserServiceGetVibrationEnabled(void);
int PS4_SYSV_ABI sceUserServiceGetVoiceRecognitionLastUsedOsk(void);
int PS4_SYSV_ABI sceUserServiceGetVoiceRecognitionTutorialState(void);
int PS4_SYSV_ABI sceUserServiceGetVolumeForController(void);
int PS4_SYSV_ABI sceUserServiceGetVolumeForGenericUSB(void);
int PS4_SYSV_ABI sceUserServiceGetVolumeForMorpheusSidetone(void);
int PS4_SYSV_ABI sceUserServiceGetVolumeForSidetone(void);
int PS4_SYSV_ABI sceUserServiceInitialize2(void);
int PS4_SYSV_ABI sceUserServiceIsGuestUser(void);
int PS4_SYSV_ABI sceUserServiceIsKratosPrimaryUser(void);
int PS4_SYSV_ABI sceUserServiceIsKratosUser(void);
int PS4_SYSV_ABI sceUserServiceIsLoggedIn(void);
int PS4_SYSV_ABI sceUserServiceIsLoggedInWithoutLock(void);
int PS4_SYSV_ABI sceUserServiceIsSharePlayClientUser(void);
int PS4_SYSV_ABI sceUserServiceIsUserStorageAccountBound(void);
int PS4_SYSV_ABI sceUserServiceLogin(void);
int PS4_SYSV_ABI sceUserServiceLogout(void);
int PS4_SYSV_ABI sceUserServiceRegisterEventCallback(void);
int PS4_SYSV_ABI sceUserServiceSetAccessibilityKeyremapData(void);
int PS4_SYSV_ABI sceUserServiceSetAccessibilityKeyremapEnable(void);
int PS4_SYSV_ABI sceUserServiceSetAccessibilityZoom(void);
int PS4_SYSV_ABI sceUserServiceSetAccountRemarks(void);
int PS4_SYSV_ABI sceUserServiceSetAgeVerified(void);
int PS4_SYSV_ABI sceUserServiceSetAppearOfflineSetting(void);
int PS4_SYSV_ABI sceUserServiceSetAppSortOrder(void);
int PS4_SYSV_ABI sceUserServiceSetAutoLoginEnabled(void);
int PS4_SYSV_ABI sceUserServiceSetCreatedVersion(void);
int PS4_SYSV_ABI sceUserServiceSetDiscPlayerFlag(void);
int PS4_SYSV_ABI sceUserServiceSetEventCalendarType(void);
int PS4_SYSV_ABI sceUserServiceSetEventFilterTeamEvent(void);
int PS4_SYSV_ABI sceUserServiceSetEventSortEvent(void);
int PS4_SYSV_ABI sceUserServiceSetEventSortTitle(void);
int PS4_SYSV_ABI sceUserServiceSetEventUiFlag(void);
int PS4_SYSV_ABI sceUserServiceSetFaceRecognitionDeleteCount(void);
int PS4_SYSV_ABI sceUserServiceSetFaceRecognitionRegisterCount(void);
int PS4_SYSV_ABI sceUserServiceSetFileBrowserFilter(void);
int PS4_SYSV_ABI sceUserServiceSetFileBrowserSortContent(void);
int PS4_SYSV_ABI sceUserServiceSetFileBrowserSortTitle(void);
int PS4_SYSV_ABI sceUserServiceSetFileSelectorFilter(void);
int PS4_SYSV_ABI sceUserServiceSetFileSelectorSortContent(void);
int PS4_SYSV_ABI sceUserServiceSetFileSelectorSortTitle(void);
int PS4_SYSV_ABI sceUserServiceSetForegroundUser(void);
int PS4_SYSV_ABI sceUserServiceSetFriendCustomListLastFocus(void);
int PS4_SYSV_ABI sceUserServiceSetFriendFlag(void);
int PS4_SYSV_ABI sceUserServiceSetGlsAccessTokenNiconicoLive(void);
int PS4_SYSV_ABI sceUserServiceSetGlsAccessTokenTwitch(void);
int PS4_SYSV_ABI sceUserServiceSetGlsAccessTokenUstream(void);
int PS4_SYSV_ABI sceUserServiceSetGlsAnonymousUserId(void);
int PS4_SYSV_ABI sceUserServiceSetGlsBcTags(void);
int PS4_SYSV_ABI sceUserServiceSetGlsBcTitle(void);
int PS4_SYSV_ABI sceUserServiceSetGlsBroadcastChannel(void);
int PS4_SYSV_ABI sceUserServiceSetGlsBroadcastersComment(void);
int PS4_SYSV_ABI sceUserServiceSetGlsBroadcastersCommentColor(void);
int PS4_SYSV_ABI sceUserServiceSetGlsBroadcastService(void);
int PS4_SYSV_ABI sceUserServiceSetGlsBroadcastUiLayout(void);
int PS4_SYSV_ABI sceUserServiceSetGlsCamCrop(void);
int PS4_SYSV_ABI sceUserServiceSetGlsCameraBgFilter(void);
int PS4_SYSV_ABI sceUserServiceSetGlsCameraBrightness(void);
int PS4_SYSV_ABI sceUserServiceSetGlsCameraChromaKeyLevel(void);
int PS4_SYSV_ABI sceUserServiceSetGlsCameraContrast(void);
int PS4_SYSV_ABI sceUserServiceSetGlsCameraDepthLevel(void);
int PS4_SYSV_ABI sceUserServiceSetGlsCameraEdgeLevel(void);
int PS4_SYSV_ABI sceUserServiceSetGlsCameraEffect(void);
int PS4_SYSV_ABI sceUserServiceSetGlsCameraEliminationLevel(void);
int PS4_SYSV_ABI sceUserServiceSetGlsCameraPosition(void);
int PS4_SYSV_ABI sceUserServiceSetGlsCameraReflection(void);
int PS4_SYSV_ABI sceUserServiceSetGlsCameraSize(void);
int PS4_SYSV_ABI sceUserServiceSetGlsCameraTransparency(void);
int PS4_SYSV_ABI sceUserServiceSetGlsCommunityId(void);
int PS4_SYSV_ABI sceUserServiceSetGlsFloatingMessage(void);
int PS4_SYSV_ABI sceUserServiceSetGlsHintFlag(void);
int PS4_SYSV_ABI sceUserServiceSetGlsInitSpectating(void);
int PS4_SYSV_ABI sceUserServiceSetGlsIsCameraHidden(void);
int PS4_SYSV_ABI sceUserServiceSetGlsIsFacebookEnabled(void);
int PS4_SYSV_ABI sceUserServiceSetGlsIsMuteEnabled(void);
int PS4_SYSV_ABI sceUserServiceSetGlsIsRecDisabled(void);
int PS4_SYSV_ABI sceUserServiceSetGlsIsRecievedMessageHidden(void);
int PS4_SYSV_ABI sceUserServiceSetGlsIsTwitterEnabled(void);
int PS4_SYSV_ABI sceUserServiceSetGlsLanguageFilter(void);
int PS4_SYSV_ABI sceUserServiceSetGlsLfpsSortOrder(void);
int PS4_SYSV_ABI sceUserServiceSetGlsLiveQuality(void);
int PS4_SYSV_ABI sceUserServiceSetGlsLiveQuality2(void);
int PS4_SYSV_ABI sceUserServiceSetGlsLiveQuality3(void);
int PS4_SYSV_ABI sceUserServiceSetGlsLiveQuality4(void);
int PS4_SYSV_ABI sceUserServiceSetGlsLiveQuality5(void);
int PS4_SYSV_ABI sceUserServiceSetGlsMessageFilterLevel(void);
int PS4_SYSV_ABI sceUserServiceSetGlsTtsFlags(void);
int PS4_SYSV_ABI sceUserServiceSetGlsTtsPitch(void);
int PS4_SYSV_ABI sceUserServiceSetGlsTtsSpeed(void);
int PS4_SYSV_ABI sceUserServiceSetGlsTtsVolume(void);
int PS4_SYSV_ABI sceUserServiceSetHmuBrightness(void);
int PS4_SYSV_ABI sceUserServiceSetHmuZoom(void);
int PS4_SYSV_ABI sceUserServiceSetHoldAudioOutDevice(void);
int PS4_SYSV_ABI sceUserServiceSetImeAutoCapitalEnabled(void);
int PS4_SYSV_ABI sceUserServiceSetImeInitFlag(void);
int PS4_SYSV_ABI sceUserServiceSetImeInputType(void);
int PS4_SYSV_ABI sceUserServiceSetImeLastUnit(void);
int PS4_SYSV_ABI sceUserServiceSetImePointerMode(void);
int PS4_SYSV_ABI sceUserServiceSetImePredictiveTextEnabled(void);
int PS4_SYSV_ABI sceUserServiceSetImeRunCount(void);
int PS4_SYSV_ABI sceUserServiceSetIPDLeft(void);
int PS4_SYSV_ABI sceUserServiceSetIPDRight(void);
int PS4_SYSV_ABI sceUserServiceSetIsFakePlus(void);
int PS4_SYSV_ABI sceUserServiceSetIsQuickSignup(void);
int PS4_SYSV_ABI sceUserServiceSetIsRemotePlayAllowed(void);
int PS4_SYSV_ABI sceUserServiceSetJapaneseInputType(void);
int PS4_SYSV_ABI sceUserServiceSetKeyboardType(void);
int PS4_SYSV_ABI sceUserServiceSetKeyRepeatSpeed(void);
int PS4_SYSV_ABI sceUserServiceSetKeyRepeatStartingTime(void);
int PS4_SYSV_ABI sceUserServiceSetLightBarBaseBrightness(void);
int PS4_SYSV_ABI sceUserServiceSetLoginFlag(void);
int PS4_SYSV_ABI sceUserServiceSetMicLevel(void);
int PS4_SYSV_ABI sceUserServiceSetMouseHandType(void);
int PS4_SYSV_ABI sceUserServiceSetMousePointerSpeed(void);
int PS4_SYSV_ABI sceUserServiceSetNotificationBehavior(void);
int PS4_SYSV_ABI sceUserServiceSetNotificationSettings(void);
int PS4_SYSV_ABI sceUserServiceSetNpAccountUpgradeFlag(void);
int PS4_SYSV_ABI sceUserServiceSetNpAge(void);
int PS4_SYSV_ABI sceUserServiceSetNpAuthErrorFlag(void);
int PS4_SYSV_ABI sceUserServiceSetNpCountryCode(void);
int PS4_SYSV_ABI sceUserServiceSetNpDateOfBirth(void);
int PS4_SYSV_ABI sceUserServiceSetNpEnv(void);
int PS4_SYSV_ABI sceUserServiceSetNpLanguageCode(void);
int PS4_SYSV_ABI sceUserServiceSetNpLanguageCode2(void);
int PS4_SYSV_ABI sceUserServiceSetNpLoginId(void);
int PS4_SYSV_ABI sceUserServiceSetNpMAccountId(void);
int PS4_SYSV_ABI sceUserServiceSetNpNpId(void);
int PS4_SYSV_ABI sceUserServiceSetNpOfflineAccountAdult(void);
int PS4_SYSV_ABI sceUserServiceSetNpOnlineId(void);
int PS4_SYSV_ABI sceUserServiceSetNpSubAccount(void);
int PS4_SYSV_ABI sceUserServiceSetPadSpeakerVolume(void);
int PS4_SYSV_ABI sceUserServiceSetParentalBdAge(void);
int PS4_SYSV_ABI sceUserServiceSetParentalBrowser(void);
int PS4_SYSV_ABI sceUserServiceSetParentalDvd(void);
int PS4_SYSV_ABI sceUserServiceSetParentalDvdRegion(void);
int PS4_SYSV_ABI sceUserServiceSetParentalGame(void);
int PS4_SYSV_ABI sceUserServiceSetParentalGameAgeLevel(void);
int PS4_SYSV_ABI sceUserServiceSetParentalMorpheus(void);
int PS4_SYSV_ABI sceUserServiceSetPartyMuteList(void);
int PS4_SYSV_ABI sceUserServiceSetPartyMuteListA(void);
int PS4_SYSV_ABI sceUserServiceSetPartySettingFlags(void);
int PS4_SYSV_ABI sceUserServiceSetPasscode(void);
int PS4_SYSV_ABI sceUserServiceSetPbtcAdditionalTime(void);
int PS4_SYSV_ABI sceUserServiceSetPbtcFlag(void);
int PS4_SYSV_ABI sceUserServiceSetPbtcFridayDuration(void);
int PS4_SYSV_ABI sceUserServiceSetPbtcFridayHoursEnd(void);
int PS4_SYSV_ABI sceUserServiceSetPbtcFridayHoursStart(void);
int PS4_SYSV_ABI sceUserServiceSetPbtcMode(void);
int PS4_SYSV_ABI sceUserServiceSetPbtcMondayDuration(void);
int PS4_SYSV_ABI sceUserServiceSetPbtcMondayHoursEnd(void);
int PS4_SYSV_ABI sceUserServiceSetPbtcMondayHoursStart(void);
int PS4_SYSV_ABI sceUserServiceSetPbtcPlayTime(void);
int PS4_SYSV_ABI sceUserServiceSetPbtcPlayTimeLastUpdated(void);
int PS4_SYSV_ABI sceUserServiceSetPbtcSaturdayDuration(void);
int PS4_SYSV_ABI sceUserServiceSetPbtcSaturdayHoursEnd(void);
int PS4_SYSV_ABI sceUserServiceSetPbtcSaturdayHoursStart(void);
int PS4_SYSV_ABI sceUserServiceSetPbtcSundayDuration(void);
int PS4_SYSV_ABI sceUserServiceSetPbtcSundayHoursEnd(void);
int PS4_SYSV_ABI sceUserServiceSetPbtcSundayHoursStart(void);
int PS4_SYSV_ABI sceUserServiceSetPbtcThursdayDuration(void);
int PS4_SYSV_ABI sceUserServiceSetPbtcThursdayHoursEnd(void);
int PS4_SYSV_ABI sceUserServiceSetPbtcThursdayHoursStart(void);
int PS4_SYSV_ABI sceUserServiceSetPbtcTuesdayDuration(void);
int PS4_SYSV_ABI sceUserServiceSetPbtcTuesdayHoursEnd(void);
int PS4_SYSV_ABI sceUserServiceSetPbtcTuesdayHoursStart(void);
int PS4_SYSV_ABI sceUserServiceSetPbtcTzOffset(void);
int PS4_SYSV_ABI sceUserServiceSetPbtcWednesdayDuration(void);
int PS4_SYSV_ABI sceUserServiceSetPbtcWednesdayHoursEnd(void);
int PS4_SYSV_ABI sceUserServiceSetPbtcWednesdayHoursStart(void);
int PS4_SYSV_ABI sceUserServiceSetPlayTogetherFlags(void);
int PS4_SYSV_ABI sceUserServiceSetPsnPasswordForDebug(void);
int PS4_SYSV_ABI sceUserServiceSetSaveDataAutoUpload(void);
int PS4_SYSV_ABI sceUserServiceSetSaveDataSort(void);
int PS4_SYSV_ABI sceUserServiceSetSaveDataTutorialFlag(void);
int PS4_SYSV_ABI sceUserServiceSetShareButtonAssign(void);
int PS4_SYSV_ABI sceUserServiceSetShareDailymotionAccessToken(void);
int PS4_SYSV_ABI sceUserServiceSetShareDailymotionRefreshToken(void);
int PS4_SYSV_ABI sceUserServiceSetSharePlayFlags(void);
int PS4_SYSV_ABI sceUserServiceSetSharePlayFramerateHost(void);
int PS4_SYSV_ABI sceUserServiceSetSharePlayResolutionHost(void);
int PS4_SYSV_ABI sceUserServiceSetShareStatus(void);
int PS4_SYSV_ABI sceUserServiceSetShareStatus2(void);
int PS4_SYSV_ABI sceUserServiceSetSystemLoggerHashedAccountId(void);
int PS4_SYSV_ABI sceUserServiceSetSystemLoggerHashedAccountIdClockType(void);
int PS4_SYSV_ABI sceUserServiceSetSystemLoggerHashedAccountIdParam(void);
int PS4_SYSV_ABI sceUserServiceSetSystemLoggerHashedAccountIdTtl(void);
int PS4_SYSV_ABI sceUserServiceSetTeamShowAboutTeam(void);
int PS4_SYSV_ABI sceUserServiceSetThemeBgImageDimmer(void);
int PS4_SYSV_ABI sceUserServiceSetThemeBgImageWaveColor(void);
int PS4_SYSV_ABI sceUserServiceSetThemeBgImageZoom(void);
int PS4_SYSV_ABI sceUserServiceSetThemeEntitlementId(void);
int PS4_SYSV_ABI sceUserServiceSetThemeHomeShareOwner(void);
int PS4_SYSV_ABI sceUserServiceSetThemeTextShadow(void);
int PS4_SYSV_ABI sceUserServiceSetThemeWaveColor(void);
int PS4_SYSV_ABI sceUserServiceSetTopMenuLimitItem(void);
int PS4_SYSV_ABI sceUserServiceSetTopMenuNotificationFlag(void);
int PS4_SYSV_ABI sceUserServiceSetTopMenuTutorialFlag(void);
int PS4_SYSV_ABI sceUserServiceSetTraditionalChineseInputType(void);
int PS4_SYSV_ABI sceUserServiceSetUserGroupIndex(void);
int PS4_SYSV_ABI sceUserServiceSetUserGroupName(void);
int PS4_SYSV_ABI sceUserServiceSetUserName(void);
int PS4_SYSV_ABI sceUserServiceSetUserStatus(void);
int PS4_SYSV_ABI sceUserServiceSetVibrationEnabled(void);
int PS4_SYSV_ABI sceUserServiceSetVoiceRecognitionLastUsedOsk(void);
int PS4_SYSV_ABI sceUserServiceSetVoiceRecognitionTutorialState(void);
int PS4_SYSV_ABI sceUserServiceSetVolumeForController(void);
int PS4_SYSV_ABI sceUserServiceSetVolumeForGenericUSB(void);
int PS4_SYSV_ABI sceUserServiceSetVolumeForMorpheusSidetone(void);
int PS4_SYSV_ABI sceUserServiceSetVolumeForSidetone(void);
int PS4_SYSV_ABI sceUserServiceTerminate(void);
int PS4_SYSV_ABI sceUserServiceUnregisterEventCallback(void);
int PS4_SYSV_ABI UserService_Func_8AC6DC4168D5FEA5(void);
int PS4_SYSV_ABI UserService_Func_A6BDC9DFDAFD02B4(void);
int PS4_SYSV_ABI UserService_Func_BB9491DFE6B4953C(void);
int PS4_SYSV_ABI UserService_Func_D2B814603E7B4477(void);

#ifdef __cplusplus
}
#endif

#endif
