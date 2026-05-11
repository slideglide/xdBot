#include "record_layer.hpp"
#include "../gdr/gdr.hpp"
#include "../global.hpp"
#include "../hacks/coin_finder.hpp"
#include "../hacks/show_trajectory.hpp"
#include "autoclicker_settings_layer.hpp"
#include "clickbot_layer.hpp"
#include "game_ui.hpp"
#include "macro_editor.hpp"
#include "mirror_settings_layer.hpp"
#include "noclip_settings_layer.hpp"
#include "render_presets_layer.hpp"
#include "trajectory_settings_layer.hpp"

#include <Geode/modify/PauseLayer.hpp>
#include <Geode/ui/GeodeUI.hpp>
#include <Geode/utils/web.hpp>

const std::vector<std::vector<RecordSetting>> settings{
    {
        {"TPS Bypass:", "macro_tps_enabled", InputType::Tps, 0.4f},
        {"Speedhack:", "macro_speedhack_enabled", InputType::Speedhack, 0.4f},
        {"Seed:", "macro_seed_enabled", InputType::Seed, 0.4f},
        {"Enable Noclip:", "macro_noclip", InputType::Settings, 0.325f,
         menu_selector(NoclipSettingsLayer::open)},
        {"Show Trajectory:", "macro_show_trajectory", InputType::Settings,
         0.325f, menu_selector(TrajectorySettingsLayer::open)},
        {"Enable Frame Stepper:", "macro_frame_stepper", InputType::None},
    },
    {{"Instant respawn:", "macro_instant_respawn", InputType::None},
     {"No death effect:", "macro_no_death_effect", InputType::None},
     {"No respawn flash:", "macro_no_respawn_flash", InputType::None},
     {"Enable Coin Finder:", "macro_coin_finder", InputType::None},
     {"Enable Layout Mode:", "macro_layout_mode", InputType::None},
     {"Auto Safe Mode:", "macro_auto_safe_mode", InputType::None}},
    {
#ifdef GEODE_IS_DESKTOP
        {"Force cursor on open:", "menu_show_cursor", InputType::None},
        {"Button on pause menu:", "menu_show_button", InputType::None},
        {"Pause on open:", "menu_pause_on_open", InputType::None},
#else
        {"Always show buttons:", "macro_always_show_buttons", InputType::None},
        {"Hide speedhack button:", "macro_hide_speedhack", InputType::None},
        {"Hide Frame Stepper button:", "macro_hide_stepper", InputType::None,
         0.3f},
#endif
        {"Hide labels on render:", "render_hide_labels", InputType::None},
        {"Hide playing label:", "macro_hide_playing_label", InputType::None},
        {"Hide recording label:", "macro_hide_recording_label",
         InputType::None}},
    {
        {"Enable Clickbot:", "clickbot_enabled", InputType::Settings, 0.325f,
         menu_selector(ClickbotLayer::open)},
        {"Enable Autoclicker:", "autoclicker_enabled", InputType::Settings,
         0.3f, menu_selector(AutoclickerLayer::open)},
        {"Always Practice Fixes:", "macro_always_practice_fixes",
         InputType::None},
        {"Ignore inputs:", "macro_ignore_inputs", InputType::None},
        {"Show Frame Label:", "macro_show_frame_label", InputType::None},
        {"Speedhack Audio:", "macro_speedhack_audio", InputType::None}
        // { "Auto Stop Playing:", "macro_auto_stop_playing", InputType::None }
    },
    {{"Respawn Time:", "respawn_time_enabled", InputType::Respawn},
     {"Input Mirror:", "p2_input_mirror", InputType::Settings, 0.325f,
      menu_selector(MirrorSettingsLayer::open)},
     {"Disable Shaders:", "disable_shaders", InputType::None},
     {"Instant Mirror Portal:", "instant_mirror_portal", InputType::None},
     {"No Mirror Portal:", "no_mirror_portal", InputType::None},
     {"Enable Auto Saving:", "macro_auto_save", InputType::Autosave}}};

class $modify(PauseLayer) {
    void customSetup() {
        PauseLayer::customSetup();

#ifdef GEODE_IS_DESKTOP

        if (!Mod::get()->getSavedValue<bool>("menu_show_button"))
            return;

#endif

        CCSprite *sprite = nullptr;

        sprite = CCSprite::createWithSpriteFrameName("GJ_playBtn2_001.png");
        sprite->setScale(0.35f);

        CCMenuItemSpriteExtra *btn = CCMenuItemExt::createSpriteExtra(sprite, [this](CCMenuItemSpriteExtra *sender) { static_cast<RecordLayer*>(Global::get().layer)->openMenu2(sender); });

        if (!Loader::get()->isModLoaded("geode.node-ids")) {
            CCMenu *menu = CCMenu::create();
            menu->setID("button"_spr);
            addChild(menu);
            btn->setPosition({214, 88});
            menu->addChild(btn);
            return;
        }

        CCNode *menu = this->getChildByID("right-button-menu");
        menu->addChild(btn);
        menu->updateLayout();
    }
};

$execute {
    geode::listenForSettingChanges<int64_t>(
        "frame_offset", +[](int64_t value) {
            auto &g = Global::get();
            g.frameOffset = value;

            if (g.layer) {
                static_cast<RecordLayer *>(g.layer)->warningLabel->setString(
                    ("WARNING: Currently recording / playing macros with a "
                     "frame offset of " +
                     geode::utils::numToString(value))
                        .c_str());
                static_cast<RecordLayer *>(g.layer)->warningLabel->setVisible(
                    value != 0);
                static_cast<RecordLayer *>(g.layer)->warningSprite->setVisible(
                    value != 0);
            }
        });

    geode::listenForSettingChanges<cocos2d::ccColor3B>(
        "background_color", +[](cocos2d::ccColor3B value) {
            auto &g = Global::get();
            if (g.layer) {
                CCArray *children = CCScene::get()->getChildren();
                if (FLAlertLayer *layer =
                        typeinfo_cast<FLAlertLayer *>(children->lastObject()))
                    layer->keyBackClicked();

                static_cast<RecordLayer *>(g.layer)->onClose(nullptr);
                static_cast<RecordLayer*>(g.layer)->openMenu(true);
            }
        });
};

void RecordLayer::openSaveMacro(CCObject *) { SaveMacroLayer::open(); }

void RecordLayer::openLoadMacro(CCObject *) {
    LoadMacroLayer::open(static_cast<geode::Popup *>(this), nullptr);
}

RecordLayer *RecordLayer::openMenu(bool instant) {
    auto &g = Global::get();
    PlayLayer *pl = PlayLayer::get();
    bool cursor = false;

    CCArray *children = CCScene::get()->getChildren();
    CCObject *child;

    if (g.layer)
        static_cast<RecordLayer *>(g.layer)->onClose(nullptr);

    if (pl && g.mod->getSavedValue<bool>("menu_pause_on_open")) {
        if (!pl->m_isPaused)
            pl->pauseGame(false);
    }
#ifdef GEODE_IS_DESKTOP
    else if (pl) {
        bool value = g.mod->getSavedValue<bool>("menu_show_cursor");
        auto gm = GameManager::sharedState();

        if (gm->getGameVariable(GameVar::ShowCursor) != value) {
            gm->setGameVariable(GameVar::ShowCursor, value);
        }
    }
#endif

    RecordLayer *layer = create();
    layer->cursorWasHidden = cursor;
    layer->m_noElasticity = instant || Global::get().speedhackEnabled;
    layer->show();

    g.layer = static_cast<geode::Popup *>(layer);

    return layer;
}

void RecordLayer::checkSpeedhack() {
    std::string speedhackValue =
        mod->getSavedValue<std::string>("macro_speedhack");

    if (std::count(speedhackValue.begin(), speedhackValue.end(), '.') == 0)
        speedhackValue += ".0";

    if (speedhackValue.back() == '.')
        speedhackValue += "0";

    if (speedhackValue[0] == '0' && speedhackValue[1] != '.')
        speedhackValue.erase(0, 1);

    if (speedhackValue[0] == '.')
        speedhackValue = "0" + speedhackValue;

    mod->setSavedValue("macro_speedhack", speedhackValue);
}

void RecordLayer::onClose(CCObject *) {
    checkSpeedhack();

    PlayLayer *pl = PlayLayer::get();

    if (cursorWasHidden && pl)
        PlatformToolbox::hideCursor();

    Global::get().layer = nullptr;

    this->setKeypadEnabled(false);
    this->setTouchEnabled(false);
    this->removeFromParentAndCleanup(true);
}

void RecordLayer::toggleRecording(CCObject *) {
    auto &g = Global::get();

    if (Global::hasIncompatibleMods() ||
        Global::enabledIncompatibleGDSettings())
        return recording->toggle(true);

    if (g.state == state::playing)
        playing->toggle(false);
    g.state = g.state == state::recording ? state::none : state::recording;

    if (g.state == state::recording) {
        g.currentAction = 0;
        g.currentFrameFix = 0;
        g.restart = true;

        PlayLayer *pl = PlayLayer::get();
        if (pl) {
            if (!pl->m_isPaused)
                pl->pauseGame(false);
        }
    }

    Interface::updateLabels();
    Interface::updateButtons();
    Macro::updateTPS();
    this->updateTPS();

    g.lastAutoSaveMS = asp::time::Instant::now();
}

void RecordLayer::togglePlaying(CCObject *) {
    auto &g = Global::get();

    if (Global::hasIncompatibleMods() ||
        Global::enabledIncompatibleGDSettings())
        return playing->toggle(true);

    if (g.state == state::recording)
        recording->toggle(false);

    g.state = g.state == state::playing ? state::none : state::playing;

    if (g.state == state::playing) {
        g.currentAction = 0;
        g.currentFrameFix = 0;

        g.macro.xdBotMacro = g.macro.botInfo.name == "xdBot";

        PlayLayer *pl = PlayLayer::get();
        PlayLayer *plScene = CCScene::get()->getChildByType<PlayLayer>(0);

        if (pl && plScene) {
            if (!pl->m_isPaused && !pl->m_levelEndAnimationStarted)
                pl->m_isPlatformer ? pl->resetLevelFromStart()
                                   : pl->resetLevel();
            else
                g.restart = true;
        }
    }

    Interface::updateLabels();
    Interface::updateButtons();
    Macro::updateTPS();
    this->updateTPS();
}

void RecordLayer::toggleRender(CCObject *btn) {
#ifndef GEODE_IS_IOS
    if (!Renderer::toggle())
        static_cast<CCMenuItemToggler *>(btn)->toggle(true);
#else
    toggleRender2(btn);
#endif
}

void RecordLayer::toggleRender2(CCObject *btn) {
    FLAlertLayer::create("Info",
                         "Rendering is <cr>not supported</c> on your platform "
                         "due to <cl>technical limitations</c>.",
                         "OK")
        ->show();
}

void RecordLayer::onEditMacro(CCObject *) { MacroEditLayer::open(); }

void RecordLayer::toggleFPS(bool on) {
    return;
    float scaleSpr = -0.8, scaleBtn = -1;
    int opacityBtn = 57, opacityLbl = 80;

    if (on) {
        return;
        scaleSpr = 0.8;
        scaleBtn = 1;
        opacityBtn = 230;
        opacityLbl = 255;
    }

    CCSprite *spr = CCSprite::createWithSpriteFrameName("edit_leftBtn_001.png");
    spr->setScale(scaleSpr);
    FPSLeft->setSprite(spr);
    FPSLeft->setScale(scaleBtn);
    FPSLeft->setOpacity(opacityBtn);

    spr = CCSprite::createWithSpriteFrameName("edit_rightBtn_001.png");
    spr->setScale(scaleSpr);
    FPSRight->setSprite(spr);
    FPSRight->setScale(scaleBtn);
    FPSRight->setOpacity(opacityBtn);

    fpsLabel->setOpacity(opacityLbl);
}

void RecordLayer::macroInfo(CCObject *) { MacroInfoLayer::create()->show(); }

void RecordLayer::textChanged(CCTextInputNode *node) {
    if (!node)
        return;

    mod = Mod::get();

    if (seedInput && node == seedInput->getInputNode()) {

        if (auto num =
                numFromString<unsigned long long>(seedInput->getString())) {
            mod->setSavedValue("macro_seed",
                               geode::utils::numToString(num.unwrap()));
            return;
        } else {
            return seedInput->setString(
                mod->getSavedValue<std::string>("macro_seed").c_str());
        }
    }

    if (codecInput && node == codecInput->getInputNode())
        mod->setSavedValue("render_codec",
                           std::string(codecInput->getString()));

    if (widthInput && std::string_view(widthInput->getString()) != "" &&
        node == widthInput->getInputNode())
        mod->setSavedValue("render_width2",
                           std::string(widthInput->getString()));

    if (heightInput && std::string_view(heightInput->getString()) != "" &&
        node == heightInput->getInputNode())
        mod->setSavedValue("render_height",
                           std::string(heightInput->getString()));

    if (bitrateInput && std::string_view(bitrateInput->getString()) != "" &&
        node == bitrateInput->getInputNode())
        mod->setSavedValue("render_bitrate",
                           std::string(bitrateInput->getString()));

    if (fpsInput && std::string_view(fpsInput->getString()) != "" &&
        node == fpsInput->getInputNode()) {
        if (geode::utils::numFromString<int>(fpsInput->getString())
                .unwrapOr(0) > 240)
            return fpsInput->setString(
                mod->getSavedValue<std::string>("render_fps").c_str());
        mod->setSavedValue("render_fps", std::string(fpsInput->getString()));
    }

    if (respawnInput && node == respawnInput->getInputNode()) {
        std::string str = respawnInput->getString();
        mod->setSavedValue("respawn_time",
                           numFromString<double>(str).unwrapOr(0.5));
    }

    if (tpsInput && node == tpsInput->getInputNode()) {
        float value = geode::utils::numFromString<float>(tpsInput->getString())
                          .unwrapOr(0.f);
        if (std::string_view(tpsInput->getString()) != "" && value < 999999 &&
            value >= 0.f) {
            Global::get().setTps(value);
            Global::get().leftOver = 0.f;
        }
    }

    if (!speedhackInput || node != speedhackInput->getInputNode())
        return;

    if (std::string_view(speedhackInput->getString()) != "" &&
        node == speedhackInput->getInputNode()) {
        std::string value = speedhackInput->getString();

        if (value == ".")
            speedhackInput->setString("0.");
        else if (std::count(value.begin(), value.end(), '.') == 2 ||
                 geode::utils::numFromString<float>(value).unwrapOr(0) > 10)
            return speedhackInput->setString(
                mod->getSavedValue<std::string>("macro_speedhack").c_str());
    }

    if (std::string_view(speedhackInput->getString()) != "")
        mod->setSavedValue("macro_speedhack",
                           std::string(speedhackInput->getString()));
}

void RecordLayer::updatePage(CCObject *obj) {
    auto &g = Global::get();
    g.currentPage +=
        static_cast<CCNode *>(obj)->getID() == "page-left" ? -1 : 1;
    if (g.currentPage == -1)
        g.currentPage = settings.size() - 1;
    else if (g.currentPage == settings.size())
        g.currentPage = 0;

    goToSettingsPage(g.currentPage);
}

void RecordLayer::toggleSetting(CCObject *obj) {
    CCMenuItemToggler *toggle = static_cast<CCMenuItemToggler *>(obj);
    std::string id = toggle->getID();
    auto &g = Global::get();
    mod = g.mod;

    bool value = !toggle->isToggled();

    g.mod->setSavedValue(id, value);

    // Some of these get checked every frame so idk i didnt want to do
    // mod->getSavedValue<bool> every time
    if (id == "macro_seed_enabled")
        g.seedEnabled = value;
    if (id == "macro_speedhack_enabled")
        g.speedhackEnabled = value;
    if (id == "macro_speedhack_audio")
        g.speedhackAudio = value;
    if (id == "p2_input_mirror")
        g.p2mirror = value;
    if (id == "clickbot_enabled")
        g.clickbotEnabled = value;
    if (id == "clickbot_playing_only")
        g.clickbotOnlyPlaying = value;
    if (id == "clickbot_holding_only")
        g.clickbotOnlyHolding = value;
    if (id == "macro_tps_enabled")
        g.setTpsEnabled(value);
    if (id == "autoclicker_enabled")
        g.autoclicker = value;
    if (id == "macro_always_practice_fixes")
        g.alwaysPracticeFixes = value;
    if (id == "disable_shaders")
        g.disableShaders = value;
    if (id == "macro_auto_save")
        g.autosaveEnabled = value;

    if (id == "macro_show_trajectory") {
        g.showTrajectory = value;
        if (!value)
            ShowTrajectory::trajectoryOff();
    }

    if (id == "macro_coin_finder") {
        g.coinFinder = value;
        if (!value)
            CoinFinder::finderOff();
    }

    if (id == "macro_show_trajectory") {
        g.showTrajectory = value;
        if (!value)
            ShowTrajectory::trajectoryOff();
    }

    if (id == "macro_show_frame_label") {
        g.frameLabel = value;
        Interface::updateLabels();
    }

    if (id == "macro_frame_stepper") {
        g.frameStepper = value;
        Interface::updateButtons();
    }

    if (id == "clickbot_enabled" || id == "clickbot_playing_only")
        Clickbot::updateSounds();

    if (id == "macro_hide_recording_label" ||
        id == "macro_hide_playing_label" || id == "render_hide_labels")
        Interface::updateLabels();

    if (id == "macro_hide_speedhack" || id == "macro_hide_stepper" ||
        id == "macro_always_show_buttons")
        Interface::updateButtons();

    if (id == "menu_show_button") {
        PlayLayer *pl = PlayLayer::get();

        if (!pl)
            return;
        if (!pl->m_isPaused)
            return;

        if (PauseLayer *layer = Utils::getPauseLayer()) {
            layer->onResume(nullptr);
            PlayLayer::get()->pauseGame(false);

            this->onClose(nullptr);
            RecordLayer::openMenu(true);
        }

        if (!value)
            Notification::create("xdBot Button is disabled.",
                                 NotificationIcon::Warning)
                ->show();
    }
}

void RecordLayer::openKeybinds(CCObject *) {
    #ifdef GEODE_IS_DESKTOP
    geode::openKeybindsPopup(std::nullopt, Mod::get());
    #else
    Interface::openButtonEditor();
    #endif
}

void RecordLayer::openPresets(CCObject *) {
    RenderPresetsLayer::create()->show();
}

void RecordLayer::onAutosaves(CCObject *) {
#ifdef GEODE_IS_IOS
    std::filesystem::path path = Mod::get()->getSaveDir() / "autosaves";
#else
    std::filesystem::path path =
        Mod::get()->getSettingValue<std::filesystem::path>("autosaves_folder");
#endif

    if (std::filesystem::exists(path))
        LoadMacroLayer::open(static_cast<geode::Popup *>(this), nullptr, true);
    else {
        FLAlertLayer::create(
            "Error", "There was an error getting the folder. ID: 5", "OK")
            ->show();
    }
}

void RecordLayer::showCodecPopup(CCObject *) {
#ifdef GEODE_IS_ANDROID
    FLAlertLayer::create("Codec", "GPU: h264_mediacodec\n CPU: libx264", "OK")
        ->show();
#endif

#ifdef GEODE_IS_WINDOWS
    FLAlertLayer::create(
        "Codec",
        "<cr>AMD:</c> h264_amf\n<cg>NVIDIA:</c> h264_nvenc\n<cl>INTEL:</c> "
        "h264_qsv\nI don't know: libx264",
        "OK")
        ->show();
#endif
}

void RecordLayer::updateDots() {
    for (int i = 0; i < dots.size(); i++) {
        if (i == Global::get().currentPage) {
            dots[i]->setScale(0.4);
            dots[i]->setOpacity(255);
        } else {
            dots[i]->setScale(0.3f);
            dots[i]->setOpacity(70);
        }
    }
}

bool RecordLayer::init() {
    if (!Popup::init(455, 271, Utils::getTexture().c_str()))
        return false;
    auto &g = Global::get();
    mod = g.mod;

    Utils::setBackgroundColor(m_bgSprite);

    cocos2d::CCPoint offset = (CCDirector::sharedDirector()->getWinSize() -
                               m_mainLayer->getContentSize()) /
                              2;
    m_mainLayer->setPosition(m_mainLayer->getPosition() - offset);
    m_closeBtn->setPosition(m_closeBtn->getPosition() + offset);
    m_bgSprite->setPosition(m_bgSprite->getPosition() + offset);

    m_closeBtn->setPosition(m_closeBtn->getPosition() + ccp(-6.75, 6.75));
    m_closeBtn->getNormalImage()->setScale(0.575f);

    menu = CCMenu::create();
    m_mainLayer->addChild(menu);

    for (int i = 0; i < settings.size(); i++) {
        CCSprite *dot = CCSprite::create("smallDot.png");
        menu->addChild(dot);
        dots.push_back(dot);
    }

    float spacing = 10.f;
    float width = (dots.size() - 1) * spacing;
    float center = 103 - (width / 2.0f);

    for (int i = 0; i < dots.size(); ++i)
        dots[i]->setPosition({center + i * spacing, 96.5f});

    updateDots();

    warningSprite =
        CCSprite::createWithSpriteFrameName("geode.loader/info-alert.png");
    warningSprite->setScale(0.675f);
    warningSprite->setPosition({82, 307});
    m_mainLayer->addChild(warningSprite);

    warningLabel =
        CCLabelBMFont::create(("WARNING: Currently recording / playing macros "
                               "with a frame offset of " +
                               geode::utils::numToString(g.frameOffset))
                                  .c_str(),
                              "bigFont.fnt");
    warningLabel->setAnchorPoint({0, 0.5});
    warningLabel->setPosition({92, 307});
    warningLabel->setScale(0.275f);
    m_mainLayer->addChild(warningLabel);

    warningSprite->setVisible(g.frameOffset != 0);
    warningLabel->setVisible(g.frameOffset != 0);

    CCLabelBMFont *versionLabel = CCLabelBMFont::create(
        ("xdBot " + getModVersionString()).c_str(), "chatFont.fnt");
    versionLabel->setOpacity(63);
    versionLabel->setPosition(ccp(-217, -125));
    versionLabel->setAnchorPoint({0, 0.5});
    versionLabel->setScale(0.4f);
    versionLabel->setSkewX(4);
    menu->addChild(versionLabel);

#ifdef GEODE_IS_WINDOWS

    CCLabelBMFont *codecBtnLbl = CCLabelBMFont::create("?", "chatFont.fnt");
    codecBtnLbl->setOpacity(148);
    codecBtnLbl->setScale(0.65f);

    CCMenuItemSpriteExtra *codecBtn = CCMenuItemExt::createSpriteExtra(codecBtnLbl, [this](CCMenuItemSpriteExtra *sender) { RecordLayer::showCodecPopup(sender); });
    codecBtn->setPosition({-26, -49});

    menu->addChild(codecBtn);

#endif

    NineSlice *bg = NineSlice::create("square02b_001.png", {0, 0, 80, 80});
    bg->setScale(0.7f);
    bg->setColor({0, 0, 0});
    bg->setOpacity(75);
    bg->setPosition(ccp(-212, 121));
    bg->setAnchorPoint({0, 1});
    bg->setContentSize({275, 151});
    menu->addChild(bg);

    bg = NineSlice::create("square02b_001.png", {0, 0, 80, 80});
    bg->setScale(0.7f);
    bg->setColor({0, 0, 0});
    bg->setOpacity(75);
    bg->setPosition(ccp(-212, 0));
    bg->setAnchorPoint({0, 1});
    bg->setContentSize({275, 169});
    menu->addChild(bg);

    bg = NineSlice::create("square02b_001.png", {0, 0, 80, 80});
    bg->setScale(0.7f);
    bg->setColor({0, 0, 0});
    bg->setOpacity(75);
    bg->setPosition(ccp(103, 2));
    bg->setContentSize({313, 339});
    menu->addChild(bg);

    recording = CCMenuItemExt::createTogglerWithStandardSprites(0.775f, [this](CCMenuItemToggler *sender) { RecordLayer::toggleRecording(sender); });
    recording->toggle(g.state == state::recording);

    playing = CCMenuItemExt::createTogglerWithStandardSprites(0.775f, [this](CCMenuItemToggler *sender) { RecordLayer::togglePlaying(sender); });
    playing->toggle(g.state == state::playing);

    recording->setPosition(ccp(-161.5, 78));
    playing->setPosition(ccp(-74.5, 78));

    menu->addChild(recording);
    menu->addChild(playing);

    actionsLabel = CCLabelBMFont::create(
        ("Actions: " + geode::utils::numToString(g.macro.inputs.size()))
            .c_str(),
        "chatFont.fnt");
    actionsLabel->limitLabelWidth(57.f, 0.6f, 0.01f);
    actionsLabel->updateLabel();
    actionsLabel->setAnchorPoint({0, 0.5});
    actionsLabel->setOpacity(83);
    actionsLabel->setPosition(ccp(-201, 110));
    menu->addChild(actionsLabel);

    CCLabelBMFont *lbl = CCLabelBMFont::create("Macro", "goldFont.fnt");
    lbl->setPosition(ccp(-116.5, 112));
    lbl->setScale(0.575f);
    menu->addChild(lbl);

    lbl = CCLabelBMFont::create("Render", "goldFont.fnt");
    lbl->setScale(0.6f);
    lbl->setPosition(ccp(-116.5, -9));
    menu->addChild(lbl);

    lbl = CCLabelBMFont::create("Settings", "goldFont.fnt");
    lbl->setPosition(ccp(103, 111));
    lbl->setScale(0.7f);
    menu->addChild(lbl);

    lbl = CCLabelBMFont::create("Record", "bigFont.fnt");
    lbl->setPosition(ccp(-161.5, 60));
    lbl->setScale(0.325f);
    menu->addChild(lbl);

    lbl = CCLabelBMFont::create("Play", "bigFont.fnt");
    lbl->setPosition(ccp(-74, 60));
    lbl->setScale(0.325f);
    menu->addChild(lbl);

    lbl = CCLabelBMFont::create("X", "chatFont.fnt");
    lbl->setPosition(ccp(-114.5, -31));
    lbl->setScale(0.7f);
    menu->addChild(lbl);

    lbl = CCLabelBMFont::create("M", "chatFont.fnt");
    lbl->setPosition(ccp(-164, -59));
    lbl->setScale(0.7f);
    menu->addChild(lbl);

    lbl = CCLabelBMFont::create("FPS", "chatFont.fnt");
    lbl->setPosition(ccp(-108.5, -59));
    lbl->setScale(0.49f);
    menu->addChild(lbl);

    ButtonSprite *btnSprite = ButtonSprite::create("Save");
    btnSprite->setScale(0.54f);

    CCMenuItemSpriteExtra *btn = CCMenuItemExt::createSpriteExtra(btnSprite, [this](CCMenuItemSpriteExtra *sender) { RecordLayer::openSaveMacro(sender); });

    btn->setPosition(ccp(-176, 34));
    menu->addChild(btn);

#ifdef GEODE_IS_DESKTOP
    btnSprite = ButtonSprite::create("Keybinds");
#else
    btnSprite = ButtonSprite::create("Buttons");
#endif
    btnSprite->setScale(0.54f);

    btn = CCMenuItemExt::createSpriteExtra(btnSprite, [this](CCMenuItemSpriteExtra *sender) { RecordLayer::openKeybinds(sender); });

    btn->setPosition(ccp(40, -100));
    menu->addChild(btn);

    btnSprite = ButtonSprite::create("More Settings");
    btnSprite->setScale(0.54f);

    btn = CCMenuItemExt::createSpriteExtra(btnSprite, [this](CCMenuItemSpriteExtra *sender) { RecordLayer::moreSettings(sender); });

    btn->setPosition(ccp(148, -100));
    menu->addChild(btn);

    btnSprite = ButtonSprite::create("Load");
    btnSprite->setScale(0.54f);

    btn = CCMenuItemExt::createSpriteExtra(btnSprite, [this](CCMenuItemSpriteExtra *sender) { RecordLayer::openLoadMacro(sender); });

    btn->setPosition(ccp(-115, 34));
    menu->addChild(btn);

    btnSprite = ButtonSprite::create("Edit");
    btnSprite->setScale(0.54f);

    btn = CCMenuItemExt::createSpriteExtra(btnSprite, [this](CCMenuItemSpriteExtra *sender) { RecordLayer::onEditMacro(sender); });

    btn->setPosition(ccp(-56, 34));
    menu->addChild(btn);

    widthInput = TextInput::create(60.f, "Width", "chatFont.fnt");
    widthInput->setPosition(ccp(-157, -31));
    widthInput->setFilter("0123456789");
    widthInput->setString(mod->getSavedValue<std::string>("render_width2"));
    widthInput->setDelegate(this);
    widthInput->setID("render-input");
    widthInput->getInputNode()->setMaxLabelScale(0.7f);
    widthInput->getInputNode()->m_textLabel->setAnchorPoint({0.5f, 0.5f});
    widthInput->getInputNode()->m_textLabel->setOpacity(150);
    widthInput->getInputNode()->m_textField->setAnchorPoint({0.5f, 0.5f});
    widthInput->setWidth(60.75f);
    widthInput->getBGSprite()->setContentHeight(41.25f);
    widthInput->getBGSprite()->setOpacity(75);
    menu->addChild(widthInput);

    heightInput = TextInput::create(60.f, "Height", "chatFont.fnt");
    heightInput->setPosition(ccp(-72.5, -31));
    heightInput->setFilter("0123456789");
    heightInput->setString(mod->getSavedValue<std::string>("render_height"));
    heightInput->setDelegate(this);
    heightInput->setID("render-input");
    heightInput->getInputNode()->setMaxLabelScale(0.7f);
    heightInput->getInputNode()->m_textLabel->setAnchorPoint({0.5f, 0.5f});
    heightInput->getInputNode()->m_textLabel->setOpacity(150);
    heightInput->getInputNode()->m_textField->setAnchorPoint({0.5f, 0.5f});
    heightInput->setWidth(60.75f);
    heightInput->getBGSprite()->setContentHeight(41.25f);
    heightInput->getBGSprite()->setOpacity(75);
    menu->addChild(heightInput);

    bitrateInput = TextInput::create(32.f, "br", "chatFont.fnt");
    bitrateInput->setPosition(ccp(-185.5, -59));
    bitrateInput->setFilter("0123456789");
    bitrateInput->setString(mod->getSavedValue<std::string>("render_bitrate"));
    bitrateInput->setDelegate(this);
    bitrateInput->getInputNode()->setMaxLabelScale(0.7f);
    bitrateInput->getInputNode()->m_textLabel->setAnchorPoint({0.5f, 0.5f});
    bitrateInput->getInputNode()->m_textLabel->setOpacity(150);
    bitrateInput->getInputNode()->m_textField->setAnchorPoint({0.5f, 0.5f});
    bitrateInput->setWidth(30.75f);
    bitrateInput->getBGSprite()->setContentHeight(41.25f);
    bitrateInput->getBGSprite()->setOpacity(75);
    menu->addChild(bitrateInput);

    CCSprite *emptyBtn =
        CCSprite::createWithSpriteFrameName("GJ_plainBtn_001.png");
    emptyBtn->setScale(0.67f);

    CCSprite *folderIcon =
        CCSprite::createWithSpriteFrameName("folderIcon_001.png");
    folderIcon->setPosition(emptyBtn->getContentSize() / 2);
    folderIcon->setScale(0.7f);

    emptyBtn->addChild(folderIcon);
    btn = CCMenuItemExt::createSpriteExtra(emptyBtn, [this](CCMenuItemSpriteExtra *sender) { RecordLayer::openPresets(sender); });
    btn->setPosition(ccp(-177.5, -97));

    menu->addChild(btn);

    CCSprite *spr =
        CCSprite::createWithSpriteFrameName("GJ_optionsBtn_001.png");
    spr->setScale(0.65f);

    btn = CCMenuItemExt::createSpriteExtra(spr, [this](CCMenuItemSpriteExtra *sender) { static_cast<RenderSettingsLayer*>(Global::get().layer)->open(sender); });
    btn->setPosition(ccp(-129.5, -97));
    menu->addChild(btn);

    codecInput = TextInput::create(79.f, "Codec", "chatFont.fnt");
    codecInput->setPosition(ccp(-60.5, -59));
    codecInput->setString(mod->getSavedValue<std::string>("render_codec"));
    codecInput->setDelegate(this);
    codecInput->setFilter("0123456789abcdefghijklmnopqrstuvwxyz-_.\"\\/");
    codecInput->getInputNode()->setMaxLabelWidth(74.f);
    codecInput->getInputNode()->m_textLabel->setAnchorPoint({0.5f, 0.5f});
    codecInput->getInputNode()->m_textLabel->setOpacity(150);
    codecInput->getInputNode()->m_textField->setAnchorPoint({0.5f, 0.5f});
    codecInput->setWidth(62.625f);
    codecInput->getBGSprite()->setContentHeight(41.25f);
    codecInput->getBGSprite()->setOpacity(75);
    menu->addChild(codecInput);

    fpsInput = TextInput::create(32.f, "FPS", "chatFont.fnt");
    fpsInput->setPosition(ccp(-133, -59));
    fpsInput->setFilter("0123456789");
    fpsInput->setString(mod->getSavedValue<std::string>("render_fps"));
    fpsInput->setDelegate(this);
    fpsInput->getInputNode()->m_textLabel->setScale(0.6);
    fpsInput->getInputNode()->setMaxLabelScale(0.7f);
    fpsInput->getInputNode()->m_textLabel->setAnchorPoint({0.5f, 0.5f});
    fpsInput->getInputNode()->m_textLabel->setOpacity(150);
    fpsInput->getInputNode()->m_textField->setAnchorPoint({0.5f, 0.5f});
    fpsInput->setWidth(30.75f);
    fpsInput->getBGSprite()->setContentHeight(41.25f);
    fpsInput->getBGSprite()->setOpacity(75);
    menu->addChild(fpsInput);

#ifndef GEODE_IS_IOS
    ButtonSprite *spriteOn2 = ButtonSprite::create("Stop");
    spriteOn2->setScale(0.74f);
    ButtonSprite *spriteOff2 = ButtonSprite::create("Start");
    spriteOff2->setScale(0.74f);

    renderToggle = CCMenuItemExt::createToggler(spriteOn2, spriteOff2, [this](CCMenuItemToggler *sender) { RecordLayer::toggleRender(sender); });
    renderToggle->toggle(g.renderer.recording);
    renderToggle->setPosition(ccp(-65.5, -100));
    menu->addChild(renderToggle);
#else
    ButtonSprite *spriteOn2 = ButtonSprite::create("N/A");
    spriteOn2->setScale(0.74f);
    ButtonSprite *spriteOff2 = ButtonSprite::create("N/A");
    spriteOff2->setScale(0.74f);

    renderToggle = CCMenuItemExt::createToggler(spriteOff2, spriteOn2, [this](CCMenuItemToggler *sender) { RecordLayer::toggleRender2(sender); });
    renderToggle->setPosition(ccp(-65.5, -100));
    menu->addChild(renderToggle);
#endif

    spr = CCSprite::createWithSpriteFrameName("GJ_infoIcon_001.png");
    spr->setScale(0.65f);
    btn = CCMenuItemExt::createSpriteExtra(spr, [this](CCMenuItemSpriteExtra *sender) { RecordLayer::macroInfo(sender); });
    btn->setPosition(ccp(-36, 107));
    menu->addChild(btn);

    spr = CCSprite::createWithSpriteFrameName("GJ_arrow_01_001.png");
    spr->setScale(0.58f);
    btn = CCMenuItemExt::createSpriteExtra(spr, [this](CCMenuItemSpriteExtra *sender) { RecordLayer::updatePage(sender); });
    btn->setPosition(ccp(-5, 0));
    btn->setID("page-left");
    menu->addChild(btn);

    spr = CCSprite::createWithSpriteFrameName("GJ_arrow_01_001.png");
    spr->setScale(0.58f);
    spr->setScaleX(-0.58f);
    btn = CCMenuItemExt::createSpriteExtra(spr, [this](CCMenuItemSpriteExtra *sender) { RecordLayer::updatePage(sender); });
    btn->setPosition(ccp(209, 4.3));
    btn->setContentSize({26, 32.4});
    menu->addChild(btn);
    static_cast<CCNode *>(btn->getChildren()->objectAtIndex(0))
        ->setPositionX(13);

    for (int i = 0; i < 7; i++) {
        CCLabelBMFont *lbl =
            CCLabelBMFont::create("_______________________", "chatFont.fnt");
        lbl->setPosition(ccp(103, 97 - (i * 29)));
        lbl->setColor(ccc3(0, 0, 0));
        lbl->setOpacity(80);
        menu->addChild(lbl);
    }

    if (!mod->getSettingValue<bool>("restore_page"))
        g.currentPage = 0;

    goToSettingsPage(g.currentPage);

    CCSprite *dickordSpr =
        CCSprite::createWithSpriteFrameName("gj_discordIcon_001.png");
    dickordSpr->setScale(0.9f);
    CCMenuItemSpriteExtra *dickordBtn = CCMenuItemExt::createSpriteExtra(dickordSpr, [this](CCMenuItemSpriteExtra *sender) { RecordLayer::onDiscord(sender); });
    dickordBtn->setPosition((CCDirector::sharedDirector()->getWinSize() / 2 -
                             m_size / 2 + ccp(-16, 16)));
    m_buttonMenu->addChild(dickordBtn);

    if (!Mod::get()->setSavedValue<bool>("dickord", true))
        dickordSpr->runAction(CCSequence::create(
            CCScaleTo::create(0.25f, 1.5f), CCRotateTo::create(0.25f, 90),
            CCRotateTo::create(0.25f, 180), CCRotateTo::create(0.25f, 270),
            CCRotateTo::create(0.25f, 0), CCScaleTo::create(0.25f, 0.9f),
            nullptr));

    return true;
}

void RecordLayer::setToggleMember(CCMenuItemToggler *toggle, std::string id) {
    auto &g = Global::get();
    if (id == "macro_speedhack_enabled")
        speedhackToggle = toggle;
    if (id == "macro_show_trajectory")
        trajectoryToggle = toggle;
    if (id == "macro_noclip")
        noclipToggle = toggle;
    if (id == "macro_frame_stepper")
        frameStepperToggle = toggle;
    if (id == "macro_tps_enabled")
        tpsToggle = toggle;
    if (id == "macro_always_practice_fixes")
        g.alwaysPracticeFixes = toggle->isToggled();
}

void RecordLayer::loadSetting(RecordSetting sett, float yPos) {
    CCLabelBMFont *lbl =
        CCLabelBMFont::create(sett.name.c_str(), "bigFont.fnt");
    lbl->setPosition(ccp(19.f, yPos));
    lbl->setAnchorPoint({0, 0.5});
    lbl->setOpacity(200);
    lbl->setScale(sett.labelScale);

    nodes.push_back(static_cast<CCNode *>(lbl));
    menu->addChild(lbl);

    float toggleScale = 0.555f;

    if (sett.disabled) {
        // Code when disabled xD!
    }

    CCMenuItemToggler *toggle = CCMenuItemExt::createTogglerWithStandardSprites(toggleScale, [this](CCMenuItemToggler *sender) { RecordLayer::toggleSetting(sender); });
    toggle->setPosition(ccp(175, yPos));
    toggle->toggle(mod->getSavedValue<bool>(sett.id));
    toggle->setID(sett.id.c_str());

    nodes.push_back(static_cast<CCNode *>(toggle));
    menu->addChild(toggle);

    setToggleMember(toggle, sett.id);

    if (sett.input == InputType::None)
        return;

    if (sett.input == InputType::Settings) {
        CCSprite *spr =
            CCSprite::createWithSpriteFrameName("GJ_optionsBtn_001.png");
        spr->setScale(0.41f);
        spr->setOpacity(215);

        CCMenuItemSpriteExtra *btn =
            CCMenuItemExt::createSpriteExtra(spr, [this, sett](CCMenuItemSpriteExtra* sender) {
                if (sett.callback) {
                    (this->*sett.callback)(sender);
                }
            });
        btn->setPosition(ccp(138, yPos));

        nodes.push_back(static_cast<CCNode *>(btn));
        menu->addChild(btn);
    }

    if (sett.input == InputType::Autosave) {
        CCSprite *emptyBtn =
            CCSprite::createWithSpriteFrameName("GJ_plainBtn_001.png");
        emptyBtn->setScale(0.469f);

        CCSprite *folderIcon =
            CCSprite::createWithSpriteFrameName("folderIcon_001.png");
        folderIcon->setPosition(emptyBtn->getContentSize() / 2);
        folderIcon->setScale(0.7f);
        emptyBtn->addChild(folderIcon);

        CCMenuItemSpriteExtra *btn = CCMenuItemExt::createSpriteExtra(
            emptyBtn, [this](CCMenuItemSpriteExtra *sender) {
                this->onAutosaves(sender);
            });
        btn->setPosition(ccp(147, yPos));

        nodes.push_back(static_cast<CCNode *>(btn));
        menu->addChild(btn);
    }

    if (sett.input == InputType::Speedhack) {
        speedhackInput = TextInput::create(32.f, "SH", "chatFont.fnt");
        speedhackInput->setPosition(ccp(127.5, yPos));
        speedhackInput->setFilter("0123456789.");
        speedhackInput->setString(mod->getSavedValue<std::string>("macro_speedhack"));
        speedhackInput->setDelegate(this);
        speedhackInput->setMaxCharCount(6);
        speedhackInput->getInputNode()->m_textLabel->setAnchorPoint({0.5f, 0.5f});
        speedhackInput->getInputNode()->m_textField->setAnchorPoint({0.5f, 0.5f});
        speedhackInput->getInputNode()->m_textLabel->setScale(0.6);
        speedhackInput->getInputNode()->m_textLabel->setOpacity(150);
        speedhackInput->getInputNode()->setMaxLabelScale(0.7f);
        speedhackInput->setWidth(35.5f);
        speedhackInput->getBGSprite()->setContentHeight(39.f);
        speedhackInput->getBGSprite()->setOpacity(75);

        nodes.push_back(static_cast<CCNode *>(speedhackInput));
        menu->addChild(speedhackInput);
    }

    if (sett.input == InputType::Tps) {
        tpsInput = TextInput::create(32.f, "tps", "chatFont.fnt");
        tpsInput->setPosition(ccp(133.5, yPos));
        tpsInput->setFilter("0123456789.");
        tpsInput->setString(
            fmt::format("{:.3}", Mod::get()->getSavedValue<double>("macro_tps"))
        );
        tpsInput->setDelegate(this);
        tpsInput->setMaxCharCount(9);
        tpsInput->getInputNode()->m_textLabel->setAnchorPoint({0.5f, 0.5f});
        tpsInput->getInputNode()->m_textField->setAnchorPoint({0.5f, 0.5f});
        tpsInput->getInputNode()->m_textLabel->setScale(0.6);
        tpsInput->getInputNode()->m_textLabel->setOpacity(150);
        tpsInput->getInputNode()->setMaxLabelScale(0.7f);
        tpsInput->setWidth(35.5f);
        tpsBg = tpsInput->getBGSprite();
        tpsBg->setContentHeight(39.f);
        tpsBg->setOpacity(75);

        nodes.push_back(static_cast<CCNode *>(tpsInput));
        menu->addChild(tpsInput);
    }

    if (sett.input == InputType::Seed) {
        seedInput = TextInput::create(85.f, "Seed", "chatFont.fnt");
        seedInput->setPosition(ccp(109.5, yPos));
        seedInput->setFilter("0123456789");
        seedInput->setString(mod->getSavedValue<std::string>("macro_seed"));
        seedInput->setDelegate(this);
        seedInput->setMaxCharCount(20);
        seedInput->getInputNode()->m_textLabel->setAnchorPoint({0.5f, 0.5f});
        seedInput->getInputNode()->m_textField->setAnchorPoint({0.5f, 0.5f});
        seedInput->getInputNode()->m_textLabel->setScale(0.6);
        seedInput->getInputNode()->m_textLabel->setOpacity(150);
        seedInput->getInputNode()->setMaxLabelScale(0.7f);
        seedInput->setWidth(91.625f);
        seedInput->getBGSprite()->setContentHeight(39.f);
        seedInput->getBGSprite()->setOpacity(75);

        nodes.push_back(static_cast<CCNode *>(seedInput));
        menu->addChild(seedInput);
    }

    if (sett.input == InputType::Respawn) {
        respawnInput = TextInput::create(32.f, "sec", "chatFont.fnt");
        respawnInput->setPosition(ccp(127.5, yPos));
        respawnInput->setFilter("0123456789.");
        respawnInput->setString(
            fmt::format("{:.2}", mod->getSavedValue<double>("respawn_time"))
        );
        respawnInput->setDelegate(this);
        respawnInput->setMaxCharCount(4);
        respawnInput->getInputNode()->m_textLabel->setAnchorPoint({0.5f, 0.5f});
        respawnInput->getInputNode()->m_textField->setAnchorPoint({0.5f, 0.5f});
        respawnInput->getInputNode()->m_textLabel->setScale(0.6);
        respawnInput->getInputNode()->m_textLabel->setOpacity(150);
        respawnInput->getInputNode()->setMaxLabelScale(0.7f);
        respawnInput->setWidth(35.5f);
        respawnInput->getBGSprite()->setContentHeight(39.f);
        respawnInput->getBGSprite()->setOpacity(75);

        nodes.push_back(static_cast<CCNode *>(respawnInput));
        menu->addChild(respawnInput);
    }
}

void RecordLayer::goToSettingsPage(int page) {
    checkSpeedhack();

    for (size_t i = 0; i < nodes.size(); i++)
        nodes[i]->removeFromParentAndCleanup(false);

    nodes.clear();

    speedhackToggle = nullptr;
    frameStepperToggle = nullptr;
    trajectoryToggle = nullptr;
    noclipToggle = nullptr;
    tpsToggle = nullptr;

    speedhackInput = nullptr;
    respawnInput = nullptr;
    seedInput = nullptr;
    tpsInput = nullptr;

    tpsBg = nullptr;

    for (size_t i = 0; i < 6; i++)
        loadSetting(settings[page][i], ySettingPositions[i]);

    updateDots();
    updateTPS();

    Mod::get()->setSavedValue("current_page", page);
}

void RecordLayer::onDiscord(CCObject *) {
    geode::createQuickPopup(
        "Discord",
        "Join the <cb>Discord</c> server?\n(<cl>discord.gg/w6yvdzVzBd</c>).",
        "No", "Yes", [](auto, bool btn2) {
            if (btn2)
                geode::utils::web::openLinkInBrowser(
                    "https://discord.gg/w6yvdzVzBd");
        });
}

void RecordLayer::updateTPS() {
    if (!tpsInput || !tpsToggle || !tpsBg)
        return;
    auto &g = Global::get();

    tpsToggle->toggle(g.tpsEnabled);
    tpsInput->setString(
        fmt::format("{:.3}", Mod::get()->getSavedValue<double>("macro_tps")));

    if (g.state == state::none || g.macro.inputs.empty()) {
        if (CCMenuItemSpriteExtra *btn =
                tpsToggle->getChildByType<CCMenuItemSpriteExtra>(0))
            if (CCSprite *spr = btn->getChildByType<CCSprite>(0))
                spr->setOpacity(255);
        if (CCMenuItemSpriteExtra *btn =
                tpsToggle->getChildByType<CCMenuItemSpriteExtra>(1))
            if (CCSprite *spr = btn->getChildByType<CCSprite>(0))
                spr->setOpacity(255);

        tpsBg->setOpacity(75);
        tpsToggle->setEnabled(true);
        tpsInput->setEnabled(true);
        tpsInput->getInputNode()->m_textLabel->setOpacity(150);
        tpsInput->defocus();
    } else {
        if (CCMenuItemSpriteExtra *btn =
                tpsToggle->getChildByType<CCMenuItemSpriteExtra>(0))
            if (CCSprite *spr = btn->getChildByType<CCSprite>(0))
                spr->setOpacity(120);
        if (CCMenuItemSpriteExtra *btn =
                tpsToggle->getChildByType<CCMenuItemSpriteExtra>(1))
            if (CCSprite *spr = btn->getChildByType<CCSprite>(0))
                spr->setOpacity(120);

        tpsBg->setOpacity(30);
        tpsToggle->setEnabled(false);
        tpsInput->setEnabled(false);
        tpsInput->getInputNode()->m_textLabel->setOpacity(120);
        tpsInput->defocus();
    }
}
