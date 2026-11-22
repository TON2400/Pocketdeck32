#include "IRInput.h"
#include "../utils/Helpers.h"

// Configure IRremote before including header-only implementation
#define DECODE_NEC
#define NO_LED_FEEDBACK_CODE
#include <IRremote.hpp>

// Centralized, configurable NEC IR mapping table (Section 4)
static const IRMappingEntry IR_KEY_TABLE[] = {
    { IR_CODE_CH_MINUS,   ACT_BACK,     '*',  -1, "CH-"        },
    { IR_CODE_CH,         ACT_HOME,     '\0', -1, "CH"         },
    { IR_CODE_CH_PLUS,    ACT_SELECT,   '#',  -1, "CH+"        },
    { IR_CODE_PREV,       ACT_LEFT,     '\0', -1, "PREV"       },
    { IR_CODE_NEXT,       ACT_RIGHT,    '\0', -1, "NEXT"       },
    { IR_CODE_PLAY_PAUSE, ACT_SELECT,   '#',  -1, "PLAY/PAUSE" },
    { IR_CODE_VOL_MINUS,  ACT_DOWN,     '\0', -1, "VOL-"       },
    { IR_CODE_VOL_PLUS,   ACT_UP,       '\0', -1, "VOL+"       },
    { IR_CODE_EQ,         ACT_MENU,     'D',  -1, "EQ"         },
    { IR_CODE_0,          ACT_KEY_ONLY, '0',   0, "0"          },
    { IR_CODE_100_PLUS,   ACT_KEY_ONLY, '*',  -1, "100+"       },
    { IR_CODE_200_PLUS,   ACT_KEY_ONLY, '#',  -1, "200+"       },
    { IR_CODE_1,          ACT_KEY_ONLY, '1',   1, "1"          },
    { IR_CODE_2,          ACT_KEY_ONLY, '2',   2, "2"          },
    { IR_CODE_3,          ACT_KEY_ONLY, '3',   3, "3"          },
    { IR_CODE_4,          ACT_KEY_ONLY, '4',   4, "4"          },
    { IR_CODE_5,          ACT_KEY_ONLY, '5',   5, "5"          },
    { IR_CODE_6,          ACT_KEY_ONLY, '6',   6, "6"          },
    { IR_CODE_7,          ACT_KEY_ONLY, '7',   7, "7"          },
    { IR_CODE_8,          ACT_KEY_ONLY, '8',   8, "8"          },
    { IR_CODE_9,          ACT_KEY_ONLY, '9',   9, "9"          }
};

static const size_t IR_KEY_TABLE_SIZE = sizeof(IR_KEY_TABLE) / sizeof(IR_KEY_TABLE[0]);

IRInput::IRInput()
    : _lastValidCode(0),
      _lastAction(ACT_NONE),
      _lastRawKey('\0'),
      _lastDigit(-1),
      _firstPressMs(0),
      _lastFrameMs(0),
      _lastRepeatEmitMs(0),
      _longPressFired(false) {}

void IRInput::begin() {
    IrReceiver.begin(PIN_IR_RECV, DISABLE_LED_FEEDBACK);
    resetState();
}

void IRInput::resetState() {
    _lastValidCode    = 0;
    _lastAction       = ACT_NONE;
    _lastRawKey       = '\0';
    _lastDigit        = -1;
    _firstPressMs     = 0;
    _lastFrameMs      = 0;
    _lastRepeatEmitMs = 0;
    _longPressFired   = false;
}

void IRInput::discardPending() {
    if (IrReceiver.decode()) {
        IrReceiver.resume();
    }
}

const IRMappingEntry* IRInput::findMapping(uint32_t rawCode, uint16_t addr, uint16_t cmd) const {
    // Reconstruct standard 32-bit MSB-first NEC format (0x00FFxxxx) from decoded address & command
    uint8_t addrMsb = Helpers::reverseBits8((uint8_t)(addr & 0xFF));
    uint8_t cmdMsb  = Helpers::reverseBits8((uint8_t)(cmd & 0xFF));
    uint32_t msbConstructed = ((uint32_t)addrMsb << 24) |
                              ((uint32_t)((~addrMsb) & 0xFF) << 16) |
                              ((uint32_t)cmdMsb << 8) |
                              ((uint32_t)((~cmdMsb) & 0xFF));

    for (size_t i = 0; i < IR_KEY_TABLE_SIZE; i++) {
        if (IR_KEY_TABLE[i].necCode == rawCode || IR_KEY_TABLE[i].necCode == msbConstructed) {
            return &IR_KEY_TABLE[i];
        }
    }
    return nullptr;
}

bool IRInput::isActionRecentlyHeld(InputAction action) const {
    if (_lastAction == ACT_NONE || _lastAction != action) {
        return false;
    }
    return (millis() - _lastFrameMs) <= 220;
}

InputEvent IRInput::poll() {
    InputEvent ev;
    ev.clear();
    ev.sourceMode = MODE_IR;

    if (!IrReceiver.decode()) {
        return ev;
    }

    uint32_t now      = millis();
    auto&    irData   = IrReceiver.decodedIRData;
    bool     isRepeat = (irData.flags & IRDATA_FLAGS_IS_REPEAT) != 0;
    auto     protocol = irData.protocol;
    uint32_t rawData  = irData.decodedRawData;
    uint16_t addr     = irData.address;
    uint16_t cmd      = irData.command;

    IrReceiver.resume();

    // Only accept NEC protocol frames
    if (protocol != NEC) {
        return ev;
    }

    if (isRepeat) {
        // Ignore orphan repeat frames if gap is too large (>250ms) or no active button
        if (_lastValidCode == 0 || (now - _lastFrameMs) > 250) {
            resetState();
            return ev;
        }
        _lastFrameMs = now;
        uint32_t heldDuration = now - _firstPressMs;

        // Controlled auto-repeat only for directional navigation keys after initial 480ms hold
        if (_lastAction == ACT_UP || _lastAction == ACT_DOWN ||
            _lastAction == ACT_LEFT || _lastAction == ACT_RIGHT) {
            if (heldDuration >= 480 && (now - _lastRepeatEmitMs) >= 160) {
                _lastRepeatEmitMs = now;
                ev.type   = EVT_REPEAT;
                ev.action = _lastAction;
                ev.rawKey = _lastRawKey;
                ev.digit  = _lastDigit;
                return ev;
            }
        } else if (!_longPressFired && heldDuration >= 650) {
            // Single long-press event for non-directional keys
            _longPressFired = true;
            ev.type   = EVT_LONG_PRESS;
            ev.action = _lastAction;
            ev.rawKey = _lastRawKey;
            ev.digit  = _lastDigit;
            return ev;
        }
        return ev;
    }

    // Non-repeat frame: lookup in predefined NEC mapping table
    const IRMappingEntry* entry = findMapping(rawData, addr, cmd);
    if (!entry) {
        // Ignore unknown IR codes safely
        resetState();
        return ev;
    }

    _lastValidCode    = entry->necCode;
    _lastAction       = entry->action;
    _lastRawKey       = entry->rawKey;
    _lastDigit        = entry->digit;
    _firstPressMs     = now;
    _lastFrameMs      = now;
    _lastRepeatEmitMs = now;
    _longPressFired   = false;

    ev.type   = EVT_PRESS;
    ev.action = entry->action;
    ev.rawKey = entry->rawKey;
    ev.digit  = entry->digit;
    return ev;
}
