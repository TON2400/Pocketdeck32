#ifndef POCKETDECK_CALCULATOR_H
#define POCKETDECK_CALCULATOR_H

#include "AppBase.h"

class CalculatorApp : public AppBase {
public:
    CalculatorApp();

    void onEnter() override;
    bool handleInput(const InputEvent& ev) override;
    void render(DisplayManager& display, InputMode mode, bool showModeBadge) override;

private:
    void clearAll();
    void appendDigit(char d);
    void appendDecimal();
    void backspace();
    void selectOperator(char op);
    void evaluate();
    void executeSoftButton(int btnIdx);

    char   _inputBuf[18];
    double _accumulator;
    char   _pendingOp;
    bool   _newEntry;
    bool   _hasError;
    char   _errorMsg[18];
    int    _softBtnIdx; // 0:+, 1:-, 2:*, 3:/, 4:., 5:=, 6:C, 7:EXIT
};

#endif // POCKETDECK_CALCULATOR_H
