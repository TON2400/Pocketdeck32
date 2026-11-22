#include "Terminal.h"
#include "SystemMonitor.h"
#include "../core/SystemManager.h"
#include "../core/StorageManager.h"
#include "../utils/Helpers.h"
#include <string.h>
#include <stdio.h>
#include <ctype.h>

static const int TERM_VISIBLE_LOG_LINES = 3;

TerminalApp::TerminalApp()
    : _logCount(0),
      _scrollUpOffset(0),
      _initializedOnce(false) {
    _cmdBuf[0] = '\0';
    for (int i = 0; i < MAX_LOG_LINES; i++) {
        _log[i][0] = '\0';
    }
}

void TerminalApp::onEnter() {
    if (!_initializedOnce) {
        clearOutput();
        appendLine("PocketDeck Shell v1");
        appendLine("Type 'help' for cmds");
        _initializedOnce = true;
    }
    _cmdBuf[0] = '\0';
    _textInput.begin(_cmdBuf, sizeof(_cmdBuf), "");
    _scrollUpOffset = 0;
}

void TerminalApp::clearOutput() {
    _logCount       = 0;
    _scrollUpOffset = 0;
    for (int i = 0; i < MAX_LOG_LINES; i++) {
        _log[i][0] = '\0';
    }
}

void TerminalApp::appendLine(const char* text) {
    if (!text) return;

    // Wrap long lines into chunks of LINE_WIDTH characters
    size_t len = strlen(text);
    size_t pos = 0;

    do {
        if (_logCount >= MAX_LOG_LINES) {
            // Shift log up by 1 line
            for (int i = 1; i < MAX_LOG_LINES; i++) {
                strcpy(_log[i - 1], _log[i]);
            }
            _logCount = MAX_LOG_LINES - 1;
        }

        size_t chunk = len - pos;
        if (chunk > LINE_WIDTH) chunk = LINE_WIDTH;
        memcpy(_log[_logCount], text + pos, chunk);
        _log[_logCount][chunk] = '\0';
        _logCount++;
        pos += chunk;
    } while (pos < len);

    _scrollUpOffset = 0;
}

void TerminalApp::executeCommand(const char* rawCmd) {
    // Trim leading/trailing spaces and lowercase copy
    char cmd[CMD_BUF_SIZE];
    int j = 0;
    for (int i = 0; rawCmd[i] != '\0' && j < CMD_BUF_SIZE - 1; i++) {
        if (j == 0 && isspace((unsigned char)rawCmd[i])) continue;
        cmd[j++] = (char)tolower((unsigned char)rawCmd[i]);
    }
    while (j > 0 && isspace((unsigned char)cmd[j - 1])) {
        j--;
    }
    cmd[j] = '\0';

    if (cmd[0] == '\0') return;

    char echoLine[28];
    snprintf(echoLine, sizeof(echoLine), "> %s", cmd);
    appendLine(echoLine);

    if (strcmp(cmd, "help") == 0) {
        appendLine("help clear info");
        appendLine("uptime version wifi");
        appendLine("ble notes games");
        appendLine("ls reboot exit");
    } else if (strcmp(cmd, "clear") == 0) {
        clearOutput();
    } else if (strcmp(cmd, "info") == 0) {
        char buf[24];
        snprintf(buf, sizeof(buf), "Chip:%s %luMHz",
                 ESP.getChipModel(), (unsigned long)ESP.getCpuFreqMHz());
        appendLine(buf);
        snprintf(buf, sizeof(buf), "Heap:%luKB FS:%luKB",
                 (unsigned long)(ESP.getFreeHeap() / 1024UL),
                 (unsigned long)(g_storage.usedBytes() / 1024UL));
        appendLine(buf);
    } else if (strcmp(cmd, "uptime") == 0) {
        char up[16];
        char buf[24];
        Helpers::formatUptime(millis(), up, sizeof(up));
        snprintf(buf, sizeof(buf), "Up: %s", up);
        appendLine(buf);
    } else if (strcmp(cmd, "version") == 0) {
        appendLine("PocketDeck v" POCKETDECK_VERSION);
        char buf[24];
        snprintf(buf, sizeof(buf), "IDF: %.14s", ESP.getSdkVersion());
        appendLine(buf);
    } else if (strcmp(cmd, "wifi") == 0) {
        char buf[24];
        snprintf(buf, sizeof(buf), "WiFi APs:%d (open)", SystemMonitorApp::lastWifiCount());
        appendLine(buf);
        g_system.launchApp(APP_WIFI_SCANNER);
    } else if (strcmp(cmd, "ble") == 0) {
        char buf[24];
        snprintf(buf, sizeof(buf), "BLE Devs:%d (open)", SystemMonitorApp::lastBleCount());
        appendLine(buf);
        g_system.launchApp(APP_BLE_EXPLORER);
    } else if (strcmp(cmd, "notes") == 0) {
        FileEntry entries[8];
        int cnt = g_storage.listDirectory("/notes", entries, 8);
        char buf[24];
        snprintf(buf, sizeof(buf), "Notes: %d file(s)", cnt);
        appendLine(buf);
        for (int i = 0; i < cnt; i++) {
            appendLine(entries[i].name);
        }
    } else if (strcmp(cmd, "ls") == 0) {
        FileEntry entries[8];
        int cnt = g_storage.listDirectory("/", entries, 8);
        for (int i = 0; i < cnt; i++) {
            appendLine(entries[i].name);
        }
    } else if (strcmp(cmd, "games") == 0) {
        appendLine("Snake, Pong, React");
        g_system.launchApp(APP_GAME_CENTER);
    } else if (strcmp(cmd, "reboot") == 0) {
        appendLine("Rebooting...");
        g_system.display().present();
        delay(200);
        ESP.restart();
    } else if (strcmp(cmd, "exit") == 0) {
        g_system.returnToMenu();
    } else {
        char err[28];
        snprintf(err, sizeof(err), "Unknown: %.11s", cmd);
        appendLine(err);
    }
}

bool TerminalApp::update() {
    return _textInput.update();
}

bool TerminalApp::handleInput(const InputEvent& ev) {
    // Allow scrolling output log with 'B' on Keypad or VOL+/VOL- on IR Remote
    if (ev.type == EVT_PRESS || ev.type == EVT_REPEAT) {
        if (ev.rawKey == 'B' || (ev.sourceMode == MODE_IR && ev.action == ACT_UP)) {
            int maxScroll = _logCount - TERM_VISIBLE_LOG_LINES;
            if (maxScroll < 0) maxScroll = 0;
            if (_scrollUpOffset < maxScroll) {
                _scrollUpOffset++;
            } else {
                _scrollUpOffset = 0;
            }
            return false;
        }
        if (ev.sourceMode == MODE_IR && ev.action == ACT_DOWN) {
            if (_scrollUpOffset > 0) _scrollUpOffset--;
            return false;
        }
    }

    TextInputResult res = _textInput.handleInput(ev);
    if (res == TEXT_RES_SUBMITTED) {
        char submitted[CMD_BUF_SIZE];
        strncpy(submitted, _cmdBuf, sizeof(submitted) - 1);
        submitted[sizeof(submitted) - 1] = '\0';
        _textInput.clearText();
        executeCommand(submitted);
        return false;
    } else if (res == TEXT_RES_CANCELLED) {
        return true; // Return to Main Menu
    }

    return false;
}

void TerminalApp::render(DisplayManager& display, InputMode mode, bool showModeBadge) {
    auto& oled = display.oled();
    display.clear();

    char hdr[24];
    snprintf(hdr, sizeof(hdr), "TERM [%s]", _textInput.getCaseModeLabel());
    display.drawHeader(hdr, mode, showModeBadge);

    // Output log area (3 lines between y=13 and y=39)
    int firstLine = _logCount - TERM_VISIBLE_LOG_LINES - _scrollUpOffset;
    if (firstLine < 0) firstLine = 0;

    for (int i = 0; i < TERM_VISIBLE_LOG_LINES; i++) {
        int idx = firstLine + i;
        if (idx >= _logCount) break;
        oled.setCursor(2, 13 + (i * 9));
        oled.print(_log[idx]);
    }

    // Input prompt box at bottom (y=41, h=12)
    oled.setCursor(1, 43);
    oled.print('>');
    _textInput.drawInputBox(oled, 8, 41, OLED_WIDTH - 10, 12, false);

    display.drawFooter("A:Case B:Scrl", "#:Run C:Exit");
}
