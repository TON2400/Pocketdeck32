#ifndef POCKETDECK_WEATHER_APP_H
#define POCKETDECK_WEATHER_APP_H

#include "AppBase.h"
#include "../core/TextInputManager.h"

struct WeatherLocation {
    char  name[24];
    char  country[6];
    char  admin[18];
    float lat;
    float lon;
    bool  valid;
};

struct DailyForecastItem {
    char    date[12]; // YYYY-MM-DD
    char    dayShort[5];
    int16_t weatherCode;
    float   tempMinC;
    float   tempMaxC;
    uint8_t precipProb;
    float   uvMax;
};

struct WeatherData {
    WeatherLocation   loc;
    float             tempC;
    float             feelsLikeC;
    int16_t           humidity;
    float             windKmh;
    int16_t           windDirDeg;
    float             pressureHpa;
    int16_t           weatherCode;
    bool              isDay;
    DailyForecastItem daily[5];
    uint8_t           dailyCount;
    char              updatedTime[12];
    uint32_t          lastFetchMs;
    bool              valid;
    bool              fromCache;
};

class WeatherApp : public AppBase {
public:
    WeatherApp();

    void onEnter() override;
    bool handleInput(const InputEvent& ev) override;
    bool update() override;
    void render(DisplayManager& display, InputMode mode, bool showModeBadge) override;

private:
    enum WeatherState : uint8_t {
        WSTATE_DASHBOARD = 0,
        WSTATE_MENU,
        WSTATE_SEARCH_INPUT,
        WSTATE_SEARCH_RESULTS,
        WSTATE_FAVORITES,
        WSTATE_RECENTS
    };

    void ensureWeatherStorage();
    void loadFavorites();
    void saveFavorites();
    void loadRecents();
    void saveRecents();
    void loadCachedWeather();
    void saveCachedWeather();

    bool isFavorite(const WeatherLocation& loc) const;
    void toggleFavorite(const WeatherLocation& loc);
    void removeFavoriteAt(int idx);
    void addRecentLocation(const WeatherLocation& loc);

    bool performCitySearch(const char* query);
    bool fetchWeatherForLocation(const WeatherLocation& loc);
    void selectAndFetchLocation(const WeatherLocation& loc);

    float formatTempVal(float celsius) const;
    char  getTempUnitChar() const;
    static const char* getConditionText(int16_t wmoCode);
    static const char* getConditionShort(int16_t wmoCode);
    static const char* getCompassDirection(int16_t deg);
    static void drawWeatherIcon(Adafruit_SSD1306& oled, int x, int y, int16_t wmoCode, bool isDay);
    static void urlEncode(const char* src, char* dst, size_t dstSize);
    static void parseDateToDayShort(const char* isoDate, char* outDay, size_t maxLen);

    static const int MAX_FAVORITES      = 8;
    static const int MAX_RECENTS        = 6;
    static const int MAX_SEARCH_RESULTS = 5;
    static const int SEARCH_BUF_SIZE    = 24;
    static const int DASHBOARD_PAGES    = 4;

    WeatherState     _state;
    int              _dashboardPage;
    int              _menuIndex;
    int              _menuScroll;
    int              _listIndex;
    int              _listScroll;

    WeatherData      _weather;
    WeatherLocation  _favorites[MAX_FAVORITES];
    int              _favCount;
    WeatherLocation  _recents[MAX_RECENTS];
    int              _recentCount;
    WeatherLocation  _searchResults[MAX_SEARCH_RESULTS];
    int              _searchResultCount;

    char             _searchQuery[SEARCH_BUF_SIZE];
    char             _statusMsg[24];
    TextInputManager _textInput;
    bool             _storageLoaded;
    bool             _pendingAutoFetch;
};

#endif // POCKETDECK_WEATHER_APP_H
