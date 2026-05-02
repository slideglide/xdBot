#pragma once

#include "macro.hpp"
#include "practice_fixes/practice_fixes.hpp"
#include "renderer/renderer.hpp"
#include <Geode/Geode.hpp>
#include <eclipse.eclipse-menu/include/config.hpp>

class Global {

#define DESELECT_INPUT(node)           \
    if (node) {                        \
        node->onClickTrackNode(false); \
        node->setDelegate(nullptr);    \
    }

    Global() {}

  public:
    static auto& get() {
        static Global instance;
        return instance;
    }

    // @brief Determine whether an incompatible mod or an incompatible setting
    // in a mod is enabled
    static bool hasIncompatibleMods();

    // @brief Determine whether an incompatible Geometry Dash setting is enabled
    static bool enabledIncompatibleGDSettings();

    // @brief Get the TPS value set in the mod
    static float getTPS();

    // @brief Get the current frame in the gameplay
    static int getCurrentFrame(bool editor = false);

    // @brief Update the RNG seed
    static void updateSeed(bool isRestart = false);

    // @brief Update audio pitch
    static void updatePitch(float value);

    // @brief Toggle, well, speedhack
    static void toggleSpeedhack();

    // @brief Step a frame
    static void frameStep();

    // @brief Toggle, well, frame stepper
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
    tpsEnabled = enabled;
    mod->setSavedValue("macro_tps_enabled", enabled);

    if (Loader::get()->getLoadedMod("eclipse.eclipse-menu")) {
        eclipse::config::setInternal("global.tpsbypass.toggle", enabled);
    } else {
        for (auto& cb : onTpsEnabledChanged)
            cb(enabled);
    }
}

void setTps(float newTps) {
    tps = newTps;
    mod->setSavedValue("macro_tps", static_cast<double>(newTps));
    if (Loader::get()->getLoadedMod("eclipse.eclipse-menu")) {
        eclipse::config::setInternal("global.tpsbypass", static_cast<double>(newTps));
    } else {
        for (auto& cb : onTpsChanged)
            cb(static_cast<double>(newTps));
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

    size_t currentAction = 0;
    size_t currentFrameFix = 0;
    int frameFixesLimit = 240;
    bool frameFixes = false;
    bool inputFixes = false;

    int currentPage = 0;
    float currentPitch = 1.f;
    uintptr_t latestSeed = 0;
    float leftOver = 0.f;
};