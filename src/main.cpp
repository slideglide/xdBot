#include "includes.hpp"

#include "practice_fixes/practice_fixes.hpp"
#include "ui/record_layer.hpp"

#include <Geode/modify/GJBaseGameLayer.hpp>
#include <Geode/modify/PauseLayer.hpp>
#include <Geode/modify/PlayLayer.hpp>

$execute {
    geode::listenForSettingChanges<std::string>(
        "macro_accuracy",
        +[](std::string value) {
            auto& g = Global::get();
            g.frameFixes = false;
            g.inputFixes = false;
            if (value == "Frame Fixes")
                g.frameFixes = true;
            if (value == "Input Fixes")
                g.inputFixes = true;
        },
        Mod::get());

    geode::listenForSettingChanges<bool>(
        "lock_delta",
        +[](bool value) {
            auto& g = Global::get();
            g.lockDelta = value;
            g.schedulerOverflow = 0.0;
            g.schedulerStepCount = 1;
        },
        Mod::get());

    geode::listenForSettingChanges<bool>(
        "auto_stop_playing",
        +[](bool value) {
            Global::get().stopPlaying = value;
        },
        Mod::get());
};

class $modify(PlayLayer) {

    struct Fields {
        int delayedLevelRestart = -1;
    };

    void postUpdate(float dt) {
        PlayLayer::postUpdate(dt);
        auto& g = Global::get();
        if (!g.postUpdateInputs.empty()) {
            auto inputs = std::move(g.postUpdateInputs);
            g.postUpdateInputs.clear();
            for (auto const& input : inputs) {
                GJBaseGameLayer::handleButton(input.down, input.button, input.player2);
            }
        }

        if (m_fields->delayedLevelRestart != -1 &&
            m_fields->delayedLevelRestart <= Global::getCurrentFrame()) {
            m_fields->delayedLevelRestart = -1;
            resetLevelFromStart();
        }
    }

    void onQuit() {
        if (Mod::get()->getSettingValue<bool>("disable_speedhack") &&
            Global::get().speedhackEnabled)
            Global::toggleSpeedhack();
        Global::get().m_frameCount = 0;
        Global::get().postUpdateInputs.clear();
        PlayLayer::onQuit();
    }

    void pauseGame(bool b1) {
        auto& g = Global::get();

        if (!m_player1 || !m_player2)
            return PlayLayer::pauseGame(b1);
        if (g.state != state::recording)
            return PlayLayer::pauseGame(b1);

        g.ignoreRecordAction = true;
        PlayLayer::pauseGame(b1);
        g.ignoreRecordAction = false;
    }

    bool init(GJGameLevel* level, bool b1, bool b2) {
        auto& g = Global::get();
        g.firstAttempt = true;
        if (!PlayLayer::init(level, b1, b2))
            return false;
        g.currentSession = asp::time::SystemTime::now().timeSinceEpoch().millis();
        g.lastAutoSaveFrame = 0;
        return true;
    }

    void resetLevel() {
        bool hadCheckpoints = m_checkpointArray->count() > 0;

        auto& g = Global::get();
        bool previousIgnore = g.ignoreRecordAction;
        g.ignoreRecordAction = true;

        PlayLayer::resetLevel();

        g.ignoreRecordAction = previousIgnore;

        if (!hadCheckpoints)
            g.m_frameCount = 0;

        int frame = Global::getCurrentFrame();
        if (g.restart && m_isPlatformer && g.state != state::none)
            m_fields->delayedLevelRestart = frame + 2;

        Global::updateSeed(true);

        g.safeMode = false;
        if (g.layoutMode)
            g.safeMode = true;

        g.currentAction = 0;
        g.currentFrameFix = 0;
        g.postUpdateInputs.clear();
        g.restart = false;
        g.respawnFrame = frame;

        if (g.state == state::recording)
            Macro::updateInfo(this);

        if ((!m_isPracticeMode || frame <= 1 ||
             (g.checkpoints.empty() && m_checkpointArray->count() <= 0)) &&
            g.state == state::recording) {
            g.macro.inputs.clear();
            g.macro.frameFixes.clear();
            g.checkpoints.clear();

            g.macro.framerate = 240.f;
            if (g.layer)
                static_cast<RecordLayer*>(g.layer)->updateTPS();
        }
    }
};

class $modify(BGLHook, GJBaseGameLayer) {

    struct Fields {
        bool macroInput = false;
        size_t queuedMacroInputs = 0;
    };

    void processQueuedButtons(float dt, bool clearInputQueue) {
        auto& g = Global::get();
        PlayLayer* pl = PlayLayer::get();

        if (!pl)
            return GJBaseGameLayer::processQueuedButtons(dt, clearInputQueue);

        Global::updateSeed();

        bool rendering = false;
#ifndef GEODE_IS_IOS
        rendering = g.renderer.recording;
#endif

        if (g.state != state::none || rendering) {
            int frame = Global::getCurrentFrame();
            if (frame > 2 && g.firstAttempt && g.macro.xdBotMacro) {
                g.firstAttempt = false;
                if ((m_isPlatformer || rendering) && !m_levelEndAnimationStarted)
                    return pl->resetLevelFromStart();
                else if (!m_levelEndAnimationStarted)
                    return pl->resetLevel();
            }

            if (g.previousFrame == frame && frame != 0 && g.macro.xdBotMacro)
                return GJBaseGameLayer::processQueuedButtons(dt, clearInputQueue);
        }

        int frame = Global::getCurrentFrame();

        if (g.macro.xdBotMacro && g.restart && !m_levelEndAnimationStarted) {
            if ((m_isPlatformer && g.state != state::none))
                return pl->resetLevelFromStart();
            else
                return pl->resetLevel();
        }

        if (g.state == state::playing)
            handlePlaying(frame);

        GJBaseGameLayer::processQueuedButtons(dt, clearInputQueue);

        if (g.state == state::none)
            return;

        g.previousFrame = frame;

        if (g.state == state::recording)
            handleRecording(frame);
    }

    void handleRecording(int frame) {
        auto& g = Global::get();

        if (!g.frameFixes || g.macro.inputs.empty())
            return;

        g.macro.recordFrameFix(frame, m_player1, m_player2);
    }

    void handlePlaying(int frame) {
        auto& g = Global::get();
        if (m_levelEndAnimationStarted)
            return;

        if (m_player1->m_isDead) {
            m_player1->releaseAllButtons();
            m_player2->releaseAllButtons();
            return;
        }

        m_fields->macroInput = true;

        bool hasQueuedState[2][4] = {};
        bool queuedState[2][4] = {};

        while (g.currentAction < g.macro.inputs.size() &&
               frame >= g.macro.inputs[g.currentAction].frame) {
            auto input = g.macro.inputs[g.currentAction];
            if (frame != g.respawnFrame) {
                if (Macro::flipControls())
                    input.player2 = !input.player2;

                int playerIndex = input.player2 ? 1 : 0;
                if (input.button < 4 && hasQueuedState[playerIndex][input.button] &&
                    queuedState[playerIndex][input.button] != input.down) {
                    g.postUpdateInputs.push_back(input);
                    g.currentAction++;
                    g.safeMode = true;
                    continue;
                }

                if (input.button < 4) {
                    hasQueuedState[playerIndex][input.button] = true;
                    queuedState[playerIndex][input.button] = input.down;
                }

                m_fields->queuedMacroInputs++;
                queueButton(input.button, input.down, input.player2, 0.0);
            }
            g.currentAction++;
            g.safeMode = true;
        }

        g.respawnFrame = -1;
        m_fields->macroInput = false;

        if (g.currentAction == g.macro.inputs.size()) {
            if (g.stopPlaying) {
                Macro::togglePlaying();
                Macro::resetState(true);
                return;
            }
        }

        if (!(g.frameFixes || g.inputFixes) || !PlayLayer::get())
            return;

        while (g.currentFrameFix < g.macro.frameFixes.size() &&
               frame >= g.macro.frameFixes[g.currentFrameFix].frame) {
            auto& fix = g.macro.frameFixes[g.currentFrameFix];

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

            g.currentFrameFix++;
        }
    }

    void handleButton(bool hold, int button, bool player2) {
        auto& g = Global::get();

        if (g.p2mirror && m_gameState.m_isDualMode && !g.autoclicker) {
            GJBaseGameLayer::handleButton(
                g.mod->getSavedValue<bool>("p2_input_mirror_inverted") ? !hold : hold,
                button,
                !player2);
        }

        if (g.state == state::none)
            return GJBaseGameLayer::handleButton(hold, button, player2);

        if (g.state == state::playing) {
            bool queuedMacroInput = m_fields->queuedMacroInputs > 0;
            if (queuedMacroInput)
                m_fields->queuedMacroInputs--;

            if (g.mod->getSavedValue<bool>("macro_ignore_inputs") && !m_fields->macroInput &&
                !queuedMacroInput)
                return;

            return GJBaseGameLayer::handleButton(hold, button, player2);
        }

        int frame = Global::getCurrentFrame();

        if (frame >= 10 && hold)
            Global::hasIncompatibleMods();

        if (g.state != state::recording)
            return GJBaseGameLayer::handleButton(hold, button, player2);

        if (g.inputFixes)
            g.macro.recordFrameFix(frame, m_player1, m_player2);

#ifdef GEODE_IS_MOBILE
        m_allowedButtons.clear();
#endif

        GJBaseGameLayer::handleButton(hold, button, player2);

        if (!m_levelSettings->m_twoPlayerMode)
            player2 = false;

        if (!g.ignoreRecordAction && !g.creatingTrajectory && !m_player1->m_isDead) {
            g.macro.recordAction(frame, button, player2, hold);

            if (g.p2mirror && m_gameState.m_isDualMode)
                g.macro.recordAction(frame,
                                     button,
                                     !player2,
                                     g.mod->getSavedValue<bool>("p2_input_mirror_inverted") ? !hold
                                                                                            : hold);
        }
    }
};

class $modify(PauseLayer) {

    void onPracticeMode(CCObject* sender) {
        PauseLayer::onPracticeMode(sender);
        if (Global::get().state != state::none)
            PlayLayer::get()->resetLevel();
    }

    void onNormalMode(CCObject* sender) {
        PauseLayer::onNormalMode(sender);
        auto& g = Global::get();
        g.checkpoints.clear();
        if (g.restart) {
            if (PlayLayer* pl = PlayLayer::get())
                pl->resetLevel();
        }
    }

    void onQuit(CCObject* sender) {
        PauseLayer::onQuit(sender);
        Macro::resetState();
        Loader::get()->queueInMainThread([] {
            auto& g = Global::get();
#ifndef GEODE_IS_IOS
            if (g.renderer.recording)
                g.renderer.stop();
#endif
        });
    }

    void goEdit() {
        PauseLayer::goEdit();
        Macro::resetState();
        Loader::get()->queueInMainThread([] {
            auto& g = Global::get();
#ifndef GEODE_IS_IOS
            if (g.renderer.recording)
                g.renderer.stop();
#endif
        });
    }
};
