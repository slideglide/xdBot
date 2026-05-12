#pragma once

#include "macro.hpp"
#include "renderer/renderer.hpp"
#include <Geode/Geode.hpp>
#include <eclipse.eclipse-menu/include/config.hpp>

class Global {

    Global() {}

  public:
    static auto& get() {
        static Global instance;
        return instance;
    }

    static bool hasIncompatibleMods();

    static bool enabledIncompatibleGDSettings();

    static float getTPS();

    static int getCurrentFrame(bool editor = false);

    static void updateSeed(bool isRestart = false);

    static void updatePitch(float value);

    static void toggleSpeedhack();

    static void frameStep();

    static void backstepFrame();

    static void toggleFrameStepper();

    Mod* mod = geode::Mod::get();
    geode::Popup* layer = nullptr;

    Macro macro;
#ifndef GEODE_IS_IOS
    Renderer renderer;
#endif
    state state = none;

    geode::utils::random::Generator gen;
    std::unordered_map<CheckpointObject*, CheckpointData> checkpoints;
    std::unordered_set<int> allKeybinds;
    std::unordered_set<int> playedFrames;
    std::vector<int> keybinds[6];

    int lastAutoSaveFrame = 0;
    asp::Instant lastAutoSaveMS = asp::Instant::now();
    int currentSession = 0;

    bool stepFrame = false;
    bool suppressNextFrameStep = false;
    bool stepFrameDraw = false;
    int stepFrameDrawMultiple = 0;
    int stepFrameParticle = 0;
    int frameStepperMusicTime = 0;

    bool cancelCheckpoint = false;
    bool ignoreRecordAction = false;
    bool restart = false;
    bool restartLater = false;
    bool creatingTrajectory = false;
    bool firstAttempt = false;

    bool disableShaders = false;
    bool safeMode = false;
    bool layoutMode = false;
    bool showTrajectory = false;
    bool coinFinder = false;
    bool frameStepper = false;
    bool speedhackEnabled = false;
    bool speedhackAudio = false;
    bool seedEnabled = false;
    bool clickbotEnabled = false;
    bool clickbotOnlyPlaying = false;
    bool clickbotOnlyHolding = false;
    bool frameLabel = false;
    bool trajectoryBothSides = false;
    bool p2mirror = false;
    bool lockDelta = false;
    bool stopPlaying = false;
    bool tpsEnabled = false;
    float tps = 240.f;

    std::vector<geode::Function<void(bool)>> onTpsEnabledChanged;
    std::vector<geode::Function<void(double)>> onTpsChanged;

    void setTpsEnabled(bool enabled) {
        if (tpsEnabled == enabled)
            return;

        if (state == none)
            previousTpsEnabled = tpsEnabled;
        tpsEnabled = enabled;

        mod->setSavedValue("macro_tps_enabled", enabled);

        for (auto& cb : onTpsEnabledChanged)
            cb(enabled);

        if (Loader::get()->getLoadedMod("eclipse.eclipse-menu")) {
            eclipse::config::setInternal("global.tpsbypass.toggle", enabled);
        }
    }

    void setTps(float newTps) {
        if (tps == newTps)
            return;

        if (state == none)
            previousTps = tps;
        tps = newTps;

        mod->setSavedValue("macro_tps", static_cast<double>(newTps));

        for (auto& cb : onTpsChanged)
            cb(static_cast<double>(newTps));

        if (Loader::get()->getLoadedMod("eclipse.eclipse-menu")) {
            eclipse::config::setInternal("global.tpsbypass", static_cast<double>(newTps));
        }
    }

    bool previousTpsEnabled = false;
    float previousTps = 0.f;
    bool autoclicker = false;
    bool autoclickerP1 = false;
    bool alwaysPracticeFixes = false;
    bool autoclickerP2 = false;
    int holdFor = 0;
    int releaseFor = 0;
    int holdFor2 = 0;
    int releaseFor2 = 0;
    bool autosaveIntervalEnabled = false;
    int autosaveInterval = 600000;
    float autosaveCheck = 2.f;
    bool autosaveEnabled = false;

    int respawnFrame = -1;
    int frameOffset = 0;
    int previousFrame = 0;

    int m_frameCount = 0;
    double schedulerOverflow = 0.0;
    int schedulerStepCount = 1;
    bool schedulerUpdating = false;
    bool schedulerFrozenUpdate = false;
    std::vector<input> postUpdateInputs;

    size_t currentAction = 0;
    size_t currentFrameFix = 0;
    bool frameFixes = false;
    bool inputFixes = false;

    int currentPage = 0;
    float currentPitch = 1.f;
    uintptr_t latestSeed = 0;
    float leftOver = 0.f;
};
