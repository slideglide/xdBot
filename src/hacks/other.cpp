#include "../core/bot.hpp"
#include "../practice_fixes/practice_fixes.hpp"
#include "../ui/layers/record_layer.hpp"

#include <array>
#include <limits>

#include <Geode/modify/CCScheduler.hpp>
#include <Geode/modify/EffectGameObject.hpp>
#include <Geode/modify/EndLevelLayer.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>
#include <Geode/modify/GJGameLevel.hpp>
#include <Geode/modify/GameLevelOptionsLayer.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/PlayerObject.hpp>

namespace {
constexpr std::array<float, 19> safeValues = {
    1.0f / 240,
    1.0f / 120,
    1.0f / 80,
    1.0f / 60,
    1.0f / 48,
    1.0f / 40,
    1.0f / 30,
    1.0f / 24,
    1.0f / 20,
    1.0f / 16,
    1.0f / 15,
    1.0f / 12,
    1.0f / 10,
    1.0f / 8,
    1.0f / 6,
    1.0f / 5,
    1.0f / 4,
    1.0f / 3,
    1.0f / 2,
};

bool useFastLockDelta(Bot const& bot) {
#ifdef GEODE_IS_IOS
    constexpr bool recording = false;
#else
    bool recording = bot.renderer.recording;
#endif
    return bot.lockDelta && bot.lockDeltaFast && bot.state == state::playing && !recording;
}

template <class Callback>
void runFastLockDeltaUpdates(Bot& bot, int steps, double physicsDt, Callback&& runUpdate) {
    while (steps > 0) {
        int currentFrame = Bot::getCurrentFrame();
        int safeSteps = steps;

        if (bot.currentAction < bot.replay.inputs.size()) {
            auto nextInputFrame = bot.replay.inputs[bot.currentAction].frame;

            if (nextInputFrame > static_cast<uint64_t>(currentFrame)) {
                auto frameGap = nextInputFrame - static_cast<uint64_t>(currentFrame);
                safeSteps = std::min<int>(
                    steps,
                    frameGap > static_cast<uint64_t>(std::numeric_limits<int>::max())
                        ? std::numeric_limits<int>::max()
                        : static_cast<int>(frameGap)
                );
            } else {
                safeSteps = 0;
            }
        }

        if (safeSteps > 0) {
            runUpdate(safeSteps, physicsDt * static_cast<double>(safeSteps));
            steps -= safeSteps;
            continue;
        }

        runUpdate(1, physicsDt);
        steps--;
    }
}
}

const std::unordered_set<int> shaderIDs = {2904,2905,2907,2909,2910,2911,2912,2913,2914,2915,2916,2917,2919,2920,2921,2922,2923,2924};

class $modify(CCScheduler) {

    void update(float dt) {
        auto& bot = Bot::get();

        if (bot.schedulerUpdating)
            return CCScheduler::update(dt);

        if (bot.state == state::playing && !bot.tpsEnabled && bot.replay.framerate != 240.f)
            bot.setTpsEnabled(true);

        if (bot.state == state::none && !bot.speedhackEnabled && !bot.tpsEnabled && !bot.lockDelta &&
            !bot.frameStepper) {
            if (bot.currentPitch != 1.f)
                Bot::updatePitch(1.f);

            return CCScheduler::update(dt);
        }
#ifndef GEODE_IS_IOS
        if (bot.renderer.recording) {
            if (bot.currentPitch != 1.f)
                Bot::updatePitch(1.f);

            return CCScheduler::update(dt);
        }
#endif

        float speedhack = 1.f;

        if (bot.speedhackEnabled && !bot.frameStepper) {
            std::string speedhackValue = bot.mod->getSavedValue<std::string>("macro_speedhack");

            if (speedhackValue != "0.0" && speedhackValue != "") {
                speedhack = geode::utils::numFromString<float>(speedhackValue).unwrap();
                float decimals = speedhack - static_cast<int>(speedhack);

                float closest = safeValues[0];
                float minDiff = std::abs(decimals - closest);

                for (float value : safeValues) {
                    if (speedhackValue == "1.0" || bot.state == state::none) {
                        closest = decimals;
                        break;
                    }

                    float diff = std::abs(decimals - value);

                    if (diff < minDiff) {
                        minDiff = diff;
                        closest = value;
                    }
                }

                speedhack = static_cast<int>(speedhack) + closest;
            }

            Bot::updatePitch(speedhack);
        }

        if (speedhack != 1.f && PlayLayer::get())
            bot.safeMode = true;

        auto* pl = PlayLayer::get();
        if (!pl || pl->m_isPaused) {
            bot.schedulerOverflow = 0.0;
            return CCScheduler::update(dt * speedhack);
        }

        double physicsDt = 1.0 / static_cast<double>(Bot::getTPS());
        double timeWarp = std::min(static_cast<double>(pl->m_gameState.m_timeWarp), 1.0);
        double timestep = physicsDt * timeWarp;
        if (timestep <= 0.0)
            timestep = physicsDt;

        if (bot.frameStepper) {
            if (pl->m_player1 && pl->m_player1->m_isDead) {
                if (bot.mod->getSavedValue<bool>("macro_instant_respawn"))
                    pl->resetLevel();
                return;
            }

            bot.safeMode = true;
            if (!bot.stepFrame) {
                bot.schedulerUpdating = true;
                bot.schedulerFrozenUpdate = true;
                bot.schedulerStepCount = 1;
                CCScheduler::update(0.0f);
                bot.schedulerStepCount = 1;
                bot.schedulerFrozenUpdate = false;
                bot.schedulerUpdating = false;
                return;
            }

            bot.stepFrame = false;
            bot.schedulerOverflow = 0.0;
            bot.schedulerUpdating = true;
            bot.schedulerStepCount = 1;
            CCScheduler::update(static_cast<float>(timestep));
            bot.schedulerStepCount = 1;
            bot.schedulerUpdating = false;
            PracticeFix::saveFrameStepperFrame();
            Bot::syncFrameStepperMusic();
            return;
        }

        if (!bot.lockDelta) {
            bot.schedulerOverflow = 0.0;
            return CCScheduler::update(dt * speedhack);
        }

        bot.schedulerOverflow += static_cast<double>(dt) * timeWarp * speedhack;
        int steps = static_cast<int>(std::floor(bot.schedulerOverflow / timestep));

        if (steps <= 0)
            return;

        bot.schedulerOverflow -= static_cast<double>(steps) * timestep;
        bot.schedulerUpdating = true;

        auto runUpdate = [&](int stepCount, double delta) {
            if (stepCount <= 0)
                return;

            bot.schedulerStepCount = stepCount;
            CCScheduler::update(static_cast<float>(delta));
            bot.schedulerStepCount = 1;
        };

        if (useFastLockDelta(bot)) {
            runFastLockDeltaUpdates(bot, steps, physicsDt, runUpdate);
        } else {
            for (int i = 0; i < steps; i++)
                runUpdate(1, physicsDt);
        }

        bot.schedulerUpdating = false;
    }

};

class $modify(PlayerObject) {

    void playDeathEffect() {
        if (!Bot::get()
                 .mod
                 ->getSavedValue<bool>(
                     "macro_no_death_effect"
                 )) {
            PlayerObject::playDeathEffect();
        }
    }

    void playSpawnEffect() {
        if (!Bot::get()
                 .mod
                 ->getSavedValue<bool>(
                     "macro_no_respawn_flash"
                 )) {
            PlayerObject::playSpawnEffect();
        }
    }
};

class $modify(PlayLayer) {

    struct Fields {
        CCObject* slopeFix = nullptr;
    };

    void postUpdate(float dt) {
        PlayLayer::postUpdate(dt);

        auto& bot = Bot::get();

        if (!bot.autosaveEnabled)
            return;

        if (!bot.autosaveIntervalEnabled)
            return;

        if (bot.autosaveCheck <
            bot.autosaveInterval) {

            bot.autosaveCheck += dt;
            return;
        }

        bot.autosaveCheck = 0.f;

        int currentTime =
            asp::time::SystemTime::now()
                .timeSinceEpoch()
                .millis();

        Bot::autoSave(
            m_level,
            currentTime
        );
    }

    void destroyPlayer(
        PlayerObject* player,
        GameObject* object
    ) {
        if (player != m_player1 &&
            player != m_player2) {

            return PlayLayer::destroyPlayer(
                player,
                object
            );
        }

        if (!m_fields->slopeFix)
            m_fields->slopeFix = object;

        auto& bot = Bot::get();

        bool player2 =
            player == m_player2;

        bool allowDeath =
            (!bot.mod->getSavedValue<bool>(
                 "macro_noclip"
             )) ||
            (!bot.mod->getSavedValue<bool>(
                 player2
                     ? "macro_noclip_p2"
                     : "macro_noclip_p1"
             )) ||
            (m_fields->slopeFix == object);

        if (allowDeath) {
            PlayLayer::destroyPlayer(
                player,
                object
            );
        } else {
            bot.safeMode = true;
        }

        if (getActionByTag(16) &&
            bot.mod->getSavedValue<bool>(
                "respawn_time_enabled"
            )) {

            stopActionByTag(16);

            auto* seq =
                CCSequence::create(
                    CCDelayTime::create(
                        bot.mod->getSavedValue<double>(
                            "respawn_time"
                        )
                    ),
                    CCCallFunc::create(
                        this,
                        callfunc_selector(
                            PlayLayer::delayedResetLevel
                        )
                    ),
                    nullptr
                );

            seq->setTag(16);

            runAction(seq);
        }
    }

    void showNewBest(
        bool p0,
        int p1,
        int p2,
        bool p3,
        bool p4,
        bool p5
    ) {
        auto& bot = Bot::get();

        if (!bot.safeMode ||
            !Mod::get()->getSavedValue<bool>(
                "macro_auto_safe_mode"
            )) {

            PlayLayer::showNewBest(
                p0,
                p1,
                p2,
                p3,
                p4,
                p5
            );
        }
    }

    void levelComplete() {
        auto& bot = Bot::get();

        bot.firstAttempt = true;

        if (bot.state == state::recording &&
            bot.autosaveEnabled &&
            bot.mod->getSavedValue<bool>(
                "autosave_levelend_enabled"
            )) {

            Bot::autoSave(
                nullptr,
                bot.currentSession
            );
        }

        bool wasTestMode =
            m_isTestMode;

        if (bot.safeMode &&
            bot.mod->getSavedValue<bool>(
                "macro_auto_safe_mode"
            )) {

            m_isTestMode = true;
        }

        if (m_isPracticeMode)
            bot.safeMode = false;

        PlayLayer::levelComplete();

        Bot::resetState(true);

        m_isTestMode = wasTestMode;
    }
};
