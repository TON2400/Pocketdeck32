#include "InputManager.h"
#include "../input/ModeSelector.h"
#include "../input/OledButton.h"
#include "../input/KeypadInput.h"
#include "../input/IRInput.h"

static ModeSelector s_modeSelector;
static OledButton   s_oledButton;
static KeypadInput  s_keypadInput;
static IRInput      s_irInput;

InputManager::InputManager()
    : _currentMode(MODE_KEYPAD),
      _modeSwitchLocked(false),
      _lastActivityMs(0) {}

void InputManager::begin() {
    s_modeSelector.begin();
    s_oledButton.begin();
    s_keypadInput.begin();
    s_irInput.begin();
    _lastActivityMs = millis();
}

bool InputManager::pollOledPowerButton() {
    return s_oledButton.pollTogglePressed();
}

void InputManager::recordActivity() {
    _lastActivityMs = millis();
}

void InputManager::setMode(InputMode mode) {
    if (_currentMode != mode) {
        _currentMode = mode;
        s_keypadInput.resetState();
        s_irInput.resetState();
        s_irInput.discardPending();
        _lastActivityMs = millis();
    }
}

const char* InputManager::getModeShortName() const {
    return (_currentMode == MODE_KEYPAD) ? "KP" : "IR";
}

const char* InputManager::getModeFullName() const {
    return (_currentMode == MODE_KEYPAD) ? "KEYPAD" : "IR REMOTE";
}

bool InputManager::isActionHeld(InputAction action) const {
    if (_currentMode == MODE_KEYPAD) {
        char held = s_keypadInput.getHeldKey();
        if (held == '\0') return false;
        return s_keypadInput.mapKeyToAction(held, false) == action;
    } else {
        return s_irInput.isActionRecentlyHeld(action);
    }
}

InputEvent InputManager::poll(bool displayPoweredOn) {
    // 1. Check physical mode selector button (GPIO4)
    if (s_modeSelector.pollTogglePressed()) {
        // Do not change input mode while OLED display is turned off
        if (displayPoweredOn && !_modeSwitchLocked) {
            InputMode nextMode = (_currentMode == MODE_KEYPAD) ? MODE_IR : MODE_KEYPAD;
            setMode(nextMode);

            InputEvent modeEv;
            modeEv.clear();
            modeEv.type       = EVT_MODE_CHANGED;
            modeEv.action     = ACT_NONE;
            modeEv.sourceMode = _currentMode;
            return modeEv;
        }
    }

    // 2. Poll only the active input source; ignore inactive source
    InputEvent ev;
    ev.clear();

    if (_currentMode == MODE_KEYPAD) {
        // Drain any IR signal silently so it never buffers or triggers
        s_irInput.discardPending();
        ev = s_keypadInput.poll();
    } else {
        ev = s_irInput.poll();
    }

    if (ev.isValid()) {
        _lastActivityMs = millis();
    }

    return ev;
}
