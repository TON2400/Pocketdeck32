#include "FileManager.h"
#include "../core/SystemManager.h"
#include "../utils/Helpers.h"
#include <string.h>
#include <stdio.h>

static const int FM_VISIBLE_ROWS = 4;

FileManagerApp::FileManagerApp()
    : _state(FM_LIST),
      _entryCount(0),
      _selectedIndex(0),
      _scrollOffset(0),
      _viewLineOffset(0),
      _activeFileSize(0),
      _activeIsDir(false) {
    strcpy(_currentDir, "/");
    _activeFileName[0] = '\0';
    _fileBuf[0]        = '\0';
}

void FileManagerApp::onEnter() {
    strcpy(_currentDir, "/");
    _state          = FM_LIST;
    _selectedIndex  = 0;
    _scrollOffset   = 0;
    _viewLineOffset = 0;
    refreshDir();
}

void FileManagerApp::refreshDir() {
    _entryCount = 0;

    // First synthetic entry in subdirectory: "[..] Up Dir"
    if (strcmp(_currentDir, "/") != 0 && _entryCount < MAX_FM_FILES) {
        strcpy(_entries[_entryCount].name, "..");
        _entries[_entryCount].size  = 0;
        _entries[_entryCount].isDir = true;
        _entryCount++;
    }

    // Synthetic entry to create a new text file in current folder
    if (_entryCount < MAX_FM_FILES) {
        strcpy(_entries[_entryCount].name, "+New File");
        _entries[_entryCount].size  = 0;
        _entries[_entryCount].isDir = false;
        _entryCount++;
    }

    int fetched = g_storage.listDirectory(_currentDir, &_entries[_entryCount], MAX_FM_FILES - _entryCount);
    _entryCount += fetched;

    if (_selectedIndex >= _entryCount) {
        _selectedIndex = (_entryCount > 0) ? (_entryCount - 1) : 0;
    }
}

void FileManagerApp::buildEntryPath(const char* name, char* outPath, size_t maxLen) const {
    if (strcmp(_currentDir, "/") == 0) {
        snprintf(outPath, maxLen, "/%s", name);
    } else {
        snprintf(outPath, maxLen, "%s/%s", _currentDir, name);
    }
}

void FileManagerApp::startCreateFile() {
    for (int n = 1; n <= 99; n++) {
        char candidate[24];
        char fullPath[48];
        snprintf(candidate, sizeof(candidate), "file%d.txt", n);
        buildEntryPath(candidate, fullPath, sizeof(fullPath));
        if (!g_storage.exists(fullPath)) {
            strncpy(_activeFileName, candidate, sizeof(_activeFileName) - 1);
            _activeFileName[sizeof(_activeFileName) - 1] = '\0';
            break;
        }
    }
    _fileBuf[0] = '\0';
    _textInput.begin(_fileBuf, sizeof(_fileBuf), "");
    _state = FM_NEW_CONTENT;
}

void FileManagerApp::saveCreatedFile() {
    char fullPath[48];
    buildEntryPath(_activeFileName, fullPath, sizeof(fullPath));
    if (g_storage.writeFile(fullPath, _fileBuf)) {
        g_system.display().showToast("FILE CREATED", 800);
    } else {
        g_system.display().showToast("WRITE ERR", 900);
    }
    refreshDir();
    _state = FM_LIST;
}

void FileManagerApp::openSelectedEntry() {
    if (_selectedIndex < 0 || _selectedIndex >= _entryCount) return;

    const auto& item = _entries[_selectedIndex];
    if (strcmp(item.name, "..") == 0) {
        strcpy(_currentDir, "/");
        _selectedIndex = 0;
        _scrollOffset  = 0;
        refreshDir();
        return;
    }

    if (strcmp(item.name, "+New File") == 0) {
        startCreateFile();
        return;
    }

    if (item.isDir) {
        buildEntryPath(item.name, _currentDir, sizeof(_currentDir));
        _selectedIndex = 0;
        _scrollOffset  = 0;
        refreshDir();
        return;
    }

    // Open text file content
    strncpy(_activeFileName, item.name, sizeof(_activeFileName) - 1);
    _activeFileName[sizeof(_activeFileName) - 1] = '\0';
    _activeFileSize = item.size;
    _activeIsDir    = false;

    char fullPath[48];
    buildEntryPath(_activeFileName, fullPath, sizeof(fullPath));
    g_storage.readFile(fullPath, _fileBuf, sizeof(_fileBuf));
    _viewLineOffset = 0;
    _state          = FM_VIEW_FILE;
}

void FileManagerApp::showSelectedInfo() {
    if (_selectedIndex < 0 || _selectedIndex >= _entryCount) return;
    const auto& item = _entries[_selectedIndex];
    if (strcmp(item.name, "..") == 0 || strcmp(item.name, "+New File") == 0) return;

    strncpy(_activeFileName, item.name, sizeof(_activeFileName) - 1);
    _activeFileName[sizeof(_activeFileName) - 1] = '\0';
    _activeFileSize = item.size;
    _activeIsDir    = item.isDir;
    _state          = FM_FILE_INFO;
}

void FileManagerApp::deleteSelectedFile() {
    char fullPath[48];
    buildEntryPath(_activeFileName, fullPath, sizeof(fullPath));
    if (g_storage.deleteFile(fullPath)) {
        g_system.display().showToast("DELETED", 800);
    } else {
        g_system.display().showToast("DEL FAILED", 900);
    }
    refreshDir();
    _state = FM_LIST;
}

bool FileManagerApp::update() {
    if (_state == FM_NEW_CONTENT) {
        return _textInput.update();
    }
    return false;
}

bool FileManagerApp::handleInput(const InputEvent& ev) {
    if (_state == FM_NEW_CONTENT) {
        TextInputResult res = _textInput.handleInput(ev);
        if (res == TEXT_RES_SUBMITTED) {
            saveCreatedFile();
        } else if (res == TEXT_RES_CANCELLED) {
            _state = FM_LIST;
        }
        return false;
    }

    if (ev.type != EVT_PRESS && ev.type != EVT_REPEAT) {
        return false;
    }

    if (_state == FM_CONFIRM_DEL) {
        if (ev.action == ACT_SELECT && ev.type == EVT_PRESS) {
            deleteSelectedFile();
        } else if (ev.action == ACT_BACK && ev.type == EVT_PRESS) {
            _state = FM_FILE_INFO;
        }
        return false;
    }

    if (_state == FM_FILE_INFO) {
        if (ev.action == ACT_BACK && ev.type == EVT_PRESS) {
            _state = FM_LIST;
        } else if ((ev.action == ACT_MENU || ev.rawKey == 'D' || ev.action == ACT_SELECT) &&
                   ev.type == EVT_PRESS && !_activeIsDir) {
            _state = FM_CONFIRM_DEL;
        }
        return false;
    }

    if (_state == FM_VIEW_FILE) {
        if (ev.action == ACT_BACK && ev.type == EVT_PRESS) {
            _state = FM_LIST;
        } else if ((ev.action == ACT_MENU || ev.rawKey == 'D') && ev.type == EVT_PRESS) {
            _state = FM_FILE_INFO;
        } else if (ev.action == ACT_UP) {
            if (_viewLineOffset > 0) _viewLineOffset--;
        } else if (ev.action == ACT_DOWN) {
            _viewLineOffset++;
        }
        return false;
    }

    // FM_LIST
    if (ev.action == ACT_BACK && ev.type == EVT_PRESS) {
        if (strcmp(_currentDir, "/") != 0) {
            strcpy(_currentDir, "/");
            _selectedIndex = 0;
            _scrollOffset  = 0;
            refreshDir();
            return false;
        }
        return true; // Return to Main Menu
    }

    if (_entryCount > 0) {
        if (ev.action == ACT_UP) {
            _selectedIndex = (_selectedIndex - 1 + _entryCount) % _entryCount;
        } else if (ev.action == ACT_DOWN) {
            _selectedIndex = (_selectedIndex + 1) % _entryCount;
        } else if (ev.action == ACT_SELECT && ev.type == EVT_PRESS) {
            openSelectedEntry();
        } else if ((ev.action == ACT_MENU || ev.rawKey == 'D') && ev.type == EVT_PRESS) {
            showSelectedInfo();
        }

        if (_selectedIndex < _scrollOffset) {
            _scrollOffset = _selectedIndex;
        } else if (_selectedIndex >= _scrollOffset + FM_VISIBLE_ROWS) {
            _scrollOffset = _selectedIndex - FM_VISIBLE_ROWS + 1;
        }
    }

    return false;
}

void FileManagerApp::render(DisplayManager& display, InputMode mode, bool showModeBadge) {
    auto& oled = display.oled();
    display.clear();

    if (_state == FM_NEW_CONTENT) {
        char hdr[24];
        snprintf(hdr, sizeof(hdr), "NEW [%s]", _textInput.getCaseModeLabel());
        display.drawHeader(hdr, mode, showModeBadge);
        _textInput.drawInputBox(oled, 2, 13, OLED_WIDTH - 4, 39, true);
        display.drawFooter("*:Bksp A:Case", "#:Save C:Exit");
        return;
    }

    if (_state == FM_CONFIRM_DEL) {
        display.drawHeader("DELETE FILE?", mode, showModeBadge);
        oled.setCursor(6, 22);
        oled.print(_activeFileName);
        oled.setCursor(6, 36);
        oled.print(F("Delete permanently?"));
        display.drawFooter("*:No", "5/#:Yes");
        return;
    }

    if (_state == FM_FILE_INFO) {
        display.drawHeader("FILE INFO", mode, showModeBadge);

        char fullPath[48];
        buildEntryPath(_activeFileName, fullPath, sizeof(fullPath));

        oled.setCursor(2, 14);
        oled.printf("Name: %.14s", _activeFileName);

        oled.setCursor(2, 24);
        oled.printf("Path: %.14s", fullPath);

        oled.setCursor(2, 34);
        oled.printf("Type: %s (%luB)", _activeIsDir ? "DIR" : "TXT", (unsigned long)_activeFileSize);

        oled.setCursor(2, 44);
        oled.printf("FS: %lu/%lu KB",
                    (unsigned long)(g_storage.usedBytes() / 1024UL),
                    (unsigned long)(g_storage.totalBytes() / 1024UL));

        display.drawFooter("*:Back", _activeIsDir ? nullptr : "5/D:Delete");
        return;
    }

    if (_state == FM_VIEW_FILE) {
        display.drawHeader(_activeFileName, mode, showModeBadge);

        const int charsPerLine = 20;
        const int maxLines     = 4;
        size_t len = strlen(_fileBuf);
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
                char ch = _fileBuf[startChar + c];
                oled.write(ch == '\n' ? ' ' : ch);
            }
        }

        display.drawFooter("*:Back", "D:Info");
        return;
    }

    // FM_LIST
    char hdr[24];
    snprintf(hdr, sizeof(hdr), "FILES %s", _currentDir);
    display.drawHeader(hdr, mode, showModeBadge);

    const int startY = 13;
    const int rowH   = 10;

    for (int i = 0; i < FM_VISIBLE_ROWS; i++) {
        int idx = _scrollOffset + i;
        if (idx >= _entryCount) break;

        int y = startY + (i * rowH);
        const auto& item = _entries[idx];

        if (idx == _selectedIndex) {
            oled.fillRect(0, y, OLED_WIDTH - 5, rowH, SSD1306_WHITE);
            oled.setTextColor(SSD1306_BLACK);
        } else {
            oled.setTextColor(SSD1306_WHITE);
        }

        oled.setCursor(2, y + 1);
        if (strcmp(item.name, "..") == 0) {
            oled.print(F("[..] Up Folder"));
        } else if (strcmp(item.name, "+New File") == 0) {
            oled.print(F("[+ New File]"));
        } else if (item.isDir) {
            oled.printf("[%s/]", item.name);
        } else {
            char sizeStr[10];
            Helpers::formatBytes(item.size, sizeStr, sizeof(sizeStr));
            oled.printf("%-13.13s %5s", item.name, sizeStr);
        }
    }

    oled.setTextColor(SSD1306_WHITE);
    display.drawScrollbar(startY, FM_VISIBLE_ROWS * rowH, _entryCount, FM_VISIBLE_ROWS, _scrollOffset);
    display.drawFooter("*:Back D:Info", "5/#:Open");
}
