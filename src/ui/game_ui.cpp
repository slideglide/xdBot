#include "game_ui.hpp"

#include <Geode/modify/PlayLayer.hpp>

class $modify(PlayLayer) {

    struct Fields {
        CCLabelBMFont* frameLabel = nullptr;
    };

    void postUpdate(float dt) {
        PlayLayer::postUpdate(dt);
        auto& g = Global::get();

#ifndef GEODE_IS_IOS
        if (g.state != state::none && g.frameLabel && !g.renderer.recording) {
            m_fields->frameLabel->setString(
                ("Frame: " + geode::utils::numToString(Global::getCurrentFrame())).c_str());
        }
#else
        if (g.state != state::none && g.frameLabel) {
            m_fields->frameLabel->setString(
                ("Frame: " + geode::utils::numToString(Global::getCurrentFrame())).c_str());
        }
#endif
    }

    void setupHasCompleted() {
        PlayLayer::setupHasCompleted();

        Interface::addLabels(this);
        Interface::addButtons(this);

        CCMenu* uiMenu = static_cast<CCMenu*>(this->getChildByID("ui-menu"_spr));
        if (uiMenu) {
            m_fields->frameLabel =
                static_cast<CCLabelBMFont*>(uiMenu->getChildByID("frame-label"_spr));
        }
    }
};

void Interface::addLabels(PlayLayer* pl) {
    CCMenu* uiMenu = static_cast<CCMenu*>(pl->getChildByID("ui-menu"_spr));
    
    if (!uiMenu) {
        uiMenu = CCMenu::create();
        uiMenu->setPosition({0, 0});
        uiMenu->setZOrder(300);
        uiMenu->setID("ui-menu"_spr);
        uiMenu->setContentSize(CCDirector::sharedDirector()->getWinSize());
        uiMenu->setTouchEnabled(false);
        pl->addChild(uiMenu);
    }

    CCSize winSize = CCDirector::sharedDirector()->getWinSize();

    CCLabelBMFont* lbl = CCLabelBMFont::create("", "chatFont.fnt");
    lbl->setPosition({winSize.width - 6.5f, 12});
    lbl->setAnchorPoint({1, 0.5});
    lbl->setID("state-label"_spr);
    lbl->setZOrder(300);
    lbl->setScale(0.625f);
    uiMenu->addChild(lbl);

    lbl = CCLabelBMFont::create("", "chatFont.fnt");
    lbl->setPosition({6.5f, 12});
    lbl->setAnchorPoint({0, 0.5});
    lbl->setID("frame-label"_spr);
    lbl->setZOrder(300);
    lbl->setScale(0.625f);
    uiMenu->addChild(lbl);

    Interface::updateLabels();
}

void Interface::addButtons(PlayLayer* pl) {
    CCMenu* uiMenu = static_cast<CCMenu*>(pl->getChildByID("ui-menu"_spr));
    
    if (!uiMenu) {
        uiMenu = CCMenu::create();
        uiMenu->setPosition({0, 0});
        uiMenu->setZOrder(300);
        uiMenu->setID("ui-menu"_spr);
        uiMenu->setContentSize(CCDirector::sharedDirector()->getWinSize());
        pl->addChild(uiMenu);
    }

    auto stepBtn = Button::createWithSpriteFrameName("GJ_arrow_02_001.png", [](auto) {
        if (!Global::get().frameStepper)
            Global::toggleFrameStepper();
        else
            Global::frameStep();
    });
    static_cast<CCSprite*>(stepBtn->getDisplayNode())->setFlipX(true);
    static_cast<CCSprite*>(stepBtn->getDisplayNode())->setPosition({0, 0});
    stepBtn->setAnchorPoint({0, 0});
    stepBtn->setID("step-frame-btn"_spr);
    uiMenu->addChild(stepBtn);

    auto disableStepperBtn = Button::createWithSpriteFrameName("GJ_deleteIcon_001.png", [](auto) {
        if (Global::get().frameStepper)
            Global::toggleFrameStepper();
    });
    static_cast<CCSprite*>(disableStepperBtn->getDisplayNode())->setPosition({0, 0});
    disableStepperBtn->setAnchorPoint({0, 0});
    disableStepperBtn->setID("disable-stepper-btn"_spr);
    uiMenu->addChild(disableStepperBtn);

    auto speedHackBtn = Button::createWithSpriteFrameName("GJ_timeIcon_001.png", [](auto) {
        Global::toggleSpeedhack();
    });
    static_cast<CCSprite*>(speedHackBtn->getDisplayNode())->setPosition({0, 0});
    speedHackBtn->setAnchorPoint({0, 0});
    speedHackBtn->setID("speedhack-btn"_spr);
    uiMenu->addChild(speedHackBtn);

    Interface::updateButtons();
}

void Interface::updateLabels() {
    PlayLayer* pl = PlayLayer::get();
    auto& g = Global::get();

    if (!pl)
        return;

    CCMenu* uiMenu = static_cast<CCMenu*>(pl->getChildByID("ui-menu"_spr));
    if (!uiMenu)
        return;

    if (g.state == state::none || !g.frameLabel)
        static_cast<CCLabelBMFont*>(uiMenu->getChildByID("frame-label"_spr))->setString("");

    CCLabelBMFont* label =
        typeinfo_cast<CCLabelBMFont*>(uiMenu->getChildByID("state-label"_spr));

    if (!label)
        return;

    if (g.mod->getSavedValue<bool>("macro_hide_labels"))
        return label->setString("");

    state state = g.state;
    std::string labelText = state == state::none ? "" : "Playing";
    if (state == state::recording)
        labelText = "Recording";

    if (labelText == "Recording" && state == state::recording &&
        g.mod->getSavedValue<bool>("macro_hide_recording_label"))
        labelText = "";

    if (labelText == "Playing" && state == state::playing &&
        g.mod->getSavedValue<bool>("macro_hide_playing_label"))
        labelText = "";

#ifndef GEODE_IS_IOS
    if (g.renderer.recording && g.mod->getSavedValue<bool>("render_hide_labels")) {
        labelText = "";
        if (CCLabelBMFont* lbl =
                typeinfo_cast<CCLabelBMFont*>(uiMenu->getChildByID("frame-label"_spr)))
            lbl->setString("");
    }
#endif

    label->setString(labelText.c_str());
}

void Interface::updateButtons() {
    PlayLayer* pl = PlayLayer::get();
    if (!pl)
        return;

    CCMenu* uiMenu = static_cast<CCMenu*>(pl->getChildByID("ui-menu"_spr));
    if (!uiMenu)
        return;

    auto& g = Global::get();

#ifdef GEODE_IS_DESKTOP
    bool isDesktop = true;
#else
    bool isDesktop = false;
#endif

    CCNode* disableStepperBtn = uiMenu->getChildByID("disable-stepper-btn"_spr);
    CCNode* stepFrameBtn = uiMenu->getChildByID("step-frame-btn"_spr);
    CCNode* speedhackBtn = uiMenu->getChildByID("speedhack-btn"_spr);

    if (!disableStepperBtn || !stepFrameBtn || !speedhackBtn)
        return;

    disableStepperBtn->setPosition(ccp(g.mod->getSavedValue<float>("button_off_pos_x"),
                                       g.mod->getSavedValue<float>("button_off_pos_y")));

    float scale = g.mod->getSavedValue<float>("button_off_scale");

    CCSprite* sprite = disableStepperBtn->getChildByType<CCSprite>(0);
    sprite->setScale(scale);
    sprite->setOpacity(static_cast<int>(g.mod->getSavedValue<float>("button_off_opacity") * 255));
    sprite->setAnchorPoint({0, 0});

    cocos2d::CCSize size = sprite->getContentSize();
    disableStepperBtn->setContentSize({size.width * scale, size.height * scale});

    stepFrameBtn->setPosition(ccp(g.mod->getSavedValue<float>("button_advance_frame_pos_x"),
                                  g.mod->getSavedValue<float>("button_advance_frame_pos_y")));

    scale = g.mod->getSavedValue<float>("button_advance_frame_scale");

    sprite = stepFrameBtn->getChildByType<CCSprite>(0);
    sprite->setScale(scale);
    sprite->setOpacity(
        static_cast<int>(g.mod->getSavedValue<float>("button_advance_frame_opacity") * 255));
    sprite->setAnchorPoint({0, 0});

    size = sprite->getContentSize();
    stepFrameBtn->setContentSize({size.width * scale, size.height * scale});

    speedhackBtn->setPosition(ccp(g.mod->getSavedValue<float>("button_speedhack_pos_x"),
                                  g.mod->getSavedValue<float>("button_speedhack_pos_y")));

    scale = g.mod->getSavedValue<float>("button_speedhack_scale");

    sprite = speedhackBtn->getChildByType<CCSprite>(0);
    sprite->setScale(scale);
    sprite->setOpacity(
        static_cast<int>(g.mod->getSavedValue<float>("button_speedhack_opacity") * 255));
    sprite->setAnchorPoint({0, 0});

    size = sprite->getContentSize();
    speedhackBtn->setContentSize({size.width * scale, size.height * scale});

    if ((g.state != state::recording && !g.mod->getSavedValue<bool>("macro_always_show_buttons")) ||
        isDesktop) {
        disableStepperBtn->setVisible(false);
        stepFrameBtn->setVisible(false);
        speedhackBtn->setVisible(false);
        return;
    }

    speedhackBtn->setVisible(!g.mod->getSavedValue<bool>("macro_hide_speedhack"));

    if (g.mod->getSavedValue<bool>("macro_hide_stepper")) {
        disableStepperBtn->setVisible(false);
        stepFrameBtn->setVisible(false);
    } else {
        stepFrameBtn->setVisible(true);
        disableStepperBtn->setVisible(g.frameStepper);
    }
}