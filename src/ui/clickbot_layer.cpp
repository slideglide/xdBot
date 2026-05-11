#include "clickbot_layer.hpp"

void ClickbotLayer::updateLabels() {
    auto &g = Global::get();

    for (size_t i = 0; i < buttonNames.size(); i++) {
        matjson::Value data =
            g.mod->getSavedValue<matjson::Value>(buttonNames[i]);

        ClickSetting settings =
            matjson::Serialize<ClickSetting>::from_json(data);

        std::string filename =
            geode::utils::string::pathToString(settings.path.filename());

        if (!std::filesystem::exists(settings.path))
            filename = "N/A";

        labels[i]->setString(filename.c_str());
        labels[i]->limitLabelWidth(72.f, 1.f, 0.01f);
        labels[i]->updateLabel();
    }
}

bool ClickbotLayer::init() {
    if (!Popup::init(432, 250, Utils::getTexture().c_str()))
        return false;
    setTitle("ClickBot");
    m_title->setPositionY(m_title->getPositionY() + 5);

    cocos2d::CCPoint offset = (CCDirector::sharedDirector()->getWinSize() -
                               m_mainLayer->getContentSize()) /
                              2;
    m_mainLayer->setPosition(m_mainLayer->getPosition() - offset);
    m_closeBtn->setPosition(m_closeBtn->getPosition() + offset);
    m_bgSprite->setPosition(m_bgSprite->getPosition() + offset);
    m_title->setPosition(m_title->getPosition() + offset);

    Utils::setBackgroundColor(m_bgSprite);

    CCMenu *menu = CCMenu::create();
    m_mainLayer->addChild(menu);

    NineSlice *bg = NineSlice::create("square02b_001.png", {0, 0, 80, 80});
    bg->setColor({0, 0, 0});
    bg->setOpacity(78);
    bg->setPosition(ccp(-79, -10));
    bg->setAnchorPoint({0.5, 0.5});
    bg->setContentSize({245, 203});
    menu->addChild(bg);

    CCLabelBMFont *lbl = CCLabelBMFont::create("Clicks", "goldFont.fnt");
    lbl->setPosition(ccp(-80, 78));
    lbl->setScale(0.625);
    menu->addChild(lbl);

    lbl = CCLabelBMFont::create("Hold Click", "bigFont.fnt");
    lbl->setPosition(ccp(-142, 57));
    lbl->setScale(0.35);
    menu->addChild(lbl);

    bg = NineSlice::create("square02b_001.png", {0, 0, 80, 80});
    bg->setScale(0.375);
    bg->setColor({0, 0, 0});
    bg->setOpacity(78);
    bg->setPosition(ccp(-150, 35));
    bg->setAnchorPoint({0.5, 0.5});
    bg->setContentSize({230, 55});
    menu->addChild(bg);

    lbl = CCLabelBMFont::create("easter egg", "chatFont.fnt");
    lbl->setPosition(ccp(-150, 35));
    menu->addChild(lbl);
    labels.push_back(lbl);

    CCSprite *spr =
        CCSprite::createWithSpriteFrameName("GJ_optionsBtn_001.png");
    spr->setScale(0.375f);

    CCMenuItemSpriteExtra *btn = CCMenuItemExt::createSpriteExtra(
        spr, [this](CCMenuItemSpriteExtra *sender) {
            ClickbotLayer::openClickSettings(sender);
        });
    btn->setPosition(ccp(-94, 35));
    btn->setID("hold_click");
    menu->addChild(btn);

    lbl = CCLabelBMFont::create("Release Click", "bigFont.fnt");
    lbl->setPosition(ccp(-21, 57));
    lbl->setScale(0.35);
    menu->addChild(lbl);

    bg = NineSlice::create("square02b_001.png", {0, 0, 80, 80});
    bg->setScale(0.375);
    bg->setColor({0, 0, 0});
    bg->setOpacity(78);
    bg->setPosition(ccp(-29, 35));
    bg->setAnchorPoint({0.5, 0.5});
    bg->setContentSize({230, 55});
    menu->addChild(bg);

    lbl = CCLabelBMFont::create("easter egg", "chatFont.fnt");
    lbl->setPosition(ccp(-29, 35));
    lbl->setScale(0.425);
    menu->addChild(lbl);
    labels.push_back(lbl);

    btn = CCMenuItemExt::createSpriteExtra(
        spr, [this](CCMenuItemSpriteExtra *sender) {
            ClickbotLayer::openClickSettings(sender);
        });
    btn->setPosition(ccp(27, 35));
    btn->setID("release_click");
    menu->addChild(btn);

    lbl = CCLabelBMFont::create("Hold Left", "bigFont.fnt");
    lbl->setPosition(ccp(-142, 0));
    lbl->setScale(0.35);
    menu->addChild(lbl);

    bg = NineSlice::create("square02b_001.png", {0, 0, 80, 80});
    bg->setScale(0.375);
    bg->setColor({0, 0, 0});
    bg->setOpacity(78);
    bg->setPosition(ccp(-150, -22));
    bg->setAnchorPoint({0.5, 0.5});
    bg->setContentSize({230, 55});
    menu->addChild(bg);

    lbl = CCLabelBMFont::create("easter egg", "chatFont.fnt");
    lbl->setPosition(ccp(-150, -22));
    lbl->setScale(0.425);
    menu->addChild(lbl);
    labels.push_back(lbl);

    btn = CCMenuItemExt::createSpriteExtra(
        spr, [this](CCMenuItemSpriteExtra *sender) {
            ClickbotLayer::openClickSettings(sender);
        });
    btn->setPosition(ccp(-94, -22));
    btn->setID("hold_left");
    menu->addChild(btn);

    lbl = CCLabelBMFont::create("Release Left", "bigFont.fnt");
    lbl->setPosition(ccp(-21, -0));
    lbl->setScale(0.35);
    menu->addChild(lbl);

    bg = NineSlice::create("square02b_001.png", {0, 0, 80, 80});
    bg->setScale(0.375);
    bg->setColor({0, 0, 0});
    bg->setOpacity(78);
    bg->setPosition(ccp(-29, -22));
    bg->setAnchorPoint({0.5, 0.5});
    bg->setContentSize({230, 55});
    menu->addChild(bg);

    lbl = CCLabelBMFont::create("easter egg", "chatFont.fnt");
    lbl->setPosition(ccp(-29, -22));
    lbl->setScale(0.425);
    menu->addChild(lbl);
    labels.push_back(lbl);

    btn = CCMenuItemExt::createSpriteExtra(
        spr, [this](CCMenuItemSpriteExtra *sender) {
            ClickbotLayer::openClickSettings(sender);
        });
    btn->setPosition(ccp(27, -22));
    btn->setID("release_left");
    menu->addChild(btn);

    lbl = CCLabelBMFont::create("Hold Right", "bigFont.fnt");
    lbl->setPosition(ccp(-142, -57));
    lbl->setScale(0.35);
    menu->addChild(lbl);

    bg = NineSlice::create("square02b_001.png", {0, 0, 80, 80});
    bg->setScale(0.375);
    bg->setColor({0, 0, 0});
    bg->setOpacity(78);
    bg->setPosition(ccp(-150, -79));
    bg->setAnchorPoint({0.5, 0.5});
    bg->setContentSize({230, 55});
    menu->addChild(bg);

    lbl = CCLabelBMFont::create("easter egg", "chatFont.fnt");
    lbl->setPosition(ccp(-150, -79));
    lbl->setScale(0.425);
    menu->addChild(lbl);
    labels.push_back(lbl);

    btn = CCMenuItemExt::createSpriteExtra(
        spr, [this](CCMenuItemSpriteExtra *sender) {
            ClickbotLayer::openClickSettings(sender);
        });
    btn->setPosition(ccp(-94, -79));
    btn->setID("hold_right");
    menu->addChild(btn);

    lbl = CCLabelBMFont::create("Release Right", "bigFont.fnt");
    lbl->setPosition(ccp(-21, -57));
    lbl->setScale(0.35);
    menu->addChild(lbl);

    bg = NineSlice::create("square02b_001.png", {0, 0, 80, 80});
    bg->setScale(0.375);
    bg->setColor({0, 0, 0});
    bg->setOpacity(78);
    bg->setPosition(ccp(-29, -79));
    bg->setAnchorPoint({0.5, 0.5});
    bg->setContentSize({230, 55});
    menu->addChild(bg);

    lbl = CCLabelBMFont::create("easter egg", "chatFont.fnt");
    lbl->setPosition(ccp(-29, -79));
    lbl->setScale(0.425);
    menu->addChild(lbl);
    labels.push_back(lbl);

    btn = CCMenuItemExt::createSpriteExtra(
        spr, [this](CCMenuItemSpriteExtra *sender) {
            ClickbotLayer::openClickSettings(sender);
        });
    btn->setPosition(ccp(27, -79));
    btn->setID("release_right");
    menu->addChild(btn);

    bg = NineSlice::create("square02b_001.png", {0, 0, 80, 80});
    bg->setColor({0, 0, 0});
    bg->setOpacity(78);
    bg->setPosition(ccp(128, -10));
    bg->setAnchorPoint({0.5, 0.5});
    bg->setContentSize({148, 203});
    menu->addChild(bg);

    lbl = CCLabelBMFont::create("Settings", "goldFont.fnt");
    lbl->setPosition(ccp(128, 78));
    lbl->setScale(0.625);
    menu->addChild(lbl);

    int volume = Mod::get()->getSavedValue<int64_t>("clickbot_volume");

    volumeSlider = SliderNode::create([this](SliderNode *sender, float value) {
        Mod::get()->setSavedValue("clickbot_volume", (int64_t)value);
        volumeLabel->setString(fmt::format("Volume ({:.0f}%)", value).c_str());
    });
    volumeSlider->setMin(0.f);
    volumeSlider->setMax(100.f);
    volumeSlider->setPosition(ccp(128, 49));
    volumeSlider->setAnchorPoint({0.f, 0.f});
    volumeSlider->setScale(0.65f);
    volumeSlider->getThumb()->setScale(0.8f);
    volumeSlider->setValue(
        Mod::get()->getSavedValue<int64_t>("clickbot_volume"));
    menu->addChild(volumeSlider);

    volumeLabel = CCLabelBMFont::create(
        ("Master Volume (" + geode::utils::numToString(volume) + "%)").c_str(),
        "goldFont.fnt");
    volumeLabel->setPosition(ccp(128, 35));
    volumeLabel->setScale(0.35f);
    menu->addChild(volumeLabel);

    float pitch = Mod::get()->getSavedValue<float>("clickbot_pitch");

    pitchSlider = SliderNode::create([this](SliderNode *sender, float value) {
        float pitch = value * 3.f;
        Global::updatePitch(pitch);
        pitchLabel->setString(
            fmt::format("Master Pitch ({:.1f})", pitch).c_str());
    });
    pitchSlider->setPosition(ccp(128, 7));
    pitchSlider->setAnchorPoint({0.f, 0.f});
    pitchSlider->setScale(0.65f);
    pitchSlider->setValue(pitch / 3.f);
    menu->addChild(pitchSlider);

    pitchLabel = CCLabelBMFont::create(
        fmt::format("Master Pitch ({:.1f})", pitch).c_str(), "goldFont.fnt");
    pitchLabel->setPosition(ccp(128, -7));
    pitchLabel->setScale(0.35f);
    menu->addChild(pitchLabel);

    // CCSprite* spriteOn =
    // CCSprite::createWithSpriteFrameName("GJ_checkOn_001.png"); CCSprite*
    // spriteOff = CCSprite::createWithSpriteFrameName("GJ_checkOff_001.png");

    // CCMenuItemToggler* toggle = CCMenuItemExt::createToggler(spriteOn,
    // spriteOn, this, menu_selector(RecordLayer::toggleSetting));
    CCMenuItemToggler *toggle = CCMenuItemExt::createTogglerWithStandardSprites(
        0.55f, [this](CCMenuItemToggler *sender) {});
    toggle->setPosition(ccp(73, -40));
    toggle->toggle(Mod::get()->getSavedValue<bool>("clickbot_playing_only"));
    toggle->setID("clickbot_playing_only");
    menu->addChild(toggle);

    lbl = CCLabelBMFont::create("Playing Macro Only", "bigFont.fnt");
    lbl->setPosition(ccp(88, -40));
    lbl->setAnchorPoint({0, 0.5});
    lbl->setScale(0.3f);
    menu->addChild(lbl);

    toggle = CCMenuItemExt::createTogglerWithStandardSprites(
        0.55f, [this](CCMenuItemToggler *sender) {
            Mod::get()->setSavedValue("clickbot_holding_only",
                                      sender->isToggled());
        });
    toggle->setPosition(ccp(73, -70));
    toggle->toggle(Mod::get()->getSavedValue<bool>("clickbot_holding_only"));
    toggle->setID("clickbot_holding_only");
    menu->addChild(toggle);

    lbl = CCLabelBMFont::create("Hold Only", "bigFont.fnt");
    lbl->setPosition(ccp(88, -70));
    lbl->setAnchorPoint({0, 0.5});
    lbl->setScale(0.3f);
    menu->addChild(lbl);

    updateLabels();

    return true;
}

ClickSettingsLayer *ClickSettingsLayer::create(std::string button,
                                               geode::Popup *layer) {
    ClickSettingsLayer *ret = new ClickSettingsLayer();
    if (ret->init(button, layer)) {
        ret->autorelease();
        return ret;
    }

    delete ret;
    return nullptr;
}

bool ClickSettingsLayer::init(std::string button, geode::Popup *layer) {
    if (!Popup::init(250, 173, Utils::getTexture().c_str()))
        return false;
    cocos2d::CCPoint offset = (CCDirector::sharedDirector()->getWinSize() -
                               m_mainLayer->getContentSize()) /
                              2;
    m_mainLayer->setPosition(m_mainLayer->getPosition() - offset);
    m_closeBtn->setPosition(m_closeBtn->getPosition() + offset);
    m_bgSprite->setPosition(m_bgSprite->getPosition() + offset);

    Utils::setBackgroundColor(m_bgSprite);

    CCMenu *menu = CCMenu::create();
    m_mainLayer->addChild(menu);

    this->button = button;
    this->clickbotLayer = layer;

    matjson::Value data = Mod::get()->getSavedValue<matjson::Value>(button);
    settings = matjson::Serialize<ClickSetting>::from_json(data);
    std::string filename =
        geode::utils::string::pathToString(settings.path.filename());

    if (!std::filesystem::exists(settings.path))
        filename = "N/A";

    NineSlice *bg = NineSlice::create("square02b_001.png", {0, 0, 80, 80});
    bg->setColor({0, 0, 0});
    bg->setScale(0.6f);
    bg->setOpacity(88);
    bg->setPosition(ccp(-37.5, 52));
    bg->setContentSize({230, 55});
    menu->addChild(bg);

    filenameLabel = CCLabelBMFont::create(filename.c_str(), "chatFont.fnt");
    filenameLabel->setPosition(ccp(-37.5, 52));
    filenameLabel->limitLabelWidth(125.f, 0.675f, 0.01f);
    filenameLabel->updateLabel();
    menu->addChild(filenameLabel);

    ButtonSprite *spr = ButtonSprite::create("Select");
    spr->setScale(0.6f);

    CCMenuItemSpriteExtra *btn = CCMenuItemExt::createSpriteExtra(
        spr, [this](CCMenuItemSpriteExtra *sender) {
            file::FilePickOptions::Filter textFilter;
            textFilter.description = "Macro Files";
            textFilter.files = {"*.mp3", "*.ogg"};

            file::FilePickOptions fileOptions;
            fileOptions.filters.push_back(textFilter);
            fileOptions.defaultPath = Mod::get()->getResourcesDir();
            m_importTask.spawn(
                file::pick(file::PickMode::OpenFile, fileOptions),
                [this](Result<std::optional<std::filesystem::path>> result) {
                    if (result.isOk()) {
                        auto pathOpt = result.unwrap();
                        if (pathOpt) {
                            std::filesystem::path path = pathOpt.value();

                            this->filenameLabel->setString(
                                geode::utils::string::pathToString(
                                    path.filename())
                                    .c_str());
                            this->settings.path = path;
                            this->saveSettings();

                            if (this->clickbotLayer) {
                                static_cast<ClickbotLayer *>(
                                    this->clickbotLayer)
                                    ->updateLabels();
                            }
                        }
                    } else {
                        log::error("Failed to pick file: {}",
                                   result.unwrapErr());
                    }
                });
        });
    btn->setPosition(ccp(77, 52));
    menu->addChild(btn);

    volumeSlider = SliderNode::create([this](SliderNode *, float value) {
        settings.volume = value;

        volumeLabel->setString(fmt::format("Volume ({:.0f}%)", value).c_str());
    });
    volumeSlider->setMin(0.f);
    volumeSlider->setMax(100.f);
    volumeSlider->setPosition(ccp(-42, 7));
    volumeSlider->setAnchorPoint({0.f, 0.f});
    volumeSlider->setScale(0.8f);
    volumeSlider->getThumb()->setScale(0.8f);
    volumeSlider->setReleaseCallback(
        [this](SliderNode *, float) { saveSettings(); });
    volumeSlider->setValue(settings.volume);
    menu->addChild(volumeSlider);

    volumeLabel = CCLabelBMFont::create(
        ("Volume (" + geode::utils::numToString(settings.volume) + "%)")
            .c_str(),
        "goldFont.fnt");
    volumeLabel->setPosition(ccp(-42, -9));
    volumeLabel->setScale(0.45f);
    menu->addChild(volumeLabel);

    pitchSlider = SliderNode::create([this](SliderNode *sender, float value) {
        settings.pitch = value;
        pitchLabel->setString(
            fmt::format("Pitch ({:.1f})", settings.pitch).c_str());
    });

    pitchSlider->setMin(0.f);
    pitchSlider->setMax(3.f);
    pitchSlider->setSnapStep(0.1f);
    pitchSlider->setReleaseCallback(
        [this](SliderNode *, float) { saveSettings(); });
    pitchSlider->setPosition(ccp(-42, -40));
    pitchSlider->setAnchorPoint({0.f, 0.f});
    pitchSlider->setScale(0.8f);
    pitchSlider->setValue(settings.pitch);
    menu->addChild(pitchSlider);

    pitchLabel = CCLabelBMFont::create(
        fmt::format("Pitch ({:.1f})", settings.pitch).c_str(), "goldFont.fnt");
    pitchLabel->setPosition(ccp(-42, -56));
    pitchLabel->setScale(0.45f);
    menu->addChild(pitchLabel);

    disableToggle = CCMenuItemExt::createTogglerWithStandardSprites(
        0.7f, [this](CCMenuItemToggler *toggle) {
            settings.disabled = !toggle->isToggled();

            saveSettings();
        });
    disableToggle->setPosition(ccp(76, -9));
    disableToggle->toggle(settings.disabled);
    menu->addChild(disableToggle);

    CCLabelBMFont *lbl = CCLabelBMFont::create("Disable", "bigFont.fnt");
    lbl->setPosition(ccp(76, -32));
    lbl->setScale(0.45f);
    menu->addChild(lbl);

    spr = ButtonSprite::create("Restore");
    spr->setScale(0.425f);

    btn = CCMenuItemExt::createSpriteExtra(
        spr, [this, button](CCMenuItemSpriteExtra *sender) {
            pitchSlider->setValue(0.33333333f);
            volumeSlider->setValue(0.33333333f);

            disableToggle->toggle(false);

            settings.disabled = false;

            std::filesystem::path path = Mod::get()->getResourcesDir() /
                                         fmt::format("default_{}.mp3", button);

            filenameLabel->setString(
                geode::utils::string::pathToString(path.filename()).c_str());

            settings.path = path;
            saveSettings();

            static_cast<ClickbotLayer *>(clickbotLayer)->updateLabels();
        });
    btn->setPosition({89, -69});
    menu->addChild(btn);

    return true;
}
