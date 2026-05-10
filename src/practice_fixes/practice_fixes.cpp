#include "practice_fixes.hpp"
#include "checkpoint.hpp"
#include <Geode/modify/GJBaseGameLayer.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/PlayerObject.hpp>
#include <Geode/modify/UILayer.hpp>

void resetTPSBypassState();

static std::vector<GameObject*>& brokenPracticeObjects() {
    static std::vector<GameObject*> objects;
    return objects;
}

struct PracticeCheckpointData {
    SupplementalPlayerState p1, p2;
    SupplementalPlayLayerState pl;
    float tps = 240.f;
    bool tpsEnabled = false;
    gd::unordered_map<int, int> persistentItemMap;
    std::array<float, 2000> varianceValues = {};
    std::vector<GameObject*> calcNonEffectObjects;
    int calcNonEffectObjectsSize = 0;
    std::vector<GameObject*> brokenObjects;
    uint64_t randomSeed = 0;

    PracticeCheckpointData() = default;
    PracticeCheckpointData(
        PlayerObject* p1Obj,
        PlayerObject* p2Obj,
        PlayLayer* plObj,
        std::vector<GameObject*> const& broken
    ) {
        if (!plObj || !p1Obj)
            return;
        pl = SupplementalPlayLayerState(plObj);
        p1 = SupplementalPlayerState(p1Obj);
        if (p2Obj)
            p2 = SupplementalPlayerState(p2Obj);
        if (plObj->m_effectManager)
            persistentItemMap = plObj->m_effectManager->m_persistentItemCountMap;
        varianceValues = plObj->m_varianceValues;
        calcNonEffectObjects = plObj->m_calcNonEffectObjects;
        calcNonEffectObjectsSize = plObj->m_calcNonEffectObjectsSize;
        brokenObjects = broken;
        randomSeed = GameToolbox::getfast_srand();
        auto& g = Global::get();
        tps = g.tps;
        tpsEnabled = g.tpsEnabled;
    }

    void apply(PlayerObject* p1Obj, PlayerObject* p2Obj, PlayLayer* plObj) const {
        if (!plObj)
            return;
        pl.apply(plObj);
        if (p1Obj)
            p1.apply(p1Obj);
        if (p2Obj)
            p2.apply(p2Obj);
        if (plObj->m_effectManager)
            plObj->m_effectManager->m_persistentItemCountMap = persistentItemMap;
        plObj->m_varianceValues = varianceValues;
        plObj->m_calcNonEffectObjects = calcNonEffectObjects;
        plObj->m_calcNonEffectObjectsSize = calcNonEffectObjectsSize;

        for (auto* obj : brokenObjects) {
            if (!obj)
                continue;
            obj->m_isDisabled = true;
            obj->m_isDisabled2 = true;
            obj->setOpacity(0.0f);
        }

        auto& g = Global::get();
        if (g.tps != tps)
            g.setTps(tps);
        if (g.tpsEnabled != tpsEnabled)
            g.setTpsEnabled(tpsEnabled);
        GameToolbox::fast_srand(randomSeed);
    }
};

class $modify(FixPlayLayer, PlayLayer) {
    struct Fields {
        std::unordered_map<CheckpointObject*, PracticeCheckpointData> m_checkpoints;
        std::unordered_map<CheckpointObject*, std::vector<input>> m_checkpointInputs;
        std::unordered_map<CheckpointObject*, std::vector<gdr_legacy::FrameFix>> m_checkpointFrameFixes;
        std::unordered_map<CheckpointObject*, int> m_checkpointFrames;
    };

    static void onModify(auto& self) {
        constexpr int32_t firstPriority = -0x500000;
        (void)self.setHookPriority("PlayLayer::loadFromCheckpoint", firstPriority);
        (void)self.setHookPriority("PlayLayer::resetLevel", firstPriority);
    }

    void loadFromCheckpoint(CheckpointObject* checkpoint) {
        bool shouldFix = PracticeFix::shouldEnable();
        auto& g = Global::get();
        bool wasRecordingOrPlaying = g.state == state::recording || g.state == state::playing;

        Macro::tryAutosave(m_level, checkpoint);

        if (shouldFix) {
            if (m_player1) m_player1->m_isDashing = false;
            if (m_gameState.m_isDualMode && m_player2) m_player2->m_isDashing = false;
        }

        PlayLayer::loadFromCheckpoint(checkpoint);
        resetTPSBypassState();

        if (shouldFix) {
            auto it = m_fields->m_checkpoints.find(checkpoint);
            if (it != m_fields->m_checkpoints.end())
                it->second.apply(m_player1, m_gameState.m_isDualMode ? m_player2 : nullptr, this);
        }

        if (wasRecordingOrPlaying) {
            auto inputIt = m_fields->m_checkpointInputs.find(checkpoint);
            if (inputIt != m_fields->m_checkpointInputs.end()) {
                g.ignoreRecordAction = true;
                if (g.state == state::recording)
                    g.macro.inputs = inputIt->second;
                auto frameIt = m_fields->m_checkpointFrames.find(checkpoint);
                if (frameIt != m_fields->m_checkpointFrames.end()) {
                    int savedFrame = frameIt->second;
                    g.m_frameCount = savedFrame;
                    int targetFrame = g.m_frameCount - g.frameOffset;
                    if (g.state == state::recording) {
                        g.macro.inputs.erase(std::remove_if(g.macro.inputs.begin(), g.macro.inputs.end(),
                            [targetFrame](const input& inp) { return inp.frame >= targetFrame; }),
                            g.macro.inputs.end());
                        auto fixIt = m_fields->m_checkpointFrameFixes.find(checkpoint);
                        if (fixIt != m_fields->m_checkpointFrameFixes.end())
                            g.macro.frameFixes = fixIt->second;
                    }
                    g.currentAction = 0;
                    while (g.currentAction < g.macro.inputs.size() &&
                           g.macro.inputs[g.currentAction].frame < targetFrame)
                        g.currentAction++;
                    g.currentFrameFix = 0;
                    while (g.currentFrameFix < g.macro.frameFixes.size() &&
                           g.macro.frameFixes[g.currentFrameFix].frame < targetFrame)
                        g.currentFrameFix++;
                }
                g.ignoreRecordAction = false;
            }
        }
    }

    CheckpointObject* createCheckpoint() {
        auto* checkpoint = PlayLayer::createCheckpoint();
        if (!checkpoint) return checkpoint;
        if (PracticeFix::shouldEnable()) {
            m_fields->m_checkpoints[checkpoint] = PracticeCheckpointData(
                m_player1,
                m_gameState.m_isDualMode ? m_player2 : nullptr,
                this,
                brokenPracticeObjects()
            );
        }
        auto& g = Global::get();
        if (g.state == state::recording || g.state == state::playing) {
            g.ignoreRecordAction = true;
            m_fields->m_checkpointInputs[checkpoint] = g.macro.inputs;
            m_fields->m_checkpointFrameFixes[checkpoint] = g.macro.frameFixes;
            m_fields->m_checkpointFrames[checkpoint] = Global::getCurrentFrame();
            g.ignoreRecordAction = false;
        }
        return checkpoint;
    }

    void removeCheckpoint(CheckpointObject* checkpoint) {
        PlayLayer::removeCheckpoint(checkpoint);
        m_fields->m_checkpoints.erase(checkpoint);
        m_fields->m_checkpointInputs.erase(checkpoint);
        m_fields->m_checkpointFrameFixes.erase(checkpoint);
        m_fields->m_checkpointFrames.erase(checkpoint);
    }

    void resetLevel() {
        bool hadCheckpoints = m_checkpointArray->count() > 0;
        bool shouldRestoreHeldButtons = PracticeFix::shouldEnable() && hadCheckpoints;
        bool p1Holding = false;
        bool p2Holding = false;
        bool p1Left = false;
        bool p1Right = false;
        bool p2Left = false;
        bool p2Right = false;

        if (shouldRestoreHeldButtons && m_uiLayer) {
            p1Holding = m_uiLayer->m_p1Jumping || m_uiLayer->m_p1TouchId != -1;
            p2Holding = m_uiLayer->m_p2Jumping || m_uiLayer->m_p2TouchId != -1;
            if (m_player1) {
                p1Left = m_player1->m_holdingLeft || m_player1->m_holdingButtons[2];
                p1Right = m_player1->m_holdingRight || m_player1->m_holdingButtons[3];
            }
            if (m_player2) {
                p2Left = m_player2->m_holdingLeft || m_player2->m_holdingButtons[2];
                p2Right = m_player2->m_holdingRight || m_player2->m_holdingButtons[3];
            }
        }

        if (!hadCheckpoints) {
            m_fields->m_checkpoints.clear();
            m_fields->m_checkpointInputs.clear();
            m_fields->m_checkpointFrameFixes.clear();
            m_fields->m_checkpointFrames.clear();
            brokenPracticeObjects().clear();
        }
        PlayLayer::resetLevel();
        if (hadCheckpoints) {
            m_resumeTimer = 0;
            if (!m_queuedButtons.empty())
                m_queuedButtons.pop_back();
        }

        if (shouldRestoreHeldButtons) {
            auto queueIfChanged = [this](int button, bool down, bool player2) {
                auto* player = player2 ? m_player2 : m_player1;
                if (!player || player->m_holdingButtons[button] == down)
                    return;
                this->queueButton(button, down, player2, 0.0);
            };

            queueIfChanged(1, p1Holding, false);
            if (m_isPlatformer) {
                queueIfChanged(2, p1Left, false);
                queueIfChanged(3, p1Right, false);
            }

            if (m_gameState.m_isDualMode && m_levelSettings->m_twoPlayerMode) {
                queueIfChanged(1, p2Holding, true);
                if (m_isPlatformer) {
                    queueIfChanged(2, p2Left, true);
                    queueIfChanged(3, p2Right, true);
                }
            }
        }
    }

};

class $modify(FixGJBaseGameLayer, GJBaseGameLayer) {
    void destroyObject(GameObject* obj) {
        if (PracticeFix::shouldEnable() && m_isPracticeMode)
            brokenPracticeObjects().push_back(obj);

        GJBaseGameLayer::destroyObject(obj);
    }
};

class $modify(FixPlayerObject, PlayerObject) {
    static void onModify(auto& self) {
        (void)self.setHookPriority("PlayerObject::releaseAllButtons", -0x500000);
    }

    void releaseAllButtons() {
        auto* pl = PlayLayer::get();
        if (!pl) {
            PlayerObject::releaseAllButtons();
            return;
        }
        auto* gjbgl = GJBaseGameLayer::get();
        if (this == gjbgl->m_player2 && !gjbgl->m_gameState.m_isDualMode) {
            PlayerObject::releaseAllButtons();
            return;
        }
        bool isP2 = this == gjbgl->m_player2;
        bool holding = isP2
            ? (gjbgl->m_uiLayer->m_p2Jumping || gjbgl->m_uiLayer->m_p2TouchId != -1)
            : (gjbgl->m_uiLayer->m_p1Jumping || gjbgl->m_uiLayer->m_p1TouchId != -1);
        if (!holding)
            PlayerObject::releaseAllButtons();
    }
};

class $modify(FixUILayer, UILayer) {
    static void onModify(auto& self) {
        (void)self.setHookPriority("UILayer::handleKeypress", -0x500000);
    }

    void handleKeypress(enumKeyCodes key, bool down, double timestamp) {
        auto* pl = PlayLayer::get();
        bool shouldPreserve = pl
            && PracticeFix::shouldEnable()
            && down
            && key == enumKeyCodes::KEY_R
            && pl->m_checkpointArray->count() > 0;

        bool p1Jumping = m_p1Jumping;
        bool p2Jumping = m_p2Jumping;
        int p1TouchId = m_p1TouchId;
        int p2TouchId = m_p2TouchId;

        UILayer::handleKeypress(key, down, timestamp);

        if (shouldPreserve) {
            m_p1Jumping = p1Jumping;
            m_p2Jumping = p2Jumping;
            m_p1TouchId = p1TouchId;
            m_p2TouchId = p2TouchId;
        }
    }
};
