#ifndef POCKETDECK_MODE_SELECTOR_H
#define POCKETDECK_MODE_SELECTOR_H

#include <Arduino.h>
#include "../utils/Constants.h"

class ModeSelector {
public:
    ModeSelector();

    void begin();
    // Returns true on clean debounced falling-edge press event
    bool pollTogglePressed();

private:
    bool     _lastRawState;
    bool     _stableState;
    uint32_t _lastDebounceTime;
    uint32_t _lastTriggerTime;
};

#endif // POCKETDECK_MODE_SELECTOR_H
