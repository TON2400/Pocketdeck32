#include "Calculator.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>

static const char* const SOFT_BTNS[8] = {
    "+", "-", "*", "/", ".", "=", "C", "X"
};

CalculatorApp::CalculatorApp() {
    clearAll();
}

void CalculatorApp::onEnter() {
    clearAll();
}

void CalculatorApp::clearAll() {
    strcpy(_inputBuf, "0");
    _accumulator = 0.0;
    _pendingOp   = '\0';
    _newEntry    = true;
    _hasError    = false;
    _errorMsg[0] = '\0';
    _softBtnIdx  = 0;
}

void CalculatorApp::appendDigit(char d) {
    if (_hasError) {
        clearAll();
    }
    if (_newEntry) {
        _inputBuf[0] = d;
        _inputBuf[1] = '\0';
        _newEntry    = false;
    } else {
        size_t len = strlen(_inputBuf);
        if (len == 1 && _inputBuf[0] == '0') {
            _inputBuf[0] = d;
        } else if (len < 13) {
            _inputBuf[len]     = d;
            _inputBuf[len + 1] = '\0';
        }
    }
}

void CalculatorApp::appendDecimal() {
    if (_hasError) {
        clearAll();
    }
    if (_newEntry) {
        strcpy(_inputBuf, "0.");
        _newEntry = false;
        return;
    }
    if (!strchr(_inputBuf, '.')) {
        size_t len = strlen(_inputBuf);
        if (len < 12) {
            _inputBuf[len]     = '.';
            _inputBuf[len + 1] = '\0';
        }
    }
}

void CalculatorApp::backspace() {
    if (_hasError) {
        clearAll();
        return;
    }
    if (_newEntry) return;

    size_t len = strlen(_inputBuf);
    if (len > 1) {
        _inputBuf[len - 1] = '\0';
    } else {
        strcpy(_inputBuf, "0");
        _newEntry = true;
    }
}

void CalculatorApp::selectOperator(char op) {
    if (_hasError) {
        clearAll();
    }
    if (_pendingOp != '\0' && !_newEntry) {
        evaluate();
        if (_hasError) return;
    } else {
        _accumulator = atof(_inputBuf);
    }
    _pendingOp = op;
    _newEntry  = true;
}

void CalculatorApp::evaluate() {
    if (_hasError || _pendingOp == '\0') return;

    double rhs = atof(_inputBuf);
    double res = 0.0;

    switch (_pendingOp) {
        case '+': res = _accumulator + rhs; break;
        case '-': res = _accumulator - rhs; break;
        case '*': res = _accumulator * rhs; break;
        case '/':
            if (fabs(rhs) < 1e-12) {
                _hasError = true;
                strcpy(_errorMsg, "ERR: DIV BY 0");
                _pendingOp = '\0';
                return;
            }
            res = _accumulator / rhs;
            break;
        default:
            return;
    }

    if (isnan(res) || isinf(res) || fabs(res) > 999999999999.0) {
        _hasError = true;
        strcpy(_errorMsg, "ERR: OVERFLOW");
        _pendingOp = '\0';
        return;
    }

    _accumulator = res;
    _pendingOp   = '\0';
    _newEntry    = true;

    // Format compactly without trailing zeros
    snprintf(_inputBuf, sizeof(_inputBuf), "%.4f", res);
    char* dot = strchr(_inputBuf, '.');
    if (dot) {
        char* end = _inputBuf + strlen(_inputBuf) - 1;
        while (end > dot && *end == '0') {
            *end-- = '\0';
        }
        if (end == dot) {
            *dot = '\0';
        }
    }
}

void CalculatorApp::executeSoftButton(int btnIdx) {
    switch (btnIdx) {
        case 0: selectOperator('+'); break;
        case 1: selectOperator('-'); break;
        case 2: selectOperator('*'); break;
        case 3: selectOperator('/'); break;
        case 4: appendDecimal();     break;
        case 5: evaluate();          break;
        case 6: clearAll();          break;
        default: break;
    }
}

bool CalculatorApp::handleInput(const InputEvent& ev) {
    if (ev.type == EVT_LONG_PRESS) {
        if (ev.rawKey == '*' || ev.rawKey == 'C' || ev.action == ACT_BACK) {
            return true; // Exit Calculator
        }
    }

    if (ev.type != EVT_PRESS && ev.type != EVT_REPEAT) {
        return false;
    }

    // Numeric keys 0-9 work directly in both Keypad and IR Mode
    if (ev.digit >= 0 && ev.digit <= 9) {
        appendDigit((char)('0' + ev.digit));
        return false;
    }

    if (ev.sourceMode == MODE_KEYPAD) {
        switch (ev.rawKey) {
            case 'A': selectOperator('+'); break;
            case 'B': selectOperator('-'); break;
            case 'C':
                if (_pendingOp == '*') selectOperator('/');
                else selectOperator('*');
                break;
            case 'D': appendDecimal(); break;
            case '#': evaluate(); break;
            case '*':
                if (!_newEntry && strcmp(_inputBuf, "0") != 0) {
                    backspace();
                } else if (_pendingOp != '\0' || _accumulator != 0.0 || _hasError) {
                    clearAll();
                } else {
                    return true; // Already clear -> exit to menu
                }
                break;
            default:
                break;
        }
    } else {
        // IR Mode navigation for soft buttons + direct shortcuts
        if (ev.action == ACT_LEFT) {
            _softBtnIdx = (_softBtnIdx + 7) % 8;
        } else if (ev.action == ACT_RIGHT) {
            _softBtnIdx = (_softBtnIdx + 1) % 8;
        } else if (ev.action == ACT_UP) {
            static const char OPS[] = { '+', '-', '*', '/' };
            int nextOp = 0;
            for (int i = 0; i < 4; i++) {
                if (_pendingOp == OPS[i]) { nextOp = (i + 1) % 4; break; }
            }
            selectOperator(OPS[nextOp]);
        } else if (ev.action == ACT_DOWN) {
            appendDecimal();
        } else if (ev.action == ACT_SELECT) {
            if (_softBtnIdx == 7) return true;
            executeSoftButton(_softBtnIdx);
        } else if (ev.rawKey == '*') {
            appendDecimal();
        } else if (ev.rawKey == '#') {
            evaluate();
        } else if (ev.action == ACT_BACK) {
            if (!_newEntry && strcmp(_inputBuf, "0") != 0) {
                backspace();
            } else if (_pendingOp != '\0' || _accumulator != 0.0 || _hasError) {
                clearAll();
            } else {
                return true;
            }
        }
    }

    return false;
}

void CalculatorApp::render(DisplayManager& display, InputMode mode, bool showModeBadge) {
    auto& oled = display.oled();
    display.clear();
    display.drawHeader("CALCULATOR", mode, showModeBadge);

    // Display box
    oled.drawRect(2, 13, OLED_WIDTH - 4, 26, SSD1306_WHITE);

    // Top line inside box: accumulator and pending operator
    oled.setTextSize(1);
    oled.setCursor(6, 15);
    if (_pendingOp != '\0') {
        oled.printf("%.2f %c", _accumulator, _pendingOp);
    } else {
        oled.print(F("READY"));
    }

    // Main value line
    const char* dispStr = _hasError ? _errorMsg : _inputBuf;
    int len = strlen(dispStr);
    int textX = OLED_WIDTH - 6 - (len * 6);
    if (textX < 6) textX = 6;
    oled.setCursor(textX, 27);
    oled.print(dispStr);

    // Soft operator strip (especially helpful for IR remote and visual reference)
    const int stripY = 41;
    const int btnW   = 15;
    for (int i = 0; i < 8; i++) {
        int bx = 4 + (i * btnW);
        bool sel = (mode == MODE_IR && i == _softBtnIdx);
        if (sel) {
            oled.fillRect(bx, stripY, btnW - 1, 11, SSD1306_WHITE);
            oled.setTextColor(SSD1306_BLACK);
        } else {
            oled.drawRect(bx, stripY, btnW - 1, 11, SSD1306_WHITE);
            oled.setTextColor(SSD1306_WHITE);
        }
        oled.setCursor(bx + 4, stripY + 2);
        oled.print(SOFT_BTNS[i]);
    }
    oled.setTextColor(SSD1306_WHITE);

    if (mode == MODE_KEYPAD) {
        display.drawFooter("A+ B- C*/ D.", "*:Clr #:=");
    } else {
        display.drawFooter("</> :Btn CH+:OK", "CH-:Clr/Exit");
    }
}
