#include "BLEExplorer.h"
#include "SystemMonitor.h"
#include "../core/SystemManager.h"
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
#include <string.h>
#include <stdio.h>

static const int BLE_VISIBLE_ROWS    = 4;
static const uint32_t BLE_SCAN_SECS  = 2;

class PocketDeckBLECallbacks : public BLEAdvertisedDeviceCallbacks {
public:
    explicit PocketDeckBLECallbacks(BLEExplorerApp* app) : _app(app) {}

    void onResult(BLEAdvertisedDevice advertisedDevice) override {
        if (!_app) return;
        const char* name = advertisedDevice.haveName() ? advertisedDevice.getName().c_str() : "";
        const char* addr = advertisedDevice.getAddress().toString().c_str();
        int32_t     rssi = (int32_t)advertisedDevice.getRSSI();
        _app->addDiscoveredDevice(name, addr, rssi);
    }

private:
    BLEExplorerApp* _app;
};

static PocketDeckBLECallbacks* s_bleCallbacks = nullptr;

BLEExplorerApp::BLEExplorerApp()
    : _count(0),
      _selectedIndex(0),
      _scrollOffset(0),
      _initialized(false),
      _pendingScan(false),
      _showDetails(false) {}

void BLEExplorerApp::onEnter() {
    _showDetails   = false;
    _selectedIndex = 0;
    _scrollOffset  = 0;
    // Schedule an on-demand scan after rendering the "Scanning BLE..." status frame
    _pendingScan   = true;
}

void BLEExplorerApp::onExit() {
    _pendingScan = false;
    if (_initialized) {
        BLEScan* pScan = BLEDevice::getScan();
        if (pScan) {
            pScan->stop();
            pScan->clearResults();
        }
    }
    SystemMonitorApp::setWirelessStatus(SystemMonitorApp::isWifiReady(),
                                        SystemMonitorApp::lastWifiCount(),
                                        false, _count);
}

void BLEExplorerApp::addDiscoveredDevice(const char* name, const char* address, int32_t rssi) {
    if (!address) return;

    // Deduplicate by MAC address and update RSSI/name if already seen
    for (int i = 0; i < _count; i++) {
        if (strcmp(_devs[i].address, address) == 0) {
            _devs[i].rssi = rssi;
            if (name && name[0] != '\0' && !_devs[i].hasName) {
                strncpy(_devs[i].name, name, sizeof(_devs[i].name) - 1);
                _devs[i].name[sizeof(_devs[i].name) - 1] = '\0';
                _devs[i].hasName = true;
            }
            return;
        }
    }

    if (_count >= MAX_BLE_DEVICES) return;

    if (name && name[0] != '\0') {
        strncpy(_devs[_count].name, name, sizeof(_devs[_count].name) - 1);
        _devs[_count].name[sizeof(_devs[_count].name) - 1] = '\0';
        _devs[_count].hasName = true;
    } else {
        strcpy(_devs[_count].name, "<Unnamed>");
        _devs[_count].hasName = false;
    }

    strncpy(_devs[_count].address, address, sizeof(_devs[_count].address) - 1);
    _devs[_count].address[sizeof(_devs[_count].address) - 1] = '\0';
    _devs[_count].rssi = rssi;
    _count++;
}

void BLEExplorerApp::triggerOnDemandScan() {
    _count         = 0;
    _selectedIndex = 0;
    _scrollOffset  = 0;
    _showDetails   = false;

    if (!_initialized) {
        BLEDevice::init("PocketDeck");
        s_bleCallbacks = new PocketDeckBLECallbacks(this);
        BLEScan* pScan = BLEDevice::getScan();
        pScan->setAdvertisedDeviceCallbacks(s_bleCallbacks, false);
        pScan->setActiveScan(true);
        pScan->setInterval(100);
        pScan->setWindow(80);
        _initialized = true;
    }

    SystemMonitorApp::setWirelessStatus(SystemMonitorApp::isWifiReady(),
                                        SystemMonitorApp::lastWifiCount(),
                                        true, _count);

    BLEScan* pScan = BLEDevice::getScan();
    if (pScan) {
        pScan->clearResults();
        pScan->start(BLE_SCAN_SECS, false);
        pScan->clearResults();
    }

    SystemMonitorApp::setWirelessStatus(SystemMonitorApp::isWifiReady(),
                                        SystemMonitorApp::lastWifiCount(),
                                        false, _count);
}

bool BLEExplorerApp::update() {
    if (_pendingScan) {
        // Render the "Scanning BLE..." screen once, then run the 2-second scan burst
        g_system.display().clear();
        g_system.display().drawHeader("BLE SCAN...", g_system.input().getMode(), true);
        g_system.display().oled().setCursor(16, 26);
        g_system.display().oled().print(F("Discovering BLE..."));
        g_system.display().oled().setCursor(28, 38);
        g_system.display().oled().print(F("(2s burst)"));
        g_system.display().present();

        _pendingScan = false;
        triggerOnDemandScan();
        return true;
    }
    return false;
}

bool BLEExplorerApp::handleInput(const InputEvent& ev) {
    if (ev.type != EVT_PRESS && ev.type != EVT_REPEAT) {
        return false;
    }

    if (_showDetails) {
        if (ev.action == ACT_BACK || ev.action == ACT_SELECT) {
            _showDetails = false;
        }
        return false;
    }

    if (ev.action == ACT_BACK && ev.type == EVT_PRESS) {
        return true; // Return to Main Menu
    }

    // On-demand rescan via D / EQ (ACT_MENU) to conserve power
    if ((ev.action == ACT_MENU || ev.rawKey == 'D') && ev.type == EVT_PRESS) {
        _pendingScan = true;
        return false;
    }

    if (_count > 0) {
        if (ev.action == ACT_UP) {
            _selectedIndex = (_selectedIndex - 1 + _count) % _count;
        } else if (ev.action == ACT_DOWN) {
            _selectedIndex = (_selectedIndex + 1) % _count;
        } else if (ev.action == ACT_SELECT && ev.type == EVT_PRESS) {
            _showDetails = true;
        }

        if (_selectedIndex < _scrollOffset) {
            _scrollOffset = _selectedIndex;
        } else if (_selectedIndex >= _scrollOffset + BLE_VISIBLE_ROWS) {
            _scrollOffset = _selectedIndex - BLE_VISIBLE_ROWS + 1;
        }
    } else if (ev.action == ACT_SELECT && ev.type == EVT_PRESS) {
        _pendingScan = true;
    }

    return false;
}

void BLEExplorerApp::render(DisplayManager& display, InputMode mode, bool showModeBadge) {
    auto& oled = display.oled();
    display.clear();

    if (_showDetails && _selectedIndex >= 0 && _selectedIndex < _count) {
        const auto& dev = _devs[_selectedIndex];
        display.drawHeader("BLE DETAIL", mode, showModeBadge);

        oled.setCursor(2, 15);
        oled.printf("Name: %.14s", dev.name);

        oled.setCursor(2, 27);
        oled.printf("MAC: %s", dev.address);

        oled.setCursor(2, 39);
        oled.printf("RSSI: %ld dBm", (long)dev.rssi);

        display.drawFooter("*:Back", "5/#:List");
        return;
    }

    char hdr[22];
    snprintf(hdr, sizeof(hdr), "BLE (%d)", _count);
    display.drawHeader(hdr, mode, showModeBadge);

    if (_count == 0) {
        oled.setCursor(16, 28);
        oled.print(F("No BLE devices"));
        display.drawFooter("*:Exit", "5/D:Rescan");
        return;
    }

    const int startY = 13;
    const int rowH   = 10;

    for (int i = 0; i < BLE_VISIBLE_ROWS; i++) {
        int idx = _scrollOffset + i;
        if (idx >= _count) break;

        int y = startY + (i * rowH);
        const auto& dev = _devs[idx];

        if (idx == _selectedIndex) {
            oled.fillRect(0, y, OLED_WIDTH - 5, rowH, SSD1306_WHITE);
            oled.setTextColor(SSD1306_BLACK);
        } else {
            oled.setTextColor(SSD1306_WHITE);
        }

        // Display Device Name (or short MAC if unnamed) + RSSI
        const char* label = dev.hasName ? dev.name : dev.address;
        char rowBuf[22];
        snprintf(rowBuf, sizeof(rowBuf), "%-14.14s %4ld", label, (long)dev.rssi);
        oled.setCursor(2, y + 1);
        oled.print(rowBuf);
    }

    oled.setTextColor(SSD1306_WHITE);
    display.drawScrollbar(startY, BLE_VISIBLE_ROWS * rowH, _count, BLE_VISIBLE_ROWS, _scrollOffset);
    display.drawFooter("*:Exit D:Scan", "5/#:Info");
}
