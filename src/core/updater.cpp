#include "updater.hpp"

#include "bot.hpp"
#include "../hacks/show_trajectory.hpp"
#include "../practice_fixes/practice_fixes.hpp"

#include <Geode/Prelude.hpp>
#include <Geode/binding/GameManager.hpp>
#include <Geode/binding/PlayLayer.hpp>
#include <Geode/binding/PlayerObject.hpp>
#include <Geode/modify/CCScheduler.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

using namespace geode::prelude;

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

bool isRendererRecording(Bot const& bot) {
#ifdef GEODE_IS_IOS
    return false;
#else
    return bot.renderer.recording;
#endif
}

bool useFastLockDelta(Bot const& bot) {
    return bot.lockDelta && bot.lockDeltaFast && bot.state == state::playing && !isRendererRecording(bot);
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

float speedhackFor(Bot& bot) {
    if (!bot.speedhackEnabled || bot.frameStepper)
        return 1.f;

    std::string speedhackValue = bot.mod->getSavedValue<std::string>("macro_speedhack");
    if (speedhackValue == "0.0" || speedhackValue.empty()) {
        Bot::updatePitch(1.f);
        return 1.f;
    }

    float speedhack = geode::utils::numFromString<float>(speedhackValue).unwrap();
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
    Bot::updatePitch(speedhack);
    return speedhack;
}

}

void BotUpdater::resetStepState() {
    overflow = 0.0;
    stepCount = 1;
}

void BotUpdater::runScheduler(float dt, SchedulerUpdate const& update) {
    auto& bot = Bot::get();

    if (updating)
        return update(dt);

    if (bot.state == state::playing && !bot.tpsEnabled && bot.replay.framerate != 240.f)
        bot.setTpsEnabled(true);

    if (bot.state == state::none && !bot.speedhackEnabled && !bot.tpsEnabled && !bot.lockDelta &&
        !bot.frameStepper) {
        if (bot.currentPitch != 1.f)
            Bot::updatePitch(1.f);

        update(dt);
        ShowTrajectory::refreshIfNeeded();
        return;
    }

    if (isRendererRecording(bot)) {
        if (bot.currentPitch != 1.f)
            Bot::updatePitch(1.f);

        update(dt);
        ShowTrajectory::refreshIfNeeded();
        return;
    }

    float speedhack = speedhackFor(bot);
    if (speedhack != 1.f && PlayLayer::get())
        bot.safeMode = true;

    auto* pl = PlayLayer::get();
    if (!pl || pl->m_isPaused) {
        resetStepState();
        update(dt * speedhack);
        ShowTrajectory::refreshIfNeeded();
        return;
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
            updating = true;
            frozenUpdate = true;
            stepCount = 1;
            update(0.0f);
            stepCount = 1;
            frozenUpdate = false;
            updating = false;
            ShowTrajectory::refreshIfNeeded();
            return;
        }

        bot.stepFrame = false;
        resetStepState();
        updating = true;
        stepCount = 1;
        update(static_cast<float>(timestep));
        stepCount = 1;
        updating = false;
        PracticeFix::saveFrameStepperFrame();
        Bot::syncFrameStepperMusic();
        ShowTrajectory::refreshIfNeeded();
        return;
    }

    if (!bot.lockDelta) {
        resetStepState();
        update(dt * speedhack);
        ShowTrajectory::refreshIfNeeded();
        return;
    }

    overflow += static_cast<double>(dt) * timeWarp * speedhack;
    int steps = static_cast<int>(std::floor(overflow / timestep));

    int stepLimit = std::max(bot.lockDeltaMaxUpr, 1);
    if (bot.lockDeltaUseVisualUpdates) {
        float fps = GameManager::get()->m_customFPSTarget;
        if (fps <= 10.0f)
            fps = 240.0f;

        int modifier = static_cast<int>(Bot::getTPS()) / std::max(static_cast<int>(fps), 1);
        stepLimit *= std::max(1, modifier);
    }

    if (!bot.lockDeltaRealTime)
        steps = std::min(steps, stepLimit);

    if (steps <= 0) {
        ShowTrajectory::refreshIfNeeded();
        return;
    }

    overflow -= static_cast<double>(steps) * timestep;
    if (!bot.lockDeltaRealTime && steps == stepLimit)
        overflow = 0.0;

    updating = true;
    auto runUpdate = [&](int nextStepCount, double delta) {
        if (nextStepCount <= 0)
            return;

        stepCount = nextStepCount;
        update(static_cast<float>(delta));
        stepCount = 1;
    };

    if (useFastLockDelta(bot)) {
        runFastLockDeltaUpdates(bot, steps, physicsDt, runUpdate);
    } else {
        for (int i = 0; i < steps; i++)
            runUpdate(1, physicsDt);
    }

    updating = false;
    ShowTrajectory::refreshIfNeeded();
}

class $modify(CCScheduler) {
    void update(float dt) {
        Bot::get().updater.runScheduler(dt, [&](float updateDt) {
            CCScheduler::update(updateDt);
        });
    }
};
