#ifndef POCKETDECK_PONG_H
#define POCKETDECK_PONG_H

#include "GameManager.h"

class PongGame : public GameBase {
public:
    PongGame();

    const char* getName() const override { return "2. Pong"; }
    void onStart() override;
    bool handleInput(const InputEvent& ev) override;
    bool update() override;
    void render(DisplayManager& display, InputMode mode, bool showModeBadge) override;

private:
    void resetMatch();
    void resetBall(bool towardsCpu);

    static const int ARENA_TOP    = 13;
    static const int ARENA_BOTTOM = 62;
    static const int PADDLE_H     = 14;
    static const int PADDLE_W     = 3;
    static const int WIN_SCORE    = 5;

    float    _playerY;
    float    _cpuY;
    float    _ballX;
    float    _ballY;
    float    _ballVX;
    float    _ballVY;
    int      _playerScore;
    int      _cpuScore;
    bool     _gameOver;
    bool     _paused;
    uint32_t _lastTickMs;
};

#endif // POCKETDECK_PONG_H
