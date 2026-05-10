#include "../includes.hpp"
#include "record_layer.hpp"

class MirrorSettingsLayer : public geode::Popup {

  private:
    bool init() {
        if (!Popup::init(200, 146, Utils::getTexture().c_str()))
            return false;
        setTitle("Input Mirror");

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

        CCLabelBMFont *lbl = CCLabelBMFont::create("Inverted", "bigFont.fnt");
        lbl->setPosition({17, -0});
        lbl->setScale(0.5f);
        menu->addChild(lbl);

        CCMenuItemToggler *toggle = CCMenuItemExt::createTogglerWithStandardSprites(0.875f, [this](CCMenuItemToggler *sender) { static_cast<RecordLayer*>(Global::get())->toggleSetting(sender); });
        toggle->setPosition({-47, -0});
        toggle->setScale(0.875f);
        toggle->toggle(
            Mod::get()->getSavedValue<bool>("p2_input_mirror_inverted"));
        toggle->setID("p2_input_mirror_inverted");
        menu->addChild(toggle);

        return true;
    }

  public:
    STATIC_CREATE(MirrorSettingsLayer)

    void open(CCObject *) { create()->show(); }
};
