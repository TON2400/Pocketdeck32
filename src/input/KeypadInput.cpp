#include "KeypadInput.h"

static const uint8_t ROW_PINS[KEYPAD_ROWS] = {
    PIN_KP_ROW1, PIN_KP_ROW2, PIN_KP_ROW3, PIN_KP_ROW4
};

static const uint8_t COL_PINS[KEYPAD_COLS] = {
    PIN_KP_COL1, PIN_KP_COL2, PIN_KP_COL3, PIN_KP_COL4
};

static const char KEY_MAP[KEYPAD_ROWS][KEYPAD_COLS] = {
    { '1', '2', '3', 'A' },
    { '4', '5', '6', 'B' },
    { '7', '8', '9', 'C' },
    { '*', '0', '#', 'D' }
};

KeypadInput::KeypadInput()
    : _lastRawKey('\0'),
      _stableKey('\0'),
      _lastScanTime(0),
      _lastDebounceTime(0),
      _pressStartTime(0),
      _longPressFired(false) {}

void KeypadInput::begin() {
    // Configure rows as high-Z inputs when inactive to prevent row-to-row shorts
    for (uint8_t r = 0; r < KEYPAD_ROWS; r++) {
        pinMode(ROW_PINS[r], INPUT_PULLUP);
    }
    // Configure columns with internal pull-up resistors
    for (uint8_t c = 0; c < KEYPAD_COLS; c++) {
        pinMode(COL_PINS[c], INPUT_PULLUP);
    }
    resetState();
}

void KeypadInput::resetState() {
    uint32_t now      = millis();
    _lastRawKey       = '\0';
    _stableKey        = '\0';
    _lastScanTime     = now;
    _lastDebounceTime = now;
    _pressStartTime   = 0;
    _longPressFired   = false;
}

char KeypadInput::scanRawMatrix() {
    char detected = '\0';

    for (uint8_t r = 0; r < KEYPAD_ROWS; r++) {
        // Drive active row LOW
        pinMode(ROW_PINS[r], OUTPUT);
        digitalWrite(ROW_PINS[r], LOW);
        delayMicroseconds(8);

        for (uint8_t c = 0; c < KEYPAD_COLS; c++) {
            if (digitalRead(COL_PINS[c]) == LOW) {
                detected = KEY_MAP[r][c];
                break;
            }
        }

        // Release row directly back to INPUT_PULLUP without driving push-pull HIGH
        pinMode(ROW_PINS[r], INPUT_PULLUP);

        if (detected != '\0') {
            break;
        }
    }

    return detected;
}

InputAction KeypadInput::mapKeyToAction(char key, bool isLongPress) const {
    if (isLongPress) {
        if (key == 'C' || key == 'D' || key == '*') {
            return ACT_HOME;
        }
        if (key == '#') {
            return ACT_SELECT;
        }
    }

    switch (key) {
        case 'A':
        case '2':
            return ACT_UP;
        case 'B':
        case '8':
            return ACT_DOWN;
        case '4':
            return ACT_LEFT;
        case '6':
            return ACT_RIGHT;
        case '5':
        case '#':
            return ACT_SELECT;
        case 'C':
        case '*':
            return ACT_BACK;
        case 'D':
            return ACT_MENU;
        default:
            return ACT_KEY_ONLY;
    }
}

InputEvent KeypadInput::poll() {
    InputEvent ev;
    ev.clear();
    ev.sourceMode = MODE_KEYPAD;

    uint32_t now = millis();
    char raw = _lastRawKey;
    if ((now - _lastScanTime) >= 2UL) {
        _lastScanTime = now;
        raw = scanRawMatrix();
        now = millis();
    }

    if (raw != _lastRawKey) {
        _lastDebounceTime = now;
        _lastRawKey = raw;
    }

    if ((now - _lastDebounceTime) >= KEYPAD_DEBOUNCE_MS) {
        if (raw != _stableKey) {
            char prevKey = _stableKey;
            _stableKey = raw;

            if (_stableKey != '\0') {
                // Clean press edge
                _pressStartTime = now;
                _longPressFired = false;

                ev.type   = EVT_PRESS;
                ev.rawKey = _stableKey;
                ev.digit  = (_stableKey >= '0' && _stableKey <= '9') ? (_stableKey - '0') : -1;
                ev.action = mapKeyToAction(_stableKey, false);
                return ev;
            } else if (prevKey != '\0') {
                // Clean release edge
                ev.type   = EVT_RELEASE;
                ev.rawKey = prevKey;
                ev.digit  = (prevKey >= '0' && prevKey <= '9') ? (prevKey - '0') : -1;
                ev.action = mapKeyToAction(prevKey, false);
                return ev;
            }
        } else if (_stableKey != '\0' && !_longPressFired) {
            if ((now - _pressStartTime) >= KEYPAD_LONG_PRESS_MS) {
                _longPressFired = true;
                ev.type   = EVT_LONG_PRESS;
                ev.rawKey = _stableKey;
                ev.digit  = (_stableKey >= '0' && _stableKey <= '9') ? (_stableKey - '0') : -1;
                ev.action = mapKeyToAction(_stableKey, true);
                return ev;
            }
        }
    }

    return ev;
}
