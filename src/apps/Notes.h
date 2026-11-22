#ifndef POCKETDECK_NOTES_H
#define POCKETDECK_NOTES_H

#include "AppBase.h"
#include "../core/StorageManager.h"
#include "../core/TextInputManager.h"

class NotesApp : public AppBase {
public:
    NotesApp();

    void onEnter() override;
    bool handleInput(const InputEvent& ev) override;
    bool update() override;
    void render(DisplayManager& display, InputMode mode, bool showModeBadge) override;

    int getNoteCount() const { return _fileCount; }

private:
    enum NotesState : uint8_t {
        STATE_LIST = 0,
        STATE_VIEW,
        STATE_EDIT,
        STATE_CONFIRM_DEL
    };

    void refreshList();
    void buildNotePath(const char* name, char* outPath, size_t maxLen) const;
    void startNewNote();
    void openSelectedNote();
    void saveCurrentNote();
    void deleteCurrentNote();

    static const int MAX_NOTES     = 16;
    static const int MAX_NOTE_SIZE = 160;

    NotesState       _state;
    FileEntry        _files[MAX_NOTES];
    int              _fileCount;
    int              _selectedIndex; // 0 = [+ New Note], 1.._fileCount = existing notes
    int              _scrollOffset;
    int              _viewLineOffset;

    char             _activeName[32];
    char             _noteBuffer[MAX_NOTE_SIZE];
    TextInputManager _textInput;
};

#endif // POCKETDECK_NOTES_H
