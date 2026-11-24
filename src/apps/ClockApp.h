#ifndef POCKETDECK_CLOCK_APP_H
#define POCKETDECK_CLOCK_APP_H

#include "AppBase.h"

class ClockApp : public AppBase {
public:
    ClockApp();

    void onEnter() override;
    bool handleInput(const InputEvent& ev) override;
    bool update() override;
    void render(DisplayManager& display, InputMode mode, bool showModeBadge) override;

private:
    enum ClockView : uint8_t {
        CLK_VIEW_MAIN = 0,
        CLK_VIEW_MENU,
        CLK_VIEW_MANUAL_SET
    };

    void startManualEdit();
    void adjustManualField(int delta);
    void saveManualEdit();
    void adjustTimezone(int stepHalfHours);

    ClockView _view;
    int       _menuIndex;
    int       _menuScroll;
    uint32_t  _lastRenderSec;

    // Manual editor fields: 0=Hour, 1=Min, 2=Sec, 3=Day, 4=Month, 5=Year
    int       _editField;
    uint16_t  _editYear;
    uint8_t   _editMonth;
    uint8_t   _editDay;
    uint8_t   _editHour;
    uint8_t   _editMinute;
    uint8_t   _editSecond;
};

#endif // POCKETDECK_CLOCK_APP_H
