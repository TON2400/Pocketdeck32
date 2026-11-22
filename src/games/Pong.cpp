#include "Pong.h"
#include "../core/SystemManager.h"
#include <stdio.h>
#include <math.h>

PongGame::PongGame()
    : _playerY(30),
      _cpuY(30),
      _ballX(64),
      _ballY(36),
      _ballVX(2.0f),
      _ballVY(1.2f),
      _playerScore(0),
      _cpuScore(0),
      _gameOver(false),
      _paused(false),
      _lastTickMs(0) {}

void PongGame::onStart() {
    resetMatch();
}

void PongGame::resetMatch() {
    _playerScore = 0;
    _cpuScore    = 0;
    _playerY     = 30.0f;
    _cpuY        = 30.0f;
    _gameOver    = false;
    _paused      = false;
    _lastTickMs  = millis();
    resetBall(true);
}

void PongGame::resetBall(bool towardsCpu) {
    _ballX  = OLED_WIDTH / 2.0f;
    _ballY  = (ARENA_TOP + ARENA_BOTTOM) / 2.0f;
    _ballVX = towardsCpu ? 2.1f : -2.1f;
    _ballVY = (random(0, 2) == 0) ? 1.1f : -1.1f;
}

bool PongGame::handleInput(const InputEvent& ev) {
    if (ev.type != EVT_PRESS && ev.type != EVT_REPEAT) {
        return false;
    }

    if (ev.action == ACT_BACK && ev.type == EVT_PRESS) {
        return true; // Return to Game Center
    }

    if (_gameOver) {
        if (ev.action == ACT_SELECT && ev.type == EVT_PRESS) {
            resetMatch();
        }
        return false;
    }

    if (ev.action == ACT_SELECT && ev.type == EVT_PRESS) {
        _paused = !_paused;
        return false;
    }

    // Discrete step support on tap (in addition to continuous hold in update())
    if (ev.action == ACT_UP) {
        _playerY -= 4.0f;
    } else if (ev.action == ACT_DOWN) {
        _playerY += 4.0f;
    }

    if (_playerY < ARENA_TOP + 1) _playerY = ARENA_TOP + 1;
    if (_playerY > ARENA_BOTTOM - PADDLE_H - 1) _playerY = ARENA_BOTTOM - PADDLE_H - 1;

    return false;
}

bool PongGame::update() {
    if (_gameOver || _paused) return false;

    uint32_t now = millis();
    if ((now - _lastTickMs) < 33) {
        return false;
    }
    _lastTickMs = now;

    // Continuous paddle motion while UP/DOWN is held
    if (g_system.input().isActionHeld(ACT_UP)) {
        _playerY -= 2.2f;
    } else if (g_system.input().isActionHeld(ACT_DOWN)) {
        _playerY += 2.2f;
    }

    if (_playerY < ARENA_TOP + 1) _playerY = ARENA_TOP + 1;
    if (_playerY > ARENA_BOTTOM - PADDLE_H - 1) _playerY = ARENA_BOTTOM - PADDLE_H - 1;

    // Update ball
    _ballX += _ballVX;
    _ballY += _ballVY;

    // Top / bottom wall bounce
    if (_ballY <= ARENA_TOP + 2) {
        _ballY  = ARENA_TOP + 2;
        _ballVY = -_ballVY;
    } else if (_ballY >= ARENA_BOTTOM - 3) {
        _ballY  = ARENA_BOTTOM - 3;
        _ballVY = -_ballVY;
    }

    // Player paddle collision (x = 4..7)
    if (_ballVX < 0 && _ballX <= 8.0f && _ballX >= 3.0f) {
        if (_ballY >= _playerY - 2 && _ballY <= _playerY + PADDLE_H + 2) {
            _ballX  = 8.0f;
            _ballVX = -_ballVX * 1.04f;
            if (_ballVX > 3.6f) _ballVX = 3.6f;
            float hitOffset = (_ballY - (_playerY + PADDLE_H / 2.0f)) / (PADDLE_H / 2.0f);
            _ballVY = hitOffset * 2.0f;
        }
    }

    // CPU paddle collision (x = 120..123)
    if (_ballVX > 0 && _ballX >= (OLED_WIDTH - 9.0f) && _ballX <= (OLED_WIDTH - 3.0f)) {
        if (_ballY >= _cpuY - 2 && _ballY <= _cpuY + PADDLE_H + 2) {
            _ballX  = OLED_WIDTH - 9.0f;
            _ballVX = -_ballVX * 1.04f;
            if (_ballVX < -3.6f) _ballVX = -3.6f;
            float hitOffset = (_ballY - (_cpuY + PADDLE_H / 2.0f)) / (PADDLE_H / 2.0f);
            _ballVY = hitOffset * 2.0f;
        }
    }

    // Simple AI tracking
    float cpuCenter = _cpuY + (PADDLE_H / 2.0f);
    if (_ballVX > 0) {
        if (cpuCenter < _ballY - 2.0f) _cpuY += 1.55f;
        else if (cpuCenter > _ballY + 2.0f) _cpuY -= 1.55f;
    }
    if (_cpuY < ARENA_TOP + 1) _cpuY = ARENA_TOP + 1;
    if (_cpuY > ARENA_BOTTOM - PADDLE_H - 1) _cpuY = ARENA_BOTTOM - PADDLE_H - 1;

    // Scoring
    if (_ballX < 0) {
        _cpuScore++;
        if (_cpuScore >= WIN_SCORE) {
            _gameOver = true;
        } else {
            resetBall(false);
        }
    } else if (_ballX > OLED_WIDTH) {
        _playerScore++;
        if (_playerScore >= WIN_SCORE) {
            _gameOver = true;
        } else {
            resetBall(true);
        }
    }

    return true;
}

void PongGame::render(DisplayManager& display, InputMode mode, bool showModeBadge) {
    auto& oled = display.oled();
    display.clear();

    char hdr[24];
    snprintf(hdr, sizeof(hdr), "PONG P1:%d CPU:%d", _playerScore, _cpuScore);
    display.drawHeader(hdr, mode, showModeBadge);

    // Arena border
    oled.drawRect(0, ARENA_TOP, OLED_WIDTH, ARENA_BOTTOM - ARENA_TOP + 1, SSD1306_WHITE);

    // Center dashed net
    for (int y = ARENA_TOP + 2; y < ARENA_BOTTOM - 2; y += 5) {
        oled.drawFastVLine(OLED_WIDTH / 2, y, 3, SSD1306_WHITE);
    }

    // Player and CPU paddles
    oled.fillRect(4, (int)_playerY, PADDLE_W, PADDLE_H, SSD1306_WHITE);
    oled.fillRect(OLED_WIDTH - 4 - PADDLE_W, (int)_cpuY, PADDLE_W, PADDLE_H, SSD1306_WHITE);

    // Ball
    oled.fillRect((int)_ballX - 1, (int)_ballY - 1, 3, 3, SSD1306_WHITE);

    if (_paused && !_gameOver) {
        oled.fillRect(34, 28, 60, 15, SSD1306_BLACK);
        oled.drawRect(34, 28, 60, 15, SSD1306_WHITE);
        oled.setCursor(46, 32);
        oled.print(F("PAUSED"));
    }

    if (_gameOver) {
        oled.fillRect(16, 22, 96, 24, SSD1306_BLACK);
        oled.drawRect(16, 22, 96, 24, SSD1306_WHITE);
        oled.setCursor(28, 25);
        oled.print(_playerScore >= WIN_SCORE ? F("PLAYER WINS!") : F("CPU WINS!"));
        oled.setCursor(22, 35);
        oled.print(F("5/#:Play *:Exit"));
    }
}
