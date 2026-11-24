#include "ClockApp.h"
#include "../core/TimeManager.h"
#include "../core/WiFiManager.h"
#include "../core/SettingsManager.h"
#include "../core/SystemManager.h"
#include <stdio.h>
#include <string.h>

static const int CLK_MENU_COUNT   = 6;
static const int CLK_VISIBLE_ROWS = 4;

ClockApp::ClockApp()
    : _view(CLK_VIEW_MAIN),
      _menuIndex(0),
      _menuScroll(0),
      _lastRenderSec(0),
      _editField(0),
      _editYear(2026),
      _editMonth(11),
      _editDay(23),
      _editHour(12),
      _editMinute(0),
      _editSecond(0) {}

void ClockApp::onEnter() {
    _view          = CLK_VIEW_MAIN;
    _menuIndex     = 0;
    _menuScroll    = 0;
    _lastRenderSec = millis() / 500UL;
}

void ClockApp::adjustTimezone(int stepHalfHours) {
    auto& cfg = g_settings.get();
    int16_t nextMin = cfg.tzOffsetMinutes + (int16_t)(stepHalfHours * 30);
    if (nextMin > 840)  nextMin = -720;
    if (nextMin < -720) nextMin = 840;
    cfg.tzOffsetMinutes = nextMin;
    g_settings.save();
    g_time.applyTimezone();

    char tzStr[16];
    TimeManager::formatTimezoneOffset(cfg.tzOffsetMinutes, tzStr, sizeof(tzStr));
    g_system.display().showToast(tzStr, 700);
}

void ClockApp::startManualEdit() {
    const auto& dt = g_time.now();
    if (dt.valid) {
        _editYear   = dt.year;
        _editMonth  = dt.month;
        _editDay    = dt.day;
        _editHour   = dt.hour;
        _editMinute = dt.minute;
        _editSecond = dt.second;
    } else {
        _editYear   = 2026;
        _editMonth  = 11;
        _editDay    = 23;
        _editHour   = 12;
        _editMinute = 0;
        _editSecond = 0;
    }
    _editField = 0;
    _view      = CLK_VIEW_MANUAL_SET;
}

void ClockApp::adjustManualField(int delta) {
    switch (_editField) {
        case 0: // Hour
            _editHour = (uint8_t)((_editHour + (delta >= 0 ? 1 : 23)) % 24);
            break;
        case 1: // Minute
            _editMinute = (uint8_t)((_editMinute + (delta >= 0 ? 1 : 59)) % 60);
            break;
        case 2: // Second
            _editSecond = (uint8_t)((_editSecond + (delta >= 0 ? 1 : 59)) % 60);
            break;
        case 3: // Day
            _editDay = (uint8_t)(((_editDay - 1 + (delta >= 0 ? 1 : 30)) % 31) + 1);
            break;
        case 4: // Month
            _editMonth = (uint8_t)(((_editMonth - 1 + (delta >= 0 ? 1 : 11)) % 12) + 1);
            break;
        case 5: // Year
            if (delta >= 0) {
                _editYear = (_editYear < 2099) ? (_editYear + 1) : 2024;
            } else {
                _editYear = (_editYear > 2024) ? (_editYear - 1) : 2099;
            }
            break;
    }
}

void ClockApp::saveManualEdit() {
    if (g_time.setManualTime(_editYear, _editMonth, _editDay, _editHour, _editMinute, _editSecond)) {
        g_system.display().showToast("TIME UPDATED", 900);
    } else {
        g_system.display().showToast("INVALID TIME", 900);
    }
    _view = CLK_VIEW_MAIN;
}

bool ClockApp::update() {
    uint32_t curHalfSec = millis() / 500UL;
    if (curHalfSec != _lastRenderSec) {
        _lastRenderSec = curHalfSec;
        return true;
    }
    return false;
}

bool ClockApp::handleInput(const InputEvent& ev) {
    if (ev.type != EVT_PRESS && ev.type != EVT_REPEAT) {
        return false;
    }

    if (_view == CLK_VIEW_MANUAL_SET) {
        if (ev.action == ACT_BACK && ev.type == EVT_PRESS) {
            _view = CLK_VIEW_MENU;
            return false;
        }
        if (ev.action == ACT_LEFT) {
            _editField = (_editField + 5) % 6;
        } else if (ev.action == ACT_RIGHT) {
            _editField = (_editField + 1) % 6;
        } else if (ev.action == ACT_UP) {
            adjustManualField(1);
        } else if (ev.action == ACT_DOWN) {
            adjustManualField(-1);
        } else if (ev.action == ACT_SELECT && ev.type == EVT_PRESS) {
            saveManualEdit();
        }
        return false;
    }

    if (_view == CLK_VIEW_MENU) {
        if (ev.action == ACT_BACK && ev.type == EVT_PRESS) {
            _view = CLK_VIEW_MAIN;
            return false;
        }
        if (ev.action == ACT_UP) {
            _menuIndex = (_menuIndex - 1 + CLK_MENU_COUNT) % CLK_MENU_COUNT;
        } else if (ev.action == ACT_DOWN) {
            _menuIndex = (_menuIndex + 1) % CLK_MENU_COUNT;
        } else if (ev.action == ACT_LEFT && _menuIndex == 1) {
            adjustTimezone(-2);
        } else if (ev.action == ACT_RIGHT && _menuIndex == 1) {
            adjustTimezone(2);
        } else if (ev.action == ACT_SELECT && ev.type == EVT_PRESS) {
            auto& cfg = g_settings.get();
            switch (_menuIndex) {
                case 0: // Sync via NTP Now
                    if (g_wifi.isConnected()) {
                        g_time.requestNtpSync();
                        g_system.display().showToast("NTP SYNCING...", 900);
                        _view = CLK_VIEW_MAIN;
                    } else {
                        g_system.display().showToast("WIFI OFFLINE", 900);
                    }
                    break;
                case 1: // Timezone step (+1 hour)
                    adjustTimezone(2);
                    break;
                case 2: // 24H / 12H Format
                    cfg.use24HourTime = !cfg.use24HourTime;
                    g_settings.save();
                    break;
                case 3: // Manual Time Set
                    startManualEdit();
                    break;
                case 4: // Auto NTP Sync toggle
                    cfg.autoNtpSync = !cfg.autoNtpSync;
                    g_settings.save();
                    break;
                case 5: // Open Wi-Fi Manager
                    g_system.launchApp(APP_WIFI_SCANNER);
                    break;
            }
        }

        if (_menuIndex < _menuScroll) {
            _menuScroll = _menuIndex;
        } else if (_menuIndex >= _menuScroll + CLK_VISIBLE_ROWS) {
            _menuScroll = _menuIndex - CLK_VISIBLE_ROWS + 1;
        }
        return false;
    }

    // CLK_VIEW_MAIN
    if (ev.action == ACT_BACK && ev.type == EVT_PRESS) {
        return true; // Return to Main Menu
    }

    if ((ev.action == ACT_MENU || ev.rawKey == 'D') && ev.type == EVT_PRESS) {
        if (g_wifi.isConnected()) {
            g_time.requestNtpSync();
            g_system.display().showToast("NTP SYNCING...", 900);
        } else if (g_wifi.hasAnySavedProfile()) {
            g_wifi.connectSaved();
            g_system.display().showToast("CONNECTING WIFI", 900);
        } else {
            g_system.display().showToast("WIFI OFFLINE", 900);
        }
        return false;
    }

    if (ev.action == ACT_LEFT && ev.type == EVT_PRESS) {
        adjustTimezone(-1); // -30 min
    } else if (ev.action == ACT_RIGHT && ev.type == EVT_PRESS) {
        adjustTimezone(1);  // +30 min
    } else if (ev.action == ACT_SELECT && ev.type == EVT_PRESS) {
        _view = CLK_VIEW_MENU;
    }

    return false;
}

void ClockApp::render(DisplayManager& display, InputMode mode, bool showModeBadge) {
    auto& oled = display.oled();
    display.clear();

    if (_view == CLK_VIEW_MANUAL_SET) {
        display.drawHeader("SET TIME/DATE", mode, showModeBadge);

        static const char* const FIELD_LABELS[] = {
            "Hour", "Minute", "Second", "Day", "Month", "Year"
        };
        oled.setCursor(2, 14);
        oled.printf("Field: %s", FIELD_LABELS[_editField]);

        // Time line with active highlight
        oled.setCursor(10, 26);
        oled.printf("%c%02u%c:%c%02u%c:%c%02u%c",
                    _editField == 0 ? '[' : ' ', (unsigned)_editHour,   _editField == 0 ? ']' : ' ',
                    _editField == 1 ? '[' : ' ', (unsigned)_editMinute, _editField == 1 ? ']' : ' ',
                    _editField == 2 ? '[' : ' ', (unsigned)_editSecond, _editField == 2 ? ']' : ' ');

        // Date line with active highlight
        oled.setCursor(6, 38);
        oled.printf("%c%02u%c/%c%02u%c/%c%04u%c",
                    _editField == 3 ? '[' : ' ', (unsigned)_editDay,   _editField == 3 ? ']' : ' ',
                    _editField == 4 ? '[' : ' ', (unsigned)_editMonth, _editField == 4 ? ']' : ' ',
                    _editField == 5 ? '[' : ' ', (unsigned)_editYear,  _editField == 5 ? ']' : ' ');

        display.drawFooter("4/6:Fld 2/8:Set", "5/#:Save");
        return;
    }

    if (_view == CLK_VIEW_MENU) {
        display.drawHeader("CLOCK & NTP", mode, showModeBadge);
        const auto& cfg = g_settings.get();

        char tzBuf[16];
        TimeManager::formatTimezoneOffset(cfg.tzOffsetMinutes, tzBuf, sizeof(tzBuf));

        char items[CLK_MENU_COUNT][24];
        snprintf(items[0], sizeof(items[0]), "1. Sync NTP Now");
        snprintf(items[1], sizeof(items[1]), "2. TZ: %s", tzBuf);
        snprintf(items[2], sizeof(items[2]), "3. Format: %s", cfg.use24HourTime ? "24-Hour" : "12-Hour");
        snprintf(items[3], sizeof(items[3]), "4. Set Time Manual");
        snprintf(items[4], sizeof(items[4]), "5. Auto NTP: %s", cfg.autoNtpSync ? "ON" : "OFF");
        snprintf(items[5], sizeof(items[5]), "6. Wi-Fi Manager >");

        const int startY = 13;
        const int rowH   = 10;
        for (int i = 0; i < CLK_VISIBLE_ROWS; i++) {
            int idx = _menuScroll + i;
            if (idx >= CLK_MENU_COUNT) break;

            int y = startY + (i * rowH);
            if (idx == _menuIndex) {
                oled.fillRect(0, y, OLED_WIDTH - 5, rowH, SSD1306_WHITE);
                oled.setTextColor(SSD1306_BLACK);
            } else {
                oled.setTextColor(SSD1306_WHITE);
            }
            oled.setCursor(3, y + 1);
            oled.print(items[idx]);
        }
        oled.setTextColor(SSD1306_WHITE);
        display.drawScrollbar(startY, CLK_VISIBLE_ROWS * rowH, CLK_MENU_COUNT, CLK_VISIBLE_ROWS, _menuScroll);
        display.drawFooter("*:Back", "5/#:Select");
        return;
    }

    // CLK_VIEW_MAIN
    char tzStr[16];
    TimeManager::formatTimezoneOffset(g_settings.get().tzOffsetMinutes, tzStr, sizeof(tzStr));
    char hdr[24];
    snprintf(hdr, sizeof(hdr), "CLOCK %s", tzStr);
    display.drawHeader(hdr, mode, showModeBadge);

    char timeBuf[16];
    g_time.formatTimeHHMMSS(timeBuf, sizeof(timeBuf));

    // Draw large clock digits
    oled.setTextSize(2);
    int16_t timeW = (int16_t)strlen(timeBuf) * 12;
    int16_t timeX = (OLED_WIDTH - timeW) / 2;
    if (timeX < 2) timeX = 2;
    oled.setCursor(timeX, 14);
    oled.print(timeBuf);
    oled.setTextSize(1);

    // Date and Day of Week line
    if (g_time.isTimeValid()) {
        const auto& dt = g_time.now();
        char dateStr[20];
        g_time.formatDateFull(dateStr, sizeof(dateStr));
        oled.setCursor(4, 33);
        oled.printf("%s, %s", TimeManager::getDayOfWeekShort(dt.dayOfWeek), dateStr);
    } else {
        oled.setCursor(10, 33);
        oled.print(F("Time not synced yet"));
    }

    // NTP & Wi-Fi status line
    oled.setCursor(4, 43);
    oled.printf("%s | WiFi:%s",
                g_time.getSyncStatusLabel(),
                g_wifi.isConnected() ? "OK" : "OFF");

    display.drawFooter("*:Exit D:Sync", "5/#:Menu");
}
