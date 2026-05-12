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

struct HeldButtonState {
    bool p1Holding = false;
    bool p2Holding = false;
    bool p1Left = false;
    bool p1Right = false;
    bool p2Left = false;
    bool p2Right = false;
    bool valid = false;

    void capture(PlayLayer* pl) {
        valid = false;
        if (!pl || !pl->m_uiLayer)
            return;

        p1Holding = pl->m_uiLayer->m_p1Jumping || pl->m_uiLayer->m_p1TouchId != -1;
        p2Holding = pl->m_uiLayer->m_p2Jumping || pl->m_uiLayer->m_p2TouchId != -1;
        if (pl->m_player1) {
            p1Left = pl->m_player1->m_holdingLeft || pl->m_player1->m_holdingButtons[2];
            p1Right = pl->m_player1->m_holdingRight || pl->m_player1->m_holdingButtons[3];
        }
        if (pl->m_player2) {
            p2Left = pl->m_player2->m_holdingLeft || pl->m_player2->m_holdingButtons[2];
            p2Right = pl->m_player2->m_holdingRight || pl->m_player2->m_holdingButtons[3];
        }
        valid = true;
    }
};

static HeldButtonState& heldButtonState() {
    static HeldButtonState state;
    return state;
}

struct PracticeCheckpointData {
    CheckpointObject* checkpoint = nullptr;
    geode::Ref<CheckpointObject> retainedCheckpoint;
    SupplementalPlayerState p1, p2;
    SupplementalPlayLayerState pl;
    double tps = 240.0;
    bool tpsEnabled = false;
    int frame = 0;
    int previousFrame = 0;
    int respawnFrame = -1;
    int frameOffset = 0;
    double schedulerOverflow = 0.0;
    size_t currentAction = 0;
    size_t currentFrameFix = 0;
    std::vector<input> inputs;
    std::vector<gdr_legacy::FrameFix> frameFixes;
    gd::unordered_map<int, int> persistentItemMap;
    std::array<float, 2000> varianceValues = {};
    std::vector<GameObject*> calcNonEffectObjects;
    int calcNonEffectObjectsSize = 0;
    std::vector<GameObject*> brokenObjects;
    uint64_t randomSeed = 0;

    PracticeCheckpointData() = default;
    PracticeCheckpointData(
        CheckpointObject* checkpointObj,
        PlayerObject* p1Obj,
        PlayerObject* p2Obj,
        PlayLayer* plObj,
        std::vector<GameObject*> const& broken
    ) {
        if (!plObj || !p1Obj)
            return;
        checkpoint = checkpointObj;
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
        frame = Global::getCurrentFrame();
        previousFrame = g.previousFrame;
        respawnFrame = g.respawnFrame;
        frameOffset = g.frameOffset;
        schedulerOverflow = g.schedulerOverflow;
        currentAction = g.currentAction;
        currentFrameFix = g.currentFrameFix;
        inputs = g.macro.inputs;
        frameFixes = g.macro.frameFixes;
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

    void applyMacroState() const {
        auto& g = Global::get();
        bool previousIgnore = g.ignoreRecordAction;
        g.ignoreRecordAction = true;

        if (g.state == state::recording) {
            g.macro.inputs = inputs;
            g.macro.frameFixes = frameFixes;
        }

        g.m_frameCount = frame;
        g.previousFrame = previousFrame;
        g.respawnFrame = respawnFrame;
        g.frameOffset = frameOffset;
        g.schedulerOverflow = schedulerOverflow;

        int targetFrame = frame - frameOffset;
        g.currentAction = 0;
        while (g.currentAction < g.macro.inputs.size() &&
               g.macro.inputs[g.currentAction].frame < targetFrame)
            g.currentAction++;

        g.currentFrameFix = 0;
        while (g.currentFrameFix < g.macro.frameFixes.size() &&
               g.macro.frameFixes[g.currentFrameFix].frame < targetFrame)
            g.currentFrameFix++;

        g.ignoreRecordAction = previousIgnore;
    }

    CheckpointData toGlobalCheckpoint() const {
        return CheckpointData{
            frame,
            p1,
            p2,
            static_cast<uintptr_t>(randomSeed),
            previousFrame
        };
    }
};

static std::deque<PracticeCheckpointData>& storedFrameStepperFrames() {
    static std::deque<PracticeCheckpointData> frames;
    return frames;
}

static bool& loadingFrameStepperBackstep() {
    static bool loading = false;
    return loading;
}

static std::optional<PracticeCheckpointData>& frameStepperBackstepState() {
    static std::optional<PracticeCheckpointData> state;
    return state;
}

void PracticeFix::clearStoredFrames() {
    if (PracticeFix::isLoadingFrameStepperBackstep())
        return;

    storedFrameStepperFrames().clear();
}

void PracticeFix::saveFrameStepperFrame() {
    if (PracticeFix::isLoadingFrameStepperBackstep())
        return;

    auto* pl = PlayLayer::get();
    if (!pl || !pl->m_player1 || pl->m_player1->m_isDead || pl->m_isPaused)
        return;

    auto& frames = storedFrameStepperFrames();
    constexpr size_t maxStoredFrames = 600;
    while (frames.size() >= maxStoredFrames)
        frames.pop_front();

    auto* checkpoint = pl->createCheckpoint();
    if (!checkpoint)
        return;

    frames.emplace_back(
        checkpoint,
        pl->m_player1,
        pl->m_gameState.m_isDualMode ? pl->m_player2 : nullptr,
        pl,
        brokenPracticeObjects()
    );
    frames.back().retainedCheckpoint.swap(checkpoint);
}

bool PracticeFix::isLoadingFrameStepperBackstep() {
    return loadingFrameStepperBackstep();
}

bool PracticeFix::applyFrameStepperBackstep(CheckpointObject* checkpoint) {
    auto* pl = PlayLayer::get();
    auto& state = frameStepperBackstepState();
    if (!pl || !state || checkpoint != state->checkpoint)
        return false;

    pl->PlayLayer::loadFromCheckpoint(checkpoint);
    state->apply(pl->m_player1, pl->m_gameState.m_isDualMode ? pl->m_player2 : nullptr, pl);
    state->applyMacroState();

    if (pl->m_player1)
        pl->m_player1->m_isDead = false;
    if (pl->m_player2)
        pl->m_player2->m_isDead = false;

    return true;
}

bool PracticeFix::backstepFrame(int frameCount) {
    auto* pl = PlayLayer::get();
    if (!pl || frameCount <= 0)
        return false;

    auto& frames = storedFrameStepperFrames();
    if (frames.size() < 2)
        return false;

    while (frameCount-- > 0 && frames.size() > 1)
        frames.pop_back();

    frameStepperBackstepState() = frames.back();
    loadingFrameStepperBackstep() = true;
    pl->m_checkpointArray->addObject(frames.back().checkpoint);
    pl->resetLevel();
    if (pl->m_checkpointArray->count() > 0 &&
        pl->m_checkpointArray->lastObject() == frames.back().checkpoint)
        pl->m_checkpointArray->removeLastObject();
    loadingFrameStepperBackstep() = false;
    frameStepperBackstepState().reset();

    return true;
}

class $modify(FixPlayLayer, PlayLayer) {
    struct Fields {
        std::vector<PracticeCheckpointData> m_checkpoints;

        PracticeCheckpointData* findCheckpoint(CheckpointObject* checkpoint) {
            auto it = std::find_if(m_checkpoints.begin(), m_checkpoints.end(),
                [checkpoint](PracticeCheckpointData const& data) {
                    return data.checkpoint == checkpoint;
                });
            return it == m_checkpoints.end() ? nullptr : &*it;
        }

        void removeCheckpoint(CheckpointObject* checkpoint) {
            m_checkpoints.erase(std::remove_if(m_checkpoints.begin(), m_checkpoints.end(),
                [checkpoint](PracticeCheckpointData const& data) {
                    return data.checkpoint == checkpoint;
                }),
                m_checkpoints.end());
        }
    };

    static void onModify(auto& self) {
        constexpr int32_t firstPriority = -0x500000;
        (void)self.setHookPriority("PlayLayer::loadFromCheckpoint", firstPriority);
        (void)self.setHookPriority("PlayLayer::resetLevel", firstPriority);
    }

    void loadFromCheckpoint(CheckpointObject* checkpoint) {
        if (PracticeFix::isLoadingFrameStepperBackstep() &&
            PracticeFix::applyFrameStepperBackstep(checkpoint))
            return;

        bool shouldFix = PracticeFix::shouldEnable();
        auto& g = Global::get();
        bool wasRecordingOrPlaying = g.state == state::recording || g.state == state::playing;
        std::optional<PracticeCheckpointData> snapshot;
        if (auto* data = m_fields->findCheckpoint(checkpoint))
            snapshot = *data;

        Macro::tryAutosave(m_level, checkpoint);

        if (shouldFix) {
            if (m_player1) m_player1->m_isDashing = false;
            if (m_gameState.m_isDualMode && m_player2) m_player2->m_isDashing = false;
        }

        PlayLayer::loadFromCheckpoint(checkpoint);
        resetTPSBypassState();

        if (snapshot) {
            if (shouldFix)
                snapshot->apply(m_player1, m_gameState.m_isDualMode ? m_player2 : nullptr, this);
            if (wasRecordingOrPlaying)
                snapshot->applyMacroState();
        }
    }

    CheckpointObject* createCheckpoint() {
        auto* checkpoint = PlayLayer::createCheckpoint();
        if (!checkpoint) return checkpoint;
        bool shouldSave = PracticeFix::shouldEnable() || Global::get().state == state::recording || Global::get().state == state::playing;
        if (shouldSave) {
            PracticeCheckpointData data(
                checkpoint,
                m_player1,
                m_gameState.m_isDualMode ? m_player2 : nullptr,
                this,
                brokenPracticeObjects()
            );
            m_fields->removeCheckpoint(checkpoint);
            m_fields->m_checkpoints.push_back(data);

            auto& g = Global::get();
            if (g.state == state::recording)
                g.checkpoints[checkpoint] = data.toGlobalCheckpoint();
        }
        return checkpoint;
    }

    void removeCheckpoint(CheckpointObject* checkpoint) {
        PlayLayer::removeCheckpoint(checkpoint);
        m_fields->removeCheckpoint(checkpoint);
        Global::get().checkpoints.erase(checkpoint);
    }

    void resetLevel() {
        bool hadCheckpoints = m_checkpointArray->count() > 0;
        bool shouldRestoreHeldButtons = PracticeFix::shouldEnable() && hadCheckpoints;
        auto held = heldButtonState();

        if (shouldRestoreHeldButtons && !held.valid) {
            held.capture(this);
        }

        if (!hadCheckpoints && !PracticeFix::isLoadingFrameStepperBackstep()) {
            m_fields->m_checkpoints.clear();
            Global::get().checkpoints.clear();
            brokenPracticeObjects().clear();
            PracticeFix::clearStoredFrames();
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

            queueIfChanged(1, held.p1Holding, false);
            if (m_isPlatformer) {
                queueIfChanged(2, held.p1Left, false);
                queueIfChanged(3, held.p1Right, false);
            }

            if (m_gameState.m_isDualMode && m_levelSettings->m_twoPlayerMode) {
                queueIfChanged(1, held.p2Holding, true);
                if (m_isPlatformer) {
                    queueIfChanged(2, held.p2Left, true);
                    queueIfChanged(3, held.p2Right, true);
                }
            }
        }

        heldButtonState().valid = false;
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
        auto& held = heldButtonState();
        if (this == gjbgl->m_player2 && !gjbgl->m_gameState.m_isDualMode) {
            PlayerObject::releaseAllButtons();
            return;
        }
        bool isP2 = this == gjbgl->m_player2;
        bool holding = held.valid
            ? (isP2 ? held.p2Holding : held.p1Holding)
            : (isP2
                ? (gjbgl->m_uiLayer->m_p2Jumping || gjbgl->m_uiLayer->m_p2TouchId != -1)
                : (gjbgl->m_uiLayer->m_p1Jumping || gjbgl->m_uiLayer->m_p1TouchId != -1));
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

        if (shouldPreserve)
            heldButtonState().capture(pl);

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
