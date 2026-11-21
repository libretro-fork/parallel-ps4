/* SPDX-FileCopyrightText: Copyright 2024-2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <ctype.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include <time/rtime.h>

#include "common/c_bridge.h"
#include "core/libraries/error_codes.h"
#include "core/libraries/kernel/kernel_c.h"
#include "core/libraries/rtc/rtc.h"
#include "core/libraries/rtc/rtc_error.h"

/* Microseconds in a minute, an hour, a day and a week, signed as the C++ literals were */
#define RTC_MINUTE_US ((s64)60 * 1000000)
#define RTC_HOUR_US ((s64)3600 * 1000000)
#define RTC_DAY_US ((s64)86400 * 1000000)
#define RTC_WEEK_US ((s64)604800 * 1000000)

/* The character at @pos of a string @len long, or 0 past its end */
static char rtc_char(const char* s, size_t len, size_t pos) {
    return pos < len ? s[pos] : '\0';
}

/* std::stoi of the @n characters at @pos: spaces skipped, a sign, the digits. 0 where there is
 * no number (std::stoi threw, and the emulator stopped). */
static int rtc_number(const char* s, size_t len, size_t pos, size_t n) {
    size_t end = pos + n < len ? pos + n : len;
    int sign = 1;
    int value = 0;
    if (pos >= len) {
        return 0;
    }
    while (pos < end && isspace((unsigned char)s[pos])) {
        pos++;
    }
    if (pos < end && (s[pos] == '+' || s[pos] == '-')) {
        sign = s[pos] == '-' ? -1 : 1;
        pos++;
    }
    while (pos < end && s[pos] >= '0' && s[pos] <= '9') {
        value = value * 10 + (s[pos] - '0');
        pos++;
    }
    return sign * value;
}

int PS4_SYSV_ABI sceRtcCheckValid(OrbisRtcDateTime* pTime) {
    c_log(C_LOG_LIB_RTC, C_LOG_LEVEL_TRACE, "sceRtcCheckValid", "called");

    if (pTime == NULL)
        return ORBIS_RTC_ERROR_INVALID_POINTER;

    if (pTime->year == 0 || pTime->year > 9999)
        return ORBIS_RTC_ERROR_INVALID_YEAR;

    if (pTime->month == 0 || pTime->month > 12)
        return ORBIS_RTC_ERROR_INVALID_MONTH;

    if (pTime->day == 0)
        return ORBIS_RTC_ERROR_INVALID_DAY;

    if (pTime->day > (int)rtime_days_in_month(pTime->year, pTime->month))
        return ORBIS_RTC_ERROR_INVALID_DAY;

    if (pTime->hour >= 24)
        return ORBIS_RTC_ERROR_INVALID_HOUR;

    if (pTime->minute >= 60)
        return ORBIS_RTC_ERROR_INVALID_MINUTE;

    if (pTime->second >= 60)
        return ORBIS_RTC_ERROR_INVALID_SECOND;

    if (pTime->microsecond >= 1000000)
        return ORBIS_RTC_ERROR_INVALID_MICROSECOND;

    return ORBIS_OK;
}

int PS4_SYSV_ABI sceRtcCompareTick(OrbisRtcTick* pTick1, OrbisRtcTick* pTick2) {
    c_log(C_LOG_LIB_RTC, C_LOG_LEVEL_TRACE, "sceRtcCompareTick", "called");

    if (pTick1 == NULL || pTick2 == NULL)
        return ORBIS_RTC_ERROR_INVALID_POINTER;

    if (pTick1->tick <= pTick2->tick)
        return 1;
    else
        return 0;

    return ORBIS_FAIL;
}

int PS4_SYSV_ABI sceRtcConvertLocalTimeToUtc(OrbisRtcTick* pTickLocal, OrbisRtcTick* pTickUtc) {
    s64 seconds;
    kernel_timezone_t timezone;
    int convertValue;
    c_log(C_LOG_LIB_RTC, C_LOG_LEVEL_TRACE, "sceRtcConvertLocalTimeToUtc", "called");

    if (pTickLocal == NULL)
        return ORBIS_RTC_ERROR_INVALID_POINTER;

    convertValue =
        kernel_convert_localtime_to_utc((s64)((pTickLocal->tick - UNIX_EPOCH_TICKS) / 1000000),
                                        0xffffffff, &seconds, &timezone, NULL);

    if (convertValue >= 0) {
        convertValue = sceRtcTickAddMinutes(pTickUtc, pTickLocal,
                                            -(timezone.tz_minuteswest + timezone.tz_dsttime));
    }

    return convertValue;
}

int PS4_SYSV_ABI sceRtcConvertUtcToLocalTime(OrbisRtcTick* pTickUtc, OrbisRtcTick* pTickLocal) {
    kernel_timezone_t timeZone;
    int returnValue;
    c_log(C_LOG_LIB_RTC, C_LOG_LEVEL_TRACE, "sceRtcConvertUtcToLocalTime", "called");

    if (pTickUtc == NULL)
        return ORBIS_RTC_ERROR_INVALID_POINTER;

    returnValue = kernel_gettimezone(&timeZone);

    sceRtcTickAddMinutes(pTickLocal, pTickUtc, timeZone.tz_minuteswest + timeZone.tz_dsttime);

    return 0;
}

int PS4_SYSV_ABI sceRtcEnd(void) {
    return ORBIS_OK;
}

int PS4_SYSV_ABI sceRtcFormatRFC2822(char* pszDateTime, const OrbisRtcTick* pTickUtc,
                                     int iTimeZoneMinutes) {
    OrbisRtcTick formatTick;
    OrbisRtcDateTime formatTime;
    int validTime;
    static const char* const week_days[7] = {"Sun, ", "Mon, ", "Tue, ", "Wed, ",
                                             "Thu, ", "Fri, ", "Sat, "};
    static const char* const months[12] = {"Jan ", "Feb ", "Mar ", "Apr ", "May ", "Jun ",
                                           "Jul ", "Aug ", "Sep ", "Oct ", "Nov ", "Dec "};
    c_log(C_LOG_LIB_RTC, C_LOG_LEVEL_TRACE, "sceRtcFormatRFC2822", "called");

    if (pszDateTime == NULL)
        return ORBIS_RTC_ERROR_INVALID_POINTER;

    if (pTickUtc == NULL) {
        sceRtcGetCurrentTick(&formatTick);
    } else {
        formatTick.tick = pTickUtc->tick;
    }

    sceRtcTickAddMinutes(&formatTick, &formatTick, iTimeZoneMinutes);

    sceRtcSetTick(&formatTime, &formatTick);

    validTime = sceRtcCheckValid(&formatTime);

    if (validTime >= 0) {
        /* Built as before: day and time two digits, the year and the zone's minutes as they
         * come, the whole written with its terminator */
        char out[64];
        char* p = out;
        int weekDay = sceRtcGetDayOfWeek(formatTime.year, formatTime.month, formatTime.day);
        *p = '\0';
        if (weekDay >= 0 && weekDay < 7) {
            p += sprintf(p, "%s", week_days[weekDay]);
        }
        p += sprintf(p, "%02u ", (unsigned)formatTime.day);
        if (formatTime.month >= 1 && formatTime.month <= 12) {
            p += sprintf(p, "%s", months[formatTime.month - 1]);
        }
        p += sprintf(p, "%u %02u:%02u:%02u ", (unsigned)formatTime.year, (unsigned)formatTime.hour,
                     (unsigned)formatTime.minute, (unsigned)formatTime.second);

        if (iTimeZoneMinutes == 0) {
            p += sprintf(p, "+0000");
        } else {
            int timeZoneHours = iTimeZoneMinutes / 60;
            int timeZoneRemainder = iTimeZoneMinutes % 60;

            if (timeZoneHours < 0) {
                p += sprintf(p, "-");
                timeZoneHours *= -1;
            } else {
                p += sprintf(p, "+");
            }

            p += sprintf(p, "%02d", timeZoneHours);

            if (timeZoneRemainder == 0) {
                p += sprintf(p, "00");
            } else {
                if (timeZoneRemainder < 0)
                    timeZoneRemainder *= -1;
                p += sprintf(p, "%d", timeZoneRemainder);
            }
        }

        memcpy(pszDateTime, out, (size_t)(p - out) + 1);
    }

    return ORBIS_OK;
}

int PS4_SYSV_ABI sceRtcFormatRFC2822LocalTime(char* pszDateTime, const OrbisRtcTick* pTickUtc) {
    kernel_timezone_t timeZone;
    c_log(C_LOG_LIB_RTC, C_LOG_LEVEL_TRACE, "sceRtcFormatRFC2822LocalTime", "called");

    kernel_gettimezone(&timeZone);

    return sceRtcFormatRFC2822(pszDateTime, pTickUtc,
                               timeZone.tz_minuteswest + timeZone.tz_dsttime);
}

int PS4_SYSV_ABI sceRtcFormatRFC3339(char* pszDateTime, const OrbisRtcTick* pTickUtc,
                                     int iTimeZoneMinutes) {
    c_log(C_LOG_LIB_RTC, C_LOG_LEVEL_TRACE, "sceRtcFormatRFC3339", "called");
    return sceRtcFormatRFC3339Precise(pszDateTime, pTickUtc, iTimeZoneMinutes);
}

int PS4_SYSV_ABI sceRtcFormatRFC3339LocalTime(char* pszDateTime, const OrbisRtcTick* pTickUtc) {
    kernel_timezone_t timeZone;
    c_log(C_LOG_LIB_RTC, C_LOG_LEVEL_TRACE, "sceRtcFormatRFC3339LocalTime", "called");

    kernel_gettimezone(&timeZone);

    return sceRtcFormatRFC3339(pszDateTime, pTickUtc,
                               timeZone.tz_minuteswest + timeZone.tz_dsttime);
}

int PS4_SYSV_ABI sceRtcFormatRFC3339Precise(char* pszDateTime, const OrbisRtcTick* pTickUtc,
                                            int iTimeZoneMinutes) {
    OrbisRtcTick formatTick;
    OrbisRtcDateTime formatTime;
    char out[64];
    char* p = out;
    c_log(C_LOG_LIB_RTC, C_LOG_LEVEL_TRACE, "sceRtcFormatRFC3339Precise", "called");

    if (pszDateTime == NULL)
        return ORBIS_RTC_ERROR_INVALID_POINTER;

    if (pTickUtc == NULL) {
        sceRtcGetCurrentTick(&formatTick);
    } else {
        formatTick.tick = pTickUtc->tick;
    }

    sceRtcTickAddMinutes(&formatTick, &formatTick, iTimeZoneMinutes);

    sceRtcSetTick(&formatTime, &formatTick);

    p += sprintf(p, "%u-%02u-%02uT%02u:%02u:%02u", (unsigned)formatTime.year,
                 (unsigned)formatTime.month, (unsigned)formatTime.day, (unsigned)formatTime.hour,
                 (unsigned)formatTime.minute, (unsigned)formatTime.second);

    if (formatTime.microsecond != 0) {
        /* The first two digits of the milliseconds, as they come */
        char ms[16];
        sprintf(ms, "%u", (unsigned)(formatTime.microsecond / 1000));
        ms[2] = ms[0] && ms[1] ? '\0' : ms[2];
        p += sprintf(p, ".%.2s", ms);
    } else {
        p += sprintf(p, ".00");
    }

    if (iTimeZoneMinutes == 0) {
        p += sprintf(p, "Z");
    } else {
        int timeZoneHours = iTimeZoneMinutes / 60;
        int timeZoneRemainder = iTimeZoneMinutes % 60;

        if (timeZoneHours < 0) {
            p += sprintf(p, "-");
            timeZoneHours *= -1;
        } else {
            p += sprintf(p, "+");
        }

        p += sprintf(p, "%02d", timeZoneHours);

        if (timeZoneRemainder == 0) {
            p += sprintf(p, ":00");
        } else {
            if (timeZoneRemainder < 0)
                timeZoneRemainder *= -1;
            p += sprintf(p, ":%d", timeZoneRemainder);
        }
    }

    memcpy(pszDateTime, out, (size_t)(p - out) + 1);

    return ORBIS_OK;
}

int PS4_SYSV_ABI sceRtcFormatRFC3339PreciseLocalTime(char* pszDateTime,
                                                     const OrbisRtcTick* pTickUtc) {
    kernel_timezone_t timeZone;
    c_log(C_LOG_LIB_RTC, C_LOG_LEVEL_TRACE, "sceRtcFormatRFC3339PreciseLocalTime", "called");

    kernel_gettimezone(&timeZone);

    return sceRtcFormatRFC3339Precise(pszDateTime, pTickUtc,
                                      timeZone.tz_minuteswest + timeZone.tz_dsttime);
}

int PS4_SYSV_ABI sceRtcGetCurrentAdNetworkTick(OrbisRtcTick* pTick) {
    kernel_timespec_t clocktime;
    int returnValue;
    c_log(C_LOG_LIB_RTC, C_LOG_LEVEL_TRACE, "sceRtcGetCurrentAdNetworkTick", "called");

    if (pTick == NULL)
        return ORBIS_RTC_ERROR_INVALID_POINTER;

    returnValue = kernel_clock_gettime(KERNEL_CLOCK_REALTIME, &clocktime);

    if (returnValue == ORBIS_OK) {
        pTick->tick = clocktime.tv_nsec / 1000 + clocktime.tv_sec * 1000000 + UNIX_EPOCH_TICKS;
    } else {
        return ORBIS_RTC_ERROR_NOT_INITIALIZED;
    }

    return ORBIS_OK;
}

int PS4_SYSV_ABI sceRtcGetCurrentClock(OrbisRtcDateTime* pTime, int timeZone) {
    kernel_timespec_t clocktime;
    int returnValue;
    c_log(C_LOG_LIB_RTC, C_LOG_LEVEL_TRACE, "sceRtcGetCurrentClock", "called");

    if (pTime == NULL)
        return ORBIS_RTC_ERROR_DATETIME_UNINITIALIZED;

    returnValue = kernel_clock_gettime(KERNEL_CLOCK_REALTIME, &clocktime);

    if (returnValue == ORBIS_OK) {
        OrbisRtcTick clockTick;
        clockTick.tick = clocktime.tv_nsec / 1000 + clocktime.tv_sec * 1000000 + UNIX_EPOCH_TICKS;

        sceRtcTickAddMinutes(&clockTick, &clockTick, timeZone);
        sceRtcSetTick(pTime, &clockTick);
    }

    return returnValue;
}

int PS4_SYSV_ABI sceRtcGetCurrentClockLocalTime(OrbisRtcDateTime* pTime) {
    kernel_timezone_t timeZone;
    int returnValue;
    c_log(C_LOG_LIB_RTC, C_LOG_LEVEL_TRACE, "sceRtcGetCurrentClockLocalTime", "called");

    if (pTime == NULL)
        return ORBIS_RTC_ERROR_DATETIME_UNINITIALIZED;

    returnValue = kernel_gettimezone(&timeZone);

    if (returnValue >= 0) {
        kernel_timespec_t clocktime;

        /* calculate total timezone offset for converting UTC to local time */
        int tzOffset = timeZone.tz_minuteswest + timeZone.tz_dsttime;

        if (returnValue >= 0) {
            OrbisRtcTick newTick;
            sceRtcGetCurrentTick(&newTick);
            sceRtcTickAddMinutes(&newTick, &newTick, tzOffset);
            sceRtcSetTick(pTime, &newTick);
        }
    }

    return returnValue;
}

int PS4_SYSV_ABI sceRtcGetCurrentDebugNetworkTick(OrbisRtcTick* pTick) {
    kernel_timespec_t clocktime;
    int returnValue;
    c_log(C_LOG_LIB_RTC, C_LOG_LEVEL_TRACE, "sceRtcGetCurrentDebugNetworkTick", "called");

    if (pTick == NULL)
        return ORBIS_RTC_ERROR_INVALID_POINTER;

    returnValue = kernel_clock_gettime(KERNEL_CLOCK_REALTIME, &clocktime);

    if (returnValue == ORBIS_OK) {
        pTick->tick = clocktime.tv_nsec / 1000 + clocktime.tv_sec * 1000000 + UNIX_EPOCH_TICKS;
    } else {
        return ORBIS_RTC_ERROR_NOT_INITIALIZED;
    }

    return ORBIS_OK;
}

int PS4_SYSV_ABI sceRtcGetCurrentNetworkTick(OrbisRtcTick* pTick) {
    kernel_timespec_t clocktime;
    int returnValue;
    c_log(C_LOG_LIB_RTC, C_LOG_LEVEL_TRACE, "sceRtcGetCurrentNetworkTick", "called");

    if (pTick == NULL)
        return ORBIS_RTC_ERROR_INVALID_POINTER;

    returnValue = kernel_clock_gettime(KERNEL_CLOCK_REALTIME, &clocktime);

    if (returnValue == ORBIS_OK) {
        pTick->tick = clocktime.tv_nsec / 1000 + clocktime.tv_sec * 1000000 + UNIX_EPOCH_TICKS;
    } else {
        return ORBIS_RTC_ERROR_NOT_INITIALIZED;
    }

    return ORBIS_OK;
}

int PS4_SYSV_ABI sceRtcGetCurrentRawNetworkTick(OrbisRtcTick* pTick) {
    kernel_timespec_t clocktime;
    int returnValue;
    c_log(C_LOG_LIB_RTC, C_LOG_LEVEL_TRACE, "sceRtcGetCurrentRawNetworkTick", "called");

    if (pTick == NULL)
        return ORBIS_RTC_ERROR_INVALID_POINTER;

    returnValue = kernel_clock_gettime(KERNEL_CLOCK_REALTIME, &clocktime);

    if (returnValue == ORBIS_OK) {
        pTick->tick = clocktime.tv_nsec / 1000 + clocktime.tv_sec * 1000000 + UNIX_EPOCH_TICKS;
    } else {
        return ORBIS_RTC_ERROR_NOT_INITIALIZED;
    }

    return ORBIS_OK;
}

int PS4_SYSV_ABI sceRtcGetCurrentTick(OrbisRtcTick* pTick) {
    kernel_timespec_t clocktime;
    int returnValue;
    c_log(C_LOG_LIB_RTC, C_LOG_LEVEL_TRACE, "sceRtcGetCurrentTick", "called");

    if (pTick == NULL)
        return ORBIS_RTC_ERROR_DATETIME_UNINITIALIZED;

    returnValue = kernel_clock_gettime(KERNEL_CLOCK_REALTIME, &clocktime);

    if (returnValue >= 0) {
        pTick->tick = clocktime.tv_nsec / 1000 + clocktime.tv_sec * 1000000 + UNIX_EPOCH_TICKS;
    }

    return ORBIS_OK;
}

int PS4_SYSV_ABI sceRtcGetDayOfWeek(int year, int month, int day) {
    int sdk_version = 0;
    int sdkResult;
    int daysInMonth;
    c_log(C_LOG_LIB_RTC, C_LOG_LEVEL_TRACE, "sceRtcGetDayOfWeek", "called");

    sdkResult = kernel_get_compiled_sdk_version(&sdk_version);
    if (sdkResult != ORBIS_OK) {
        sdk_version = 0;
    }

    if (sdk_version < 0x3000000) {
        if (year < 1) {
            return ORBIS_RTC_ERROR_INVALID_YEAR;
        }
        if (month > 12 || month <= 0) {
            return ORBIS_RTC_ERROR_INVALID_MONTH;
        }
    } else {
        if (year > 9999 || year < 1) {
            return ORBIS_RTC_ERROR_INVALID_YEAR;
        }
        if (month > 12 || month <= 0) {
            return ORBIS_RTC_ERROR_INVALID_MONTH;
        }
    }

    daysInMonth = sceRtcGetDaysInMonth(year, month);

    if (day <= 0 || day > daysInMonth)
        return ORBIS_RTC_ERROR_INVALID_DAY;

    return (int)rtime_weekday(rtime_days_from_civil(year, (unsigned)month, (unsigned)day));
}

int PS4_SYSV_ABI sceRtcGetDaysInMonth(int year, int month) {
    c_log(C_LOG_LIB_RTC, C_LOG_LEVEL_TRACE, "sceRtcGetDaysInMonth", "called");

    if (year <= 0)
        return ORBIS_RTC_ERROR_INVALID_YEAR;

    if (month <= 0 || month > 12)
        return ORBIS_RTC_ERROR_INVALID_MONTH;

    return (int)rtime_days_in_month(year, month);
}

int PS4_SYSV_ABI sceRtcGetDosTime(OrbisRtcDateTime* pTime, u32* dosTime) {
    int isValid;
    c_log(C_LOG_LIB_RTC, C_LOG_LEVEL_TRACE, "sceRtcGetDosTime", "called");

    if (pTime == NULL || dosTime == NULL)
        return ORBIS_RTC_ERROR_INVALID_POINTER;

    isValid = sceRtcCheckValid(pTime);
    if (isValid != ORBIS_OK) {
        return isValid;
    }

    *dosTime |= (pTime->second / 2) & 0x1F;
    *dosTime |= (pTime->minute & 0x3F) << 5;
    *dosTime |= (pTime->hour & 0x1F) << 11;
    *dosTime |= (pTime->day & 0x1F) << 16;
    *dosTime |= (pTime->month & 0x0F) << 21;
    *dosTime |= ((pTime->year - 1980) & 0x7F) << 25;

    return ORBIS_OK;
}

int PS4_SYSV_ABI sceRtcGetTick(OrbisRtcDateTime* pTime, OrbisRtcTick* pTick) {
    int isTimeValid;
    int c;
    int ya;
    u64 days;
    u64 msec;
    c_log(C_LOG_LIB_RTC, C_LOG_LEVEL_TRACE, "sceRtcGetTick", "called");

    if (pTime == NULL || pTick == NULL)
        return ORBIS_RTC_ERROR_INVALID_POINTER;

    isTimeValid = sceRtcCheckValid(pTime);
    if (isTimeValid != 0)
        return isTimeValid;

    if (pTime->month > 2) {
        pTime->month -= 3;
    } else {
        pTime->month += 9;
        pTime->year -= 1;
    }

    c = pTime->year / 100;
    ya = pTime->year - 100 * c;

    days = ((146097 * c) >> 2) + ((1461 * ya) >> 2) + (153 * pTime->month + 2) / 5 + pTime->day;
    days -= 307;
    days *= RTC_DAY_US;

    msec = pTime->hour * RTC_HOUR_US + pTime->minute * RTC_MINUTE_US + pTime->second * 1000000 +
           pTime->microsecond;

    pTick->tick = days + msec;

    return ORBIS_OK;
}

u32 PS4_SYSV_ABI sceRtcGetTickResolution(void) {
    c_log(C_LOG_LIB_RTC, C_LOG_LEVEL_TRACE, "sceRtcGetTickResolution", "called");

    return 1000000;
}

int PS4_SYSV_ABI sceRtcGetTime_t(OrbisRtcDateTime* pTime, s64* llTime) {
    OrbisRtcTick timeTick;
    int isValid;
    c_log(C_LOG_LIB_RTC, C_LOG_LEVEL_TRACE, "sceRtcGetTime_t", "called");

    if (pTime == NULL || llTime == NULL)
        return ORBIS_RTC_ERROR_INVALID_POINTER;

    isValid = sceRtcCheckValid(pTime);
    if (isValid != ORBIS_OK) {
        return isValid;
    }

    sceRtcGetTick(pTime, &timeTick);

    if (timeTick.tick < UNIX_EPOCH_TICKS) {
        *llTime = 0;
    } else {
        *llTime = (s64)((timeTick.tick - UNIX_EPOCH_TICKS) / 1000000);
    }

    return ORBIS_OK;
}

int PS4_SYSV_ABI sceRtcGetWin32FileTime(OrbisRtcDateTime* pTime, uint64_t* ulWin32Time) {
    int isValid;
    OrbisRtcTick timeTick;
    c_log(C_LOG_LIB_RTC, C_LOG_LEVEL_TRACE, "sceRtcGetWin32FileTime", "called");

    if (pTime == NULL || ulWin32Time == NULL)
        return ORBIS_RTC_ERROR_INVALID_POINTER;

    isValid = sceRtcCheckValid(pTime);
    if (isValid != ORBIS_OK) {
        return isValid;
    }

    sceRtcGetTick(pTime, &timeTick);

    if (timeTick.tick < WIN32_FILETIME_EPOCH_TICKS) {
        *ulWin32Time = 0;
    } else {
        *ulWin32Time = (timeTick.tick - WIN32_FILETIME_EPOCH_TICKS) * 10;
    }

    return ORBIS_OK;
}

int PS4_SYSV_ABI sceRtcInit(void) {
    return ORBIS_OK;
}

int PS4_SYSV_ABI sceRtcIsLeapYear(int yearInt) {
    c_log(C_LOG_LIB_RTC, C_LOG_LEVEL_TRACE, "sceRtcIsLeapYear", "called");

    if (yearInt < 1)
        return ORBIS_RTC_ERROR_INVALID_YEAR;

    return rtime_is_leap_year(yearInt);
}

static int rtc_month_from_string(const char* s, size_t len, size_t pos) {
    int m;
    static const char names[12][4] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                                      "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
    if (pos + 3 > len) {
        return 1;
    }
    for (m = 0; m < 12; m++) {
        if (memcmp(s + pos, names[m], 3) == 0) {
            return m + 1;
        }
    }
    return 1;
}

int PS4_SYSV_ABI sceRtcParseDateTime(OrbisRtcTick* pTickUtc, const char* pszDateTime) {
    size_t len;
    char formatKey;
    OrbisRtcDateTime dateTime;
    c_log(C_LOG_LIB_RTC, C_LOG_LEVEL_TRACE, "sceRtcParseDateTime", "called");

    if (pTickUtc == NULL || pszDateTime == NULL)
        return ORBIS_RTC_ERROR_INVALID_POINTER;

    len = strlen(pszDateTime);
    formatKey = rtc_char(pszDateTime, len, 19);

    if (formatKey == 'Z' || formatKey == '-' || formatKey == '+' || formatKey == '.') {
        /* RFC3339 */
        sceRtcParseRFC3339(pTickUtc, pszDateTime);
    } else if (formatKey == ':') {
        /* RFC2822 */
        dateTime.day = (u16)rtc_number(pszDateTime, len, 5, 2);
        dateTime.month = (u16)rtc_month_from_string(pszDateTime, len, 8);
        dateTime.year = (u16)rtc_number(pszDateTime, len, 12, 4);
        dateTime.hour = (u16)rtc_number(pszDateTime, len, 17, 2);
        dateTime.minute = (u16)rtc_number(pszDateTime, len, 20, 2);
        dateTime.second = (u16)rtc_number(pszDateTime, len, 23, 2);
        dateTime.microsecond = 0;

        sceRtcGetTick(&dateTime, pTickUtc);

        if (rtc_char(pszDateTime, len, 26) == '+') {
            int timeZoneOffset = rtc_number(pszDateTime, len, 27, 2) * 60;
            timeZoneOffset += rtc_number(pszDateTime, len, 29, 2);
            sceRtcTickAddMinutes(pTickUtc, pTickUtc, timeZoneOffset);
        } else if (rtc_char(pszDateTime, len, 26) == '-') {
            int timeZoneOffset = rtc_number(pszDateTime, len, 27, 2) * 60;
            timeZoneOffset += rtc_number(pszDateTime, len, 29, 2);
            timeZoneOffset *= -1;
            sceRtcTickAddMinutes(pTickUtc, pTickUtc, timeZoneOffset);
        }

    } else {
        /* asctime */
        dateTime.month = (u16)rtc_month_from_string(pszDateTime, len, 4);
        dateTime.day = (u16)rtc_number(pszDateTime, len, 8, 2);
        dateTime.hour = (u16)rtc_number(pszDateTime, len, 11, 2);
        dateTime.minute = (u16)rtc_number(pszDateTime, len, 14, 2);
        dateTime.second = (u16)rtc_number(pszDateTime, len, 17, 2);
        dateTime.year = (u16)rtc_number(pszDateTime, len, 20, 4);
        dateTime.microsecond = 0;

        sceRtcGetTick(&dateTime, pTickUtc);
    }

    return ORBIS_OK;
}

int PS4_SYSV_ABI sceRtcParseRFC3339(OrbisRtcTick* pTickUtc, const char* pszDateTime) {
    size_t len;
    OrbisRtcDateTime dateTime;
    size_t timezone_pos = 22;
    char zone;
    c_log(C_LOG_LIB_RTC, C_LOG_LEVEL_TRACE, "sceRtcParseRFC3339", "called");

    if (pTickUtc == NULL || pszDateTime == NULL)
        return ORBIS_RTC_ERROR_INVALID_POINTER;

    len = strlen(pszDateTime);
    dateTime.year = (u16)rtc_number(pszDateTime, len, 0, 4);
    dateTime.month = (u16)rtc_number(pszDateTime, len, 5, 2);
    dateTime.day = (u16)rtc_number(pszDateTime, len, 8, 2);
    dateTime.hour = (u16)rtc_number(pszDateTime, len, 11, 2);
    dateTime.minute = (u16)rtc_number(pszDateTime, len, 14, 2);
    dateTime.second = (u16)rtc_number(pszDateTime, len, 17, 2);
    if (rtc_char(pszDateTime, len, 19) == '.') {
        dateTime.microsecond = (u32)rtc_number(pszDateTime, len, 20, 2);
    } else {
        timezone_pos = 19;
        dateTime.microsecond = 0;
    }

    sceRtcGetTick(&dateTime, pTickUtc);

    zone = rtc_char(pszDateTime, len, timezone_pos);
    if (zone != 'Z') {
        if (zone == '-') {
            int timeZoneOffset = rtc_number(pszDateTime, len, timezone_pos + 1, 2) * 60;
            timeZoneOffset += rtc_number(pszDateTime, len, timezone_pos + 4, 2);
            timeZoneOffset *= -1;
            sceRtcTickAddMinutes(pTickUtc, pTickUtc, timeZoneOffset);
        } else if (zone == '+') {
            int timeZoneOffset = rtc_number(pszDateTime, len, timezone_pos + 1, 2) * 60;
            timeZoneOffset += rtc_number(pszDateTime, len, timezone_pos + 4, 2);
            sceRtcTickAddMinutes(pTickUtc, pTickUtc, timeZoneOffset);
        }
    }

    return ORBIS_OK;
}

void PS4_SYSV_ABI sceRtcSetConf(void* p1, void* p2, s32 minuteswest, s32 dsttime) {
    kernel_timezone_t tz;
    c_log(C_LOG_LIB_RTC, C_LOG_LEVEL_INFO, "sceRtcSetConf", "called");
    tz.tz_minuteswest = minuteswest;
    tz.tz_dsttime = dsttime;
    kernel_settimeofday(NULL, &tz);
}

int PS4_SYSV_ABI sceRtcSetCurrentAdNetworkTick(OrbisRtcTick* pTick) {
    s32 ret = 0;
    c_log(C_LOG_LIB_RTC, C_LOG_LEVEL_INFO, "sceRtcSetCurrentAdNetworkTick", "called");
    if (UNIX_EPOCH_TICKS >= pTick->tick) {
        return ORBIS_RTC_ERROR_INVALID_VALUE;
    }
    if (pTick != NULL) {
        u64 temp = pTick->tick - UNIX_EPOCH_TICKS;
        kernel_timespec_t ts;
        ts.tv_sec = (s64)(temp / 1000000);
        ts.tv_nsec = (s64)(temp % 1000000);
        ret = kernel_posix_clock_settime(ORBIS_RTC_CLOCK_ID_AD_NETWORK, &ts);
    } else {
        ret = kernel_posix_clock_settime(ORBIS_RTC_CLOCK_ID_AD_NETWORK, NULL);
    }
    if (ret < 0) {
        return kernel_errno_error();
    }
    return ret;
}

int PS4_SYSV_ABI sceRtcSetCurrentDebugNetworkTick(OrbisRtcTick* pTick) {
    s32 ret = 0;
    c_log(C_LOG_LIB_RTC, C_LOG_LEVEL_INFO, "sceRtcSetCurrentDebugNetworkTick", "called");
    if (UNIX_EPOCH_TICKS >= pTick->tick) {
        return ORBIS_RTC_ERROR_INVALID_VALUE;
    }
    if (pTick != NULL) {
        u64 temp = pTick->tick - UNIX_EPOCH_TICKS;
        kernel_timespec_t ts;
        ts.tv_sec = (s64)(temp / 1000000);
        ts.tv_nsec = (s64)(temp % 1000000);
        ret = kernel_posix_clock_settime(ORBIS_RTC_CLOCK_ID_DEBUG_NETWORK, &ts);
    } else {
        ret = kernel_posix_clock_settime(ORBIS_RTC_CLOCK_ID_DEBUG_NETWORK, NULL);
    }
    if (ret < 0) {
        return kernel_errno_error();
    }
    return ret;
}

int PS4_SYSV_ABI sceRtcSetCurrentNetworkTick(OrbisRtcTick* pTick) {
    s32 ret = 0;
    c_log(C_LOG_LIB_RTC, C_LOG_LEVEL_INFO, "sceRtcSetCurrentNetworkTick", "called");
    if (UNIX_EPOCH_TICKS >= pTick->tick) {
        return ORBIS_RTC_ERROR_INVALID_VALUE;
    }
    if (pTick != NULL) {
        u64 temp = pTick->tick - UNIX_EPOCH_TICKS;
        kernel_timespec_t ts;
        ts.tv_sec = (s64)(temp / 1000000);
        ts.tv_nsec = (s64)(temp % 1000000);
        ret = kernel_posix_clock_settime(ORBIS_RTC_CLOCK_ID_NETWORK, &ts);
    } else {
        ret = kernel_posix_clock_settime(ORBIS_RTC_CLOCK_ID_NETWORK, NULL);
    }
    if (ret < 0) {
        return kernel_errno_error();
    }
    return ret;
}

int PS4_SYSV_ABI sceRtcSetCurrentTick(OrbisRtcTick* pTick) {
    s64 temp;
    kernel_timeval_t tv;
    c_log(C_LOG_LIB_RTC, C_LOG_LEVEL_INFO, "sceRtcSetCurrentTick", "called");
    if (pTick == NULL) {
        return ORBIS_RTC_ERROR_INVALID_POINTER;
    }
    if (UNIX_EPOCH_TICKS >= pTick->tick) {
        return ORBIS_RTC_ERROR_INVALID_VALUE;
    }
    temp = (s64)(pTick->tick - UNIX_EPOCH_TICKS);
    tv.tv_sec = temp / 1000000;
    tv.tv_usec = temp % 1000000;
    return kernel_settimeofday(&tv, NULL);
}

int PS4_SYSV_ABI sceRtcSetDosTime(OrbisRtcDateTime* pTime, u32 dosTime) {
    int16_t days;
    c_log(C_LOG_LIB_RTC, C_LOG_LEVEL_TRACE, "sceRtcSetDosTime", "called");

    if (pTime == NULL)
        return ORBIS_RTC_ERROR_INVALID_POINTER;

    pTime->microsecond = 0;
    pTime->second = (dosTime << 1) & 0x3e;
    pTime->minute = (dosTime >> 5) & 0x3f;
    pTime->hour = (dosTime & 0xf800) >> 0xb;

    days = dosTime >> 0x10;

    pTime->day = days & 0x1f;
    pTime->month = (days >> 5) & 0x0f;
    pTime->year = (days >> 9) + 1980;
    return ORBIS_OK;
}

int PS4_SYSV_ABI sceRtcSetTick(OrbisRtcDateTime* pTime, OrbisRtcTick* pTick) {
    u32 ly, ld, lm, j;
    u64 days, msec;
    c_log(C_LOG_LIB_RTC, C_LOG_LEVEL_TRACE, "sceRtcSetTick", "called");

    if (pTime == NULL || pTick == NULL)
        return ORBIS_RTC_ERROR_INVALID_POINTER;

    days = pTick->tick / RTC_DAY_US;
    msec = pTick->tick % RTC_DAY_US;

    days += 307;

    j = (days << 2) - 1;
    ly = j / 146097;

    j -= (146097 * ly);
    ld = j >> 2;

    j = ((ld << 2) + 3) / 1461;
    ld = (((ld << 2) + 7) - 1461 * j) >> 2;

    lm = (5 * ld - 3) / 153;
    ld = (5 * ld + 2 - 153 * lm) / 5;
    ly = 100 * ly + j;

    if (lm < 10) {
        lm += 3;
    } else {
        lm -= 9;
        ly++;
    }

    pTime->year = ly;
    pTime->month = lm;
    pTime->day = ld;

    pTime->hour = msec / RTC_HOUR_US;
    msec %= RTC_HOUR_US;
    pTime->minute = msec / RTC_MINUTE_US;
    msec %= RTC_MINUTE_US;
    pTime->second = msec / 1000000;
    msec %= 1000000;
    pTime->microsecond = msec;

    return ORBIS_OK;
}

int PS4_SYSV_ABI sceRtcSetTime_t(OrbisRtcDateTime* pTime, s64 llTime) {
    int sdk_version;
    int sdkResult;
    OrbisRtcTick newTick;
    c_log(C_LOG_LIB_RTC, C_LOG_LEVEL_TRACE, "sceRtcSetTime_t", "called");

    if (pTime == NULL)
        return ORBIS_RTC_ERROR_INVALID_POINTER;

    sdkResult = kernel_get_compiled_sdk_version(&sdk_version);
    if (sdkResult != ORBIS_OK) {
        sdk_version = 0;
    }

    if (sdk_version < 0x3000000) {
        newTick.tick = (u64)((llTime & 0xffffffff) * 1000000);
    } else {
        if (llTime < 0) {
            return ORBIS_RTC_ERROR_INVALID_VALUE;
        }
        newTick.tick = (u64)(llTime * 1000000);
    }

    newTick.tick += UNIX_EPOCH_TICKS;
    sceRtcSetTick(pTime, &newTick);

    return ORBIS_OK;
}

int PS4_SYSV_ABI sceRtcSetWin32FileTime(OrbisRtcDateTime* pTime, int64_t ulWin32Time) {
    u64 convertedTime;
    OrbisRtcTick convertedTick;
    c_log(C_LOG_LIB_RTC, C_LOG_LEVEL_TRACE, "sceRtcSetWin32FileTime", "called");

    if (pTime == NULL)
        return ORBIS_RTC_ERROR_INVALID_POINTER;

    convertedTime = (ulWin32Time / 10) + WIN32_FILETIME_EPOCH_TICKS;

    convertedTick.tick = convertedTime;

    sceRtcSetTick(pTime, &convertedTick);

    return ORBIS_OK;
}

int PS4_SYSV_ABI sceRtcTickAddDays(OrbisRtcTick* pTick1, OrbisRtcTick* pTick2, int32_t lAdd) {
    c_log(C_LOG_LIB_RTC, C_LOG_LEVEL_TRACE, "sceRtcTickAddDays", "called");

    if (pTick1 == NULL || pTick2 == NULL)
        return ORBIS_RTC_ERROR_INVALID_POINTER;

    pTick1->tick = (lAdd * RTC_DAY_US) + pTick2->tick;

    return ORBIS_OK;
}

int PS4_SYSV_ABI sceRtcTickAddHours(OrbisRtcTick* pTick1, OrbisRtcTick* pTick2, int32_t lAdd) {
    c_log(C_LOG_LIB_RTC, C_LOG_LEVEL_TRACE, "sceRtcTickAddHours", "called");

    if (pTick1 == NULL || pTick2 == NULL)
        return ORBIS_RTC_ERROR_INVALID_POINTER;

    pTick1->tick = (lAdd * RTC_HOUR_US) + pTick2->tick;

    return ORBIS_OK;
}

int PS4_SYSV_ABI sceRtcTickAddMicroseconds(OrbisRtcTick* pTick1, OrbisRtcTick* pTick2,
                                           int64_t lAdd) {
    c_log(C_LOG_LIB_RTC, C_LOG_LEVEL_TRACE, "sceRtcTickAddMicroseconds", "called");

    if (pTick1 == NULL || pTick2 == NULL)
        return ORBIS_RTC_ERROR_INVALID_POINTER;

    pTick1->tick = lAdd + pTick2->tick;

    return ORBIS_OK;
}

int PS4_SYSV_ABI sceRtcTickAddMinutes(OrbisRtcTick* pTick1, OrbisRtcTick* pTick2, int64_t lAdd) {
    c_log(C_LOG_LIB_RTC, C_LOG_LEVEL_TRACE, "sceRtcTickAddMinutes", "called");

    if (pTick1 == NULL || pTick2 == NULL)
        return ORBIS_RTC_ERROR_INVALID_POINTER;

    pTick1->tick = (lAdd * RTC_MINUTE_US) + pTick2->tick;

    return ORBIS_OK;
}

int PS4_SYSV_ABI sceRtcTickAddMonths(OrbisRtcTick* pTick1, OrbisRtcTick* pTick2, int32_t lAdd) {
    OrbisRtcDateTime time;
    s64 s;
    s64 tempMonth;
    int lastDay;
    int timeIsValid;
    c_log(C_LOG_LIB_RTC, C_LOG_LEVEL_TRACE, "sceRtcTickAddMonths", "called");

    if (pTick1 == NULL || pTick2 == NULL)
        return ORBIS_RTC_ERROR_INVALID_POINTER;

    if (lAdd == 0) {
        pTick1->tick = pTick2->tick;
        return ORBIS_OK;
    }

    sceRtcSetTick(&time, pTick1);

    if (lAdd >= 0) {
        s = 1;
    } else {
        s = -1;
    }

    time.year += (lAdd / 12);
    tempMonth = time.month + (lAdd % 12) - 1;

    if (tempMonth > 11 || tempMonth < 0) {
        tempMonth -= s * 12;
        time.year += s;
    }

    time.month = tempMonth + 1;

    lastDay = (int)rtime_days_in_month(time.year, time.month);

    if (time.day > lastDay) {
        time.day = (u16)lastDay;
    }

    timeIsValid = sceRtcCheckValid(&time);
    if (timeIsValid == ORBIS_OK) {
        sceRtcGetTick(&time, pTick1);
    } else {
        return timeIsValid;
    }

    return ORBIS_OK;
}

int PS4_SYSV_ABI sceRtcTickAddSeconds(OrbisRtcTick* pTick1, OrbisRtcTick* pTick2, int64_t lAdd) {
    c_log(C_LOG_LIB_RTC, C_LOG_LEVEL_TRACE, "sceRtcTickAddSeconds", "called");

    if (pTick1 == NULL || pTick2 == NULL)
        return ORBIS_RTC_ERROR_INVALID_POINTER;

    pTick1->tick = (lAdd * 1000000) + pTick2->tick;

    return ORBIS_OK;
}

int PS4_SYSV_ABI sceRtcTickAddTicks(OrbisRtcTick* pTick1, OrbisRtcTick* pTick2, int64_t lAdd) {
    c_log(C_LOG_LIB_RTC, C_LOG_LEVEL_TRACE, "sceRtcTickAddTicks", "called");

    if (pTick1 == NULL || pTick2 == NULL)
        return ORBIS_RTC_ERROR_INVALID_POINTER;

    pTick1->tick = lAdd + pTick2->tick;

    return ORBIS_OK;
}

int PS4_SYSV_ABI sceRtcTickAddWeeks(OrbisRtcTick* pTick1, OrbisRtcTick* pTick2, int32_t lAdd) {
    c_log(C_LOG_LIB_RTC, C_LOG_LEVEL_TRACE, "sceRtcTickAddWeeks", "called");

    if (pTick1 == NULL || pTick2 == NULL)
        return ORBIS_RTC_ERROR_INVALID_POINTER;

    pTick1->tick = (lAdd * RTC_WEEK_US) + pTick2->tick;

    return ORBIS_OK;
}

int PS4_SYSV_ABI sceRtcTickAddYears(OrbisRtcTick* pTick1, OrbisRtcTick* pTick2, int32_t lAdd) {
    OrbisRtcDateTime time;
    int timeIsValid;
    c_log(C_LOG_LIB_RTC, C_LOG_LEVEL_TRACE, "sceRtcTickAddYears", "called");

    if (pTick1 == NULL || pTick2 == NULL)
        return ORBIS_RTC_ERROR_INVALID_POINTER;

    if (lAdd == 0) {
        pTick1->tick = pTick2->tick;
        return ORBIS_OK;
    }

    sceRtcSetTick(&time, pTick1);

    time.year += lAdd;

    timeIsValid = sceRtcCheckValid(&time);
    if (timeIsValid == ORBIS_OK) {
        sceRtcGetTick(&time, pTick1);
    } else {
        return timeIsValid;
    }

    return ORBIS_OK;
}
