#ifndef POCKETDECK_DISPLAY_MANAGER_H
#define POCKETDECK_DISPLAY_MANAGER_H

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "../utils/Constants.h"
#include "InputManager.h"

class DisplayManager {
public:
    DisplayManager();

    bool begin();

    Adafruit_SSD1306& oled() { return _display; }

    void clear();
    void present();

    void markDirty() { _dirty = true; }
    bool isDirty() const { return _dirty; }

    // Common UI widgets
    void drawBootScreen();
    void drawHeader(const char* title, InputMode mode, bool showModeIndicator = true);
    void drawFooter(const char* leftHint, const char* rightHint = nullptr);
    void drawScrollbar(int topY, int height, int totalItems, int visibleItems, int firstVisible);

    // Toast overlay system
    void showToast(const char* message, uint32_t durationMs = 900);
    bool hasActiveToast() const;
    void updateToastState();
    void renderToastOverlay();

    // Hardware display controls
    void setContrast(uint8_t contrast);
    void setInverted(bool inverted);
    void setPower(bool on);
    bool isPoweredOn() const { return _powerOn; }

private:
    Adafruit_SSD1306 _display;
    bool             _initialized;
    bool             _dirty;
    bool             _powerOn;
    char             _toastMsg[28];
    uint32_t         _toastExpireMs;
    bool             _toastActive;
};

#endif // POCKETDECK_DISPLAY_MANAGER_H
