#include "DisplayManager.h"
#include <string.h>

DisplayManager::DisplayManager()
    : _display(OLED_WIDTH, OLED_HEIGHT, &Wire, OLED_RESET_PIN, 400000UL, 400000UL),
      _initialized(false),
      _dirty(true),
      _powerOn(true),
      _toastExpireMs(0),
      _toastActive(false) {
    _toastMsg[0] = '\0';
}

bool DisplayManager::begin() {
    Wire.begin(PIN_OLED_SDA, PIN_OLED_SCL, 400000UL);
    if (!_display.begin(SSD1306_SWITCHCAPVCC, OLED_I2C_ADDR, true, false)) {
        _initialized = false;
        return false;
    }
    _initialized = true;
    _display.clearDisplay();
    _display.setTextColor(SSD1306_WHITE);
    _display.setTextWrap(false);
    _display.display();
    return true;
}

void DisplayManager::clear() {
    if (!_initialized) return;
    _display.clearDisplay();
}

void DisplayManager::present() {
    if (!_initialized) return;
    if (_toastActive) {
        renderToastOverlay();
    }
    _display.display();
    _dirty = false;
}

void DisplayManager::drawBootScreen() {
    if (!_initialized) return;
    _display.clearDisplay();
    _display.drawRect(0, 0, OLED_WIDTH, OLED_HEIGHT, SSD1306_WHITE);
    _display.drawRect(2, 2, OLED_WIDTH - 4, OLED_HEIGHT - 4, SSD1306_WHITE);

    _display.setTextSize(1);
    _display.setCursor(16, 10);
    _display.print(F("// POCKETDECK //"));

    _display.drawFastHLine(14, 21, 100, SSD1306_WHITE);

    _display.setCursor(14, 27);
    _display.print(F("ESP32 MINI CYBERDECK"));

    _display.setCursor(28, 39);
    _display.print(F("FW v" POCKETDECK_VERSION));

    _display.setCursor(18, 50);
    _display.print(F("[KP/IR READY]"));

    _display.display();
    _dirty = false;
}

void DisplayManager::drawHeader(const char* title, InputMode mode, bool showModeIndicator) {
    if (!_initialized) return;

    // Top bar background
    _display.fillRect(0, 0, OLED_WIDTH, 11, SSD1306_WHITE);
    _display.setTextColor(SSD1306_BLACK);
    _display.setTextSize(1);
    _display.setCursor(2, 2);
    _display.print(title);

    if (showModeIndicator) {
        const char* badge = (mode == MODE_KEYPAD) ? "[KP]" : "[IR]";
        int16_t badgeX = OLED_WIDTH - (strlen(badge) * 6) - 2;
        _display.setCursor(badgeX, 2);
        _display.print(badge);
    }

    // Restore default white text on black background
    _display.setTextColor(SSD1306_WHITE);
}

void DisplayManager::drawFooter(const char* leftHint, const char* rightHint) {
    if (!_initialized) return;

    _display.drawFastHLine(0, 54, OLED_WIDTH, SSD1306_WHITE);
    _display.setTextSize(1);
    _display.setTextColor(SSD1306_WHITE);

    if (leftHint) {
        _display.setCursor(1, 56);
        _display.print(leftHint);
    }
    if (rightHint) {
        int16_t rx = OLED_WIDTH - (strlen(rightHint) * 6) - 1;
        if (rx < 0) rx = 0;
        _display.setCursor(rx, 56);
        _display.print(rightHint);
    }
}

void DisplayManager::drawScrollbar(int topY, int height, int totalItems, int visibleItems, int firstVisible) {
    if (!_initialized || totalItems <= visibleItems || visibleItems <= 0) return;

    int trackX = OLED_WIDTH - 3;
    _display.drawRect(trackX, topY, 3, height, SSD1306_WHITE);

    int thumbH = (height * visibleItems) / totalItems;
    if (thumbH < 4) thumbH = 4;
    if (thumbH > height) thumbH = height;

    int maxScroll = totalItems - visibleItems;
    int thumbY = topY;
    if (maxScroll > 0) {
        thumbY = topY + ((height - thumbH) * firstVisible) / maxScroll;
    }
    _display.fillRect(trackX, thumbY, 3, thumbH, SSD1306_WHITE);
}

void DisplayManager::showToast(const char* message, uint32_t durationMs) {
    if (!message) return;
    strncpy(_toastMsg, message, sizeof(_toastMsg) - 1);
    _toastMsg[sizeof(_toastMsg) - 1] = '\0';
    _toastExpireMs = millis() + durationMs;
    _toastActive   = true;
    _dirty         = true;
}

bool DisplayManager::hasActiveToast() const {
    return _toastActive;
}

void DisplayManager::updateToastState() {
    if (_toastActive && (int32_t)(millis() - _toastExpireMs) >= 0) {
        _toastActive = false;
        _dirty       = true;
    }
}

void DisplayManager::renderToastOverlay() {
    if (!_toastActive) return;

    int len = strlen(_toastMsg);
    int boxW = (len * 6) + 12;
    if (boxW > OLED_WIDTH - 4) boxW = OLED_WIDTH - 4;
    if (boxW < 64) boxW = 64;
    int boxH = 18;
    int boxX = (OLED_WIDTH - boxW) / 2;
    int boxY = (OLED_HEIGHT - boxH) / 2;

    _display.fillRect(boxX, boxY, boxW, boxH, SSD1306_BLACK);
    _display.drawRect(boxX, boxY, boxW, boxH, SSD1306_WHITE);
    _display.drawRect(boxX + 1, boxY + 1, boxW - 2, boxH - 2, SSD1306_WHITE);

    int textX = boxX + (boxW - (len * 6)) / 2;
    if (textX < boxX + 3) textX = boxX + 3;
    _display.setTextColor(SSD1306_WHITE);
    _display.setTextSize(1);
    _display.setCursor(textX, boxY + 5);
    _display.print(_toastMsg);
}

void DisplayManager::setContrast(uint8_t contrast) {
    if (!_initialized) return;
    _display.ssd1306_command(SSD1306_SETCONTRAST);
    _display.ssd1306_command(contrast);
}

void DisplayManager::setInverted(bool inverted) {
    if (!_initialized) return;
    _display.invertDisplay(inverted);
}

void DisplayManager::setPower(bool on) {
    if (!_initialized || _powerOn == on) return;
    _powerOn = on;
    _display.ssd1306_command(on ? SSD1306_DISPLAYON : SSD1306_DISPLAYOFF);
    if (on) {
        _dirty = true;
    }
}

void DisplayManager::togglePower() {
    setPower(!_powerOn);
}
