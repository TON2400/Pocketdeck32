#include "SettingsApp.h"
#include "../core/SettingsManager.h"
#include "../core/TimeManager.h"
#include "../core/WiFiManager.h"
#include "../core/SystemManager.h"
#include <stdio.h>

static const int SETTINGS_ITEM_COUNT   = 14;
static const int SETTINGS_VISIBLE_ROWS = 4;

SettingsApp::SettingsApp()
    : _selectedIndex(0),
      _scrollOffset(0),
      _showingInfo(false) {}

void SettingsApp::onEnter() {
    _selectedIndex = 0;
    _scrollOffset  = 0;
    _showingInfo   = false;
}

void SettingsApp::adjustSelected(int delta) {
    auto& cfg = g_settings.get();

    switch (_selectedIndex) {
        case 0: // Mode Indicator
            cfg.showModeIndicator = !cfg.showModeIndicator;
            g_settings.save();
            g_system.applySettings();
            break;
        case 1: // Default Boot Input Mode
            cfg.defaultInputMode = (cfg.defaultInputMode == MODE_KEYPAD) ? MODE_IR : MODE_KEYPAD;
            g_settings.save();
            break;
        case 2: // Brightness
            cfg.brightnessLevel = (uint8_t)((cfg.brightnessLevel + (delta >= 0 ? 1 : 2)) % 3);
            g_settings.save();
            g_system.applySettings();
            break;
        case 3: // Invert Display
            cfg.invertDisplay = !cfg.invertDisplay;
            g_settings.save();
            g_system.applySettings();
            break;
        case 4: { // Screen Timeout (0, 15, 30, 60, 120, 300)
            static const uint16_t TIMEOUTS[] = { 0, 15, 30, 60, 120, 300 };
            static const int NUM_T = 6;
            int idx = 0;
            for (int i = 0; i < NUM_T; i++) {
                if (TIMEOUTS[i] == cfg.screenTimeoutSec) {
                    idx = i;
                    break;
                }
            }
            idx = (idx + (delta >= 0 ? 1 : (NUM_T - 1))) % NUM_T;
            cfg.screenTimeoutSec = TIMEOUTS[idx];
            g_settings.save();
            break;
        }
        case 5: // Weather Temperature Unit (C / F)
            cfg.weatherFahrenheit = !cfg.weatherFahrenheit;
            g_settings.save();
            break;
        case 6: { // Weather Auto-Refresh Interval (0, 15, 30, 60 min)
            static const uint8_t INTERVALS[] = { 0, 15, 30, 60 };
            int idx = 0;
            for (int i = 0; i < 4; i++) {
                if (INTERVALS[i] == cfg.weatherRefreshMin) {
                    idx = i;
                    break;
                }
            }
            idx = (idx + (delta >= 0 ? 1 : 3)) % 4;
            cfg.weatherRefreshMin = INTERVALS[idx];
            g_settings.save();
            break;
        }
        case 7: { // Timezone Offset (-12:00 to +14:00 in 30-min increments)
            int16_t nextMin = cfg.tzOffsetMinutes + (int16_t)((delta >= 0 ? 1 : -1) * 30);
            if (nextMin > 840)  nextMin = -720;
            if (nextMin < -720) nextMin = 840;
            cfg.tzOffsetMinutes = nextMin;
            g_settings.save();
            g_time.applyTimezone();
            break;
        }
        case 8: // Time Format (24H / 12H)
            cfg.use24HourTime = !cfg.use24HourTime;
            g_settings.save();
            break;
        case 9: // Auto NTP Sync on Wi-Fi connect
            cfg.autoNtpSync = !cfg.autoNtpSync;
            g_settings.save();
            if (cfg.autoNtpSync && g_wifi.isConnected()) {
                g_time.requestNtpSync();
            }
            break;
        case 10: // Auto Wi-Fi Connect on Boot
            cfg.autoConnectWiFi = !cfg.autoConnectWiFi;
            g_settings.save();
            break;
        case 11: // System Info view
            _showingInfo = true;
            break;
        case 12: // Reset Defaults
            g_settings.resetDefaults();
            g_settings.save();
            g_time.applyTimezone();
            g_system.applySettings();
            g_system.display().showToast("DEFAULTS SET", 900);
            break;
        case 13: // Restart ESP32
            g_system.display().clear();
            g_system.display().oled().setCursor(24, 28);
            g_system.display().oled().print(F("REBOOTING..."));
            g_system.display().present();
            delay(250);
            ESP.restart();
            break;
    }
}

bool SettingsApp::handleInput(const InputEvent& ev) {
    if (ev.type != EVT_PRESS && ev.type != EVT_REPEAT) {
        return false;
    }

    if (_showingInfo) {
        if (ev.action == ACT_BACK || ev.action == ACT_SELECT) {
            _showingInfo = false;
        }
        return false;
    }

    if (ev.action == ACT_BACK && ev.type == EVT_PRESS) {
        return true; // Return to Main Menu
    }

    if (ev.action == ACT_UP) {
        _selectedIndex = (_selectedIndex - 1 + SETTINGS_ITEM_COUNT) % SETTINGS_ITEM_COUNT;
    } else if (ev.action == ACT_DOWN) {
        _selectedIndex = (_selectedIndex + 1) % SETTINGS_ITEM_COUNT;
    } else if (ev.action == ACT_LEFT && ev.type == EVT_PRESS) {
        adjustSelected(-1);
    } else if ((ev.action == ACT_RIGHT || ev.action == ACT_SELECT) && ev.type == EVT_PRESS) {
        adjustSelected(1);
    }

    if (_selectedIndex < _scrollOffset) {
        _scrollOffset = _selectedIndex;
    } else if (_selectedIndex >= _scrollOffset + SETTINGS_VISIBLE_ROWS) {
        _scrollOffset = _selectedIndex - SETTINGS_VISIBLE_ROWS + 1;
    }

    return false;
}

void SettingsApp::render(DisplayManager& display, InputMode mode, bool showModeBadge) {
    auto& oled = display.oled();
    display.clear();
    display.drawHeader("SETTINGS", mode, showModeBadge);

    if (_showingInfo) {
        oled.setCursor(2, 14);
        oled.print(F("PocketDeck v" POCKETDECK_VERSION));
        oled.setCursor(2, 24);
        oled.printf("Chip: %s r%d", ESP.getChipModel(), ESP.getChipRevision());
        oled.setCursor(2, 34);
        oled.printf("CPU: %luMHz OLED:G33", (unsigned long)ESP.getCpuFreqMHz());
        oled.setCursor(2, 44);
        oled.printf("Flash:%luMB Heap:%luK",
                    (unsigned long)(ESP.getFlashChipSize() / (1024UL * 1024UL)),
                    (unsigned long)(ESP.getFreeHeap() / 1024UL));
        display.drawFooter("*:Back", nullptr);
        return;
    }

    const auto& cfg = g_settings.get();
    const char* brightStr = (cfg.brightnessLevel == 0) ? "LOW" :
                            (cfg.brightnessLevel == 1) ? "MED" : "HIGH";
    char tzStr[14];
    TimeManager::formatTimezoneOffset(cfg.tzOffsetMinutes, tzStr, sizeof(tzStr));

    char lines[SETTINGS_ITEM_COUNT][24];
    snprintf(lines[0],  sizeof(lines[0]),  "Mode Badge: %s", cfg.showModeIndicator ? "ON" : "OFF");
    snprintf(lines[1],  sizeof(lines[1]),  "Boot Input: %s", cfg.defaultInputMode == MODE_KEYPAD ? "KP" : "IR");
    snprintf(lines[2],  sizeof(lines[2]),  "Brightness: %s", brightStr);
    snprintf(lines[3],  sizeof(lines[3]),  "Invert OLED: %s", cfg.invertDisplay ? "YES" : "NO");
    if (cfg.screenTimeoutSec == 0) {
        snprintf(lines[4], sizeof(lines[4]), "Timeout: OFF");
    } else {
        snprintf(lines[4], sizeof(lines[4]), "Timeout: %us", (unsigned)cfg.screenTimeoutSec);
    }
    snprintf(lines[5],  sizeof(lines[5]),  "Temp Unit: %s", cfg.weatherFahrenheit ? "FAHR (F)" : "CELS (C)");
    if (cfg.weatherRefreshMin == 0) {
        snprintf(lines[6], sizeof(lines[6]), "Wx Refresh: MANUAL");
    } else {
        snprintf(lines[6], sizeof(lines[6]), "Wx Refresh: %um", (unsigned)cfg.weatherRefreshMin);
    }
    snprintf(lines[7],  sizeof(lines[7]),  "Timezone: %s", tzStr);
    snprintf(lines[8],  sizeof(lines[8]),  "Time Fmt: %s", cfg.use24HourTime ? "24-HOUR" : "12-HOUR");
    snprintf(lines[9],  sizeof(lines[9]),  "Auto NTP: %s", cfg.autoNtpSync ? "ON" : "OFF");
    snprintf(lines[10], sizeof(lines[10]), "Auto Wi-Fi: %s", cfg.autoConnectWiFi ? "ON" : "OFF");
    snprintf(lines[11], sizeof(lines[11]), "System Info >");
    snprintf(lines[12], sizeof(lines[12]), "Reset Defaults");
    snprintf(lines[13], sizeof(lines[13]), "Restart Device");

    const int startY = 13;
    const int rowH   = 10;

    for (int i = 0; i < SETTINGS_VISIBLE_ROWS; i++) {
        int idx = _scrollOffset + i;
        if (idx >= SETTINGS_ITEM_COUNT) break;

        int y = startY + (i * rowH);
        if (idx == _selectedIndex) {
            oled.fillRect(0, y, OLED_WIDTH - 5, rowH, SSD1306_WHITE);
            oled.setTextColor(SSD1306_BLACK);
        } else {
            oled.setTextColor(SSD1306_WHITE);
        }

        oled.setCursor(3, y + 1);
        oled.print(lines[idx]);
    }

    oled.setTextColor(SSD1306_WHITE);
    display.drawScrollbar(startY, SETTINGS_VISIBLE_ROWS * rowH, SETTINGS_ITEM_COUNT, SETTINGS_VISIBLE_ROWS, _scrollOffset);
    display.drawFooter("*:Back", "5/#:Toggle");
}
