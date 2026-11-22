#include "ModeSelector.h"

ModeSelector::ModeSelector()
    : _lastRawState(HIGH),
      _stableState(HIGH),
      _lastDebounceTime(0),
      _lastTriggerTime(0) {}

void ModeSelector::begin() {
    // Physical Mode Selector Button on GPIO4 with internal pull-up (active LOW)
    pinMode(PIN_MODE_BTN, INPUT_PULLUP);
    _lastRawState = digitalRead(PIN_MODE_BTN);
    _stableState  = _lastRawState;
}

bool ModeSelector::pollTogglePressed() {
    bool currentRaw = digitalRead(PIN_MODE_BTN);
    uint32_t now = millis();

    if (currentRaw != _lastRawState) {
        _lastDebounceTime = now;
        _lastRawState = currentRaw;
    }

    if ((now - _lastDebounceTime) >= MODE_BTN_DEBOUNCE_MS) {
        if (currentRaw != _stableState) {
            _stableState = currentRaw;
            // Falling edge detection (HIGH -> LOW press) with cooldown guard
            if (_stableState == LOW) {
                if ((now - _lastTriggerTime) >= MODE_BTN_COOLDOWN_MS) {
                    _lastTriggerTime = now;
                    return true;
                }
            }
        }
    }
    return false;
}
