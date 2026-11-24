#ifndef POCKETDECK_OLED_BUTTON_H
#define POCKETDECK_OLED_BUTTON_H

#include <Arduino.h>
#include "../utils/Constants.h"

class OledButton {
public:
    OledButton();

    void begin();
    // Returns true on clean debounced falling-edge press event on GPIO33
    bool pollTogglePressed();

private:
    bool     _lastRawState;
    bool     _stableState;
    uint32_t _lastDebounceTime;
    uint32_t _lastTriggerTime;
};

#endif // POCKETDECK_OLED_BUTTON_H
