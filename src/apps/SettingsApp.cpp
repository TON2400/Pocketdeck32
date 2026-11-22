#include "SettingsApp.h"
#include "../core/SettingsManager.h"
#include "../core/SystemManager.h"
#include <stdio.h>

static const int SETTINGS_ITEM_COUNT   = 8;
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
        case 4: { // Screen Timeout (0, 30, 60, 120)
            static const uint16_t TIMEOUTS[] = { 0, 30, 60, 120 };
            int idx = 0;
            for (int i = 0; i < 4; i++) {
                if (TIMEOUTS[i] == cfg.screenTimeoutSec) {
                    idx = i;
                    break;
                }
            }
            idx = (idx + (delta >= 0 ? 1 : 3)) % 4;
            cfg.screenTimeoutSec = TIMEOUTS[idx];
            g_settings.save();
            break;
        }
        case 5: // System Info view
            _showingInfo = true;
            break;
        case 6: // Reset Defaults
            g_settings.resetDefaults();
            g_settings.save();
            g_system.applySettings();
            g_system.display().showToast("DEFAULTS SET", 900);
            break;
        case 7: // Restart ESP32
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
        oled.printf("CPU: %luMHz Cores:%d", (unsigned long)ESP.getCpuFreqMHz(), ESP.getChipCores());
        oled.setCursor(2, 44);
        oled.printf("Flash: %luMB", (unsigned long)(ESP.getFlashChipSize() / (1024UL * 1024UL)));
        display.drawFooter("*:Back", nullptr);
        return;
    }

    const auto& cfg = g_settings.get();
    const char* brightStr = (cfg.brightnessLevel == 0) ? "LOW" :
                            (cfg.brightnessLevel == 1) ? "MED" : "HIGH";

    char lines[SETTINGS_ITEM_COUNT][24];
    snprintf(lines[0], sizeof(lines[0]), "Mode Badge: %s", cfg.showModeIndicator ? "ON" : "OFF");
    snprintf(lines[1], sizeof(lines[1]), "Boot Input: %s", cfg.defaultInputMode == MODE_KEYPAD ? "KP" : "IR");
    snprintf(lines[2], sizeof(lines[2]), "Brightness: %s", brightStr);
    snprintf(lines[3], sizeof(lines[3]), "Invert OLED: %s", cfg.invertDisplay ? "YES" : "NO");
    if (cfg.screenTimeoutSec == 0) {
        snprintf(lines[4], sizeof(lines[4]), "Timeout: OFF");
    } else {
        snprintf(lines[4], sizeof(lines[4]), "Timeout: %us", (unsigned)cfg.screenTimeoutSec);
    }
    snprintf(lines[5], sizeof(lines[5]), "System Info >");
    snprintf(lines[6], sizeof(lines[6]), "Reset Defaults");
    snprintf(lines[7], sizeof(lines[7]), "Restart Device");

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
