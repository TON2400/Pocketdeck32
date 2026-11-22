#include "TextInputManager.h"
#include <string.h>
#include <ctype.h>

// Proposed Character Mapping (Section 6)
static const char* const MAP_KEY_1 = ".,?!1-_/:";
static const char* const MAP_KEY_2 = "ABC2";
static const char* const MAP_KEY_3 = "DEF3";
static const char* const MAP_KEY_4 = "GHI4";
static const char* const MAP_KEY_5 = "JKL5";
static const char* const MAP_KEY_6 = "MNO6";
static const char* const MAP_KEY_7 = "PQRS7";
static const char* const MAP_KEY_8 = "TUV8";
static const char* const MAP_KEY_9 = "WXYZ9";
static const char* const MAP_KEY_0 = " 0";

TextInputManager::TextInputManager()
    : _buffer(nullptr),
      _maxSize(0),
      _length(0),
      _pendingKey('\0'),
      _pendingIndex(0),
      _lastTapMs(0),
      _timeoutMs(MULTITAP_TIMEOUT_MS),
      _caseMode(CASE_LOWER) {}

void TextInputManager::begin(char* targetBuffer, size_t maxBufferSize, const char* initialText) {
    _buffer  = targetBuffer;
    _maxSize = maxBufferSize;
    _pendingKey   = '\0';
    _pendingIndex = 0;
    _lastTapMs    = 0;

    if (!_buffer || _maxSize == 0) {
        _length = 0;
        return;
    }

    if (initialText) {
        strncpy(_buffer, initialText, _maxSize - 1);
        _buffer[_maxSize - 1] = '\0';
        _length = strlen(_buffer);
    } else {
        _buffer[0] = '\0';
        _length = 0;
    }
}

void TextInputManager::reset() {
    _pendingKey   = '\0';
    _pendingIndex = 0;
    _lastTapMs    = 0;
}

void TextInputManager::clearText() {
    reset();
    if (_buffer && _maxSize > 0) {
        _buffer[0] = '\0';
    }
    _length = 0;
}

const char* TextInputManager::getKeyCharset(char key) const {
    switch (key) {
        case '1': return MAP_KEY_1;
        case '2': return MAP_KEY_2;
        case '3': return MAP_KEY_3;
        case '4': return MAP_KEY_4;
        case '5': return MAP_KEY_5;
        case '6': return MAP_KEY_6;
        case '7': return MAP_KEY_7;
        case '8': return MAP_KEY_8;
        case '9': return MAP_KEY_9;
        case '0': return MAP_KEY_0;
        default:  return nullptr;
    }
}

char TextInputManager::getPendingChar() const {
    if (_pendingKey == '\0') return '\0';
    const char* charset = getKeyCharset(_pendingKey);
    if (!charset) return '\0';

    size_t len = strlen(charset);
    if (len == 0) return '\0';
    char c = charset[_pendingIndex % len];
    if (_caseMode == CASE_LOWER && c >= 'A' && c <= 'Z') {
        c = (char)tolower((unsigned char)c);
    }
    return c;
}

void TextInputManager::cycleCaseMode() {
    commitPending();
    if (_caseMode == CASE_LOWER) {
        _caseMode = CASE_UPPER;
    } else if (_caseMode == CASE_UPPER) {
        _caseMode = CASE_NUM;
    } else {
        _caseMode = CASE_LOWER;
    }
}

const char* TextInputManager::getCaseModeLabel() const {
    switch (_caseMode) {
        case CASE_UPPER: return "ABC";
        case CASE_LOWER: return "abc";
        case CASE_NUM:   return "123";
        default:         return "abc";
    }
}

void TextInputManager::commitPending() {
    if (_pendingKey == '\0' || !_buffer) return;

    char c = getPendingChar();
    if (c != '\0' && (_length + 1) < _maxSize) {
        _buffer[_length++] = c;
        _buffer[_length]   = '\0';
    }
    _pendingKey   = '\0';
    _pendingIndex = 0;
}

bool TextInputManager::update() {
    if (_pendingKey != '\0') {
        if ((millis() - _lastTapMs) >= _timeoutMs) {
            commitPending();
            return true;
        }
    }
    return false;
}

TextInputResult TextInputManager::handleInput(const InputEvent& ev) {
    if (!_buffer || _maxSize <= 1) return TEXT_RES_NONE;

    if (ev.type != EVT_PRESS && ev.type != EVT_LONG_PRESS) {
        return TEXT_RES_NONE;
    }

    // Long press '*' clears entire line; Long press 'C' cancels
    if (ev.type == EVT_LONG_PRESS) {
        if (ev.rawKey == '*') {
            clearText();
            return TEXT_RES_CHANGED;
        }
        if (ev.rawKey == 'C' || ev.action == ACT_BACK) {
            commitPending();
            return TEXT_RES_CANCELLED;
        }
        return TEXT_RES_NONE;
    }

    char key = ev.rawKey;

    // 'A' toggles case mode (abc -> ABC -> 123)
    if (key == 'A') {
        cycleCaseMode();
        return TEXT_RES_CHANGED;
    }

    // 'C' cancels/exits text editing
    if (key == 'C' || (ev.sourceMode == MODE_IR && ev.action == ACT_BACK)) {
        commitPending();
        return TEXT_RES_CANCELLED;
    }

    // '#' or 'D' or IR SELECT commits pending char and submits
    if (key == '#' || key == 'D' || (ev.sourceMode == MODE_IR && ev.action == ACT_SELECT)) {
        commitPending();
        return TEXT_RES_SUBMITTED;
    }

    // '*' is Backspace
    if (key == '*') {
        if (_pendingKey != '\0') {
            // Cancel uncommitted pending character
            _pendingKey   = '\0';
            _pendingIndex = 0;
            return TEXT_RES_CHANGED;
        } else if (_length > 0) {
            _buffer[--_length] = '\0';
            return TEXT_RES_CHANGED;
        }
        return TEXT_RES_NONE;
    }

    // Numeric keys '0'..'9'
    if (key >= '0' && key <= '9') {
        // In numeric mode (CASE_NUM) or IR remote mode, commit digit immediately
        if (_caseMode == CASE_NUM) {
            commitPending();
            if ((_length + 1) < _maxSize) {
                _buffer[_length++] = key;
                _buffer[_length]   = '\0';
                return TEXT_RES_CHANGED;
            }
            return TEXT_RES_NONE;
        }

        // Multi-tap cycling
        if (_pendingKey == key && (millis() - _lastTapMs) < _timeoutMs) {
            const char* charset = getKeyCharset(key);
            size_t setLen = charset ? strlen(charset) : 1;
            _pendingIndex = (_pendingIndex + 1) % setLen;
            _lastTapMs    = millis();
            return TEXT_RES_CHANGED;
        } else {
            // Different key pressed: commit previous pending character first
            commitPending();
            if ((_length + 1) < _maxSize) {
                _pendingKey   = key;
                _pendingIndex = 0;
                _lastTapMs    = millis();
                return TEXT_RES_CHANGED;
            }
        }
    }

    return TEXT_RES_NONE;
}

void TextInputManager::drawInputBox(Adafruit_SSD1306& oled, int x, int y, int w, int h, bool multiline) const {
    oled.drawRect(x, y, w, h, SSD1306_WHITE);

    int innerX = x + 3;
    int innerY = y + 3;
    int maxCharsPerLine = (w - 6) / 6;
    if (maxCharsPerLine < 1) maxCharsPerLine = 1;

    char pending = getPendingChar();
    size_t totalDisplayLen = _length + (pending != '\0' ? 1 : 0);

    if (!multiline) {
        // Single-line scrolling view
        size_t startIdx = 0;
        if ((int)totalDisplayLen >= maxCharsPerLine) {
            startIdx = totalDisplayLen - maxCharsPerLine + 1;
        }

        oled.setCursor(innerX, innerY);
        oled.setTextColor(SSD1306_WHITE);
        for (size_t i = startIdx; i < _length; i++) {
            oled.write(_buffer[i]);
        }

        int curX = innerX + (int)(_length - startIdx) * 6;
        if (pending != '\0') {
            // Draw pending character inverted so user clearly sees uncommitted state
            oled.fillRect(curX, innerY - 1, 6, 9, SSD1306_WHITE);
            oled.setTextColor(SSD1306_BLACK);
            oled.setCursor(curX, innerY);
            oled.write(pending);
            oled.setTextColor(SSD1306_WHITE);
        } else {
            // Draw cursor underscore
            oled.drawFastHLine(curX, innerY + 7, 5, SSD1306_WHITE);
        }
    } else {
        // Multi-line wrapped view for Notes editor
        int maxLines = (h - 4) / 9;
        if (maxLines < 1) maxLines = 1;

        int totalLines = ((int)totalDisplayLen / maxCharsPerLine) + 1;
        int firstLine = 0;
        if (totalLines > maxLines) {
            firstLine = totalLines - maxLines;
        }

        size_t startChar = (size_t)(firstLine * maxCharsPerLine);
        int line = 0;
        int col  = 0;

        for (size_t i = startChar; i < _length; i++) {
            int cx = innerX + (col * 6);
            int cy = innerY + (line * 9);
            oled.setCursor(cx, cy);
            oled.setTextColor(SSD1306_WHITE);
            oled.write(_buffer[i]);

            col++;
            if (col >= maxCharsPerLine) {
                col = 0;
                line++;
                if (line >= maxLines) break;
            }
        }

        if (line < maxLines) {
            int cx = innerX + (col * 6);
            int cy = innerY + (line * 9);
            if (pending != '\0') {
                oled.fillRect(cx, cy - 1, 6, 9, SSD1306_WHITE);
                oled.setTextColor(SSD1306_BLACK);
                oled.setCursor(cx, cy);
                oled.write(pending);
                oled.setTextColor(SSD1306_WHITE);
            } else {
                oled.drawFastHLine(cx, cy + 7, 5, SSD1306_WHITE);
            }
        }
    }
}
