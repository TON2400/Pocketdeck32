#ifndef POCKETDECK_TIME_MANAGER_H
#define POCKETDECK_TIME_MANAGER_H

#include <Arduino.h>

struct DateTimeInfo {
    uint16_t year;
    uint8_t  month;
    uint8_t  day;
    uint8_t  hour;
    uint8_t  minute;
    uint8_t  second;
    uint8_t  dayOfWeek; // 0 = Sunday .. 6 = Saturday
    bool     valid;
};

enum TimeSyncStatus : uint8_t {
    TIME_SYNC_NONE = 0,
    TIME_SYNC_IN_PROGRESS,
    TIME_SYNC_OK_NTP,
    TIME_SYNC_OK_MANUAL,
    TIME_SYNC_FAILED
};

class TimeManager {
public:
    TimeManager();

    void begin();
    void update();

    // Called automatically by WiFiManager when Wi-Fi connects
    void onWiFiConnected();

    // Start non-blocking NTP synchronization
    bool requestNtpSync();

    // Apply current timezone offset from SettingsManager
    void applyTimezone();

    // Manual offline clock setter
    bool setManualTime(uint16_t year, uint8_t month, uint8_t day,
                       uint8_t hour, uint8_t minute, uint8_t second);

    const DateTimeInfo& now() const { return _now; }
    bool isTimeValid() const { return _now.valid; }
    TimeSyncStatus getSyncStatus() const { return _status; }
    const char* getSyncStatusLabel() const;
    uint32_t getLastSyncElapsedSec() const;

    // Formatting utilities
    void formatTimeHHMM(char* outBuf, size_t maxLen) const;
    void formatTimeHHMMSS(char* outBuf, size_t maxLen) const;
    void formatDateFull(char* outBuf, size_t maxLen) const;
    void formatDateISO(char* outBuf, size_t maxLen) const;

    static const char* getDayOfWeekName(uint8_t dow);
    static const char* getDayOfWeekShort(uint8_t dow);
    static void formatTimezoneOffset(int16_t offsetMinutes, char* outBuf, size_t maxLen);
    static uint8_t calcDayOfWeek(uint16_t y, uint8_t m, uint8_t d);

private:
    void refreshLocalTime();

    DateTimeInfo   _now;
    TimeSyncStatus _status;
    uint32_t       _syncStartMs;
    uint32_t       _lastSyncSuccessMs;
    uint32_t       _lastTickMs;
    bool           _hasEverSyncedNtp;
};

extern TimeManager g_time;

#endif // POCKETDECK_TIME_MANAGER_H
