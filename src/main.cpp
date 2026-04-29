#include "includes.hpp"

#include "ui/record_layer.hpp"
#include "practice_fixes/practice_fixes.hpp"

#include <Geode/modify/GJBaseGameLayer.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/PauseLayer.hpp>

$execute {
    geode::listenForSettingChanges<std::string>("macro_accuracy", +[](std::string value) {
        auto& g = Global::get();
        g.frameFixes = false;
        g.inputFixes = false;
        if (value == "Frame Fixes") g.frameFixes = true;
        if (value == "Input Fixes") g.inputFixes = true;
    }, Mod::get());

    geode::listenForSettingChanges<int64_t>("frame_fixes_limit", +[](int64_t value) {
        Global::get().frameFixesLimit = value;
    }, Mod::get());

    geode::listenForSettingChanges<bool>("lock_delta", +[](bool value) {
        Global::get().lockDelta = value;
    }, Mod::get());

    geode::listenForSettingChanges<bool>("auto_stop_playing", +[](bool value) {
        Global::get().stopPlaying = value;
    }, Mod::get());
};

class $modify(EclipseSyncGJBGLHook, GJBaseGameLayer) {
    void update(float dt) {
        GJBaseGameLayer::update(dt);

        static bool hasEclipse = Loader::get()->getLoadedMod("eclipse.eclipse-menu") != nullptr;
        if (!hasEclipse) return;

        auto& g = Global::get();
        if (!g.tpsEnabled) return;

        bool eclipseEnabled = eclipse::config::getInternal("global.tpsbypass.toggle", false);
        double eclipseTps = eclipse::config::getInternal("global.tpsbypass", 240.0);

        if (!eclipseEnabled) {
            eclipse::config::setInternal("global.tpsbypass.toggle", true);
        }

        if (eclipseTps != static_cast<double>(g.tps)) {
            eclipse::config::setInternal("global.tpsbypass", static_cast<double>(g.tps));
        }
    }
};

class $modify(PlayLayer) {

    struct Fields {
        int delayedLevelRestart = -1;
    };

    void postUpdate(float dt) {
        PlayLayer::postUpdate(dt);
        auto& g = Global::get();
        if (m_fields->delayedLevelRestart != -1 && m_fields->delayedLevelRestart >= Global::getCurrentFrame()) {
            m_fields->delayedLevelRestart = -1;
            resetLevelFromStart();
        }
    }

    void onQuit() {
        if (Mod::get()->getSettingValue<bool>("disable_speedhack") && Global::get().speedhackEnabled)
            Global::toggleSpeedhack();
        Global::get().m_frameCount = 0;
        PlayLayer::onQuit();
    }

    void pauseGame(bool b1) {
        auto& g = Global::get();

        if (!m_player1 || !m_player2) return PlayLayer::pauseGame(b1);
        if (g.state != state::recording) return PlayLayer::pauseGame(b1);

        g.ignoreRecordAction = true;
        int frame = Global::getCurrentFrame() + 1;

        if (m_player1->m_holdingButtons[1]) {
            handleButton(false, 1, false);
            g.macro.inputs.push_back(input(frame, 1, false, false));
        }
        if (m_isPlatformer) {
            if (m_player1->m_holdingButtons[2]) {
                handleButton(false, 2, false);
                g.macro.inputs.push_back(input(frame, 2, false, false));
            }
            if (m_player1->m_holdingButtons[3]) {
                handleButton(false, 3, false);
                g.macro.inputs.push_back(input(frame, 3, false, false));
            }
        }

        if (m_levelSettings->m_twoPlayerMode) {
            if (m_player2->m_holdingButtons[1]) {
                handleButton(false, 1, true);
                g.macro.inputs.push_back(input(frame, 1, true, false));
            }
            if (m_isPlatformer) {
                if (m_player2->m_holdingButtons[2]) {
                    handleButton(false, 2, true);
                    g.macro.inputs.push_back(input(frame, 2, true, false));
                }
                if (m_player2->m_holdingButtons[3]) {
                    handleButton(false, 3, true);
                    g.macro.inputs.push_back(input(frame, 3, true, false));
                }
            }
        }

        g.ignoreRecordAction = false;
        PlayLayer::pauseGame(b1);
    }

    bool init(GJGameLevel* level, bool b1, bool b2) {
        auto& g = Global::get();
        g.firstAttempt = true;
        if (!PlayLayer::init(level, b1, b2)) return false;
        g.currentSession = asp::time::SystemTime::now().timeSinceEpoch().millis();
        g.lastAutoSaveFrame = 0;
        return true;
    }

    void resetLevel() {
        bool hadCheckpoints = m_checkpointArray->count() > 0;

        auto& g = Global::get();
        bool previousIgnore = g.ignoreRecordAction;
        g.ignoreRecordAction = true;

        PlayerObject* players[2] = { m_player1, m_player2 };
        g.respawnHeldButtons[0] = players[0] ? players[0]->m_holdingButtons[1] : false;
        g.respawnHeldButtons[1] = players[0] ? players[0]->m_holdingButtons[2] : false;
        g.respawnHeldButtons[2] = players[0] ? players[0]->m_holdingButtons[3] : false;
        g.respawnHeldButtons[3] = players[1] ? players[1]->m_holdingButtons[1] : false;
        g.respawnHeldButtons[4] = players[1] ? players[1]->m_holdingButtons[2] : false;
        g.respawnHeldButtons[5] = players[1] ? players[1]->m_holdingButtons[3] : false;

        PlayLayer::resetLevel();

        g.ignoreRecordAction = previousIgnore;

        if (!hadCheckpoints)
            g.m_frameCount = 0;

        int frame = Global::getCurrentFrame();
        if (g.restart && m_isPlatformer && g.state != state::none)
            m_fields->delayedLevelRestart = frame + 2;

        Global::updateSeed(true);

        g.safeMode = false;
        if (g.layoutMode) g.safeMode = true;

        g.currentAction = 0;
        g.currentFrameFix = 0;
        g.restart = false;
        g.respawnFrame = frame;

        if (g.state == state::recording)
            Macro::updateInfo(this);

        if ((!m_isPracticeMode || frame <= 1 || (g.checkpoints.empty() && m_checkpointArray->count() <= 0)) && g.state == state::recording) {
            g.macro.inputs.clear();
            g.macro.frameFixes.clear();
            g.checkpoints.clear();

            g.macro.framerate = 240.f;
            if (g.layer) static_cast<RecordLayer*>(g.layer)->updateTPS();

            Macro::resetVariables();

            m_player1->m_holdingRight = false;
            m_player1->m_holdingLeft = false;
            m_player2->m_holdingRight = false;
            m_player2->m_holdingLeft = false;

            m_player1->m_holdingButtons[2] = false;
            m_player1->m_holdingButtons[3] = false;
            m_player2->m_holdingButtons[2] = false;
            m_player2->m_holdingButtons[3] = false;
        }

        if (!m_isPlatformer || (!g.alwaysPracticeFixes && g.state != state::recording)) return;

        g.ignoreRecordAction = true;
        for (int i = 0; i < 4; i++) {
            bool player2 = sidesButtons[i] > 2;
            if (g.heldButtons[sidesButtons[i]])
                handleButton(true, indexButton[sidesButtons[i]], player2);
        }
        g.ignoreRecordAction = false;
    }
};

class $modify(BGLHook, GJBaseGameLayer) {

    struct Fields {
        bool macroInput = false;
        bool isHalfTick = false;
        bool isLastTick = false;
    };

    bool isInHalfTick() { return m_fields->isHalfTick; }
    bool isInLastTick() { return m_fields->isLastTick; }

#ifndef GEODE_IS_MACOS
    void processCommands(float dt, bool isHalfTick, bool isLastTick) {
        m_fields->isHalfTick = isHalfTick;
        m_fields->isLastTick = isLastTick;

        auto& g = Global::get();
        PlayLayer* pl = PlayLayer::get();

        if (!pl)
            return GJBaseGameLayer::processCommands(dt, isHalfTick, isLastTick);

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
                return GJBaseGameLayer::processCommands(dt, isHalfTick, isLastTick);
        }

        GJBaseGameLayer::processCommands(dt, isHalfTick, isLastTick);
        
        if (isHalfTick) return;
        if (g.state == state::none) return;

        int frame = Global::getCurrentFrame();
        g.previousFrame = frame;

        if (g.macro.xdBotMacro && g.restart && !m_levelEndAnimationStarted) {
#ifndef GEODE_IS_MOBILE
            if ((m_isPlatformer && g.state != state::none))
                return pl->resetLevelFromStart();
            else
                return pl->resetLevel();
#else
            if (m_isPlatformer && g.state != state::none)
                return pl->resetLevelFromStart();
            else
                return pl->resetLevel();
#endif
        }

        if (g.state == state::recording) handleRecording(frame);
        if (g.state == state::playing) handlePlaying(Global::getCurrentFrame());
    }
#else
    void processQueuedButtons(float dt, bool clearInputQueue) {
        auto& g = Global::get();
        PlayLayer* pl = PlayLayer::get();

        if (!pl)
            return GJBaseGameLayer::processQueuedButtons(dt, clearInputQueue);

        Global::updateSeed();

        bool rendering = false;

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

        GJBaseGameLayer::processQueuedButtons(dt, clearInputQueue);

        if (g.state == state::none) return;

        int frame = Global::getCurrentFrame();
        g.previousFrame = frame;

        if (g.macro.xdBotMacro && g.restart && !m_levelEndAnimationStarted) {
            if ((m_isPlatformer && g.state != state::none))
                return pl->resetLevelFromStart();
            else
                return pl->resetLevel();
        }

        if (g.state == state::recording) handleRecording(frame);
        if (g.state == state::playing) handlePlaying(Global::getCurrentFrame());
    }
    #endif

    void handleRecording(int frame) {
        auto& g = Global::get();

        if (g.ignoreFrame != -1) {
            if (g.ignoreFrame < frame) g.ignoreFrame = -1;
        }

        if (frame > g.ignoreJumpButton && g.ignoreJumpButton != -1)
            g.ignoreJumpButton = -1;

        if (!g.frameFixes || g.macro.inputs.empty()) return;

        if (!g.macro.frameFixes.empty())
            if (1.f / Global::getTPS() * (frame - g.macro.frameFixes.back().frame) < 1.f / g.frameFixesLimit)
                return;

        g.macro.recordFrameFix(frame, m_player1, m_player2);
    }

    bool isHoldActivatedRingMode(PlayerObject* player) {
        if (!player) return false;
        return player->m_isShip || player->m_isBird || player->m_isDart || player->m_isSwing;
    }

    void playerTouchedRing(PlayerObject* player, RingObject* ring) {
        GJBaseGameLayer::playerTouchedRing(player, ring);

        auto& g = Global::get();
        if (!player || !ring || g.state == state::none) return;
        if (!isHoldActivatedRingMode(player)) return;

        bool player2 = m_gameState.m_isDualMode && player == m_player2;
        int jumpIdx = player2 ? 3 : 0;

        // Hold restore is a continued hold, not a new click, so activate the ring directly.
        if (g.respawnHeldButtons[jumpIdx] && player->m_holdingButtons[1]) {
            player->ringJump(ring, false);
        }
    }

    void handlePlaying(int frame) {
        auto& g = Global::get();
        if (m_levelEndAnimationStarted) return;

        if (m_player1->m_isDead) {
            m_player1->releaseAllButtons();
            m_player2->releaseAllButtons();
            return;
        }
        
        m_fields->macroInput = true;

        while (g.currentAction < g.macro.inputs.size() && frame >= g.macro.inputs[g.currentAction].frame) {
            auto input = g.macro.inputs[g.currentAction];
            if (frame != g.respawnFrame) {
                if (Macro::flipControls()) input.player2 = !input.player2;
                GJBaseGameLayer::handleButton(input.down, input.button, input.player2);
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

        if (!(g.frameFixes || g.inputFixes) || !PlayLayer::get()) return;

        while (g.currentFrameFix < g.macro.frameFixes.size() && frame >= g.macro.frameFixes[g.currentFrameFix].frame) {
            auto& fix = g.macro.frameFixes[g.currentFrameFix];

            PlayerObject* p1 = m_player1;
            PlayerObject* p2 = m_player2;

            p1->setPosition(fix.p1.pos);
            if (fix.p1.rotate && fix.p1.rotation != 0.f)
                p1->setRotation(fix.p1.rotation);
            p1->m_yVelocity = fix.p1.yVelocity;
            p1->m_platformerXVelocity = fix.p1.xVelocity;
            p1->m_isDashing = fix.p1.isDashing;
            p1->m_isOnGround = fix.p1.isOnGround;

            if (m_gameState.m_isDualMode) {
                p2->setPosition(fix.p2.pos);
                if (fix.p2.rotate && fix.p2.rotation != 0.f)
                    p2->setRotation(fix.p2.rotation);
                p2->m_yVelocity = fix.p2.yVelocity;
                p2->m_platformerXVelocity = fix.p2.xVelocity;
                p2->m_isDashing = fix.p2.isDashing;
                p2->m_isOnGround = fix.p2.isOnGround;
            }

            g.currentFrameFix++;
        }
    }
        
    void handleButton(bool hold, int button, bool player2) {
        auto& g = Global::get();

        if (g.p2mirror && m_gameState.m_isDualMode && !g.autoclicker) {
            GJBaseGameLayer::handleButton(
                g.mod->getSavedValue<bool>("p2_input_mirror_inverted") ? !hold : hold,
                button, !player2);
        }

        if (g.state == state::none)
            return GJBaseGameLayer::handleButton(hold, button, player2);

        if (g.state == state::playing) {
            if (g.mod->getSavedValue<bool>("macro_ignore_inputs") && !m_fields->macroInput)
                return;
            else
                return GJBaseGameLayer::handleButton(hold, button, player2);
        } else if (g.ignoreFrame != -1 && hold) {
            return;
        }

        int frame = Global::getCurrentFrame();

        if (frame >= 10 && hold)
            Global::hasIncompatibleMods();

        bool isDelayedInput = false;
        bool isDelayedRelease = false;

        if ((isDelayedInput || g.ignoreJumpButton == frame || isDelayedRelease) && button == 1)
            return;

        if (g.state != state::recording)
            return GJBaseGameLayer::handleButton(hold, button, player2);

        if (g.inputFixes)
            g.macro.recordFrameFix(frame, m_player1, m_player2);
        
        #ifdef GEODE_IS_MOBILE
        m_allowedButtons.clear(); // mobile swift click patch bypass
        #endif

        GJBaseGameLayer::handleButton(hold, button, player2);

        if (!m_levelSettings->m_twoPlayerMode)
            player2 = false;

        if (!g.ignoreRecordAction && !g.creatingTrajectory && !m_player1->m_isDead) {
            int idx = buttonIndex[player2 ? 1 : 0].at(button);
            if (g.respawnHeldButtons[idx]) {
                if (hold) return;
                g.respawnHeldButtons[idx] = false;
            }

            g.macro.recordAction(frame, button, player2, hold);
            if (g.p2mirror && m_gameState.m_isDualMode)
                g.macro.recordAction(frame, button, !player2,
                    g.mod->getSavedValue<bool>("p2_input_mirror_inverted") ? !hold : hold);
        }
    }
};

class $modify(PauseLayer) {

    void onPracticeMode(CCObject* sender) {
        PauseLayer::onPracticeMode(sender);
        if (Global::get().state != state::none) PlayLayer::get()->resetLevel();
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
            if (g.renderer.recording) g.renderer.stop();
#endif
        });
    }

    void goEdit() {
        PauseLayer::goEdit();
        Macro::resetState();
        Loader::get()->queueInMainThread([] {
            auto& g = Global::get();
#ifndef GEODE_IS_IOS
            if (g.renderer.recording) g.renderer.stop();
#endif
        });
    }
};
