#ifndef POCKETDECK_SYSTEM_MANAGER_H
#define POCKETDECK_SYSTEM_MANAGER_H

#include <Arduino.h>
#include "../utils/Constants.h"
#include "InputManager.h"
#include "DisplayManager.h"
#include "MenuManager.h"
#include "../apps/AppBase.h"

class SystemManager {
public:
    SystemManager();

    void begin();
    void update();

    void launchApp(AppId id);
    void returnToMenu();

    InputManager&   input()   { return _input; }
    DisplayManager& display() { return _display; }
    MenuManager&    menu()    { return _menu; }

    void registerApp(AppId id, AppBase* app);

    // Settings hooks applied by SettingsManager
    void applySettings();

private:
    InputManager   _input;
    DisplayManager _display;
    MenuManager    _menu;

    AppBase*       _apps[APP_COUNT];
    AppId          _activeAppId;
    bool           _inBootScreen;
    uint32_t       _bootStartMs;
    int8_t         _lastMenuMinute;
};

extern SystemManager g_system;

#endif // POCKETDECK_SYSTEM_MANAGER_H
