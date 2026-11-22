#ifndef POCKETDECK_REACTION_GAME_H
#define POCKETDECK_REACTION_GAME_H

#include "GameManager.h"

class ReactionGame : public GameBase {
public:
    ReactionGame();

    const char* getName() const override { return "3. Cyber Reflex"; }
    void onStart() override;
    bool handleInput(const InputEvent& ev) override;
    bool update() override;
    void render(DisplayManager& display, InputMode mode, bool showModeBadge) override;

private:
    enum ReflexState : uint8_t {
        R_IDLE = 0,
        R_WAITING,
        R_TRIGGERED,
        R_EARLY,
        R_RESULT
    };

    void armRound();

    ReflexState _state;
    uint32_t    _triggerAtMs;
    uint32_t    _signalStartMs;
    uint32_t    _lastReactionMs;
    uint32_t    _bestReactionMs;
};

#endif // POCKETDECK_REACTION_GAME_H
