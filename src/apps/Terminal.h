#ifndef POCKETDECK_TERMINAL_H
#define POCKETDECK_TERMINAL_H

#include "AppBase.h"
#include "../core/TextInputManager.h"

class TerminalApp : public AppBase {
public:
    TerminalApp();

    void onEnter() override;
    bool handleInput(const InputEvent& ev) override;
    bool update() override;
    void render(DisplayManager& display, InputMode mode, bool showModeBadge) override;

    void appendLine(const char* text);
    void clearOutput();

private:
    void executeCommand(const char* rawCmd);

    static const int MAX_LOG_LINES = 16;
    static const int LINE_WIDTH    = 21;
    static const int CMD_BUF_SIZE  = 24;

    char             _log[MAX_LOG_LINES][LINE_WIDTH + 1];
    int              _logCount;
    int              _scrollUpOffset; // 0 = bottom (latest lines)
    char             _cmdBuf[CMD_BUF_SIZE];
    TextInputManager _textInput;
    bool             _initializedOnce;
};

#endif // POCKETDECK_TERMINAL_H
