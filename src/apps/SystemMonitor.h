#ifndef POCKETDECK_SYSTEM_MONITOR_H
#define POCKETDECK_SYSTEM_MONITOR_H

#include "AppBase.h"

class SystemMonitorApp : public AppBase {
public:
    SystemMonitorApp();

    void onEnter() override;
    bool handleInput(const InputEvent& ev) override;
    bool update() override;
    void render(DisplayManager& display, InputMode mode, bool showModeBadge) override;

    static void setWirelessStatus(bool wifiActive, int wifiCount, bool bleActive, int bleCount);
    static bool isWifiReady() { return _wifiActive; }
    static bool isBleReady()  { return _bleActive; }
    static int  lastWifiCount() { return _lastWifiCount; }
    static int  lastBleCount()  { return _lastBleCount; }

private:
    int      _page;
    uint32_t _lastRefreshMs;

    static bool _wifiActive;
    static int  _lastWifiCount;
    static bool _bleActive;
    static int  _lastBleCount;
};

#endif // POCKETDECK_SYSTEM_MONITOR_H
