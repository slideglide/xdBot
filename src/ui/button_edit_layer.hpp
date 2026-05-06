pragma once

#include "../includes.hpp"

inline const std::vector<std::string> indexToID = {
    "button_off",
    "button_advance_frame",
    "button_speedhack",
};

inline const std::map<std::string, size_t> IDtoIndex = {
    {"button_off", 0},
    {"button_advance_frame", 1},
    {"button_speedhack", 2},
};

inline const std::map<std::string, std::string> IDtoName = {
    {"button_off", "Frame Stepper Off"},
    {"button_advance_frame", "Advance Frame"},
    {"button_speedhack", "Toggle Speedhack"},
};

struct MovingButton {
    size_t index = 0;
    cocos2d::CCSprite* sprite = nullptr;
    cocos2d::CCPoint offset = ccp(0, 0);
};

class ButtonEditLayer : public geode::Popup {
private:
    bool init() override;

public:
    STATIC_CREATE(ButtonEditLayer)

    geode::Mod* mod = nullptr;
    cocos2d::CCMenu* menu = nullptr;

    geode::SliderNode* scaleSlider = nullptr;
    geode::SliderNode* opacitySlider = nullptr;

    cocos2d::CCLabelBMFont* scaleLbl = nullptr;
    cocos2d::CCLabelBMFont* opacityLbl = nullptr;
    cocos2d::CCLabelBMFont* selectedLbl = nullptr;

    std::vector<cocos2d::CCSprite*> spriteButtons;

    MovingButton movingButton;

    size_t currentSelected = 0;

    std::map<std::string, cocos2d::CCPoint> positions;
    std::map<std::string, float> scales;
    std::map<std::string, float> opacities;

    bool ccTouchBegan(cocos2d::CCTouch* touch, cocos2d::CCEvent* event) override;
    void ccTouchMoved(cocos2d::CCTouch* touch, cocos2d::CCEvent* event) override;
    void ccTouchEnded(cocos2d::CCTouch* touch, cocos2d::CCEvent* event) override;

    void updateSelectedLabels();
    void updateScale(geode::SliderNode* sender, float value);
    void updateOpacity(geode::SliderNode* sender, float value);
    void updateSelected(std::string const& selected);

    void addSliders();
    void addSprites();
    void restoreDefaults();
    void loadSavedButtonState();
    void saveButtonState();

    static bool isPointInButton(
        cocos2d::CCPoint clickPos,
        cocos2d::CCPoint btnPos,
        cocos2d::CCSize btnSize
    );
};
