#include "Snake.h"
#include <stdio.h>

SnakeGame::SnakeGame()
    : _length(3),
      _dirX(1),
      _dirY(0),
      _nextDirX(1),
      _nextDirY(0),
      _score(0),
      _highScore(0),
      _gameOver(false),
      _paused(false),
      _lastStepMs(0) {}

void SnakeGame::onStart() {
    resetGame();
}

void SnakeGame::resetGame() {
    _length   = 3;
    int startX = GRID_W / 2;
    int startY = GRID_H / 2;
    for (int i = 0; i < _length; i++) {
        _body[i].x = (int8_t)(startX - i);
        _body[i].y = (int8_t)startY;
    }
    _dirX       = 1;
    _dirY       = 0;
    _nextDirX   = 1;
    _nextDirY   = 0;
    _score      = 0;
    _gameOver   = false;
    _paused     = false;
    _lastStepMs = millis();
    spawnFood();
}

void SnakeGame::spawnFood() {
    for (int attempt = 0; attempt < 60; attempt++) {
        int8_t fx = (int8_t)random(0, GRID_W);
        int8_t fy = (int8_t)random(0, GRID_H);
        bool collision = false;
        for (int i = 0; i < _length; i++) {
            if (_body[i].x == fx && _body[i].y == fy) {
                collision = true;
                break;
            }
        }
        if (!collision) {
            _food.x = fx;
            _food.y = fy;
            return;
        }
    }
    _food.x = 1;
    _food.y = 1;
}

void SnakeGame::stepSnake() {
    _dirX = _nextDirX;
    _dirY = _nextDirY;

    int8_t nx = _body[0].x + _dirX;
    int8_t ny = _body[0].y + _dirY;

    // Wall collision check
    if (nx < 0 || nx >= GRID_W || ny < 0 || ny >= GRID_H) {
        _gameOver = true;
        if (_score > _highScore) _highScore = _score;
        return;
    }

    // Self collision check
    for (int i = 0; i < _length; i++) {
        if (_body[i].x == nx && _body[i].y == ny) {
            _gameOver = true;
            if (_score > _highScore) _highScore = _score;
            return;
        }
    }

    bool ate = (nx == _food.x && ny == _food.y);
    if (ate && _length < MAX_BODY) {
        _length++;
        _score += 10;
        if (_score > _highScore) _highScore = _score;
    }

    for (int i = _length - 1; i > 0; i--) {
        _body[i] = _body[i - 1];
    }
    _body[0].x = nx;
    _body[0].y = ny;

    if (ate) {
        spawnFood();
    }
}

bool SnakeGame::handleInput(const InputEvent& ev) {
    if (ev.type != EVT_PRESS && ev.type != EVT_REPEAT) {
        return false;
    }

    if (ev.action == ACT_BACK && ev.type == EVT_PRESS) {
        return true; // Return to Game Center
    }

    if (_gameOver) {
        if (ev.action == ACT_SELECT && ev.type == EVT_PRESS) {
            resetGame();
        }
        return false;
    }

    if ((ev.action == ACT_MENU || ev.action == ACT_SELECT) && ev.type == EVT_PRESS) {
        _paused = !_paused;
        return false;
    }

    if (ev.action == ACT_UP && _dirY == 0) {
        _nextDirX = 0;
        _nextDirY = -1;
    } else if (ev.action == ACT_DOWN && _dirY == 0) {
        _nextDirX = 0;
        _nextDirY = 1;
    } else if (ev.action == ACT_LEFT && _dirX == 0) {
        _nextDirX = -1;
        _nextDirY = 0;
    } else if (ev.action == ACT_RIGHT && _dirX == 0) {
        _nextDirX = 1;
        _nextDirY = 0;
    }

    return false;
}

bool SnakeGame::update() {
    if (_gameOver || _paused) return false;

    uint32_t interval = 145;
    if (_score >= 50)  interval = 120;
    if (_score >= 100) interval = 95;

    uint32_t now = millis();
    if ((now - _lastStepMs) >= interval) {
        _lastStepMs = now;
        stepSnake();
        return true;
    }
    return false;
}

void SnakeGame::render(DisplayManager& display, InputMode mode, bool showModeBadge) {
    auto& oled = display.oled();
    display.clear();

    char hdr[24];
    snprintf(hdr, sizeof(hdr), "SNAKE S:%d H:%d", _score, _highScore);
    display.drawHeader(hdr, mode, showModeBadge);

    const int boardX = 3;
    const int boardY = 13;
    const int boardW = GRID_W * CELL_PX;
    const int boardH = GRID_H * CELL_PX;

    oled.drawRect(boardX - 1, boardY - 1, boardW + 2, boardH + 2, SSD1306_WHITE);

    // Draw food
    oled.drawRect(boardX + (_food.x * CELL_PX), boardY + (_food.y * CELL_PX),
                  CELL_PX - 1, CELL_PX - 1, SSD1306_WHITE);

    // Draw snake segments
    for (int i = 0; i < _length; i++) {
        oled.fillRect(boardX + (_body[i].x * CELL_PX), boardY + (_body[i].y * CELL_PX),
                      CELL_PX - 1, CELL_PX - 1, SSD1306_WHITE);
    }

    if (_paused && !_gameOver) {
        oled.fillRect(30, 26, 68, 16, SSD1306_BLACK);
        oled.drawRect(30, 26, 68, 16, SSD1306_WHITE);
        oled.setCursor(44, 30);
        oled.print(F("PAUSED"));
    }

    if (_gameOver) {
        oled.fillRect(16, 22, 96, 24, SSD1306_BLACK);
        oled.drawRect(16, 22, 96, 24, SSD1306_WHITE);
        oled.setCursor(34, 25);
        oled.print(F("GAME OVER"));
        oled.setCursor(22, 35);
        oled.print(F("5/#:Play *:Exit"));
    }
}
