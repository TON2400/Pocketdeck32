#include "SettingsManager.h"
#include <Preferences.h>

SettingsManager g_settings;
static Preferences s_prefs;

SettingsManager::SettingsManager() {
    resetDefaults();
}

void SettingsManager::resetDefaults() {
    _settings.showModeIndicator = true;
    _settings.defaultInputMode  = MODE_KEYPAD;
    _settings.brightnessLevel   = 2;
    _settings.invertDisplay     = false;
    _settings.screenTimeoutSec  = 60;
    _settings.weatherFahrenheit = false;
    _settings.weatherRefreshMin = 30;
    _settings.tzOffsetMinutes   = 420; // Default UTC+07:00
    _settings.use24HourTime     = true;
    _settings.autoNtpSync       = true;
    _settings.autoConnectWiFi   = true;
}

void SettingsManager::begin() {
    resetDefaults();
    if (s_prefs.begin("pdeck_cfg", true)) {
        _settings.showModeIndicator = s_prefs.getBool("showMode", true);
        _settings.defaultInputMode  = (InputMode)s_prefs.getUChar("defMode", (uint8_t)MODE_KEYPAD);
        _settings.brightnessLevel   = s_prefs.getUChar("bright", 2);
        if (_settings.brightnessLevel > 2) _settings.brightnessLevel = 2;
        _settings.invertDisplay     = s_prefs.getBool("invert", false);
        _settings.screenTimeoutSec  = s_prefs.getUShort("timeout", 60);
        _settings.weatherFahrenheit = s_prefs.getBool("wFahr", false);
        _settings.weatherRefreshMin = s_prefs.getUChar("wRefMin", 30);
        _settings.tzOffsetMinutes   = s_prefs.getShort("tzMin", 420);
        if (_settings.tzOffsetMinutes < -720 || _settings.tzOffsetMinutes > 840) {
            _settings.tzOffsetMinutes = 420;
        }
        _settings.use24HourTime     = s_prefs.getBool("time24h", true);
        _settings.autoNtpSync       = s_prefs.getBool("autoNtp", true);
        _settings.autoConnectWiFi   = s_prefs.getBool("autoWifi", true);
        s_prefs.end();
    }
}

void SettingsManager::save() {
    if (s_prefs.begin("pdeck_cfg", false)) {
        s_prefs.putBool("showMode", _settings.showModeIndicator);
        s_prefs.putUChar("defMode", (uint8_t)_settings.defaultInputMode);
        s_prefs.putUChar("bright", _settings.brightnessLevel);
        s_prefs.putBool("invert", _settings.invertDisplay);
        s_prefs.putUShort("timeout", _settings.screenTimeoutSec);
        s_prefs.putBool("wFahr", _settings.weatherFahrenheit);
        s_prefs.putUChar("wRefMin", _settings.weatherRefreshMin);
        s_prefs.putShort("tzMin", _settings.tzOffsetMinutes);
        s_prefs.putBool("time24h", _settings.use24HourTime);
        s_prefs.putBool("autoNtp", _settings.autoNtpSync);
        s_prefs.putBool("autoWifi", _settings.autoConnectWiFi);
        s_prefs.end();
    }
}

uint8_t SettingsManager::getContrastValue() const {
    switch (_settings.brightnessLevel) {
        case 0:  return 16;
        case 1:  return 128;
        default: return 255;
    }
}
