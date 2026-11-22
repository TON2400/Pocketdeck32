#ifndef POCKETDECK_APP_BASE_H
#define POCKETDECK_APP_BASE_H

#include "../core/InputManager.h"
#include "../core/DisplayManager.h"

class AppBase {
public:
    virtual ~AppBase() {}

    virtual void onEnter() = 0;
    virtual void onExit() {}

    // Process an input event. Return true to exit application back to Main Menu.
    virtual bool handleInput(const InputEvent& ev) = 0;

    // Periodic non-blocking tick. Return true if screen needs redrawing.
    virtual bool update() { return false; }

    // Render application view to OLED buffer.
    virtual void render(DisplayManager& display, InputMode mode, bool showModeBadge) = 0;
};

#endif // POCKETDECK_APP_BASE_H
