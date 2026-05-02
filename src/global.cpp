#include "global.hpp"
#include "ui/game_ui.hpp"
#include "ui/record_layer.hpp"

#include <Geode/modify/CCTextInputNode.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>
#include <random>

class $modify(CCTextInputNode) {

    bool ccTouchBegan(cocos2d::CCTouch* v1, cocos2d::CCEvent* v2) {
        if (this->getID() == "disabled-input"_spr)
            return false;

        return CCTextInputNode::ccTouchBegan(v1, v2);
    }
};

struct IncompatibleSetting {
    std::string ID;
    bool incompatValue;
    bool isModToggle = false;
    bool isSavedValue = false;
};

struct IncompatibleMod {
    std::string ID;
    bool canBeDisabled;
    std::vector<IncompatibleSetting> incompatSettings;
};

const std::vector<IncompatibleMod> incompatibleMods{
    {"syzzi.click_between_frames", true, {{"soft-toggle", false, true}, {"actual-delta", true}}},
    {"alphalaneous.click_after_frames", true, {{"soft-toggle", false, true}}},
    {"thesillydoggo.qolmod", true, {{"tps-bypass_enabled", true, false, true}}}};

bool Global::hasIncompatibleMods() {
    std::vector<std::string> modsToDisable;
    std::vector<std::string> settingsToDisable;

    if (Mod* mod = Loader::get()->getLoadedMod("firee.prism")) {
        auto json = mod->getSavedValue<matjson::Value>("values");
        for (const auto& obj : json.asArray().unwrap()) {

            if (obj["name"].asString().unwrapOrDefault() != "TPS Bypass")
                continue;

            if (obj["value"].asInt().unwrapOrDefault() != 240)
                settingsToDisable.push_back("<cr>TPS Bypass (Prism Menu)</c>");

            break;
        }
    }

#ifdef GEODE_IS_WINDOWS

    if (Mod* mod = Loader::get()->getLoadedMod("tobyadd.gdh")) {
        std::filesystem::path configPath = mod->getSaveDir() / "config.json";

        if (std::filesystem::exists(configPath)) {
            auto jsonResult = geode::utils::file::readJson(configPath);
            if (jsonResult) {
                auto jsonData = jsonResult.unwrap();
                if (jsonData.contains("tps_enabled")) {
                    if (jsonData["tps_enabled"].asBool())
                        settingsToDisable.push_back("<cr>TPS Bypass (GDH)</c>");
                }
            }
        }
    }

#else

    if (Mod* mod = Loader::get()->getLoadedMod("tobyadd.gdh_mobile")) {
        std::filesystem::path configPath = mod->getSaveDir() / "config.json";

        if (std::filesystem::exists(configPath)) {
            auto jsonResult = geode::utils::file::readJson(configPath);
            if (jsonResult) {
                auto jsonData = jsonResult.unwrap();
                if (jsonData.contains("fps_value")) {
                    auto fpsValue = jsonData["fps_value"].asDouble();
                    if (fpsValue.isOk() && fpsValue.unwrap() != 240) {
                        settingsToDisable.push_back("<cr>TPS Bypass (GDH)</c>");
                    }
                }
            }
        }
    }

#endif

    for (IncompatibleMod incompatMod : incompatibleMods) {
        Mod* mod = Loader::get()->getLoadedMod(incompatMod.ID);

        if (!mod)
            continue;

        std::string modName = mod->getName();

        if (!incompatMod.canBeDisabled) {
            modsToDisable.push_back(modName);
            continue;
        }

        for (IncompatibleSetting sett : incompatMod.incompatSettings) {
            bool value = sett.isSavedValue ? mod->getSavedValue<bool>(sett.ID)
                                           : mod->getSettingValue<bool>(sett.ID);

            if (value != sett.incompatValue)
                continue;

            if (sett.isModToggle)
                modsToDisable.push_back(modName);
            else {
                std::string settName =
                    sett.isSavedValue ? sett.ID : mod->getSetting(sett.ID)->getDisplayName();
                settingsToDisable.push_back(fmt::format("{} ({})", settName, modName));
            }
        }
    }

    if (!modsToDisable.empty()) {
        geode::utils::StringBuffer incompatString;

        for (const std::string name : modsToDisable)
            incompatString.append("<cr>{}</c>{}", name, (name != modsToDisable.back() ? ", " : ""));

        FLAlertLayer::create(
            "Warning", "The following mods are incompatible: \n" + incompatString.str(), "OK")
            ->show();

    } else if (!settingsToDisable.empty()) {
        geode::utils::StringBuffer incompatString;

        for (const std::string name : settingsToDisable)
            incompatString.append(
                "<cr>{}</c>{}", name, (name != settingsToDisable.back() ? ", " : ""));

        FLAlertLayer::create("Warning",
                             "The following mod settings are incompatible: \n" +
                                 incompatString.str(),
                             "OK")
            ->show();
    }

    bool ret = !modsToDisable.empty() || !settingsToDisable.empty();

    if (ret) {
        Global::get().state = state::none;
        Interface::updateLabels();
        Interface::updateButtons();
    }

    return ret;
}

bool Global::enabledIncompatibleGDSettings() {
    std::vector<std::string> settingsToDisable;

    if (GameManager::sharedState()->getGameVariable(GameVar::ClickBetweenSteps))
        settingsToDisable.push_back("Click Between Steps");

#ifdef GEODE_IS_MOBILE
    bool cbsOverridden = false;

    if (PlayLayer* pl = PlayLayer::get()) {
        if (pl->m_level->m_cbsOverride == 1) {
            cbsOverridden = true;
        }
    }

    if (!cbsOverridden) {
        if (LevelInfoLayer* lil = LevelInfoLayer::get()) {
            if (lil->m_level->m_cbsOverride == 1) {
                cbsOverridden = true;
            }
        }
    }

    if (!cbsOverridden) {
        if (CCScene* scene = CCScene::get()) {
            for (auto obj : scene->getChildrenExt<CCObject*>()) {
                if (PlayLayer* pl = typeinfo_cast<PlayLayer*>(obj)) {
                    if (pl->m_level->m_cbsOverride == 1) {
                        cbsOverridden = true;
                        break;
                    }
                }
                if (LevelInfoLayer* lil = typeinfo_cast<LevelInfoLayer*>(obj)) {
                    if (lil->m_level->m_cbsOverride == 1) {
                        cbsOverridden = true;
                        break;
                    }
                }
            }
        }
    }

    if (cbsOverridden) {
        settingsToDisable.push_back("Click Between Steps (Level Settings Override)");
    }
#endif

    if (!settingsToDisable.empty()) {
        geode::utils::StringBuffer incompatString;

        for (const std::string& name : settingsToDisable)
            incompatString.append(
                "<cr>{}</c>{}", name, (name != settingsToDisable.back() ? ", " : ""));

        FLAlertLayer::create("Warning",
                             "The following GD settings are incompatible: \n" +
                                 incompatString.str(),
                             "OK")
            ->show();
    }

    bool ret = !settingsToDisable.empty();

    if (ret) {
        Global::get().state = state::none;
        Interface::updateLabels();
        Interface::updateButtons();
    }

    return ret;
}

float Global::getTPS() {
    auto& g = Global::get();
    return g.tpsEnabled ? g.tps : 240.f;
}

int Global::getCurrentFrame(bool editor) {
    auto& g = Global::get();
    auto* pl = PlayLayer::get();

    if (pl && g.m_frameCount > 0) {
        return g.m_frameCount - g.frameOffset;
    }

    if (!pl)
        return 0;

    if (g.macro.isLegacy) {
        return pl->m_gameState.m_currentProgress / 2 - g.frameOffset;
    }

    return pl->m_gameState.m_currentProgress - g.frameOffset;
}

class $modify(FrameCounterGJBaseGameLayer, GJBaseGameLayer) {
    void processCommands(float dt, bool isHalfTick, bool isLastTick) {
        auto& g = Global::get();
        auto* playLayer = PlayLayer::get();
        bool isPlaying = playLayer && !playLayer->m_hasCompletedLevel && !playLayer->m_isPaused &&
                         playLayer->m_gameState.m_currentProgress > 0 &&
                         !playLayer->m_player1->m_isDead &&
                         (!playLayer->m_gameState.m_isDualMode || !playLayer->m_player2->m_isDead);

        if (g.state == state::playing && !g.tpsEnabled) {
            g.setTpsEnabled(true);
        }

        GJBaseGameLayer::processCommands(dt, isHalfTick, isLastTick);

        if (isPlaying) {
            g.m_frameCount++;
        }
    }
};

void Global::updateSeed(bool isRestart) {
    auto& g = Global::get();

    if (g.seedEnabled) {
        PlayLayer* pl = PlayLayer::get();
        if (!pl)
            return;

        uint64_t seed =
            geode::utils::numFromString<uint64_t>(g.mod->getSavedValue<std::string>("macro_seed"))
                .unwrapOr(0);

        uint64_t finalSeed;

        if (!pl->m_player1->m_isDead) {
            g.gen.seed(seed + static_cast<uint64_t>(pl->m_gameState.m_currentProgress));

            finalSeed = g.gen.generate(10000, 1000000000);
        } else {
            finalSeed = geode::utils::random::generate(1000, 1000000000);
        }

        GameToolbox::fast_srand(finalSeed);
        g.safeMode = true;
    }

    if (isRestart && g.state == state::recording) {
        uint64_t gdSeed = GameToolbox::getfast_srand();
        g.macro.seed = gdSeed;

        g.gen.seed(gdSeed);
    }
}

void Global::updatePitch(float value) {
    auto& g = Global::get();
    if (!g.speedhackAudio) {
        if (g.currentPitch != 1.f)
            value = 1.f;
        else
            return;
    }

    FMODAudioEngine* fmod = FMODAudioEngine::sharedEngine();
    FMOD::ChannelGroup* channel = nullptr;
    fmod->m_system->getMasterChannelGroup(&channel);

    if (channel) {
        channel->setPitch(value);
        g.currentPitch = value;
    }
}

void Global::frameStep() {
    auto& g = Global::get();
    if (!PlayLayer::get() || !g.frameStepper)
        return;

    g.stepFrame = true;
    g.stepFrameDraw = true;
    g.stepFrameParticle = 4;
}

void Global::toggleSpeedhack() {
    auto& g = Global::get();
    g.mod->setSavedValue("macro_speedhack_enabled",
                         !g.mod->getSavedValue<bool>("macro_speedhack_enabled"));
    g.speedhackEnabled = g.mod->getSavedValue<bool>("macro_speedhack_enabled");

    if (g.layer) {
        if (static_cast<RecordLayer*>(g.layer)->speedhackToggle)
            static_cast<RecordLayer*>(g.layer)->speedhackToggle->toggle(
                g.mod->getSavedValue<bool>("macro_speedhack_enabled"));
    }

    if (!g.mod->getSavedValue<bool>("macro_speedhack_enabled"))
        Global::updatePitch(1.f);
}

void Global::toggleFrameStepper() {
    auto& g = Global::get();

    g.frameStepper = !g.frameStepper;
    g.mod->setSavedValue("macro_frame_stepper", g.frameStepper);

    if (!g.frameStepper) {
        g.stepFrame = false;
        g.stepFrameParticle = false;

        if (PlayLayer::get() && g.frameStepperMusicTime != 0) {
            FMODAudioEngine::sharedEngine()->setMusicTimeMS(g.frameStepperMusicTime, true, 0);
            g.frameStepperMusicTime = 0;
        }
    } else {
        if (PlayLayer::get())
            g.frameStepperMusicTime = FMODAudioEngine::sharedEngine()->getMusicTimeMS(0);
    }

    if (auto rl = static_cast<RecordLayer*>(g.layer)) {
        if (rl->frameStepperToggle)
            rl->frameStepperToggle->toggle(g.frameStepper);
    }

    Interface::updateButtons();
}

$execute {
    auto& g = Global::get();

    if (!g.mod->setSavedValue("defaults_set_14", true)) {
        g.mod->setSavedValue("render_fade_in_video", geode::utils::numToString(2));
        g.mod->setSavedValue("render_fade_out_video", geode::utils::numToString(2));
    }

    if (!g.mod->setSavedValue("defaults_set_12", true)) {
        g.mod->setSettingValue<std::filesystem::path>("macros_folder",
                                                      g.mod->getSaveDir() / "macros");
        g.mod->setSettingValue<std::filesystem::path>("autosaves_folder",
                                                      g.mod->getSaveDir() / "autosaves");
    }

#ifdef GEODE_IS_ANDROID

    if (!g.mod->setSavedValue("defaults_set_15", true))
        g.mod->setSavedValue("render_video_args", std::string(""));

    if (!g.mod->setSavedValue("defaults_set_11", true))
        g.mod->setSavedValue("render_codec", std::string("h264_mediacodec"));

#endif

    if (!g.mod->setSavedValue("defaults_set_10", true)) {
        g.mod->setSettingValue("restore_page", true);

        g.mod->setSavedValue("autosave_interval_enabled", false);
        g.mod->setSavedValue("autosave_interval", geode::utils::numToString(10));
        g.mod->setSavedValue("autosave_checkpoint_enabled", true);
        g.mod->setSavedValue("autosave_levelend_enabled", true);

        g.mod->setSavedValue("render_fade_in_video", geode::utils::numToString(2));
        g.mod->setSavedValue("render_fade_out_video", geode::utils::numToString(2));

        g.mod->setSavedValue("macro_auto_stop_playing", false);
        g.mod->setSavedValue("macro_tps", 240.f);
        g.mod->setSavedValue("macro_tps_enabled", false);

        g.mod->setSavedValue("autoclicker_hold_for", 5);
        g.mod->setSavedValue("autoclicker_release_for", 5);
        g.mod->setSavedValue("autoclicker_hold_for2", 5);
        g.mod->setSavedValue("autoclicker_release_for2", 5);
        g.mod->setSavedValue("autoclicker_p1", true);
        g.mod->setSavedValue("autoclicker_p2", true);

        g.mod->setSavedValue("trajectory_color1", ccc3(74, 226, 85));
        g.mod->setSavedValue("trajectory_color2", ccc3(130, 8, 8));
        g.mod->setSavedValue("trajectory_length", geode::utils::numToString(240));
    }

    if (!g.mod->setSavedValue("defaults_set3", true)) {
        g.mod->setSettingValue<std::filesystem::path>("render_folder",
                                                      g.mod->getSaveDir() / "renders");
        g.mod->setSavedValue("render_file_extension", std::string(".mp4"));
        g.mod->setSavedValue("render_sfx_volume", 1.f);
        g.mod->setSavedValue("render_music_volume", 1.f);
        g.mod->setSavedValue("respawn_time", 0.5f);
        g.mod->setSavedValue("render_seconds_after", geode::utils::numToString(2));
        g.mod->setSavedValue("render_args", std::string("-pix_fmt yuv420p"));
        g.mod->setSavedValue("macro_noclip_p1", true);
        g.mod->setSavedValue("macro_noclip_p2", true);

        g.mod->setSavedValue("render_width2", geode::utils::numToString(1920));
        g.mod->setSavedValue("render_height", geode::utils::numToString(1080));
        g.mod->setSavedValue("render_bitrate", geode::utils::numToString(12));
        g.mod->setSavedValue("render_fps", geode::utils::numToString(60));
        g.mod->setSavedValue("render_video_args",
                             std::string("colorspace=all=bt709:iall=bt470bg:fast=1"));

        g.mod->setSavedValue("render_codec", std::string("libx264"));
#ifdef GEODE_IS_WINDOWS
        g.mod->setSettingValue("ffmpeg_path", geode::dirs::getGameDir() / "ffmpeg.exe");
#endif

        g.mod->setSavedValue("render_hide_labels", true);

        g.mod->setSavedValue("macro_seed", geode::utils::numToString(1));
        g.mod->setSavedValue("macro_speedhack", std::string("0.5"));
        g.mod->setSavedValue("macro_fps", 3);

        g.mod->setSavedValue("macro_ignore_inputs", true);
        g.mod->setSavedValue("macro_auto_safe_mode", true);
        g.mod->setSavedValue("macro_speedhack_audio", true);
        g.mod->setSavedValue("macro_show_frame_label", false);
        g.mod->setSavedValue("macro_hide_playing_label", true);

        g.mod->setSavedValue("menu_show_button", true);
        g.mod->setSavedValue("menu_pause_on_open", false);
        g.mod->setSavedValue("menu_show_cursor", true);

#ifdef GEODE_IS_MOBILE
        g.mod->setSavedValue("menu_show_cursor", false);
#endif
    }

    g.showTrajectory = g.mod->getSavedValue<bool>("macro_show_trajectory");
    g.coinFinder = g.mod->getSavedValue<bool>("macro_coin_finder");
    g.frameStepper = g.mod->getSavedValue<bool>("macro_frame_stepper");
    g.seedEnabled = g.mod->getSavedValue<bool>("macro_seed_enabled");
    g.frameLabel = g.mod->getSavedValue<bool>("macro_show_frame_label");
    g.speedhackAudio = g.mod->getSavedValue<bool>("macro_speedhack_audio");
    g.trajectoryBothSides = g.mod->getSavedValue<bool>("macro_trajectory_both_sides");
    g.p2mirror = g.mod->getSavedValue<bool>("p2_input_mirror");
    g.tpsEnabled = g.mod->getSavedValue<bool>("macro_tps_enabled");
    g.tps = g.mod->getSavedValue<double>("macro_tps");

    if (Loader::get()->getLoadedMod("eclipse.eclipse-menu")) {
        geode::queueInMainThread([&g] {
            eclipse::config::setInternal("global.tpsbypass.toggle", g.tpsEnabled);
            eclipse::config::setInternal("global.tpsbypass", static_cast<double>(g.tps));
        });
    }
    g.autoclicker = g.mod->getSavedValue<bool>("autoclicker_enabled");
    g.autoclickerP1 = g.mod->getSavedValue<bool>("autoclicker_p1");
    g.autoclickerP2 = g.mod->getSavedValue<bool>("autoclicker_p2");
    g.disableShaders = g.mod->getSavedValue<bool>("disableShaders");
    g.autosaveIntervalEnabled = g.mod->getSavedValue<bool>("autosave_interval_enabled");
    g.autosaveEnabled = g.mod->getSavedValue<bool>("macro_auto_save");

    g.holdFor = g.mod->getSavedValue<int64_t>("autoclicker_hold_for");
    g.releaseFor = g.mod->getSavedValue<int64_t>("autoclicker_release_for");
    g.holdFor2 = g.mod->getSavedValue<int64_t>("autoclicker_hold_for2");
    g.releaseFor2 = g.mod->getSavedValue<int64_t>("autoclicker_release_for2");
    g.currentPage = g.mod->getSavedValue<int64_t>("current_page");

    g.autosaveInterval =
        (geode::utils::numFromString<float>(g.mod->getSavedValue<std::string>("autosave_interval"))
             .unwrapOr(0.f) *
         60);

    g.speedhackEnabled = false;
    g.mod->setSavedValue("macro_speedhack_enabled", false);

    g.frameOffset = g.mod->getSettingValue<int64_t>("frame_offset");
    g.frameFixesLimit = g.mod->getSettingValue<int64_t>("frame_fixes_limit");
    g.lockDelta = g.mod->getSettingValue<bool>("lock_delta");
    g.stopPlaying = g.mod->getSettingValue<bool>("auto_stop_playing");

    if (g.mod->getSettingValue<std::string>("macro_accuracy") == "Frame Fixes")
        g.frameFixes = true;
    else if (g.mod->getSettingValue<std::string>("macro_accuracy") == "Input Fixes")
        g.inputFixes = true;

    g.macro.author = "N/A";
    g.macro.description = "N/A";
    g.macro.gameVersion =
        static_cast<int>(std::round(static_cast<double>(GEODE_GD_VERSION) * 1000.0));
};
