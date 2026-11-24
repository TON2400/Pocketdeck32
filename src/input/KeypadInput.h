#ifndef POCKETDECK_KEYPAD_INPUT_H
#define POCKETDECK_KEYPAD_INPUT_H

#include <Arduino.h>
#include "../core/InputManager.h"

class KeypadInput {
public:
    KeypadInput();

    void begin();
    void resetState();
    InputEvent poll();

    char getHeldKey() const { return _stableKey; }
    InputAction mapKeyToAction(char key, bool isLongPress) const;

private:
    char scanRawMatrix();

    char     _lastRawKey;
    char     _stableKey;
    uint32_t _lastScanTime;
    uint32_t _lastDebounceTime;
    uint32_t _pressStartTime;
    bool     _longPressFired;
};

#endif // POCKETDECK_KEYPAD_INPUT_H
