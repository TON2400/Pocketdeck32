#ifndef POCKETDECK_MENU_MANAGER_H
#define POCKETDECK_MENU_MANAGER_H

#include <Arduino.h>
#include "../utils/Constants.h"
#include "InputManager.h"
#include "DisplayManager.h"

struct MenuItem {
    AppId       id;
    const char* label;
};

class MenuManager {
public:
    MenuManager();

    void reset();
    // Process menu navigation. Returns selected AppId, or APP_NONE if still in menu.
    AppId handleInput(const InputEvent& ev, bool& outNeedsRedraw);
    void render(DisplayManager& display, InputMode mode, bool showModeBadge);

    int getSelectedIndex() const { return _selectedIndex; }
    void setSelectedIndex(int idx);

private:
    int _selectedIndex;
    int _scrollOffset;
};

#endif // POCKETDECK_MENU_MANAGER_H
