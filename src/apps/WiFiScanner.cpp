#include "WiFiScanner.h"
#include "SystemMonitor.h"
#include <WiFi.h>
#include <string.h>
#include <stdio.h>

static const int WIFI_VISIBLE_ROWS = 4;

WiFiScannerApp::WiFiScannerApp()
    : _count(0),
      _selectedIndex(0),
      _scrollOffset(0),
      _scanning(false),
      _showDetails(false),
      _scanStartMs(0) {}

void WiFiScannerApp::onEnter() {
    _showDetails   = false;
    _selectedIndex = 0;
    _scrollOffset  = 0;
    startAsyncScan();
}

void WiFiScannerApp::onExit() {
    if (_scanning) {
        WiFi.scanDelete();
        _scanning = false;
    }
    // Put Wi-Fi radio back to idle/off when leaving scanner to save power
    WiFi.mode(WIFI_OFF);
    SystemMonitorApp::setWirelessStatus(false, _count,
                                        SystemMonitorApp::isBleReady(),
                                        SystemMonitorApp::lastBleCount());
}

void WiFiScannerApp::startAsyncScan() {
    if (_scanning) return;

    WiFi.mode(WIFI_STA);
    WiFi.disconnect(true);
    delay(10);

    // Non-blocking async scan
    WiFi.scanDelete();
    int16_t state = WiFi.scanNetworks(true, false);
    _scanning      = (state == WIFI_SCAN_RUNNING || state >= 0);
    _scanStartMs   = millis();
    _showDetails   = false;

    SystemMonitorApp::setWirelessStatus(true, _count,
                                        SystemMonitorApp::isBleReady(),
                                        SystemMonitorApp::lastBleCount());
}

void WiFiScannerApp::collectScanResults(int found) {
    _count = 0;
    for (int i = 0; i < found && _count < MAX_WIFI_RESULTS; i++) {
        String s = WiFi.SSID(i);
        if (s.length() == 0) {
            strcpy(_nets[_count].ssid, "<Hidden>");
        } else {
            strncpy(_nets[_count].ssid, s.c_str(), sizeof(_nets[_count].ssid) - 1);
            _nets[_count].ssid[sizeof(_nets[_count].ssid) - 1] = '\0';
        }
        _nets[_count].rssi    = WiFi.RSSI(i);
        _nets[_count].channel = WiFi.channel(i);
        _nets[_count].encType = (uint8_t)WiFi.encryptionType(i);
        _nets[_count].isOpen  = (_nets[_count].encType == WIFI_AUTH_OPEN);
        _count++;
    }
    WiFi.scanDelete();
    _selectedIndex = 0;
    _scrollOffset  = 0;

    SystemMonitorApp::setWirelessStatus(true, _count,
                                        SystemMonitorApp::isBleReady(),
                                        SystemMonitorApp::lastBleCount());
}

const char* WiFiScannerApp::getEncryptionLabel(uint8_t encType) const {
    switch (encType) {
        case WIFI_AUTH_OPEN:            return "OPEN";
        case WIFI_AUTH_WEP:             return "WEP";
        case WIFI_AUTH_WPA_PSK:         return "WPA-PSK";
        case WIFI_AUTH_WPA2_PSK:        return "WPA2-PSK";
        case WIFI_AUTH_WPA_WPA2_PSK:    return "WPA/WPA2";
        case WIFI_AUTH_WPA3_PSK:        return "WPA3-PSK";
        case WIFI_AUTH_WPA2_WPA3_PSK:   return "WPA2/WPA3";
        default:                        return "SECURED";
    }
}

bool WiFiScannerApp::update() {
    if (!_scanning) return false;

    int16_t res = WiFi.scanComplete();
    if (res == WIFI_SCAN_RUNNING) {
        return false;
    } else if (res >= 0) {
        _scanning = false;
        collectScanResults(res);
        return true;
    } else {
        // Scan failed or timed out
        _scanning = false;
        return true;
    }
}

bool WiFiScannerApp::handleInput(const InputEvent& ev) {
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

    // Rescan on MENU ('D' or IR 'EQ') or SELECT when list is empty
    if ((ev.action == ACT_MENU || ev.rawKey == 'D') && ev.type == EVT_PRESS) {
        startAsyncScan();
        return false;
    }

    if (_count > 0 && !_scanning) {
        if (ev.action == ACT_UP) {
            _selectedIndex = (_selectedIndex - 1 + _count) % _count;
        } else if (ev.action == ACT_DOWN) {
            _selectedIndex = (_selectedIndex + 1) % _count;
        } else if (ev.action == ACT_SELECT && ev.type == EVT_PRESS) {
            _showDetails = true;
        }

        if (_selectedIndex < _scrollOffset) {
            _scrollOffset = _selectedIndex;
        } else if (_selectedIndex >= _scrollOffset + WIFI_VISIBLE_ROWS) {
            _scrollOffset = _selectedIndex - WIFI_VISIBLE_ROWS + 1;
        }
    } else if (ev.action == ACT_SELECT && ev.type == EVT_PRESS && !_scanning) {
        startAsyncScan();
    }

    return false;
}

void WiFiScannerApp::render(DisplayManager& display, InputMode mode, bool showModeBadge) {
    auto& oled = display.oled();
    display.clear();

    if (_scanning) {
        display.drawHeader("WIFI SCAN...", mode, showModeBadge);
        oled.setCursor(16, 26);
        oled.print(F("Scanning 2.4GHz..."));
        oled.setCursor(22, 38);
        oled.print(F("Please wait"));
        display.drawFooter("*:Exit", nullptr);
        return;
    }

    if (_showDetails && _selectedIndex >= 0 && _selectedIndex < _count) {
        const auto& net = _nets[_selectedIndex];
        display.drawHeader("WIFI DETAIL", mode, showModeBadge);

        oled.setCursor(2, 14);
        oled.printf("SSID: %.15s", net.ssid);

        oled.setCursor(2, 24);
        oled.printf("RSSI: %ld dBm", (long)net.rssi);

        oled.setCursor(2, 34);
        oled.printf("Chan: CH %ld", (long)net.channel);

        oled.setCursor(2, 44);
        oled.printf("Auth: %s", getEncryptionLabel(net.encType));

        display.drawFooter("*:Back", "5/#:List");
        return;
    }

    char hdr[22];
    snprintf(hdr, sizeof(hdr), "WIFI (%d)", _count);
    display.drawHeader(hdr, mode, showModeBadge);

    if (_count == 0) {
        oled.setCursor(14, 28);
        oled.print(F("No networks found"));
        display.drawFooter("*:Exit", "5/D:Rescan");
        return;
    }

    const int startY = 13;
    const int rowH   = 10;

    for (int i = 0; i < WIFI_VISIBLE_ROWS; i++) {
        int idx = _scrollOffset + i;
        if (idx >= _count) break;

        int y = startY + (i * rowH);
        const auto& net = _nets[idx];

        if (idx == _selectedIndex) {
            oled.fillRect(0, y, OLED_WIDTH - 5, rowH, SSD1306_WHITE);
            oled.setTextColor(SSD1306_BLACK);
        } else {
            oled.setTextColor(SSD1306_WHITE);
        }

        // Format compactly: "*SSID       -65" (* = encrypted, space = open)
        char rowBuf[22];
        snprintf(rowBuf, sizeof(rowBuf), "%c%-11.11s %3ld C%ld",
                 net.isOpen ? ' ' : '*',
                 net.ssid,
                 (long)net.rssi,
                 (long)net.channel);
        oled.setCursor(2, y + 1);
        oled.print(rowBuf);
    }

    oled.setTextColor(SSD1306_WHITE);
    display.drawScrollbar(startY, WIFI_VISIBLE_ROWS * rowH, _count, WIFI_VISIBLE_ROWS, _scrollOffset);
    display.drawFooter("*:Exit D:Scan", "5/#:Info");
}
