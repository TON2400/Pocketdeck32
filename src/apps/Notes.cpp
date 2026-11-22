#include "Notes.h"
#include "../core/SystemManager.h"
#include <string.h>
#include <stdio.h>

static const int NOTES_VISIBLE_ROWS = 4;

NotesApp::NotesApp()
    : _state(STATE_LIST),
      _fileCount(0),
      _selectedIndex(0),
      _scrollOffset(0),
      _viewLineOffset(0) {
    _activeName[0] = '\0';
    _noteBuffer[0] = '\0';
}

void NotesApp::onEnter() {
    _state          = STATE_LIST;
    _selectedIndex  = 0;
    _scrollOffset   = 0;
    _viewLineOffset = 0;
    refreshList();
}

void NotesApp::refreshList() {
    _fileCount = g_storage.listDirectory("/notes", _files, MAX_NOTES);
    int totalItems = _fileCount + 1; // +1 for "[+ New Note]"
    if (_selectedIndex >= totalItems) {
        _selectedIndex = totalItems - 1;
    }
    if (_selectedIndex < 0) {
        _selectedIndex = 0;
    }
}

void NotesApp::buildNotePath(const char* name, char* outPath, size_t maxLen) const {
    snprintf(outPath, maxLen, "/notes/%s", name);
}

void NotesApp::startNewNote() {
    // Find next unused noteN.txt filename
    for (int n = 1; n <= 99; n++) {
        char candidate[24];
        char fullPath[40];
        snprintf(candidate, sizeof(candidate), "note%d.txt", n);
        buildNotePath(candidate, fullPath, sizeof(fullPath));
        if (!g_storage.exists(fullPath)) {
            strncpy(_activeName, candidate, sizeof(_activeName) - 1);
            _activeName[sizeof(_activeName) - 1] = '\0';
            break;
        }
    }
    _noteBuffer[0] = '\0';
    _textInput.begin(_noteBuffer, sizeof(_noteBuffer), "");
    _state = STATE_EDIT;
}

void NotesApp::openSelectedNote() {
    if (_selectedIndex == 0) {
        startNewNote();
        return;
    }

    int fileIdx = _selectedIndex - 1;
    if (fileIdx < 0 || fileIdx >= _fileCount) return;

    strncpy(_activeName, _files[fileIdx].name, sizeof(_activeName) - 1);
    _activeName[sizeof(_activeName) - 1] = '\0';

    char fullPath[48];
    buildNotePath(_activeName, fullPath, sizeof(fullPath));
    g_storage.readFile(fullPath, _noteBuffer, sizeof(_noteBuffer));
    _viewLineOffset = 0;
    _state          = STATE_VIEW;
}

void NotesApp::saveCurrentNote() {
    char fullPath[48];
    buildNotePath(_activeName, fullPath, sizeof(fullPath));
    if (g_storage.writeFile(fullPath, _noteBuffer)) {
        g_system.display().showToast("NOTE SAVED", 800);
    } else {
        g_system.display().showToast("SAVE ERROR", 900);
    }
    refreshList();
    _state = STATE_VIEW;
}

void NotesApp::deleteCurrentNote() {
    char fullPath[48];
    buildNotePath(_activeName, fullPath, sizeof(fullPath));
    if (g_storage.deleteFile(fullPath)) {
        g_system.display().showToast("DELETED", 800);
    }
    refreshList();
    _state = STATE_LIST;
}

bool NotesApp::update() {
    if (_state == STATE_EDIT) {
        return _textInput.update();
    }
    return false;
}

bool NotesApp::handleInput(const InputEvent& ev) {
    if (_state == STATE_EDIT) {
        TextInputResult res = _textInput.handleInput(ev);
        if (res == TEXT_RES_SUBMITTED) {
            saveCurrentNote();
        } else if (res == TEXT_RES_CANCELLED) {
            _state = STATE_LIST;
        }
        return false;
    }

    if (ev.type != EVT_PRESS && ev.type != EVT_REPEAT) {
        return false;
    }

    if (_state == STATE_CONFIRM_DEL) {
        if (ev.action == ACT_SELECT && ev.type == EVT_PRESS) {
            deleteCurrentNote();
        } else if (ev.action == ACT_BACK && ev.type == EVT_PRESS) {
            _state = STATE_VIEW;
        }
        return false;
    }

    if (_state == STATE_VIEW) {
        if (ev.action == ACT_BACK && ev.type == EVT_PRESS) {
            _state = STATE_LIST;
        } else if (ev.action == ACT_SELECT && ev.type == EVT_PRESS) {
            // Enter Edit Mode with existing content
            _textInput.begin(_noteBuffer, sizeof(_noteBuffer), _noteBuffer);
            _state = STATE_EDIT;
        } else if ((ev.action == ACT_MENU || ev.rawKey == 'D') && ev.type == EVT_PRESS) {
            _state = STATE_CONFIRM_DEL;
        } else if (ev.action == ACT_UP) {
            if (_viewLineOffset > 0) _viewLineOffset--;
        } else if (ev.action == ACT_DOWN) {
            _viewLineOffset++;
        }
        return false;
    }

    // STATE_LIST
    if (ev.action == ACT_BACK && ev.type == EVT_PRESS) {
        return true; // Exit to Main Menu
    }

    int totalItems = _fileCount + 1;
    if (ev.action == ACT_UP) {
        _selectedIndex = (_selectedIndex - 1 + totalItems) % totalItems;
    } else if (ev.action == ACT_DOWN) {
        _selectedIndex = (_selectedIndex + 1) % totalItems;
    } else if (ev.action == ACT_SELECT && ev.type == EVT_PRESS) {
        openSelectedNote();
    } else if ((ev.action == ACT_MENU || ev.rawKey == 'D') && ev.type == EVT_PRESS && _selectedIndex > 0) {
        int fileIdx = _selectedIndex - 1;
        strncpy(_activeName, _files[fileIdx].name, sizeof(_activeName) - 1);
        _activeName[sizeof(_activeName) - 1] = '\0';
        _state = STATE_CONFIRM_DEL;
    }

    if (_selectedIndex < _scrollOffset) {
        _scrollOffset = _selectedIndex;
    } else if (_selectedIndex >= _scrollOffset + NOTES_VISIBLE_ROWS) {
        _scrollOffset = _selectedIndex - NOTES_VISIBLE_ROWS + 1;
    }

    return false;
}

void NotesApp::render(DisplayManager& display, InputMode mode, bool showModeBadge) {
    auto& oled = display.oled();
    display.clear();

    if (_state == STATE_EDIT) {
        char hdr[24];
        snprintf(hdr, sizeof(hdr), "EDIT [%s]", _textInput.getCaseModeLabel());
        display.drawHeader(hdr, mode, showModeBadge);
        _textInput.drawInputBox(oled, 2, 13, OLED_WIDTH - 4, 39, true);
        display.drawFooter("*:Bksp A:Case", "#:Save C:Exit");
        return;
    }

    if (_state == STATE_CONFIRM_DEL) {
        display.drawHeader("DELETE NOTE?", mode, showModeBadge);
        oled.setCursor(8, 22);
        oled.print(_activeName);
        oled.setCursor(8, 36);
        oled.print(F("Confirm deletion?"));
        display.drawFooter("*:Cancel", "5/#:Delete");
        return;
    }

    if (_state == STATE_VIEW) {
        display.drawHeader(_activeName, mode, showModeBadge);

        const int charsPerLine = 20;
        const int maxLines     = 4;
        size_t len = strlen(_noteBuffer);
        int totalLines = ((int)len / charsPerLine) + 1;
        if (_viewLineOffset > totalLines - 1) {
            _viewLineOffset = (totalLines > 0) ? (totalLines - 1) : 0;
        }

        for (int line = 0; line < maxLines; line++) {
            int srcLine = _viewLineOffset + line;
            size_t startChar = (size_t)(srcLine * charsPerLine);
            if (startChar >= len) break;

            oled.setCursor(2, 14 + (line * 10));
            for (int c = 0; c < charsPerLine && (startChar + c) < len; c++) {
                char ch = _noteBuffer[startChar + c];
                oled.write(ch == '\n' ? ' ' : ch);
            }
        }

        display.drawFooter("*:Back D:Del", "5/#:Edit");
        return;
    }

    // STATE_LIST
    display.drawHeader("NOTES", mode, showModeBadge);

    int totalItems = _fileCount + 1;
    const int startY = 13;
    const int rowH   = 10;

    for (int i = 0; i < NOTES_VISIBLE_ROWS; i++) {
        int idx = _scrollOffset + i;
        if (idx >= totalItems) break;

        int y = startY + (i * rowH);
        if (idx == _selectedIndex) {
            oled.fillRect(0, y, OLED_WIDTH - 5, rowH, SSD1306_WHITE);
            oled.setTextColor(SSD1306_BLACK);
        } else {
            oled.setTextColor(SSD1306_WHITE);
        }

        oled.setCursor(3, y + 1);
        if (idx == 0) {
            oled.print(F("[+ New Note]"));
        } else {
            oled.print(_files[idx - 1].name);
        }
    }

    oled.setTextColor(SSD1306_WHITE);
    display.drawScrollbar(startY, NOTES_VISIBLE_ROWS * rowH, totalItems, NOTES_VISIBLE_ROWS, _scrollOffset);
    display.drawFooter("*:Back D:Del", "5/#:Open");
}
