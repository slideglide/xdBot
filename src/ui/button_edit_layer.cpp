#include "button_edit_layer.hpp"
#include "game_ui.hpp"

namespace {
    void setDefaultButtonValues(geode::Mod* mod) {
        if (!mod)
            return;

        auto winSize = cocos2d::CCDirector::sharedDirector()->getWinSize();

        mod->setSavedValue("button_off_pos_x", 62.f);
        mod->setSavedValue("button_off_pos_y", winSize.height - 35.f);
        mod->setSavedValue("button_off_scale", 1.f);
        mod->setSavedValue("button_off_opacity", 1.f);

        mod->setSavedValue("button_advance_frame_pos_x", 100.f);
        mod->setSavedValue("button_advance_frame_pos_y", winSize.height - 50.f);
        mod->setSavedValue("button_advance_frame_scale", 0.9f);
        mod->setSavedValue("button_advance_frame_opacity", 1.f);

        mod->setSavedValue("button_speedhack_pos_x", winSize.width - 62.f);
        mod->setSavedValue("button_speedhack_pos_y", winSize.height - 38.f);
        mod->setSavedValue("button_speedhack_scale", 1.f);
        mod->setSavedValue("button_speedhack_opacity", 1.f);
    }

    float getSavedFloat(geode::Mod* mod, std::string const& key, float fallback) {
        if (!mod)
            return fallback;

        auto value = mod->getSavedValue<float>(key);
        if (!std::isfinite(value))
            return fallback;

        return value;
    }

    cocos2d::CCSprite* createCheckedSprite(char const* frameName) {
        auto spr = cocos2d::CCSprite::createWithSpriteFrameName(frameName);
        if (!spr)
            geode::log::error("ButtonEditLayer: missing sprite frame '{}'", frameName);
        return spr;
    }
}

$on_mod(Loaded) {
    auto mod = geode::Mod::get();
    if (!mod)
        return;

    if (!mod->getSavedValue<bool>("button_defaults5")) {
        setDefaultButtonValues(mod);
        mod->setSavedValue("button_defaults5", true);
    }
}

bool ButtonEditLayer::ccTouchBegan(cocos2d::CCTouch* touch, cocos2d::CCEvent* event) {
    if (!FLAlertLayer::ccTouchBegan(touch, event))
        return false;

    if (!touch)
        return false;

    if (movingButton.sprite)
        return true;

    std::string selected;
    auto location = touch->getLocation();

    for (size_t i = 0; i < spriteButtons.size(); i++) {
        auto spr = spriteButtons[i];
        if (!spr)
            continue;

        auto btnPos = spr->getPosition();
        auto btnSize = spr->getContentSize() * spr->getScale();

        if (ButtonEditLayer::isPointInButton(location, btnPos, btnSize)) {
            movingButton = {i, spr, location - btnPos};
            selected = indexToID[i];
        }
    }

    if (!selected.empty())
        updateSelected(selected);

    return true;
}

void ButtonEditLayer::ccTouchMoved(cocos2d::CCTouch* touch, cocos2d::CCEvent* event) {
    FLAlertLayer::ccTouchMoved(touch, event);

    if (!touch || !movingButton.sprite)
        return;

    if (movingButton.index >= indexToID.size())
        return;

    auto position = touch->getLocation() - movingButton.offset;
    movingButton.sprite->setPosition(position);
    positions[indexToID[movingButton.index]] = position;
}

void ButtonEditLayer::ccTouchEnded(cocos2d::CCTouch* touch, cocos2d::CCEvent* event) {
    FLAlertLayer::ccTouchEnded(touch, event);
    movingButton = {};
}

bool ButtonEditLayer::isPointInButton(
    cocos2d::CCPoint clickPos,
    cocos2d::CCPoint pos,
    cocos2d::CCSize size
) {
    return clickPos.x >= pos.x && clickPos.x <= pos.x + size.width &&
        clickPos.y >= pos.y && clickPos.y <= pos.y + size.height;
}

void ButtonEditLayer::updateSelectedLabels() {
    if (currentSelected >= indexToID.size() || !scaleLbl || !opacityLbl)
        return;

    auto const& selected = indexToID[currentSelected];

    auto scaleIt = scales.find(selected);
    auto opacityIt = opacities.find(selected);

    float scale = scaleIt != scales.end() ? scaleIt->second : 1.f;
    float opacity = opacityIt != opacities.end() ? opacityIt->second : 1.f;

    scaleLbl->setString(fmt::format("Scale ({:.1f})", scale).c_str());
    opacityLbl->setString(fmt::format("Opacity ({:.1f})", opacity).c_str());
}

void ButtonEditLayer::updateSelected(std::string const& selected) {
    auto indexIt = IDtoIndex.find(selected);
    if (indexIt == IDtoIndex.end()) {
        geode::log::warn("ButtonEditLayer: tried to select unknown button '{}'", selected);
        return;
    }

    currentSelected = indexIt->second;

    if (scaleSlider) {
        scaleSlider->removeFromParentAndCleanup(true);
        scaleSlider = nullptr;
    }

    if (opacitySlider) {
        opacitySlider->removeFromParentAndCleanup(true);
        opacitySlider = nullptr;
    }

    addSliders();
    updateSelectedLabels();

    if (selectedLbl) {
        auto nameIt = IDtoName.find(selected);
        selectedLbl->setString(nameIt != IDtoName.end() ? nameIt->second.c_str() : selected.c_str());
    }
}

bool ButtonEditLayer::init() {
    if (!Popup::init(200.f, 131.f, Utils::getTexture().c_str()))
        return false;

    mod = geode::Mod::get();
    if (!mod) {
        geode::log::error("ButtonEditLayer: Mod::get() returned nullptr");
        return false;
    }

    if (!mod->getSavedValue<bool>("button_defaults5")) {
        setDefaultButtonValues(mod);
        mod->setSavedValue("button_defaults5", true);
    }

    auto winSize = cocos2d::CCDirector::sharedDirector()->getWinSize();
    auto offset = (winSize - m_mainLayer->getContentSize()) / 2;
    m_mainLayer->setPosition(m_mainLayer->getPosition() - offset);

    if (m_closeBtn)
        m_closeBtn->setPosition(m_closeBtn->getPosition() + offset);

    if (m_bgSprite) {
        m_bgSprite->setPosition(m_bgSprite->getPosition() + offset);
        Utils::setBackgroundColor(m_bgSprite);
        m_bgSprite->setPositionY(231.f);
    }

    if (m_closeBtn)
        m_closeBtn->setVisible(false);

    setOpacity(207);

    menu = cocos2d::CCMenu::create();
    if (!menu)
        return false;

    m_mainLayer->addChild(menu);

    selectedLbl = cocos2d::CCLabelBMFont::create("Frame Stepper Off", "goldFont.fnt");
    if (selectedLbl) {
        selectedLbl->setScale(0.5f);
        selectedLbl->setPositionY(121.f);
        menu->addChild(selectedLbl);
    }

    auto bg = geode::NineSlice::create("square02b_001.png", {0, 0, 80, 80});
    if (bg) {
        bg->setColor({0, 0, 0});
        bg->setOpacity(78);
        bg->setPositionY(77.f);
        bg->setAnchorPoint({0.5f, 0.5f});
        bg->setContentSize({180.f, 60.f});
        menu->addChild(bg);
    }

    scaleLbl = cocos2d::CCLabelBMFont::create("Scale (1.0)", "chatFont.fnt");
    if (scaleLbl) {
        scaleLbl->setScale(0.625f);
        scaleLbl->setPosition({-79.f, 89.f});
        scaleLbl->setAnchorPoint({0.f, 0.5f});
        menu->addChild(scaleLbl);
    }

    opacityLbl = cocos2d::CCLabelBMFont::create("Opacity (1.0)", "chatFont.fnt");
    if (opacityLbl) {
        opacityLbl->setScale(0.625f);
        opacityLbl->setPosition({-79.f, 66.f});
        opacityLbl->setAnchorPoint({0.f, 0.5f});
        menu->addChild(opacityLbl);
    }

    auto saveSpr = ButtonSprite::create("Save");
    if (saveSpr) {
        saveSpr->setScale(0.6f);
        auto saveBtn = CCMenuItemExt::createSpriteExtra(saveSpr, [this](cocos2d::CCObject* sender) {
            saveButtonState();
            onClose(sender);
            Interface::updateButtons();
        });
        saveBtn->setPosition({48.f, 30.f});
        menu->addChild(saveBtn);
    }

    auto cancelSpr = ButtonSprite::create("Cancel");
    if (cancelSpr) {
        cancelSpr->setScale(0.6f);
        auto cancelBtn = CCMenuItemExt::createSpriteExtra(cancelSpr, [this](cocos2d::CCObject* sender) {
            onClose(sender);
        });
        cancelBtn->setPosition({-40.f, 30.f});
        menu->addChild(cancelBtn);
    }

    auto restoreSpr = ButtonSprite::create("Restore");
    if (restoreSpr) {
        restoreSpr->setScale(0.55f);
        auto restoreBtn = CCMenuItemExt::createSpriteExtra(restoreSpr, [this](cocos2d::CCObject*) {
            restoreDefaults();
        });
        restoreBtn->setPosition({242.f, -144.f});
        menu->addChild(restoreBtn);

        if (auto buttonSprite = restoreBtn->getChildByType<ButtonSprite>(0)) {
            if (auto slice = buttonSprite->getChildByType<geode::NineSlice>(0))
                slice->setOpacity(130);

            if (auto label = buttonSprite->getChildByType<cocos2d::CCLabelBMFont>(0))
                label->setOpacity(130);
        }
    }

    loadSavedButtonState();
    addSprites();
    updateSelected("button_advance_frame");

    return true;
}

void ButtonEditLayer::updateScale(geode::SliderNode*, float value) {
    if (currentSelected >= indexToID.size() || currentSelected >= spriteButtons.size())
        return;

    if (auto spr = spriteButtons[currentSelected])
        spr->setScale(value);

    scales[indexToID[currentSelected]] = value;
    updateSelectedLabels();
}

void ButtonEditLayer::updateOpacity(geode::SliderNode*, float value) {
    if (currentSelected >= indexToID.size() || currentSelected >= spriteButtons.size())
        return;

    int opacity = static_cast<int>(std::clamp(value, 0.f, 1.f) * 255.f);

    if (auto spr = spriteButtons[currentSelected])
        spr->setOpacity(opacity);

    opacities[indexToID[currentSelected]] = value;
    updateSelectedLabels();
}

void ButtonEditLayer::addSliders() {
    if (!menu || currentSelected >= indexToID.size())
        return;

    auto const& selected = indexToID[currentSelected];
    auto opacityIt = opacities.find(selected);
    auto scaleIt = scales.find(selected);

    float opacity = opacityIt != opacities.end() ? opacityIt->second : 1.f;
    float scale = scaleIt != scales.end() ? scaleIt->second : 1.f;

    opacitySlider = geode::SliderNode::create([this](geode::SliderNode* sender, float value) {
        updateOpacity(sender, value);
    });
    if (opacitySlider) {
        opacitySlider->setMin(0.05f);
        opacitySlider->setMax(1.f);
        opacitySlider->setPosition({31.f, 66.f});
        opacitySlider->setScale(0.4f);
        opacitySlider->setValue(opacity);
        menu->addChild(opacitySlider);
    }

    scaleSlider = geode::SliderNode::create([this](geode::SliderNode* sender, float value) {
        updateScale(sender, value);
    });
    if (scaleSlider) {
        scaleSlider->setMin(0.12f);
        scaleSlider->setMax(1.f);
        scaleSlider->setPosition({31.f, 89.f});
        scaleSlider->setScale(0.4f);
        scaleSlider->setValue(scale);
        menu->addChild(scaleSlider);
    }
}

void ButtonEditLayer::addSprites() {
    if (!mod)
        return;

    spriteButtons.clear();

    auto addButtonSprite = [this](
        char const* frameName,
        std::string const& id,
        bool flipX = false
    ) {
        auto spr = createCheckedSprite(frameName);
        if (!spr)
            return;

        auto posIt = positions.find(id);
        auto scaleIt = scales.find(id);
        auto opacityIt = opacities.find(id);

        auto position = posIt != positions.end()
            ? posIt->second
            : ccp(
                getSavedFloat(mod, id + "_pos_x", 0.f),
                getSavedFloat(mod, id + "_pos_y", 0.f)
            );

        auto scale = scaleIt != scales.end() ? scaleIt->second : getSavedFloat(mod, id + "_scale", 1.f);
        auto opacity = opacityIt != opacities.end()
            ? opacityIt->second
            : getSavedFloat(mod, id + "_opacity", 1.f);

        spr->setAnchorPoint({0.f, 0.f});
        spr->setFlipX(flipX);
        spr->setPosition(position);
        spr->setScale(scale);
        spr->setOpacity(static_cast<int>(std::clamp(opacity, 0.f, 1.f) * 255.f));
        m_mainLayer->addChild(spr);
        spriteButtons.push_back(spr);
    };

    addButtonSprite("GJ_deleteIcon_001.png", "button_off");
    addButtonSprite("GJ_arrow_02_001.png", "button_advance_frame", true);
    addButtonSprite("GJ_timeIcon_001.png", "button_speedhack");
}

void ButtonEditLayer::restoreDefaults() {
    for (auto spr : spriteButtons) {
        if (spr)
            spr->removeFromParentAndCleanup(true);
    }
    spriteButtons.clear();
    movingButton = {};

    setDefaultButtonValues(mod);
    loadSavedButtonState();
    addSprites();
    updateSelected("button_advance_frame");
}

void ButtonEditLayer::loadSavedButtonState() {
    if (!mod)
        return;

    positions.clear();
    scales.clear();
    opacities.clear();

    for (auto const& id : indexToID) {
        positions[id] = ccp(
            getSavedFloat(mod, id + "_pos_x", 0.f),
            getSavedFloat(mod, id + "_pos_y", 0.f)
        );
        scales[id] = getSavedFloat(mod, id + "_scale", 1.f);
        opacities[id] = getSavedFloat(mod, id + "_opacity", 1.f);
    }
}

void ButtonEditLayer::saveButtonState() {
    if (!mod)
        return;

    for (auto const& id : indexToID) {
        auto posIt = positions.find(id);
        auto scaleIt = scales.find(id);
        auto opacityIt = opacities.find(id);

        if (posIt != positions.end()) {
            mod->setSavedValue(id + "_pos_x", posIt->second.x);
            mod->setSavedValue(id + "_pos_y", posIt->second.y);
        }

        if (scaleIt != scales.end())
            mod->setSavedValue(id + "_scale", scaleIt->second);

        if (opacityIt != opacities.end())
            mod->setSavedValue(id + "_opacity", opacityIt->second);
    }
}
