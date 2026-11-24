#include "SystemManager.h"
#include "StorageManager.h"
#include "SettingsManager.h"
#include "WiFiManager.h"
#include "TimeManager.h"
#include "../apps/Terminal.h"
#include "../apps/WiFiScanner.h"
#include "../apps/BLEExplorer.h"
#include "../apps/Notes.h"
#include "../apps/FileManager.h"
#include "../apps/Calculator.h"
#include "../apps/SystemMonitor.h"
#include "../apps/SettingsApp.h"
#include "../apps/WeatherApp.h"
#include "../apps/ClockApp.h"
#include "../games/GameManager.h"
#include <stdio.h>

SystemManager g_system;

static TerminalApp      s_terminalApp;
static WiFiScannerApp   s_wifiScannerApp;
static BLEExplorerApp   s_bleExplorerApp;
static NotesApp         s_notesApp;
static FileManagerApp   s_fileManagerApp;
static CalculatorApp    s_calculatorApp;
static GameManagerApp   s_gameManagerApp;
static SystemMonitorApp s_systemMonitorApp;
static SettingsApp      s_settingsApp;
static WeatherApp       s_weatherApp;
static ClockApp         s_clockApp;

SystemManager::SystemManager()
    : _activeAppId(APP_NONE),
      _inBootScreen(true),
      _bootStartMs(0),
      _lastMenuMinute(-1) {
    for (int i = 0; i < APP_COUNT; i++) {
        _apps[i] = nullptr;
    }
}

void SystemManager::registerApp(AppId id, AppBase* app) {
    if (id > APP_NONE && id < APP_COUNT) {
        _apps[id] = app;
    }
}

void SystemManager::begin() {
    Serial.begin(115200);
    randomSeed(esp_random());

    g_settings.begin();
    g_storage.begin();

    _input.begin();
    _input.setMode(g_settings.get().defaultInputMode);

    _display.begin();
    applySettings();

    g_time.begin();
    g_wifi.begin();

    // Register all Main Menu applications
    registerApp(APP_TERMINAL,       &s_terminalApp);
    registerApp(APP_WIFI_SCANNER,   &s_wifiScannerApp);
    registerApp(APP_BLE_EXPLORER,   &s_bleExplorerApp);
    registerApp(APP_NOTES,          &s_notesApp);
    registerApp(APP_FILE_MANAGER,   &s_fileManagerApp);
    registerApp(APP_CALCULATOR,     &s_calculatorApp);
    registerApp(APP_GAME_CENTER,    &s_gameManagerApp);
    registerApp(APP_SYSTEM_MONITOR, &s_systemMonitorApp);
    registerApp(APP_SETTINGS,       &s_settingsApp);
    registerApp(APP_WEATHER,        &s_weatherApp);
    registerApp(APP_CLOCK,          &s_clockApp);

    _bootStartMs  = millis();
    _inBootScreen = true;
    _display.drawBootScreen();
}

void SystemManager::launchApp(AppId id) {
    if (id <= APP_NONE || id >= APP_COUNT) {
        returnToMenu();
        return;
    }

    if (_activeAppId != APP_NONE && _apps[_activeAppId]) {
        _apps[_activeAppId]->onExit();
    }

    _activeAppId = id;
    if (_apps[_activeAppId]) {
        _apps[_activeAppId]->onEnter();
    }
    _display.markDirty();
}

void SystemManager::returnToMenu() {
    if (_activeAppId != APP_NONE && _apps[_activeAppId]) {
        _apps[_activeAppId]->onExit();
    }
    _activeAppId = APP_NONE;
    _input.setModeSwitchLocked(false);
    _display.markDirty();
}

void SystemManager::applySettings() {
    _display.setContrast(g_settings.getContrastValue());
    _display.setInverted(g_settings.get().invertDisplay);
    _display.markDirty();
}

void SystemManager::update() {
    uint32_t now = millis();

    // 0. Always update non-blocking Wi-Fi Manager and NTP Time Manager
    g_wifi.update();
    g_time.update();

    // 1. Poll dedicated physical OLED Power Toggle Button (GPIO33)
    //    Independent of GPIO4 Mode Selector, Keypad, and IR.
    //    Preserves all current application, menu, and UI state.
    if (_input.pollOledPowerButton()) {
        if (_display.isPoweredOn()) {
            _display.setPower(false);
        } else {
            _display.setPower(true);
            _input.recordActivity();
            _display.markDirty();
        }
        return;
    }

    // 2. Handle non-blocking boot screen
    if (_inBootScreen) {
        InputEvent bootEv = _input.poll(_display.isPoweredOn());
        if ((now - _bootStartMs) >= 1200 || (bootEv.isValid() && bootEv.type == EVT_PRESS)) {
            _inBootScreen = false;
            _display.markDirty();
        } else {
            return;
        }
    }

    // 3. Poll unified Input Manager (Keypad / IR / GPIO4 Mode Selector)
    InputEvent ev = _input.poll(_display.isPoweredOn());

    if (ev.isValid() && ev.type != EVT_RELEASE) {
        // Wake screen if sleeping due to timeout or manual toggle without triggering app actions
        if (!_display.isPoweredOn()) {
            _display.setPower(true);
            _input.recordActivity();
            _display.markDirty();
            return;
        }

        if (ev.type == EVT_MODE_CHANGED) {
            char msg[24];
            snprintf(msg, sizeof(msg), "MODE: %s", _input.getModeFullName());
            _display.showToast(msg, 900);
        } else if (ev.action == ACT_HOME && (ev.type == EVT_PRESS || ev.type == EVT_LONG_PRESS)) {
            if (_activeAppId != APP_NONE) {
                returnToMenu();
            }
        } else if (_activeAppId == APP_NONE) {
            bool redraw = false;
            AppId chosen = _menu.handleInput(ev, redraw);
            if (redraw) {
                _display.markDirty();
            }
            if (chosen != APP_NONE) {
                launchApp(chosen);
            }
        } else {
            AppBase* app = _apps[_activeAppId];
            if (app) {
                bool shouldExit = app->handleInput(ev);
                if (shouldExit) {
                    returnToMenu();
                } else {
                    _display.markDirty();
                }
            } else if (ev.action == ACT_BACK && ev.type == EVT_PRESS) {
                returnToMenu();
            }
        }
    }

    // 4. Screen timeout handling (refresh timestamp after input polling to prevent unsigned underflow)
    uint16_t timeoutSec = g_settings.get().screenTimeoutSec;
    if (timeoutSec > 0 && _display.isPoweredOn()) {
        uint32_t checkNow = millis();
        uint32_t lastAct  = _input.getLastActivityMs();
        if ((int32_t)(checkNow - lastAct) >= (int32_t)((uint32_t)timeoutSec * 1000UL)) {
            _display.setPower(false);
        }
    }

    // 5. Periodic update for active app or Main Menu clock header
    if (_activeAppId != APP_NONE && _apps[_activeAppId]) {
        if (_apps[_activeAppId]->update()) {
            _display.markDirty();
        }
    } else if (_activeAppId == APP_NONE && g_time.isTimeValid()) {
        int8_t curMin = (int8_t)g_time.now().minute;
        if (curMin != _lastMenuMinute) {
            _lastMenuMinute = curMin;
            _display.markDirty();
        }
    }

    // 6. Update toast expiration
    _display.updateToastState();

    // 7. Render if dirty and OLED is powered ON
    if (_display.isDirty() && _display.isPoweredOn()) {
        bool showBadge = g_settings.get().showModeIndicator;
        if (_activeAppId == APP_NONE) {
            _menu.render(_display, _input.getMode(), showBadge);
        } else if (_apps[_activeAppId]) {
            _apps[_activeAppId]->render(_display, _input.getMode(), showBadge);
        } else {
            _display.clear();
            _display.drawHeader("APP", _input.getMode(), showBadge);
            _display.oled().setCursor(10, 28);
            _display.oled().print(F("Not loaded"));
            _display.drawFooter("*:Back", nullptr);
        }
        _display.present();
    }
}
