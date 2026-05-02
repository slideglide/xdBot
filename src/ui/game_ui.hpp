
#pragma once

#include "../includes.hpp"
#include "button_edit_layer.hpp"
#include "record_layer.hpp"

class Interface {

  public:
    static void openButtonEditor() {
        ButtonEditLayer *layer = ButtonEditLayer::create();
        layer->m_noElasticity = true;
        layer->show();
    }

    static void addLabels(PlayLayer *pl);

    static void addButtons(PlayLayer *pl);

    static void updateLabels();

    static void updateButtons();
};