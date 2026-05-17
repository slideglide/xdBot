#include "settings.hpp"

#include "../core/bot.hpp"

Settings& Settings::get() {
    static Settings instance;
    return instance;
}

Mod* Settings::mod() const {
    return Mod::get();
}

void Settings::applyDefaults() const {
    auto* mod = this->mod();

    if (!mod->setSavedValue("defaults_set_14", true)) {
        mod->setSavedValue("render_fade_in_video", geode::utils::numToString(2));
        mod->setSavedValue("render_fade_out_video", geode::utils::numToString(2));
    }

    if (!mod->setSavedValue("defaults_set_12", true)) {
        setValue("macros_folder", mod->getSaveDir() / "macros");
        setValue("autosaves_folder", mod->getSaveDir() / "autosaves");
    }

#ifdef GEODE_IS_ANDROID
    if (!mod->setSavedValue("defaults_set_15", true))
        mod->setSavedValue("render_video_args", std::string(""));

    if (!mod->setSavedValue("defaults_set_11", true))
        mod->setSavedValue("render_codec", std::string("h264_mediacodec"));
#endif

    if (!mod->setSavedValue("defaults_set_10", true)) {
        setValue("restore_page", true);

        mod->setSavedValue("autosave_interval_enabled", false);
        mod->setSavedValue("autosave_interval", geode::utils::numToString(10));
        mod->setSavedValue("autosave_checkpoint_enabled", true);
        mod->setSavedValue("autosave_levelend_enabled", true);

        mod->setSavedValue("render_fade_in_video", geode::utils::numToString(2));
        mod->setSavedValue("render_fade_out_video", geode::utils::numToString(2));

        mod->setSavedValue("macro_auto_stop_playing", false);
        mod->setSavedValue("macro_tps", 240.f);
        mod->setSavedValue("macro_tps_enabled", false);

        mod->setSavedValue("autoclicker_hold_for", 5);
        mod->setSavedValue("autoclicker_release_for", 5);
        mod->setSavedValue("autoclicker_hold_for2", 5);
        mod->setSavedValue("autoclicker_release_for2", 5);
        mod->setSavedValue("autoclicker_p1", true);
        mod->setSavedValue("autoclicker_p2", true);

        mod->setSavedValue("trajectory_color1", ccc3(74, 226, 85));
        mod->setSavedValue("trajectory_color2", ccc3(130, 8, 8));
        mod->setSavedValue("trajectory_length", geode::utils::numToString(240));
    }

    if (!mod->setSavedValue("defaults_set3", true)) {
        setValue("render_folder", mod->getSaveDir() / "renders");
        mod->setSavedValue("render_file_extension", std::string(".mp4"));
        mod->setSavedValue("render_sfx_volume", 1.f);
        mod->setSavedValue("render_music_volume", 1.f);
        mod->setSavedValue("respawn_time", 0.5f);
        mod->setSavedValue("render_seconds_after", geode::utils::numToString(2));
        mod->setSavedValue("render_args", std::string("-pix_fmt yuv420p"));
        mod->setSavedValue("macro_noclip_p1", true);
        mod->setSavedValue("macro_noclip_p2", true);

        mod->setSavedValue("render_width2", geode::utils::numToString(1920));
        mod->setSavedValue("render_height", geode::utils::numToString(1080));
        mod->setSavedValue("render_bitrate", geode::utils::numToString(12));
        mod->setSavedValue("render_fps", geode::utils::numToString(60));
        mod->setSavedValue("render_video_args",
                           std::string("colorspace=all=bt709:iall=bt470bg:fast=1"));

        mod->setSavedValue("render_codec", std::string("libx264"));
#ifdef GEODE_IS_WINDOWS
        setValue("ffmpeg_path", geode::dirs::getGameDir() / "ffmpeg.exe");
#endif

        mod->setSavedValue("render_hide_labels", true);

        mod->setSavedValue("macro_seed", geode::utils::numToString(1));
        mod->setSavedValue("macro_speedhack", std::string("0.5"));
        mod->setSavedValue("macro_fps", 3);

        mod->setSavedValue("macro_ignore_inputs", true);
        mod->setSavedValue("macro_auto_safe_mode", true);
        mod->setSavedValue("macro_speedhack_audio", true);
        mod->setSavedValue("macro_show_frame_label", false);
        mod->setSavedValue("macro_hide_playing_label", true);

        mod->setSavedValue("menu_show_button", true);
        mod->setSavedValue("menu_pause_on_open", false);
        mod->setSavedValue("menu_show_cursor", true);

#ifdef GEODE_IS_MOBILE
        mod->setSavedValue("menu_show_cursor", false);
#endif
    }
}

void Settings::loadRuntimeState(Bot& bot) const {
    bot.showTrajectory = saved<bool>("macro_show_trajectory");
    bot.coinFinder = saved<bool>("macro_coin_finder");
    bot.frameStepper = saved<bool>("macro_frame_stepper");
    bot.seedEnabled = saved<bool>("macro_seed_enabled");
    bot.frameLabel = saved<bool>("macro_show_frame_label");
    bot.speedhackAudio = saved<bool>("macro_speedhack_audio");
    bot.trajectoryBothSides = saved<bool>("macro_trajectory_both_sides");
    bot.p2mirror = saved<bool>("p2_input_mirror");
    bot.tpsEnabled = saved<bool>("macro_tps_enabled");
    bot.tps = saved<double>("macro_tps");

    if (Loader::get()->getLoadedMod("eclipse.eclipse-menu")) {
        geode::queueInMainThread([&bot] {
            eclipse::config::setInternal("global.tpsbypass.toggle", bot.tpsEnabled);
            eclipse::config::setInternal("global.tpsbypass", static_cast<double>(bot.tps));
        });
    }

    bot.autoclicker = saved<bool>("autoclicker_enabled");
    bot.autoclickerP1 = saved<bool>("autoclicker_p1");
    bot.autoclickerP2 = saved<bool>("autoclicker_p2");
    bot.disableShaders = saved<bool>("disableShaders");
    bot.autosaveIntervalEnabled = saved<bool>("autosave_interval_enabled");
    bot.autosaveEnabled = saved<bool>("macro_auto_save");

    bot.holdFor = saved<int64_t>("autoclicker_hold_for");
    bot.releaseFor = saved<int64_t>("autoclicker_release_for");
    bot.holdFor2 = saved<int64_t>("autoclicker_hold_for2");
    bot.releaseFor2 = saved<int64_t>("autoclicker_release_for2");
    bot.currentPage = saved<int64_t>("current_page");

    bot.autosaveInterval =
        geode::utils::numFromString<float>(saved<std::string>("autosave_interval")).unwrapOr(0.f) *
        60;

    bot.speedhackEnabled = false;
    save("macro_speedhack_enabled", false);

    bot.frameOffset = value<int64_t>("frame_offset");
    bot.lockDelta = value<bool>("lock_delta");
    bot.lockDeltaFast = value<std::string>("lock_delta_mode") == "Fast";
    bot.stopPlaying = value<bool>("auto_stop_playing");

    std::string accuracy = value<std::string>("macro_accuracy");
    bot.frameFixes = accuracy == "Frame Fixes";
    bot.inputFixes = accuracy == "Input Fixes";

    bot.replay.author = "N/A";
    bot.replay.description = "N/A";
    bot.replay.gameVersion =
        static_cast<int>(std::round(static_cast<double>(GEODE_GD_VERSION) * 1000.0));
}
