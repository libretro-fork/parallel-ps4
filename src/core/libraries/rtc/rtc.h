/* SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef SHADPS4_LIBRARIES_RTC_H
#define SHADPS4_LIBRARIES_RTC_H

/* libSceRtc: the game's real-time clock, dates and ticks. Registered in sysmodule_internal.cpp. */

#include <retro_common_api.h>

#include "common/types.h"

RETRO_BEGIN_DECLS

#define ORBIS_RTC_DAYOFWEEK_SUNDAY ((int32_t)0)
#define ORBIS_RTC_DAYOFWEEK_MONDAY ((int32_t)1)
#define ORBIS_RTC_DAYOFWEEK_TUESDAY ((int32_t)2)
#define ORBIS_RTC_DAYOFWEEK_WEDNESDAY ((int32_t)3)
#define ORBIS_RTC_DAYOFWEEK_THURSDAY ((int32_t)4)
#define ORBIS_RTC_DAYOFWEEK_FRIDAY ((int32_t)5)
#define ORBIS_RTC_DAYOFWEEK_SATURDAY ((int32_t)6)

#define ORBIS_RTC_CLOCK_ID_NETWORK ((int32_t)0x10)
#define ORBIS_RTC_CLOCK_ID_DEBUG_NETWORK ((int32_t)0x11)
#define ORBIS_RTC_CLOCK_ID_AD_NETWORK ((int32_t)0x12)

#define UNIX_EPOCH_TICKS ((int64_t)0xdcbffeff2bc000)
#define WIN32_FILETIME_EPOCH_TICKS ((int64_t)0xb36168b6a58000)

typedef struct OrbisRtcTick {
    u64 tick;
} OrbisRtcTick;

typedef struct OrbisRtcDateTime {
    u16 year;
    u16 month;
    u16 day;
    u16 hour;
    u16 minute;
    u16 second;
    u32 microsecond;
} OrbisRtcDateTime;

int PS4_SYSV_ABI sceRtcCheckValid(OrbisRtcDateTime* pTime);
int PS4_SYSV_ABI sceRtcCompareTick(OrbisRtcTick* pTick1, OrbisRtcTick* pTick2);
int PS4_SYSV_ABI sceRtcConvertLocalTimeToUtc(OrbisRtcTick* pTickLocal, OrbisRtcTick* pTickUtc);
int PS4_SYSV_ABI sceRtcConvertUtcToLocalTime(OrbisRtcTick* pTickUtc, OrbisRtcTick* pTickLocal);
int PS4_SYSV_ABI sceRtcEnd(void);
int PS4_SYSV_ABI sceRtcFormatRFC2822(char* pszDateTime, const OrbisRtcTick* pTickUtc, int minutes);
int PS4_SYSV_ABI sceRtcFormatRFC2822LocalTime(char* pszDateTime, const OrbisRtcTick* pTickUtc);
int PS4_SYSV_ABI sceRtcFormatRFC3339(char* pszDateTime, const OrbisRtcTick* pTickUtc, int minutes);
int PS4_SYSV_ABI sceRtcFormatRFC3339LocalTime(char* pszDateTime, const OrbisRtcTick* pTickUtc);
int PS4_SYSV_ABI sceRtcFormatRFC3339Precise(char* pszDateTime, const OrbisRtcTick* pTickUtc,
                                            int minutes);
int PS4_SYSV_ABI sceRtcFormatRFC3339PreciseLocalTime(char* pszDateTime,
                                                     const OrbisRtcTick* pTickUtc);
int PS4_SYSV_ABI sceRtcGetCurrentAdNetworkTick(OrbisRtcTick* pTick);
int PS4_SYSV_ABI sceRtcGetCurrentClock(OrbisRtcDateTime* pTime, int timeZone);
int PS4_SYSV_ABI sceRtcGetCurrentClockLocalTime(OrbisRtcDateTime* pTime);
int PS4_SYSV_ABI sceRtcGetCurrentDebugNetworkTick(OrbisRtcTick* pTick);
int PS4_SYSV_ABI sceRtcGetCurrentNetworkTick(OrbisRtcTick* pTick);
int PS4_SYSV_ABI sceRtcGetCurrentRawNetworkTick(OrbisRtcTick* pTick);
int PS4_SYSV_ABI sceRtcGetCurrentTick(OrbisRtcTick* pTick);
int PS4_SYSV_ABI sceRtcGetDayOfWeek(int year, int month, int day);
int PS4_SYSV_ABI sceRtcGetDaysInMonth(int year, int month);
int PS4_SYSV_ABI sceRtcGetDosTime(OrbisRtcDateTime* pTime, u32* dosTime);
int PS4_SYSV_ABI sceRtcGetTick(OrbisRtcDateTime* pTime, OrbisRtcTick* pTick);
u32 PS4_SYSV_ABI sceRtcGetTickResolution(void);
int PS4_SYSV_ABI sceRtcGetTime_t(OrbisRtcDateTime* pTime, s64* llTime);
int PS4_SYSV_ABI sceRtcGetWin32FileTime(OrbisRtcDateTime* pTime, uint64_t* ulWin32Time);
int PS4_SYSV_ABI sceRtcInit(void);
int PS4_SYSV_ABI sceRtcIsLeapYear(int yearInt);
int PS4_SYSV_ABI sceRtcParseDateTime(OrbisRtcTick* pTickUtc, const char* pszDateTime);
int PS4_SYSV_ABI sceRtcParseRFC3339(OrbisRtcTick* pTickUtc, const char* pszDateTime);
void PS4_SYSV_ABI sceRtcSetConf(void* p1, void* p2, s32 minuteswest, s32 dsttime);
int PS4_SYSV_ABI sceRtcSetCurrentAdNetworkTick(OrbisRtcTick* pTick);
int PS4_SYSV_ABI sceRtcSetCurrentDebugNetworkTick(OrbisRtcTick* pTick);
int PS4_SYSV_ABI sceRtcSetCurrentNetworkTick(OrbisRtcTick* pTick);
int PS4_SYSV_ABI sceRtcSetCurrentTick(OrbisRtcTick* pTick);
int PS4_SYSV_ABI sceRtcSetDosTime(OrbisRtcDateTime* pTime, u32 dosTime);
int PS4_SYSV_ABI sceRtcSetTick(OrbisRtcDateTime* pTime, OrbisRtcTick* pTick);
int PS4_SYSV_ABI sceRtcSetTime_t(OrbisRtcDateTime* pTime, s64 llTime);
int PS4_SYSV_ABI sceRtcSetWin32FileTime(OrbisRtcDateTime* pTime, int64_t ulWin32Time);
int PS4_SYSV_ABI sceRtcTickAddDays(OrbisRtcTick* pTick1, OrbisRtcTick* pTick2, int32_t lAdd);
int PS4_SYSV_ABI sceRtcTickAddHours(OrbisRtcTick* pTick1, OrbisRtcTick* pTick2, int32_t lAdd);
int PS4_SYSV_ABI sceRtcTickAddMicroseconds(OrbisRtcTick* pTick1, OrbisRtcTick* pTick2,
                                           int64_t lAdd);
int PS4_SYSV_ABI sceRtcTickAddMinutes(OrbisRtcTick* pTick1, OrbisRtcTick* pTick2, int64_t lAdd);
int PS4_SYSV_ABI sceRtcTickAddMonths(OrbisRtcTick* pTick1, OrbisRtcTick* pTick2, int32_t lAdd);
int PS4_SYSV_ABI sceRtcTickAddSeconds(OrbisRtcTick* pTick1, OrbisRtcTick* pTick2, int64_t lAdd);
int PS4_SYSV_ABI sceRtcTickAddTicks(OrbisRtcTick* pTick1, OrbisRtcTick* pTick2, int64_t lAdd);
int PS4_SYSV_ABI sceRtcTickAddWeeks(OrbisRtcTick* pTick1, OrbisRtcTick* pTick2, int32_t lAdd);
int PS4_SYSV_ABI sceRtcTickAddYears(OrbisRtcTick* pTick1, OrbisRtcTick* pTick2, int32_t lAdd);

RETRO_END_DECLS

#endif
