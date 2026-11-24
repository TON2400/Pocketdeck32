#ifndef POCKETDECK_INPUT_MANAGER_H
#define POCKETDECK_INPUT_MANAGER_H

#include <Arduino.h>
#include "../utils/Constants.h"

enum InputMode : uint8_t {
    MODE_KEYPAD = 0,
    MODE_IR     = 1
};

enum InputAction : uint8_t {
    ACT_NONE = 0,
    ACT_UP,
    ACT_DOWN,
    ACT_LEFT,
    ACT_RIGHT,
    ACT_SELECT,
    ACT_BACK,
    ACT_HOME,
    ACT_MENU,
    ACT_KEY_ONLY
};

enum InputEventType : uint8_t {
    EVT_NONE = 0,
    EVT_PRESS,
    EVT_RELEASE,
    EVT_LONG_PRESS,
    EVT_REPEAT,
    EVT_MODE_CHANGED
};

struct InputEvent {
    InputEventType type;
    InputAction    action;
    char           rawKey;     // '0'-'9', 'A'-'D', '*', '#', or '\0'
    int8_t         digit;      // 0-9 if numeric key, else -1
    InputMode      sourceMode;

    void clear() {
        type       = EVT_NONE;
        action     = ACT_NONE;
        rawKey     = '\0';
        digit      = -1;
        sourceMode = MODE_KEYPAD;
    }

    bool isValid() const {
        return type != EVT_NONE;
    }
};

class InputManager {
public:
    InputManager();

    void begin();
    InputEvent poll(bool displayPoweredOn = true);

    // Poll dedicated physical OLED power toggle button (GPIO33)
    bool pollOledPowerButton();

    // Reset inactivity timeout timestamp (e.g. when waking OLED)
    void recordActivity();

    InputMode getMode() const { return _currentMode; }
    void setMode(InputMode mode);
    const char* getModeShortName() const;
    const char* getModeFullName() const;

    void setModeSwitchLocked(bool locked) { _modeSwitchLocked = locked; }
    bool isModeSwitchLocked() const { return _modeSwitchLocked; }

    // Check if a directional key is currently held down (useful for real-time games like Pong)
    bool isActionHeld(InputAction action) const;

    uint32_t getLastActivityMs() const { return _lastActivityMs; }

private:
    InputMode _currentMode;
    bool      _modeSwitchLocked;
    uint32_t  _lastActivityMs;
};

#endif // POCKETDECK_INPUT_MANAGER_H
