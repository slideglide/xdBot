#include "../core/bot.hpp"
#include "../practice_fixes/practice_fixes.hpp"
#include "../ui/layers/record_layer.hpp"

#include <unordered_set>

#include <Geode/modify/EffectGameObject.hpp>
#include <Geode/modify/EndLevelLayer.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>
#include <Geode/modify/GJGameLevel.hpp>
#include <Geode/modify/GameLevelOptionsLayer.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/PlayerObject.hpp>

const std::unordered_set<int> shaderIDs = {2904,2905,2907,2909,2910,2911,2912,2913,2914,2915,2916,2917,2919,2920,2921,2922,2923,2924};

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
