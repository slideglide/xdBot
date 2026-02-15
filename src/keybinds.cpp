#ifdef GEODE_IS_DESKTOP
#include "includes.hpp"
#include "ui/record_layer.hpp"
#include "ui/game_ui.hpp"
#include "ui/clickbot_layer.hpp"
#include "ui/macro_editor.hpp"
#include "ui/render_settings_layer.hpp"
#include "hacks/layout_mode.hpp"
#include "hacks/show_trajectory.hpp"

#include <Geode/modify/CCKeyboardDispatcher.hpp>
#include <Geode/modify/CCTouchDispatcher.hpp>
#include <Geode/loader/SettingV3.hpp>
#include <Geode/loader/GameEvent.hpp>
#include <Geode/ui/GeodeUI.hpp>

class $modify(CCKeyboardDispatcher) {
    bool dispatchKeyboardMSG(enumKeyCodes key, bool isKeyDown, bool isKeyRepeat, double time) {
        
        auto& g = Global::get();
        
        int keyInt = static_cast<int>(key);
        if (g.allKeybinds.contains(keyInt) && !isKeyRepeat) {
            for (size_t i = 0; i < 6; i++) {
                if (std::find(g.keybinds[i].begin(), g.keybinds[i].end(), keyInt) != g.keybinds[i].end())
                g.heldButtons[i] = isKeyDown;
            }
        }
        
        // if (key == enumKeyCodes::KEY_L && !isKeyRepeat && isKeyDown) {
        // }
        
        // if (key == enumKeyCodes::KEY_F && !isKeyRepeat && isKeyDown && PlayLayer::get()) {
        //   log::debug("POS DEBUG {}", PlayLayer::get()->m_player1->getPosition());
        //   log::debug("POS2 DEBUG {}", PlayLayer::get()->m_player2->getPosition());
        // }
        
        
        // if (key == enumKeyCodes::KEY_J && !isKeyRepeat && isKeyDown && PlayLayer::get()) {
        //   std::string str = ZipUtils::decompressString(PlayLayer::get()->m_level->m_levelString.c_str(), true, 0);
        //   log::debug("{}", str);
        // }
        
        return CCKeyboardDispatcher::dispatchKeyboardMSG(key, isKeyDown, isKeyRepeat, time);
    }
};

void handleKeybind(std::string const& id, bool down) {
    auto& g = Global::get();
    
    if (!down || (LevelEditorLayer::get() && !g.mod->getSettingValue<bool>("editor_keybinds")) || g.mod->getSettingValue<bool>("disable_keybinds")) return;
    
    if (auto scene = CCScene::get()) {
        if (g.layer && scene->getChildByIndex(-1) != g.layer) return;
    }
    
    if (g.layer && id != "open_menu_keybind") return;
    
    if (g.state != state::recording && g.mod->getSettingValue<bool>("recording_only_keybinds")) return;
    
    if (id == "open_menu_keybind") {
        if (g.layer) {
            static_cast<RecordLayer*>(g.layer)->onClose(nullptr);
            return;
        }
        
        RecordLayer::openMenu();
    }
    
    else if (id == "toggle_recording_keybind")
    Macro::toggleRecording();
    
    else if (id == "toggle_playing_keybind")
    Macro::togglePlaying();
    
    else if (id == "toggle_frame_stepper_keybind" && PlayLayer::get())
    Global::toggleFrameStepper();
    
    else if (id == "step_frame_keybind")
    Global::frameStep();
    
    else if (id == "toggle_speedhack_keybind")
    Global::toggleSpeedhack();
    
    else if (id == "show_trajectory_keybind") {
        g.mod->setSavedValue("macro_show_trajectory", !g.mod->getSavedValue<bool>("macro_show_trajectory"));
        
        if (g.layer) {
            if (static_cast<RecordLayer*>(g.layer)->trajectoryToggle)
            static_cast<RecordLayer*>(g.layer)->trajectoryToggle->toggle(g.mod->getSavedValue<bool>("macro_show_trajectory"));
        }
        
        g.showTrajectory = g.mod->getSavedValue<bool>("macro_show_trajectory");
        if (!g.showTrajectory) ShowTrajectory::trajectoryOff();
    }
    
    //   else if (id == "toggle_render_keybind" && PlayLayer::get()) {
    //     #ifndef GEODE_IS_IOS
    //     bool result = Renderer::toggle();
    
    //     // if (result && Global::get().renderer.recording)
    //     if (result)
    //       Notification::create("Started Rendering", NotificationIcon::Info)->show();
    
    //     if (g.layer) {
    //       // if (static_cast<RecordLayer*>(g.layer)->renderToggle)
    //       //   static_cast<RecordLayer*>(g.layer)->renderToggle->toggle(Global::get().renderer.recording);
    //     }
    //     #endif
    
    //   }
    
    else if (id == "toggle_noclip_keybind") {
        g.mod->setSavedValue("macro_noclip", !g.mod->getSavedValue<bool>("macro_noclip"));
        
        if (g.layer) {
            if (static_cast<RecordLayer*>(g.layer)->noclipToggle)
            static_cast<RecordLayer*>(g.layer)->noclipToggle->toggle(g.mod->getSavedValue<bool>("macro_noclip"));
        }
    }
}

$execute{
    listenForKeybindSettingPresses("open_menu_keybind", [](Keybind const&, bool down, bool repeat) {
        if (!repeat) handleKeybind("open_menu_keybind", down);
    });
    listenForKeybindSettingPresses("toggle_recording_keybind", [](Keybind const&, bool down, bool repeat) {
        if (!repeat) handleKeybind("toggle_recording_keybind", down);
    });
    listenForKeybindSettingPresses("toggle_playing_keybind", [](Keybind const&, bool down, bool repeat) {
        if (!repeat) handleKeybind("toggle_playing_keybind", down);
    });
    listenForKeybindSettingPresses("toggle_speedhack_keybind", [](Keybind const&, bool down, bool repeat) {
        if (!repeat) handleKeybind("toggle_speedhack_keybind", down);
    });
    listenForKeybindSettingPresses("toggle_frame_stepper_keybind", [](Keybind const&, bool down, bool repeat) {
        if (!repeat) handleKeybind("toggle_frame_stepper_keybind", down);
    });
    listenForKeybindSettingPresses("step_frame_keybind", [](Keybind const&, bool down, bool repeat) {
        if (!repeat) handleKeybind("step_frame_keybind", down);
    });
    listenForKeybindSettingPresses("show_trajectory_keybind", [](Keybind const&, bool down, bool repeat) {
        if (!repeat) handleKeybind("show_trajectory_keybind", down);
    });
    //   listenForKeybindSettingPresses("toggle_render_keybind", [](Keybind const&, bool down, bool repeat) {
    //     if (!repeat) handleKeybind("toggle_render_keybind", down);
    //   });
    listenForKeybindSettingPresses("toggle_noclip_keybind", [](Keybind const&, bool down, bool repeat) {
        if (!repeat) handleKeybind("toggle_noclip_keybind", down);
    });
}
#endif