#ifndef POCKETDECK_GAME_MANAGER_H
#define POCKETDECK_GAME_MANAGER_H

#include "../apps/AppBase.h"

class GameBase {
public:
    virtual ~GameBase() {}

    virtual const char* getName() const = 0;
    virtual void onStart() = 0;
    // Return true to return to the Game Center selection list
    virtual bool handleInput(const InputEvent& ev) = 0;
    // Periodic non-blocking game loop tick. Return true if frame needs redraw.
    virtual bool update() = 0;
    virtual void render(DisplayManager& display, InputMode mode, bool showModeBadge) = 0;
};

class GameManagerApp : public AppBase {
public:
    GameManagerApp();

    void onEnter() override;
    void onExit() override;
    bool handleInput(const InputEvent& ev) override;
    bool update() override;
    void render(DisplayManager& display, InputMode mode, bool showModeBadge) override;

    void registerGame(GameBase* game);

private:
    static const int MAX_GAMES = 6;

    GameBase* _games[MAX_GAMES];
    int       _gameCount;
    int       _selectedIndex;
    GameBase* _activeGame;
};

#endif // POCKETDECK_GAME_MANAGER_H
