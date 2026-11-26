/* SPDX-FileCopyrightText: Copyright 2024-2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

/* libSceSystemService, with libSceShellCoreUtil, libSceLncUtil, libSceSystemStateMgr and
 * libSceAppMessaging: the system's parameters (language, formats, time zone), the splash
 * screen, the queue of system events the game polls, and restarting into another executable.
 * The rest of the libraries' functions are not implemented. */

#include <string.h>
#include <time.h>

#include <retro_atomic.h>

#include "common/c_bridge.h"
#include "core/emulator_settings.h"
#include "core/libraries/error_codes.h"
#include "core/libraries/kernel/kernel_c.h"
#include "core/libraries/music/music_player_service.h"
#include "core/libraries/system/systemservice.h"
#include "core/libraries/system/systemservice_c.h"
#include "core/libraries/system/systemservice_error.h"

#define LOG_CLASS C_LOG_LIB_SYSTEM_SERVICE

static retro_atomic_int_t g_splash_hidden;

/* The events the game has not received yet. Any thread adds (NP's for an invitation, the
 * game's own for an entitlement), the game's thread receives: an adder claims a slot by the
 * count of events added, fills the event in place and posts it, and the game receives the
 * slots in that order as they are posted. An event is 8 KiB, so it is written once, where it
 * waits, and copied once, to the game. A claim that finds every slot taken gets none. */
#define EVENT_SLOTS 16
/* Adders that may be claiming at once, each of which the room is checked for before it adds */
#define EVENT_ADDERS 4

static OrbisSystemServiceEvent g_events[EVENT_SLOTS];
static retro_atomic_int_t g_events_posted[EVENT_SLOTS];
static retro_atomic_int_t g_events_added;
static retro_atomic_int_t g_events_taken;

static int events_waiting(void) {
    return (int)((unsigned)retro_atomic_load_acquire_int(&g_events_added) -
                 (unsigned)retro_atomic_load_acquire_int(&g_events_taken));
}

OrbisSystemServiceEvent* system_service_event_claim(void) {
    unsigned added;
    OrbisSystemServiceEvent* event;
    if (events_waiting() >= EVENT_SLOTS - EVENT_ADDERS) {
        c_log(LOG_CLASS, C_LOG_LEVEL_ERROR, "system_service_event_claim",
              "The game receives no events: one dropped");
        return NULL;
    }
    added = (unsigned)retro_atomic_fetch_add_int(&g_events_added, 1);
    event = &g_events[added % EVENT_SLOTS];
    memset(event, 0, sizeof(*event));
    return event;
}

void system_service_event_post(OrbisSystemServiceEvent* event) {
    retro_atomic_store_release_int(&g_events_posted[event - g_events], 1);
}

void system_service_push_entitlement_update(void) {
    OrbisSystemServiceEvent* event = system_service_event_claim();
    if (event == NULL) {
        return;
    }
    /* User 0, service label 0: the claim zeroed them */
    event->event_type = ORBIS_SYSTEM_SERVICE_EVENT_ENTITLEMENT_UPDATE;
    system_service_event_post(event);
}

bool system_service_is_splash_visible(void) {
    return emu_settings_get_show_splash() && !retro_atomic_load_relaxed_int(&g_splash_hidden);
}

int PS4_SYSV_ABI sceShellCoreUtilIsBgmCpuBudgetAcquired(void) {
    c_log(LOG_CLASS, C_LOG_LEVEL_DEBUG, "sceShellCoreUtilIsBgmCpuBudgetAcquired", "called");
    return 1;
}

int PS4_SYSV_ABI sceShellCoreUtilIsBgmCpuBudgetAvailable(void) {
    c_log(LOG_CLASS, C_LOG_LEVEL_DEBUG, "sceShellCoreUtilIsBgmCpuBudgetAvailable", "called");
    return 1;
}

int PS4_SYSV_ABI sceShellCoreUtilIsBgmPlaying(void) {
    c_log(LOG_CLASS, C_LOG_LEVEL_DEBUG, "sceShellCoreUtilIsBgmPlaying", "called");
    return music_player_is_bgm_playing() ? 1 : 0;
}

s32 PS4_SYSV_ABI
sceSystemServiceGetDisplaySafeAreaInfo(OrbisSystemServiceDisplaySafeAreaInfo* info) {
    c_log(LOG_CLASS, C_LOG_LEVEL_DEBUG, "sceSystemServiceGetDisplaySafeAreaInfo", "called");
    if (info == NULL) {
        c_log(LOG_CLASS, C_LOG_LEVEL_ERROR, "sceSystemServiceGetDisplaySafeAreaInfo",
              "OrbisSystemServiceDisplaySafeAreaInfo is null");
        return ORBIS_SYSTEM_SERVICE_ERROR_PARAMETER;
    }
    info->ratio = 1.0f;
    return ORBIS_OK;
}

/* What an HDR game tone maps to: the brightest and darkest the frontend shows (HDR Output), in
 * nits. The full-frame peak is given the peak too: the frontend does not say what a display
 * holds over the whole screen. */
int PS4_SYSV_ABI
sceSystemServiceGetHdrToneMapLuminance(OrbisSystemServiceHdrToneMapLuminance* luminance) {
    float peak;
    if (luminance == NULL) {
        return ORBIS_SYSTEM_SERVICE_ERROR_PARAMETER;
    }
    peak = (float)emu_settings_get_hdr_max_nits();
    luminance->max_full_frame_tone_map_luminance = peak;
    luminance->max_tone_map_luminance = peak;
    luminance->min_tone_map_luminance = (float)emu_settings_get_hdr_min_nits();
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceSystemServiceGetStatus(OrbisSystemServiceStatus* status) {
#ifndef NDEBUG
    c_log(LOG_CLASS, C_LOG_LEVEL_TRACE, "sceSystemServiceGetStatus", "called");
#endif
    if (status == NULL) {
        c_log(LOG_CLASS, C_LOG_LEVEL_ERROR, "sceSystemServiceGetStatus",
              "OrbisSystemServiceStatus is null");
        return ORBIS_SYSTEM_SERVICE_ERROR_PARAMETER;
    }
    status->event_num = events_waiting();
    status->is_system_ui_overlaid = 0;
    status->is_in_background_execution = 0;
    status->is_cpu_mode7_cpu_normal = 1;
    status->is_game_live_streaming_on_air = 0;
    status->is_out_of_vr_play_area = 0;
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceSystemServiceHideSplashScreen(void) {
    c_log(LOG_CLASS, C_LOG_LEVEL_INFO, "sceSystemServiceHideSplashScreen", "called");
    retro_atomic_store_relaxed_int(&g_splash_hidden, 1);
    return ORBIS_OK;
}

int PS4_SYSV_ABI sceSystemServiceIsBgmPlaying(void) {
    c_log(LOG_CLASS, C_LOG_LEVEL_DEBUG, "sceSystemServiceIsBgmPlaying", "called");
    return music_player_is_bgm_playing() ? 1 : 0;
}

int PS4_SYSV_ABI sceSystemServiceLoadExec(const char* path, const char* argv[]) {
    c_log(LOG_CLASS, C_LOG_LEVEL_DEBUG, "sceSystemServiceLoadExec", "called");
    system_service_load_exec(path, (const char* const*)argv);
    return ORBIS_OK;
}

/* The language as the game's SDK knows it: one a newer firmware added is told as the one it
 * is closest to */
static s32 language_for_sdk(s32 lang) {
    s32 sdk = 0;
    if (kernel_get_compiled_sdk_version(&sdk) != 0) {
        sdk = 0;
    }
    if (lang == 0x15 && sdk < KERNEL_FW_200) {
        lang = 0x12;
    }
    if (lang == 0x16 && sdk < KERNEL_FW_250) {
        lang = 2;
    }
    if ((lang >= 0x17 && lang <= 0x1a) && sdk < KERNEL_FW_500) {
        lang = 0x12;
    }
    if ((lang >= 0x1b && lang <= 0x1d) && sdk < KERNEL_FW_500) {
        lang = 1;
    }
    if (lang == 0x1e && sdk < KERNEL_FW_1000) {
        lang = 0x12;
    }
    return lang;
}

/* The host's local time now: the time zone and the summer time are the user's */
static void local_now(struct tm* out) {
    time_t t = time(NULL);
#ifdef _WIN32
    localtime_s(out, &t);
#else
    localtime_r(&t, out);
#endif
}

s32 PS4_SYSV_ABI sceSystemServiceParamGetInt(s32 param_id, int* value) {
    struct tm now;
    c_log(LOG_CLASS, C_LOG_LEVEL_DEBUG, "sceSystemServiceParamGetInt", "called param_id %u",
          (u32)param_id);
    if (value == NULL) {
        c_log(LOG_CLASS, C_LOG_LEVEL_ERROR, "sceSystemServiceParamGetInt", "value is null");
        return ORBIS_SYSTEM_SERVICE_ERROR_PARAMETER;
    }
    switch (param_id) {
    case ORBIS_SYSTEM_SERVICE_PARAM_ID_LANG:
        *value = language_for_sdk(emu_settings_get_console_language());
        break;
    case ORBIS_SYSTEM_SERVICE_PARAM_ID_DATE_FORMAT:
        *value = ORBIS_SYSTEM_PARAM_DATE_FORMAT_DDMMYYYY;
        break;
    case ORBIS_SYSTEM_SERVICE_PARAM_ID_TIME_FORMAT:
        *value = ORBIS_SYSTEM_PARAM_TIME_FORMAT_24HOUR;
        break;
    case ORBIS_SYSTEM_SERVICE_PARAM_ID_TIME_ZONE:
#ifdef _WIN32
        _tzset();
        *value = -(int)(_timezone / 60);
#else
        local_now(&now);
        *value = (int)((now.tm_gmtoff - (now.tm_isdst > 0 ? 3600 : 0)) / 60);
#endif
        break;
    case ORBIS_SYSTEM_SERVICE_PARAM_ID_SUMMERTIME:
        local_now(&now);
        *value = now.tm_isdst > 0 ? 1 : 0;
        break;
    case ORBIS_SYSTEM_SERVICE_PARAM_ID_GAME_PARENTAL_LEVEL:
        *value = ORBIS_SYSTEM_PARAM_GAME_PARENTAL_OFF;
        break;
    case ORBIS_SYSTEM_SERVICE_PARAM_ID_ENTER_BUTTON_ASSIGN:
        *value = emu_settings_get_is_circle_enter() ? ORBIS_SYSTEM_PARAM_ENTER_BUTTON_ASSIGN_CIRCLE
                                                    : ORBIS_SYSTEM_PARAM_ENTER_BUTTON_ASSIGN_CROSS;
        break;
    default:
        c_log(LOG_CLASS, C_LOG_LEVEL_ERROR, "sceSystemServiceParamGetInt",
              "param_id %u unsupported!", (u32)param_id);
        *value = 0;
    }
    return ORBIS_OK;
}

s32 system_service_param_get_int(s32 param_id, s32* value) {
    return sceSystemServiceParamGetInt(param_id, value);
}

s32 PS4_SYSV_ABI sceSystemServiceReceiveEvent(OrbisSystemServiceEvent* event) {
    unsigned taken;
    unsigned slot;
#ifndef NDEBUG
    c_log(LOG_CLASS, C_LOG_LEVEL_TRACE, "sceSystemServiceReceiveEvent", "called");
#endif
    if (event == NULL) {
        return ORBIS_SYSTEM_SERVICE_ERROR_PARAMETER;
    }
    taken = (unsigned)retro_atomic_load_relaxed_int(&g_events_taken);
    slot = taken % EVENT_SLOTS;
    if (!retro_atomic_load_acquire_int(&g_events_posted[slot])) {
        return ORBIS_SYSTEM_SERVICE_ERROR_NO_EVENT;
    }
    memcpy(event, &g_events[slot], sizeof(*event));
    retro_atomic_store_release_int(&g_events_posted[slot], 0);
    retro_atomic_store_release_int(&g_events_taken, (int)(taken + 1));
    c_log(LOG_CLASS, C_LOG_LEVEL_DEBUG, "sceSystemServiceReceiveEvent",
          "delivered event type=0x%x (%d still queued)", (u32)event->event_type, events_waiting());
    return ORBIS_OK;
}

/* The functions that are not implemented */
#define SYSTEM_SERVICE_STUB(name)                                                                  \
    int PS4_SYSV_ABI name(void) {                                                                  \
        c_log(LOG_CLASS, C_LOG_LEVEL_ERROR, #name, "(STUBBED) called");                            \
        return ORBIS_OK;                                                                           \
    }

SYSTEM_SERVICE_STUB(sceAppMessagingClearEventFlag)
SYSTEM_SERVICE_STUB(sceAppMessagingReceiveMsg)
SYSTEM_SERVICE_STUB(sceAppMessagingSendMsg)
SYSTEM_SERVICE_STUB(sceAppMessagingSendMsgToShellCore)
SYSTEM_SERVICE_STUB(sceAppMessagingSendMsgToShellUI)
SYSTEM_SERVICE_STUB(sceAppMessagingSetEventFlag)
SYSTEM_SERVICE_STUB(sceAppMessagingTryGetEventFlag)
SYSTEM_SERVICE_STUB(sceAppMessagingTryReceiveMsg)
SYSTEM_SERVICE_STUB(SystemService_Func_C8E899ABEF7F64C4)
SYSTEM_SERVICE_STUB(SystemService_Func_F74BA759B9C8D2A1)
SYSTEM_SERVICE_STUB(sceLncUtilAcquireCpuBudgetOfExtraAudioDevices)
SYSTEM_SERVICE_STUB(sceLncUtilAcquireCpuBudgetOfImeForBigApp)
SYSTEM_SERVICE_STUB(sceLncUtilAcquireCpuBudgetOfInGameStore)
SYSTEM_SERVICE_STUB(sceLncUtilActivateCdlg)
SYSTEM_SERVICE_STUB(sceLncUtilAddLocalProcess)
SYSTEM_SERVICE_STUB(sceLncUtilBlockAppSuspend)
SYSTEM_SERVICE_STUB(sceLncUtilBlockingGetEventForDaemon)
SYSTEM_SERVICE_STUB(sceLncUtilContinueApp)
SYSTEM_SERVICE_STUB(sceLncUtilCrashSyscore)
SYSTEM_SERVICE_STUB(sceLncUtilDeactivateCdlg)
SYSTEM_SERVICE_STUB(sceLncUtilDeclareReadyForSuspend)
SYSTEM_SERVICE_STUB(sceLncUtilDisableSuspendNotification)
SYSTEM_SERVICE_STUB(sceLncUtilEnableSuspendNotification)
SYSTEM_SERVICE_STUB(sceLncUtilFinishSpecialResume)
SYSTEM_SERVICE_STUB(sceLncUtilForceKillApp)
SYSTEM_SERVICE_STUB(sceLncUtilForceKillLocalProcess)
SYSTEM_SERVICE_STUB(sceLncUtilGetApp0DirPath)
SYSTEM_SERVICE_STUB(sceLncUtilGetAppCategory)
SYSTEM_SERVICE_STUB(sceLncUtilGetAppFocusedAppStatus)
SYSTEM_SERVICE_STUB(sceLncUtilGetAppId)
SYSTEM_SERVICE_STUB(sceLncUtilGetAppIdOfBigApp)
SYSTEM_SERVICE_STUB(sceLncUtilGetAppIdOfMiniApp)
SYSTEM_SERVICE_STUB(sceLncUtilGetAppLaunchedUser)
SYSTEM_SERVICE_STUB(sceLncUtilGetAppStatus)
SYSTEM_SERVICE_STUB(sceLncUtilGetAppStatusListForShellUIReboot)
SYSTEM_SERVICE_STUB(sceLncUtilGetAppTitleId)
SYSTEM_SERVICE_STUB(sceLncUtilGetAppType)
SYSTEM_SERVICE_STUB(sceLncUtilGetCdlgExec)
SYSTEM_SERVICE_STUB(sceLncUtilGetCoredumpState)
SYSTEM_SERVICE_STUB(sceLncUtilGetDbgExecutablePath)
SYSTEM_SERVICE_STUB(sceLncUtilGetEventForDaemon)
SYSTEM_SERVICE_STUB(sceLncUtilGetEventForShellUI)
SYSTEM_SERVICE_STUB(sceLncUtilGetGpuCrashFullDumpAppStatus)
SYSTEM_SERVICE_STUB(sceLncUtilGetLocalProcessStatusList)
SYSTEM_SERVICE_STUB(sceLncUtilGetParentSocket)
SYSTEM_SERVICE_STUB(sceLncUtilGetResultKillApp)
SYSTEM_SERVICE_STUB(sceLncUtilGetResultLaunchAppByTitleId)
SYSTEM_SERVICE_STUB(sceLncUtilInitialize)
SYSTEM_SERVICE_STUB(sceLncUtilIsActiveCdlg)
SYSTEM_SERVICE_STUB(sceLncUtilIsAppLaunched)
SYSTEM_SERVICE_STUB(sceLncUtilIsAppSuspended)
SYSTEM_SERVICE_STUB(sceLncUtilIsCpuBudgetOfExtraAudioDevicesAvailable)
SYSTEM_SERVICE_STUB(sceLncUtilIsPs2Emu)
SYSTEM_SERVICE_STUB(sceLncUtilIsShellUiFgAndGameBgCpuMode)
SYSTEM_SERVICE_STUB(sceLncUtilKickCoredumpOnlyProcMem)
SYSTEM_SERVICE_STUB(sceLncUtilKillApp)
SYSTEM_SERVICE_STUB(sceLncUtilKillAppWithReason)
SYSTEM_SERVICE_STUB(sceLncUtilKillLocalProcess)
SYSTEM_SERVICE_STUB(sceLncUtilLaunchApp)
SYSTEM_SERVICE_STUB(sceLncUtilLoadExec)
SYSTEM_SERVICE_STUB(sceLncUtilNotifyCoredumpRequestEnd)
SYSTEM_SERVICE_STUB(sceLncUtilNotifyCoredumpRequestProgress)
SYSTEM_SERVICE_STUB(sceLncUtilNotifyVshReady)
SYSTEM_SERVICE_STUB(sceLncUtilRaiseException)
SYSTEM_SERVICE_STUB(sceLncUtilRaiseExceptionLocalProcess)
SYSTEM_SERVICE_STUB(sceLncUtilRegisterCdlgSharedMemoryName)
SYSTEM_SERVICE_STUB(sceLncUtilRegisterDaemon)
SYSTEM_SERVICE_STUB(sceLncUtilRegisterShellUI)
SYSTEM_SERVICE_STUB(sceLncUtilReleaseCpuBudgetOfExtraAudioDevices)
SYSTEM_SERVICE_STUB(sceLncUtilReleaseCpuBudgetOfImeForBigApp)
SYSTEM_SERVICE_STUB(sceLncUtilReleaseCpuBudgetOfInGameStore)
SYSTEM_SERVICE_STUB(sceLncUtilResumeApp)
SYSTEM_SERVICE_STUB(sceLncUtilResumeLocalProcess)
SYSTEM_SERVICE_STUB(sceLncUtilSetAppFocus)
SYSTEM_SERVICE_STUB(sceLncUtilSetCdlgExec)
SYSTEM_SERVICE_STUB(sceLncUtilSetControllerFocus)
SYSTEM_SERVICE_STUB(sceLncUtilSetControllerFocusPermission)
SYSTEM_SERVICE_STUB(sceLncUtilStartKillApp)
SYSTEM_SERVICE_STUB(sceLncUtilStartLaunchAppByTitleId)
SYSTEM_SERVICE_STUB(sceLncUtilSuspendApp)
SYSTEM_SERVICE_STUB(sceLncUtilSuspendBackgroundApp)
SYSTEM_SERVICE_STUB(sceLncUtilSuspendLocalProcess)
SYSTEM_SERVICE_STUB(sceLncUtilSystemSuspend)
SYSTEM_SERVICE_STUB(sceLncUtilTerminate)
SYSTEM_SERVICE_STUB(sceLncUtilTryBlockAppSuspend)
SYSTEM_SERVICE_STUB(sceLncUtilUnblockAppSuspend)
SYSTEM_SERVICE_STUB(sceLncUtilUnregisterCdlgSharedMemoryName)
SYSTEM_SERVICE_STUB(sceLncUtilUnregisterDaemon)
SYSTEM_SERVICE_STUB(sceLncUtilUnregisterShellUI)
SYSTEM_SERVICE_STUB(sceSystemServiceActivateHevcSoft)
SYSTEM_SERVICE_STUB(sceSystemServiceActivateHevcSoftAbort)
SYSTEM_SERVICE_STUB(sceSystemServiceActivateHevcSoftGetStatus)
SYSTEM_SERVICE_STUB(sceSystemServiceActivateHevcSoftInit)
SYSTEM_SERVICE_STUB(sceSystemServiceActivateHevcSoftIsActivated)
SYSTEM_SERVICE_STUB(sceSystemServiceActivateHevcSoftStart)
SYSTEM_SERVICE_STUB(sceSystemServiceActivateHevcSoftTerm)
SYSTEM_SERVICE_STUB(sceShellCoreUtilAccessibilityZoomLock)
SYSTEM_SERVICE_STUB(sceShellCoreUtilAccessibilityZoomUnlock)
SYSTEM_SERVICE_STUB(sceShellCoreUtilAcquireBgmCpuBudget)
SYSTEM_SERVICE_STUB(sceShellCoreUtilAcquireRemotePlayCpuBudget)
SYSTEM_SERVICE_STUB(sceShellCoreUtilAcquireSharePlayCpuBudget)
SYSTEM_SERVICE_STUB(sceShellCoreUtilActivateAbort)
SYSTEM_SERVICE_STUB(sceShellCoreUtilActivateGetStatus)
SYSTEM_SERVICE_STUB(sceShellCoreUtilActivateInit)
SYSTEM_SERVICE_STUB(sceShellCoreUtilActivateIsActivated)
SYSTEM_SERVICE_STUB(sceShellCoreUtilActivateRecordActivation)
SYSTEM_SERVICE_STUB(sceShellCoreUtilActivateStart)
SYSTEM_SERVICE_STUB(sceShellCoreUtilActivateStartAsync)
SYSTEM_SERVICE_STUB(sceShellCoreUtilActivateTerm)
SYSTEM_SERVICE_STUB(sceShellCoreUtilChangeRunLevel)
SYSTEM_SERVICE_STUB(sceShellCoreUtilChangeToStaffModeForIDU)
SYSTEM_SERVICE_STUB(sceShellCoreUtilCheckerAbort)
SYSTEM_SERVICE_STUB(sceShellCoreUtilCleanupCrashReport)
SYSTEM_SERVICE_STUB(sceShellCoreUtilClearAppData)
SYSTEM_SERVICE_STUB(sceShellCoreUtilClearPsnAccountInfo)
SYSTEM_SERVICE_STUB(sceShellCoreUtilCrashReportRequestCancel)
SYSTEM_SERVICE_STUB(sceShellCoreUtilDeclareBeginOfExternalStorageAppMove)
SYSTEM_SERVICE_STUB(sceShellCoreUtilDeclareEndOfExternalStorageAppMove)
SYSTEM_SERVICE_STUB(sceShellCoreUtilDeleteDiscInstalledTitleWorkaroundFile)
SYSTEM_SERVICE_STUB(sceShellCoreUtilDeleteDownloadedHidConfigFile)
SYSTEM_SERVICE_STUB(sceShellCoreUtilDeleteDownloadedNetEvConfigFile)
SYSTEM_SERVICE_STUB(sceShellCoreUtilDeleteDownloadedTitleWorkaroundFile)
SYSTEM_SERVICE_STUB(sceShellCoreUtilDeleteSmrHddDummyData)
SYSTEM_SERVICE_STUB(sceShellCoreUtilDoFsck)
SYSTEM_SERVICE_STUB(sceShellCoreUtilDownloadHidConfigFileFromServer)
SYSTEM_SERVICE_STUB(sceShellCoreUtilDownloadNetEvConfigFileFromServer)
SYSTEM_SERVICE_STUB(sceShellCoreUtilDownloadTitleWorkaroundFileFromServer)
SYSTEM_SERVICE_STUB(sceShellCoreUtilEnterPowerLockSection)
SYSTEM_SERVICE_STUB(sceShellCoreUtilExecuteCrashReport)
SYSTEM_SERVICE_STUB(sceShellCoreUtilExfatFormatExternalHdd)
SYSTEM_SERVICE_STUB(sceShellCoreUtilExitMiniApp)
SYSTEM_SERVICE_STUB(sceShellCoreUtilExitMiniAppWithValue)
SYSTEM_SERVICE_STUB(sceShellCoreUtilFillUpSpaceOnSmrHdd)
SYSTEM_SERVICE_STUB(sceShellCoreUtilFormatExternalHdd)
SYSTEM_SERVICE_STUB(sceShellCoreUtilFormatHddForRestore)
SYSTEM_SERVICE_STUB(sceShellCoreUtilFreeUpSpaceOnSmrHdd)
SYSTEM_SERVICE_STUB(sceShellCoreUtilGetAppData)
SYSTEM_SERVICE_STUB(sceShellCoreUtilGetAppEnableTTS)
SYSTEM_SERVICE_STUB(sceShellCoreUtilGetAppEnterButtonAssign)
SYSTEM_SERVICE_STUB(sceShellCoreUtilGetAppLaunchedParamInt)
SYSTEM_SERVICE_STUB(sceShellCoreUtilGetAppLaunchedParamIntByBudgetType)
SYSTEM_SERVICE_STUB(sceShellCoreUtilGetAppLaunchedParamString)
SYSTEM_SERVICE_STUB(sceShellCoreUtilGetAppLaunchedParamStringByBudgetType)
SYSTEM_SERVICE_STUB(sceShellCoreUtilGetAppLaunchTypeInfo)
SYSTEM_SERVICE_STUB(sceShellCoreUtilGetAutoPowerDownRemainingSeconds)
SYSTEM_SERVICE_STUB(sceShellCoreUtilGetBasicProductShape)
SYSTEM_SERVICE_STUB(sceShellCoreUtilGetCheckerString)
SYSTEM_SERVICE_STUB(sceShellCoreUtilGetCheckerStringEx)
SYSTEM_SERVICE_STUB(sceShellCoreUtilGetCloudClientStatus)
SYSTEM_SERVICE_STUB(sceShellCoreUtilGetCrashReportCoreFileSetSize)
SYSTEM_SERVICE_STUB(sceShellCoreUtilGetCrashReportFilterInfoStart)
SYSTEM_SERVICE_STUB(sceShellCoreUtilGetCrashReportInfoForBoot)
SYSTEM_SERVICE_STUB(sceShellCoreUtilGetCrashReportInfoForBootStart)
SYSTEM_SERVICE_STUB(sceShellCoreUtilGetCrashReportInfoStart)
SYSTEM_SERVICE_STUB(sceShellCoreutilGetCrashReportProcessInformation)
SYSTEM_SERVICE_STUB(sceShellCoreUtilGetCrashReportResult)
SYSTEM_SERVICE_STUB(sceShellCoreUtilGetCrashReportStatus)
SYSTEM_SERVICE_STUB(sceShellCoreUtilGetCrashReportUploadStatus)
SYSTEM_SERVICE_STUB(sceShellCoreUtilGetDeviceIndexBehavior)
SYSTEM_SERVICE_STUB(sceShellCoreUtilGetDeviceIndexBehaviorWithTimeout)
SYSTEM_SERVICE_STUB(sceShellCoreUtilGetEffectiveTotalSizeOfUserPartition)
SYSTEM_SERVICE_STUB(sceShellCoreUtilGetFreeSizeOfAvContentsTmp)
SYSTEM_SERVICE_STUB(sceShellCoreUtilGetFreeSizeOfUserPartition)
SYSTEM_SERVICE_STUB(sceShellCoreUtilGetFsckProgress)
SYSTEM_SERVICE_STUB(sceShellCoreUtilGetGameLiveStreamingStatus)
SYSTEM_SERVICE_STUB(sceShellCoreUtilGetGnmCompositorOnScreenProfilerFlag)
SYSTEM_SERVICE_STUB(sceShellCoreUtilGetGpuLoadEmulationMode)
SYSTEM_SERVICE_STUB(sceShellCoreUtilGetGpuLoadEmulationModeByAppId)
SYSTEM_SERVICE_STUB(sceShellCoreUtilGetHidConfigFileInfoString)
SYSTEM_SERVICE_STUB(sceShellCoreUtilGetHidConfigFileString)
SYSTEM_SERVICE_STUB(sceShellCoreUtilGetHidConfigName)
SYSTEM_SERVICE_STUB(sceShellCoreUtilGetHidConfigNum)
SYSTEM_SERVICE_STUB(sceShellCoreUtilGetIDUMode)
SYSTEM_SERVICE_STUB(sceShellCoreUtilGetImposeMenuFlagForPs2Emu)
SYSTEM_SERVICE_STUB(sceShellCoreUtilGetManifestFileStatus)
SYSTEM_SERVICE_STUB(sceShellCoreUtilGetNeedSizeOfAppContent)
SYSTEM_SERVICE_STUB(sceShellCoreUtilGetNetEvConfigFileInfoString)
SYSTEM_SERVICE_STUB(sceShellCoreUtilGetOptimizationStatus)
SYSTEM_SERVICE_STUB(sceShellCoreUtilGetOutOfVrPlayZoneWarning)
SYSTEM_SERVICE_STUB(sceShellCoreUtilGetPapcGamePcl)
SYSTEM_SERVICE_STUB(sceShellCoreUtilGetPbtcUserInfoList)
SYSTEM_SERVICE_STUB(sceShellCoreUtilGetPlatformPrivacyDefinitionEventData)
SYSTEM_SERVICE_STUB(sceShellCoreUtilGetPlatformPrivacySetting)
SYSTEM_SERVICE_STUB(sceShellCoreUtilGetProgressOfFormatExternalHdd)
SYSTEM_SERVICE_STUB(sceShellCoreUtilGetProgressOfFsck)
SYSTEM_SERVICE_STUB(sceShellCoreUtilGetPsnAccountInfo)
SYSTEM_SERVICE_STUB(sceShellCoreUtilGetPsStoreIconLayout)
SYSTEM_SERVICE_STUB(sceShellCoreUtilGetPsStoreIconState)
SYSTEM_SERVICE_STUB(sceShellCoreUtilGetRegion)
SYSTEM_SERVICE_STUB(sceShellCoreUtilGetRemotePlayStatus)
SYSTEM_SERVICE_STUB(sceShellCoreUtilGetRunLevel)
SYSTEM_SERVICE_STUB(sceShellCoreUtilGetSharePlayStatus)
SYSTEM_SERVICE_STUB(sceShellCoreUtilGetShellUIVMStats)
SYSTEM_SERVICE_STUB(sceShellCoreUtilGetSmrHddInfoString)
SYSTEM_SERVICE_STUB(sceShellCoreUtilGetSocialScreenStatus)
SYSTEM_SERVICE_STUB(sceShellCoreUtilGetSplashScreenState)
SYSTEM_SERVICE_STUB(sceShellCoreUtilGetSupportSiteURL)
SYSTEM_SERVICE_STUB(sceShellCoreUtilGetSuspendConfirmationDialogFlag)
SYSTEM_SERVICE_STUB(sceShellCoreUtilGetSystemBGState)
SYSTEM_SERVICE_STUB(sceShellCoreUtilGetSystemBGWaveColor)
SYSTEM_SERVICE_STUB(sceShellCoreUtilGetSystemBGWaveState)
SYSTEM_SERVICE_STUB(sceShellCoreUtilGetTitleWorkaroundFileInfoString)
SYSTEM_SERVICE_STUB(sceShellCoreUtilGetTitleWorkaroundFileString)
SYSTEM_SERVICE_STUB(sceShellCoreUtilGetUIStatus)
SYSTEM_SERVICE_STUB(sceShellCoreUtilGetUserFocus)
SYSTEM_SERVICE_STUB(sceShellCoreUtilGetUserIdOfMorpheusUser)
SYSTEM_SERVICE_STUB(sceShellCoreUtilGetVersionNumberOfCameraCalibrationData)
SYSTEM_SERVICE_STUB(sceShellCoreUtilGoBackToKratosCurrentSessionGame)
SYSTEM_SERVICE_STUB(sceShellCoreUtilHideBlocksFromUser)
SYSTEM_SERVICE_STUB(sceShellCoreUtilIncrementVersionNumberOfCameraCalibrationData)
SYSTEM_SERVICE_STUB(sceShellCoreUtilIsAccessibilityZoomLocked)
SYSTEM_SERVICE_STUB(sceShellCoreUtilIsExternalStorageAppMoveInProgress)
SYSTEM_SERVICE_STUB(sceShellCoreUtilIsEyeToEyeDistanceAdjusted)
SYSTEM_SERVICE_STUB(sceShellCoreUtilIsGameLiveStreamingOnAir)
SYSTEM_SERVICE_STUB(sceShellCoreUtilIsImposeScreenOverlaid)
SYSTEM_SERVICE_STUB(sceShellCoreUtilIsInSystemSuspendBlackList)
SYSTEM_SERVICE_STUB(sceShellCoreUtilIsInternalKratosUser)
SYSTEM_SERVICE_STUB(sceShellCoreUtilIsKilledOrSuspendedByLogout)
SYSTEM_SERVICE_STUB(sceShellCoreUtilIsNeededCrashReport)
SYSTEM_SERVICE_STUB(sceShellCoreUtilIsPowerSaveAlertRequested)
SYSTEM_SERVICE_STUB(sceShellCoreUtilIsRemotePlayCpuBudgetAcquired)
SYSTEM_SERVICE_STUB(sceShellCoreUtilIsScreenSaverOn)
SYSTEM_SERVICE_STUB(sceShellCoreUtilIsSharePlayCpuBudgetAcquired)
SYSTEM_SERVICE_STUB(sceShellCoreUtilIsShowCrashReport)
SYSTEM_SERVICE_STUB(sceShellCoreUtilIsTemperatureDanger)
SYSTEM_SERVICE_STUB(sceShellCoreUtilIsTitleWorkaroundEnabled)
SYSTEM_SERVICE_STUB(sceShellCoreUtilIsUsbMassStorageMounted)
SYSTEM_SERVICE_STUB(sceShellCoreUtilLaunchByUri)
SYSTEM_SERVICE_STUB(sceShellCoreUtilLeavePowerLockSection)
SYSTEM_SERVICE_STUB(sceShellCoreUtilLog)
SYSTEM_SERVICE_STUB(sceShellCoreUtilMakeManifestFile)
SYSTEM_SERVICE_STUB(sceShellCoreUtilMountAppRight)
SYSTEM_SERVICE_STUB(sceShellCoreUtilMountDownloadDataForShellUI)
SYSTEM_SERVICE_STUB(sceShellCoreUtilMountHddForBackup)
SYSTEM_SERVICE_STUB(sceShellCoreUtilMountHddForRestore)
SYSTEM_SERVICE_STUB(sceShellCoreUtilNavigateToAnotherApp)
SYSTEM_SERVICE_STUB(sceShellCoreUtilNavigateToGoHome)
SYSTEM_SERVICE_STUB(sceShellCoreUtilNavigateToLaunchedApp)
SYSTEM_SERVICE_STUB(sceShellCoreUtilNotificationCancelForIDU)
SYSTEM_SERVICE_STUB(sceShellCoreUtilNotificationRequestedForIDU)
SYSTEM_SERVICE_STUB(sceShellCoreUtilNotifyBgmCoreTermination)
SYSTEM_SERVICE_STUB(sceShellCoreUtilNotifyFarsightUIDone)
SYSTEM_SERVICE_STUB(sceShellCoreUtilNotifyFsReadError)
SYSTEM_SERVICE_STUB(sceShellCoreUtilNotifyPsnAccountInfoReceived)
SYSTEM_SERVICE_STUB(sceShellCoreUtilNotifyYouTubeAccountLinkStatusChanged)
SYSTEM_SERVICE_STUB(sceShellCoreUtilPfAuthClientConsoleTokenClearCache)
SYSTEM_SERVICE_STUB(sceShellCoreUtilPostActivityForPsNow)
SYSTEM_SERVICE_STUB(sceShellCoreUtilPostErrorLog)
SYSTEM_SERVICE_STUB(sceShellCoreUtilPostLaunchConfirmResult)
SYSTEM_SERVICE_STUB(sceShellCoreUtilPostPsmEventToShellUI)
SYSTEM_SERVICE_STUB(sceShellCoreUtilPreNotifyOfGameLiveStreaming)
SYSTEM_SERVICE_STUB(sceShellCoreUtilPreNotifyOfRemotePlay)
SYSTEM_SERVICE_STUB(sceShellCoreUtilPreNotifyOfSharePlay)
SYSTEM_SERVICE_STUB(sceShellCoreUtilReleaseBgmCpuBudget)
SYSTEM_SERVICE_STUB(sceShellCoreUtilReleaseRemotePlayCpuBudget)
SYSTEM_SERVICE_STUB(sceShellCoreUtilReleaseSharePlayCpuBudget)
SYSTEM_SERVICE_STUB(sceShellCoreUtilReportSessionErrorToGaikaiController)
SYSTEM_SERVICE_STUB(sceShellCoreUtilReportUnexpectedFatalErrorToSystemTelemetry)
SYSTEM_SERVICE_STUB(sceShellCoreUtilRequestCameraCalibration)
SYSTEM_SERVICE_STUB(sceShellCoreUtilRequestEjectDevice)
SYSTEM_SERVICE_STUB(sceShellCoreUtilRequestRebootApp)
SYSTEM_SERVICE_STUB(sceShellCoreUtilRequestShutdown)
SYSTEM_SERVICE_STUB(sceShellCoreUtilResetAutoPowerDownTimer)
SYSTEM_SERVICE_STUB(sceShellCoreUtilResetBgdcConfig)
SYSTEM_SERVICE_STUB(sceShellCoreUtilSetAppData)
SYSTEM_SERVICE_STUB(sceShellCoreUtilSetBgmProhibition)
SYSTEM_SERVICE_STUB(sceShellCoreUtilSetDeviceIndexBehavior)
SYSTEM_SERVICE_STUB(sceShellCoreUtilSetGameLiveStreamingOnAirFlag)
SYSTEM_SERVICE_STUB(sceShellCoreUtilSetGameLiveStreamingStatus)
SYSTEM_SERVICE_STUB(sceShellCoreUtilSetGnmCompositorOnScreenProfilerFlag)
SYSTEM_SERVICE_STUB(sceShellCoreUtilSetGpuLoadEmulationMode)
SYSTEM_SERVICE_STUB(sceShellCoreUtilSetGpuLoadEmulationModeByAppId)
SYSTEM_SERVICE_STUB(sceShellCoreUtilSetIDUMode)
SYSTEM_SERVICE_STUB(sceShellCoreUtilSetImposeStatusFlag)
SYSTEM_SERVICE_STUB(sceShellCoreUtilSetPsStoreIconLayout)
SYSTEM_SERVICE_STUB(sceShellCoreUtilSetPsStoreIconState)
SYSTEM_SERVICE_STUB(sceShellCoreUtilSetRemotePlayStatus)
SYSTEM_SERVICE_STUB(sceShellCoreUtilSetSharePlayStatus)
SYSTEM_SERVICE_STUB(sceShellCoreUtilSetSkipUpdateCheck)
SYSTEM_SERVICE_STUB(sceShellCoreUtilSetSocialScreenStatus)
SYSTEM_SERVICE_STUB(sceShellCoreUtilSetSplashScreenState)
SYSTEM_SERVICE_STUB(sceShellCoreUtilSetSystemBGState)
SYSTEM_SERVICE_STUB(sceShellCoreUtilSetSystemBGWaveColor)
SYSTEM_SERVICE_STUB(sceShellCoreUtilSetSystemBGWaveState)
SYSTEM_SERVICE_STUB(sceShellCoreUtilSetUIStatus)
SYSTEM_SERVICE_STUB(sceShellCoreUtilSetUserFocus)
SYSTEM_SERVICE_STUB(sceShellCoreUtilShowCriticalErrorDialog)
SYSTEM_SERVICE_STUB(sceShellCoreUtilShowErrorDialog)
SYSTEM_SERVICE_STUB(sceShellCoreUtilShowErrorDialogWithFormatArgs)
SYSTEM_SERVICE_STUB(sceShellCoreUtilShowErrorDialogWithParam)
SYSTEM_SERVICE_STUB(sceShellCoreUtilShowPsUnderLockIndicator)
SYSTEM_SERVICE_STUB(sceShellCoreUtilSignalUserInput)
SYSTEM_SERVICE_STUB(sceShellCoreUtilStartOptimization)
SYSTEM_SERVICE_STUB(sceShellCoreUtilStartPsNowGame)
SYSTEM_SERVICE_STUB(sceShellCoreUtilStopOptimization)
SYSTEM_SERVICE_STUB(sceShellCoreUtilStopPsNowGame)
SYSTEM_SERVICE_STUB(sceShellCoreUtilTestBusTransferSpeed)
SYSTEM_SERVICE_STUB(sceShellCoreUtilTickHeartBeat)
SYSTEM_SERVICE_STUB(sceShellCoreUtilTriggerPapcRecalculation)
SYSTEM_SERVICE_STUB(sceShellCoreUtilTriggerPapcUpdate)
SYSTEM_SERVICE_STUB(sceShellCoreUtilTurnOffScreenSaver)
SYSTEM_SERVICE_STUB(sceShellCoreUtilUnmountAppRight)
SYSTEM_SERVICE_STUB(sceShellCoreUtilUnmountDownloadDataForShellUI)
SYSTEM_SERVICE_STUB(sceShellCoreUtilUnmountHddForBackup)
SYSTEM_SERVICE_STUB(sceShellCoreUtilUnmountHddForRestore)
SYSTEM_SERVICE_STUB(sceShellCoreUtilWriteSmrHddDummyData)
SYSTEM_SERVICE_STUB(SystemService_Func_1E5CA5A71FA7F028)
SYSTEM_SERVICE_STUB(SystemService_Func_6D43644F75C38346)
SYSTEM_SERVICE_STUB(SystemService_Func_739FB849CB28F445)
SYSTEM_SERVICE_STUB(SystemService_Func_B20628FF35C74111)
SYSTEM_SERVICE_STUB(sceSystemServiceAddLocalProcessForJvm)
SYSTEM_SERVICE_STUB(sceSystemServiceGetParentSocketForJvm)
SYSTEM_SERVICE_STUB(sceSystemServiceKillLocalProcessForJvm)
SYSTEM_SERVICE_STUB(sceSystemServiceAcquireFb0)
SYSTEM_SERVICE_STUB(sceSystemServiceAddLocalProcess)
SYSTEM_SERVICE_STUB(sceSystemServiceAddLocalProcessForPsmKit)
SYSTEM_SERVICE_STUB(sceSystemServiceChangeAcpClock)
SYSTEM_SERVICE_STUB(sceSystemServiceChangeCpuClock)
SYSTEM_SERVICE_STUB(sceSystemServiceChangeGpuClock)
SYSTEM_SERVICE_STUB(sceSystemServiceChangeMemoryClock)
SYSTEM_SERVICE_STUB(sceSystemServiceChangeMemoryClockToBaseMode)
SYSTEM_SERVICE_STUB(sceSystemServiceChangeMemoryClockToDefault)
SYSTEM_SERVICE_STUB(sceSystemServiceChangeMemoryClockToMultiMediaMode)
SYSTEM_SERVICE_STUB(sceSystemServiceChangeNumberOfGpuCu)
SYSTEM_SERVICE_STUB(sceSystemServiceChangeSamuClock)
SYSTEM_SERVICE_STUB(sceSystemServiceChangeUvdClock)
SYSTEM_SERVICE_STUB(sceSystemServiceChangeVceClock)
SYSTEM_SERVICE_STUB(sceSystemServiceDisableMusicPlayer)
SYSTEM_SERVICE_STUB(sceSystemServiceDisablePersonalEyeToEyeDistanceSetting)
SYSTEM_SERVICE_STUB(sceSystemServiceDisableSuspendConfirmationDialog)
SYSTEM_SERVICE_STUB(sceSystemServiceEnablePersonalEyeToEyeDistanceSetting)
SYSTEM_SERVICE_STUB(sceSystemServiceEnableSuspendConfirmationDialog)
SYSTEM_SERVICE_STUB(sceSystemServiceGetAppFocusedAppStatus)
SYSTEM_SERVICE_STUB(sceSystemServiceGetAppIdOfBigApp)
SYSTEM_SERVICE_STUB(sceSystemServiceGetAppIdOfMiniApp)
SYSTEM_SERVICE_STUB(sceSystemServiceGetAppStatus)
SYSTEM_SERVICE_STUB(sceSystemServiceGetAppType)
SYSTEM_SERVICE_STUB(sceSystemServiceGetEventForDaemon)
SYSTEM_SERVICE_STUB(sceSystemServiceGetGpuLoadEmulationMode)
SYSTEM_SERVICE_STUB(sceSystemServiceGetLocalProcessStatusList)
SYSTEM_SERVICE_STUB(sceSystemServiceGetParentSocket)
SYSTEM_SERVICE_STUB(sceSystemServiceGetParentSocketForPsmKit)
SYSTEM_SERVICE_STUB(sceSystemServiceGetPSButtonEvent)
SYSTEM_SERVICE_STUB(sceSystemServiceGetRenderingMode)
SYSTEM_SERVICE_STUB(sceSystemServiceGetTitleWorkaroundInfo)
SYSTEM_SERVICE_STUB(sceSystemServiceGetVersionNumberOfCameraCalibrationData)
SYSTEM_SERVICE_STUB(sceSystemServiceIsAppSuspended)
SYSTEM_SERVICE_STUB(sceSystemServiceIsEyeToEyeDistanceAdjusted)
SYSTEM_SERVICE_STUB(sceSystemServiceIsScreenSaverOn)
SYSTEM_SERVICE_STUB(sceSystemServiceIsShellUiFgAndGameBgCpuMode)
SYSTEM_SERVICE_STUB(sceSystemServiceKillApp)
SYSTEM_SERVICE_STUB(sceSystemServiceKillLocalProcess)
SYSTEM_SERVICE_STUB(sceSystemServiceKillLocalProcessForPsmKit)
SYSTEM_SERVICE_STUB(sceSystemServiceLaunchApp)
SYSTEM_SERVICE_STUB(sceSystemServiceLaunchEventDetails)
SYSTEM_SERVICE_STUB(sceSystemServiceLaunchTournamentList)
SYSTEM_SERVICE_STUB(sceSystemServiceLaunchTournamentsTeamProfile)
SYSTEM_SERVICE_STUB(sceSystemServiceLaunchWebBrowser)
SYSTEM_SERVICE_STUB(sceSystemServiceNavigateToAnotherApp)
SYSTEM_SERVICE_STUB(sceSystemServiceNavigateToGoBack)
SYSTEM_SERVICE_STUB(sceSystemServiceNavigateToGoBackWithValue)
SYSTEM_SERVICE_STUB(sceSystemServiceNavigateToGoHome)
SYSTEM_SERVICE_STUB(sceSystemServiceParamGetString)
SYSTEM_SERVICE_STUB(sceSystemServicePowerTick)
SYSTEM_SERVICE_STUB(sceSystemServiceRaiseExceptionLocalProcess)
SYSTEM_SERVICE_STUB(sceSystemServiceReenableMusicPlayer)
SYSTEM_SERVICE_STUB(sceSystemServiceRegisterDaemon)
SYSTEM_SERVICE_STUB(sceSystemServiceReleaseFb0)
SYSTEM_SERVICE_STUB(sceSystemServiceReportAbnormalTermination)
SYSTEM_SERVICE_STUB(sceSystemServiceRequestCameraCalibration)
SYSTEM_SERVICE_STUB(sceSystemServiceRequestToChangeRenderingMode)
SYSTEM_SERVICE_STUB(sceSystemServiceResumeLocalProcess)
SYSTEM_SERVICE_STUB(sceSystemServiceSetControllerFocusPermission)
SYSTEM_SERVICE_STUB(sceSystemServiceSetGpuLoadEmulationMode)
SYSTEM_SERVICE_STUB(sceSystemServiceSetOutOfVrPlayAreaFlag)
SYSTEM_SERVICE_STUB(sceSystemServiceSetOutOfVrPlayZoneWarning)
SYSTEM_SERVICE_STUB(sceSystemServiceShowControllerSettings)
SYSTEM_SERVICE_STUB(sceSystemServiceShowDisplaySafeAreaSettings)
SYSTEM_SERVICE_STUB(sceSystemServiceShowEyeToEyeDistanceSetting)
SYSTEM_SERVICE_STUB(sceSystemServiceSuspendBackgroundApp)
SYSTEM_SERVICE_STUB(sceSystemServiceSuspendLocalProcess)
SYSTEM_SERVICE_STUB(sceSystemServiceTickVideoPlayback)
SYSTEM_SERVICE_STUB(sceSystemServiceTurnOffScreenSaver)
SYSTEM_SERVICE_STUB(SystemService_Func_9031A344CB540F1A)
SYSTEM_SERVICE_STUB(SystemService_Func_A9D4CF2568EAB837)
SYSTEM_SERVICE_STUB(sceSystemServiceLaunchWebApp)
SYSTEM_SERVICE_STUB(SystemService_Func_B8495C766861FDCF)
SYSTEM_SERVICE_STUB(sceSystemServiceGetDbgExecutablePath)
SYSTEM_SERVICE_STUB(sceSystemServiceActivateHevc)
SYSTEM_SERVICE_STUB(sceSystemServiceActivateHevcAbort)
SYSTEM_SERVICE_STUB(sceSystemServiceActivateHevcGetStatus)
SYSTEM_SERVICE_STUB(sceSystemServiceActivateHevcInit)
SYSTEM_SERVICE_STUB(sceSystemServiceActivateHevcIsActivated)
SYSTEM_SERVICE_STUB(sceSystemServiceActivateHevcStart)
SYSTEM_SERVICE_STUB(sceSystemServiceActivateHevcTerm)
SYSTEM_SERVICE_STUB(sceSystemServiceActivateMpeg2Abort)
SYSTEM_SERVICE_STUB(sceSystemServiceActivateMpeg2GetStatus)
SYSTEM_SERVICE_STUB(sceSystemServiceActivateMpeg2Init)
SYSTEM_SERVICE_STUB(sceSystemServiceActivateMpeg2IsActivated)
SYSTEM_SERVICE_STUB(sceSystemServiceActivateMpeg2Start)
SYSTEM_SERVICE_STUB(sceSystemServiceActivateMpeg2Term)
SYSTEM_SERVICE_STUB(sceSystemStateMgrCancelShutdownTimer)
SYSTEM_SERVICE_STUB(sceSystemStateMgrEnterMediaPlaybackMode)
SYSTEM_SERVICE_STUB(sceSystemStateMgrEnterStandby)
SYSTEM_SERVICE_STUB(sceSystemStateMgrExtendShutdownTimer)
SYSTEM_SERVICE_STUB(sceSystemStateMgrExtendShutdownTimerForPostAutoUpdateProcess)
SYSTEM_SERVICE_STUB(sceSystemStateMgrGetCurrentState)
SYSTEM_SERVICE_STUB(sceSystemStateMgrGetTriggerCode)
SYSTEM_SERVICE_STUB(sceSystemStateMgrIsBdDriveReady)
SYSTEM_SERVICE_STUB(sceSystemStateMgrIsGpuPerformanceNormal)
SYSTEM_SERVICE_STUB(sceSystemStateMgrIsShellUIShutdownInProgress)
SYSTEM_SERVICE_STUB(sceSystemStateMgrIsStandbyModeEnabled)
SYSTEM_SERVICE_STUB(sceSystemStateMgrLeaveMediaPlaybackMode)
SYSTEM_SERVICE_STUB(sceSystemStateMgrNotifySystemSuspendResumeProgress)
SYSTEM_SERVICE_STUB(sceSystemStateMgrReboot)
SYSTEM_SERVICE_STUB(sceSystemStateMgrSendCecOneTouchPlayCommand)
SYSTEM_SERVICE_STUB(sceSystemStateMgrStartRebootTimer)
SYSTEM_SERVICE_STUB(sceSystemStateMgrStartShutdownTimer)
SYSTEM_SERVICE_STUB(sceSystemStateMgrStartStadbyTimer)
SYSTEM_SERVICE_STUB(sceSystemStateMgrStartVshAutoUpdateTimer)
SYSTEM_SERVICE_STUB(sceSystemStateMgrTickMusicPlayback)
SYSTEM_SERVICE_STUB(sceSystemStateMgrTickPartyChat)
SYSTEM_SERVICE_STUB(sceSystemStateMgrTurnOff)
SYSTEM_SERVICE_STUB(sceSystemStateMgrVshAutoUpdate)
SYSTEM_SERVICE_STUB(sceSystemStateMgrWaitVshAutoUpdateVerifyDone)
SYSTEM_SERVICE_STUB(sceSystemStateMgrWakeUp)
SYSTEM_SERVICE_STUB(SystemService_Func_89F262179C22B49E)
SYSTEM_SERVICE_STUB(SystemService_Func_AC8A8FAB4A1696B8)
SYSTEM_SERVICE_STUB(sceSystemServiceInvokeAppLaunchLink)
SYSTEM_SERVICE_STUB(sceSystemServiceShowClosedCaptionAdvancedSettings)
SYSTEM_SERVICE_STUB(sceSystemServiceShowClosedCaptionSettings)
SYSTEM_SERVICE_STUB(sceSystemServiceSetPowerSaveLevel)
SYSTEM_SERVICE_STUB(sceSystemServiceInitializeForShellCore)
SYSTEM_SERVICE_STUB(SystemService_Func_7C1183FC73629929)
SYSTEM_SERVICE_STUB(sceSystemServiceDisablePartyVoice)
SYSTEM_SERVICE_STUB(sceSystemServiceReenablePartyVoice)
SYSTEM_SERVICE_STUB(sceSystemServiceGetPlatformPrivacyDefinitionData)
SYSTEM_SERVICE_STUB(sceSystemServiceGetPlatformPrivacyDefinitionVersion)
SYSTEM_SERVICE_STUB(sceSystemServiceGetPlatformPrivacySetting)
SYSTEM_SERVICE_STUB(sceSystemServiceDeclareReadyForSuspend)
SYSTEM_SERVICE_STUB(sceSystemServiceDisableSuspendNotification)
SYSTEM_SERVICE_STUB(sceSystemServiceEnableSuspendNotification)
SYSTEM_SERVICE_STUB(sceSystemServiceRequestPowerOff)
SYSTEM_SERVICE_STUB(sceSystemServiceRequestReboot)
SYSTEM_SERVICE_STUB(sceSystemServiceAddLocalProcessForPs2Emu)
SYSTEM_SERVICE_STUB(sceSystemServiceGetParentSocketForPs2Emu)
SYSTEM_SERVICE_STUB(sceSystemServiceKillLocalProcessForPs2Emu)
SYSTEM_SERVICE_STUB(sceSystemServiceShowImposeMenuForPs2Emu)
SYSTEM_SERVICE_STUB(sceSystemServiceSaveVideoToken)
SYSTEM_SERVICE_STUB(sceSystemServiceLaunchStore)
SYSTEM_SERVICE_STUB(sceSystemServiceTelemetrySetData)
SYSTEM_SERVICE_STUB(SystemService_Func_C67FC780F5B6F71E)
SYSTEM_SERVICE_STUB(sceSystemServiceLaunchUdsApp)
SYSTEM_SERVICE_STUB(sceSystemServiceLoadExecVideoServiceWebApp)
SYSTEM_SERVICE_STUB(sceSystemServiceDisableVoiceRecognition)
SYSTEM_SERVICE_STUB(sceSystemServiceReenableVoiceRecognition)
SYSTEM_SERVICE_STUB(SystemService_Func_6B1CDB955F0EBD65)
SYSTEM_SERVICE_STUB(SystemService_Func_CB5E885E225F69F0)
