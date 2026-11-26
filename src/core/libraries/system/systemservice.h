/* SPDX-FileCopyrightText: Copyright 2024-2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

/* libSceSystemService and the libraries registered with it. Reference:
 * https://github.com/OpenOrbis/OpenOrbis-PS4-Toolchain/blob/master/include/orbis/_types/sys_service.h
 */

#ifndef SHADPS4_LIBRARIES_SYSTEMSERVICE_H
#define SHADPS4_LIBRARIES_SYSTEMSERVICE_H

#include <boolean.h>

#include "common/types.h"
#include "core/libraries/system/userservice.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum OrbisSystemServiceParamId {
    ORBIS_SYSTEM_SERVICE_PARAM_ID_LANG = 1,
    ORBIS_SYSTEM_SERVICE_PARAM_ID_DATE_FORMAT = 2,
    ORBIS_SYSTEM_SERVICE_PARAM_ID_TIME_FORMAT = 3,
    ORBIS_SYSTEM_SERVICE_PARAM_ID_TIME_ZONE = 4,
    ORBIS_SYSTEM_SERVICE_PARAM_ID_SUMMERTIME = 5,
    ORBIS_SYSTEM_SERVICE_PARAM_ID_SYSTEM_NAME = 6,
    ORBIS_SYSTEM_SERVICE_PARAM_ID_GAME_PARENTAL_LEVEL = 7,
    ORBIS_SYSTEM_SERVICE_PARAM_ID_ENTER_BUTTON_ASSIGN = 1000
} OrbisSystemServiceParamId;

typedef enum OrbisSystemParamDateFormat {
    ORBIS_SYSTEM_PARAM_DATE_FORMAT_YYYYMMDD = 0,
    ORBIS_SYSTEM_PARAM_DATE_FORMAT_DDMMYYYY = 1,
    ORBIS_SYSTEM_PARAM_DATE_FORMAT_MMDDYYYY = 2
} OrbisSystemParamDateFormat;

typedef enum OrbisSystemParamTimeFormat {
    ORBIS_SYSTEM_PARAM_TIME_FORMAT_12HOUR = 0,
    ORBIS_SYSTEM_PARAM_TIME_FORMAT_24HOUR = 1
} OrbisSystemParamTimeFormat;

/* Levels 1 to 11 are their numbers */
typedef enum OrbisSystemParamGameParentalLevel {
    ORBIS_SYSTEM_PARAM_GAME_PARENTAL_OFF = 0,
    ORBIS_SYSTEM_PARAM_GAME_PARENTAL_LEVEL11 = 11
} OrbisSystemParamGameParentalLevel;

typedef enum OrbisSystemParamEnterButtonAssign {
    ORBIS_SYSTEM_PARAM_ENTER_BUTTON_ASSIGN_CIRCLE = 0,
    ORBIS_SYSTEM_PARAM_ENTER_BUTTON_ASSIGN_CROSS = 1
} OrbisSystemParamEnterButtonAssign;

/* OrbisSystemServiceEvent.event_type */
#define ORBIS_SYSTEM_SERVICE_EVENT_INVALID (-1)
#define ORBIS_SYSTEM_SERVICE_EVENT_ON_RESUME 0x10000000
#define ORBIS_SYSTEM_SERVICE_EVENT_GAME_LIVE_STREAMING_STATUS_UPDATE 0x10000001
#define ORBIS_SYSTEM_SERVICE_EVENT_SESSION_INVITATION 0x10000002
#define ORBIS_SYSTEM_SERVICE_EVENT_ENTITLEMENT_UPDATE 0x10000003
#define ORBIS_SYSTEM_SERVICE_EVENT_GAME_CUSTOM_DATA 0x10000004
#define ORBIS_SYSTEM_SERVICE_EVENT_DISPLAY_SAFE_AREA_UPDATE 0x10000005
#define ORBIS_SYSTEM_SERVICE_EVENT_URL_OPEN 0x10000006
#define ORBIS_SYSTEM_SERVICE_EVENT_LAUNCH_APP 0x10000007
#define ORBIS_SYSTEM_SERVICE_EVENT_APP_LAUNCH_LINK 0x10000008
#define ORBIS_SYSTEM_SERVICE_EVENT_ADDCONTENT_INSTALL 0x10000009
#define ORBIS_SYSTEM_SERVICE_EVENT_RESET_VR_POSITION 0x1000000a
#define ORBIS_SYSTEM_SERVICE_EVENT_JOIN_EVENT 0x1000000b
#define ORBIS_SYSTEM_SERVICE_EVENT_PLAYGO_LOCUS_UPDATE 0x1000000c
#define ORBIS_SYSTEM_SERVICE_EVENT_PLAY_TOGETHER_HOST 0x1000000d
#define ORBIS_SYSTEM_SERVICE_EVENT_SERVICE_ENTITLEMENT_UPDATE 0x1000000e
#define ORBIS_SYSTEM_SERVICE_EVENT_EYE_TO_EYE_DISTANCE_UPDATE 0x1000000f
#define ORBIS_SYSTEM_SERVICE_EVENT_JOIN_MATCH_EVENT 0x10000010
#define ORBIS_SYSTEM_SERVICE_EVENT_PLAY_TOGETHER_HOST_A 0x10000011
#define ORBIS_SYSTEM_SERVICE_EVENT_WEB_BROWSER_CLOSED 0x10000012
#define ORBIS_SYSTEM_SERVICE_EVENT_CONTROLLER_SETTINGS_CLOSED 0x10000013
#define ORBIS_SYSTEM_SERVICE_EVENT_JOIN_TEAM_ON_TEAM_MATCH_EVENT 0x10000014
#define ORBIS_SYSTEM_SERVICE_EVENT_OPEN_SHARE_MENU 0x30000000

typedef struct OrbisSystemServiceStatus {
    s32 event_num;
    u8 is_system_ui_overlaid;
    u8 is_in_background_execution;
    u8 is_cpu_mode7_cpu_normal;
    u8 is_game_live_streaming_on_air;
    u8 is_out_of_vr_play_area;
    /* reserved bytes follow */
} OrbisSystemServiceStatus;

typedef struct OrbisSystemServiceDisplaySafeAreaInfo {
    float ratio;
    u8 reserved[128];
} OrbisSystemServiceDisplaySafeAreaInfo;

typedef struct OrbisSystemServiceHdrToneMapLuminance {
    float max_full_frame_tone_map_luminance;
    float max_tone_map_luminance;
    float min_tone_map_luminance;
} OrbisSystemServiceHdrToneMapLuminance;

/* The payload is @param, read by the event's type: a source and a URL (1024 and 4096
 * characters), a launch argument (a u32 size, then its bytes), a join event (an s32 user, an
 * event ID of 37 characters, a boot argument), an entitlement update (an s32 user, a u32 NP
 * service label), or a library's own structure (NP's session invitation). */
typedef struct OrbisSystemServiceEvent {
    s32 event_type; /* ORBIS_SYSTEM_SERVICE_EVENT_* */
    char param[8192];
} OrbisSystemServiceEvent;

/* A place in the queue for an event the game will receive, zeroed, to be filled where it is
 * and then posted with system_service_event_post: the game receives it once posted, in the
 * order of the claims. NULL when the queue is full. From any thread. */
OrbisSystemServiceEvent* system_service_event_claim(void);
void system_service_event_post(OrbisSystemServiceEvent* event);

/* Whether the splash screen is to be shown: the core option, until the game hides it */
bool system_service_is_splash_visible(void);

/* sceSystemServiceLoadExec's work: restarts the emulator into the game's executable @path
 * with @argv (NULL, or a list ended by NULL); the emulator exits if there is no such file.
 * Defined in emulator.cpp. */
void system_service_load_exec(const char* path, const char* const* argv);

int PS4_SYSV_ABI sceShellCoreUtilIsBgmCpuBudgetAcquired(void);
int PS4_SYSV_ABI sceShellCoreUtilIsBgmCpuBudgetAvailable(void);
int PS4_SYSV_ABI sceShellCoreUtilIsBgmPlaying(void);
s32 PS4_SYSV_ABI
sceSystemServiceGetDisplaySafeAreaInfo(OrbisSystemServiceDisplaySafeAreaInfo* info);
int PS4_SYSV_ABI
sceSystemServiceGetHdrToneMapLuminance(OrbisSystemServiceHdrToneMapLuminance* luminance);
s32 PS4_SYSV_ABI sceSystemServiceGetStatus(OrbisSystemServiceStatus* status);
s32 PS4_SYSV_ABI sceSystemServiceHideSplashScreen(void);
int PS4_SYSV_ABI sceSystemServiceIsBgmPlaying(void);
int PS4_SYSV_ABI sceSystemServiceLoadExec(const char* path, const char* argv[]);
s32 PS4_SYSV_ABI sceSystemServiceParamGetInt(s32 param_id, int* value);
s32 PS4_SYSV_ABI sceSystemServiceReceiveEvent(OrbisSystemServiceEvent* event);

/* Not implemented: they log and return ORBIS_OK */
int PS4_SYSV_ABI sceAppMessagingClearEventFlag(void);
int PS4_SYSV_ABI sceAppMessagingReceiveMsg(void);
int PS4_SYSV_ABI sceAppMessagingSendMsg(void);
int PS4_SYSV_ABI sceAppMessagingSendMsgToShellCore(void);
int PS4_SYSV_ABI sceAppMessagingSendMsgToShellUI(void);
int PS4_SYSV_ABI sceAppMessagingSetEventFlag(void);
int PS4_SYSV_ABI sceAppMessagingTryGetEventFlag(void);
int PS4_SYSV_ABI sceAppMessagingTryReceiveMsg(void);
int PS4_SYSV_ABI SystemService_Func_C8E899ABEF7F64C4(void);
int PS4_SYSV_ABI SystemService_Func_F74BA759B9C8D2A1(void);
int PS4_SYSV_ABI sceLncUtilAcquireCpuBudgetOfExtraAudioDevices(void);
int PS4_SYSV_ABI sceLncUtilAcquireCpuBudgetOfImeForBigApp(void);
int PS4_SYSV_ABI sceLncUtilAcquireCpuBudgetOfInGameStore(void);
int PS4_SYSV_ABI sceLncUtilActivateCdlg(void);
int PS4_SYSV_ABI sceLncUtilAddLocalProcess(void);
int PS4_SYSV_ABI sceLncUtilBlockAppSuspend(void);
int PS4_SYSV_ABI sceLncUtilBlockingGetEventForDaemon(void);
int PS4_SYSV_ABI sceLncUtilContinueApp(void);
int PS4_SYSV_ABI sceLncUtilCrashSyscore(void);
int PS4_SYSV_ABI sceLncUtilDeactivateCdlg(void);
int PS4_SYSV_ABI sceLncUtilDeclareReadyForSuspend(void);
int PS4_SYSV_ABI sceLncUtilDisableSuspendNotification(void);
int PS4_SYSV_ABI sceLncUtilEnableSuspendNotification(void);
int PS4_SYSV_ABI sceLncUtilFinishSpecialResume(void);
int PS4_SYSV_ABI sceLncUtilForceKillApp(void);
int PS4_SYSV_ABI sceLncUtilForceKillLocalProcess(void);
int PS4_SYSV_ABI sceLncUtilGetApp0DirPath(void);
int PS4_SYSV_ABI sceLncUtilGetAppCategory(void);
int PS4_SYSV_ABI sceLncUtilGetAppFocusedAppStatus(void);
int PS4_SYSV_ABI sceLncUtilGetAppId(void);
int PS4_SYSV_ABI sceLncUtilGetAppIdOfBigApp(void);
int PS4_SYSV_ABI sceLncUtilGetAppIdOfMiniApp(void);
int PS4_SYSV_ABI sceLncUtilGetAppLaunchedUser(void);
int PS4_SYSV_ABI sceLncUtilGetAppStatus(void);
int PS4_SYSV_ABI sceLncUtilGetAppStatusListForShellUIReboot(void);
int PS4_SYSV_ABI sceLncUtilGetAppTitleId(void);
int PS4_SYSV_ABI sceLncUtilGetAppType(void);
int PS4_SYSV_ABI sceLncUtilGetCdlgExec(void);
int PS4_SYSV_ABI sceLncUtilGetCoredumpState(void);
int PS4_SYSV_ABI sceLncUtilGetDbgExecutablePath(void);
int PS4_SYSV_ABI sceLncUtilGetEventForDaemon(void);
int PS4_SYSV_ABI sceLncUtilGetEventForShellUI(void);
int PS4_SYSV_ABI sceLncUtilGetGpuCrashFullDumpAppStatus(void);
int PS4_SYSV_ABI sceLncUtilGetLocalProcessStatusList(void);
int PS4_SYSV_ABI sceLncUtilGetParentSocket(void);
int PS4_SYSV_ABI sceLncUtilGetResultKillApp(void);
int PS4_SYSV_ABI sceLncUtilGetResultLaunchAppByTitleId(void);
int PS4_SYSV_ABI sceLncUtilInitialize(void);
int PS4_SYSV_ABI sceLncUtilIsActiveCdlg(void);
int PS4_SYSV_ABI sceLncUtilIsAppLaunched(void);
int PS4_SYSV_ABI sceLncUtilIsAppSuspended(void);
int PS4_SYSV_ABI sceLncUtilIsCpuBudgetOfExtraAudioDevicesAvailable(void);
int PS4_SYSV_ABI sceLncUtilIsPs2Emu(void);
int PS4_SYSV_ABI sceLncUtilIsShellUiFgAndGameBgCpuMode(void);
int PS4_SYSV_ABI sceLncUtilKickCoredumpOnlyProcMem(void);
int PS4_SYSV_ABI sceLncUtilKillApp(void);
int PS4_SYSV_ABI sceLncUtilKillAppWithReason(void);
int PS4_SYSV_ABI sceLncUtilKillLocalProcess(void);
int PS4_SYSV_ABI sceLncUtilLaunchApp(void);
int PS4_SYSV_ABI sceLncUtilLoadExec(void);
int PS4_SYSV_ABI sceLncUtilNotifyCoredumpRequestEnd(void);
int PS4_SYSV_ABI sceLncUtilNotifyCoredumpRequestProgress(void);
int PS4_SYSV_ABI sceLncUtilNotifyVshReady(void);
int PS4_SYSV_ABI sceLncUtilRaiseException(void);
int PS4_SYSV_ABI sceLncUtilRaiseExceptionLocalProcess(void);
int PS4_SYSV_ABI sceLncUtilRegisterCdlgSharedMemoryName(void);
int PS4_SYSV_ABI sceLncUtilRegisterDaemon(void);
int PS4_SYSV_ABI sceLncUtilRegisterShellUI(void);
int PS4_SYSV_ABI sceLncUtilReleaseCpuBudgetOfExtraAudioDevices(void);
int PS4_SYSV_ABI sceLncUtilReleaseCpuBudgetOfImeForBigApp(void);
int PS4_SYSV_ABI sceLncUtilReleaseCpuBudgetOfInGameStore(void);
int PS4_SYSV_ABI sceLncUtilResumeApp(void);
int PS4_SYSV_ABI sceLncUtilResumeLocalProcess(void);
int PS4_SYSV_ABI sceLncUtilSetAppFocus(void);
int PS4_SYSV_ABI sceLncUtilSetCdlgExec(void);
int PS4_SYSV_ABI sceLncUtilSetControllerFocus(void);
int PS4_SYSV_ABI sceLncUtilSetControllerFocusPermission(void);
int PS4_SYSV_ABI sceLncUtilStartKillApp(void);
int PS4_SYSV_ABI sceLncUtilStartLaunchAppByTitleId(void);
int PS4_SYSV_ABI sceLncUtilSuspendApp(void);
int PS4_SYSV_ABI sceLncUtilSuspendBackgroundApp(void);
int PS4_SYSV_ABI sceLncUtilSuspendLocalProcess(void);
int PS4_SYSV_ABI sceLncUtilSystemSuspend(void);
int PS4_SYSV_ABI sceLncUtilTerminate(void);
int PS4_SYSV_ABI sceLncUtilTryBlockAppSuspend(void);
int PS4_SYSV_ABI sceLncUtilUnblockAppSuspend(void);
int PS4_SYSV_ABI sceLncUtilUnregisterCdlgSharedMemoryName(void);
int PS4_SYSV_ABI sceLncUtilUnregisterDaemon(void);
int PS4_SYSV_ABI sceLncUtilUnregisterShellUI(void);
int PS4_SYSV_ABI sceSystemServiceActivateHevcSoft(void);
int PS4_SYSV_ABI sceSystemServiceActivateHevcSoftAbort(void);
int PS4_SYSV_ABI sceSystemServiceActivateHevcSoftGetStatus(void);
int PS4_SYSV_ABI sceSystemServiceActivateHevcSoftInit(void);
int PS4_SYSV_ABI sceSystemServiceActivateHevcSoftIsActivated(void);
int PS4_SYSV_ABI sceSystemServiceActivateHevcSoftStart(void);
int PS4_SYSV_ABI sceSystemServiceActivateHevcSoftTerm(void);
int PS4_SYSV_ABI sceShellCoreUtilAccessibilityZoomLock(void);
int PS4_SYSV_ABI sceShellCoreUtilAccessibilityZoomUnlock(void);
int PS4_SYSV_ABI sceShellCoreUtilAcquireBgmCpuBudget(void);
int PS4_SYSV_ABI sceShellCoreUtilAcquireRemotePlayCpuBudget(void);
int PS4_SYSV_ABI sceShellCoreUtilAcquireSharePlayCpuBudget(void);
int PS4_SYSV_ABI sceShellCoreUtilActivateAbort(void);
int PS4_SYSV_ABI sceShellCoreUtilActivateGetStatus(void);
int PS4_SYSV_ABI sceShellCoreUtilActivateInit(void);
int PS4_SYSV_ABI sceShellCoreUtilActivateIsActivated(void);
int PS4_SYSV_ABI sceShellCoreUtilActivateRecordActivation(void);
int PS4_SYSV_ABI sceShellCoreUtilActivateStart(void);
int PS4_SYSV_ABI sceShellCoreUtilActivateStartAsync(void);
int PS4_SYSV_ABI sceShellCoreUtilActivateTerm(void);
int PS4_SYSV_ABI sceShellCoreUtilChangeRunLevel(void);
int PS4_SYSV_ABI sceShellCoreUtilChangeToStaffModeForIDU(void);
int PS4_SYSV_ABI sceShellCoreUtilCheckerAbort(void);
int PS4_SYSV_ABI sceShellCoreUtilCleanupCrashReport(void);
int PS4_SYSV_ABI sceShellCoreUtilClearAppData(void);
int PS4_SYSV_ABI sceShellCoreUtilClearPsnAccountInfo(void);
int PS4_SYSV_ABI sceShellCoreUtilCrashReportRequestCancel(void);
int PS4_SYSV_ABI sceShellCoreUtilDeclareBeginOfExternalStorageAppMove(void);
int PS4_SYSV_ABI sceShellCoreUtilDeclareEndOfExternalStorageAppMove(void);
int PS4_SYSV_ABI sceShellCoreUtilDeleteDiscInstalledTitleWorkaroundFile(void);
int PS4_SYSV_ABI sceShellCoreUtilDeleteDownloadedHidConfigFile(void);
int PS4_SYSV_ABI sceShellCoreUtilDeleteDownloadedNetEvConfigFile(void);
int PS4_SYSV_ABI sceShellCoreUtilDeleteDownloadedTitleWorkaroundFile(void);
int PS4_SYSV_ABI sceShellCoreUtilDeleteSmrHddDummyData(void);
int PS4_SYSV_ABI sceShellCoreUtilDoFsck(void);
int PS4_SYSV_ABI sceShellCoreUtilDownloadHidConfigFileFromServer(void);
int PS4_SYSV_ABI sceShellCoreUtilDownloadNetEvConfigFileFromServer(void);
int PS4_SYSV_ABI sceShellCoreUtilDownloadTitleWorkaroundFileFromServer(void);
int PS4_SYSV_ABI sceShellCoreUtilEnterPowerLockSection(void);
int PS4_SYSV_ABI sceShellCoreUtilExecuteCrashReport(void);
int PS4_SYSV_ABI sceShellCoreUtilExfatFormatExternalHdd(void);
int PS4_SYSV_ABI sceShellCoreUtilExitMiniApp(void);
int PS4_SYSV_ABI sceShellCoreUtilExitMiniAppWithValue(void);
int PS4_SYSV_ABI sceShellCoreUtilFillUpSpaceOnSmrHdd(void);
int PS4_SYSV_ABI sceShellCoreUtilFormatExternalHdd(void);
int PS4_SYSV_ABI sceShellCoreUtilFormatHddForRestore(void);
int PS4_SYSV_ABI sceShellCoreUtilFreeUpSpaceOnSmrHdd(void);
int PS4_SYSV_ABI sceShellCoreUtilGetAppData(void);
int PS4_SYSV_ABI sceShellCoreUtilGetAppEnableTTS(void);
int PS4_SYSV_ABI sceShellCoreUtilGetAppEnterButtonAssign(void);
int PS4_SYSV_ABI sceShellCoreUtilGetAppLaunchedParamInt(void);
int PS4_SYSV_ABI sceShellCoreUtilGetAppLaunchedParamIntByBudgetType(void);
int PS4_SYSV_ABI sceShellCoreUtilGetAppLaunchedParamString(void);
int PS4_SYSV_ABI sceShellCoreUtilGetAppLaunchedParamStringByBudgetType(void);
int PS4_SYSV_ABI sceShellCoreUtilGetAppLaunchTypeInfo(void);
int PS4_SYSV_ABI sceShellCoreUtilGetAutoPowerDownRemainingSeconds(void);
int PS4_SYSV_ABI sceShellCoreUtilGetBasicProductShape(void);
int PS4_SYSV_ABI sceShellCoreUtilGetCheckerString(void);
int PS4_SYSV_ABI sceShellCoreUtilGetCheckerStringEx(void);
int PS4_SYSV_ABI sceShellCoreUtilGetCloudClientStatus(void);
int PS4_SYSV_ABI sceShellCoreUtilGetCrashReportCoreFileSetSize(void);
int PS4_SYSV_ABI sceShellCoreUtilGetCrashReportFilterInfoStart(void);
int PS4_SYSV_ABI sceShellCoreUtilGetCrashReportInfoForBoot(void);
int PS4_SYSV_ABI sceShellCoreUtilGetCrashReportInfoForBootStart(void);
int PS4_SYSV_ABI sceShellCoreUtilGetCrashReportInfoStart(void);
int PS4_SYSV_ABI sceShellCoreutilGetCrashReportProcessInformation(void);
int PS4_SYSV_ABI sceShellCoreUtilGetCrashReportResult(void);
int PS4_SYSV_ABI sceShellCoreUtilGetCrashReportStatus(void);
int PS4_SYSV_ABI sceShellCoreUtilGetCrashReportUploadStatus(void);
int PS4_SYSV_ABI sceShellCoreUtilGetDeviceIndexBehavior(void);
int PS4_SYSV_ABI sceShellCoreUtilGetDeviceIndexBehaviorWithTimeout(void);
int PS4_SYSV_ABI sceShellCoreUtilGetEffectiveTotalSizeOfUserPartition(void);
int PS4_SYSV_ABI sceShellCoreUtilGetFreeSizeOfAvContentsTmp(void);
int PS4_SYSV_ABI sceShellCoreUtilGetFreeSizeOfUserPartition(void);
int PS4_SYSV_ABI sceShellCoreUtilGetFsckProgress(void);
int PS4_SYSV_ABI sceShellCoreUtilGetGameLiveStreamingStatus(void);
int PS4_SYSV_ABI sceShellCoreUtilGetGnmCompositorOnScreenProfilerFlag(void);
int PS4_SYSV_ABI sceShellCoreUtilGetGpuLoadEmulationMode(void);
int PS4_SYSV_ABI sceShellCoreUtilGetGpuLoadEmulationModeByAppId(void);
int PS4_SYSV_ABI sceShellCoreUtilGetHidConfigFileInfoString(void);
int PS4_SYSV_ABI sceShellCoreUtilGetHidConfigFileString(void);
int PS4_SYSV_ABI sceShellCoreUtilGetHidConfigName(void);
int PS4_SYSV_ABI sceShellCoreUtilGetHidConfigNum(void);
int PS4_SYSV_ABI sceShellCoreUtilGetIDUMode(void);
int PS4_SYSV_ABI sceShellCoreUtilGetImposeMenuFlagForPs2Emu(void);
int PS4_SYSV_ABI sceShellCoreUtilGetManifestFileStatus(void);
int PS4_SYSV_ABI sceShellCoreUtilGetNeedSizeOfAppContent(void);
int PS4_SYSV_ABI sceShellCoreUtilGetNetEvConfigFileInfoString(void);
int PS4_SYSV_ABI sceShellCoreUtilGetOptimizationStatus(void);
int PS4_SYSV_ABI sceShellCoreUtilGetOutOfVrPlayZoneWarning(void);
int PS4_SYSV_ABI sceShellCoreUtilGetPapcGamePcl(void);
int PS4_SYSV_ABI sceShellCoreUtilGetPbtcUserInfoList(void);
int PS4_SYSV_ABI sceShellCoreUtilGetPlatformPrivacyDefinitionEventData(void);
int PS4_SYSV_ABI sceShellCoreUtilGetPlatformPrivacySetting(void);
int PS4_SYSV_ABI sceShellCoreUtilGetProgressOfFormatExternalHdd(void);
int PS4_SYSV_ABI sceShellCoreUtilGetProgressOfFsck(void);
int PS4_SYSV_ABI sceShellCoreUtilGetPsnAccountInfo(void);
int PS4_SYSV_ABI sceShellCoreUtilGetPsStoreIconLayout(void);
int PS4_SYSV_ABI sceShellCoreUtilGetPsStoreIconState(void);
int PS4_SYSV_ABI sceShellCoreUtilGetRegion(void);
int PS4_SYSV_ABI sceShellCoreUtilGetRemotePlayStatus(void);
int PS4_SYSV_ABI sceShellCoreUtilGetRunLevel(void);
int PS4_SYSV_ABI sceShellCoreUtilGetSharePlayStatus(void);
int PS4_SYSV_ABI sceShellCoreUtilGetShellUIVMStats(void);
int PS4_SYSV_ABI sceShellCoreUtilGetSmrHddInfoString(void);
int PS4_SYSV_ABI sceShellCoreUtilGetSocialScreenStatus(void);
int PS4_SYSV_ABI sceShellCoreUtilGetSplashScreenState(void);
int PS4_SYSV_ABI sceShellCoreUtilGetSupportSiteURL(void);
int PS4_SYSV_ABI sceShellCoreUtilGetSuspendConfirmationDialogFlag(void);
int PS4_SYSV_ABI sceShellCoreUtilGetSystemBGState(void);
int PS4_SYSV_ABI sceShellCoreUtilGetSystemBGWaveColor(void);
int PS4_SYSV_ABI sceShellCoreUtilGetSystemBGWaveState(void);
int PS4_SYSV_ABI sceShellCoreUtilGetTitleWorkaroundFileInfoString(void);
int PS4_SYSV_ABI sceShellCoreUtilGetTitleWorkaroundFileString(void);
int PS4_SYSV_ABI sceShellCoreUtilGetUIStatus(void);
int PS4_SYSV_ABI sceShellCoreUtilGetUserFocus(void);
int PS4_SYSV_ABI sceShellCoreUtilGetUserIdOfMorpheusUser(void);
int PS4_SYSV_ABI sceShellCoreUtilGetVersionNumberOfCameraCalibrationData(void);
int PS4_SYSV_ABI sceShellCoreUtilGoBackToKratosCurrentSessionGame(void);
int PS4_SYSV_ABI sceShellCoreUtilHideBlocksFromUser(void);
int PS4_SYSV_ABI sceShellCoreUtilIncrementVersionNumberOfCameraCalibrationData(void);
int PS4_SYSV_ABI sceShellCoreUtilIsAccessibilityZoomLocked(void);
int PS4_SYSV_ABI sceShellCoreUtilIsExternalStorageAppMoveInProgress(void);
int PS4_SYSV_ABI sceShellCoreUtilIsEyeToEyeDistanceAdjusted(void);
int PS4_SYSV_ABI sceShellCoreUtilIsGameLiveStreamingOnAir(void);
int PS4_SYSV_ABI sceShellCoreUtilIsImposeScreenOverlaid(void);
int PS4_SYSV_ABI sceShellCoreUtilIsInSystemSuspendBlackList(void);
int PS4_SYSV_ABI sceShellCoreUtilIsInternalKratosUser(void);
int PS4_SYSV_ABI sceShellCoreUtilIsKilledOrSuspendedByLogout(void);
int PS4_SYSV_ABI sceShellCoreUtilIsNeededCrashReport(void);
int PS4_SYSV_ABI sceShellCoreUtilIsPowerSaveAlertRequested(void);
int PS4_SYSV_ABI sceShellCoreUtilIsRemotePlayCpuBudgetAcquired(void);
int PS4_SYSV_ABI sceShellCoreUtilIsScreenSaverOn(void);
int PS4_SYSV_ABI sceShellCoreUtilIsSharePlayCpuBudgetAcquired(void);
int PS4_SYSV_ABI sceShellCoreUtilIsShowCrashReport(void);
int PS4_SYSV_ABI sceShellCoreUtilIsTemperatureDanger(void);
int PS4_SYSV_ABI sceShellCoreUtilIsTitleWorkaroundEnabled(void);
int PS4_SYSV_ABI sceShellCoreUtilIsUsbMassStorageMounted(void);
int PS4_SYSV_ABI sceShellCoreUtilLaunchByUri(void);
int PS4_SYSV_ABI sceShellCoreUtilLeavePowerLockSection(void);
int PS4_SYSV_ABI sceShellCoreUtilLog(void);
int PS4_SYSV_ABI sceShellCoreUtilMakeManifestFile(void);
int PS4_SYSV_ABI sceShellCoreUtilMountAppRight(void);
int PS4_SYSV_ABI sceShellCoreUtilMountDownloadDataForShellUI(void);
int PS4_SYSV_ABI sceShellCoreUtilMountHddForBackup(void);
int PS4_SYSV_ABI sceShellCoreUtilMountHddForRestore(void);
int PS4_SYSV_ABI sceShellCoreUtilNavigateToAnotherApp(void);
int PS4_SYSV_ABI sceShellCoreUtilNavigateToGoHome(void);
int PS4_SYSV_ABI sceShellCoreUtilNavigateToLaunchedApp(void);
int PS4_SYSV_ABI sceShellCoreUtilNotificationCancelForIDU(void);
int PS4_SYSV_ABI sceShellCoreUtilNotificationRequestedForIDU(void);
int PS4_SYSV_ABI sceShellCoreUtilNotifyBgmCoreTermination(void);
int PS4_SYSV_ABI sceShellCoreUtilNotifyFarsightUIDone(void);
int PS4_SYSV_ABI sceShellCoreUtilNotifyFsReadError(void);
int PS4_SYSV_ABI sceShellCoreUtilNotifyPsnAccountInfoReceived(void);
int PS4_SYSV_ABI sceShellCoreUtilNotifyYouTubeAccountLinkStatusChanged(void);
int PS4_SYSV_ABI sceShellCoreUtilPfAuthClientConsoleTokenClearCache(void);
int PS4_SYSV_ABI sceShellCoreUtilPostActivityForPsNow(void);
int PS4_SYSV_ABI sceShellCoreUtilPostErrorLog(void);
int PS4_SYSV_ABI sceShellCoreUtilPostLaunchConfirmResult(void);
int PS4_SYSV_ABI sceShellCoreUtilPostPsmEventToShellUI(void);
int PS4_SYSV_ABI sceShellCoreUtilPreNotifyOfGameLiveStreaming(void);
int PS4_SYSV_ABI sceShellCoreUtilPreNotifyOfRemotePlay(void);
int PS4_SYSV_ABI sceShellCoreUtilPreNotifyOfSharePlay(void);
int PS4_SYSV_ABI sceShellCoreUtilReleaseBgmCpuBudget(void);
int PS4_SYSV_ABI sceShellCoreUtilReleaseRemotePlayCpuBudget(void);
int PS4_SYSV_ABI sceShellCoreUtilReleaseSharePlayCpuBudget(void);
int PS4_SYSV_ABI sceShellCoreUtilReportSessionErrorToGaikaiController(void);
int PS4_SYSV_ABI sceShellCoreUtilReportUnexpectedFatalErrorToSystemTelemetry(void);
int PS4_SYSV_ABI sceShellCoreUtilRequestCameraCalibration(void);
int PS4_SYSV_ABI sceShellCoreUtilRequestEjectDevice(void);
int PS4_SYSV_ABI sceShellCoreUtilRequestRebootApp(void);
int PS4_SYSV_ABI sceShellCoreUtilRequestShutdown(void);
int PS4_SYSV_ABI sceShellCoreUtilResetAutoPowerDownTimer(void);
int PS4_SYSV_ABI sceShellCoreUtilResetBgdcConfig(void);
int PS4_SYSV_ABI sceShellCoreUtilSetAppData(void);
int PS4_SYSV_ABI sceShellCoreUtilSetBgmProhibition(void);
int PS4_SYSV_ABI sceShellCoreUtilSetDeviceIndexBehavior(void);
int PS4_SYSV_ABI sceShellCoreUtilSetGameLiveStreamingOnAirFlag(void);
int PS4_SYSV_ABI sceShellCoreUtilSetGameLiveStreamingStatus(void);
int PS4_SYSV_ABI sceShellCoreUtilSetGnmCompositorOnScreenProfilerFlag(void);
int PS4_SYSV_ABI sceShellCoreUtilSetGpuLoadEmulationMode(void);
int PS4_SYSV_ABI sceShellCoreUtilSetGpuLoadEmulationModeByAppId(void);
int PS4_SYSV_ABI sceShellCoreUtilSetIDUMode(void);
int PS4_SYSV_ABI sceShellCoreUtilSetImposeStatusFlag(void);
int PS4_SYSV_ABI sceShellCoreUtilSetPsStoreIconLayout(void);
int PS4_SYSV_ABI sceShellCoreUtilSetPsStoreIconState(void);
int PS4_SYSV_ABI sceShellCoreUtilSetRemotePlayStatus(void);
int PS4_SYSV_ABI sceShellCoreUtilSetSharePlayStatus(void);
int PS4_SYSV_ABI sceShellCoreUtilSetSkipUpdateCheck(void);
int PS4_SYSV_ABI sceShellCoreUtilSetSocialScreenStatus(void);
int PS4_SYSV_ABI sceShellCoreUtilSetSplashScreenState(void);
int PS4_SYSV_ABI sceShellCoreUtilSetSystemBGState(void);
int PS4_SYSV_ABI sceShellCoreUtilSetSystemBGWaveColor(void);
int PS4_SYSV_ABI sceShellCoreUtilSetSystemBGWaveState(void);
int PS4_SYSV_ABI sceShellCoreUtilSetUIStatus(void);
int PS4_SYSV_ABI sceShellCoreUtilSetUserFocus(void);
int PS4_SYSV_ABI sceShellCoreUtilShowCriticalErrorDialog(void);
int PS4_SYSV_ABI sceShellCoreUtilShowErrorDialog(void);
int PS4_SYSV_ABI sceShellCoreUtilShowErrorDialogWithFormatArgs(void);
int PS4_SYSV_ABI sceShellCoreUtilShowErrorDialogWithParam(void);
int PS4_SYSV_ABI sceShellCoreUtilShowPsUnderLockIndicator(void);
int PS4_SYSV_ABI sceShellCoreUtilSignalUserInput(void);
int PS4_SYSV_ABI sceShellCoreUtilStartOptimization(void);
int PS4_SYSV_ABI sceShellCoreUtilStartPsNowGame(void);
int PS4_SYSV_ABI sceShellCoreUtilStopOptimization(void);
int PS4_SYSV_ABI sceShellCoreUtilStopPsNowGame(void);
int PS4_SYSV_ABI sceShellCoreUtilTestBusTransferSpeed(void);
int PS4_SYSV_ABI sceShellCoreUtilTickHeartBeat(void);
int PS4_SYSV_ABI sceShellCoreUtilTriggerPapcRecalculation(void);
int PS4_SYSV_ABI sceShellCoreUtilTriggerPapcUpdate(void);
int PS4_SYSV_ABI sceShellCoreUtilTurnOffScreenSaver(void);
int PS4_SYSV_ABI sceShellCoreUtilUnmountAppRight(void);
int PS4_SYSV_ABI sceShellCoreUtilUnmountDownloadDataForShellUI(void);
int PS4_SYSV_ABI sceShellCoreUtilUnmountHddForBackup(void);
int PS4_SYSV_ABI sceShellCoreUtilUnmountHddForRestore(void);
int PS4_SYSV_ABI sceShellCoreUtilWriteSmrHddDummyData(void);
int PS4_SYSV_ABI SystemService_Func_1E5CA5A71FA7F028(void);
int PS4_SYSV_ABI SystemService_Func_6D43644F75C38346(void);
int PS4_SYSV_ABI SystemService_Func_739FB849CB28F445(void);
int PS4_SYSV_ABI SystemService_Func_B20628FF35C74111(void);
int PS4_SYSV_ABI sceSystemServiceAddLocalProcessForJvm(void);
int PS4_SYSV_ABI sceSystemServiceGetParentSocketForJvm(void);
int PS4_SYSV_ABI sceSystemServiceKillLocalProcessForJvm(void);
int PS4_SYSV_ABI sceSystemServiceAcquireFb0(void);
int PS4_SYSV_ABI sceSystemServiceAddLocalProcess(void);
int PS4_SYSV_ABI sceSystemServiceAddLocalProcessForPsmKit(void);
int PS4_SYSV_ABI sceSystemServiceChangeAcpClock(void);
int PS4_SYSV_ABI sceSystemServiceChangeCpuClock(void);
int PS4_SYSV_ABI sceSystemServiceChangeGpuClock(void);
int PS4_SYSV_ABI sceSystemServiceChangeMemoryClock(void);
int PS4_SYSV_ABI sceSystemServiceChangeMemoryClockToBaseMode(void);
int PS4_SYSV_ABI sceSystemServiceChangeMemoryClockToDefault(void);
int PS4_SYSV_ABI sceSystemServiceChangeMemoryClockToMultiMediaMode(void);
int PS4_SYSV_ABI sceSystemServiceChangeNumberOfGpuCu(void);
int PS4_SYSV_ABI sceSystemServiceChangeSamuClock(void);
int PS4_SYSV_ABI sceSystemServiceChangeUvdClock(void);
int PS4_SYSV_ABI sceSystemServiceChangeVceClock(void);
int PS4_SYSV_ABI sceSystemServiceDisableMusicPlayer(void);
int PS4_SYSV_ABI sceSystemServiceDisablePersonalEyeToEyeDistanceSetting(void);
int PS4_SYSV_ABI sceSystemServiceDisableSuspendConfirmationDialog(void);
int PS4_SYSV_ABI sceSystemServiceEnablePersonalEyeToEyeDistanceSetting(void);
int PS4_SYSV_ABI sceSystemServiceEnableSuspendConfirmationDialog(void);
int PS4_SYSV_ABI sceSystemServiceGetAppFocusedAppStatus(void);
int PS4_SYSV_ABI sceSystemServiceGetAppIdOfBigApp(void);
int PS4_SYSV_ABI sceSystemServiceGetAppIdOfMiniApp(void);
int PS4_SYSV_ABI sceSystemServiceGetAppStatus(void);
int PS4_SYSV_ABI sceSystemServiceGetAppType(void);
int PS4_SYSV_ABI sceSystemServiceGetEventForDaemon(void);
int PS4_SYSV_ABI sceSystemServiceGetGpuLoadEmulationMode(void);
int PS4_SYSV_ABI sceSystemServiceGetLocalProcessStatusList(void);
int PS4_SYSV_ABI sceSystemServiceGetParentSocket(void);
int PS4_SYSV_ABI sceSystemServiceGetParentSocketForPsmKit(void);
int PS4_SYSV_ABI sceSystemServiceGetPSButtonEvent(void);
int PS4_SYSV_ABI sceSystemServiceGetRenderingMode(void);
int PS4_SYSV_ABI sceSystemServiceGetTitleWorkaroundInfo(void);
int PS4_SYSV_ABI sceSystemServiceGetVersionNumberOfCameraCalibrationData(void);
int PS4_SYSV_ABI sceSystemServiceIsAppSuspended(void);
int PS4_SYSV_ABI sceSystemServiceIsEyeToEyeDistanceAdjusted(void);
int PS4_SYSV_ABI sceSystemServiceIsScreenSaverOn(void);
int PS4_SYSV_ABI sceSystemServiceIsShellUiFgAndGameBgCpuMode(void);
int PS4_SYSV_ABI sceSystemServiceKillApp(void);
int PS4_SYSV_ABI sceSystemServiceKillLocalProcess(void);
int PS4_SYSV_ABI sceSystemServiceKillLocalProcessForPsmKit(void);
int PS4_SYSV_ABI sceSystemServiceLaunchApp(void);
int PS4_SYSV_ABI sceSystemServiceLaunchEventDetails(void);
int PS4_SYSV_ABI sceSystemServiceLaunchTournamentList(void);
int PS4_SYSV_ABI sceSystemServiceLaunchTournamentsTeamProfile(void);
int PS4_SYSV_ABI sceSystemServiceLaunchWebBrowser(void);
int PS4_SYSV_ABI sceSystemServiceNavigateToAnotherApp(void);
int PS4_SYSV_ABI sceSystemServiceNavigateToGoBack(void);
int PS4_SYSV_ABI sceSystemServiceNavigateToGoBackWithValue(void);
int PS4_SYSV_ABI sceSystemServiceNavigateToGoHome(void);
int PS4_SYSV_ABI sceSystemServiceParamGetString(void);
int PS4_SYSV_ABI sceSystemServicePowerTick(void);
int PS4_SYSV_ABI sceSystemServiceRaiseExceptionLocalProcess(void);
int PS4_SYSV_ABI sceSystemServiceReenableMusicPlayer(void);
int PS4_SYSV_ABI sceSystemServiceRegisterDaemon(void);
int PS4_SYSV_ABI sceSystemServiceReleaseFb0(void);
int PS4_SYSV_ABI sceSystemServiceReportAbnormalTermination(void);
int PS4_SYSV_ABI sceSystemServiceRequestCameraCalibration(void);
int PS4_SYSV_ABI sceSystemServiceRequestToChangeRenderingMode(void);
int PS4_SYSV_ABI sceSystemServiceResumeLocalProcess(void);
int PS4_SYSV_ABI sceSystemServiceSetControllerFocusPermission(void);
int PS4_SYSV_ABI sceSystemServiceSetGpuLoadEmulationMode(void);
int PS4_SYSV_ABI sceSystemServiceSetOutOfVrPlayAreaFlag(void);
int PS4_SYSV_ABI sceSystemServiceSetOutOfVrPlayZoneWarning(void);
int PS4_SYSV_ABI sceSystemServiceShowControllerSettings(void);
int PS4_SYSV_ABI sceSystemServiceShowDisplaySafeAreaSettings(void);
int PS4_SYSV_ABI sceSystemServiceShowEyeToEyeDistanceSetting(void);
int PS4_SYSV_ABI sceSystemServiceSuspendBackgroundApp(void);
int PS4_SYSV_ABI sceSystemServiceSuspendLocalProcess(void);
int PS4_SYSV_ABI sceSystemServiceTickVideoPlayback(void);
int PS4_SYSV_ABI sceSystemServiceTurnOffScreenSaver(void);
int PS4_SYSV_ABI SystemService_Func_9031A344CB540F1A(void);
int PS4_SYSV_ABI SystemService_Func_A9D4CF2568EAB837(void);
int PS4_SYSV_ABI sceSystemServiceLaunchWebApp(void);
int PS4_SYSV_ABI SystemService_Func_B8495C766861FDCF(void);
int PS4_SYSV_ABI sceSystemServiceGetDbgExecutablePath(void);
int PS4_SYSV_ABI sceSystemServiceActivateHevc(void);
int PS4_SYSV_ABI sceSystemServiceActivateHevcAbort(void);
int PS4_SYSV_ABI sceSystemServiceActivateHevcGetStatus(void);
int PS4_SYSV_ABI sceSystemServiceActivateHevcInit(void);
int PS4_SYSV_ABI sceSystemServiceActivateHevcIsActivated(void);
int PS4_SYSV_ABI sceSystemServiceActivateHevcStart(void);
int PS4_SYSV_ABI sceSystemServiceActivateHevcTerm(void);
int PS4_SYSV_ABI sceSystemServiceActivateMpeg2Abort(void);
int PS4_SYSV_ABI sceSystemServiceActivateMpeg2GetStatus(void);
int PS4_SYSV_ABI sceSystemServiceActivateMpeg2Init(void);
int PS4_SYSV_ABI sceSystemServiceActivateMpeg2IsActivated(void);
int PS4_SYSV_ABI sceSystemServiceActivateMpeg2Start(void);
int PS4_SYSV_ABI sceSystemServiceActivateMpeg2Term(void);
int PS4_SYSV_ABI sceSystemStateMgrCancelShutdownTimer(void);
int PS4_SYSV_ABI sceSystemStateMgrEnterMediaPlaybackMode(void);
int PS4_SYSV_ABI sceSystemStateMgrEnterStandby(void);
int PS4_SYSV_ABI sceSystemStateMgrExtendShutdownTimer(void);
int PS4_SYSV_ABI sceSystemStateMgrExtendShutdownTimerForPostAutoUpdateProcess(void);
int PS4_SYSV_ABI sceSystemStateMgrGetCurrentState(void);
int PS4_SYSV_ABI sceSystemStateMgrGetTriggerCode(void);
int PS4_SYSV_ABI sceSystemStateMgrIsBdDriveReady(void);
int PS4_SYSV_ABI sceSystemStateMgrIsGpuPerformanceNormal(void);
int PS4_SYSV_ABI sceSystemStateMgrIsShellUIShutdownInProgress(void);
int PS4_SYSV_ABI sceSystemStateMgrIsStandbyModeEnabled(void);
int PS4_SYSV_ABI sceSystemStateMgrLeaveMediaPlaybackMode(void);
int PS4_SYSV_ABI sceSystemStateMgrNotifySystemSuspendResumeProgress(void);
int PS4_SYSV_ABI sceSystemStateMgrReboot(void);
int PS4_SYSV_ABI sceSystemStateMgrSendCecOneTouchPlayCommand(void);
int PS4_SYSV_ABI sceSystemStateMgrStartRebootTimer(void);
int PS4_SYSV_ABI sceSystemStateMgrStartShutdownTimer(void);
int PS4_SYSV_ABI sceSystemStateMgrStartStadbyTimer(void);
int PS4_SYSV_ABI sceSystemStateMgrStartVshAutoUpdateTimer(void);
int PS4_SYSV_ABI sceSystemStateMgrTickMusicPlayback(void);
int PS4_SYSV_ABI sceSystemStateMgrTickPartyChat(void);
int PS4_SYSV_ABI sceSystemStateMgrTurnOff(void);
int PS4_SYSV_ABI sceSystemStateMgrVshAutoUpdate(void);
int PS4_SYSV_ABI sceSystemStateMgrWaitVshAutoUpdateVerifyDone(void);
int PS4_SYSV_ABI sceSystemStateMgrWakeUp(void);
int PS4_SYSV_ABI SystemService_Func_89F262179C22B49E(void);
int PS4_SYSV_ABI SystemService_Func_AC8A8FAB4A1696B8(void);
int PS4_SYSV_ABI sceSystemServiceInvokeAppLaunchLink(void);
int PS4_SYSV_ABI sceSystemServiceShowClosedCaptionAdvancedSettings(void);
int PS4_SYSV_ABI sceSystemServiceShowClosedCaptionSettings(void);
int PS4_SYSV_ABI sceSystemServiceSetPowerSaveLevel(void);
int PS4_SYSV_ABI sceSystemServiceInitializeForShellCore(void);
int PS4_SYSV_ABI SystemService_Func_7C1183FC73629929(void);
int PS4_SYSV_ABI sceSystemServiceDisablePartyVoice(void);
int PS4_SYSV_ABI sceSystemServiceReenablePartyVoice(void);
int PS4_SYSV_ABI sceSystemServiceGetPlatformPrivacyDefinitionData(void);
int PS4_SYSV_ABI sceSystemServiceGetPlatformPrivacyDefinitionVersion(void);
int PS4_SYSV_ABI sceSystemServiceGetPlatformPrivacySetting(void);
int PS4_SYSV_ABI sceSystemServiceDeclareReadyForSuspend(void);
int PS4_SYSV_ABI sceSystemServiceDisableSuspendNotification(void);
int PS4_SYSV_ABI sceSystemServiceEnableSuspendNotification(void);
int PS4_SYSV_ABI sceSystemServiceRequestPowerOff(void);
int PS4_SYSV_ABI sceSystemServiceRequestReboot(void);
int PS4_SYSV_ABI sceSystemServiceAddLocalProcessForPs2Emu(void);
int PS4_SYSV_ABI sceSystemServiceGetParentSocketForPs2Emu(void);
int PS4_SYSV_ABI sceSystemServiceKillLocalProcessForPs2Emu(void);
int PS4_SYSV_ABI sceSystemServiceShowImposeMenuForPs2Emu(void);
int PS4_SYSV_ABI sceSystemServiceSaveVideoToken(void);
int PS4_SYSV_ABI sceSystemServiceLaunchStore(void);
int PS4_SYSV_ABI sceSystemServiceTelemetrySetData(void);
int PS4_SYSV_ABI SystemService_Func_C67FC780F5B6F71E(void);
int PS4_SYSV_ABI sceSystemServiceLaunchUdsApp(void);
int PS4_SYSV_ABI sceSystemServiceLoadExecVideoServiceWebApp(void);
int PS4_SYSV_ABI sceSystemServiceDisableVoiceRecognition(void);
int PS4_SYSV_ABI sceSystemServiceReenableVoiceRecognition(void);
int PS4_SYSV_ABI SystemService_Func_6B1CDB955F0EBD65(void);
int PS4_SYSV_ABI SystemService_Func_CB5E885E225F69F0(void);

#ifdef __cplusplus
}
#endif

#endif
