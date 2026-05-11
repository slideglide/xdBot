#pragma once

#include "../includes.hpp"
#include <Geode/ui/GeodeUI.hpp>

#include "load_macro_layer.hpp"
#include "macro_info_layer.hpp"
#include "render_settings_layer.hpp"
#include "save_macro_layer.hpp"

enum InputType { None, Settings, Autosave, Speedhack, Seed, Respawn, Tps };

struct RecordSetting {
    std::string name;
    std::string id;
    InputType input;
    float labelScale = 0.325f;
    cocos2d::SEL_MenuHandler callback = nullptr;
    bool disabled = false;
};

const float ySettingPositions[6]{76.5f, 47.5f, 18.5f, -11.5f, -40.5f, -69.5f};

class RecordLayer : public geode::Popup, public TextInputDelegate {
  public:
    CCMenuItemToggler *recording = nullptr;
    CCMenuItemToggler *playing = nullptr;
    CCMenuItemToggler *speedhackToggle = nullptr;
    CCMenuItemToggler *trajectoryToggle = nullptr;
    CCMenuItemToggler *noclipToggle = nullptr;
    CCMenuItemToggler *frameStepperToggle = nullptr;
    CCMenuItemToggler *renderToggle = nullptr;
    CCMenuItemToggler *tpsToggle = nullptr;

    CCLabelBMFont *actionsLabel = nullptr;
    CCLabelBMFont *fpsLabel = nullptr;
    CCLabelBMFont *warningLabel = nullptr;

    CCSprite *warningSprite = nullptr;
    NineSlice *tpsBg = nullptr;

    CCMenuItemSpriteExtra *FPSLeft = nullptr;
    CCMenuItemSpriteExtra *FPSRight = nullptr;

    TextInput *widthInput = nullptr;
    TextInput *heightInput = nullptr;
    TextInput *bitrateInput = nullptr;
    TextInput *fpsInput = nullptr;
    TextInput *codecInput = nullptr;
    TextInput *seedInput = nullptr;
    TextInput *speedhackInput = nullptr;
    TextInput *respawnInput = nullptr;
    TextInput *tpsInput = nullptr;

    std::vector<CCNode *> nodes;
    std::vector<CCSprite *> dots;

    CCMenu *menu = nullptr;

    Mod *mod = nullptr;

    bool cursorWasHidden = false;

  protected:
    bool init() override;

    ~RecordLayer() override {
        if (widthInput) {
            widthInput->defocus();
            widthInput->setDelegate(nullptr);
        }
        if (heightInput) {
            heightInput->defocus();
            heightInput->setDelegate(nullptr);
        }
        if (bitrateInput) {
            bitrateInput->defocus();
            bitrateInput->setDelegate(nullptr);
        }
        if (fpsInput) {
            fpsInput->defocus();
            fpsInput->setDelegate(nullptr);
        }
        if (codecInput) {
            codecInput->defocus();
            codecInput->setDelegate(nullptr);
        }
        if (seedInput) {
            seedInput->defocus();
            seedInput->setDelegate(nullptr);
        }
        if (speedhackInput) {
            speedhackInput->defocus();
            speedhackInput->setDelegate(nullptr);
        }
        if (respawnInput) {
            respawnInput->defocus();
            respawnInput->setDelegate(nullptr);
        }
        if (tpsInput) {
            tpsInput->defocus();
            tpsInput->setDelegate(nullptr);
        }
        cocos2d::CCTouchDispatcher::get()->unregisterForcePrio(this);
        Global::get().layer = nullptr;
    }

  public:
    static std::string getTPSString();

    STATIC_CREATE(RecordLayer)

    virtual void onClose(cocos2d::CCObject *) override;

    void textChanged(CCTextInputNode *node) override;

    void checkSpeedhack();

    static RecordLayer *openMenu(bool instant = false);

    void openMenu2(CCObject *) {
        RecordLayer::openMenu(
            Mod::get()->getSettingValue<bool>("open_menu_instant"));
    }

    void moreSettings(CCObject *) { geode::openSettingsPopup(mod, false); }

    void updateDots();

    void openLoadMacro(CCObject *);

    void openSaveMacro(CCObject *);

    void showCodecPopup(CCObject *);

    void toggleRecording(CCObject *);

    void togglePlaying(CCObject *);

    void toggleRender(CCObject *btn);

    void toggleRender2(CCObject *btn);

    void openPresets(CCObject *);

    void onAutosaves(CCObject *);

    void goToSettingsPage(int page);

    void loadSetting(RecordSetting sett, float yPos);

    void setToggleMember(CCMenuItemToggler *toggle, std::string id);

    void onEditMacro(CCObject *);

    void macroInfo(CCObject *);

    void updatePage(CCObject *obj);

    void toggleSetting(CCObject *obj);

    void openKeybinds(CCObject *);

    void toggleFPS(bool on);

    void onDiscord(CCObject *);

    void updateTPS();
};
