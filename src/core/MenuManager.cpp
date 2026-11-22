#include "MenuManager.h"

static const MenuItem MAIN_MENU_ITEMS[] = {
    { APP_TERMINAL,       "1. Terminal"       },
    { APP_WIFI_SCANNER,   "2. Wi-Fi Scanner"  },
    { APP_BLE_EXPLORER,   "3. BLE Explorer"   },
    { APP_NOTES,          "4. Notes"          },
    { APP_FILE_MANAGER,   "5. File Manager"   },
    { APP_CALCULATOR,     "6. Calculator"     },
    { APP_GAME_CENTER,    "7. Game Center"    },
    { APP_SYSTEM_MONITOR, "8. System Monitor" },
    { APP_SETTINGS,       "9. Settings"       }
};

static const int MAIN_MENU_COUNT   = sizeof(MAIN_MENU_ITEMS) / sizeof(MAIN_MENU_ITEMS[0]);
static const int MENU_VISIBLE_ROWS = 4;

MenuManager::MenuManager()
    : _selectedIndex(0),
      _scrollOffset(0) {}

void MenuManager::reset() {
    _selectedIndex = 0;
    _scrollOffset  = 0;
}

void MenuManager::setSelectedIndex(int idx) {
    if (idx < 0) idx = 0;
    if (idx >= MAIN_MENU_COUNT) idx = MAIN_MENU_COUNT - 1;
    _selectedIndex = idx;

    if (_selectedIndex < _scrollOffset) {
        _scrollOffset = _selectedIndex;
    } else if (_selectedIndex >= _scrollOffset + MENU_VISIBLE_ROWS) {
        _scrollOffset = _selectedIndex - MENU_VISIBLE_ROWS + 1;
    }
}

AppId MenuManager::handleInput(const InputEvent& ev, bool& outNeedsRedraw) {
    outNeedsRedraw = false;

    if (ev.type != EVT_PRESS && ev.type != EVT_REPEAT) {
        return APP_NONE;
    }

    // In IR Mode, number buttons 1-9 jump directly to the corresponding app.
    // In Keypad Mode, A/B or 2/8 move up/down and 5/# selects, while 1/3/4/6/7/9 can also jump or navigate.
    if (ev.sourceMode == MODE_IR && ev.type == EVT_PRESS && ev.digit >= 1 && ev.digit <= MAIN_MENU_COUNT) {
        setSelectedIndex(ev.digit - 1);
        outNeedsRedraw = true;
        return MAIN_MENU_ITEMS[_selectedIndex].id;
    }

    if (ev.action == ACT_UP) {
        int next = (_selectedIndex - 1 + MAIN_MENU_COUNT) % MAIN_MENU_COUNT;
        setSelectedIndex(next);
        outNeedsRedraw = true;
        return APP_NONE;
    }

    if (ev.action == ACT_DOWN) {
        int next = (_selectedIndex + 1) % MAIN_MENU_COUNT;
        setSelectedIndex(next);
        outNeedsRedraw = true;
        return APP_NONE;
    }

    if (ev.action == ACT_SELECT && ev.type == EVT_PRESS) {
        outNeedsRedraw = true;
        return MAIN_MENU_ITEMS[_selectedIndex].id;
    }

    return APP_NONE;
}

void MenuManager::render(DisplayManager& display, InputMode mode, bool showModeBadge) {
    auto& oled = display.oled();
    display.clear();
    display.drawHeader("POCKETDECK", mode, showModeBadge);

    const int startY = 13;
    const int rowH   = 10;

    for (int i = 0; i < MENU_VISIBLE_ROWS; i++) {
        int itemIdx = _scrollOffset + i;
        if (itemIdx >= MAIN_MENU_COUNT) break;

        int y = startY + (i * rowH);
        bool selected = (itemIdx == _selectedIndex);

        if (selected) {
            oled.fillRect(0, y, OLED_WIDTH - 5, rowH, SSD1306_WHITE);
            oled.setTextColor(SSD1306_BLACK);
        } else {
            oled.setTextColor(SSD1306_WHITE);
        }

        oled.setCursor(3, y + 1);
        oled.print(MAIN_MENU_ITEMS[itemIdx].label);
    }

    oled.setTextColor(SSD1306_WHITE);
    display.drawScrollbar(startY, MENU_VISIBLE_ROWS * rowH, MAIN_MENU_COUNT, MENU_VISIBLE_ROWS, _scrollOffset);

    if (mode == MODE_KEYPAD) {
        display.drawFooter("A/B,2/8:Nav", "5/#:Open");
    } else {
        display.drawFooter("VOL:Nav", "CH+/1-9:Open");
    }
}
