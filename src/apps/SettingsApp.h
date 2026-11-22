#ifndef POCKETDECK_SETTINGS_APP_H
#define POCKETDECK_SETTINGS_APP_H

#include "AppBase.h"

class SettingsApp : public AppBase {
public:
    SettingsApp();

    void onEnter() override;
    bool handleInput(const InputEvent& ev) override;
    void render(DisplayManager& display, InputMode mode, bool showModeBadge) override;

private:
    void adjustSelected(int delta);

    int  _selectedIndex;
    int  _scrollOffset;
    bool _showingInfo;
};

#endif // POCKETDECK_SETTINGS_APP_H
