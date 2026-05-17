#pragma once

#include <functional>

class BotUpdater {
public:
    using SchedulerUpdate = std::function<void(float)>;

    void runScheduler(float dt, SchedulerUpdate const& update);
    void resetStepState();

    bool isFrozenUpdate() const {
        return frozenUpdate;
    }

    double overflow = 0.0;
    int frameCount = 0;
    int stepCount = 1;
    bool updating = false;
    bool frozenUpdate = false;
};
