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
