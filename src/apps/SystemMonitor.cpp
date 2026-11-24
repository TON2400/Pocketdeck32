#include "SystemMonitor.h"
#include "../core/StorageManager.h"
#include "../core/WiFiManager.h"
#include "../core/TimeManager.h"
#include "../utils/Helpers.h"
#include <stdio.h>

bool SystemMonitorApp::_wifiActive    = false;
int  SystemMonitorApp::_lastWifiCount = 0;
bool SystemMonitorApp::_bleActive     = false;
int  SystemMonitorApp::_lastBleCount  = 0;

void SystemMonitorApp::setWirelessStatus(bool wifiActive, int wifiCount, bool bleActive, int bleCount) {
    _wifiActive    = wifiActive;
    _lastWifiCount = wifiCount;
    _bleActive     = bleActive;
    _lastBleCount  = bleCount;
}

SystemMonitorApp::SystemMonitorApp()
    : _page(0),
      _lastRefreshMs(0) {}

void SystemMonitorApp::onEnter() {
    _page          = 0;
    _lastRefreshMs = millis();
}

bool SystemMonitorApp::handleInput(const InputEvent& ev) {
    if (ev.type != EVT_PRESS) return false;

    if (ev.action == ACT_BACK) {
        return true;
    }

    if (ev.action == ACT_UP || ev.action == ACT_LEFT ||
        ev.action == ACT_DOWN || ev.action == ACT_RIGHT ||
        ev.action == ACT_SELECT) {
        _page = (_page + 1) % 2;
    }

    return false;
}

bool SystemMonitorApp::update() {
    uint32_t now = millis();
    if ((now - _lastRefreshMs) >= 1000UL) {
        _lastRefreshMs = now;
        return true;
    }
    return false;
}

void SystemMonitorApp::render(DisplayManager& display, InputMode mode, bool showModeBadge) {
    auto& oled = display.oled();
    display.clear();
    display.drawHeader(_page == 0 ? "SYS MON (1/2)" : "SYS MON (2/2)", mode, showModeBadge);

    char upStr[16];
    Helpers::formatUptime(millis(), upStr, sizeof(upStr));

    if (_page == 0) {
        uint32_t freeHeap  = ESP.getFreeHeap();
        uint32_t totalHeap = ESP.getHeapSize();
        uint32_t usedPct   = totalHeap > 0 ? ((totalHeap - freeHeap) * 100UL) / totalHeap : 0;

        oled.setCursor(2, 14);
        oled.printf("Uptime: %s", upStr);

        oled.setCursor(2, 24);
        oled.printf("Heap: %luKB (%lu%%)", (unsigned long)(freeHeap / 1024UL), (unsigned long)usedPct);

        oled.setCursor(2, 34);
        oled.printf("Input: %s", (mode == MODE_KEYPAD) ? "KEYPAD" : "IR REMOTE");

        oled.setCursor(2, 44);
        oled.printf("WiFi:%s(%d) BLE:%s(%d)",
                    _wifiActive ? "ON" : "IDLE", _lastWifiCount,
                    _bleActive  ? "ON" : "IDLE", _lastBleCount);
    } else {
        uint32_t fsUsed  = g_storage.usedBytes();
        uint32_t fsTotal = g_storage.totalBytes();

        oled.setCursor(2, 14);
        oled.printf("CPU: %luMHz (%s)", (unsigned long)ESP.getCpuFreqMHz(), ESP.getChipModel());

        oled.setCursor(2, 24);
        oled.printf("MinHeap: %lu KB", (unsigned long)(ESP.getMinFreeHeap() / 1024UL));

        oled.setCursor(2, 34);
        oled.printf("FS:%lu/%luK NTP:%s",
                    (unsigned long)(fsUsed / 1024UL),
                    (unsigned long)(fsTotal / 1024UL),
                    g_time.isTimeValid() ? "OK" : "--");

        oled.setCursor(2, 44);
        if (g_wifi.isConnected()) {
            char ipBuf[20];
            g_wifi.getIPString(ipBuf, sizeof(ipBuf));
            oled.printf("IP: %s", ipBuf);
        } else {
            oled.printf("SDK: %s", ESP.getSdkVersion());
        }
    }

    display.drawFooter("*:Back", "2/8/5:Page");
}
