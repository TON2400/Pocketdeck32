#include "WeatherApp.h"
#include "../core/SystemManager.h"
#include "../core/StorageManager.h"
#include "../core/SettingsManager.h"
#include "../core/WiFiManager.h"
#include "../core/TimeManager.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <LittleFS.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <math.h>

static const int W_VISIBLE_ROWS = 4;
static const int W_MENU_COUNT   = 7;

// Lightweight bounded JSON parsing helpers (zero dynamic allocation, no buffer truncation)
static const char* findJsonKeyBounded(const char* start, const char* end, const char* key) {
    if (!start || !key) return nullptr;
    char pattern[36];
    snprintf(pattern, sizeof(pattern), "\"%s\"", key);
    size_t patLen = strlen(pattern);

    const char* cur = start;
    while (cur && (!end || (cur + patLen) <= end)) {
        const char* p = strstr(cur, pattern);
        if (!p || (end && (p + patLen) > end)) return nullptr;
        p += patLen;
        while ((!end || p < end) && (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n')) p++;
        if ((!end || p < end) && *p == ':') {
            p++;
            while ((!end || p < end) && (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n')) p++;
            return p;
        }
        cur = p;
    }
    return nullptr;
}

static const char* findJsonKey(const char* json, const char* key) {
    return findJsonKeyBounded(json, nullptr, key);
}

static bool extractJsonStringBounded(const char* start, const char* end, const char* key, char* outBuf, size_t maxLen) {
    if (!outBuf || maxLen == 0) return false;
    outBuf[0] = '\0';
    const char* p = findJsonKeyBounded(start, end, key);
    if (!p || *p != '\"') return false;
    p++; // skip opening quote
    size_t i = 0;
    while (*p && (!end || p < end) && *p != '\"' && i + 1 < maxLen) {
        if (*p == '\\' && *(p + 1) != '\0') {
            p++;
        }
        unsigned char uc = (unsigned char)*p;
        if (uc >= 32 && uc < 127) {
            outBuf[i++] = *p;
        }
        p++;
    }
    outBuf[i] = '\0';
    return true;
}

static bool extractJsonFloatBounded(const char* start, const char* end, const char* key, float& outVal) {
    const char* p = findJsonKeyBounded(start, end, key);
    if (!p) return false;
    if (*p == '-' || (*p >= '0' && *p <= '9')) {
        outVal = (float)atof(p);
        return true;
    }
    return false;
}

static bool extractJsonString(const char* json, const char* key, char* outBuf, size_t maxLen) {
    return extractJsonStringBounded(json, nullptr, key, outBuf, maxLen);
}

static bool extractJsonFloat(const char* json, const char* key, float& outVal) {
    return extractJsonFloatBounded(json, nullptr, key, outVal);
}

static bool extractJsonInt(const char* json, const char* key, int16_t& outVal) {
    const char* p = findJsonKey(json, key);
    if (!p) return false;
    if (*p == '-' || (*p >= '0' && *p <= '9')) {
        outVal = (int16_t)atoi(p);
        return true;
    }
    return false;
}

static int extractJsonFloatArray(const char* section, const char* key, float* outArr, int maxCount) {
    const char* p = findJsonKey(section, key);
    if (!p || *p != '[') return 0;
    p++;
    int count = 0;
    while (*p && *p != ']' && count < maxCount) {
        while (*p && (*p == ' ' || *p == ',' || *p == '\n' || *p == '\r')) p++;
        if (*p == ']' || *p == '\0') break;
        if (strncmp(p, "null", 4) == 0) {
            outArr[count++] = 0.0f;
            p += 4;
        } else {
            outArr[count++] = (float)atof(p);
            while (*p && *p != ',' && *p != ']') p++;
        }
    }
    return count;
}

static int extractJsonIntArray(const char* section, const char* key, int16_t* outArr, int maxCount) {
    const char* p = findJsonKey(section, key);
    if (!p || *p != '[') return 0;
    p++;
    int count = 0;
    while (*p && *p != ']' && count < maxCount) {
        while (*p && (*p == ' ' || *p == ',' || *p == '\n' || *p == '\r')) p++;
        if (*p == ']' || *p == '\0') break;
        if (strncmp(p, "null", 4) == 0) {
            outArr[count++] = 0;
            p += 4;
        } else {
            outArr[count++] = (int16_t)atoi(p);
            while (*p && *p != ',' && *p != ']') p++;
        }
    }
    return count;
}

static int extractJsonStringArray(const char* section, const char* key, char outArr[][12], int maxCount) {
    const char* p = findJsonKey(section, key);
    if (!p || *p != '[') return 0;
    p++;
    int count = 0;
    while (*p && *p != ']' && count < maxCount) {
        while (*p && *p != '\"' && *p != ']') p++;
        if (*p != '\"') break;
        p++; // skip opening quote
        size_t i = 0;
        while (*p && *p != '\"' && i < 11) {
            outArr[count][i++] = *p++;
        }
        outArr[count][i] = '\0';
        while (*p && *p != '\"') p++;
        if (*p == '\"') p++;
        count++;
    }
    return count;
}

WeatherApp::WeatherApp()
    : _state(WSTATE_DASHBOARD),
      _dashboardPage(0),
      _menuIndex(0),
      _menuScroll(0),
      _listIndex(0),
      _listScroll(0),
      _favCount(0),
      _recentCount(0),
      _searchResultCount(0),
      _storageLoaded(false),
      _pendingAutoFetch(false) {
    memset(&_weather, 0, sizeof(_weather));
    _searchQuery[0] = '\0';
    _statusMsg[0]   = '\0';
}

void WeatherApp::onEnter() {
    if (!_storageLoaded) {
        ensureWeatherStorage();
        loadFavorites();
        loadRecents();
        loadCachedWeather();
        _storageLoaded = true;
    }

    _state         = WSTATE_DASHBOARD;
    _dashboardPage = 0;
    _menuIndex     = 0;
    _menuScroll    = 0;

    // If no active weather location yet, select first recent or first favorite
    if (!_weather.loc.valid) {
        if (_recentCount > 0) {
            _weather.loc = _recents[0];
        } else if (_favCount > 0) {
            _weather.loc = _favorites[0];
        }
    }

    // If Wi-Fi is connected and we have no valid weather (or cache is stale), schedule auto fetch
    if (g_wifi.isConnected() && _weather.loc.valid) {
        uint8_t refMin = g_settings.get().weatherRefreshMin;
        uint32_t staleMs = (refMin > 0) ? ((uint32_t)refMin * 60000UL) : 1800000UL;
        if (!_weather.valid || _weather.fromCache || (millis() - _weather.lastFetchMs) >= staleMs) {
            _pendingAutoFetch = true;
        }
    }
}

void WeatherApp::ensureWeatherStorage() {
    if (!g_storage.isMounted()) return;
    if (!LittleFS.exists("/weather")) {
        LittleFS.mkdir("/weather");
    }
    if (!LittleFS.exists("/weather/favorites.txt")) {
        const char* defaultFavs =
            "Tokyo|JP|Tokyo|35.6895|139.6917\n"
            "New York|US|New York|40.7143|-74.0060\n"
            "London|GB|England|51.5085|-0.1257\n"
            "Hanoi|VN|Ha Noi|21.0245|105.8412\n";
        g_storage.writeFile("/weather/favorites.txt", defaultFavs);
    }
}

static bool parseLocationLine(const char* line, WeatherLocation& outLoc) {
    if (!line || line[0] == '\0') return false;
    char buf[96];
    strncpy(buf, line, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';

    char* p1 = strtok(buf, "|");
    char* p2 = strtok(nullptr, "|");
    char* p3 = strtok(nullptr, "|");
    char* p4 = strtok(nullptr, "|");
    char* p5 = strtok(nullptr, "|\r\n");

    if (!p1 || !p4 || !p5) return false;

    strncpy(outLoc.name, p1, sizeof(outLoc.name) - 1);
    outLoc.name[sizeof(outLoc.name) - 1] = '\0';

    strncpy(outLoc.country, p2 ? p2 : "", sizeof(outLoc.country) - 1);
    outLoc.country[sizeof(outLoc.country) - 1] = '\0';

    strncpy(outLoc.admin, p3 ? p3 : "", sizeof(outLoc.admin) - 1);
    outLoc.admin[sizeof(outLoc.admin) - 1] = '\0';

    outLoc.lat   = (float)atof(p4);
    outLoc.lon   = (float)atof(p5);
    outLoc.valid = (outLoc.name[0] != '\0');
    return outLoc.valid;
}

void WeatherApp::loadFavorites() {
    _favCount = 0;
    char buf[512];
    if (!g_storage.readFile("/weather/favorites.txt", buf, sizeof(buf))) return;

    char* savePtr = nullptr;
    char* line = strtok_r(buf, "\n", &savePtr);
    while (line && _favCount < MAX_FAVORITES) {
        WeatherLocation loc = {};
        if (parseLocationLine(line, loc)) {
            _favorites[_favCount++] = loc;
        }
        line = strtok_r(nullptr, "\n", &savePtr);
    }
}

void WeatherApp::saveFavorites() {
    char buf[512];
    buf[0] = '\0';
    size_t len = 0;
    for (int i = 0; i < _favCount; i++) {
        if (!_favorites[i].valid) continue;
        int n = snprintf(buf + len, sizeof(buf) - len, "%s|%s|%s|%.4f|%.4f\n",
                         _favorites[i].name,
                         _favorites[i].country,
                         _favorites[i].admin,
                         (double)_favorites[i].lat,
                         (double)_favorites[i].lon);
        if (n > 0 && (len + (size_t)n) < sizeof(buf)) {
            len += (size_t)n;
        }
    }
    g_storage.writeFile("/weather/favorites.txt", buf);
}

void WeatherApp::loadRecents() {
    _recentCount = 0;
    char buf[448];
    if (!g_storage.readFile("/weather/recents.txt", buf, sizeof(buf))) return;

    char* savePtr = nullptr;
    char* line = strtok_r(buf, "\n", &savePtr);
    while (line && _recentCount < MAX_RECENTS) {
        WeatherLocation loc = {};
        if (parseLocationLine(line, loc)) {
            _recents[_recentCount++] = loc;
        }
        line = strtok_r(nullptr, "\n", &savePtr);
    }
}

void WeatherApp::saveRecents() {
    char buf[448];
    buf[0] = '\0';
    size_t len = 0;
    for (int i = 0; i < _recentCount; i++) {
        if (!_recents[i].valid) continue;
        int n = snprintf(buf + len, sizeof(buf) - len, "%s|%s|%s|%.4f|%.4f\n",
                         _recents[i].name,
                         _recents[i].country,
                         _recents[i].admin,
                         (double)_recents[i].lat,
                         (double)_recents[i].lon);
        if (n > 0 && (len + (size_t)n) < sizeof(buf)) {
            len += (size_t)n;
        }
    }
    g_storage.writeFile("/weather/recents.txt", buf);
}

void WeatherApp::loadCachedWeather() {
    char buf[512];
    if (!g_storage.readFile("/weather/cache.txt", buf, sizeof(buf))) return;

    char* savePtr = nullptr;
    char* line1 = strtok_r(buf, "\n", &savePtr);
    char* line2 = strtok_r(nullptr, "\n", &savePtr);
    if (!line1 || !line2) return;

    WeatherLocation loc = {};
    if (!parseLocationLine(line1, loc)) return;

    float tC = 0, flC = 0, wKmh = 0, pHpa = 1013.0f;
    int hum = 0, wDir = 0, wCode = 0, isDay = 1;
    char upd[12] = "--:--";

    if (sscanf(line2, "%f|%f|%d|%f|%d|%f|%d|%d|%11s",
               &tC, &flC, &hum, &wKmh, &wDir, &pHpa, &wCode, &isDay, upd) >= 8) {
        _weather.loc         = loc;
        _weather.tempC       = tC;
        _weather.feelsLikeC  = flC;
        _weather.humidity    = (int16_t)hum;
        _weather.windKmh     = wKmh;
        _weather.windDirDeg  = (int16_t)wDir;
        _weather.pressureHpa = pHpa;
        _weather.weatherCode = (int16_t)wCode;
        _weather.isDay       = (isDay != 0);
        strncpy(_weather.updatedTime, upd, sizeof(_weather.updatedTime) - 1);
        _weather.updatedTime[sizeof(_weather.updatedTime) - 1] = '\0';

        _weather.dailyCount = 0;
        char* dLine = strtok_r(nullptr, "\n", &savePtr);
        while (dLine && _weather.dailyCount < 5) {
            DailyForecastItem& df = _weather.daily[_weather.dailyCount];
            int dCode = 0, dProb = 0;
            float dMin = 0, dMax = 0, dUv = 0;
            if (sscanf(dLine, "%11[^|]|%4[^|]|%d|%f|%f|%d|%f",
                       df.date, df.dayShort, &dCode, &dMin, &dMax, &dProb, &dUv) == 7) {
                df.weatherCode = (int16_t)dCode;
                df.tempMinC    = dMin;
                df.tempMaxC    = dMax;
                df.precipProb  = (uint8_t)dProb;
                df.uvMax       = dUv;
                _weather.dailyCount++;
            }
            dLine = strtok_r(nullptr, "\n", &savePtr);
        }

        _weather.valid     = true;
        _weather.fromCache = true;
    }
}

void WeatherApp::saveCachedWeather() {
    if (!_weather.valid || !_weather.loc.valid) return;
    char buf[512];
    size_t len = 0;

    int n = snprintf(buf + len, sizeof(buf) - len,
                     "%s|%s|%s|%.4f|%.4f\n"
                     "%.1f|%.1f|%d|%.1f|%d|%.1f|%d|%d|%s\n",
                     _weather.loc.name, _weather.loc.country, _weather.loc.admin,
                     (double)_weather.loc.lat, (double)_weather.loc.lon,
                     (double)_weather.tempC, (double)_weather.feelsLikeC,
                     (int)_weather.humidity, (double)_weather.windKmh,
                     (int)_weather.windDirDeg, (double)_weather.pressureHpa,
                     (int)_weather.weatherCode, _weather.isDay ? 1 : 0,
                     _weather.updatedTime[0] ? _weather.updatedTime : "--:--");
    if (n > 0) len += (size_t)n;

    for (uint8_t i = 0; i < _weather.dailyCount && i < 5; i++) {
        const auto& df = _weather.daily[i];
        n = snprintf(buf + len, sizeof(buf) - len, "%s|%s|%d|%.1f|%.1f|%u|%.1f\n",
                     df.date, df.dayShort, (int)df.weatherCode,
                     (double)df.tempMinC, (double)df.tempMaxC,
                     (unsigned)df.precipProb, (double)df.uvMax);
        if (n > 0 && (len + (size_t)n) < sizeof(buf)) {
            len += (size_t)n;
        }
    }
    g_storage.writeFile("/weather/cache.txt", buf);
}

bool WeatherApp::isFavorite(const WeatherLocation& loc) const {
    if (!loc.valid) return false;
    for (int i = 0; i < _favCount; i++) {
        if (strcasecmp(_favorites[i].name, loc.name) == 0 &&
            strcasecmp(_favorites[i].country, loc.country) == 0) {
            return true;
        }
    }
    return false;
}

void WeatherApp::toggleFavorite(const WeatherLocation& loc) {
    if (!loc.valid) return;
    for (int i = 0; i < _favCount; i++) {
        if (strcasecmp(_favorites[i].name, loc.name) == 0 &&
            strcasecmp(_favorites[i].country, loc.country) == 0) {
            removeFavoriteAt(i);
            g_system.display().showToast("FAV REMOVED", 800);
            return;
        }
    }

    if (_favCount >= MAX_FAVORITES) {
        // Shift out oldest favorite
        for (int i = 1; i < MAX_FAVORITES; i++) {
            _favorites[i - 1] = _favorites[i];
        }
        _favCount = MAX_FAVORITES - 1;
    }
    _favorites[_favCount++] = loc;
    saveFavorites();
    g_system.display().showToast("FAV SAVED *", 800);
}

void WeatherApp::removeFavoriteAt(int idx) {
    if (idx < 0 || idx >= _favCount) return;
    for (int i = idx + 1; i < _favCount; i++) {
        _favorites[i - 1] = _favorites[i];
    }
    _favCount--;
    saveFavorites();
}

void WeatherApp::addRecentLocation(const WeatherLocation& loc) {
    if (!loc.valid) return;

    // Remove existing matching entry so it moves to index 0 (MRU)
    for (int i = 0; i < _recentCount; i++) {
        if (strcasecmp(_recents[i].name, loc.name) == 0 &&
            strcasecmp(_recents[i].country, loc.country) == 0) {
            for (int j = i; j > 0; j--) {
                _recents[j] = _recents[j - 1];
            }
            _recents[0] = loc;
            saveRecents();
            return;
        }
    }

    int limit = (_recentCount < MAX_RECENTS) ? _recentCount : (MAX_RECENTS - 1);
    for (int i = limit; i > 0; i--) {
        _recents[i] = _recents[i - 1];
    }
    _recents[0] = loc;
    if (_recentCount < MAX_RECENTS) _recentCount++;
    saveRecents();
}

void WeatherApp::urlEncode(const char* src, char* dst, size_t dstSize) {
    if (!src || !dst || dstSize == 0) return;
    static const char* hex = "0123456789ABCDEF";
    size_t o = 0;
    for (size_t i = 0; src[i] != '\0' && o + 4 < dstSize; i++) {
        unsigned char c = (unsigned char)src[i];
        if (isalnum(c) || c == '-' || c == '_' || c == '.') {
            dst[o++] = (char)c;
        } else if (c == ' ') {
            dst[o++] = '%';
            dst[o++] = '2';
            dst[o++] = '0';
        } else {
            dst[o++] = '%';
            dst[o++] = hex[(c >> 4) & 0x0F];
            dst[o++] = hex[c & 0x0F];
        }
    }
    dst[o] = '\0';
}

void WeatherApp::parseDateToDayShort(const char* isoDate, char* outDay, size_t maxLen) {
    if (!outDay || maxLen == 0) return;
    int y = 0, m = 0, d = 0;
    if (isoDate && sscanf(isoDate, "%d-%d-%d", &y, &m, &d) == 3 && y >= 2024) {
        uint8_t dow = TimeManager::calcDayOfWeek((uint16_t)y, (uint8_t)m, (uint8_t)d);
        strncpy(outDay, TimeManager::getDayOfWeekShort(dow), maxLen - 1);
        outDay[maxLen - 1] = '\0';
    } else {
        strncpy(outDay, "---", maxLen - 1);
        outDay[maxLen - 1] = '\0';
    }
}

bool WeatherApp::performCitySearch(const char* query) {
    _searchResultCount = 0;
    if (!query || query[0] == '\0') return false;

    if (!g_wifi.isConnected()) {
        g_system.display().showToast("WIFI OFFLINE", 1000);
        return false;
    }

    // Draw searching status screen before blocking HTTP call
    auto& disp = g_system.display();
    disp.clear();
    disp.drawHeader("CITY SEARCH...", g_system.input().getMode(), true);
    disp.oled().setCursor(8, 24);
    disp.oled().printf("Searching: %.11s", query);
    disp.oled().setCursor(16, 38);
    disp.oled().print(F("Open-Meteo Geo..."));
    disp.present();

    char encoded[72];
    urlEncode(query, encoded, sizeof(encoded));

    char url[256];
    snprintf(url, sizeof(url),
             "http://geocoding-api.open-meteo.com/v1/search?name=%s&count=5&language=en&format=json",
             encoded);

    WiFiClient client;
    HTTPClient http;
    http.setTimeout(8000);
    http.setConnectTimeout(5000);
    http.useHTTP10(true);
    http.setReuse(false);

    if (!http.begin(client, url)) {
        g_system.display().showToast("HTTP INIT ERR", 900);
        return false;
    }

    int code = http.GET();
    if (code != HTTP_CODE_OK) {
        http.end();
        char errMsg[22];
        snprintf(errMsg, sizeof(errMsg), "GEO ERR %d", code);
        g_system.display().showToast(errMsg, 1000);
        return false;
    }

    String payload = http.getString();
    http.end();

    const char* jsonBuf = payload.c_str();
    const char* arrStart = findJsonKey(jsonBuf, "results");
    if (!arrStart || *arrStart != '[') {
        return false;
    }

    const char* cur = arrStart + 1;
    while (*cur && _searchResultCount < MAX_SEARCH_RESULTS) {
        const char* objStart = strchr(cur, '{');
        if (!objStart) break;

        // Find matching closing brace for this result object
        int depth = 0;
        const char* objEnd = objStart;
        while (*objEnd) {
            if (*objEnd == '{') depth++;
            else if (*objEnd == '}') {
                depth--;
                if (depth == 0) break;
            }
            objEnd++;
        }
        if (*objEnd != '}') break;

        WeatherLocation loc = {};
        if (extractJsonStringBounded(objStart, objEnd, "name", loc.name, sizeof(loc.name)) &&
            extractJsonFloatBounded(objStart, objEnd, "latitude", loc.lat) &&
            extractJsonFloatBounded(objStart, objEnd, "longitude", loc.lon)) {
            if (!extractJsonStringBounded(objStart, objEnd, "country_code", loc.country, sizeof(loc.country))) {
                extractJsonStringBounded(objStart, objEnd, "country", loc.country, sizeof(loc.country));
            }
            extractJsonStringBounded(objStart, objEnd, "admin1", loc.admin, sizeof(loc.admin));
            loc.valid = true;
            _searchResults[_searchResultCount++] = loc;
        }

        cur = objEnd + 1;
    }

    return (_searchResultCount > 0);
}

bool WeatherApp::fetchWeatherForLocation(const WeatherLocation& loc) {
    if (!loc.valid) return false;

    if (!g_wifi.isConnected()) {
        g_system.display().showToast("WIFI OFFLINE", 1000);
        return false;
    }

    auto& disp = g_system.display();
    disp.clear();
    disp.drawHeader("WEATHER SYNC", g_system.input().getMode(), true);
    disp.oled().setCursor(8, 24);
    disp.oled().printf("City: %.14s", loc.name);
    disp.oled().setCursor(8, 36);
    disp.oled().print(F("Fetching forecast..."));
    disp.present();

    // Full URL is ~340 bytes; use a 512-byte buffer so snprintf never truncates query parameters
    char url[512];
    snprintf(url, sizeof(url),
             "http://api.open-meteo.com/v1/forecast?latitude=%.4f&longitude=%.4f"
             "&current=temperature_2m,relative_humidity_2m,apparent_temperature,weather_code,"
             "surface_pressure,wind_speed_10m,wind_direction_10m,is_day"
             "&daily=weather_code,temperature_2m_max,temperature_2m_min,"
             "precipitation_probability_max,uv_index_max&timezone=auto&forecast_days=5",
             (double)loc.lat, (double)loc.lon);

    WiFiClient client;
    HTTPClient http;
    http.setTimeout(8000);
    http.setConnectTimeout(5000);
    http.useHTTP10(true);
    http.setReuse(false);

    if (!http.begin(client, url)) {
        g_system.display().showToast("HTTP ERR", 900);
        return false;
    }

    int code = http.GET();
    if (code != HTTP_CODE_OK) {
        http.end();
        char errMsg[22];
        snprintf(errMsg, sizeof(errMsg), "FETCH ERR %d", code);
        g_system.display().showToast(errMsg, 1000);
        return false;
    }

    String payload = http.getString();
    http.end();

    const char* jsonBuf = payload.c_str();
    const char* curSec = findJsonKey(jsonBuf, "current");
    if (!curSec) {
        g_system.display().showToast("PARSE ERR", 900);
        return false;
    }

    _weather.loc = loc;
    extractJsonFloat(curSec, "temperature_2m", _weather.tempC);
    if (!extractJsonFloat(curSec, "apparent_temperature", _weather.feelsLikeC)) {
        _weather.feelsLikeC = _weather.tempC;
    }
    extractJsonInt(curSec, "relative_humidity_2m", _weather.humidity);
    extractJsonFloat(curSec, "wind_speed_10m", _weather.windKmh);
    extractJsonInt(curSec, "wind_direction_10m", _weather.windDirDeg);
    extractJsonFloat(curSec, "surface_pressure", _weather.pressureHpa);
    extractJsonInt(curSec, "weather_code", _weather.weatherCode);

    int16_t isDayVal = 1;
    extractJsonInt(curSec, "is_day", isDayVal);
    _weather.isDay = (isDayVal != 0);

    // Parse 5-day forecast from "daily" object
    const char* dailySec = findJsonKey(jsonBuf, "daily");
    _weather.dailyCount = 0;
    if (dailySec) {
        char dates[5][12] = {};
        int16_t codes[5]  = {};
        float tMax[5]     = {};
        float tMin[5]     = {};
        int16_t pProb[5]  = {};
        float uv[5]       = {};

        int nDates = extractJsonStringArray(dailySec, "time", dates, 5);
        extractJsonIntArray(dailySec, "weather_code", codes, 5);
        extractJsonFloatArray(dailySec, "temperature_2m_max", tMax, 5);
        extractJsonFloatArray(dailySec, "temperature_2m_min", tMin, 5);
        extractJsonIntArray(dailySec, "precipitation_probability_max", pProb, 5);
        extractJsonFloatArray(dailySec, "uv_index_max", uv, 5);

        for (int i = 0; i < nDates && i < 5; i++) {
            strncpy(_weather.daily[i].date, dates[i], sizeof(_weather.daily[i].date) - 1);
            _weather.daily[i].date[sizeof(_weather.daily[i].date) - 1] = '\0';
            parseDateToDayShort(dates[i], _weather.daily[i].dayShort, sizeof(_weather.daily[i].dayShort));
            _weather.daily[i].weatherCode = codes[i];
            _weather.daily[i].tempMaxC    = tMax[i];
            _weather.daily[i].tempMinC    = tMin[i];
            _weather.daily[i].precipProb  = (uint8_t)(pProb[i] >= 0 ? pProb[i] : 0);
            _weather.daily[i].uvMax       = uv[i];
            _weather.dailyCount++;
        }
    }

    if (g_time.isTimeValid()) {
        g_time.formatTimeHHMM(_weather.updatedTime, sizeof(_weather.updatedTime));
    } else {
        char apiTime[20];
        if (extractJsonString(curSec, "time", apiTime, sizeof(apiTime)) && strlen(apiTime) >= 16) {
            strncpy(_weather.updatedTime, apiTime + 11, 5);
            _weather.updatedTime[5] = '\0';
        } else {
            strcpy(_weather.updatedTime, "LIVE");
        }
    }

    _weather.lastFetchMs = millis();
    _weather.valid       = true;
    _weather.fromCache   = false;

    addRecentLocation(loc);
    saveCachedWeather();
    g_system.display().showToast("WEATHER UPDATED", 800);
    return true;
}

void WeatherApp::selectAndFetchLocation(const WeatherLocation& loc) {
    if (!loc.valid) return;
    _weather.loc   = loc;
    _state         = WSTATE_DASHBOARD;
    _dashboardPage = 0;
    addRecentLocation(loc);
    fetchWeatherForLocation(loc);
}

float WeatherApp::formatTempVal(float celsius) const {
    if (g_settings.get().weatherFahrenheit) {
        return (celsius * 1.8f) + 32.0f;
    }
    return celsius;
}

char WeatherApp::getTempUnitChar() const {
    return g_settings.get().weatherFahrenheit ? 'F' : 'C';
}

const char* WeatherApp::getConditionText(int16_t wmoCode) {
    switch (wmoCode) {
        case 0:  return "Clear Sky";
        case 1:  return "Mainly Clear";
        case 2:  return "Partly Cloudy";
        case 3:  return "Overcast";
        case 45:
        case 48: return "Foggy";
        case 51:
        case 53:
        case 55: return "Drizzle";
        case 56:
        case 57: return "Freezing Drzl";
        case 61: return "Light Rain";
        case 63: return "Moderate Rain";
        case 65: return "Heavy Rain";
        case 66:
        case 67: return "Freezing Rain";
        case 71:
        case 73:
        case 75:
        case 77: return "Snowfall";
        case 80:
        case 81:
        case 82: return "Rain Showers";
        case 85:
        case 86: return "Snow Showers";
        case 95: return "Thunderstorm";
        case 96:
        case 99: return "Storm & Hail";
        default: return "Cloudy";
    }
}

const char* WeatherApp::getConditionShort(int16_t wmoCode) {
    switch (wmoCode) {
        case 0:  return "CLR";
        case 1:  return "SUN";
        case 2:  return "PCL";
        case 3:  return "OVC";
        case 45:
        case 48: return "FOG";
        case 51:
        case 53:
        case 55:
        case 56:
        case 57: return "DRZ";
        case 61:
        case 63:
        case 65:
        case 66:
        case 67: return "RAN";
        case 71:
        case 73:
        case 75:
        case 77:
        case 85:
        case 86: return "SNW";
        case 80:
        case 81:
        case 82: return "SHW";
        case 95:
        case 96:
        case 99: return "STM";
        default: return "CLD";
    }
}

const char* WeatherApp::getCompassDirection(int16_t deg) {
    static const char* const DIRS[] = { "N", "NE", "E", "SE", "S", "SW", "W", "NW" };
    int idx = ((deg % 360 + 360) % 360 + 22) / 45;
    return DIRS[idx % 8];
}

void WeatherApp::drawWeatherIcon(Adafruit_SSD1306& oled, int x, int y, int16_t wmoCode, bool isDay) {
    if (wmoCode == 0 || wmoCode == 1) {
        if (isDay) {
            // Sun icon
            oled.drawCircle(x + 8, y + 8, 4, SSD1306_WHITE);
            oled.drawFastVLine(x + 8, y + 1, 2, SSD1306_WHITE);
            oled.drawFastVLine(x + 8, y + 14, 2, SSD1306_WHITE);
            oled.drawFastHLine(x + 1, y + 8, 2, SSD1306_WHITE);
            oled.drawFastHLine(x + 14, y + 8, 2, SSD1306_WHITE);
            oled.drawPixel(x + 3, y + 3, SSD1306_WHITE);
            oled.drawPixel(x + 13, y + 3, SSD1306_WHITE);
            oled.drawPixel(x + 3, y + 13, SSD1306_WHITE);
            oled.drawPixel(x + 13, y + 13, SSD1306_WHITE);
        } else {
            // Crescent moon + stars
            oled.fillCircle(x + 8, y + 8, 5, SSD1306_WHITE);
            oled.fillCircle(x + 10, y + 6, 4, SSD1306_BLACK);
            oled.drawPixel(x + 2, y + 3, SSD1306_WHITE);
            oled.drawPixel(x + 14, y + 12, SSD1306_WHITE);
        }
        return;
    }

    // Cloud base for cloudy/rain/snow/storm
    oled.drawRoundRect(x + 2, y + 5, 13, 6, 2, SSD1306_WHITE);
    oled.fillCircle(x + 6, y + 5, 3, SSD1306_WHITE);
    oled.fillCircle(x + 10, y + 6, 2, SSD1306_WHITE);

    if (wmoCode >= 51 && wmoCode <= 67 || (wmoCode >= 80 && wmoCode <= 82)) {
        // Rain drops
        oled.drawLine(x + 4, y + 12, x + 3, y + 15, SSD1306_WHITE);
        oled.drawLine(x + 8, y + 12, x + 7, y + 15, SSD1306_WHITE);
        oled.drawLine(x + 12, y + 12, x + 11, y + 15, SSD1306_WHITE);
    } else if (wmoCode >= 71 && wmoCode <= 77 || wmoCode == 85 || wmoCode == 86) {
        // Snowflakes
        oled.drawPixel(x + 4, y + 13, SSD1306_WHITE);
        oled.drawPixel(x + 8, y + 14, SSD1306_WHITE);
        oled.drawPixel(x + 12, y + 13, SSD1306_WHITE);
    } else if (wmoCode >= 95) {
        // Lightning bolt
        oled.drawLine(x + 8, y + 11, x + 6, y + 13, SSD1306_WHITE);
        oled.drawLine(x + 6, y + 13, x + 9, y + 13, SSD1306_WHITE);
        oled.drawLine(x + 9, y + 13, x + 7, y + 15, SSD1306_WHITE);
    } else if (wmoCode == 45 || wmoCode == 48) {
        // Fog lines
        oled.drawFastHLine(x + 2, y + 13, 12, SSD1306_WHITE);
        oled.drawFastHLine(x + 4, y + 15, 9, SSD1306_WHITE);
    }
}

bool WeatherApp::update() {
    if (_state == WSTATE_SEARCH_INPUT) {
        return _textInput.update();
    }

    if (_pendingAutoFetch) {
        _pendingAutoFetch = false;
        if (g_wifi.isConnected() && _weather.loc.valid) {
            fetchWeatherForLocation(_weather.loc);
            return true;
        }
    }

    // Periodic automatic weather refresh if configured in Settings
    uint8_t refMin = g_settings.get().weatherRefreshMin;
    if (refMin > 0 && _state == WSTATE_DASHBOARD && g_wifi.isConnected() && _weather.valid) {
        uint32_t intervalMs = (uint32_t)refMin * 60000UL;
        if ((millis() - _weather.lastFetchMs) >= intervalMs) {
            fetchWeatherForLocation(_weather.loc);
            return true;
        }
    }

    return false;
}

bool WeatherApp::handleInput(const InputEvent& ev) {
    if (_state == WSTATE_SEARCH_INPUT) {
        TextInputResult res = _textInput.handleInput(ev);
        if (res == TEXT_RES_SUBMITTED) {
            if (_searchQuery[0] != '\0') {
                if (performCitySearch(_searchQuery)) {
                    _listIndex  = 0;
                    _listScroll = 0;
                    _state      = WSTATE_SEARCH_RESULTS;
                } else if (g_wifi.isConnected()) {
                    g_system.display().showToast("NO CITY FOUND", 900);
                }
            }
        } else if (res == TEXT_RES_CANCELLED) {
            _state = WSTATE_MENU;
        }
        return false;
    }

    if (ev.type != EVT_PRESS && ev.type != EVT_REPEAT) {
        return false;
    }

    if (_state == WSTATE_SEARCH_RESULTS) {
        if (ev.action == ACT_BACK && ev.type == EVT_PRESS) {
            _state = WSTATE_SEARCH_INPUT;
            return false;
        }
        if (_searchResultCount > 0) {
            if (ev.action == ACT_UP) {
                _listIndex = (_listIndex - 1 + _searchResultCount) % _searchResultCount;
            } else if (ev.action == ACT_DOWN) {
                _listIndex = (_listIndex + 1) % _searchResultCount;
            } else if (ev.action == ACT_SELECT && ev.type == EVT_PRESS) {
                selectAndFetchLocation(_searchResults[_listIndex]);
                return false;
            } else if ((ev.action == ACT_MENU || ev.rawKey == 'D') && ev.type == EVT_PRESS) {
                toggleFavorite(_searchResults[_listIndex]);
            }

            if (_listIndex < _listScroll) {
                _listScroll = _listIndex;
            } else if (_listIndex >= _listScroll + W_VISIBLE_ROWS) {
                _listScroll = _listIndex - W_VISIBLE_ROWS + 1;
            }
        }
        return false;
    }

    if (_state == WSTATE_FAVORITES) {
        if (ev.action == ACT_BACK && ev.type == EVT_PRESS) {
            _state = WSTATE_MENU;
            return false;
        }
        if (_favCount > 0) {
            if (ev.action == ACT_UP) {
                _listIndex = (_listIndex - 1 + _favCount) % _favCount;
            } else if (ev.action == ACT_DOWN) {
                _listIndex = (_listIndex + 1) % _favCount;
            } else if (ev.action == ACT_SELECT && ev.type == EVT_PRESS) {
                selectAndFetchLocation(_favorites[_listIndex]);
                return false;
            } else if ((ev.action == ACT_MENU || ev.rawKey == 'D') && ev.type == EVT_PRESS) {
                removeFavoriteAt(_listIndex);
                if (_listIndex >= _favCount && _favCount > 0) {
                    _listIndex = _favCount - 1;
                }
                g_system.display().showToast("FAV REMOVED", 800);
            }

            if (_listIndex < _listScroll) {
                _listScroll = _listIndex;
            } else if (_listIndex >= _listScroll + W_VISIBLE_ROWS) {
                _listScroll = _listIndex - W_VISIBLE_ROWS + 1;
            }
        }
        return false;
    }

    if (_state == WSTATE_RECENTS) {
        if (ev.action == ACT_BACK && ev.type == EVT_PRESS) {
            _state = WSTATE_MENU;
            return false;
        }
        if (_recentCount > 0) {
            if (ev.action == ACT_UP) {
                _listIndex = (_listIndex - 1 + _recentCount) % _recentCount;
            } else if (ev.action == ACT_DOWN) {
                _listIndex = (_listIndex + 1) % _recentCount;
            } else if (ev.action == ACT_SELECT && ev.type == EVT_PRESS) {
                selectAndFetchLocation(_recents[_listIndex]);
                return false;
            } else if ((ev.action == ACT_MENU || ev.rawKey == 'D') && ev.type == EVT_PRESS) {
                toggleFavorite(_recents[_listIndex]);
            }

            if (_listIndex < _listScroll) {
                _listScroll = _listIndex;
            } else if (_listIndex >= _listScroll + W_VISIBLE_ROWS) {
                _listScroll = _listIndex - W_VISIBLE_ROWS + 1;
            }
        }
        return false;
    }

    if (_state == WSTATE_MENU) {
        if (ev.action == ACT_BACK && ev.type == EVT_PRESS) {
            _state = WSTATE_DASHBOARD;
            return false;
        }
        if (ev.action == ACT_UP) {
            _menuIndex = (_menuIndex - 1 + W_MENU_COUNT) % W_MENU_COUNT;
        } else if (ev.action == ACT_DOWN) {
            _menuIndex = (_menuIndex + 1) % W_MENU_COUNT;
        } else if (ev.action == ACT_SELECT && ev.type == EVT_PRESS) {
            switch (_menuIndex) {
                case 0: // Search City
                    _searchQuery[0] = '\0';
                    _textInput.begin(_searchQuery, sizeof(_searchQuery), "");
                    _state = WSTATE_SEARCH_INPUT;
                    break;
                case 1: // Favorite Cities
                    _listIndex  = 0;
                    _listScroll = 0;
                    _state      = WSTATE_FAVORITES;
                    break;
                case 2: // Recent Locations
                    _listIndex  = 0;
                    _listScroll = 0;
                    _state      = WSTATE_RECENTS;
                    break;
                case 3: // Refresh Weather
                    _state = WSTATE_DASHBOARD;
                    if (_weather.loc.valid) {
                        fetchWeatherForLocation(_weather.loc);
                    }
                    break;
                case 4: // Toggle Favorite
                    if (_weather.loc.valid) {
                        toggleFavorite(_weather.loc);
                    }
                    break;
                case 5: { // Toggle Temp Unit (°C / °F)
                    auto& cfg = g_settings.get();
                    cfg.weatherFahrenheit = !cfg.weatherFahrenheit;
                    g_settings.save();
                    g_system.display().showToast(cfg.weatherFahrenheit ? "UNIT: FAHR (F)" : "UNIT: CELS (C)", 700);
                    break;
                }
                case 6: // Wi-Fi Setup
                    g_system.launchApp(APP_WIFI_SCANNER);
                    break;
            }
        }

        if (_menuIndex < _menuScroll) {
            _menuScroll = _menuIndex;
        } else if (_menuIndex >= _menuScroll + W_VISIBLE_ROWS) {
            _menuScroll = _menuIndex - W_VISIBLE_ROWS + 1;
        }
        return false;
    }

    // WSTATE_DASHBOARD
    if (ev.action == ACT_BACK && ev.type == EVT_PRESS) {
        return true; // Return to Main Menu
    }

    if (ev.action == ACT_LEFT || ev.action == ACT_UP) {
        _dashboardPage = (_dashboardPage - 1 + DASHBOARD_PAGES) % DASHBOARD_PAGES;
    } else if (ev.action == ACT_RIGHT || ev.action == ACT_DOWN) {
        _dashboardPage = (_dashboardPage + 1) % DASHBOARD_PAGES;
    } else if (ev.action == ACT_SELECT && ev.type == EVT_PRESS) {
        _state = WSTATE_MENU;
    } else if ((ev.action == ACT_MENU || ev.rawKey == 'D') && ev.type == EVT_PRESS) {
        if (g_wifi.isConnected() && _weather.loc.valid) {
            fetchWeatherForLocation(_weather.loc);
        } else {
            _state = WSTATE_MENU;
        }
    } else if (ev.rawKey == '1' && ev.type == EVT_PRESS) {
        // Quick shortcut 1: Search City
        _searchQuery[0] = '\0';
        _textInput.begin(_searchQuery, sizeof(_searchQuery), "");
        _state = WSTATE_SEARCH_INPUT;
    } else if (ev.rawKey == '3' && ev.type == EVT_PRESS) {
        // Quick shortcut 3: Toggle Favorite
        if (_weather.loc.valid) {
            toggleFavorite(_weather.loc);
        }
    }

    return false;
}

void WeatherApp::render(DisplayManager& display, InputMode mode, bool showModeBadge) {
    auto& oled = display.oled();
    display.clear();

    if (_state == WSTATE_SEARCH_INPUT) {
        char hdr[24];
        snprintf(hdr, sizeof(hdr), "CITY [%s]", _textInput.getCaseModeLabel());
        display.drawHeader(hdr, mode, showModeBadge);

        oled.setCursor(2, 15);
        oled.print(F("Enter city name:"));
        _textInput.drawInputBox(oled, 2, 26, OLED_WIDTH - 4, 14, false);

        oled.setCursor(2, 43);
        oled.printf("WiFi: %s", g_wifi.isConnected() ? "CONNECTED" : "OFFLINE");
        display.drawFooter("*:Bksp A:Case", "#:Find C:Back");
        return;
    }

    if (_state == WSTATE_SEARCH_RESULTS) {
        char hdr[24];
        snprintf(hdr, sizeof(hdr), "RESULTS (%d)", _searchResultCount);
        display.drawHeader(hdr, mode, showModeBadge);

        if (_searchResultCount == 0) {
            oled.setCursor(16, 28);
            oled.print(F("No cities matched"));
            display.drawFooter("*:Back", nullptr);
            return;
        }

        const int startY = 13;
        const int rowH   = 10;
        for (int i = 0; i < W_VISIBLE_ROWS; i++) {
            int idx = _listScroll + i;
            if (idx >= _searchResultCount) break;

            int y = startY + (i * rowH);
            const auto& loc = _searchResults[idx];
            if (idx == _listIndex) {
                oled.fillRect(0, y, OLED_WIDTH - 5, rowH, SSD1306_WHITE);
                oled.setTextColor(SSD1306_BLACK);
            } else {
                oled.setTextColor(SSD1306_WHITE);
            }

            char line[24];
            if (loc.admin[0] != '\0') {
                snprintf(line, sizeof(line), "%c%.11s,%s %.5s",
                         isFavorite(loc) ? '*' : ' ',
                         loc.name, loc.country, loc.admin);
            } else {
                snprintf(line, sizeof(line), "%c%.15s, %s",
                         isFavorite(loc) ? '*' : ' ',
                         loc.name, loc.country);
            }
            oled.setCursor(2, y + 1);
            oled.print(line);
        }
        oled.setTextColor(SSD1306_WHITE);
        display.drawScrollbar(startY, W_VISIBLE_ROWS * rowH, _searchResultCount, W_VISIBLE_ROWS, _listScroll);
        display.drawFooter("*:Back D:Fav", "5/#:Load");
        return;
    }

    if (_state == WSTATE_FAVORITES) {
        char hdr[24];
        snprintf(hdr, sizeof(hdr), "FAVORITES (%d)", _favCount);
        display.drawHeader(hdr, mode, showModeBadge);

        if (_favCount == 0) {
            oled.setCursor(10, 28);
            oled.print(F("No favorite cities"));
            display.drawFooter("*:Back", nullptr);
            return;
        }

        const int startY = 13;
        const int rowH   = 10;
        for (int i = 0; i < W_VISIBLE_ROWS; i++) {
            int idx = _listScroll + i;
            if (idx >= _favCount) break;

            int y = startY + (i * rowH);
            const auto& loc = _favorites[idx];
            if (idx == _listIndex) {
                oled.fillRect(0, y, OLED_WIDTH - 5, rowH, SSD1306_WHITE);
                oled.setTextColor(SSD1306_BLACK);
            } else {
                oled.setTextColor(SSD1306_WHITE);
            }

            char line[24];
            snprintf(line, sizeof(line), "* %-13.13s %s", loc.name, loc.country);
            oled.setCursor(2, y + 1);
            oled.print(line);
        }
        oled.setTextColor(SSD1306_WHITE);
        display.drawScrollbar(startY, W_VISIBLE_ROWS * rowH, _favCount, W_VISIBLE_ROWS, _listScroll);
        display.drawFooter("*:Back D:Del", "5/#:Load");
        return;
    }

    if (_state == WSTATE_RECENTS) {
        char hdr[24];
        snprintf(hdr, sizeof(hdr), "RECENTS (%d)", _recentCount);
        display.drawHeader(hdr, mode, showModeBadge);

        if (_recentCount == 0) {
            oled.setCursor(12, 28);
            oled.print(F("No recent cities"));
            display.drawFooter("*:Back", nullptr);
            return;
        }

        const int startY = 13;
        const int rowH   = 10;
        for (int i = 0; i < W_VISIBLE_ROWS; i++) {
            int idx = _listScroll + i;
            if (idx >= _recentCount) break;

            int y = startY + (i * rowH);
            const auto& loc = _recents[idx];
            if (idx == _listIndex) {
                oled.fillRect(0, y, OLED_WIDTH - 5, rowH, SSD1306_WHITE);
                oled.setTextColor(SSD1306_BLACK);
            } else {
                oled.setTextColor(SSD1306_WHITE);
            }

            char line[24];
            snprintf(line, sizeof(line), "%c %-13.13s %s",
                     isFavorite(loc) ? '*' : ' ', loc.name, loc.country);
            oled.setCursor(2, y + 1);
            oled.print(line);
        }
        oled.setTextColor(SSD1306_WHITE);
        display.drawScrollbar(startY, W_VISIBLE_ROWS * rowH, _recentCount, W_VISIBLE_ROWS, _listScroll);
        display.drawFooter("*:Back D:Fav", "5/#:Load");
        return;
    }

    if (_state == WSTATE_MENU) {
        display.drawHeader("WEATHER MENU", mode, showModeBadge);

        bool fav = isFavorite(_weather.loc);
        char items[W_MENU_COUNT][24];
        snprintf(items[0], sizeof(items[0]), "1. Search City");
        snprintf(items[1], sizeof(items[1]), "2. Favorites (%d)", _favCount);
        snprintf(items[2], sizeof(items[2]), "3. Recent Cities (%d)", _recentCount);
        snprintf(items[3], sizeof(items[3]), "4. Refresh Weather");
        snprintf(items[4], sizeof(items[4]), "5. %s", fav ? "Remove Favorite" : "Add to Favorites");
        snprintf(items[5], sizeof(items[5]), "6. Unit: %s",
                 g_settings.get().weatherFahrenheit ? "Fahrenheit (F)" : "Celsius (C)");
        snprintf(items[6], sizeof(items[6]), "7. Wi-Fi Setup >");

        const int startY = 13;
        const int rowH   = 10;
        for (int i = 0; i < W_VISIBLE_ROWS; i++) {
            int idx = _menuScroll + i;
            if (idx >= W_MENU_COUNT) break;

            int y = startY + (i * rowH);
            if (idx == _menuIndex) {
                oled.fillRect(0, y, OLED_WIDTH - 5, rowH, SSD1306_WHITE);
                oled.setTextColor(SSD1306_BLACK);
            } else {
                oled.setTextColor(SSD1306_WHITE);
            }
            oled.setCursor(3, y + 1);
            oled.print(items[idx]);
        }
        oled.setTextColor(SSD1306_WHITE);
        display.drawScrollbar(startY, W_VISIBLE_ROWS * rowH, W_MENU_COUNT, W_VISIBLE_ROWS, _menuScroll);
        display.drawFooter("*:Back", "5/#:Select");
        return;
    }

    // WSTATE_DASHBOARD
    char hdr[24];
    if (_weather.loc.valid) {
        snprintf(hdr, sizeof(hdr), "%c%.11s,%s (%d/4)",
                 isFavorite(_weather.loc) ? '*' : ' ',
                 _weather.loc.name,
                 _weather.loc.country,
                 _dashboardPage + 1);
    } else {
        snprintf(hdr, sizeof(hdr), "WEATHER (%d/4)", _dashboardPage + 1);
    }
    display.drawHeader(hdr, mode, showModeBadge);

    if (!_weather.valid) {
        oled.setCursor(6, 16);
        if (_weather.loc.valid) {
            oled.printf("City: %s, %s", _weather.loc.name, _weather.loc.country);
        } else {
            oled.print(F("No city loaded yet"));
        }
        oled.setCursor(6, 28);
        if (g_wifi.isConnected()) {
            oled.print(F("Press D: Fetch Live"));
        } else {
            oled.print(F("Wi-Fi is Disconnected"));
        }
        oled.setCursor(6, 40);
        oled.print(F("Press 5/#: Menu/Find"));
        display.drawFooter("*:Exit D:Sync", "5/#:Menu");
        return;
    }

    char unit = getTempUnitChar();

    if (_dashboardPage == 0) {
        // Page 1: Current Weather Overview + Icon
        drawWeatherIcon(oled, 4, 14, _weather.weatherCode, _weather.isDay);

        char tempStr[12];
        snprintf(tempStr, sizeof(tempStr), "%.1f%c",
                 (double)formatTempVal(_weather.tempC), unit);
        oled.setTextSize(2);
        oled.setCursor(26, 14);
        oled.print(tempStr);
        oled.setTextSize(1);

        oled.setCursor(4, 33);
        oled.printf("%-13.13s %s",
                    getConditionText(_weather.weatherCode),
                    _weather.fromCache ? "[CACH]" : "[LIVE]");

        if (_weather.dailyCount > 0) {
            oled.setCursor(4, 43);
            oled.printf("H:%.0f L:%.0f Hum:%d%%",
                        (double)formatTempVal(_weather.daily[0].tempMaxC),
                        (double)formatTempVal(_weather.daily[0].tempMinC),
                        (int)_weather.humidity);
        } else {
            oled.setCursor(4, 43);
            oled.printf("Hum:%d%% W:%.0fkm/h",
                        (int)_weather.humidity, (double)_weather.windKmh);
        }
    } else if (_dashboardPage == 1) {
        // Page 2: Detailed Weather Telemetry
        oled.setCursor(2, 14);
        oled.printf("Feels: %.1f%c Hum:%d%%",
                    (double)formatTempVal(_weather.feelsLikeC), unit, (int)_weather.humidity);

        oled.setCursor(2, 24);
        oled.printf("Wind: %.1fkm/h (%s)",
                    (double)_weather.windKmh, getCompassDirection(_weather.windDirDeg));

        oled.setCursor(2, 34);
        if (_weather.dailyCount > 0) {
            oled.printf("P:%.0fhPa Rain:%u%%",
                        (double)_weather.pressureHpa, (unsigned)_weather.daily[0].precipProb);
        } else {
            oled.printf("Pres: %.1f hPa", (double)_weather.pressureHpa);
        }

        oled.setCursor(2, 44);
        if (_weather.dailyCount > 0) {
            oled.printf("UV:%.1f Upd:%s",
                        (double)_weather.daily[0].uvMax, _weather.updatedTime);
        } else {
            oled.printf("Lat:%.2f Lon:%.2f",
                        (double)_weather.loc.lat, (double)_weather.loc.lon);
        }
    } else {
        // Pages 3 & 4: 5-Day Forecast (Page 3 shows Days 1-3, Page 4 shows Days 3-5)
        int startDay = (_dashboardPage == 2) ? 0 : 2;
        if (startDay >= (int)_weather.dailyCount) startDay = 0;

        oled.setCursor(2, 13);
        oled.printf("5-DAY FORECAST (%c)", unit);

        for (int row = 0; row < 3; row++) {
            int dIdx = startDay + row;
            if (dIdx >= (int)_weather.dailyCount) break;
            const auto& df = _weather.daily[dIdx];
            int y = 23 + (row * 10);
            oled.setCursor(2, y);
            oled.printf("%-3.3s %-3.3s %3.0f/%-3.0f %2u%%",
                        df.dayShort,
                        getConditionShort(df.weatherCode),
                        (double)formatTempVal(df.tempMinC),
                        (double)formatTempVal(df.tempMaxC),
                        (unsigned)df.precipProb);
        }
    }

    display.drawFooter("4/6:Page D:Sync", "5/#:Menu");
}
