#include "WiFiScanner.h"
#include "SystemMonitor.h"
#include "../core/WiFiManager.h"
#include "../core/TimeManager.h"
#include "../core/SystemManager.h"
#include <WiFi.h>
#include <string.h>
#include <stdio.h>

static const int WIFI_VISIBLE_ROWS = 4;

WiFiScannerApp::WiFiScannerApp()
    : _count(0),
      _selectedIndex(0),
      _scrollOffset(0),
      _scanning(false),
      _view(WVIEW_LIST),
      _scanStartMs(0) {
    _passBuf[0] = '\0';
}

void WiFiScannerApp::onEnter() {
    _view          = WVIEW_LIST;
    _selectedIndex = 0;
    _scrollOffset  = 0;
    if (_count == 0 && !g_wifi.isConnecting()) {
        startAsyncScan();
    }
}

void WiFiScannerApp::onExit() {
    if (_scanning) {
        WiFi.scanDelete();
        _scanning = false;
    }
    // Keep Wi-Fi active if connected or connecting; only power off radio if completely idle
    if (!g_wifi.isConnected() && !g_wifi.isConnecting()) {
        WiFi.mode(WIFI_OFF);
        SystemMonitorApp::setWirelessStatus(false, _count,
                                            SystemMonitorApp::isBleReady(),
                                            SystemMonitorApp::lastBleCount());
    } else {
        SystemMonitorApp::setWirelessStatus(true, _count,
                                            SystemMonitorApp::isBleReady(),
                                            SystemMonitorApp::lastBleCount());
    }
}

void WiFiScannerApp::startAsyncScan() {
    if (_scanning) return;

    if (!g_wifi.isConnected() && !g_wifi.isConnecting()) {
        WiFi.mode(WIFI_STA);
    }

    // Non-blocking async scan (preserves existing STA connection if connected)
    WiFi.scanDelete();
    int16_t state = WiFi.scanNetworks(true, false);
    _scanning     = (state == WIFI_SCAN_RUNNING || state >= 0);
    _scanStartMs  = millis();
    _view         = WVIEW_LIST;

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

    SystemMonitorApp::setWirelessStatus(g_wifi.isConnected() || g_wifi.isConnecting(), _count,
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

void WiFiScannerApp::beginConnectSelected(bool forcePasswordPrompt) {
    if (_selectedIndex < 0 || _selectedIndex >= _count) return;
    const auto& net = _nets[_selectedIndex];

    if (strcmp(net.ssid, "<Hidden>") == 0) {
        g_system.display().showToast("HIDDEN SSID", 900);
        return;
    }

    char curSsid[33];
    g_wifi.getConnectedSSID(curSsid, sizeof(curSsid));
    if (!forcePasswordPrompt && g_wifi.isConnected() && strcmp(curSsid, net.ssid) == 0) {
        g_wifi.disconnect(false);
        g_system.display().showToast("DISCONNECTED", 800);
        return;
    }

    if (net.isOpen) {
        g_wifi.connect(net.ssid, "", true);
        _view = WVIEW_CONNECTING;
        return;
    }

    char savedPass[65];
    if (!forcePasswordPrompt && g_wifi.hasSavedProfile(net.ssid, savedPass, sizeof(savedPass))) {
        g_wifi.connect(net.ssid, savedPass, true);
        _view = WVIEW_CONNECTING;
        return;
    }

    // Prompt user for Wi-Fi password using TextInputManager
    _passBuf[0] = '\0';
    if (g_wifi.hasSavedProfile(net.ssid, savedPass, sizeof(savedPass))) {
        strncpy(_passBuf, savedPass, sizeof(_passBuf) - 1);
        _passBuf[sizeof(_passBuf) - 1] = '\0';
    }
    _textInput.begin(_passBuf, sizeof(_passBuf), _passBuf);
    _view = WVIEW_PASSWORD;
}

bool WiFiScannerApp::update() {
    if (_view == WVIEW_PASSWORD) {
        return _textInput.update();
    }

    if (_view == WVIEW_CONNECTING) {
        if (g_wifi.isConnected()) {
            _view = WVIEW_DETAILS;
            g_system.display().showToast("WIFI CONNECTED!", 900);
            return true;
        } else if (!g_wifi.isConnecting()) {
            _view = WVIEW_DETAILS;
            g_system.display().showToast("CONNECT FAILED", 900);
            return true;
        }
        return true;
    }

    if (!_scanning) return false;

    int16_t res = WiFi.scanComplete();
    if (res == WIFI_SCAN_RUNNING) {
        return false;
    } else if (res >= 0) {
        _scanning = false;
        collectScanResults(res);
        return true;
    } else {
        _scanning = false;
        return true;
    }
}

bool WiFiScannerApp::handleInput(const InputEvent& ev) {
    if (_view == WVIEW_PASSWORD) {
        TextInputResult res = _textInput.handleInput(ev);
        if (res == TEXT_RES_SUBMITTED) {
            if (_selectedIndex >= 0 && _selectedIndex < _count) {
                g_wifi.connect(_nets[_selectedIndex].ssid, _passBuf, true);
                _view = WVIEW_CONNECTING;
            } else {
                _view = WVIEW_DETAILS;
            }
        } else if (res == TEXT_RES_CANCELLED) {
            _view = WVIEW_DETAILS;
        }
        return false;
    }

    if (ev.type != EVT_PRESS && ev.type != EVT_REPEAT) {
        return false;
    }

    if (_view == WVIEW_CONNECTING) {
        if (ev.action == ACT_BACK && ev.type == EVT_PRESS) {
            g_wifi.disconnect(false);
            _view = WVIEW_DETAILS;
        }
        return false;
    }

    if (_view == WVIEW_DETAILS) {
        if (ev.action == ACT_BACK && ev.type == EVT_PRESS) {
            _view = WVIEW_LIST;
        } else if (ev.action == ACT_SELECT && ev.type == EVT_PRESS) {
            beginConnectSelected(false);
        } else if ((ev.action == ACT_MENU || ev.rawKey == 'D') && ev.type == EVT_PRESS) {
            if (_selectedIndex >= 0 && _selectedIndex < _count) {
                const auto& net = _nets[_selectedIndex];
                if (!net.isOpen) {
                    beginConnectSelected(true); // Enter/change password
                }
            }
        } else if (ev.rawKey == '3' && ev.type == EVT_PRESS) {
            if (_selectedIndex >= 0 && _selectedIndex < _count) {
                g_wifi.forgetProfile(_nets[_selectedIndex].ssid);
                g_system.display().showToast("SAVED FORGOTTEN", 800);
            }
        }
        return false;
    }

    // WVIEW_LIST
    if (ev.action == ACT_BACK && ev.type == EVT_PRESS) {
        return true; // Return to Main Menu
    }

    // Rescan on MENU ('D' or IR 'EQ')
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
            _view = WVIEW_DETAILS;
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

    if (_view == WVIEW_PASSWORD && _selectedIndex >= 0 && _selectedIndex < _count) {
        char hdr[24];
        snprintf(hdr, sizeof(hdr), "PASS [%s]", _textInput.getCaseModeLabel());
        display.drawHeader(hdr, mode, showModeBadge);

        oled.setCursor(2, 14);
        oled.printf("SSID: %.15s", _nets[_selectedIndex].ssid);
        _textInput.drawInputBox(oled, 2, 26, OLED_WIDTH - 4, 14, false);
        oled.setCursor(2, 43);
        oled.print(F("#:Connect  *:Bksp"));
        display.drawFooter("A:Case *:Bksp", "#:Join C:Back");
        return;
    }

    if (_view == WVIEW_CONNECTING && _selectedIndex >= 0 && _selectedIndex < _count) {
        display.drawHeader("CONNECTING...", mode, showModeBadge);
        oled.setCursor(6, 20);
        oled.printf("SSID: %.15s", _nets[_selectedIndex].ssid);
        oled.setCursor(6, 34);
        oled.print(F("Associating & DHCP..."));
        display.drawFooter("*:Cancel", nullptr);
        return;
    }

    char curSsid[33];
    g_wifi.getConnectedSSID(curSsid, sizeof(curSsid));
    bool wifiConn = g_wifi.isConnected();

    if (_view == WVIEW_DETAILS && _selectedIndex >= 0 && _selectedIndex < _count) {
        const auto& net = _nets[_selectedIndex];
        bool isThisConn  = wifiConn && (strcmp(curSsid, net.ssid) == 0);
        bool isThisSaved = g_wifi.hasSavedProfile(net.ssid);

        display.drawHeader(isThisConn ? "WIFI [ONLINE]" : "WIFI DETAIL", mode, showModeBadge);

        oled.setCursor(2, 13);
        oled.printf("SSID: %.15s", net.ssid);

        oled.setCursor(2, 23);
        oled.printf("RSSI:%lddBm CH:%ld", (long)net.rssi, (long)net.channel);

        oled.setCursor(2, 33);
        oled.printf("Auth:%s %s",
                    getEncryptionLabel(net.encType),
                    isThisSaved ? "[SAV]" : "");

        oled.setCursor(2, 43);
        if (isThisConn) {
            char ipBuf[20];
            g_wifi.getIPString(ipBuf, sizeof(ipBuf));
            oled.printf("IP: %s", ipBuf);
            display.drawFooter("*:Back D:Pass", "5/#:Disconn");
        } else {
            oled.print(F("Status: Disconnected"));
            display.drawFooter("*:Back D:Pass", "5/#:Connect");
        }
        return;
    }

    char hdr[24];
    snprintf(hdr, sizeof(hdr), "WIFI (%d)%s", _count, wifiConn ? " [ON]" : "");
    display.drawHeader(hdr, mode, showModeBadge);

    if (_count == 0) {
        oled.setCursor(14, 24);
        oled.print(F("No networks found"));
        if (wifiConn) {
            char ipBuf[20];
            g_wifi.getIPString(ipBuf, sizeof(ipBuf));
            oled.setCursor(8, 36);
            oled.printf("IP: %s", ipBuf);
        }
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
        bool isThisConn = wifiConn && (strcmp(curSsid, net.ssid) == 0);

        if (idx == _selectedIndex) {
            oled.fillRect(0, y, OLED_WIDTH - 5, rowH, SSD1306_WHITE);
            oled.setTextColor(SSD1306_BLACK);
        } else {
            oled.setTextColor(SSD1306_WHITE);
        }

        // Format compactly: ">SSID -65 C6" (> = connected, * = encrypted, space = open)
        char prefix = isThisConn ? '>' : (net.isOpen ? ' ' : '*');
        char rowBuf[22];
        snprintf(rowBuf, sizeof(rowBuf), "%c%-11.11s %3ld C%ld",
                 prefix,
                 net.ssid,
                 (long)net.rssi,
                 (long)net.channel);
        oled.setCursor(2, y + 1);
        oled.print(rowBuf);
    }

    oled.setTextColor(SSD1306_WHITE);
    display.drawScrollbar(startY, WIFI_VISIBLE_ROWS * rowH, _count, WIFI_VISIBLE_ROWS, _scrollOffset);
    display.drawFooter("*:Exit D:Scan", "5/#:Connect");
}
