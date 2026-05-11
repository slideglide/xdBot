#pragma once

#include "../includes.hpp"

class RenderSettingsLayer : public geode::Popup, public TextInputDelegate {

  public:
    Slider *sfxSlider = nullptr;
    Slider *musicSlider = nullptr;
    TextInput *fadeInInput = nullptr;
    TextInput *fadeOutInput = nullptr;
    TextInput *extensionInput = nullptr;

    TextInput *argsInput = nullptr;
    TextInput *audioArgsInput = nullptr;
    TextInput *secondsInput = nullptr;
    TextInput *videoArgsInput = nullptr;

    Mod *mod = nullptr;

  private:
    bool init() override;

  public:
    ~RenderSettingsLayer() override {
        if (argsInput) {
            argsInput->defocus();
            argsInput->setDelegate(nullptr);
        }
        if (audioArgsInput) {
            audioArgsInput->defocus();
            audioArgsInput->setDelegate(nullptr);
        }
        if (secondsInput) {
            secondsInput->defocus();
            secondsInput->setDelegate(nullptr);
        }
        if (videoArgsInput) {
            videoArgsInput->defocus();
            videoArgsInput->setDelegate(nullptr);
        }
    }

    STATIC_CREATE(RenderSettingsLayer)

        void open(CCObject *) {
        create()->show();
    }
    void close(CCObject *) { keyBackClicked(); }

    void textChanged(CCTextInputNode *node) override;
    void onSlider(CCObject *);
    void onDefaults(CCObject *);
    void showInfoPopup(CCObject *);
};
