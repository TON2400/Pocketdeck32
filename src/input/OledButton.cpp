#include "OledButton.h"

OledButton::OledButton()
    : _lastRawState(HIGH),
      _stableState(HIGH),
      _lastDebounceTime(0),
      _lastTriggerTime(0) {}

void OledButton::begin() {
    // Dedicated physical OLED ON/OFF Toggle Button on GPIO33 with internal pull-up (active LOW)
    pinMode(PIN_OLED_BTN, INPUT_PULLUP);
    _lastRawState = (digitalRead(PIN_OLED_BTN) != LOW);
    _stableState  = _lastRawState;
}

bool OledButton::pollTogglePressed() {
    bool currentRaw = (digitalRead(PIN_OLED_BTN) != LOW);
    uint32_t now = millis();

    if (currentRaw != _lastRawState) {
        _lastDebounceTime = now;
        _lastRawState = currentRaw;
    }

    if ((now - _lastDebounceTime) >= OLED_BTN_DEBOUNCE_MS) {
        if (currentRaw != _stableState) {
            _stableState = currentRaw;
            // Falling edge detection (HIGH -> LOW press) with cooldown guard
            if (_stableState == LOW) {
                if ((now - _lastTriggerTime) >= OLED_BTN_COOLDOWN_MS) {
                    _lastTriggerTime = now;
                    return true;
                }
            }
        }
    }
    return false;
}
