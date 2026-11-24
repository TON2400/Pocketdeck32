#ifndef POCKETDECK_SETTINGS_MANAGER_H
#define POCKETDECK_SETTINGS_MANAGER_H

#include <Arduino.h>
#include "InputManager.h"

struct SystemSettings {
    bool      showModeIndicator;
    InputMode defaultInputMode;
    uint8_t   brightnessLevel;    // 0 = Low, 1 = Med, 2 = High
    bool      invertDisplay;
    uint16_t  screenTimeoutSec;   // 0 = Off, 15, 30, 60, 120, 300
    bool      weatherFahrenheit;  // false = Celsius (°C), true = Fahrenheit (°F)
    uint8_t   weatherRefreshMin;  // 0 = Off, 15, 30, 60 min
    int16_t   tzOffsetMinutes;    // UTC offset in minutes (e.g. 420 = UTC+07:00)
    bool      use24HourTime;      // true = 24H, false = 12H
    bool      autoNtpSync;        // true = auto NTP sync on Wi-Fi connect
    bool      autoConnectWiFi;    // true = auto reconnect saved Wi-Fi on boot
};

class SettingsManager {
public:
    SettingsManager();

    void begin();
    void save();
    void resetDefaults();

    SystemSettings& get() { return _settings; }
    const SystemSettings& get() const { return _settings; }

    uint8_t getContrastValue() const;

private:
    SystemSettings _settings;
};

extern SettingsManager g_settings;

#endif // POCKETDECK_SETTINGS_MANAGER_H
