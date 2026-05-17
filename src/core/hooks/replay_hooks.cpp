#include "../bot.hpp"

#include "../../practice_fixes/practice_fixes.hpp"
#include "../../ui/layers/record_layer.hpp"

#include <Geode/modify/GJBaseGameLayer.hpp>
#include <Geode/modify/PauseLayer.hpp>
#include <Geode/modify/PlayLayer.hpp>
$execute {
    auto& settings = Settings::get();

    settings.listen<std::string>("macro_accuracy", +[](std::string value) {
        auto& bot = Bot::get();
        bot.frameFixes = value == "Frame Fixes";
        bot.inputFixes = value == "Input Fixes";
    });

    settings.listen<bool>("lock_delta", +[](bool value) {
        auto& bot = Bot::get();
        bot.lockDelta = value;
        bot.updater.resetStepState();
    });

    settings.listen<std::string>("lock_delta_mode", +[](std::string value) {
        auto& bot = Bot::get();
        bot.lockDeltaFast = value == "Fast";
        bot.updater.resetStepState();
    });

    settings.listen<bool>("lock_delta_real_time", +[](bool value) {
        auto& bot = Bot::get();
        bot.lockDeltaRealTime = value;
        bot.updater.resetStepState();
    });

    settings.listen<int64_t>("lock_delta_max_upr", +[](int64_t value) {
        auto& bot = Bot::get();
        bot.lockDeltaMaxUpr = std::max<int64_t>(value, 1);
        bot.updater.resetStepState();
    });

    settings.listen<bool>("lock_delta_use_visual_updates", +[](bool value) {
        auto& bot = Bot::get();
        bot.lockDeltaUseVisualUpdates = value;
        bot.updater.resetStepState();
    });

    settings.listen<bool>("auto_stop_playing", +[](bool value) {
        Bot::get().stopPlaying = value;
    });
};

class $modify(PlayLayer) {

    struct Fields {
        int delayedLevelRestart = -1;
    };

    void postUpdate(float dt) {
        PlayLayer::postUpdate(dt);

        if (m_fields->delayedLevelRestart != -1 &&
            m_fields->delayedLevelRestart <= Bot::getCurrentFrame()) {
            m_fields->delayedLevelRestart = -1;
            resetLevelFromStart();
        }
    }

    void onQuit() {
        if (Settings::get().value<bool>("disable_speedhack") &&
            Bot::get().speedhackEnabled)
            Bot::toggleSpeedhack();
        Bot::get().attemptStartFrame = 0;
        Bot::get().updater.frameCount = 0;
        PlayLayer::onQuit();
    }

    void pauseGame(bool b1) {
        auto& bot = Bot::get();

        if (!m_player1 || !m_player2)
            return PlayLayer::pauseGame(b1);
        if (bot.state != state::recording)
            return PlayLayer::pauseGame(b1);

        bot.ignoreRecordAction = true;
        PlayLayer::pauseGame(b1);
        bot.ignoreRecordAction = false;
    }

    bool init(GJGameLevel* level, bool b1, bool b2) {
        auto& bot = Bot::get();
        bot.firstAttempt = true;
        bot.attemptStartFrame = 0;
        bot.updater.frameCount = 0;
        if (!PlayLayer::init(level, b1, b2))
            return false;
        bot.currentSession = asp::time::SystemTime::now().timeSinceEpoch().millis();
        bot.lastAutoSaveFrame = 0;
        return true;
    }

    void resetLevel() {
        bool hadCheckpoints = m_checkpointArray->count() > 0;

        auto& bot = Bot::get();
        bool previousIgnore = bot.ignoreRecordAction;
        bot.ignoreRecordAction = true;

        PlayLayer::resetLevel();

        bot.ignoreRecordAction = previousIgnore;

        if (!hadCheckpoints) {
            bot.attemptStartFrame = 0;
            bot.updater.frameCount = 0;
            bot.previousFrame = 0;
        }

        int frame = Bot::getCurrentFrame();
        if (bot.restart && m_isPlatformer && bot.state != state::none)
            m_fields->delayedLevelRestart = frame + 2;

        Bot::updateSeed(true);

        bot.safeMode = false;
        if (bot.layoutMode)
            bot.safeMode = true;

        bot.currentAction = 0;
        bot.currentFrameFix = 0;
        bot.restart = false;
        bot.respawnFrame = frame;

        if (bot.state == state::recording)
            Bot::updateMacroInfo(this);

        if ((!m_isPracticeMode || frame <= 1 ||
             (bot.checkpoints.empty() && m_checkpointArray->count() <= 0)) &&
            bot.state == state::recording) {
            bot.replay.inputs.clear();
            bot.replay.frameFixes.clear();
            bot.checkpoints.clear();

            bot.replay.framerate = 240.f;
            if (bot.layer)
                static_cast<RecordLayer*>(bot.layer)->updateTPS();
        }
    }
};

class $modify(BGLHook, GJBaseGameLayer) {

    struct Fields {
        bool macroInput = false;
        size_t queuedMacroInputs = 0;
    };

    void processQueuedButtons(float dt, bool clearInputQueue) {
        auto& bot = Bot::get();
        PlayLayer* pl = PlayLayer::get();

        if (!pl)
            return GJBaseGameLayer::processQueuedButtons(dt, clearInputQueue);

        Bot::updateSeed();

        bool rendering = false;
#ifndef GEODE_IS_IOS
        rendering = bot.renderer.recording;
#endif

        if (bot.state != state::none || rendering) {
            int frame = Bot::getCurrentFrame();
            if (frame > 2 && bot.firstAttempt && bot.replay.xdBotMacro) {
                bot.firstAttempt = false;
                if ((m_isPlatformer || rendering) && !m_levelEndAnimationStarted)
                    return pl->resetLevelFromStart();
                else if (!m_levelEndAnimationStarted)
                    return pl->resetLevel();
            }

            if (bot.previousFrame == frame && frame != 0 && bot.replay.xdBotMacro)
                return GJBaseGameLayer::processQueuedButtons(dt, clearInputQueue);
        }

        int frame = Bot::getCurrentFrame();

        if (bot.replay.xdBotMacro && bot.restart && !m_levelEndAnimationStarted) {
            if ((m_isPlatformer && bot.state != state::none))
                return pl->resetLevelFromStart();
            else
                return pl->resetLevel();
        }

        if (bot.state == state::playing)
            handlePlaying(frame);

        GJBaseGameLayer::processQueuedButtons(dt, clearInputQueue);

        if (bot.state == state::none)
            return;

        bot.previousFrame = frame;

        if (bot.state == state::recording)
            handleRecording(frame);
    }

    void handleRecording(int frame) {
        auto& bot = Bot::get();

        if (!bot.frameFixes || bot.replay.inputs.empty())
            return;

        Bot::recordFrameFix(frame, m_player1, m_player2);
    }

    void handlePlaying(int frame) {
        auto& bot = Bot::get();
        if (m_levelEndAnimationStarted)
            return;

        if (m_player1->m_isDead) {
            m_player1->releaseAllButtons();
            m_player2->releaseAllButtons();
            return;
        }

        m_fields->macroInput = true;

        while (bot.currentAction < bot.replay.inputs.size() &&
               frame >= bot.replay.inputs[bot.currentAction].frame) {
            auto input = bot.replay.inputs[bot.currentAction];
            if (frame != bot.respawnFrame) {
                input.player2 = !input.player2;

                m_fields->queuedMacroInputs++;
                queueButton(input.button, input.down, input.player2, 0.0);
            }
            bot.currentAction++;
            bot.safeMode = true;
        }

        bot.respawnFrame = -1;
        m_fields->macroInput = false;

        if (bot.currentAction == bot.replay.inputs.size()) {
            if (bot.stopPlaying) {
                Bot::togglePlaying();
                Bot::resetState(true);
                return;
            }
        }

        if (!(bot.frameFixes || bot.inputFixes) || !PlayLayer::get())
            return;

        while (bot.currentFrameFix < bot.replay.frameFixes.size() &&
               frame >= bot.replay.frameFixes[bot.currentFrameFix].frame) {
            auto& fix = bot.replay.frameFixes[bot.currentFrameFix];

            PlayerObject* p1 = m_player1;
            PlayerObject* p2 = m_player2;

            p1->setPosition(fix.p1.pos);
            if (fix.p1.rotate && fix.p1.rotation != 0.f)
                p1->setRotation(fix.p1.rotation);
            p1->m_yVelocity = fix.p1.yVelocity;
            p1->m_platformerXVelocity = fix.p1.xVelocity;

            if (m_gameState.m_isDualMode) {
                p2->setPosition(fix.p2.pos);
                if (fix.p2.rotate && fix.p2.rotation != 0.f)
                    p2->setRotation(fix.p2.rotation);
                p2->m_yVelocity = fix.p2.yVelocity;
                p2->m_platformerXVelocity = fix.p2.xVelocity;
            }

            bot.currentFrameFix++;
        }
    }

    void handleButton(bool hold, int button, bool player2) {
        auto& bot = Bot::get();

        if (bot.p2mirror && m_gameState.m_isDualMode && !bot.autoclicker) {
            GJBaseGameLayer::handleButton(
                bot.mod->getSavedValue<bool>("p2_input_mirror_inverted") ? !hold : hold,
                button,
                !player2);
        }

        if (bot.state == state::none)
            return GJBaseGameLayer::handleButton(hold, button, player2);

        if (bot.state == state::playing) {
            bool queuedMacroInput = m_fields->queuedMacroInputs > 0;
            if (queuedMacroInput)
                m_fields->queuedMacroInputs--;

            if (bot.mod->getSavedValue<bool>("macro_ignore_inputs") && !m_fields->macroInput &&
                !queuedMacroInput)
                return;

            return GJBaseGameLayer::handleButton(hold, button, player2);
        }

        int frame = Bot::getCurrentFrame();

        if (frame >= 10 && hold)
            Bot::hasIncompatibleMods();

        if (bot.state != state::recording)
            return GJBaseGameLayer::handleButton(hold, button, player2);

        if (bot.inputFixes)
            Bot::recordFrameFix(frame, m_player1, m_player2);

#ifdef GEODE_IS_MOBILE
        m_allowedButtons.clear();
#endif

        GJBaseGameLayer::handleButton(hold, button, player2);

        if (!m_levelSettings->m_twoPlayerMode)
            player2 = false;

        if (!bot.ignoreRecordAction && !bot.creatingTrajectory && !m_player1->m_isDead) {
            Bot::recordAction(frame, button, player2, hold);

            if (bot.p2mirror && m_gameState.m_isDualMode)
                Bot::recordAction(frame,
                                  button,
                                  !player2,
                                  bot.mod->getSavedValue<bool>("p2_input_mirror_inverted") ? !hold
                                                                                          : hold);
        }
    }
};

class $modify(PauseLayer) {

    void onPracticeMode(CCObject* sender) {
        PauseLayer::onPracticeMode(sender);
        if (Bot::get().state != state::none)
            PlayLayer::get()->resetLevel();
    }

    void onNormalMode(CCObject* sender) {
        PauseLayer::onNormalMode(sender);
        auto& bot = Bot::get();
        bot.checkpoints.clear();
        if (bot.restart) {
            if (PlayLayer* pl = PlayLayer::get())
                pl->resetLevel();
        }
    }

    void onQuit(CCObject* sender) {
        PauseLayer::onQuit(sender);
        Bot::resetState();
        Loader::get()->queueInMainThread([] {
            auto& bot = Bot::get();
#ifndef GEODE_IS_IOS
            if (bot.renderer.recording)
                bot.renderer.stop();
#endif
        });
    }

    void goEdit() {
        PauseLayer::goEdit();
        Bot::resetState();
        Loader::get()->queueInMainThread([] {
            auto& bot = Bot::get();
#ifndef GEODE_IS_IOS
            if (bot.renderer.recording)
                bot.renderer.stop();
#endif
        });
    }
};
