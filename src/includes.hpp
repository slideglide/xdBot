#pragma once

#include <Geode/Geode.hpp>
#include <eclipse.eclipse-menu/include/config.hpp>

#include <string>

#include "global.hpp"
#include "macro.hpp"
#include "renderer/renderer.hpp"
#include <Geode/ui/Button.hpp>
#include <Geode/ui/SliderNode.hpp>

using namespace geode::prelude;

const int indexButton[6] = {1, 2, 3, 1, 2, 3};

const std::map<int, int> buttonIndex[2] = {{{1, 0}, {2, 1}, {3, 2}},
                                           {{1, 3}, {2, 4}, {3, 5}}};

const int sidesButtons[4] = {1, 2, 4, 5};

const std::string buttonIDs[6] = {
    "robtop.geometry-dash/jump-p1",       "robtop.geometry-dash/move-left-p1",
    "robtop.geometry-dash/move-right-p1", "robtop.geometry-dash/jump-p2",
    "robtop.geometry-dash/move-left-p2",  "robtop.geometry-dash/move-right-p2"};

#define STATIC_CREATE(class)                                                   \
    static class *create() {                                                   \
        class *ret = new class();                                              \
        if (ret->init()) {                                                     \
            ret->autorelease();                                                \
            return ret;                                                        \
        }                                                                      \
        delete ret;                                                            \
        return nullptr;                                                        \
    }