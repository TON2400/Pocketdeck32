#include "ReactionGame.h"
#include <stdio.h>

ReactionGame::ReactionGame()
    : _state(R_IDLE),
      _triggerAtMs(0),
      _signalStartMs(0),
      _lastReactionMs(0),
      _bestReactionMs(0) {}

void ReactionGame::onStart() {
    _state          = R_IDLE;
    _lastReactionMs = 0;
}

void ReactionGame::armRound() {
    _state       = R_WAITING;
    _triggerAtMs = millis() + (uint32_t)random(1200, 3400);
}

bool ReactionGame::update() {
    if (_state == R_WAITING) {
        if ((int32_t)(millis() - _triggerAtMs) >= 0) {
            _state         = R_TRIGGERED;
            _signalStartMs = millis();
            return true;
        }
    }
    return false;
}

bool ReactionGame::handleInput(const InputEvent& ev) {
    if (ev.type != EVT_PRESS) {
        return false;
    }

    if (ev.action == ACT_BACK) {
        return true; // Return to Game Center
    }

    if (ev.action == ACT_SELECT || ev.digit >= 0) {
        if (_state == R_IDLE || _state == R_RESULT || _state == R_EARLY) {
            armRound();
        } else if (_state == R_WAITING) {
            // Pressed before signal appeared
            _state = R_EARLY;
        } else if (_state == R_TRIGGERED) {
            _lastReactionMs = millis() - _signalStartMs;
            if (_bestReactionMs == 0 || _lastReactionMs < _bestReactionMs) {
                _bestReactionMs = _lastReactionMs;
            }
            _state = R_RESULT;
        }
    }

    return false;
}

void ReactionGame::render(DisplayManager& display, InputMode mode, bool showModeBadge) {
    auto& oled = display.oled();
    display.clear();
    display.drawHeader("CYBER REFLEX", mode, showModeBadge);

    if (_state == R_IDLE) {
        oled.setCursor(10, 18);
        oled.print(F("Test your reaction"));
        oled.setCursor(10, 29);
        oled.print(F("speed in ms!"));
        if (_bestReactionMs > 0) {
            oled.setCursor(10, 41);
            oled.printf("Best: %lu ms", (unsigned long)_bestReactionMs);
        }
        display.drawFooter("*:Exit", "5/#:Start");
    } else if (_state == R_WAITING) {
        oled.drawRect(10, 18, OLED_WIDTH - 20, 30, SSD1306_WHITE);
        oled.setCursor(22, 29);
        oled.print(F("WAIT FOR IT..."));
        display.drawFooter("*:Exit", "Hold steady");
    } else if (_state == R_TRIGGERED) {
        oled.fillRect(8, 16, OLED_WIDTH - 16, 34, SSD1306_WHITE);
        oled.setTextColor(SSD1306_BLACK);
        oled.setCursor(26, 29);
        oled.print(F("PRESS 5/# NOW!"));
        oled.setTextColor(SSD1306_WHITE);
        display.drawFooter("*:Exit", "5/#:HIT!");
    } else if (_state == R_EARLY) {
        oled.setCursor(22, 22);
        oled.print(F("FALSE START!"));
        oled.setCursor(16, 34);
        oled.print(F("Pressed too soon"));
        display.drawFooter("*:Exit", "5/#:Retry");
    } else if (_state == R_RESULT) {
        oled.setCursor(12, 16);
        oled.printf("Time: %lu ms", (unsigned long)_lastReactionMs);

        oled.setCursor(12, 28);
        oled.printf("Best: %lu ms", (unsigned long)_bestReactionMs);

        oled.setCursor(12, 40);
        if (_lastReactionMs < 200) {
            oled.print(F("Rank: CYBERNETIC!"));
        } else if (_lastReactionMs < 280) {
            oled.print(F("Rank: SHARP"));
        } else {
            oled.print(F("Rank: NOMINAL"));
        }
        display.drawFooter("*:Exit", "5/#:Again");
    }
}
