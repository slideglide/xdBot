#include "practice_fixes.hpp"
#include "checkpoint.hpp"
#include <Geode/modify/GJBaseGameLayer.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/PlayerObject.hpp>

void resetTPSBypassState();

struct PracticeCheckpointData {
    SupplementalPlayerState p1, p2;
    SupplementalPlayLayerState pl;

    PracticeCheckpointData() = default;
    PracticeCheckpointData(PlayerObject* p1Obj, PlayerObject* p2Obj, PlayLayer* plObj) {
        if (!plObj || !p1Obj)
            return;
        pl = SupplementalPlayLayerState(plObj);
        p1 = SupplementalPlayerState(p1Obj);
        if (p2Obj)
            p2 = SupplementalPlayerState(p2Obj);
    }

    void apply(PlayerObject* p1Obj, PlayerObject* p2Obj, PlayLayer* plObj) const {
        if (!plObj)
            return;
        pl.apply(plObj);
        if (p1Obj)
            p1.apply(p1Obj);
        if (p2Obj)
            p2.apply(p2Obj);
    }
};

class $modify(FixPlayLayer, PlayLayer) {
    struct Fields {
        std::unordered_map<CheckpointObject*, PracticeCheckpointData> m_checkpoints;
        std::unordered_map<CheckpointObject*, std::vector<input>> m_checkpointInputs;
        std::unordered_map<CheckpointObject*, std::vector<gdr_legacy::FrameFix>>
            m_checkpointFrameFixes;
        std::unordered_map<CheckpointObject*, int> m_checkpointFrames;
        bool m_suppressPushButtonSideEffects = false;
        SupplementalPlayerState m_pendingRestoreP1;
        SupplementalPlayerState m_pendingRestoreP2;
        std::map<int, bool> m_physicallyHeldP1;
        std::map<int, bool> m_physicallyHeldP2;
    };

    void loadFromCheckpoint(CheckpointObject* checkpoint) {
        auto* fields = m_fields.self();
        auto& g = Global::get();
        bool wasRecordingOrPlaying = (g.state == state::recording || g.state == state::playing);
        bool shouldFix = PracticeFix::shouldEnable();

        Macro::tryAutosave(m_level, checkpoint);

        if (shouldFix) {
            if (m_player1)
                m_player1->m_isDashing = false;
            if (m_gameState.m_isDualMode && m_player2)
                m_player2->m_isDashing = false;
        }

        auto physP1 = fields->m_physicallyHeldP1;
        auto physP2 = fields->m_physicallyHeldP2;

        PlayLayer::loadFromCheckpoint(checkpoint);

        resetTPSBypassState();

        if (shouldFix) {
            auto it = fields->m_checkpoints.find(checkpoint);
            if (it != fields->m_checkpoints.end()) {
                it->second.apply(m_player1, m_gameState.m_isDualMode ? m_player2 : nullptr, this);
                fields->m_physicallyHeldP1 = physP1;
                fields->m_physicallyHeldP2 = physP2;
                fields->m_suppressPushButtonSideEffects = true;
                fields->m_pendingRestoreP1 = it->second.p1;
                fields->m_pendingRestoreP2 = it->second.p2;
            }
        }

        if (wasRecordingOrPlaying) {
            auto inputIt = fields->m_checkpointInputs.find(checkpoint);
            if (inputIt != fields->m_checkpointInputs.end()) {
                g.ignoreRecordAction = true;

                if (g.state == state::recording) {
                    g.macro.inputs = inputIt->second;
                }

                auto frameIt = fields->m_checkpointFrames.find(checkpoint);
                if (frameIt != fields->m_checkpointFrames.end()) {
                    int savedFrame = frameIt->second;
                    g.m_frameCount = savedFrame;
                    int targetFrame = g.m_frameCount - g.frameOffset;

                    if (g.state == state::recording) {
                        auto sizeBefore = g.macro.inputs.size();
                        g.macro.inputs.erase(std::remove_if(g.macro.inputs.begin(),
                                                            g.macro.inputs.end(),
                                                            [targetFrame](const input& inp) {
                                                                return inp.frame >= targetFrame;
                                                            }),
                                             g.macro.inputs.end());
                    }

                    g.currentAction = 0;
                    while (g.currentAction < g.macro.inputs.size() &&
                           g.macro.inputs[g.currentAction].frame < targetFrame) {
                        g.currentAction++;
                    }
                    g.currentFrameFix = 0;
                    while (g.currentFrameFix < g.macro.frameFixes.size() &&
                           g.macro.frameFixes[g.currentFrameFix].frame < targetFrame) {
                        g.currentFrameFix++;
                    }

                } else {

                    if (g.state == state::recording) {
                        auto fixIt = fields->m_checkpointFrameFixes.find(checkpoint);
                        if (fixIt != fields->m_checkpointFrameFixes.end()) {
                            g.macro.frameFixes = fixIt->second;
                            int targetFrame = g.m_frameCount - g.frameOffset;
                            g.currentFrameFix = 0;
                            while (g.currentFrameFix < g.macro.frameFixes.size() &&
                                   g.macro.frameFixes[g.currentFrameFix].frame < targetFrame) {
                                g.currentFrameFix++;
                            }
                        }
                    }

                    g.ignoreRecordAction = false;
                }
            }
        }
    }

    CheckpointObject* createCheckpoint() {
        bool shouldFix = PracticeFix::shouldEnable();
        auto* checkpoint = PlayLayer::createCheckpoint();

        if (!checkpoint)
            return checkpoint;

        if (shouldFix) {
            auto* fields = m_fields.self();
            fields->m_checkpoints[checkpoint] = PracticeCheckpointData(
                m_player1, m_gameState.m_isDualMode ? m_player2 : nullptr, this);

            auto& saved = fields->m_checkpoints[checkpoint];

            saved.p1.m_jumpHeld =
                fields->m_physicallyHeldP1.count(1) && fields->m_physicallyHeldP1.at(1);
            if (m_gameState.m_isDualMode)
                saved.p2.m_jumpHeld =
                    fields->m_physicallyHeldP2.count(1) && fields->m_physicallyHeldP2.at(1);
        }

        auto& g = Global::get();
        if (g.state == state::recording || g.state == state::playing) {
            auto* fields = m_fields.self();
            g.ignoreRecordAction = true;
            fields->m_checkpointInputs[checkpoint] = g.macro.inputs;
            fields->m_checkpointFrameFixes[checkpoint] = g.macro.frameFixes;
            fields->m_checkpointFrames[checkpoint] = Global::getCurrentFrame();
            g.ignoreRecordAction = false;
        }

        return checkpoint;
    }

    void removeCheckpoint(CheckpointObject* checkpoint) {
        PlayLayer::removeCheckpoint(checkpoint);

        auto* fields = m_fields.self();
        fields->m_checkpoints.erase(checkpoint);
        fields->m_checkpointInputs.erase(checkpoint);
        fields->m_checkpointFrameFixes.erase(checkpoint);
        fields->m_checkpointFrames.erase(checkpoint);
    }

    void resetLevel() {
        bool hadCheckpoints = m_checkpointArray->count() > 0;

        if (!hadCheckpoints) {
            auto* fields = m_fields.self();
            fields->m_checkpoints.clear();
            fields->m_checkpointInputs.clear();
            fields->m_checkpointFrameFixes.clear();
            fields->m_checkpointFrames.clear();
        }

        PlayLayer::resetLevel();

        if (hadCheckpoints)
            m_resumeTimer = 0;
    }
};

// hold restore, took me a lot of time to figure out how to do this but this
// hook was the solution!
class $modify(FixGJBaseGameLayer, GJBaseGameLayer) {
    void handleButton(bool down, int button, bool isPlayer1) {
        auto* pl = PlayLayer::get();
        bool shouldFix = PracticeFix::shouldEnable();

        if (pl) {
            auto* fields = static_cast<FixPlayLayer*>(pl)->m_fields.self();
            auto& physHeld = isPlayer1 ? fields->m_physicallyHeldP1 : fields->m_physicallyHeldP2;
            physHeld[button] = down;
        }

        if (!shouldFix || !pl || !down) {
            GJBaseGameLayer::handleButton(down, button, isPlayer1);
            return;
        }

        auto* fields = static_cast<FixPlayLayer*>(pl)->m_fields.self();

        if (!fields->m_suppressPushButtonSideEffects) {
            GJBaseGameLayer::handleButton(down, button, isPlayer1);
            return;
        }

        const SupplementalPlayerState& saved =
            isPlayer1 ? fields->m_pendingRestoreP1 : fields->m_pendingRestoreP2;

        bool wasHeld = false;
        switch (button) {
        case 1:
            wasHeld = saved.m_jumpHeld;
            break;
        case 2:
            wasHeld = saved.m_holdingLeft;
            break;
        case 3:
            wasHeld = saved.m_holdingRight;
            break;
        }

        if (wasHeld)
            return;

        GJBaseGameLayer::handleButton(down, button, isPlayer1);
    }

    void processQueuedButtons(float dt, bool clearInputQueue) {
        GJBaseGameLayer::processQueuedButtons(dt, clearInputQueue);

        auto* pl = PlayLayer::get();
        if (!pl)
            return;

        auto* fields = static_cast<FixPlayLayer*>(pl)->m_fields.self();
        if (!fields->m_suppressPushButtonSideEffects)
            return;

        fields->m_suppressPushButtonSideEffects = false;

        bool shouldFix = PracticeFix::shouldEnable();
        if (!shouldFix) {
            fields->m_pendingRestoreP1 = {};
            fields->m_pendingRestoreP2 = {};
            return;
        }

        auto clearIfNotHeld =
            [&](PlayerObject* player, const SupplementalPlayerState& saved, bool isP1) {
                if (!player)
                    return;
                auto& phys = isP1 ? fields->m_physicallyHeldP1 : fields->m_physicallyHeldP2;

                auto isHeld = [&](int btn) {
                    auto it = phys.find(btn);
                    return it != phys.end() && it->second;
                };

                if (saved.m_holdingLeft && !isHeld(2))
                    GJBaseGameLayer::handleButton(false, 2, isP1);
                if (saved.m_holdingRight && !isHeld(3))
                    GJBaseGameLayer::handleButton(false, 3, isP1);
                if (saved.m_jumpHeld && !isHeld(1))
                    GJBaseGameLayer::handleButton(false, 1, isP1);
            };

        clearIfNotHeld(pl->m_player1, fields->m_pendingRestoreP1, true);
        clearIfNotHeld(pl->m_player2, fields->m_pendingRestoreP2, false);

        fields->m_pendingRestoreP1 = {};
        fields->m_pendingRestoreP2 = {};
    }
};