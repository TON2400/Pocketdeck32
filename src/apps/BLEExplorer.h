#ifndef POCKETDECK_BLE_EXPLORER_H
#define POCKETDECK_BLE_EXPLORER_H

#include "AppBase.h"

struct BLEDevEntry {
    char    name[22];
    char    address[18];
    int32_t rssi;
    bool    hasName;
};

class BLEExplorerApp : public AppBase {
public:
    BLEExplorerApp();

    void onEnter() override;
    void onExit() override;
    bool handleInput(const InputEvent& ev) override;
    bool update() override;
    void render(DisplayManager& display, InputMode mode, bool showModeBadge) override;

    void addDiscoveredDevice(const char* name, const char* address, int32_t rssi);
    int  getLastDeviceCount() const { return _count; }

private:
    void triggerOnDemandScan();

    static const int MAX_BLE_DEVICES = 16;

    BLEDevEntry _devs[MAX_BLE_DEVICES];
    int         _count;
    int         _selectedIndex;
    int         _scrollOffset;
    bool        _initialized;
    bool        _pendingScan;
    bool        _showDetails;
};

#endif // POCKETDECK_BLE_EXPLORER_H
