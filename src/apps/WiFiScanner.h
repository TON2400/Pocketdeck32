#ifndef POCKETDECK_WIFI_SCANNER_H
#define POCKETDECK_WIFI_SCANNER_H

#include "AppBase.h"
#include "../core/TextInputManager.h"

struct WiFiNetEntry {
    char    ssid[33];
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
    enum WiFiViewState : uint8_t {
        WVIEW_LIST = 0,
        WVIEW_DETAILS,
        WVIEW_PASSWORD,
        WVIEW_CONNECTING
    };

    void startAsyncScan();
    void collectScanResults(int found);
    void beginConnectSelected(bool forcePasswordPrompt = false);
    const char* getEncryptionLabel(uint8_t encType) const;

    static const int MAX_WIFI_RESULTS = 16;
    static const int PASS_BUF_SIZE    = 64;

    WiFiNetEntry     _nets[MAX_WIFI_RESULTS];
    int              _count;
    int              _selectedIndex;
    int              _scrollOffset;
    bool             _scanning;
    WiFiViewState    _view;
    uint32_t         _scanStartMs;

    char             _passBuf[PASS_BUF_SIZE];
    TextInputManager _textInput;
};

#endif // POCKETDECK_WIFI_SCANNER_H
