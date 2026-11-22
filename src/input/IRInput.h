#ifndef POCKETDECK_IR_INPUT_H
#define POCKETDECK_IR_INPUT_H

#include <Arduino.h>
#include "../core/InputManager.h"

struct IRMappingEntry {
    uint32_t    necCode;
    InputAction action;
    char        rawKey;
    int8_t      digit;
    const char* label;
};

class IRInput {
public:
    IRInput();

    void begin();
    void resetState();
    void discardPending();
    InputEvent poll();

    bool isActionRecentlyHeld(InputAction action) const;
    uint32_t getLastCode() const { return _lastValidCode; }

private:
    const IRMappingEntry* findMapping(uint32_t rawCode, uint16_t addr, uint16_t cmd) const;

    uint32_t    _lastValidCode;
    InputAction _lastAction;
    char        _lastRawKey;
    int8_t      _lastDigit;
    uint32_t    _firstPressMs;
    uint32_t    _lastFrameMs;
    uint32_t    _lastRepeatEmitMs;
    bool        _longPressFired;
};

#endif // POCKETDECK_IR_INPUT_H
