#ifndef POCKETDECK_WIFI_SCANNER_H
#define POCKETDECK_WIFI_SCANNER_H

#include "AppBase.h"

struct WiFiNetEntry {
    char    ssid[25];
    int32_t rssi;
    int32_t channel;
    uint8_t encType;
    bool    isOpen;
};

class WiFiScannerApp : public AppBase {
public:
    WiFiScannerApp();

    void onEnter() override;
    void onExit() override;
    bool handleInput(const InputEvent& ev) override;
    bool update() override;
    void render(DisplayManager& display, InputMode mode, bool showModeBadge) override;

    int getLastNetworkCount() const { return _count; }

private:
    void startAsyncScan();
    void collectScanResults(int found);
    const char* getEncryptionLabel(uint8_t encType) const;

    static const int MAX_WIFI_RESULTS = 16;

    WiFiNetEntry _nets[MAX_WIFI_RESULTS];
    int          _count;
    int          _selectedIndex;
    int          _scrollOffset;
    bool         _scanning;
    bool         _showDetails;
    uint32_t     _scanStartMs;
};

#endif // POCKETDECK_WIFI_SCANNER_H
