#include "../includes.hpp"
#include "../hacks/show_trajectory.hpp"

class TrajectorySettingsLayer : public geode::Popup, public TextInputDelegate {
    
public:
    STATIC_CREATE(TrajectorySettingsLayer)

    void open(CCObject*) {
        create()->show();
    }

private:
    ColorChannelSprite* color1 = nullptr;
    ColorChannelSprite* color2 = nullptr;

    TextInput* input = nullptr;

    bool init() override {
        if (!Popup::init(250, 180, Utils::getTexture().c_str())) return false;
        setTitle("Show Trajectory");

        Utils::setBackgroundColor(m_bgSprite);

        CCSprite* spriteOn = CCSprite::createWithSpriteFrameName("GJ_checkOn_001.png");
        CCSprite* spriteOff = CCSprite::createWithSpriteFrameName("GJ_checkOff_001.png");

        CCLabelBMFont* lbl = CCLabelBMFont::create("Both Sides", "bigFont.fnt");
        lbl->setPosition({m_size.width - 68, 77});
        lbl->setScale(0.35f);
        m_mainLayer->addChild(lbl);

        CCMenuItemToggler* toggle = CCMenuItemToggler::create(spriteOff, spriteOn, this, menu_selector(TrajectorySettingsLayer::onToggle));
        toggle->setPosition({m_size.width - 68, 56});
        toggle->setScale(0.5f);
        toggle->toggle(Mod::get()->getSavedValue<bool>("macro_trajectory_both_sides"));
        m_buttonMenu->addChild(toggle);

        lbl = CCLabelBMFont::create("Primary Color", "bigFont.fnt");
        lbl->setPosition({68, 126});
        lbl->setScale(0.35f);
        m_mainLayer->addChild(lbl);

        lbl = CCLabelBMFont::create("Secondary Color", "bigFont.fnt");
        lbl->setPosition({m_size.width - 68, 126});
        lbl->setScale(0.35f);
        m_mainLayer->addChild(lbl);

        lbl = CCLabelBMFont::create("Trajectory Length", "bigFont.fnt");
        lbl->setPosition({68, 77});
        lbl->setScale(0.35f);    
        m_mainLayer->addChild(lbl);

        input = TextInput::create(50.f, "0");
        input->setPosition({68, 56});
        input->setScale(0.65f);
        input->getInputNode()->setAllowedChars("0123456789");
        input->getInputNode()->setDelegate(this);
        input->setString(Mod::get()->getSavedValue<std::string>("trajectory_length").c_str());
        m_mainLayer->addChild(input);

        color1 = ColorChannelSprite::create();
        color1->setColor(ShowTrajectory::ccc3BFromccc4F(ShowTrajectory::get().color1));
        color1->setScale(0.5f);

        CCMenuItemSpriteExtra* colorButton = CCMenuItemSpriteExtra::create(
            color1,
            this,
            menu_selector(TrajectorySettingsLayer::openColorPicker)
        );
        colorButton->setPosition({68, 109});
        colorButton->setTag(1);
        m_buttonMenu->addChild(colorButton);

        color2 = ColorChannelSprite::create();
        color2->setColor(ShowTrajectory::ccc3BFromccc4F(ShowTrajectory::get().color2));
        color2->setScale(0.5f);

        colorButton = CCMenuItemSpriteExtra::create(
            color2,
            this,
            menu_selector(TrajectorySettingsLayer::openColorPicker)
        );
        colorButton->setPosition({m_size.width - 68, 109});
        m_buttonMenu->addChild(colorButton);

        ButtonSprite* btnSpr = ButtonSprite::create("Ok");
        btnSpr->setScale(0.73f);
        CCMenuItemSpriteExtra* btn = CCMenuItemSpriteExtra::create(btnSpr, this, menu_selector(TrajectorySettingsLayer::onClose));
        btn->setPosition({m_size.width / 2, 23});
        m_buttonMenu->addChild(btn);

        btnSpr = ButtonSprite::create("Restore");
        btnSpr->setScale(0.3f);
        btn = CCMenuItemSpriteExtra::create(btnSpr, this, menu_selector(TrajectorySettingsLayer::onRestore));
        btn->setPosition({223, 14});
        m_buttonMenu->addChild(btn);

        return true;
    }

    void onToggle(CCObject* obj) {
        CCMenuItemToggler* toggle = static_cast<CCMenuItemToggler*>(obj);

        Mod::get()->setSavedValue("macro_trajectory_both_sides", !toggle->isToggled());
        Global::get().trajectoryBothSides = !toggle->isToggled();
    }

    void onRestore(CCObject*) {
        color1->setColor(ccc3(74, 226, 85));
        color2->setColor(ccc3(130, 8, 8));
        input->setString("240");

        textChanged(nullptr);
    }

    // Corrected color conversion function
    cocos2d::ccColor4F ccColor4FFromccColor4B(const cocos2d::ccColor4B& color) {
        return cocos2d::ccColor4F(
            color.r / 255.0f, 
            color.g / 255.0f, 
            color.b / 255.0f, 
            color.a / 255.0f
        );
    }

    void openColorPicker(CCObject* obj) {
        ColorChannelSprite* color = static_cast<CCNode*>(obj)->getTag() == 1 ? color1 : color2;

        ColorPickPopup* popup = ColorPickPopup::create(color->getColor());
        popup->setColorTarget(color);
        popup->setCallback([this, color](const cocos2d::ccColor4B& newColor) {
            color->setColor(ccc3(newColor.r, newColor.g, newColor.b));
            ShowTrajectory::get().color1 = ccColor4FFromccColor4B(newColor);
            ShowTrajectory::get().updateMergedColor();

            Mod::get()->setSavedValue("trajectory_color1", ShowTrajectory::ccc3BFromccc4F(ccColor4FFromccColor4B(newColor)));
        });

        popup->show();
    }

    void textChanged(CCTextInputNode*) override {
        std::string str = input->getString();
        int length = geode::utils::numFromString<int>(str).unwrapOr(0);

        if (length > 2560 || length < 1 || str.empty()) 
            return input->setString(Mod::get()->getSavedValue<std::string>("trajectory_length").c_str());

        ShowTrajectory::get().length = length;
        Mod::get()->setSavedValue("trajectory_length", str);
    }
};
