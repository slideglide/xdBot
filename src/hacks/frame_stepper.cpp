#include "../includes.hpp"
#include "../practice_fixes/practice_fixes.hpp"

#include <Geode/modify/CCParticleSystem.hpp>
#include <Geode/modify/PlayLayer.hpp>

class $modify(FrameStepperPlayLayer, PlayLayer) {
    void setupHasCompleted() {
        PlayLayer::setupHasCompleted();

        if (Global::get().frameStepper)
            Global::toggleFrameStepper();
    }

    void resetLevel() {
        if (!PracticeFix::isLoadingFrameStepperBackstep())
            PracticeFix::clearStoredFrames();
        PlayLayer::resetLevel();
        if (Global::get().frameStepper && !PracticeFix::isLoadingFrameStepperBackstep())
            PracticeFix::saveFrameStepperFrame();
    }

    void onQuit() {
        PracticeFix::clearStoredFrames();
        PlayLayer::onQuit();
    }
};

class $modify(FrameStepperParticles, CCParticleSystem) {
    void update(float dt) override {
        auto& g = Global::get();
        if (!PlayLayer::get() || !g.frameStepper)
            return CCParticleSystem::update(dt);

        if (g.stepFrameParticle > 0) {
            g.stepFrameParticle--;
            return CCParticleSystem::update(dt);
        }
    }
};
