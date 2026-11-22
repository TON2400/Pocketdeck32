#ifndef POCKETDECK_FILE_MANAGER_H
#define POCKETDECK_FILE_MANAGER_H

#include "AppBase.h"
#include "../core/StorageManager.h"
#include "../core/TextInputManager.h"

class FileManagerApp : public AppBase {
public:
    FileManagerApp();

    void onEnter() override;
    bool handleInput(const InputEvent& ev) override;
    bool update() override;
    void render(DisplayManager& display, InputMode mode, bool showModeBadge) override;

private:
    enum FMState : uint8_t {
        FM_LIST = 0,
        FM_VIEW_FILE,
        FM_FILE_INFO,
        FM_NEW_CONTENT,
        FM_CONFIRM_DEL
    };

    void refreshDir();
    void buildEntryPath(const char* name, char* outPath, size_t maxLen) const;
    void openSelectedEntry();
    void showSelectedInfo();
    void startCreateFile();
    void saveCreatedFile();
    void deleteSelectedFile();

    static const int MAX_FM_FILES  = 16;
    static const int MAX_VIEW_SIZE = 192;

    FMState          _state;
    char             _currentDir[24];
    FileEntry        _entries[MAX_FM_FILES];
    int              _entryCount;
    int              _selectedIndex;
    int              _scrollOffset;
    int              _viewLineOffset;

    char             _activeFileName[32];
    uint32_t         _activeFileSize;
    bool             _activeIsDir;
    char             _fileBuf[MAX_VIEW_SIZE];
    TextInputManager _textInput;
};

#endif // POCKETDECK_FILE_MANAGER_H
