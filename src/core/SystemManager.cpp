#include "SystemManager.h"
#include "StorageManager.h"
#include "SettingsManager.h"
#include "../apps/Terminal.h"
#include "../apps/WiFiScanner.h"
#include "../apps/BLEExplorer.h"
#include "../apps/Notes.h"
#include "../apps/FileManager.h"
#include "../apps/Calculator.h"
#include "../apps/SystemMonitor.h"
#include "../apps/SettingsApp.h"
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

SystemManager::SystemManager()
    : _activeAppId(APP_NONE),
      _inBootScreen(true),
      _bootStartMs(0) {
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

    // Register all 9 Main Menu applications
    registerApp(APP_TERMINAL,       &s_terminalApp);
    registerApp(APP_WIFI_SCANNER,   &s_wifiScannerApp);
    registerApp(APP_BLE_EXPLORER,   &s_bleExplorerApp);
    registerApp(APP_NOTES,          &s_notesApp);
    registerApp(APP_FILE_MANAGER,   &s_fileManagerApp);
    registerApp(APP_CALCULATOR,     &s_calculatorApp);
    registerApp(APP_GAME_CENTER,    &s_gameManagerApp);
    registerApp(APP_SYSTEM_MONITOR, &s_systemMonitorApp);
    registerApp(APP_SETTINGS,       &s_settingsApp);

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

    // 1. Handle non-blocking boot screen
    if (_inBootScreen) {
        InputEvent bootEv = _input.poll();
        if ((now - _bootStartMs) >= 1200 || (bootEv.isValid() && bootEv.type == EVT_PRESS)) {
            _inBootScreen = false;
            _display.markDirty();
        } else {
            return;
        }
    }

    // 2. Poll unified Input Manager
    InputEvent ev = _input.poll();

    if (ev.isValid()) {
        // Wake screen if sleeping due to timeout
        if (!_display.isPoweredOn()) {
            _display.setPower(true);
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

    // 3. Screen timeout handling
    uint16_t timeoutSec = g_settings.get().screenTimeoutSec;
    if (timeoutSec > 0 && _display.isPoweredOn()) {
        if ((now - _input.getLastActivityMs()) >= ((uint32_t)timeoutSec * 1000UL)) {
            _display.setPower(false);
        }
    }

    // 4. Periodic update for active app
    if (_activeAppId != APP_NONE && _apps[_activeAppId]) {
        if (_apps[_activeAppId]->update()) {
            _display.markDirty();
        }
    }

    // 5. Update toast expiration
    _display.updateToastState();

    // 6. Render if dirty
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
