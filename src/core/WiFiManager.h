#ifndef POCKETDECK_WIFI_MANAGER_H
#define POCKETDECK_WIFI_MANAGER_H

#include <Arduino.h>

enum WiFiConnState : uint8_t {
    WIFI_MGR_IDLE = 0,
    WIFI_MGR_CONNECTING,
    WIFI_MGR_CONNECTED,
    WIFI_MGR_FAILED,
    WIFI_MGR_DISCONNECTED
};

struct WiFiSavedProfile {
    char ssid[33];
    char pass[65];
    bool used;
};

class WiFiManager {
public:
    WiFiManager();

    void begin();
    void update();

    bool connect(const char* ssid, const char* password, bool saveCredentials = true);
    bool connectSaved();
    void disconnect(bool turnOffRadio = false);

    bool isConnected() const;
    bool isConnecting() const { return _state == WIFI_MGR_CONNECTING; }
    WiFiConnState getState() const { return _state; }
    const char* getStateLabel() const;

    void getConnectedSSID(char* outBuf, size_t maxLen) const;
    void getIPString(char* outBuf, size_t maxLen) const;
    int32_t getRSSI() const;

    bool hasSavedProfile(const char* ssid, char* outPass = nullptr, size_t passLen = 0) const;
    bool hasAnySavedProfile() const;
    const char* getLastSavedSSID() const { return _lastSsid; }

    void saveProfile(const char* ssid, const char* password);
    void forgetProfile(const char* ssid);
    void forgetAllProfiles();

    int getSavedProfileCount() const;
    const WiFiSavedProfile* getSavedProfile(int index) const;

private:
    void loadProfiles();
    void persistProfiles();
    void syncMonitorStatus();

    static const int MAX_PROFILES = 4;
    static const uint32_t CONNECT_TIMEOUT_MS = 12000UL;

    WiFiSavedProfile _profiles[MAX_PROFILES];
    char             _lastSsid[33];
    char             _targetSsid[33];
    char             _targetPass[65];
    bool             _saveOnConnect;

    WiFiConnState    _state;
    uint32_t         _connectStartMs;
    uint32_t         _lastCheckMs;
};

extern WiFiManager g_wifi;

#endif // POCKETDECK_WIFI_MANAGER_H
