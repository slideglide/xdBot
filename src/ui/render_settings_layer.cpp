#include "render_settings_layer.hpp"
#include "record_layer.hpp"

#include <Geode/modify/SliderTouchLogic.hpp>

class $modify(SliderTouchLogic) {
    bool ccTouchBegan(cocos2d::CCTouch* v1, cocos2d::CCEvent* v2) {
        if (std::string_view(m_slider->getID()) == "disabled-slider"_spr)
            return false;
        return SliderTouchLogic::ccTouchBegan(v1, v2);
    }
};

void RenderSettingsLayer::textChanged(CCTextInputNode* node) {

    if (secondsInput->getString() != "" && node == secondsInput->getInputNode()) {
        std::string value = secondsInput->getString();
        if (value == ".")
            secondsInput->setString("0.");
        else if (std::count(value.begin(), value.end(), '.') == 2)
            return secondsInput->setString(
                mod->getSavedValue<std::string>("render_seconds_after").c_str());
    }

    mod->setSavedValue("render_seconds_after", std::string(secondsInput->getString()));
    mod->setSavedValue("render_args", std::string(argsInput->getString()));
    mod->setSavedValue("render_audio_args", std::string(audioArgsInput->getString()));
    mod->setSavedValue("render_video_args", std::string(videoArgsInput->getString()));
    mod->setSavedValue("render_fade_in_time", std::string(fadeInInput->getString()));
    mod->setSavedValue("render_fade_out_time", std::string(fadeOutInput->getString()));
    mod->setSavedValue("render_file_extension", std::string(extensionInput->getString()));
}

void RenderSettingsLayer::onDefaults(CCObject*) {
    geode::createQuickPopup(
        "Restore",
        "<cr>Restore</c> default render settings?",
        "Cancel",
        "Yes",
        [this](auto, bool btn2) {
            if (!btn2)
                return;
            auto& g = Global::get();

            g.mod->setSavedValue("render_args", std::string("-pix_fmt yuv420p"));
            g.mod->setSavedValue("render_audio_args", std::string(""));
            g.mod->setSavedValue("render_video_args",
                                 std::string("colorspace=all=bt709:iall=bt470bg:fast=1"));
            g.mod->setSavedValue("render_only_song", false);
            g.mod->setSavedValue("render_music_volume", 1.0);
            g.mod->setSavedValue("render_sfx_volume", 1.0);
            g.mod->setSavedValue("render_file_extension", std::string(".mp4"));
            g.mod->setSavedValue("render_fade_in", false);
            g.mod->setSavedValue("render_fade_out", false);
            g.mod->setSavedValue("render_fade_in_time", geode::utils::numToString(2));
            g.mod->setSavedValue("render_fade_out_time", geode::utils::numToString(2));
            g.mod->setSavedValue("render_hide_endscreen", false);
            g.mod->setSavedValue("render_hide_levelcomplete", false);

            auto children = CCScene::get()->getChildrenExt();
            for (auto obj : children) {
                if (auto layer = typeinfo_cast<RecordLayer*>(obj)) {
                    layer->onClose(nullptr);
                    break;
                }
            }

            this->keyBackClicked();
            static_cast<RecordLayer*>(Global::get().layer)->openMenu(true);
            RenderSettingsLayer* layer = create();
            layer->m_noElasticity = true;
            layer->show();
        });
}

bool RenderSettingsLayer::init() {
    if (!Popup::init(396, 277, Utils::getTexture().c_str()))
        return false;
    setTitle("Render Settings");
#ifndef GEODE_IS_IOS
    bool usingApi = Renderer::shouldUseAPI();
#endif

    cocos2d::CCPoint offset =
        (CCDirector::sharedDirector()->getWinSize() - m_mainLayer->getContentSize()) / 2;
    m_mainLayer->setPosition(m_mainLayer->getPosition() - offset);
    m_closeBtn->setPosition(m_closeBtn->getPosition() + offset);
    m_bgSprite->setPosition(m_bgSprite->getPosition() + offset);
    m_title->setPosition(m_title->getPosition() + offset);

    mod = Mod::get();
    Utils::setBackgroundColor(m_bgSprite);

    if (mod->getSavedValue<std::string>("render_seconds_after") == "")
        mod->setSavedValue("render_seconds_after", geode::utils::numToString(0));

    CCMenu* menu = CCMenu::create();
    m_mainLayer->addChild(menu);
    menu->setPositionX(menu->getPositionX() - 67);

    CCLabelBMFont* lbl = CCLabelBMFont::create("Extra Args:", "bigFont.fnt");
    lbl->setPosition(ccp(-105, 88));
    lbl->setAnchorPoint({0, 0.5});
#ifndef GEODE_IS_IOS
    lbl->setOpacity(usingApi ? 90 : 200);
#else
    lbl->setOpacity(200);
#endif
    lbl->setScale(0.325);
    menu->addChild(lbl);

    argsInput = TextInput::create(170.f, "args", "chatFont.fnt");
    argsInput->setPosition(ccp(41.5f, 87.25f));
    argsInput->setString(mod->getSavedValue<std::string>("render_args"));
    argsInput->setDelegate(this);
    argsInput->setFilter(" 0123456789abcdefghijklmnopqrstuvwxyz-_:;.\"\\/[](){}+=<>|!*&'%@");
    argsInput->getInputNode()->setLabelPlaceholderColor(ccc3(163, 135, 121));
    argsInput->getInputNode()->m_textLabel->setAnchorPoint({0.5f, 0.5f});
    argsInput->getInputNode()->m_textLabel->setOpacity(150);
    argsInput->getInputNode()->m_textField->setAnchorPoint({0.5f, 0.5f});
    argsInput->setWidth(139.f);
    argsInput->getBGSprite()->setContentHeight(39.f);
#ifndef GEODE_IS_IOS
    argsInput->getBGSprite()->setOpacity(usingApi ? 40 : 75);
#else
    argsInput->getBGSprite()->setOpacity(75);
#endif
    menu->addChild(argsInput);

    lbl = CCLabelBMFont::create("Audio Args:", "bigFont.fnt");
    lbl->setPosition(ccp(-105, 55));
    lbl->setAnchorPoint({0, 0.5});
#ifndef GEODE_IS_IOS
    lbl->setOpacity(usingApi ? 90 : 200);
#else
    lbl->setOpacity(200);
#endif
    lbl->setScale(0.325);
    menu->addChild(lbl);

    audioArgsInput = TextInput::create(165.f, "audio args", "chatFont.fnt");
    audioArgsInput->setPosition(ccp(41.f, 55.25f));
    audioArgsInput->setString(mod->getSavedValue<std::string>("render_audio_args"));
    audioArgsInput->setDelegate(this);
    audioArgsInput->setFilter(" 0123456789abcdefghijklmnopqrstuvwxyz-_:;.\"\\/[](){}+=<>|!*&'%@");
    audioArgsInput->getInputNode()->setLabelPlaceholderColor(ccc3(163, 135, 121));
    audioArgsInput->getInputNode()->m_textLabel->setAnchorPoint({0.5f, 0.5f});
    audioArgsInput->getInputNode()->m_textLabel->setOpacity(150);
    audioArgsInput->getInputNode()->m_textField->setAnchorPoint({0.5f, 0.5f});
    audioArgsInput->setWidth(142.25f);
    audioArgsInput->getBGSprite()->setContentHeight(39.f);
#ifndef GEODE_IS_IOS
    audioArgsInput->getBGSprite()->setOpacity(usingApi ? 40 : 75);
#else
    audioArgsInput->getBGSprite()->setOpacity(75);
#endif
    menu->addChild(audioArgsInput);

    lbl = CCLabelBMFont::create("Video Args:", "bigFont.fnt");
    lbl->setAnchorPoint({0, 0.5});
#ifndef GEODE_IS_IOS
    lbl->setOpacity(usingApi ? 90 : 200);
#else
    lbl->setOpacity(200);
#endif
    lbl->setScale(0.325f);
    lbl->setPosition({-105, 24});
    menu->addChild(lbl);

    videoArgsInput = TextInput::create(165.f, "video args", "chatFont.fnt");
    videoArgsInput->setPosition({41.f, 24.25f});
#ifndef GEODE_IS_IOS
    videoArgsInput->setString(usingApi ? "" : mod->getSavedValue<std::string>("render_video_args"));
#else
    videoArgsInput->setString(mod->getSavedValue<std::string>("render_video_args"));
#endif
    videoArgsInput->setDelegate(this);
    videoArgsInput->setFilter(" 0123456789abcdefghijklmnopqrstuvwxyz-_:;.\"\\/[](){}+=<>|!*&'%@");
    videoArgsInput->getInputNode()->setLabelPlaceholderColor(ccc3(163, 135, 121));
    videoArgsInput->getInputNode()->m_textLabel->setAnchorPoint({0.5f, 0.5f});
    videoArgsInput->getInputNode()->m_textLabel->setOpacity(150);
    videoArgsInput->getInputNode()->m_textField->setAnchorPoint({0.5f, 0.5f});
    videoArgsInput->setWidth(140.25f);
    videoArgsInput->getBGSprite()->setContentHeight(39.f);
#ifndef GEODE_IS_IOS
    videoArgsInput->getBGSprite()->setOpacity(usingApi ? 40 : 75);
#else
    videoArgsInput->getBGSprite()->setOpacity(75);
#endif
    menu->addChild(videoArgsInput);

    lbl = CCLabelBMFont::create("Render after completion:", "bigFont.fnt");
    lbl->setPosition(ccp(-105, -5));
    lbl->setAnchorPoint({0, 0.5});
    lbl->setOpacity(200);
    lbl->setScale(0.325);
    menu->addChild(lbl);

    secondsInput = TextInput::create(47.f, "sec", "chatFont.fnt");
    secondsInput->setPosition(ccp(64.5f, -6.25f));
    secondsInput->setString(mod->getSavedValue<std::string>("render_seconds_after"));
    secondsInput->setDelegate(this);
    secondsInput->setFilter("0123456789.");
    secondsInput->setMaxCharCount(2);
    secondsInput->getInputNode()->setLabelPlaceholderColor(ccc3(163, 135, 121));
    secondsInput->getInputNode()->m_textLabel->setAnchorPoint({0.5f, 0.5f});
    secondsInput->getInputNode()->m_textLabel->setOpacity(150);
    secondsInput->getInputNode()->m_textField->setAnchorPoint({0.5f, 0.5f});
    secondsInput->setWidth(30.75f);
    secondsInput->getBGSprite()->setContentHeight(41.25f);
    secondsInput->getBGSprite()->setOpacity(75);
    menu->addChild(secondsInput);

    lbl = CCLabelBMFont::create("s", "chatFont.fnt");
    lbl->setPosition(ccp(84, -5));
    lbl->setAnchorPoint({0, 0.5});
    lbl->setScale(0.825);
    menu->addChild(lbl);

    lbl = CCLabelBMFont::create("File Extension:", "bigFont.fnt");
    lbl->setPosition(ccp(110, -5));
    lbl->setAnchorPoint({0, 0.5});
    lbl->setOpacity(200);
    lbl->setScale(0.3f);
    menu->addChild(lbl);

    extensionInput = TextInput::create(46.f, "", "chatFont.fnt");
    extensionInput->setScale(0.8f);
    extensionInput->setPosition(ccp(209, -5));
    extensionInput->setString(
        Mod::get()->getSavedValue<std::string>("render_file_extension").c_str());
    extensionInput->getInputNode()->setDelegate(this);
    extensionInput->getInputNode()->setAllowedChars("abcdefghijklmnñopqrstuvwxyz0123456789.");
    menu->addChild(extensionInput);

    lbl = CCLabelBMFont::create("Fade In:", "bigFont.fnt");
    lbl->setPosition(ccp(25, -32));
    lbl->setAnchorPoint({0, 0.5});
    lbl->setOpacity(200);
    lbl->setScale(0.325);
    menu->addChild(lbl);

    fadeInInput = TextInput::create(50.f, "s", "bigFont.fnt");
    fadeInInput->setScale(0.5f);
    fadeInInput->setPosition(ccp(100, -32));
    fadeInInput->setString(Mod::get()->getSavedValue<std::string>("render_fade_in_time").c_str());
    fadeInInput->getInputNode()->setDelegate(this);
    fadeInInput->getInputNode()->setAllowedChars("0123456789.");
    menu->addChild(fadeInInput);

    CCMenuItemToggler* toggle =
        CCMenuItemExt::createTogglerWithStandardSprites(0.555f, [this](CCMenuItemToggler* sender) {
            static_cast<RecordLayer*>(Global::get().layer)->toggleSetting(sender);
        });
    toggle->setPosition(ccp(130, -32));
    toggle->toggle(mod->getSavedValue<bool>("render_fade_in"));
    toggle->setID("render_fade_in");
    menu->addChild(toggle);

    lbl = CCLabelBMFont::create("Fade Out:", "bigFont.fnt");
    lbl->setPosition(ccp(25, -58));
    lbl->setAnchorPoint({0, 0.5});
    lbl->setOpacity(200);
    lbl->setScale(0.325);
    menu->addChild(lbl);

    fadeOutInput = TextInput::create(50.f, "s", "bigFont.fnt");
    fadeOutInput->setScale(0.5f);
    fadeOutInput->setPosition(ccp(100, -58));
    fadeOutInput->setString(Mod::get()->getSavedValue<std::string>("render_fade_out_time").c_str());
    fadeOutInput->getInputNode()->setDelegate(this);
    fadeOutInput->getInputNode()->setAllowedChars("0123456789.");
    menu->addChild(fadeOutInput);

    toggle =
        CCMenuItemExt::createTogglerWithStandardSprites(0.555f, [this](CCMenuItemToggler* sender) {
            static_cast<RecordLayer*>(Global::get().layer)->toggleSetting(sender);
        });
    toggle->setPosition(ccp(130, -58));
    toggle->toggle(mod->getSavedValue<bool>("render_fade_out"));
    toggle->setID("render_fade_out");
    menu->addChild(toggle);

    lbl = CCLabelBMFont::create("Hide Endscreen:", "bigFont.fnt");
    lbl->setPosition(ccp(-105, -32));
    lbl->setAnchorPoint({0, 0.5});
    lbl->setOpacity(200);
    lbl->setScale(0.325);
    menu->addChild(lbl);

    toggle =
        CCMenuItemExt::createTogglerWithStandardSprites(0.555f, [this](CCMenuItemToggler* sender) {
            static_cast<RecordLayer*>(Global::get().layer)->toggleSetting(sender);
        });
    toggle->setPosition(ccp(0, -32));
    toggle->toggle(mod->getSavedValue<bool>("render_hide_endscreen"));
    toggle->setID("render_hide_endscreen");
    menu->addChild(toggle);

    lbl = CCLabelBMFont::create("Hide Level Complete:", "bigFont.fnt");
    lbl->setPosition(ccp(-105, -58));
    lbl->setAnchorPoint({0, 0.5});
    lbl->setOpacity(200);
    lbl->setScale(0.25);
    menu->addChild(lbl);

    toggle =
        CCMenuItemExt::createTogglerWithStandardSprites(0.555f, [this](CCMenuItemToggler* sender) {
            static_cast<RecordLayer*>(Global::get().layer)->toggleSetting(sender);
        });
    toggle->setPosition(ccp(0, -58));
    toggle->toggle(mod->getSavedValue<bool>("render_hide_levelcomplete"));
    toggle->setID("render_hide_levelcomplete");
    menu->addChild(toggle);

    lbl = CCLabelBMFont::create("SFX Volume", "goldFont.fnt");
    lbl->setScale(0.475f);
    lbl->setPosition({188, 42});
    menu->addChild(lbl);

    sfxSlider = Slider::create(this, menu_selector(RenderSettingsLayer::onSlider), 1.f);
    sfxSlider->setPosition({188, 24});
    sfxSlider->setAnchorPoint({0.f, 0.f});
    sfxSlider->setScale(0.545f);
    sfxSlider->setValue(Mod::get()->getSavedValue<double>("render_sfx_volume"));
    menu->addChild(sfxSlider);

    lbl = CCLabelBMFont::create("Music Volume", "goldFont.fnt");
    lbl->setScale(0.475f);
    lbl->setPosition({188, 87});
    menu->addChild(lbl);

    musicSlider = Slider::create(this, menu_selector(RenderSettingsLayer::onSlider), 1.f);
    musicSlider->setPosition({188, 69});
    musicSlider->setAnchorPoint({0.f, 0.f});
    musicSlider->setScale(0.545f);
    musicSlider->setValue(Mod::get()->getSavedValue<double>("render_music_volume"));
    menu->addChild(musicSlider);

    ButtonSprite* spr = ButtonSprite::create("OK");
    spr->setScale(0.875);
    CCMenuItemSpriteExtra* btn =
        CCMenuItemExt::createSpriteExtra(spr, [this](CCMenuItemSpriteExtra* sender) {
            RenderSettingsLayer::close(sender);
        });
    btn->setPosition(ccp(67, -83));
    menu->addChild(btn);

    spr = ButtonSprite::create("Restore Defaults");
    spr->setScale(0.375f);
    btn = CCMenuItemExt::createSpriteExtra(spr, [this](CCMenuItemSpriteExtra* sender) {
        RenderSettingsLayer::onDefaults(sender);
    });
    btn->setPosition({211, -106});
    menu->addChild(btn);

#ifndef GEODE_IS_IOS
    if (usingApi) {
        argsInput->setEnabled(false);
        argsInput->getInputNode()->m_textLabel->setOpacity(100);
        audioArgsInput->setEnabled(false);
        audioArgsInput->getInputNode()->m_textLabel->setOpacity(100);
        videoArgsInput->setEnabled(false);
        videoArgsInput->getInputNode()->m_textLabel->setOpacity(100);

        extensionInput->setEnabled(false);
        extensionInput->getInputNode()->m_textLabel->setOpacity(100);
        extensionInput->getBGSprite()->setOpacity(40);

        fadeOutInput->setEnabled(false);
        fadeOutInput->getInputNode()->m_textLabel->setOpacity(100);
        fadeOutInput->getBGSprite()->setOpacity(40);

        fadeInInput->setEnabled(false);
        fadeInInput->getInputNode()->m_textLabel->setOpacity(100);
        fadeInInput->getBGSprite()->setOpacity(40);

        sfxSlider->setID("disabled-slider"_spr);
        musicSlider->setID("disabled-slider"_spr);
        sfxSlider->m_sliderBar->setOpacity(100);
        sfxSlider->m_groove->setOpacity(100);
        sfxSlider->m_touchLogic->setOpacity(100);
        musicSlider->m_sliderBar->setOpacity(100);
        musicSlider->m_groove->setOpacity(100);
        musicSlider->m_touchLogic->setOpacity(100);
    }
#endif

    return true;
}

void RenderSettingsLayer::onSlider(CCObject*) {
    Mod::get()->setSavedValue("render_sfx_volume", sfxSlider->getValue());
    Mod::get()->setSavedValue("render_music_volume", musicSlider->getValue());
}

void RenderSettingsLayer::showInfoPopup(CCObject*) {
    FLAlertLayer::create("Record Audio",
                         "Records the game's audio and adds it to the video, "
                         "capturing all song and SFX triggers.",
                         "OK")
        ->show();
}
