#pragma once

#include "updater.hpp"

#include "../gdr/gdr.hpp"
#include "../practice_fixes/checkpoint.hpp"
#include "../renderer/renderer.hpp"
#include "../settings/settings.hpp"
#include <Geode/Geode.hpp>
#include <eclipse.eclipse-menu/include/config.hpp>

enum state { none, recording, playing };

enum class SaveFormat { GDR2, GDR1, JSON };

struct CheckpointData {
    int frame;
    SupplementalPlayerState p1;
    SupplementalPlayerState p2;
    uintptr_t seed;
    int previousFrame;
};

class Bot {

    Bot();

  public:
    static auto& get() {
        static Bot instance;
        return instance;
    }

    static bool hasIncompatibleMods();

    static bool enabledIncompatibleGDSettings();

    static float getTPS();

    static int getCurrentFrame(bool editor = false);

    static void updateSeed(bool isRestart = false);

    static void updatePitch(float value);

    static void toggleSpeedhack();

    static void recordAction(int frame, int button, bool player2, bool hold);

    static void recordFrameFix(int frame, PlayerObject* p1, PlayerObject* p2);

    static void autoSave(GJGameLevel* level, int number);

    static void tryAutosave(GJGameLevel* level, CheckpointObject* cp);

    static void updateMacroInfo(PlayLayer* pl);

    static void updateMacroTPS();

    static void toggleRecording();

    static void togglePlaying();

    static void resetState(bool cp = false);

    static bool shouldStep();

    static bool flipControls();

    static void frameStep();

    static void backstepFrame();

    static void syncFrameStepperMusic();

    static void toggleFrameStepper();

    Mod* mod = geode::Mod::get();
    Settings& settings = Settings::get();
    geode::Popup* layer = nullptr;

    BotUpdater updater;
    BotReplay replay;
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
    bool lockDeltaFast = false;
    bool lockDeltaRealTime = true;
    bool lockDeltaUseVisualUpdates = true;
    int lockDeltaMaxUpr = 10;
    bool stopPlaying = false;
    bool tpsEnabled = false;
    float tps = 240.f;
    bool clickBetweenFramesWasEnabled = false;
    bool clickBetweenStepsWasEnabled = false;
    bool clickBetweenFramesAutoDisabled = false;
    bool clickBetweenStepsAutoDisabled = false;
    bool disableCheckpointsWasEnabled = false;
    bool disableCheckpointsAutoDisabled = false;

    std::vector<geode::Function<void(bool)>> onTpsEnabledChanged;
    std::vector<geode::Function<void(double)>> onTpsChanged;

    void setTpsEnabled(bool enabled) {
        if (tpsEnabled == enabled)
            return;

        if (state == none)
            previousTpsEnabled = tpsEnabled;
        tpsEnabled = enabled;

        settings.save("macro_tps_enabled", enabled);

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

        settings.save("macro_tps", static_cast<double>(newTps));

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
    int attemptStartFrame = 0;
    int frameOffset = 0;
    int previousFrame = 0;

    size_t currentAction = 0;
    size_t currentFrameFix = 0;
    bool frameFixes = false;
    bool inputFixes = false;

    int currentPage = 0;
    float currentPitch = 1.f;
    uintptr_t latestSeed = 0;
    float leftOver = 0.f;
};
