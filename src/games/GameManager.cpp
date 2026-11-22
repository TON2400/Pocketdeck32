#include "GameManager.h"
#include "Snake.h"
#include "Pong.h"
#include "ReactionGame.h"

static SnakeGame    s_snakeGame;
static PongGame     s_pongGame;
static ReactionGame s_reactionGame;

GameManagerApp::GameManagerApp()
    : _gameCount(0),
      _selectedIndex(0),
      _activeGame(nullptr) {
    registerGame(&s_snakeGame);
    registerGame(&s_pongGame);
    registerGame(&s_reactionGame);
}

void GameManagerApp::registerGame(GameBase* game) {
    if (game && _gameCount < MAX_GAMES) {
        _games[_gameCount++] = game;
    }
}

void GameManagerApp::onEnter() {
    _activeGame    = nullptr;
    _selectedIndex = 0;
}

void GameManagerApp::onExit() {
    _activeGame = nullptr;
}

bool GameManagerApp::update() {
    if (_activeGame) {
        return _activeGame->update();
    }
    return false;
}

bool GameManagerApp::handleInput(const InputEvent& ev) {
    if (_activeGame) {
        bool exitGame = _activeGame->handleInput(ev);
        if (exitGame) {
            _activeGame = nullptr;
        }
        return false;
    }

    if (ev.type != EVT_PRESS && ev.type != EVT_REPEAT) {
        return false;
    }

    if (ev.action == ACT_BACK && ev.type == EVT_PRESS) {
        return true; // Return to Main Menu
    }

    if (_gameCount > 0) {
        if (ev.action == ACT_UP) {
            _selectedIndex = (_selectedIndex - 1 + _gameCount) % _gameCount;
        } else if (ev.action == ACT_DOWN) {
            _selectedIndex = (_selectedIndex + 1) % _gameCount;
        } else if (ev.action == ACT_SELECT && ev.type == EVT_PRESS) {
            _activeGame = _games[_selectedIndex];
            _activeGame->onStart();
        } else if (ev.digit >= 1 && ev.digit <= _gameCount && ev.type == EVT_PRESS) {
            _selectedIndex = ev.digit - 1;
            _activeGame    = _games[_selectedIndex];
            _activeGame->onStart();
        }
    }

    return false;
}

void GameManagerApp::render(DisplayManager& display, InputMode mode, bool showModeBadge) {
    if (_activeGame) {
        _activeGame->render(display, mode, showModeBadge);
        return;
    }

    auto& oled = display.oled();
    display.clear();
    display.drawHeader("GAME CENTER", mode, showModeBadge);

    const int startY = 14;
    const int rowH   = 11;

    for (int i = 0; i < _gameCount; i++) {
        int y = startY + (i * rowH);
        if (i == _selectedIndex) {
            oled.fillRect(0, y, OLED_WIDTH - 2, rowH, SSD1306_WHITE);
            oled.setTextColor(SSD1306_BLACK);
        } else {
            oled.setTextColor(SSD1306_WHITE);
        }
        oled.setCursor(4, y + 2);
        oled.print(_games[i]->getName());
    }

    oled.setTextColor(SSD1306_WHITE);
    display.drawFooter("*:Back", "5/#:Play");
}
