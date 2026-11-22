#ifndef POCKETDECK_TEXT_INPUT_MANAGER_H
#define POCKETDECK_TEXT_INPUT_MANAGER_H

#include <Arduino.h>
#include "../utils/Constants.h"
#include "InputManager.h"
#include "DisplayManager.h"

enum TextCaseMode : uint8_t {
    CASE_UPPER = 0,
    CASE_LOWER = 1,
    CASE_NUM   = 2
};

enum TextInputResult : uint8_t {
    TEXT_RES_NONE = 0,
    TEXT_RES_CHANGED,
    TEXT_RES_SUBMITTED,
    TEXT_RES_CANCELLED
};

class TextInputManager {
public:
    TextInputManager();

    void begin(char* targetBuffer, size_t maxBufferSize, const char* initialText = nullptr);
    void reset();
    void clearText();

    // Non-blocking timeout check to auto-commit pending multi-tap character.
    // Returns true if a pending character was committed and display needs refresh.
    bool update();

    // Process keypad / IR input event
    TextInputResult handleInput(const InputEvent& ev);

    // Commit any active pending character immediately
    void commitPending();

    const char* getText() const { return _buffer ? _buffer : ""; }
    size_t getLength() const { return _length; }
    bool hasPendingChar() const { return _pendingKey != '\0'; }
    char getPendingChar() const;

    TextCaseMode getCaseMode() const { return _caseMode; }
    void cycleCaseMode();
    const char* getCaseModeLabel() const;

    void setTimeoutMs(uint32_t timeoutMs) { _timeoutMs = timeoutMs; }
    uint32_t getTimeoutMs() const { return _timeoutMs; }

    // Reusable UI helper to draw a text input box with cursor & pending highlight
    void drawInputBox(Adafruit_SSD1306& oled, int x, int y, int w, int h, bool multiline = false) const;

private:
    const char* getKeyCharset(char key) const;

    char*        _buffer;
    size_t       _maxSize;
    size_t       _length;

    char         _pendingKey;
    uint8_t      _pendingIndex;
    uint32_t     _lastTapMs;
    uint32_t     _timeoutMs;
    TextCaseMode _caseMode;
};

#endif // POCKETDECK_TEXT_INPUT_MANAGER_H
