#include "../bot.hpp"

#include <Geode/modify/GJBaseGameLayer.hpp>

class $modify(FrameCounterGJBaseGameLayer, GJBaseGameLayer) {
    void processQueuedButtons(float dt, bool clearInputQueue) {
        auto& bot = Bot::get();
        auto* pl = PlayLayer::get();

        bool isPlaying =
            pl &&
            !pl->m_hasCompletedLevel &&
            !pl->m_isPaused &&
            pl->m_player1 &&
            !pl->m_player1->m_isDead &&
            (!pl->m_gameState.m_isDualMode || (pl->m_player2 && !pl->m_player2->m_isDead));

        if (bot.state == state::playing && !bot.tpsEnabled && bot.replay.framerate != 240.f) {
            bot.setTpsEnabled(true);
        }

        GJBaseGameLayer::processQueuedButtons(dt, clearInputQueue);

        if (!isPlaying || bot.updater.isFrozenUpdate())
            return;

        int frame = static_cast<int>(pl->m_gameState.m_levelTime * Bot::getTPS());
        bot.updater.frameCount = std::max(frame + 1, 0);
    }
};
