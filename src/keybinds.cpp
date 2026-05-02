#include "hacks/show_trajectory.hpp"
#include "includes.hpp"
#include "ui/record_layer.hpp"

#include <Geode/loader/GameEvent.hpp>
#include <Geode/loader/SettingV3.hpp>
#include <Geode/utils/Keyboard.hpp>

// $on_mod(Loaded) {
//     KeyboardInputEvent().listen([](KeyboardInputData& data) {
//         auto& g = Global::get();
//         int keyInt = static_cast<int>(data.key);

//         bool isKeyRepeat = (data.action ==
//         KeyboardInputData::Action::Repeat); bool isKeyDown = (data.action ==
//         KeyboardInputData::Action::Press);

//         if (g.allKeybinds.contains(keyInt) && !isKeyRepeat) {
//             for (size_t i = 0; i < 6; i++) {
//                 if (std::find(g.keybinds[i].begin(), g.keybinds[i].end(),
//                 keyInt) != g.keybinds[i].end()) {
//                     g.heldButtons[i] = isKeyDown;
//                 }
//             }
//         }

//         return ListenerResult::Propagate;
//     }).leak();
// }

void handleKeybind(std::string_view id, bool down, bool repeat, double time) {
    auto& g = Global::get();

    if (!down || (LevelEditorLayer::get() && !g.mod->getSettingValue<bool>("editor_keybinds")) ||
        g.mod->getSettingValue<bool>("disable_keybinds")) {
        return;
    }

    if (auto scene = CCScene::get()) {
        if (id == "open_menu_keybind" && g.layer && scene->getChildByIndex(-1) != g.layer)
            return;
    }

    if (g.state != state::recording && g.mod->getSettingValue<bool>("recording_only_keybinds"))
        return;

    static std::unordered_map<std::string_view, geode::Function<void()>> handlers = [] {
        std::unordered_map<std::string_view, geode::Function<void()>> map;

        map["open_menu_keybind"] = []() {
            auto& g = Global::get();
            if (g.layer) {
                static_cast<RecordLayer*>(g.layer)->onClose(nullptr);
            } else {
                RecordLayer::openMenu(g.mod->getSettingValue<bool>("open_menu_instant"));
            }
        };

        map["toggle_recording_keybind"] = []() {
            Macro::toggleRecording();
        };
        map["toggle_playing_keybind"] = []() {
            Macro::togglePlaying();
        };

        map["toggle_frame_stepper_keybind"] = []() {
            if (PlayLayer::get())
                Global::toggleFrameStepper();
        };

        map["step_frame_keybind"] = []() {
            Global::frameStep();
        };

        map["toggle_speedhack_keybind"] = []() {
            Global::toggleSpeedhack();
        };

        map["show_trajectory_keybind"] = []() {
            auto& g = Global::get();
            bool newState = !g.mod->getSavedValue<bool>("macro_show_trajectory");
            g.mod->setSavedValue("macro_show_trajectory", newState);
            g.showTrajectory = newState;

            if (auto* layer = static_cast<RecordLayer*>(g.layer)) {
                if (layer->trajectoryToggle)
                    layer->trajectoryToggle->toggle(newState);
            }
            if (!newState)
                ShowTrajectory::trajectoryOff();
        };

#ifndef GEODE_IS_IOS
        map["toggle_render_keybind"] = []() {
            if (PlayLayer::get()) {
                auto& g = Global::get();
                if (Renderer::toggle()) {
                    Notification::create("Started Rendering", NotificationIcon::Info)->show();
                }
                if (auto* layer = static_cast<RecordLayer*>(g.layer)) {
                    if (layer->renderToggle)
                        layer->renderToggle->toggle(g.renderer.recording);
                }
            }
        };
#endif

        map["toggle_noclip_keybind"] = []() {
            auto& g = Global::get();
            bool newState = !g.mod->getSavedValue<bool>("macro_noclip");
            g.mod->setSavedValue("macro_noclip", newState);

            if (auto* layer = static_cast<RecordLayer*>(g.layer)) {
                if (layer->noclipToggle)
                    layer->noclipToggle->toggle(newState);
            }
        };

        return map;
    }();

    if (auto it = handlers.find(id); it != handlers.end()) {
        it->second();
    }
}

$execute {
    struct KeybindEntry {
        std::string_view id;
        bool passRepeat;
    };

    constexpr KeybindEntry keybinds[] = {
        {"open_menu_keybind", false},
        {"toggle_recording_keybind", false},
        {"toggle_playing_keybind", false},
        {"toggle_speedhack_keybind", false},
        {"toggle_frame_stepper_keybind", false},
        {"step_frame_keybind", true},
        {"show_trajectory_keybind", false},
#ifndef GEODE_IS_IOS
        {"toggle_render_keybind", false},
#endif
        {"toggle_noclip_keybind", false},
    };

    for (const auto& entry : keybinds) {
        listenForKeybindSettingPresses(
            std::string(entry.id),
            [id = entry.id,
             passRepeat = entry.passRepeat](Keybind const&, bool down, bool repeat, double time) {
                handleKeybind(id, down, passRepeat ? repeat : false, time);
            });
    }
}