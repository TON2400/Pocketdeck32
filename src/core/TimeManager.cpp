#include "TimeManager.h"
#include "SettingsManager.h"
#include "WiFiManager.h"
#include <time.h>
#include <sys/time.h>
#include <stdio.h>
#include <string.h>

TimeManager g_time;

static const char* const DOW_NAMES[] = {
    "Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"
};

static const char* const DOW_SHORT[] = {
    "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"
};

static const char* const MONTH_SHORT[] = {
    "Jan", "Feb", "Mar", "Apr", "May", "Jun",
    "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
};

TimeManager::TimeManager()
    : _status(TIME_SYNC_NONE),
      _syncStartMs(0),
      _lastSyncSuccessMs(0),
      _lastTickMs(0),
      _hasEverSyncedNtp(false) {
    _now = { 2026, 1, 1, 0, 0, 0, 4, false };
}

void TimeManager::begin() {
    applyTimezone();
    refreshLocalTime();
}

void TimeManager::applyTimezone() {
    long offsetSec = (long)g_settings.get().tzOffsetMinutes * 60L;
    configTime(offsetSec, 0, "pool.ntp.org", "time.google.com", "time.nist.gov");
    refreshLocalTime();
}

void TimeManager::onWiFiConnected() {
    if (g_settings.get().autoNtpSync) {
        requestNtpSync();
    }
}

bool TimeManager::requestNtpSync() {
    if (!g_wifi.isConnected()) {
        _status = TIME_SYNC_FAILED;
        return false;
    }

    long offsetSec = (long)g_settings.get().tzOffsetMinutes * 60L;
    configTime(offsetSec, 0, "pool.ntp.org", "time.google.com", "time.nist.gov");

    _status      = TIME_SYNC_IN_PROGRESS;
    _syncStartMs = millis();
    return true;
}

bool TimeManager::setManualTime(uint16_t year, uint8_t month, uint8_t day,
                                uint8_t hour, uint8_t minute, uint8_t second) {
    if (year < 2024 || year > 2099 || month < 1 || month > 12 || day < 1 || day > 31) {
        return false;
    }

    struct tm t = {};
    t.tm_year = (int)year - 1900;
    t.tm_mon  = (int)month - 1;
    t.tm_mday = (int)day;
    t.tm_hour = (int)hour;
    t.tm_min  = (int)minute;
    t.tm_sec  = (int)second;

    // mktime interprets struct tm in current local timezone (configured by configTime)
    time_t epoch = mktime(&t);
    if (epoch == (time_t)-1) {
        return false;
    }

    struct timeval tv = { .tv_sec = epoch, .tv_usec = 0 };
    settimeofday(&tv, nullptr);

    _status = TIME_SYNC_OK_MANUAL;
    _lastSyncSuccessMs = millis();
    refreshLocalTime();
    return true;
}

void TimeManager::refreshLocalTime() {
    time_t nowEpoch = time(nullptr);
    // Epoch > Nov 2023 means internal clock has been set (either via NTP or manual)
    if (nowEpoch > 1700000000L) {
        struct tm ti = {};
        localtime_r(&nowEpoch, &ti);
        _now.year      = (uint16_t)(ti.tm_year + 1900);
        _now.month     = (uint8_t)(ti.tm_mon + 1);
        _now.day       = (uint8_t)ti.tm_mday;
        _now.hour      = (uint8_t)ti.tm_hour;
        _now.minute    = (uint8_t)ti.tm_min;
        _now.second    = (uint8_t)ti.tm_sec;
        _now.dayOfWeek = (uint8_t)ti.tm_wday;
        _now.valid     = true;
    } else {
        _now.valid = false;
    }
}

void TimeManager::update() {
    uint32_t nowMs = millis();
    if ((nowMs - _lastTickMs) < 250UL) return;
    _lastTickMs = nowMs;

    refreshLocalTime();

    if (_status == TIME_SYNC_IN_PROGRESS) {
        // Check if SNTP updated the system clock
        if (_now.valid && (nowMs - _syncStartMs) >= 600UL) {
            _status            = TIME_SYNC_OK_NTP;
            _lastSyncSuccessMs = nowMs;
            _hasEverSyncedNtp  = true;
        } else if ((nowMs - _syncStartMs) >= 10000UL) {
            // Timed out waiting for NTP response
            if (_now.valid) {
                _status = _hasEverSyncedNtp ? TIME_SYNC_OK_NTP : TIME_SYNC_OK_MANUAL;
            } else {
                _status = TIME_SYNC_FAILED;
            }
        }
    } else if (g_settings.get().autoNtpSync && g_wifi.isConnected()) {
        // Periodic background re-sync every 60 minutes while Wi-Fi is connected
        if (_lastSyncSuccessMs > 0 && (nowMs - _lastSyncSuccessMs) >= 3600000UL) {
            requestNtpSync();
        }
    }
}

const char* TimeManager::getSyncStatusLabel() const {
    switch (_status) {
        case TIME_SYNC_IN_PROGRESS: return "SYNCING NTP...";
        case TIME_SYNC_OK_NTP:      return "NTP SYNCED";
        case TIME_SYNC_OK_MANUAL:   return "MANUAL TIME";
        case TIME_SYNC_FAILED:      return "SYNC FAILED";
        default:                    return _now.valid ? "TIME READY" : "NOT SYNCED";
    }
}

uint32_t TimeManager::getLastSyncElapsedSec() const {
    if (_lastSyncSuccessMs == 0) return 0;
    return (millis() - _lastSyncSuccessMs) / 1000UL;
}

void TimeManager::formatTimeHHMM(char* outBuf, size_t maxLen) const {
    if (!outBuf || maxLen == 0) return;
    if (!_now.valid) {
        strncpy(outBuf, "--:--", maxLen - 1);
        outBuf[maxLen - 1] = '\0';
        return;
    }
    if (g_settings.get().use24HourTime) {
        snprintf(outBuf, maxLen, "%02u:%02u", (unsigned)_now.hour, (unsigned)_now.minute);
    } else {
        uint8_t h12 = _now.hour % 12;
        if (h12 == 0) h12 = 12;
        const char* ampm = (_now.hour >= 12) ? "PM" : "AM";
        snprintf(outBuf, maxLen, "%02u:%02u%s", (unsigned)h12, (unsigned)_now.minute, ampm);
    }
}

void TimeManager::formatTimeHHMMSS(char* outBuf, size_t maxLen) const {
    if (!outBuf || maxLen == 0) return;
    if (!_now.valid) {
        strncpy(outBuf, "--:--:--", maxLen - 1);
        outBuf[maxLen - 1] = '\0';
        return;
    }
    if (g_settings.get().use24HourTime) {
        snprintf(outBuf, maxLen, "%02u:%02u:%02u",
                 (unsigned)_now.hour, (unsigned)_now.minute, (unsigned)_now.second);
    } else {
        uint8_t h12 = _now.hour % 12;
        if (h12 == 0) h12 = 12;
        const char* ampm = (_now.hour >= 12) ? "P" : "A";
        snprintf(outBuf, maxLen, "%02u:%02u:%02u%s",
                 (unsigned)h12, (unsigned)_now.minute, (unsigned)_now.second, ampm);
    }
}

void TimeManager::formatDateFull(char* outBuf, size_t maxLen) const {
    if (!outBuf || maxLen == 0) return;
    if (!_now.valid || _now.month < 1 || _now.month > 12) {
        strncpy(outBuf, "Date Not Set", maxLen - 1);
        outBuf[maxLen - 1] = '\0';
        return;
    }
    snprintf(outBuf, maxLen, "%02u %s %04u",
             (unsigned)_now.day, MONTH_SHORT[_now.month - 1], (unsigned)_now.year);
}

void TimeManager::formatDateISO(char* outBuf, size_t maxLen) const {
    if (!outBuf || maxLen == 0) return;
    if (!_now.valid) {
        strncpy(outBuf, "----/--/--", maxLen - 1);
        outBuf[maxLen - 1] = '\0';
        return;
    }
    snprintf(outBuf, maxLen, "%04u-%02u-%02u",
             (unsigned)_now.year, (unsigned)_now.month, (unsigned)_now.day);
}

const char* TimeManager::getDayOfWeekName(uint8_t dow) {
    return DOW_NAMES[dow % 7];
}

const char* TimeManager::getDayOfWeekShort(uint8_t dow) {
    return DOW_SHORT[dow % 7];
}

void TimeManager::formatTimezoneOffset(int16_t offsetMinutes, char* outBuf, size_t maxLen) {
    if (!outBuf || maxLen == 0) return;
    char sign = (offsetMinutes >= 0) ? '+' : '-';
    int16_t absMin = (offsetMinutes >= 0) ? offsetMinutes : -offsetMinutes;
    int16_t hrs  = absMin / 60;
    int16_t mins = absMin % 60;
    if (mins == 0) {
        snprintf(outBuf, maxLen, "UTC%c%d", sign, (int)hrs);
    } else {
        snprintf(outBuf, maxLen, "UTC%c%02d:%02d", sign, (int)hrs, (int)mins);
    }
}

uint8_t TimeManager::calcDayOfWeek(uint16_t y, uint8_t m, uint8_t d) {
    static const uint8_t t[] = { 0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4 };
    if (m < 1 || m > 12) return 0;
    if (m < 3) y -= 1;
    return (uint8_t)((y + y / 4 - y / 100 + y / 400 + t[m - 1] + d) % 7);
}
