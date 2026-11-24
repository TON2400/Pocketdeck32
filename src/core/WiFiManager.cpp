#include "WiFiManager.h"
#include "SettingsManager.h"
#include "TimeManager.h"
#include "../apps/SystemMonitor.h"
#include <WiFi.h>
#include <Preferences.h>
#include <string.h>
#include <stdio.h>

WiFiManager g_wifi;
static Preferences s_wifiPrefs;

WiFiManager::WiFiManager()
    : _saveOnConnect(false),
      _state(WIFI_MGR_IDLE),
      _connectStartMs(0),
      _lastCheckMs(0) {
    _lastSsid[0]   = '\0';
    _targetSsid[0] = '\0';
    _targetPass[0] = '\0';
    for (int i = 0; i < MAX_PROFILES; i++) {
        _profiles[i].ssid[0] = '\0';
        _profiles[i].pass[0] = '\0';
        _profiles[i].used    = false;
    }
}

void WiFiManager::begin() {
    loadProfiles();
    WiFi.persistent(false);
    WiFi.setAutoReconnect(true);

    if (g_settings.get().autoConnectWiFi && hasAnySavedProfile()) {
        connectSaved();
    } else {
        _state = WIFI_MGR_IDLE;
        syncMonitorStatus();
    }
}

void WiFiManager::loadProfiles() {
    for (int i = 0; i < MAX_PROFILES; i++) {
        _profiles[i].ssid[0] = '\0';
        _profiles[i].pass[0] = '\0';
        _profiles[i].used    = false;
    }
    _lastSsid[0] = '\0';

    if (s_wifiPrefs.begin("pdeck_wifi", true)) {
        String last = s_wifiPrefs.getString("last", "");
        strncpy(_lastSsid, last.c_str(), sizeof(_lastSsid) - 1);
        _lastSsid[sizeof(_lastSsid) - 1] = '\0';

        for (int i = 0; i < MAX_PROFILES; i++) {
            char kSsid[10];
            char kPass[10];
            snprintf(kSsid, sizeof(kSsid), "ssid%d", i);
            snprintf(kPass, sizeof(kPass), "pass%d", i);
            String s = s_wifiPrefs.getString(kSsid, "");
            if (s.length() > 0) {
                String p = s_wifiPrefs.getString(kPass, "");
                strncpy(_profiles[i].ssid, s.c_str(), sizeof(_profiles[i].ssid) - 1);
                _profiles[i].ssid[sizeof(_profiles[i].ssid) - 1] = '\0';
                strncpy(_profiles[i].pass, p.c_str(), sizeof(_profiles[i].pass) - 1);
                _profiles[i].pass[sizeof(_profiles[i].pass) - 1] = '\0';
                _profiles[i].used = true;
            }
        }
        s_wifiPrefs.end();
    }
}

void WiFiManager::persistProfiles() {
    if (s_wifiPrefs.begin("pdeck_wifi", false)) {
        s_wifiPrefs.putString("last", _lastSsid);
        for (int i = 0; i < MAX_PROFILES; i++) {
            char kSsid[10];
            char kPass[10];
            snprintf(kSsid, sizeof(kSsid), "ssid%d", i);
            snprintf(kPass, sizeof(kPass), "pass%d", i);
            if (_profiles[i].used && _profiles[i].ssid[0] != '\0') {
                s_wifiPrefs.putString(kSsid, _profiles[i].ssid);
                s_wifiPrefs.putString(kPass, _profiles[i].pass);
            } else {
                s_wifiPrefs.putString(kSsid, "");
                s_wifiPrefs.putString(kPass, "");
            }
        }
        s_wifiPrefs.end();
    }
}

void WiFiManager::saveProfile(const char* ssid, const char* password) {
    if (!ssid || ssid[0] == '\0') return;
    const char* pass = password ? password : "";

    strncpy(_lastSsid, ssid, sizeof(_lastSsid) - 1);
    _lastSsid[sizeof(_lastSsid) - 1] = '\0';

    // Update existing profile if matching SSID
    for (int i = 0; i < MAX_PROFILES; i++) {
        if (_profiles[i].used && strcmp(_profiles[i].ssid, ssid) == 0) {
            strncpy(_profiles[i].pass, pass, sizeof(_profiles[i].pass) - 1);
            _profiles[i].pass[sizeof(_profiles[i].pass) - 1] = '\0';
            persistProfiles();
            return;
        }
    }

    // Find empty slot
    for (int i = 0; i < MAX_PROFILES; i++) {
        if (!_profiles[i].used) {
            strncpy(_profiles[i].ssid, ssid, sizeof(_profiles[i].ssid) - 1);
            _profiles[i].ssid[sizeof(_profiles[i].ssid) - 1] = '\0';
            strncpy(_profiles[i].pass, pass, sizeof(_profiles[i].pass) - 1);
            _profiles[i].pass[sizeof(_profiles[i].pass) - 1] = '\0';
            _profiles[i].used = true;
            persistProfiles();
            return;
        }
    }

    // Shift and overwrite oldest slot (index 0)
    for (int i = 1; i < MAX_PROFILES; i++) {
        _profiles[i - 1] = _profiles[i];
    }
    strncpy(_profiles[MAX_PROFILES - 1].ssid, ssid, sizeof(_profiles[0].ssid) - 1);
    _profiles[MAX_PROFILES - 1].ssid[sizeof(_profiles[0].ssid) - 1] = '\0';
    strncpy(_profiles[MAX_PROFILES - 1].pass, pass, sizeof(_profiles[0].pass) - 1);
    _profiles[MAX_PROFILES - 1].pass[sizeof(_profiles[0].pass) - 1] = '\0';
    _profiles[MAX_PROFILES - 1].used = true;
    persistProfiles();
}

void WiFiManager::forgetProfile(const char* ssid) {
    if (!ssid || ssid[0] == '\0') return;
    for (int i = 0; i < MAX_PROFILES; i++) {
        if (_profiles[i].used && strcmp(_profiles[i].ssid, ssid) == 0) {
            _profiles[i].ssid[0] = '\0';
            _profiles[i].pass[0] = '\0';
            _profiles[i].used    = false;
        }
    }
    if (strcmp(_lastSsid, ssid) == 0) {
        _lastSsid[0] = '\0';
        for (int i = 0; i < MAX_PROFILES; i++) {
            if (_profiles[i].used) {
                strncpy(_lastSsid, _profiles[i].ssid, sizeof(_lastSsid) - 1);
                _lastSsid[sizeof(_lastSsid) - 1] = '\0';
                break;
            }
        }
    }
    persistProfiles();
}

void WiFiManager::forgetAllProfiles() {
    for (int i = 0; i < MAX_PROFILES; i++) {
        _profiles[i].ssid[0] = '\0';
        _profiles[i].pass[0] = '\0';
        _profiles[i].used    = false;
    }
    _lastSsid[0] = '\0';
    persistProfiles();
}

bool WiFiManager::hasSavedProfile(const char* ssid, char* outPass, size_t passLen) const {
    if (!ssid || ssid[0] == '\0') return false;
    for (int i = 0; i < MAX_PROFILES; i++) {
        if (_profiles[i].used && strcmp(_profiles[i].ssid, ssid) == 0) {
            if (outPass && passLen > 0) {
                strncpy(outPass, _profiles[i].pass, passLen - 1);
                outPass[passLen - 1] = '\0';
            }
            return true;
        }
    }
    return false;
}

bool WiFiManager::hasAnySavedProfile() const {
    for (int i = 0; i < MAX_PROFILES; i++) {
        if (_profiles[i].used && _profiles[i].ssid[0] != '\0') {
            return true;
        }
    }
    return false;
}

int WiFiManager::getSavedProfileCount() const {
    int cnt = 0;
    for (int i = 0; i < MAX_PROFILES; i++) {
        if (_profiles[i].used && _profiles[i].ssid[0] != '\0') cnt++;
    }
    return cnt;
}

const WiFiSavedProfile* WiFiManager::getSavedProfile(int index) const {
    int cnt = 0;
    for (int i = 0; i < MAX_PROFILES; i++) {
        if (_profiles[i].used && _profiles[i].ssid[0] != '\0') {
            if (cnt == index) return &_profiles[i];
            cnt++;
        }
    }
    return nullptr;
}

bool WiFiManager::connect(const char* ssid, const char* password, bool saveCredentials) {
    if (!ssid || ssid[0] == '\0') return false;

    strncpy(_targetSsid, ssid, sizeof(_targetSsid) - 1);
    _targetSsid[sizeof(_targetSsid) - 1] = '\0';

    if (password && password[0] != '\0') {
        strncpy(_targetPass, password, sizeof(_targetPass) - 1);
        _targetPass[sizeof(_targetPass) - 1] = '\0';
    } else {
        _targetPass[0] = '\0';
    }

    _saveOnConnect = saveCredentials;

    WiFi.mode(WIFI_STA);
    WiFi.disconnect(false);
    if (_targetPass[0] != '\0') {
        WiFi.begin(_targetSsid, _targetPass);
    } else {
        WiFi.begin(_targetSsid);
    }

    _state          = WIFI_MGR_CONNECTING;
    _connectStartMs = millis();
    _lastCheckMs    = _connectStartMs;
    syncMonitorStatus();
    return true;
}

bool WiFiManager::connectSaved() {
    if (_lastSsid[0] != '\0') {
        char pass[65];
        if (hasSavedProfile(_lastSsid, pass, sizeof(pass))) {
            return connect(_lastSsid, pass, false);
        }
    }
    for (int i = 0; i < MAX_PROFILES; i++) {
        if (_profiles[i].used && _profiles[i].ssid[0] != '\0') {
            return connect(_profiles[i].ssid, _profiles[i].pass, false);
        }
    }
    return false;
}

void WiFiManager::disconnect(bool turnOffRadio) {
    WiFi.disconnect(turnOffRadio);
    if (turnOffRadio) {
        WiFi.mode(WIFI_OFF);
        _state = WIFI_MGR_IDLE;
    } else {
        _state = WIFI_MGR_DISCONNECTED;
    }
    syncMonitorStatus();
}

bool WiFiManager::isConnected() const {
    return (WiFi.getMode() != WIFI_OFF) && (WiFi.status() == WL_CONNECTED);
}

const char* WiFiManager::getStateLabel() const {
    if (isConnected()) return "CONNECTED";
    switch (_state) {
        case WIFI_MGR_CONNECTING:   return "CONNECTING";
        case WIFI_MGR_CONNECTED:    return "CONNECTED";
        case WIFI_MGR_FAILED:       return "CONN FAILED";
        case WIFI_MGR_DISCONNECTED: return "DISCONNECTED";
        default:                    return "IDLE";
    }
}

void WiFiManager::getConnectedSSID(char* outBuf, size_t maxLen) const {
    if (!outBuf || maxLen == 0) return;
    outBuf[0] = '\0';
    if (isConnected()) {
        String s = WiFi.SSID();
        strncpy(outBuf, s.c_str(), maxLen - 1);
        outBuf[maxLen - 1] = '\0';
    } else if (_state == WIFI_MGR_CONNECTING) {
        strncpy(outBuf, _targetSsid, maxLen - 1);
        outBuf[maxLen - 1] = '\0';
    }
}

void WiFiManager::getIPString(char* outBuf, size_t maxLen) const {
    if (!outBuf || maxLen == 0) return;
    if (isConnected()) {
        IPAddress ip = WiFi.localIP();
        snprintf(outBuf, maxLen, "%u.%u.%u.%u", ip[0], ip[1], ip[2], ip[3]);
    } else {
        strncpy(outBuf, "0.0.0.0", maxLen - 1);
        outBuf[maxLen - 1] = '\0';
    }
}

int32_t WiFiManager::getRSSI() const {
    if (isConnected()) {
        return WiFi.RSSI();
    }
    return 0;
}

void WiFiManager::syncMonitorStatus() {
    bool active = isConnected() || (_state == WIFI_MGR_CONNECTING);
    SystemMonitorApp::setWirelessStatus(active,
                                        SystemMonitorApp::lastWifiCount(),
                                        SystemMonitorApp::isBleReady(),
                                        SystemMonitorApp::lastBleCount());
}

void WiFiManager::update() {
    uint32_t now = millis();
    if ((now - _lastCheckMs) < 200UL) return;
    _lastCheckMs = now;

    if (_state == WIFI_MGR_CONNECTING) {
        wl_status_t st = WiFi.status();
        if (st == WL_CONNECTED) {
            _state = WIFI_MGR_CONNECTED;
            if (_saveOnConnect && _targetSsid[0] != '\0') {
                saveProfile(_targetSsid, _targetPass);
                _saveOnConnect = false;
            }
            syncMonitorStatus();
            g_time.onWiFiConnected();
        } else if ((now - _connectStartMs) >= CONNECT_TIMEOUT_MS ||
                   st == WL_CONNECT_FAILED || st == WL_NO_SSID_AVAIL) {
            if ((now - _connectStartMs) >= CONNECT_TIMEOUT_MS) {
                WiFi.disconnect(false);
                _state = WIFI_MGR_FAILED;
                syncMonitorStatus();
            }
        }
    } else if (_state == WIFI_MGR_CONNECTED) {
        if (WiFi.status() != WL_CONNECTED) {
            _state = WIFI_MGR_DISCONNECTED;
            syncMonitorStatus();
        }
    } else {
        if (WiFi.getMode() != WIFI_OFF && WiFi.status() == WL_CONNECTED) {
            _state = WIFI_MGR_CONNECTED;
            syncMonitorStatus();
            g_time.onWiFiConnected();
        }
    }
}
