#ifndef POCKETDECK_SNAKE_H
#define POCKETDECK_SNAKE_H

#include "GameManager.h"

class SnakeGame : public GameBase {
public:
    SnakeGame();

    const char* getName() const override { return "1. Snake"; }
    void onStart() override;
    bool handleInput(const InputEvent& ev) override;
    bool update() override;
    void render(DisplayManager& display, InputMode mode, bool showModeBadge) override;

private:
    struct Point {
        int8_t x;
        int8_t y;
    };

    void resetGame();
    void spawnFood();
    void stepSnake();

    static const int GRID_W   = 30;
    static const int GRID_H   = 12;
    static const int CELL_PX  = 4;
    static const int MAX_BODY = 96;

    Point    _body[MAX_BODY];
    int      _length;
    Point    _food;
    int8_t   _dirX;
    int8_t   _dirY;
    int8_t   _nextDirX;
    int8_t   _nextDirY;
    int      _score;
    int      _highScore;
    bool     _gameOver;
    bool     _paused;
    uint32_t _lastStepMs;
};

#endif // POCKETDECK_SNAKE_H
