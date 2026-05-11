#include "../includes.hpp"

class AutoSaveLayer : public geode::Popup, public TextInputDelegate {

  private:
    TextInput *intervalInput = nullptr;
    CCMenuItemToggler *intervalToggle = nullptr;
    CCMenuItemToggler *levelEndToggle = nullptr;
    CCMenuItemToggler *checkpointToggle = nullptr;
    CCLabelBMFont *intervalLbl = nullptr;
    CCLabelBMFont *intervalLbl2 = nullptr;

    void textChanged(CCTextInputNode *) override {
        std::string str = intervalInput->getString();
        float mins = geode::utils::numFromString<float>(str).unwrapOr(0.f);

        Global::get().autosaveInterval = static_cast<int>(mins * 60);
        Mod::get()->setSavedValue("autosave_interval", str);
    }

    bool init() override {
        if (!Popup::init(250, 190, Utils::getTexture().c_str()))
            return false;
        setTitle("AutoSave");
        m_title->setScale(0.575f);
        m_title->setPositionY(171);

        CCLabelBMFont *lbl =
            CCLabelBMFont::create("Per Interval", "bigFont.fnt");
        lbl->setPosition({56, 130});
        lbl->setAnchorPoint({0, 0.5f});
        lbl->setScale(0.32f);
        m_mainLayer->addChild(lbl);

        lbl = CCLabelBMFont::create("Per Checkpoint", "bigFont.fnt");
        lbl->setPosition({56, 94});
        lbl->setAnchorPoint({0, 0.5f});
        lbl->setScale(0.32f);
        m_mainLayer->addChild(lbl);

        lbl = CCLabelBMFont::create("On Level End", "bigFont.fnt");
        lbl->setPosition({56, 57});
        lbl->setAnchorPoint({0, 0.5f});
        lbl->setScale(0.32f);
        m_mainLayer->addChild(lbl);

        intervalLbl = CCLabelBMFont::create("Interval", "bigFont.fnt");
        intervalLbl->setPosition({169, 150});
        intervalLbl->setScale(0.325f);
        intervalLbl->setOpacity(94);
        m_mainLayer->addChild(intervalLbl);

        intervalLbl2 = CCLabelBMFont::create("mins", "bigFont.fnt");
        intervalLbl2->setPosition({188, 130});
        intervalLbl2->setAnchorPoint({0, 0.5f});
        intervalLbl2->setScale(0.22f);
        m_mainLayer->addChild(intervalLbl2);

        intervalInput = TextInput::create(50, "0", "bigFont.fnt");
        intervalInput->setPosition({169, 130});
        intervalInput->setScale(0.675f);
        intervalInput->setString(
            Mod::get()
                ->getSavedValue<std::string>("autosave_interval")
                .c_str());
        intervalInput->getInputNode()->setDelegate(this);
        intervalInput->getInputNode()->setAllowedChars("0123456789.");
        intervalInput->getInputNode()->setMaxLabelLength(4);
        m_mainLayer->addChild(intervalInput);

        intervalToggle = CCMenuItemExt::createTogglerWithStandardSprites(0.6f, [this](CCMenuItemToggler *sender) { AutoSaveLayer::onToggle(sender); });
        intervalToggle->setPosition({35, 130});
        intervalToggle->toggle(
            Mod::get()->getSavedValue<bool>("autosave_interval_enabled"));
        m_buttonMenu->addChild(intervalToggle);

        checkpointToggle = CCMenuItemExt::createTogglerWithStandardSprites(0.6f, [this](CCMenuItemToggler *sender) { AutoSaveLayer::onToggle(sender); });
        checkpointToggle->setPosition({35, 94});
        checkpointToggle->toggle(
            Mod::get()->getSavedValue<bool>("autosave_checkpoint_enabled"));
        m_buttonMenu->addChild(checkpointToggle);

        levelEndToggle = CCMenuItemExt::createTogglerWithStandardSprites(0.6f, [this](CCMenuItemToggler *sender) { AutoSaveLayer::onToggle(sender); });
        levelEndToggle->setPosition({35, 57});
        levelEndToggle->toggle(
            Mod::get()->getSavedValue<bool>("autosave_levelend_enabled"));
        m_buttonMenu->addChild(levelEndToggle);

        ButtonSprite *btnSpr = ButtonSprite::create("OK");
        btnSpr->setScale(0.7f);
        CCMenuItemSpriteExtra *btn = CCMenuItemExt::createSpriteExtra(btnSpr, [this](CCMenuItemSpriteExtra *sender) { AutoSaveLayer::onClose(sender); });
        btn->setPosition({m_size.width / 2, 24});
        m_buttonMenu->addChild(btn);

        textChanged(nullptr);
        updateInputs();

        return true;
    }

    void onToggle(CCObject *) {
        Loader::get()->queueInMainThread([this] {
            auto &g = Global::get();
            g.autosaveIntervalEnabled = intervalToggle->isToggled();
            g.mod->setSavedValue("autosave_interval_enabled",
                                 g.autosaveIntervalEnabled);
            g.mod->setSavedValue("autosave_checkpoint_enabled",
                                 checkpointToggle->isToggled());
            g.mod->setSavedValue("autosave_levelend_enabled",
                                 levelEndToggle->isToggled());
            updateInputs();
        });
    }

    void updateInputs() {
        auto &g = Global::get();

        intervalLbl->setOpacity(g.autosaveIntervalEnabled ? 255 : 100);
        intervalLbl2->setOpacity(g.autosaveIntervalEnabled ? 255 : 100);
        intervalInput->getBGSprite()->setOpacity(
            g.autosaveIntervalEnabled ? 90 : 30);
        intervalInput->setEnabled(g.autosaveIntervalEnabled);
        intervalInput->getInputNode()->m_textLabel->setOpacity(
            g.autosaveIntervalEnabled ? 255 : 100);
        if (!g.autosaveIntervalEnabled) {
            intervalInput->getInputNode()->detachWithIME();
            intervalInput->getInputNode()->onClickTrackNode(false);
            intervalInput->getInputNode()->m_cursor->setVisible(false);
        }
    }

  public:
    STATIC_CREATE(AutoSaveLayer)
};
